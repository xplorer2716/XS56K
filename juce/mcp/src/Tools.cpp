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
            if (parameter.scope == ParameterScope::Sample)
                return "current sample";
            if (parameter.scope == ParameterScope::MultiPart)
                return "current multi";
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
                case ParameterScope::Sample:
                    text += " Of the current sample.";
                    break;
                case ParameterScope::MultiPart:
                    text += " Per part of the current multi.";
                    break;
            }
            if (parameter.readOnly)
                text += " This parameter is read-only: it can be read, not set.";
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

        /// A value given to a tool, as the catalogue resolves it: a number, a label, on/off or true/false.
        ValueResolution resolveJsonValue(const ParameterDefinition& parameter, const json& value)
        {
            if (value.is_string())
                return resolveValue(parameter, value.get<std::string>());
            if (value.is_boolean())
            {
                if (parameter.kind != ParameterKind::Switch)
                    return {std::nullopt, parameter.name + " takes " + describeRange(parameter) + ", not true or false."};
                return resolveValue(parameter, std::int64_t{value.get<bool>() ? 1 : 0});
            }
            if (value.is_number())
            {
                const auto whole = wholeNumber(value);
                return whole ? resolveValue(parameter, *whole) : resolveValue(parameter, value.dump());
            }
            return {std::nullopt, "The argument 'value' must be a number, a text or true/false."};
        }

        /// A tool that can lose data (a save that replaces a file) says so in its annotations.
        ToolDefinition markDestructive(ToolDefinition tool)
        {
            tool.annotations.destructive = true;
            return tool;
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
               "'confirm'. list_samples, select_sample, get_sample_parameters and set_sample_parameter edit the samples in the "
               "same way (list_parameters with the domain \"sample\" names their parameters); no tool creates, deletes or "
               "loads a sample. list_multis, select_multi, get_multi_parameters and set_multi_parameter edit the parts of a "
               "multi (parts are numbered from 1; no tool creates, deletes or renames a multi). Changes act on the sampler's "
               "memory, not on disk; nothing is saved by this server.";
    }

    std::string diskInstructions(bool refreshOffered)
    {
        const std::string browse =
            " The disk tools (list_disks, select_disk, list_disk_contents, open_folder, close_folder) browse the sampler's own "
            "disks, not the computer's files; ";
        if (refreshOffered)
            return browse +
                   "list_disks refreshes the sampler's disk list only with refresh true, which is slow and "
                   "has left a real S5000 answering nothing until it was switched off and on.";
        return browse + "this server never asks the sampler to refresh its disk list (it has left a real S5000 answering nothing).";
    }

    std::vector<Tool> makeProgramEditingTools(SamplerGateway& gateway, const ParameterCatalogue& catalogue, ExtraCatalogues extra)
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
                       "Lists the parameters that can be read and set: their names, what they accept and what they do. Call it "
                       "before get_parameters or set_parameter to learn the names, and optionally give a group to list only that "
                       "group. The parameters are those of a program (the default) unless a domain is given.",
                       objectSchema(json{{"group",
                                          {{"type", "string"},
                                           {"description",
                                            "One group of the domain: for a program " + groupNames(catalogue) +
                                                " (\"lfo\" is both LFOs)."}}},
                                         {"domain",
                                          {{"type", "string"},
                                           {"description", std::string("What the parameters belong to: \"program\" (the default)") +
                                                               (extra.sample != nullptr ? ", \"sample\" (the current sample's)" : "") +
                                                               (extra.multi != nullptr ? ", \"multi\" (a part of the current multi)" : "") + "."}}}}),
                       true, true),
            [&catalogue, extra](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"group", "domain"}))
                    return *refused;
                const ParameterCatalogue* chosen = &catalogue;
                if (arguments.contains("domain"))
                {
                    if (!arguments.at("domain").is_string())
                        return failure("The argument 'domain' must be a string.");
                    const std::string domain = normalizeText(arguments.at("domain").get<std::string>());
                    if (domain == "program")
                        chosen = &catalogue;
                    else if (domain == "sample" && extra.sample != nullptr)
                        chosen = extra.sample;
                    else if (domain == "multi" && extra.multi != nullptr)
                        chosen = extra.multi;
                    else
                        return failure("Unknown domain '" + arguments.at("domain").get<std::string>() + "'. The domains are: program" +
                                       (extra.sample != nullptr ? ", sample" : "") + (extra.multi != nullptr ? ", multi" : "") + ".");
                }
                std::vector<const GroupDefinition*> groups;
                if (arguments.contains("group"))
                {
                    if (!arguments.at("group").is_string())
                        return failure("The argument 'group' must be a string.");
                    groups = chosen->findGroups(arguments.at("group").get<std::string>());
                    if (groups.empty())
                        return failure("Unknown group '" + arguments.at("group").get<std::string>() + "'. The groups are: " +
                                       groupNames(*chosen) + ".");
                }
                else
                {
                    for (const GroupDefinition& group : chosen->groups())
                        groups.push_back(&group);
                }
                return ok("Parameters (values are in the sampler's own units):\n" + parameterListFor(*chosen, groups));
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

                const ValueResolution resolved = resolveJsonValue(parameter, arguments.at("value"));
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

    std::vector<Tool> makeSampleTools(SamplerGateway& gateway, const ParameterCatalogue& sampleCatalogue)
    {
        std::vector<Tool> tools;

        // list_samples
        tools.push_back(Tool{
            definition("list_samples", "List samples",
                       "Lists the samples in the sampler's memory with their positions (from 0) and says which one is current.",
                       objectSchema(), true, true),
            [&gateway](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {}))
                    return *refused;
                const auto listing = gateway.listSamples();
                if (!listing.ok())
                    return failure(listing.problem);
                if (listing.value->samples.empty())
                    return ok("The sampler holds no sample.");
                std::string text = "Samples in memory (" + std::to_string(listing.value->samples.size()) + "):\n";
                for (const SampleEntry& sample : listing.value->samples)
                {
                    const bool current = listing.value->current && *listing.value->current == sample.index;
                    text += std::to_string(sample.index) + ": " + sample.name + (current ? " (current)" : "") + "\n";
                }
                if (!listing.value->current)
                    text += "No sample is selected: use select_sample.\n";
                return ok(std::move(text));
            }});

        // select_sample
        tools.push_back(Tool{
            definition("select_sample", "Select a sample",
                       std::string("Makes a sample of the sampler's memory the current one, by its name or its position (from 0, see "
                                   "list_samples). get_sample_parameters and set_sample_parameter act on it. Give exactly one of name "
                                   "and index. ") +
                           MEMORY_NOTICE,
                       objectSchema(json{{"name", {{"type", "string"}, {"description", "The sample's name."}}},
                                         {"index", {{"type", "integer"}, {"minimum", 0}, {"description", "The sample's position, from 0."}}}}),
                       false, true),
            [&gateway](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"name", "index"}))
                    return *refused;
                const bool hasName = arguments.contains("name");
                const bool hasIndex = arguments.contains("index");
                if (hasName == hasIndex)
                    return failure(hasName ? "Give either 'name' or 'index', not both." : "Give either 'name' or 'index'.");
                Outcome<SampleEntry> selected;
                if (hasName)
                {
                    if (!arguments.at("name").is_string())
                        return failure("The argument 'name' must be a string.");
                    selected = gateway.selectSampleByName(arguments.at("name").get<std::string>());
                }
                else
                {
                    const auto index = wholeNumber(arguments.at("index"));
                    if (!index || *index < 0 || *index > MAX_PROGRAM_INDEX)
                        return failure("The argument 'index' must be a whole number from 0 to " + std::to_string(MAX_PROGRAM_INDEX) + ".");
                    selected = gateway.selectSampleByIndex(static_cast<int>(*index));
                }
                if (!selected.ok())
                    return failure(selected.problem);
                return ok("Selected the sample \"" + selected.value->name + "\" (position " + std::to_string(selected.value->index) + ").");
            }});

        // get_sample_parameters
        tools.push_back(Tool{
            definition("get_sample_parameters", "Read sample parameters",
                       "Reads the current sample's parameters (see list_parameters with the domain \"sample\"): a whole group, or "
                       "a list of parameter names. Give exactly one of group and parameters. Select a sample first.",
                       objectSchema(json{{"group", {{"type", "string"}, {"description", "A group: " + groupNames(sampleCatalogue) + "."}}},
                                         {"parameters",
                                          {{"type", "array"},
                                           {"items", {{"type", "string"}}},
                                           {"description", "Parameter names, for example [\"sample loop start\", \"sample loop end\"]."}}}}),
                       true, true),
            [&gateway, &sampleCatalogue](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"group", "parameters"}))
                    return *refused;
                const bool hasGroup = arguments.contains("group");
                const bool hasList = arguments.contains("parameters");
                if (hasGroup == hasList)
                    return failure(hasGroup ? "Give either 'group' or 'parameters', not both." : "Give either 'group' or 'parameters'.");

                std::vector<const ParameterDefinition*> wanted;
                if (hasGroup)
                {
                    if (!arguments.at("group").is_string())
                        return failure("The argument 'group' must be a string.");
                    const auto groups = sampleCatalogue.findGroups(arguments.at("group").get<std::string>());
                    if (groups.empty())
                        return failure("Unknown group '" + arguments.at("group").get<std::string>() + "'. The groups are: " +
                                       groupNames(sampleCatalogue) + ".");
                    for (const GroupDefinition* group : groups)
                    {
                        const auto members = sampleCatalogue.parametersInGroup(*group);
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
                        const auto resolution = sampleCatalogue.resolveName(name.get<std::string>());
                        if (resolution.parameter == nullptr)
                            problems.push_back(unknownParameterProblem(name.get<std::string>(), resolution));
                        else
                            wanted.push_back(resolution.parameter);
                    }
                    if (!problems.empty())
                        return failure(joined(problems, "\n"));
                }

                std::string text;
                for (const ParameterDefinition* parameter : wanted)
                {
                    const auto read = gateway.readSampleParameter(*parameter);
                    if (!read.ok())
                        return failure(text + read.problem);
                    text += describeReading(*parameter, {ParameterValue{std::nullopt, std::nullopt, *read.value}}, KeygroupSelection::all(),
                                            ZoneSelection::all()) + "\n";
                }
                return ok(std::move(text));
            }});

        // set_sample_parameter
        tools.push_back(Tool{
            definition("set_sample_parameter", "Set a sample parameter",
                       std::string("Sets one parameter of the current sample (see list_parameters with the domain \"sample\"), then reads it "
                                   "back from the sampler and reports what it holds. Positions are in sample points. Parameters of the "
                                   "group \"info\" are read-only. Select a sample first. ") +
                           MEMORY_NOTICE,
                       objectSchema(json{{"parameter", {{"type", "string"}, {"description", "The parameter's name, for example \"sample loop end\"."}}},
                                         {"value",
                                          {{"type", json::array({"number", "string"})},
                                           {"description", "A number, or a choice label such as \"loop in rel\"."}}}},
                                    json::array({"parameter", "value"})),
                       false, true),
            [&gateway, &sampleCatalogue](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"parameter", "value"}))
                    return *refused;
                if (!arguments.at("parameter").is_string())
                    return failure("The argument 'parameter' must be a string.");
                const std::string said = arguments.at("parameter").get<std::string>();
                const NameResolution resolution = sampleCatalogue.resolveName(said);
                if (resolution.parameter == nullptr)
                    return failure(unknownParameterProblem(said, resolution));
                const ParameterDefinition& parameter = *resolution.parameter;
                if (parameter.readOnly)
                    return failure(parameter.name + " is read-only: the sampler reports it and it cannot be set.");
                const ValueResolution resolved = resolveJsonValue(parameter, arguments.at("value"));
                if (!resolved.value)
                    return failure(resolved.problem);
                const auto written = gateway.writeSampleParameter(parameter, *resolved.value);
                if (!written.ok())
                    return failure(written.problem);
                return ok(describeReading(parameter, {ParameterValue{std::nullopt, std::nullopt, *written.value}}, KeygroupSelection::all(),
                                          ZoneSelection::all()) +
                          ", as read back from the sampler's memory.");
            }});

        return tools;
    }

    namespace
    {
        constexpr int MAX_PARTS = 128;

        // The `part` argument of the multi tools: a number from 1, or "all".
        struct PartArgument
        {
            std::optional<PartSelection> selection;
            bool given = false;
            std::string problem;
        };

        PartArgument partArgument(const json& arguments)
        {
            const auto given = arguments.find("part");
            if (given == arguments.end() || given->is_null())
                return {PartSelection::all(), false, {}};
            const std::string rule = "part must be a number from 1 to " + std::to_string(MAX_PARTS) + " or \"all\" (got " + given->dump() + ").";
            if (given->is_string())
            {
                if (normalizeText(given->get<std::string>()) == KEYGROUP_ALL)
                    return {PartSelection::all(), true, {}};
                return {std::nullopt, true, rule};
            }
            const auto number = wholeNumber(*given);
            if (!number || *number < 1 || *number > MAX_PARTS)
                return {std::nullopt, true, rule};
            return {PartSelection::of(static_cast<int>(*number)), true, {}};
        }

        /// "part level = 80 (part 3)", "part mute = on (all 32 parts)", or one value per part when they differ.
        std::string describePartReading(const ParameterDefinition& parameter, const std::vector<PartValue>& values, PartSelection parts)
        {
            const bool allEqual = std::all_of(values.begin(), values.end(), [&values](const PartValue& value) {
                return value.value == values.front().value;
            });
            if (allEqual)
                return parameter.name + " = " + describeValue(parameter, values.front().value) + " (" +
                       (parts.part ? "part " + std::to_string(*parts.part) : "all " + plural(static_cast<std::int64_t>(values.size()), "part")) + ")";
            std::vector<std::string> perPart;
            for (const PartValue& value : values)
                perPart.push_back("part " + std::to_string(value.part) + " = " + describeValue(parameter, value.value));
            return parameter.name + ": " + joined(perPart, ", ");
        }
    }

    std::vector<Tool> makeMultiTools(SamplerGateway& gateway, const ParameterCatalogue& multiCatalogue)
    {
        std::vector<Tool> tools;

        // list_multis
        tools.push_back(Tool{
            definition("list_multis", "List multis",
                       "Lists the multis in the sampler's memory with their positions (from 0), says which one is current and how many parts "
                       "it has.",
                       objectSchema(), true, true),
            [&gateway](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {}))
                    return *refused;
                const auto listing = gateway.listMultis();
                if (!listing.ok())
                    return failure(listing.problem);
                if (listing.value->multis.empty())
                    return ok("The sampler holds no multi.");
                std::string text = "Multis in memory (" + std::to_string(listing.value->multis.size()) + "):\n";
                for (const MultiEntry& multi : listing.value->multis)
                {
                    const bool current = listing.value->current && *listing.value->current == multi.index;
                    text += std::to_string(multi.index) + ": " + multi.name;
                    if (current)
                        text += " (current, " + plural(listing.value->currentPartCount.value_or(0), "part") + ")";
                    text += "\n";
                }
                if (!listing.value->current)
                    text += "No multi is selected: use select_multi.\n";
                return ok(std::move(text));
            }});

        // select_multi
        tools.push_back(Tool{
            definition("select_multi", "Select a multi",
                       std::string("Makes a multi of the sampler's memory the current one, by its name or its position (from 0, see "
                                   "list_multis). get_multi_parameters and set_multi_parameter act on it. Give exactly one of name and "
                                   "index. ") +
                           MEMORY_NOTICE,
                       objectSchema(json{{"name", {{"type", "string"}, {"description", "The multi's name."}}},
                                         {"index", {{"type", "integer"}, {"minimum", 0}, {"description", "The multi's position, from 0."}}}}),
                       false, true),
            [&gateway](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"name", "index"}))
                    return *refused;
                const bool hasName = arguments.contains("name");
                const bool hasIndex = arguments.contains("index");
                if (hasName == hasIndex)
                    return failure(hasName ? "Give either 'name' or 'index', not both." : "Give either 'name' or 'index'.");
                Outcome<MultiEntry> selected;
                if (hasName)
                {
                    if (!arguments.at("name").is_string())
                        return failure("The argument 'name' must be a string.");
                    selected = gateway.selectMultiByName(arguments.at("name").get<std::string>());
                }
                else
                {
                    const auto index = wholeNumber(arguments.at("index"));
                    if (!index || *index < 0 || *index > MAX_PROGRAM_INDEX)
                        return failure("The argument 'index' must be a whole number from 0 to " + std::to_string(MAX_PROGRAM_INDEX) + ".");
                    selected = gateway.selectMultiByIndex(static_cast<int>(*index));
                }
                if (!selected.ok())
                    return failure(selected.problem);
                return ok("Selected the multi \"" + selected.value->name + "\" (position " + std::to_string(selected.value->index) + ", " +
                          plural(selected.value->partCount, "part") + ").");
            }});

        const json partSchema{{"type", json::array({"integer", "string"})},
                              {"description", "Which part of the current multi: a number from 1, or \"all\"."}};

        // get_multi_parameters
        tools.push_back(Tool{
            definition("get_multi_parameters", "Read multi part parameters",
                       "Reads the parameters of the parts of the current multi (see list_parameters with the domain \"multi\"): a whole "
                       "group, or a list of parameter names, for one part or for every part (the default). Give exactly one of group and "
                       "parameters. Select a multi first.",
                       objectSchema(json{{"group", {{"type", "string"}, {"description", "A group: " + groupNames(multiCatalogue) + "."}}},
                                         {"parameters",
                                          {{"type", "array"},
                                           {"items", {{"type", "string"}}},
                                           {"description", "Parameter names, for example [\"part level\", \"part pan\"]."}}},
                                         {"part", partSchema}}),
                       true, true),
            [&gateway, &multiCatalogue](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"group", "parameters", "part"}))
                    return *refused;
                const bool hasGroup = arguments.contains("group");
                const bool hasList = arguments.contains("parameters");
                if (hasGroup == hasList)
                    return failure(hasGroup ? "Give either 'group' or 'parameters', not both." : "Give either 'group' or 'parameters'.");
                const PartArgument part = partArgument(arguments);
                if (!part.selection)
                    return failure(part.problem);

                std::vector<const ParameterDefinition*> wanted;
                if (hasGroup)
                {
                    if (!arguments.at("group").is_string())
                        return failure("The argument 'group' must be a string.");
                    const auto groups = multiCatalogue.findGroups(arguments.at("group").get<std::string>());
                    if (groups.empty())
                        return failure("Unknown group '" + arguments.at("group").get<std::string>() + "'. The groups are: " +
                                       groupNames(multiCatalogue) + ".");
                    for (const GroupDefinition* group : groups)
                    {
                        const auto members = multiCatalogue.parametersInGroup(*group);
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
                        const auto resolution = multiCatalogue.resolveName(name.get<std::string>());
                        if (resolution.parameter == nullptr)
                            problems.push_back(unknownParameterProblem(name.get<std::string>(), resolution));
                        else
                            wanted.push_back(resolution.parameter);
                    }
                    if (!problems.empty())
                        return failure(joined(problems, "\n"));
                }

                std::string text;
                for (const ParameterDefinition* parameter : wanted)
                {
                    const auto read = gateway.readMultiParameter(*parameter, *part.selection);
                    if (!read.ok())
                        return failure(text + read.problem);
                    text += describePartReading(*parameter, *read.value, *part.selection) + "\n";
                }
                return ok(std::move(text));
            }});

        // set_multi_parameter
        tools.push_back(Tool{
            definition("set_multi_parameter", "Set a multi part parameter",
                       std::string("Sets one parameter of one part of the current multi (see list_parameters with the domain \"multi\"), or "
                                   "of every part with part \"all\", then reads it back from the sampler and reports what it holds. The part "
                                   "is required. Select a multi first. ") +
                           MEMORY_NOTICE,
                       objectSchema(json{{"parameter", {{"type", "string"}, {"description", "The parameter's name, for example \"part level\"."}}},
                                         {"value",
                                          {{"type", json::array({"number", "string", "boolean"})},
                                           {"description", "A number, a choice label such as \"10B\" or \"RV3\", or on/off (true/false)."}}},
                                         {"part", partSchema}},
                                    json::array({"parameter", "value", "part"})),
                       false, true),
            [&gateway, &multiCatalogue](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"parameter", "value", "part"}))
                    return *refused;
                if (!arguments.contains("part"))
                    return failure("Give the 'part' to set: a number from 1, or \"all\".");
                if (!arguments.at("parameter").is_string())
                    return failure("The argument 'parameter' must be a string.");
                const std::string said = arguments.at("parameter").get<std::string>();
                const NameResolution resolution = multiCatalogue.resolveName(said);
                if (resolution.parameter == nullptr)
                    return failure(unknownParameterProblem(said, resolution));
                const ParameterDefinition& parameter = *resolution.parameter;
                const PartArgument part = partArgument(arguments);
                if (!part.selection)
                    return failure(part.problem);
                const ValueResolution resolved = resolveJsonValue(parameter, arguments.at("value"));
                if (!resolved.value)
                    return failure(resolved.problem);
                const auto written = gateway.writeMultiParameter(parameter, *resolved.value, *part.selection);
                if (!written.ok())
                    return failure(written.problem);
                return ok(describePartReading(parameter, *written.value, *part.selection) + ", as read back from the sampler's memory.");
            }});

        return tools;
    }

    namespace
    {
        constexpr const char* DISK_TYPES[] = {"floppy", "hard disk", "CD-ROM", "removable"};
        constexpr int DISK_TYPE_COUNT = 4;
        constexpr const char* DISK_FORMATS[] = {"other", "MSDOS", "FAT32", "ISO9660", "S1000", "S3000", "EMU", "ROLAND"};
        constexpr int DISK_FORMAT_COUNT = 8;

        constexpr const char* DISK_NOTICE =
            "It acts on the sampler's own disks, not on the computer's files, and changes nothing in the sampler's memory.";

        constexpr const char* DISK_FILES_NOTICE = "It acts on the sampler's own disks, not on the computer's files.";

        std::string diskType(int type)
        {
            return type >= 0 && type < DISK_TYPE_COUNT ? DISK_TYPES[type] : "type " + std::to_string(type);
        }

        std::string diskFormat(int format)
        {
            return format >= 0 && format < DISK_FORMAT_COUNT ? DISK_FORMATS[format] : "format " + std::to_string(format);
        }

        /// "programs 4 (was 3, added INIT)" or "samples 0 (was 0)".
        std::string describeKind(const char* kind, const std::vector<std::string>& before, const std::vector<std::string>& after)
        {
            std::vector<std::string> remaining = before;
            std::vector<std::string> added;
            for (const std::string& name : after)
            {
                const auto known = std::find(remaining.begin(), remaining.end(), name);
                if (known != remaining.end())
                    remaining.erase(known);
                else
                    added.push_back(name);
            }
            std::string text = std::string(kind) + " " + std::to_string(after.size()) + " (was " + std::to_string(before.size());
            if (!added.empty())
                text += ", added " + joined(added, ", ");
            return text + ")";
        }

        /// "program", "sample" or "multi" as a client says it, and its name for a message.
        std::optional<SaveKind> saveKindArgument(const json& arguments)
        {
            const auto given = arguments.find("kind");
            if (given == arguments.end() || !given->is_string())
                return std::nullopt;
            const std::string said = normalizeText(given->get<std::string>());
            if (said == "program")
                return SaveKind::Program;
            if (said == "sample")
                return SaveKind::Sample;
            if (said == "multi")
                return SaveKind::Multi;
            return std::nullopt;
        }

        const char* saveKindName(SaveKind kind)
        {
            return kind == SaveKind::Program ? "program" : kind == SaveKind::Sample ? "sample" : "multi";
        }

        /// A boolean argument that defaults to false, or the reason it is not one.
        std::optional<std::string> flagArgument(const json& arguments, const char* name, bool& value)
        {
            value = false;
            const auto given = arguments.find(name);
            if (given == arguments.end())
                return std::nullopt;
            if (!given->is_boolean())
                return "The argument '" + std::string(name) + "' must be true or false.";
            value = given->get<bool>();
            return std::nullopt;
        }

        std::vector<std::string> gainedFiles(const SaveOutcome& outcome)
        {
            std::vector<std::string> gained;
            for (const DiskFileEntry& file : outcome.filesAfter)
            {
                const bool known = std::any_of(outcome.filesBefore.begin(), outcome.filesBefore.end(),
                                               [&file](const DiskFileEntry& before) { return before.name == file.name; });
                if (!known)
                    gained.push_back(file.name);
            }
            return gained;
        }

        std::string describeMemory(const LoadOutcome& outcome)
        {
            return "Memory now: " + describeKind("programs", outcome.before.programs, outcome.after.programs) + ", " +
                   describeKind("samples", outcome.before.samples, outcome.after.samples) + ", " +
                   describeKind("multis", outcome.before.multis, outcome.after.multis) + ".";
        }

        std::string describeContents(const DiskContents& contents)
        {
            std::string text = "Current folder of the disk \"" + contents.diskName + "\": " + (contents.path.empty() ? "(root)" : contents.path) + "\n";
            if (contents.folders.empty())
                text += "Folders: none\n";
            else
            {
                text += "Folders (" + std::to_string(contents.folders.size()) + "):\n";
                for (const std::string& folder : contents.folders)
                    text += "  " + folder + "\n";
            }
            if (contents.files.empty())
                text += "Files: none\n";
            else
            {
                text += "Files (" + std::to_string(contents.files.size()) + "):\n";
                for (const DiskFileEntry& file : contents.files)
                    text += "  " + file.name + " (" + std::to_string(file.sizeBytes) + " bytes)\n";
            }
            return text;
        }
    }

    std::vector<Tool> makeDiskTools(SamplerGateway& gateway, bool offerRefresh)
    {
        std::vector<Tool> tools;

        // list_disks: the `refresh` argument exists only with --allow-disk-refresh (DEC-MCP-020)
        const std::string refreshDescription =
            offerRefresh ? "The sampler's refresh of its disk list is sent only with refresh "
                           "true: it is the one command that has left a real S5000 answering nothing until it was switched off and "
                           "on, and it can take long. "
                         : "This server never sends the sampler's refresh of its disk list: it has left a real S5000 answering "
                           "nothing until it was switched off and on. ";
        json listDisksSchema = objectSchema(json::object());
        if (offerRefresh)
            listDisksSchema = objectSchema(json{{"refresh",
                                                 {{"type", "boolean"},
                                                  {"description", "Ask the sampler to refresh its list of disks first (slow, and the risky call). Default false."}}}});
        tools.push_back(Tool{
            definition("list_disks", "List disks",
                       std::string("Lists the disks connected to the sampler with their handle, type, format and whether they can be "
                                   "written, and marks the current one. ") +
                           refreshDescription + DISK_NOTICE,
                       listDisksSchema, true, true),
            [&gateway, offerRefresh](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"refresh"}))
                    return *refused;
                bool refresh = false;
                if (arguments.contains("refresh"))
                {
                    if (!arguments.at("refresh").is_boolean())
                        return failure("The argument 'refresh' must be true or false.");
                    refresh = arguments.at("refresh").get<bool>();
                }
                if (refresh && !offerRefresh)
                    return failure("This server does not send the sampler's refresh of its disk list: it has left a real S5000 "
                                   "answering nothing until it was switched off and on. The owner can allow it by launching the "
                                   "server with --allow-disk-refresh. Nothing was sent.");
                const auto disks = gateway.listDisks(refresh);
                if (!disks.ok())
                    return failure(disks.problem);
                if (disks.value->empty())
                    return ok("No disk is connected to the sampler." +
                              std::string(refresh ? "" : offerRefresh ? " (The sampler's list was not refreshed: if a disk was just plugged in, call list_disks with refresh true.)"
                                                                      : " (This server does not refresh the sampler's list of disks: if a disk was just plugged in, the sampler may have to be told on its front panel.)"));
                std::string text = "Disks connected (" + std::to_string(disks.value->size()) + "):\n";
                for (const DiskEntry& disk : *disks.value)
                    text += std::to_string(disk.handle) + ": " + disk.name + " (" + diskType(disk.type) + ", " + diskFormat(disk.format) + ", " +
                            (disk.writable ? "writable" : "read-only") + (disk.current ? ", current" : "") + ")\n";
                return ok(std::move(text));
            }});

        // select_disk
        tools.push_back(Tool{
            definition("select_disk", "Select a disk",
                       std::string("Makes a disk the current one, by its name or its handle (see list_disks); list_disk_contents, "
                                   "open_folder and close_folder act on it, at the root of the disk. Give exactly one of name and handle. ") +
                           DISK_NOTICE,
                       objectSchema(json{{"name", {{"type", "string"}, {"description", "The disk's name."}}},
                                         {"handle", {{"type", "integer"}, {"minimum", 0}, {"description", "The disk's handle, from list_disks."}}}}),
                       false, true),
            [&gateway](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"name", "handle"}))
                    return *refused;
                const bool hasName = arguments.contains("name");
                const bool hasHandle = arguments.contains("handle");
                if (hasName == hasHandle)
                    return failure(hasName ? "Give either 'name' or 'handle', not both." : "Give either 'name' or 'handle'.");
                Outcome<DiskEntry> selected;
                if (hasName)
                {
                    if (!arguments.at("name").is_string())
                        return failure("The argument 'name' must be a string.");
                    selected = gateway.selectDiskByName(arguments.at("name").get<std::string>());
                }
                else
                {
                    const auto handle = wholeNumber(arguments.at("handle"));
                    if (!handle || *handle < 0 || *handle > MAX_PROGRAM_INDEX)
                        return failure("The argument 'handle' must be a whole number from 0 to " + std::to_string(MAX_PROGRAM_INDEX) + ".");
                    selected = gateway.selectDiskByHandle(static_cast<int>(*handle));
                }
                if (!selected.ok())
                    return failure(selected.problem);
                return ok("Selected the disk \"" + selected.value->name + "\" (" + diskType(selected.value->type) + ", " +
                          (selected.value->writable ? "writable" : "read-only") + "); its current folder is the root of the disk.");
            }});

        // list_disk_contents
        tools.push_back(Tool{
            definition("list_disk_contents", "List a disk's current folder",
                       std::string("Lists the sub-folders and the files, with their sizes in bytes, of the current folder of the current "
                                   "disk (see select_disk), and says which folder that is. ") +
                           DISK_NOTICE,
                       objectSchema(), true, true),
            [&gateway](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {}))
                    return *refused;
                const auto contents = gateway.listDiskContents();
                if (!contents.ok())
                    return failure(contents.problem);
                return ok(describeContents(*contents.value));
            }});

        // open_folder
        tools.push_back(Tool{
            definition("open_folder", "Open a folder",
                       std::string("Descends into a sub-folder of the current folder of the current disk, and lists what it holds. ") + DISK_NOTICE,
                       objectSchema(json{{"name", {{"type", "string"}, {"description", "The sub-folder's name, from list_disk_contents."}}}},
                                    json::array({"name"})),
                       false, false),
            [&gateway](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"name"}))
                    return *refused;
                if (!arguments.contains("name") || !arguments.at("name").is_string())
                    return failure("Give the 'name' of the sub-folder to open.");
                const auto contents = gateway.openFolder(arguments.at("name").get<std::string>());
                if (!contents.ok())
                    return failure(contents.problem);
                return ok(describeContents(*contents.value));
            }});

        // load_file
        tools.push_back(Tool{
            definition("load_file", "Load a file",
                       std::string("Loads a file of the current folder of the current disk into the sampler's memory (see list_disk_contents; "
                                   "its extension decides whether it is a program, a sample or a multi) and says what the memory holds "
                                   "before and after. With with_dependents the files it depends on are loaded too (a program and its samples). "
                                   "sample_mode says how a sample is loaded: normal, ram or virtual. It can take long, and a sampler that stops "
                                   "answering may have to be switched off and on; nothing is retried. ") +
                           DISK_FILES_NOTICE + std::string(" It adds to the sampler's memory, which is lost if the sampler is switched off without saving."),
                       objectSchema(json{{"name", {{"type", "string"}, {"description", "The file's name, from list_disk_contents."}}},
                                         {"with_dependents", {{"type", "boolean"}, {"description", "Also load the files it depends on. Default false."}}},
                                         {"sample_mode",
                                          {{"type", "string"},
                                           {"description", "For a sample: \"normal\" (the default), \"ram\" or \"virtual\"."}}}},
                                    json::array({"name"})),
                       false, false),
            [&gateway](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"name", "with_dependents", "sample_mode"}))
                    return *refused;
                if (!arguments.contains("name") || !arguments.at("name").is_string())
                    return failure("Give the 'name' of the file to load.");
                bool withDependents = false;
                if (arguments.contains("with_dependents"))
                {
                    if (!arguments.at("with_dependents").is_boolean())
                        return failure("The argument 'with_dependents' must be true or false.");
                    withDependents = arguments.at("with_dependents").get<bool>();
                }
                SampleLoadMode mode = SampleLoadMode::Normal;
                if (arguments.contains("sample_mode"))
                {
                    const std::string said = arguments.at("sample_mode").is_string() ? normalizeText(arguments.at("sample_mode").get<std::string>()) : "";
                    if (said == "normal")
                        mode = SampleLoadMode::Normal;
                    else if (said == "ram")
                        mode = SampleLoadMode::Ram;
                    else if (said == "virtual")
                        mode = SampleLoadMode::Virtual;
                    else
                        return failure("The argument 'sample_mode' must be \"normal\", \"ram\" or \"virtual\".");
                }
                const std::string name = arguments.at("name").get<std::string>();
                const auto loaded = gateway.loadFile(name, withDependents, mode);
                if (!loaded.ok())
                    return failure(loaded.problem);
                return ok("Loaded \"" + name + "\" from the disk \"" + loaded.value->diskName + "\" (folder " +
                          (loaded.value->path.empty() ? "(root)" : loaded.value->path) + (withDependents ? ", with the files it depends on" : "") +
                          "). " + describeMemory(*loaded.value));
            }});

        // load_folder
        tools.push_back(Tool{
            definition("load_folder", "Load a folder",
                       std::string("Loads a sub-folder of the current folder of the current disk, and everything it holds, into the "
                                   "sampler's memory, and says what the memory holds before and after. It can take long, and a sampler that stops "
                                   "answering may have to be switched off and on; nothing is retried. ") +
                           DISK_FILES_NOTICE + std::string(" It adds to the sampler's memory, which is lost if the sampler is switched off without saving."),
                       objectSchema(json{{"name", {{"type", "string"}, {"description", "The sub-folder's name, from list_disk_contents."}}}},
                                    json::array({"name"})),
                       false, false),
            [&gateway](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"name"}))
                    return *refused;
                if (!arguments.contains("name") || !arguments.at("name").is_string())
                    return failure("Give the 'name' of the sub-folder to load.");
                const std::string name = arguments.at("name").get<std::string>();
                const auto loaded = gateway.loadFolder(name);
                if (!loaded.ok())
                    return failure(loaded.problem);
                return ok("Loaded the folder \"" + name + "\" from the disk \"" + loaded.value->diskName + "\". " + describeMemory(*loaded.value));
            }});

        // save_memory_item
        tools.push_back(Tool{
            markDestructive(definition("save_memory_item", "Save a program, sample or multi to the disk",
                       std::string("Saves one item of the sampler's memory, found by its name, to the current folder of the current disk "
                                   "(see select_disk and open_folder), which must be writable, and says whether a file of its name is there "
                                   "afterwards. A file of the item's name that is already in the folder is NOT replaced unless overwrite is true: "
                                   "overwrite true loses the old file. save_children also saves what the item uses (a program's samples). It can "
                                   "take long, and a sampler that stops answering may have to be switched off and on; nothing is retried. ") +
                           DISK_FILES_NOTICE,
                       objectSchema(json{{"kind", {{"type", "string"}, {"description", "\"program\", \"sample\" or \"multi\"."}}},
                                         {"name", {{"type", "string"}, {"description", "The item's name, from list_programs, list_samples or list_multis."}}},
                                         {"overwrite", {{"type", "boolean"}, {"description", "Replace a file of that name already in the folder. Default false."}}},
                                         {"save_children", {{"type", "boolean"}, {"description", "Also save what the item depends on. Default false."}}}},
                                    json::array({"kind", "name"})),
                       false, false)),
            [&gateway](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"kind", "name", "overwrite", "save_children"}))
                    return *refused;
                const auto kind = saveKindArgument(arguments);
                if (!kind)
                    return failure("Give the 'kind' of the item: \"program\", \"sample\" or \"multi\".");
                if (!arguments.contains("name") || !arguments.at("name").is_string())
                    return failure("Give the 'name' of the item to save.");
                bool overwrite = false;
                bool children = false;
                if (const auto problem = flagArgument(arguments, "overwrite", overwrite))
                    return failure(*problem);
                if (const auto problem = flagArgument(arguments, "save_children", children))
                    return failure(*problem);
                const auto saved = gateway.saveMemoryItem(*kind, arguments.at("name").get<std::string>(), overwrite, children);
                if (!saved.ok())
                    return failure(saved.problem);
                const SaveOutcome& outcome = *saved.value;
                const std::string where = "the disk \"" + outcome.diskName + "\" (folder " + (outcome.path.empty() ? "(root)" : outcome.path) + ")";
                if (!outcome.savedFile)
                    return failure("The sampler accepted the save of the " + std::string(saveKindName(*kind)) + " \"" + outcome.itemName +
                                   "\" to " + where + " but no file bearing that name appears in the folder (files added: " +
                                   (gainedFiles(outcome).empty() ? "none" : joined(gainedFiles(outcome), ", ")) +
                                   "). It may have saved it under another name, or not at all.");
                const bool replaced = std::any_of(outcome.filesBefore.begin(), outcome.filesBefore.end(),
                                                  [&outcome](const DiskFileEntry& file) { return file.name == outcome.savedFile->name; });
                return ok("Saved the " + std::string(saveKindName(*kind)) + " \"" + outcome.itemName + "\" to " + where + ": the file " +
                          outcome.savedFile->name + " (" + std::to_string(outcome.savedFile->sizeBytes) + " bytes) is in the folder" +
                          (replaced ? ", replacing the file of that name" : "") + ".");
            }});

        // save_all_memory_items
        tools.push_back(Tool{
            markDestructive(definition("save_all_memory_items", "Save every program, sample or multi to the disk",
                       std::string("Saves every item of a kind of the sampler's memory to the current folder of the current disk, which must be "
                                   "writable, and says how many files the folder gained. It is sent only when confirm is exactly the number of items "
                                   "of that kind in memory (see list_programs, list_samples, list_multis). Files of the items' names already in the "
                                   "folder are NOT replaced unless overwrite is true: overwrite true loses the old files. It can take long, and a "
                                   "sampler that stops answering may have to be switched off and on; nothing is retried. ") +
                           DISK_FILES_NOTICE,
                       objectSchema(json{{"kind", {{"type", "string"}, {"description", "\"program\", \"sample\" or \"multi\"."}}},
                                         {"confirm", {{"type", "integer"}, {"minimum", 1}, {"description", "How many items of that kind are in memory."}}},
                                         {"overwrite", {{"type", "boolean"}, {"description", "Replace files of the same names already in the folder. Default false."}}},
                                         {"save_children", {{"type", "boolean"}, {"description", "Also save what the items depend on. Default false."}}}},
                                    json::array({"kind", "confirm"})),
                       false, false)),
            [&gateway](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {"kind", "confirm", "overwrite", "save_children"}))
                    return *refused;
                const auto kind = saveKindArgument(arguments);
                if (!kind)
                    return failure("Give the 'kind' of the items: \"program\", \"sample\" or \"multi\".");
                if (!arguments.contains("confirm"))
                    return failure("Give 'confirm', the number of items of that kind in the sampler's memory.");
                const auto confirm = arguments.at("confirm").is_number() ? wholeNumber(arguments.at("confirm")) : std::nullopt;
                if (!confirm || *confirm < 1 || *confirm > MAX_PROGRAM_INDEX)
                    return failure("The argument 'confirm' must be a whole number: how many items of that kind are in memory.");
                bool overwrite = false;
                bool children = false;
                if (const auto problem = flagArgument(arguments, "overwrite", overwrite))
                    return failure(*problem);
                if (const auto problem = flagArgument(arguments, "save_children", children))
                    return failure(*problem);
                const auto saved = gateway.saveAllMemoryItems(*kind, static_cast<int>(*confirm), overwrite, children);
                if (!saved.ok())
                    return failure(saved.problem);
                const SaveOutcome& outcome = *saved.value;
                const std::vector<std::string> gained = gainedFiles(outcome);
                return ok("Saved the " + plural(outcome.itemCount, saveKindName(*kind)) + " to the disk \"" + outcome.diskName + "\" (folder " +
                          (outcome.path.empty() ? "(root)" : outcome.path) + "): the folder gained " +
                          (gained.empty() ? std::string("no new file (files of the same names may have been replaced)")
                                          : plural(static_cast<std::int64_t>(gained.size()), "file") + " (" + joined(gained, ", ") + ")") +
                          ".");
            }});

        // close_folder
        tools.push_back(Tool{
            definition("close_folder", "Go up one folder",
                       std::string("Goes back up from the current folder to its parent on the current disk, and lists what the parent "
                                   "holds. It does nothing at the root. ") +
                           DISK_NOTICE,
                       objectSchema(), false, false),
            [&gateway](const json& arguments) {
                if (const auto refused = unknownArguments(arguments, {}))
                    return *refused;
                const auto contents = gateway.closeFolder();
                if (!contents.ok())
                    return failure(contents.problem);
                return ok(describeContents(*contents.value));
            }});

        return tools;
    }

    std::vector<Tool> makeAllTools(SamplerGateway& gateway, const ParameterCatalogue& catalogue, ToolOptions options)
    {
        ExtraCatalogues extra;
        extra.sample = &ParameterCatalogue::samples();
        extra.multi = &ParameterCatalogue::multis();
        std::vector<Tool> tools = makeProgramEditingTools(gateway, catalogue, extra);
        for (Tool& tool : makeProgramStructureTools(gateway))
            tools.push_back(std::move(tool));
        for (Tool& tool : makeSampleTools(gateway, ParameterCatalogue::samples()))
            tools.push_back(std::move(tool));
        for (Tool& tool : makeMultiTools(gateway, ParameterCatalogue::multis()))
            tools.push_back(std::move(tool));
        if (options.allowDisk)
        {
            for (Tool& tool : makeDiskTools(gateway, options.allowDiskRefresh))
                tools.push_back(std::move(tool));
        }
        return tools;
    }
}
