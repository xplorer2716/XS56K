/*
XS56K - a realtime editor for the AKAI S5000/S6000 samplers
Copyright (C) 2026 https://github.com/xplorer2716

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "mcp/Tools.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <set>
#include <utility>

using nlohmann::json;

namespace mcp
{
    namespace
    {
        constexpr const char* KEYGROUP_ALL = "all";
        constexpr std::int64_t FIRST_KEYGROUP = 1;
        constexpr std::int64_t FIRST_ZONE = 1;
        constexpr std::int64_t LAST_ZONE = 4;
        constexpr std::int64_t MAX_PROGRAM_INDEX = 16383;
        // The sampler's program names are 12 characters on its screen; the wire takes up to 20 (the AKM item), and
        // whether the sampler keeps more is observed on the real sampler. [ADR-MCP-002 (DEC-MCP-011)]
        constexpr std::size_t MAX_PROGRAM_NAME_LENGTH = 12;
        constexpr std::int64_t MIN_NEW_KEYGROUPS = 1;
        constexpr std::int64_t MAX_NEW_KEYGROUPS = 99;
        constexpr char FIRST_PRINTABLE = ' ';
        constexpr char LAST_PRINTABLE = '~';

        constexpr const char* MEMORY_NOTICE =
            "It acts on the sampler's memory, not on disk: nothing is saved, and the change is lost if the sampler is "
            "switched off without saving the program from its front panel.";

        constexpr const char* KEYGROUP_ARGUMENT_DESCRIPTION =
            "Which keygroup of the current program: a number from 1, or \"all\" (the default). It applies to the "
            "keygroup and zone parameters (filter, envelopes, zones); the LFO parameters belong to the program.";

        constexpr const char* ZONE_ARGUMENT_DESCRIPTION =
            "Which zone of the keygroup(s), 1 to 4, or \"all\" (the default). Only for the zone parameters (group \"zone\").";

        std::string plural(std::int64_t count, const std::string& noun)
        {
            return std::to_string(count) + " " + noun + (count == 1 ? "" : "s");
        }

        ToolResult ok(std::string text)
        {
            return ToolResult{std::move(text), false};
        }

        ToolResult failure(std::string text)
        {
            return ToolResult{std::move(text), true};
        }

        std::string joined(const std::vector<std::string>& items, const std::string& separator)
        {
            std::string text;
            for (std::size_t i = 0; i < items.size(); ++i)
                text += (i == 0 ? "" : separator) + items[i];
            return text;
        }

        /// A refusal of the arguments a tool was given that it does not take, or nothing.
        std::optional<ToolResult> unknownArguments(const json& arguments, std::initializer_list<const char*> allowed)
        {
            const std::set<std::string> known(allowed.begin(), allowed.end());
            for (const auto& [key, value] : arguments.items())
            {
                if (known.count(key) == 0)
                {
                    const std::vector<std::string> names(allowed.begin(), allowed.end());
                    return failure("Unknown argument '" + key + "'. " +
                                   (names.empty() ? std::string("This tool takes no argument.")
                                                  : "This tool takes: " + joined(names, ", ") + "."));
                }
            }
            return std::nullopt;
        }

        /// A whole number from JSON: an integer, or a float that is one (2.0).
        std::optional<std::int64_t> wholeNumber(const json& value)
        {
            if (value.is_number_integer())
                return value.get<std::int64_t>();
            if (value.is_number_float())
            {
                const double number = value.get<double>();
                if (std::floor(number) == number && std::fabs(number) < 9.0e15)
                    return static_cast<std::int64_t>(number);
            }
            return std::nullopt;
        }

        // The `keygroup` argument: absent or "all" is every keygroup, a number from 1 is one.
        struct KeygroupArgument
        {
            std::optional<KeygroupSelection> selection;
            std::string problem;
        };

        KeygroupArgument keygroupArgument(const json& arguments)
        {
            const auto given = arguments.find("keygroup");
            if (given == arguments.end() || given->is_null())
                return {KeygroupSelection::all(), {}};
            if (given->is_string())
            {
                const std::string text = normalizeText(given->get<std::string>());
                if (text == KEYGROUP_ALL)
                    return {KeygroupSelection::all(), {}};
                return {std::nullopt, "keygroup must be a number from 1 or \"all\" (got '" + given->get<std::string>() + "')."};
            }
            const auto number = wholeNumber(*given);
            if (!number || *number < FIRST_KEYGROUP)
                return {std::nullopt, "keygroup must be a number from 1 or \"all\" (got " + given->dump() + ")."};
            return {KeygroupSelection::of(static_cast<int>(*number)), {}};
        }

        // The `zone` argument: absent or "all" is every zone, a number from 1 to 4 is one.
        struct ZoneArgument
        {
            std::optional<ZoneSelection> selection;
            bool given = false;
            std::string problem;
        };

        ZoneArgument zoneArgument(const json& arguments)
        {
            const auto given = arguments.find("zone");
            if (given == arguments.end() || given->is_null())
                return {ZoneSelection::all(), false, {}};
            const std::string rule = "zone must be a number from " + std::to_string(FIRST_ZONE) + " to " + std::to_string(LAST_ZONE) +
                                     " or \"all\" (got " + given->dump() + ").";
            if (given->is_string())
            {
                if (normalizeText(given->get<std::string>()) == KEYGROUP_ALL)
                    return {ZoneSelection::all(), true, {}};
                return {std::nullopt, true, rule};
            }
            const auto number = wholeNumber(*given);
            if (!number || *number < FIRST_ZONE || *number > LAST_ZONE)
                return {std::nullopt, true, rule};
            return {ZoneSelection::of(static_cast<int>(*number)), true, {}};
        }

        std::size_t distinctKeygroups(const std::vector<ParameterValue>& values)
        {
            std::set<int> keygroups;
            for (const ParameterValue& value : values)
                keygroups.insert(value.keygroup.value_or(0));
            return keygroups.size();
        }

        std::size_t distinctZones(const std::vector<ParameterValue>& values)
        {
            std::set<int> zones;
            for (const ParameterValue& value : values)
                zones.insert(value.zone.value_or(0));
            return zones.size();
        }

        std::string describeTarget(const ParameterDefinition& parameter, const std::vector<ParameterValue>& values,
                                   KeygroupSelection selection, ZoneSelection zones)
        {
            if (parameter.scope == ParameterScope::Program)
                return "program";
            std::string target = selection.keygroup ? "keygroup " + std::to_string(*selection.keygroup)
                                                    : "all " + plural(static_cast<std::int64_t>(distinctKeygroups(values)), "keygroup");
            if (parameter.scope == ParameterScope::Zone)
                target += zones.zone ? ", zone " + std::to_string(*zones.zone)
                                     : ", all " + plural(static_cast<std::int64_t>(distinctZones(values)), "zone");
            return target;
        }

        /// One line for one parameter: "filter cutoff = 80 (all 3 keygroups)", or one value per keygroup when they differ.
        std::string describeReading(const ParameterDefinition& parameter, const std::vector<ParameterValue>& values,
                                    KeygroupSelection selection, ZoneSelection zones)
        {
            const bool allEqual = std::all_of(values.begin(), values.end(), [&values](const ParameterValue& value) {
                return value.value == values.front().value;
            });
            if (allEqual)
                return parameter.name + " = " + describeValue(parameter, values.front().value) + " (" +
                       describeTarget(parameter, values, selection, zones) + ")";

            std::vector<std::string> perValue;
            for (const ParameterValue& value : values)
            {
                std::string place;
                if (!selection.keygroup)
                    place = "keygroup " + std::to_string(value.keygroup.value_or(0));
                if (value.zone && !zones.zone)
                    place += std::string(place.empty() ? "" : " ") + "zone " + std::to_string(*value.zone);
                perValue.push_back(place + " = " + describeValue(parameter, value.value));
            }
            return parameter.name + ": " + joined(perValue, ", ");
        }

        std::string describeParameter(const ParameterDefinition& parameter)
        {
            std::string text = "- " + parameter.name + ": ";
            text += parameter.kind == ParameterKind::Choice ? "one of: " + describeRange(parameter) : describeRange(parameter);
            text += ". " + parameter.description;
            switch (parameter.scope)
            {
                case ParameterScope::Keygroup:
                    text += " Per keygroup.";
                    break;
                case ParameterScope::Zone:
                    text += " Per zone (and keygroup).";
                    break;
                case ParameterScope::Program:
                    text += " Per program.";
                    break;
            }
            if (!parameter.aliases.empty())
                text += " Also called: " + joined(parameter.aliases, ", ") + ".";
            return text;
        }

        std::string parameterListFor(const ParameterCatalogue& catalogue, const std::vector<const GroupDefinition*>& groups)
        {
            std::string text;
            for (const GroupDefinition* group : groups)
            {
                text += "\n" + group->name + ": " + group->description + "\n";
                for (const ParameterDefinition* parameter : catalogue.parametersInGroup(*group))
                    text += describeParameter(*parameter) + "\n";
            }
            return text;
        }

        std::string groupNames(const ParameterCatalogue& catalogue)
        {
            std::vector<std::string> names;
            for (const GroupDefinition& group : catalogue.groups())
                names.push_back(group.name);
            return joined(names, ", ");
        }

        std::string unknownParameterProblem(const std::string& said, const NameResolution& resolution)
        {
            std::string text = "Unknown parameter '" + said + "'.";
            if (resolution.suggestions.empty())
                return text + " Call list_parameters to see the names.";
            return text + " Did you mean: " + joined(resolution.suggestions, ", ") + "?";
        }

        ToolDefinition definition(const char* name, const char* title, std::string description, json schema, bool readOnly,
                                  bool idempotent)
        {
            ToolDefinition tool;
            tool.name = name;
            tool.title = title;
            tool.description = std::move(description);
            tool.inputSchema = std::move(schema);
            tool.annotations.readOnly = readOnly;
            tool.annotations.destructive = false;
            tool.annotations.idempotent = idempotent;
            tool.annotations.openWorld = false;
            return tool;
        }

        json objectSchema(json properties = json::object(), json required = json::array())
        {
            json schema{{"type", "object"}, {"properties", std::move(properties)}, {"additionalProperties", false}};
            if (!required.empty())
                schema["required"] = std::move(required);
            return schema;
        }

        /// The problem with a program name the sampler would not take, or nothing.
        std::optional<std::string> programNameProblem(const json& argument, const char* field)
        {
            if (!argument.is_string())
                return "The argument '" + std::string(field) + "' must be a string.";
            const std::string name = argument.get<std::string>();
            const std::string accepted = "A program name is 1 to " + std::to_string(MAX_PROGRAM_NAME_LENGTH) +
                                         " characters, letters, digits, spaces and punctuation of plain ASCII.";
            if (name.empty() || name.size() > MAX_PROGRAM_NAME_LENGTH)
                return "The name \"" + name + "\" has " + std::to_string(name.size()) + " characters. " + accepted;
            if (!std::all_of(name.begin(), name.end(), [](char c) { return c >= FIRST_PRINTABLE && c <= LAST_PRINTABLE; }))
                return "The name \"" + name + "\" has a character the sampler does not take. " + accepted;
            return std::nullopt;
        }

        json keygroupSchema()
        {
            return json{{"type", json::array({"integer", "string"})}, {"description", KEYGROUP_ARGUMENT_DESCRIPTION}};
        }

        json zoneSchema()
        {
            return json{{"type", json::array({"integer", "string"})}, {"description", ZONE_ARGUMENT_DESCRIPTION}};
        }

        std::string notAZoneParameter(const ParameterDefinition& parameter)
        {
            return parameter.name + " is not a zone parameter: leave 'zone' out (the zone parameters are in the group \"zone\").";
        }
    }

    std::string programEditingInstructions()
    {
        return "This server edits a program of an AKAI S5000/S6000 sampler. Call get_status to see which program is "
               "current, list_programs and select_program to change it, and list_parameters to learn the parameter names "
               "and what each accepts: the filter, the amplitude envelope, the filter envelope and the two LFOs. Use "
               "get_parameters to read values and set_parameter to change one. Values are in the sampler's own units "
               "(0 to 100 for most), signed values are plain signed numbers, and choices are named as the sampler's "
               "screen names them (for example \"2-POLE LP+\"). create_program, rename_program and delete_program change the "
               "list of programs: delete_program deletes only the current program and only when its name is given as "
               "'confirm'. Changes act on the sampler's memory, not on disk; nothing is saved by this server.";
    }

    std::vector<Tool> makeProgramEditingTools(SamplerGateway& gateway, const ParameterCatalogue& catalogue)
    {
        std::vector<Tool> tools;

        // get_status
        tools.push_back(Tool{
            definition("get_status", "Sampler status",
                       "Says whether the sampler answers, how many programs are in its memory and which one is current "
                       "(with its number of keygroups). Call it first.",
                       objectSchema(), true, true),
            [&gateway](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {}))
                    return *refused;
                const auto status = gateway.status();
                if (!status.ok())
                    return failure(status.problem);
                std::string text = "Connected to the sampler (DeviceID " + std::to_string(status.value->deviceId) + ").\n";
                if (status.value->programCount == 0)
                    return ok(text + "The sampler holds no program.");
                text += plural(status.value->programCount, "program") + " in memory.\n";
                if (status.value->currentProgram)
                    text += "Current program: " + *status.value->currentProgram + ", " +
                            plural(status.value->keygroupCount.value_or(0), "keygroup") + ".";
                else
                    text += "No program is selected: use select_program.";
                return ok(std::move(text));
            }});

        // list_programs
        tools.push_back(Tool{
            definition("list_programs", "List programs", "Lists the programs in the sampler's memory with their positions (from 0).",
                       objectSchema(), true, true),
            [&gateway](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {}))
                    return *refused;
                const auto programs = gateway.listPrograms();
                if (!programs.ok())
                    return failure(programs.problem);
                if (programs.value->empty())
                    return ok("The sampler holds no program.");
                std::string text = "Programs in memory (" + std::to_string(programs.value->size()) + "):\n";
                for (const ProgramEntry& program : *programs.value)
                    text += std::to_string(program.index) + ": " + program.name + "\n";
                return ok(std::move(text));
            }});

        // select_program
        tools.push_back(Tool{
            definition("select_program", "Select a program",
                       std::string("Makes a program of the sampler's memory the current one, by its name or its position (from 0, "
                                   "see list_programs). Every later edit acts on it. Give exactly one of name and index. ") +
                           MEMORY_NOTICE,
                       objectSchema(json{{"name", {{"type", "string"}, {"description", "The program's name."}}},
                                         {"index", {{"type", "integer"}, {"minimum", 0}, {"description", "The program's position, from 0."}}}}),
                       false, true),
            [&gateway](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"name", "index"}))
                    return *refused;
                const bool hasName = arguments.contains("name");
                const bool hasIndex = arguments.contains("index");
                if (hasName == hasIndex)
                    return failure(hasName ? "Give either 'name' or 'index', not both."
                                           : "Give either 'name' or 'index'.");

                Outcome<ProgramInfo> selected;
                if (hasName)
                {
                    if (!arguments.at("name").is_string())
                        return failure("The argument 'name' must be a string.");
                    selected = gateway.selectProgramByName(arguments.at("name").get<std::string>());
                }
                else
                {
                    const auto index = wholeNumber(arguments.at("index"));
                    if (!index || *index < 0 || *index > MAX_PROGRAM_INDEX)
                        return failure("The argument 'index' must be a whole number from 0 to " +
                                       std::to_string(MAX_PROGRAM_INDEX) + ".");
                    selected = gateway.selectProgramByIndex(static_cast<int>(*index));
                }
                if (!selected.ok())
                    return failure(selected.problem);
                return ok("Selected the program \"" + selected.value->name + "\" (" +
                          plural(selected.value->keygroupCount, "keygroup") + ").");
            }});

        // list_parameters
        tools.push_back(Tool{
            definition("list_parameters", "List parameters",
                       "Lists the parameters of a program that can be read and set: their names, what they accept and what "
                       "they do. Call it before get_parameters or set_parameter to learn the names, and optionally give a "
                       "group to list only that group.",
                       objectSchema(json{{"group",
                                          {{"type", "string"},
                                           {"description",
                                            "One group: " + groupNames(catalogue) + " (\"lfo\" is both LFOs)."}}}}),
                       true, true),
            [&catalogue](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"group"}))
                    return *refused;
                std::vector<const GroupDefinition*> groups;
                if (arguments.contains("group"))
                {
                    if (!arguments.at("group").is_string())
                        return failure("The argument 'group' must be a string.");
                    groups = catalogue.findGroups(arguments.at("group").get<std::string>());
                    if (groups.empty())
                        return failure("Unknown group '" + arguments.at("group").get<std::string>() + "'. The groups are: " +
                                       groupNames(catalogue) + ".");
                }
                else
                {
                    for (const GroupDefinition& group : catalogue.groups())
                        groups.push_back(&group);
                }
                return ok("Parameters (values are in the sampler's own units):\n" + parameterListFor(catalogue, groups));
            }});

        // get_parameters
        tools.push_back(Tool{
            definition("get_parameters", "Read parameters",
                       "Reads the current values of parameters of the current program: a whole group, or a list of parameter "
                       "names (see list_parameters). Give exactly one of group and parameters. Reading a keygroup parameter "
                       "moves the sampler's current keygroup selection.",
                       objectSchema(json{{"group", {{"type", "string"}, {"description", "A group, for example \"filter\" or \"lfo 1\"."}}},
                                         {"parameters",
                                          {{"type", "array"},
                                           {"items", {{"type", "string"}}},
                                           {"description", "Parameter names, for example [\"filter cutoff\", \"filter resonance\"]."}}},
                                         {"keygroup", keygroupSchema()},
                                         {"zone", zoneSchema()}}),
                       false, true),
            [&gateway, &catalogue](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"group", "parameters", "keygroup", "zone"}))
                    return *refused;
                const bool hasGroup = arguments.contains("group");
                const bool hasList = arguments.contains("parameters");
                if (hasGroup == hasList)
                    return failure(hasGroup ? "Give either 'group' or 'parameters', not both."
                                            : "Give either 'group' or 'parameters'.");
                const KeygroupArgument keygroup = keygroupArgument(arguments);
                if (!keygroup.selection)
                    return failure(keygroup.problem);
                const ZoneArgument zone = zoneArgument(arguments);
                if (!zone.selection)
                    return failure(zone.problem);

                std::vector<const ParameterDefinition*> wanted;
                if (hasGroup)
                {
                    if (!arguments.at("group").is_string())
                        return failure("The argument 'group' must be a string.");
                    const auto groups = catalogue.findGroups(arguments.at("group").get<std::string>());
                    if (groups.empty())
                        return failure("Unknown group '" + arguments.at("group").get<std::string>() + "'. The groups are: " +
                                       groupNames(catalogue) + ".");
                    for (const GroupDefinition* group : groups)
                    {
                        const auto members = catalogue.parametersInGroup(*group);
                        wanted.insert(wanted.end(), members.begin(), members.end());
                    }
                }
                else
                {
                    const json& names = arguments.at("parameters");
                    if (!names.is_array() || names.empty() ||
                        !std::all_of(names.begin(), names.end(), [](const json& name) { return name.is_string(); }))
                        return failure("The argument 'parameters' must be a list of parameter names.");
                    std::vector<std::string> problems;
                    for (const json& name : names)
                    {
                        const auto resolution = catalogue.resolveName(name.get<std::string>());
                        if (resolution.parameter == nullptr)
                            problems.push_back(unknownParameterProblem(name.get<std::string>(), resolution));
                        else
                            wanted.push_back(resolution.parameter);
                    }
                    if (!problems.empty())
                        return failure(joined(problems, "\n"));
                }

                if (zone.given)
                {
                    for (const ParameterDefinition* parameter : wanted)
                    {
                        if (parameter->scope != ParameterScope::Zone)
                            return failure(notAZoneParameter(*parameter));
                    }
                }
                std::string text;
                for (const ParameterDefinition* parameter : wanted)
                {
                    const auto read = gateway.readParameter(*parameter, *keygroup.selection, *zone.selection);
                    if (!read.ok())
                        return failure(text + read.problem);
                    text += describeReading(*parameter, *read.value, *keygroup.selection, *zone.selection) + "\n";
                }
                return ok(std::move(text));
            }});

        // set_parameter
        tools.push_back(Tool{
            definition("set_parameter", "Set a parameter",
                       std::string("Sets one parameter of the current program (see list_parameters for the names and what each "
                                   "accepts), then reads it back from the sampler and reports what it holds. The value is a "
                                   "number in the sampler's own units, or the name of a choice such as \"2-POLE LP+\", or "
                                   "on/off. Nothing is sent when the name or the value is not valid. ") +
                           MEMORY_NOTICE,
                       objectSchema(json{{"parameter", {{"type", "string"}, {"description", "The parameter's name, for example \"filter cutoff\"."}}},
                                         {"value",
                                          {{"type", json::array({"number", "string", "boolean"})},
                                           {"description", "A number, a choice label, or on/off (true/false)."}}},
                                         {"keygroup", keygroupSchema()},
                                         {"zone", zoneSchema()}},
                                    json::array({"parameter", "value"})),
                       false, true),
            [&gateway, &catalogue](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"parameter", "value", "keygroup", "zone"}))
                    return *refused;
                if (!arguments.at("parameter").is_string())
                    return failure("The argument 'parameter' must be a string.");
                const std::string said = arguments.at("parameter").get<std::string>();
                const NameResolution resolution = catalogue.resolveName(said);
                if (resolution.parameter == nullptr)
                    return failure(unknownParameterProblem(said, resolution));
                const ParameterDefinition& parameter = *resolution.parameter;

                const KeygroupArgument keygroup = keygroupArgument(arguments);
                if (!keygroup.selection)
                    return failure(keygroup.problem);
                if (parameter.scope == ParameterScope::Program && keygroup.selection->keygroup)
                    return failure(parameter.name + " belongs to the program, not to a keygroup: leave 'keygroup' out (or use \"" +
                                   KEYGROUP_ALL + "\").");
                const ZoneArgument zone = zoneArgument(arguments);
                if (!zone.selection)
                    return failure(zone.problem);
                if (zone.given && parameter.scope != ParameterScope::Zone)
                    return failure(notAZoneParameter(parameter));

                const json& value = arguments.at("value");
                ValueResolution resolved;
                if (value.is_string())
                    resolved = resolveValue(parameter, value.get<std::string>());
                else if (value.is_boolean())
                {
                    if (parameter.kind != ParameterKind::Switch)
                        return failure(parameter.name + " takes " + describeRange(parameter) + ", not true or false.");
                    resolved = resolveValue(parameter, std::int64_t{value.get<bool>() ? 1 : 0});
                }
                else if (value.is_number())
                {
                    const auto whole = wholeNumber(value);
                    resolved = whole ? resolveValue(parameter, *whole) : resolveValue(parameter, value.dump());
                }
                else
                    return failure("The argument 'value' must be a number, a text or true/false.");
                if (!resolved.value)
                    return failure(resolved.problem);

                const auto written = gateway.writeParameter(parameter, *resolved.value, *keygroup.selection, *zone.selection);
                if (!written.ok())
                    return failure(written.problem);
                return ok(describeReading(parameter, *written.value, *keygroup.selection, *zone.selection) +
                          ", as read back from the sampler's memory.");
            }});

        return tools;
    }

    std::vector<Tool> makeProgramStructureTools(SamplerGateway& gateway)
    {
        std::vector<Tool> tools;

        // create_program
        ToolDefinition create =
            definition("create_program", "Create a program",
                       std::string("Creates a new program with the given name and number of keygroups (1 to 99) and makes it the "
                                   "current program, so the next edits act on it. ") +
                           MEMORY_NOTICE,
                       objectSchema(json{{"name", {{"type", "string"}, {"description", "The new program's name, 1 to 12 characters."}}},
                                         {"keygroups",
                                          {{"type", "integer"},
                                           {"minimum", MIN_NEW_KEYGROUPS},
                                           {"maximum", MAX_NEW_KEYGROUPS},
                                           {"description", "How many keygroups the program has, 1 to 99."}}}},
                                    json::array({"name", "keygroups"})),
                       false, false);
        tools.push_back(Tool{std::move(create), [&gateway](const json& arguments) {
                                 if (const auto refused = unknownArguments(arguments, {"name", "keygroups"}))
                                     return *refused;
                                 if (!arguments.contains("name") || !arguments.contains("keygroups"))
                                     return failure("Give both 'name' and 'keygroups'.");
                                 if (const auto problem = programNameProblem(arguments.at("name"), "name"))
                                     return failure(*problem);
                                 const auto count = wholeNumber(arguments.at("keygroups"));
                                 if (!count || *count < MIN_NEW_KEYGROUPS || *count > MAX_NEW_KEYGROUPS)
                                     return failure("The argument 'keygroups' must be a whole number from " + std::to_string(MIN_NEW_KEYGROUPS) +
                                                    " to " + std::to_string(MAX_NEW_KEYGROUPS) + ".");
                                 const auto created = gateway.createProgram(arguments.at("name").get<std::string>(), static_cast<int>(*count));
                                 if (!created.ok())
                                     return failure(created.problem);
                                 return ok("Created the program \"" + created.value->name + "\" with " +
                                           plural(created.value->keygroupCount, "keygroup") + "; it is now the current program.");
                             }});

        // rename_program
        ToolDefinition rename =
            definition("rename_program", "Rename the current program",
                       std::string("Renames the current program (see get_status) and reads the new name back. ") + MEMORY_NOTICE,
                       objectSchema(json{{"name", {{"type", "string"}, {"description", "The new name, 1 to 12 characters."}}}},
                                    json::array({"name"})),
                       false, true);
        tools.push_back(Tool{std::move(rename), [&gateway](const json& arguments) {
                                 if (const auto refused = unknownArguments(arguments, {"name"}))
                                     return *refused;
                                 if (!arguments.contains("name"))
                                     return failure("Give the new 'name'.");
                                 if (const auto problem = programNameProblem(arguments.at("name"), "name"))
                                     return failure(*problem);
                                 const auto renamed = gateway.renameCurrentProgram(arguments.at("name").get<std::string>());
                                 if (!renamed.ok())
                                     return failure(renamed.problem);
                                 return ok("Renamed the program \"" + renamed.value->oldName + "\" to \"" + renamed.value->newName +
                                           "\", as read back from the sampler's memory.");
                             }});

        // delete_program
        ToolDefinition remove =
            definition("delete_program", "Delete the current program",
                       std::string("Deletes the CURRENT program (see get_status), and only if 'confirm' is exactly its name: "
                                   "otherwise nothing is deleted and the answer says which program is current. It cannot be "
                                   "undone from here. ") +
                           MEMORY_NOTICE,
                       objectSchema(json{{"confirm", {{"type", "string"}, {"description", "The exact name of the current program, to confirm the deletion."}}}},
                                    json::array({"confirm"})),
                       false, false);
        remove.annotations.destructive = true;
        tools.push_back(Tool{std::move(remove), [&gateway](const json& arguments) {
                                 if (const auto refused = unknownArguments(arguments, {"confirm"}))
                                     return *refused;
                                 if (!arguments.contains("confirm") || !arguments.at("confirm").is_string())
                                     return failure("Give 'confirm', the exact name of the current program.");
                                 const std::string confirm = arguments.at("confirm").get<std::string>();
                                 const auto deletion = gateway.deleteCurrentProgram(confirm);
                                 if (!deletion.ok())
                                     return failure(deletion.problem);
                                 if (!deletion.value->deleted)
                                     return failure("The current program is \"" + deletion.value->name + "\", not \"" + confirm +
                                                    "\": nothing was deleted. Select the program to delete first, then confirm with its name.");
                                 std::string text = "Deleted the program \"" + deletion.value->name + "\".";
                                 if (deletion.value->remaining)
                                     text += " The sampler now holds " + plural(*deletion.value->remaining, "program") +
                                             "; use select_program to choose the one to edit.";
                                 return ok(std::move(text));
                             }});

        return tools;
    }

    std::vector<Tool> makeAllTools(SamplerGateway& gateway, const ParameterCatalogue& catalogue)
    {
        std::vector<Tool> tools = makeProgramEditingTools(gateway, catalogue);
        for (Tool& tool : makeProgramStructureTools(gateway))
            tools.push_back(std::move(tool));
        return tools;
    }
}
