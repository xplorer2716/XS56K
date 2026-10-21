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
#include "app/EditController.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <iterator>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "app/EditorLayouts.generated.hpp"

namespace app
{
    // [RQ-APP-048, RQ-APP-053, RQ-APP-058, RQ-APP-060, RQ-APP-086 to RQ-APP-088, RQ-APP-090, RQ-APP-093, RQ-APP-094, ADR-APP-004 (DEC-APP-025,
    // DEC-APP-028, DEC-APP-030, DEC-APP-033, DEC-APP-034), TASK-APP-036, TASK-APP-041, TASK-APP-043, TASK-APP-044, TASK-APP-046, TASK-APP-047]
    namespace
    {
        constexpr const char* READING_LABEL = "Reading ";
        constexpr const char* SETTING_LABEL = "Setting the ";
        constexpr const char* OF = " of ";
        /// The unit of a position in a sample: bounded by the sample's length rather than the catalogue's whole word. [RQ-APP-088]
        constexpr const char* POSITION_UNIT = "sample points";
        constexpr const char* LENGTH_NAME = "sample length";
        constexpr const char* LOOP_START_NAME = "sample loop start";
        constexpr const char* LOOP_END_NAME = "sample loop end";
        constexpr const char* NO_VALUE = "The sampler answered no value.";
        constexpr const char* ZONE_SAMPLE_NAME = "zone sample";
        constexpr const char* OF_ZONE = " of zone ";
        constexpr const char* NO_SAMPLE_REMOVAL = "The editor cannot remove a zone's sample yet: choose another sample. Nothing was sent.";
        constexpr int MOST_KEYGROUPS = 99;  // a program's keygroups at most (RQ-MCP-035)
        constexpr const char* ADDING_LABEL = "Adding keygroups to ";
        constexpr const char* ADD_ACTION = "Add";
        constexpr const char* DELETING_LABEL = "Deleting keygroup ";
        constexpr const char* KEYGROUP_WORD = "keygroup ";
        constexpr const char* FULL = " has the 99 keygroups a program can have: nothing was sent.";
        constexpr const char* LAST = " is the program's last keygroup: a program keeps one. Nothing was sent.";
        constexpr const char* NOTHING_SENT = ": nothing was sent.";
        constexpr const char* DELETING_ALL_LABEL = "Deleting a keygroup of ";
        constexpr const char* CHOOSE_ONE = "All keygroups are chosen: choose one keygroup to delete. Nothing was sent.";

        using Value = mcp::Outcome<std::int64_t>;
        /// A value as shown: with all keygroups chosen, the first keygroup's, and whether the keygroups do not share it. [RQ-APP-059]
        struct Shown
        {
            std::int64_t value = 0;
            bool differs = false;
        };
        using ShownValue = mcp::Outcome<Shown>;
        /// What making an item current brings back: a program's keygroup count, nothing for a sample.
        using Selected = mcp::Outcome<std::optional<int>>;

        /// What an editor edits and how: the kind, the layout's editor name, the catalogue, how the item is made current and how one of its
        /// parameters is read and written. [RQ-APP-048, RQ-APP-093, DEC-APP-030]
        struct Binding
        {
            MemoryKind kind;
            std::string_view editor;
            const mcp::ParameterCatalogue& (*catalogue)();
            Selected (*select)(mcp::SamplerGateway&, const std::string&);
            ShownValue (*read)(mcp::SamplerGateway&, const mcp::ParameterDefinition&, int keygroup, int zone);
            Value (*write)(mcp::SamplerGateway&, const mcp::ParameterDefinition&, std::int64_t, int keygroup, int zone);
        };

        /// A value of the tab to read: its catalogue row and its zone (1 to 4 in a zone grid, 0 otherwise). [DEC-APP-033]
        struct Target
        {
            const mcp::ParameterDefinition* definition = nullptr;
            int zone = 0;
        };

        /// The zone selection of a value: one zone in a grid, none asked for otherwise.
        mcp::ZoneSelection zonesOf(int zone)
        {
            return zone > 0 ? mcp::ZoneSelection::of(zone) : mcp::ZoneSelection::all();
        }

        /// A zone's sample has no catalogue row: this one names it, a choice whose labels are the samples in memory. [RQ-APP-060, DEC-APP-033]
        const mcp::ParameterDefinition& zoneSampleDefinition()
        {
            static const mcp::ParameterDefinition definition = [] {
                mcp::ParameterDefinition made;
                made.name = ZONE_SAMPLE_NAME;
                made.scope = mcp::ParameterScope::Zone;
                made.kind = mcp::ParameterKind::Choice;
                return made;
            }();
            return definition;
        }

        bool isZoneSample(const mcp::ParameterDefinition* definition)
        {
            return definition == &zoneSampleDefinition();
        }

        /// A program parameter's one value: of the program, or of the keygroup asked for.
        Value programValue(const mcp::Outcome<std::vector<mcp::ParameterValue>>& values)
        {
            if (!values.ok())
                return Value::failure(values.problem);
            if (values.value->empty())
                return Value::failure(NO_VALUE);
            return Value::success(values.value->front().value);
        }

        Selected selectSample(mcp::SamplerGateway& gateway, const std::string& name)
        {
            const auto selected = gateway.selectSampleByName(name);
            return selected.ok() ? Selected::success(std::nullopt) : Selected::failure(selected.problem);
        }

        ShownValue readSample(mcp::SamplerGateway& gateway, const mcp::ParameterDefinition& parameter, int /*keygroup*/, int /*zone*/)
        {
            const auto read = gateway.readSampleParameter(parameter);
            return read.ok() ? ShownValue::success(Shown{*read.value, false}) : ShownValue::failure(read.problem);
        }

        Value writeSample(mcp::SamplerGateway& gateway, const mcp::ParameterDefinition& parameter, std::int64_t value, int /*keygroup*/,
                          int /*zone*/)
        {
            return gateway.writeSampleParameter(parameter, value);
        }

        Selected selectProgram(mcp::SamplerGateway& gateway, const std::string& name)
        {
            const auto selected = gateway.selectProgramByName(name);
            return selected.ok() ? Selected::success(selected.value->keygroupCount) : Selected::failure(selected.problem);
        }

        /// The keygroup chosen, or all of them. [RQ-APP-058, RQ-APP-059]
        mcp::KeygroupSelection keygroupsOf(int keygroup)
        {
            return keygroup == ALL_KEYGROUPS ? mcp::KeygroupSelection::all() : mcp::KeygroupSelection::of(keygroup);
        }

        // A program parameter takes no keygroup: the gateway leaves the selection aside for it; a zone's, the zone of its column. With all
        // keygroups, a value per keygroup comes back: the first is shown, and whether the others differ. [RQ-APP-094, RQ-APP-059, DEC-APP-033]
        ShownValue readProgram(mcp::SamplerGateway& gateway, const mcp::ParameterDefinition& parameter, int keygroup, int zone)
        {
            const auto values = gateway.readParameter(parameter, keygroupsOf(keygroup), zonesOf(zone));
            const auto first = programValue(values);
            if (!first.ok())
                return ShownValue::failure(first.problem);
            const bool differs = std::ranges::any_of(*values.value, [&first](const mcp::ParameterValue& value) { return value.value != *first.value; });
            return ShownValue::success(Shown{*first.value, differs});
        }

        Value writeProgram(mcp::SamplerGateway& gateway, const mcp::ParameterDefinition& parameter, std::int64_t value, int keygroup, int zone)
        {
            return programValue(gateway.writeParameter(parameter, value, keygroupsOf(keygroup), zonesOf(zone)));
        }

        const std::array<Binding, 2> BINDINGS{{
            {MemoryKind::Sample, "sample", &mcp::ParameterCatalogue::samples, &selectSample, &readSample, &writeSample},
            {MemoryKind::Program, "program", &mcp::ParameterCatalogue::standard, &selectProgram, &readProgram, &writeProgram},
        }};

        const Binding* bindingOf(MemoryKind kind)
        {
            const auto found = std::ranges::find_if(BINDINGS, [kind](const Binding& binding) { return binding.kind == kind; });
            return found == BINDINGS.end() ? nullptr : std::to_address(found);
        }

        const Binding* bindingOf(const EditState& state)
        {
            return state.item ? bindingOf(state.item->kind) : nullptr;
        }

        /// The editor layout of a kind, or null while the kind has no editor. [RQ-APP-048]
        const layouts::EditorLayout* layoutOf(MemoryKind kind)
        {
            const Binding* binding = bindingOf(kind);
            if (binding == nullptr)
                return nullptr;
            const auto& all = layouts::kEditorLayouts;
            const auto found = std::ranges::find_if(all, [binding](const layouts::EditorLayout& layout) { return layout.editor == binding->editor; });
            return found == all.end() ? nullptr : std::to_address(found);
        }

        const mcp::ParameterDefinition* definitionOf(const Binding& binding, std::string_view name)
        {
            const auto& rows = binding.catalogue().parameters();
            const auto found = std::ranges::find_if(rows, [name](const mcp::ParameterDefinition& row) { return row.name == name; });
            if (found != rows.end())
                return std::to_address(found);
            return binding.kind == MemoryKind::Program && name == ZONE_SAMPLE_NAME ? &zoneSampleDefinition() : nullptr;
        }

        /// A group's values: one per parameter, or, in a zone grid, one per parameter and zone, parameter by parameter. [DEC-APP-033]
        void addGroupValues(const Binding& binding, const layouts::LayoutGroup& group, std::vector<EditValue>& into)
        {
            for (const layouts::LayoutParameter& parameter : group.parameters)
            {
                const mcp::ParameterDefinition* definition = definitionOf(binding, parameter.name);
                if (definition == nullptr)
                    continue;
                for (int zone = group.zones > 0 ? 1 : 0; zone <= group.zones; ++zone)
                    into.emplace_back(definition, &parameter, 0, definition->min, definition->max, false, zone);
            }
        }

        /// The tab's parameters in the layout's order, then the catalogue's read-only rows, each with the catalogue's range; nothing read yet.
        EditState emptyState(const MemoryItem& item, const layouts::EditorLayout* layout, std::size_t tab = 0)
        {
            EditState state;
            state.item = item;
            state.layout = layout;
            state.tab = tab;
            const Binding* binding = bindingOf(item.kind);
            if (layout == nullptr || tab >= layout->tabs.size() || binding == nullptr)
                return state;
            for (const layouts::LayoutGroup& group : layout->tabs[state.tab].groups)
                addGroupValues(*binding, group, state.values);
            for (const mcp::ParameterDefinition& definition : binding->catalogue().parameters())
            {
                if (definition.readOnly)
                    state.info.emplace_back(&definition, nullptr, 0, definition.min, definition.max, false);
            }
            return state;
        }

        /// What one reading brings back: the tab's values and the information, in the state's order, a program's keygroup count and the
        /// keygroup read (the first when the one asked for is gone).
        struct Reading
        {
            std::vector<std::int64_t> values;
            std::vector<bool> differs;  ///< per value: the keygroups do not share it (all keygroups chosen) [RQ-APP-059]
            std::vector<std::int64_t> info;
            std::optional<int> keygroups;
            int keygroup = 1;
            std::vector<std::string> samples;
        };

        using Definitions = std::vector<Target>;

        /// The samples in memory and the sample of each zone of a keygroup, read once for all the zone samples of a tab. [RQ-APP-060]
        struct ZoneSamples
        {
            bool read = false;
            std::vector<std::string> memory;
            std::vector<mcp::ZoneSampleEntry> zones;
        };

        std::optional<std::string> readZoneSamples(mcp::SamplerGateway& gateway, int keygroup, ZoneSamples& samples)
        {
            const auto listing = gateway.listSamples();
            if (!listing.ok())
                return listing.problem;
            const auto zones = gateway.readZoneSamples(keygroupsOf(keygroup));
            if (!zones.ok())
                return zones.problem;
            samples.memory.clear();
            for (const mcp::SampleEntry& entry : listing.value->samples)
                samples.memory.push_back(entry.name);
            samples.zones = zones.value->entries;
            samples.read = true;
            return std::nullopt;
        }

        /// A zone's sample as a value: 1 plus its place among the samples in memory, 0 when the zone has none; with all keygroups, the first
        /// keygroup's, and whether the others play another. [RQ-APP-060, RQ-APP-059, DEC-APP-033]
        Shown zoneSampleValue(const ZoneSamples& samples, int zone)
        {
            const auto entry = std::ranges::find_if(samples.zones, [zone](const mcp::ZoneSampleEntry& found) { return found.zone == zone; });
            if (entry == samples.zones.end())
                return {};
            const bool differs = std::ranges::any_of(samples.zones, [&entry](const mcp::ZoneSampleEntry& other) {
                return other.zone == entry->zone && other.sample != entry->sample;
            });
            if (entry->sample.empty())
                return Shown{0, differs};
            const auto place = std::ranges::find(samples.memory, entry->sample);
            return Shown{place == samples.memory.end() ? 0 : static_cast<std::int64_t>(std::distance(samples.memory.begin(), place)) + 1, differs};
        }

        /// Each value of `rows` read from the current item, into `into`, and the samples in memory into `memory` when a zone's sample is
        /// among them; the problem of the first that fails.
        std::optional<std::string> readInto(const Binding& binding, mcp::SamplerGateway& gateway, const Definitions& rows, int keygroup,
                                            std::vector<std::int64_t>& into, std::vector<bool>& differs, std::vector<std::string>& memory)
        {
            ZoneSamples samples;
            for (const Target& target : rows)
            {
                if (isZoneSample(target.definition))
                {
                    if (const auto problem = samples.read ? std::nullopt : readZoneSamples(gateway, keygroup, samples))
                        return problem;
                    const Shown shown = zoneSampleValue(samples, target.zone);
                    into.push_back(shown.value);
                    differs.push_back(shown.differs);
                    continue;
                }
                const auto read = binding.read(gateway, *target.definition, keygroup, target.zone);
                if (!read.ok())
                    return read.problem;
                into.push_back(read.value->value);
                differs.push_back(read.value->differs);
            }
            if (samples.read)
                memory = samples.memory;
            return std::nullopt;
        }

        mcp::Outcome<Reading> readOn(const Binding& binding, mcp::SamplerGateway& gateway, const std::string& item, const Definitions& values,
                                     const Definitions& info, int keygroup)
        {
            // The edited item is made the sampler's current one first: the front panel may have moved it. [DEC-APP-025]
            const auto selected = binding.select(gateway, item);
            if (!selected.ok())
                return mcp::Outcome<Reading>::failure(selected.problem);
            Reading reading;
            reading.keygroups = *selected.value;
            reading.keygroup = reading.keygroups.has_value() && keygroup > *reading.keygroups ? 1 : keygroup;  // the program lost keygroups meanwhile
            std::vector<bool> infoDiffers;  // the information is the item's own: it never differs
            for (const auto& [rows, into, differs] :
                 {std::tuple{&values, &reading.values, &reading.differs}, std::tuple{&info, &reading.info, &infoDiffers}})
            {
                if (const auto problem = readInto(binding, gateway, *rows, reading.keygroup, *into, *differs, reading.samples))
                    return mcp::Outcome<Reading>::failure(*problem);
            }
            return mcp::Outcome<Reading>::success(std::move(reading));
        }

        /// What a write brings back: its problem, if any, and the tab read again, whatever happened, so that the panel shows what the sampler
        /// holds and the pending state always ends. [RQ-APP-052, RQ-APP-089, DEC-APP-026]
        struct Written
        {
            std::optional<std::string> problem;
            std::vector<std::int64_t> values;
            std::vector<bool> differs;
            std::vector<std::string> samples;
        };

        /// What a value is written to: its row, its zone, and the samples in memory a zone's sample is chosen among.
        struct Write
        {
            const mcp::ParameterDefinition* parameter = nullptr;
            int zone = 0;
            std::vector<std::string> samples;
        };

        /// One write on the sampler: a zone's sample assigned by name, to each keygroup of `keygroups` when all are chosen, any other value
        /// set; the problem, if any. [RQ-APP-060, RQ-APP-059, DEC-APP-033]
        std::optional<std::string> setOn(const Binding& binding, mcp::SamplerGateway& gateway, const Write& target, std::int64_t value, int keygroup,
                                         int keygroups)
        {
            if (isZoneSample(target.parameter))
            {
                const std::string& sample = target.samples[static_cast<std::size_t>(value - 1)];
                const int first = keygroup == ALL_KEYGROUPS ? 1 : keygroup;
                const int last = keygroup == ALL_KEYGROUPS ? keygroups : keygroup;
                for (int each = first; each <= last; ++each)
                {
                    if (const auto assigned = gateway.assignZoneSample(each, target.zone, sample); !assigned.ok())
                        return assigned.problem;
                }
                return std::nullopt;
            }
            const auto set = binding.write(gateway, *target.parameter, value, keygroup, target.zone);
            return set.ok() ? std::nullopt : std::optional<std::string>(set.problem);
        }

        Written writeOn(const Binding& binding, mcp::SamplerGateway& gateway, const std::string& item, const Write& target, std::int64_t value,
                        const Definitions& tab, int keygroup)
        {
            const mcp::ParameterDefinition& parameter = *target.parameter;
            Written written;
            const auto selected = binding.select(gateway, item);
            if (!selected.ok())
            {
                written.problem = selected.problem;
                return written;
            }
            // The S5000's Set Loop End moves the loop start to keep the loop length: the loop start held before is sent again after it, Set
            // Loop Start moving nothing else. A start beyond the new end (2^28 - 1 after a start pushed below 0) is not. [RQ-APP-090, DEC-APP-028]
            std::optional<std::int64_t> loopStart;
            const mcp::ParameterDefinition* loopStartParameter = definitionOf(binding, LOOP_START_NAME);
            if (parameter.name == LOOP_END_NAME && loopStartParameter != nullptr)
            {
                if (const auto read = binding.read(gateway, *loopStartParameter, keygroup, 0); read.ok() && read.value->value <= value)
                    loopStart = read.value->value;
            }
            if (const auto problem = setOn(binding, gateway, target, value, keygroup, selected.value->value_or(1)))
                written.problem = problem;
            else if (loopStart.has_value())
            {
                if (const auto again = binding.write(gateway, *loopStartParameter, *loopStart, keygroup, 0); !again.ok())
                    written.problem = again.problem;
            }
            if (const auto problem = readInto(binding, gateway, tab, keygroup, written.values, written.differs, written.samples))
            {
                written.problem = written.problem ? *written.problem + " " + *problem : *problem;
                written.values.clear();
                written.differs.clear();
            }
            return written;
        }

        /// Whether the panel still shows `item`'s `tab` on `keygroup`: a result for another screen is not shown. [RQ-APP-091]
        bool showsScreen(const EditState& now, const MemoryItem& item, std::size_t tab, int keygroup)
        {
            return now.item == std::optional<MemoryItem>(item) && now.tab == tab && now.keygroup == keygroup;
        }

        /// The state with what a reading brought back: the information, the values and their positions' upper bound. [RQ-APP-088]
        EditState withReading(const EditState& now, const Reading& reading)
        {
            EditState next = now;
            next.keygroups = reading.keygroups;
            next.keygroup = reading.keygroup;
            next.samples = reading.samples;
            std::int64_t length = 0;
            for (std::size_t i = 0; i < next.info.size() && i < reading.info.size(); ++i)
            {
                next.info[i].value = reading.info[i];
                if (next.info[i].definition->name == LENGTH_NAME)
                    length = reading.info[i];
            }
            for (std::size_t i = 0; i < next.values.size() && i < reading.values.size(); ++i)
            {
                EditValue& value = next.values[i];
                value.value = reading.values[i];
                value.differs = i < reading.differs.size() && reading.differs[i];
                if (value.definition->unit == POSITION_UNIT)
                    value.max = length;
                if (isZoneSample(value.definition))
                    value.max = static_cast<std::int64_t>(next.samples.size());  // [RQ-APP-060]
            }
            return next;
        }

        /// The state with the values a write read again; the parameter written stays pending while another value waits for it. [RQ-APP-054]
        EditState withWritten(const EditState& now, std::size_t index, const Written& written, bool waiting)
        {
            EditState next = now;
            if (!written.samples.empty())
                next.samples = written.samples;
            for (std::size_t i = 0; i < next.values.size() && i < written.values.size(); ++i)
            {
                next.values[i].value = written.values[i];
                next.values[i].differs = i < written.differs.size() && written.differs[i];
            }
            next.values[index].pending = waiting;
            return next;
        }

        std::string lowerCase(std::string_view text)
        {
            std::string lower(text);
            std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lower;
        }

        /// The value shown for `name` in `zone` (0 outside a zone grid), if the tab has it.
        std::optional<std::int64_t> shownValue(const EditState& state, std::string_view name, int zone)
        {
            for (const EditValue& value : state.values)
            {
                if (value.definition->name == name && value.zone == zone)
                    return value.value;
            }
            return std::nullopt;
        }

        /// The range a value may take now: its own, narrowed by its nearest neighbours in each order of the layout; a neighbour outside the
        /// value's own range (2^28 - 1, a loop start the S5000 pushed below 0) bounds nothing; in a zone grid, its neighbours of the same zone.
        /// [RQ-APP-088, RQ-APP-092, DEC-APP-029, DEC-APP-033]
        std::pair<std::int64_t, std::int64_t> rangeOf(const EditState& state, const EditValue& shown)
        {
            std::int64_t min = shown.min;
            std::int64_t max = shown.max;
            const auto within = [&shown](std::int64_t value) { return value >= shown.min && value <= shown.max; };
            for (const std::span<const std::string_view> order : state.layout->orders)
            {
                const auto at = std::ranges::find(order, shown.definition->name);
                if (at == order.end())
                    continue;
                for (auto before = at; before != order.begin();)
                {
                    if (const auto value = shownValue(state, *--before, shown.zone); value && within(*value))
                    {
                        min = std::max(min, *value);
                        break;
                    }
                }
                for (auto after = at + 1; after != order.end(); ++after)
                {
                    if (const auto value = shownValue(state, *after, shown.zone); value && within(*value))
                    {
                        max = std::min(max, *value);
                        break;
                    }
                }
            }
            return {min, max};
        }

        Definitions definitionsOf(const std::vector<EditValue>& values)
        {
            Definitions definitions;
            for (const EditValue& value : values)
                definitions.push_back(Target{value.definition, value.zone});
            return definitions;
        }

        /// What the person reads in the busy line: "Setting the level of zone 2 of PAD". [RQ-APP-052]
        std::string settingLabel(const EditValue& shown, const std::string& item)
        {
            std::string label = SETTING_LABEL + lowerCase(shown.layout->label);
            if (shown.zone > 0)
                label += OF_ZONE + std::to_string(shown.zone);
            return label + OF + item;
        }
    }

    EditController::EditController(Model& model, Controller& controller) : _model(model), _controller(controller)
    {
        _model.addListener(*this);
    }

    EditController::~EditController()
    {
        _model.removeListener(*this);
    }

    void EditController::modelChanged(ModelPart part)
    {
        if (part == ModelPart::Connection)
        {
            if (_model.connection().state != ConnectionState::Connected && _model.edit().item)
                _model.setEdit(EditState{});
            return;
        }
        if (part != ModelPart::Memory)
            return;
        const auto& selection = _model.memory().selection;
        if (selection != _model.edit().item)
            open(selection);
    }

    void EditController::open(std::optional<MemoryItem> item)
    {
        _writing.clear();
        _waiting.clear();
        if (!item)
        {
            _model.setEdit(EditState{});
            return;
        }
        _model.setEdit(emptyState(*item, layoutOf(item->kind)));
        if (_model.edit().layout != nullptr)
            read();
    }

    void EditController::readAgain()
    {
        if (_model.edit().layout != nullptr)
            read();
    }

    void EditController::showTab(std::size_t index)
    {
        const EditState& state = _model.edit();
        if (state.layout == nullptr || index >= state.layout->tabs.size() || index == state.tab)
            return;
        EditState next = emptyState(*state.item, state.layout, index);
        next.keygroups = state.keygroups;
        next.keygroup = state.keygroup;
        _writing.clear();  // a write in flight ends on the tab it was for; its result is not shown [RQ-APP-049]
        _waiting.clear();
        _model.setEdit(std::move(next));
        read();
    }

    void EditController::chooseKeygroup(int keygroup)
    {
        const EditState& state = _model.edit();
        const bool known = keygroup == ALL_KEYGROUPS || (keygroup >= 1 && keygroup <= state.keygroups.value_or(0));  // [RQ-APP-059]
        if (state.layout == nullptr || !state.keygroups.has_value() || !known || keygroup == state.keygroup)
            return;
        EditState next = state;
        next.keygroup = keygroup;
        for (EditValue& value : next.values)
            value.pending = false;
        _writing.clear();  // a write in flight ends on the keygroup it was for; its result is not shown [RQ-APP-058]
        _waiting.clear();
        _model.setEdit(std::move(next));
        read();
    }

    void EditController::addKeygroups()
    {
        const EditState& state = _model.edit();
        if (state.layout == nullptr || !state.keygroups.has_value())
            return;
        const MemoryItem item = *state.item;
        const int have = *state.keygroups;
        if (have >= MOST_KEYGROUPS)
        {
            _model.setProblem(Problem{ADDING_LABEL + item.name, item.name + FULL});
            return;
        }
        const int most = MOST_KEYGROUPS - have;
        const std::string text = item.name + " has " + std::to_string(have) + " keygroups; a program has up to " + std::to_string(MOST_KEYGROUPS) +
                                 ". How many keygroups to add, from 1 to " + std::to_string(most) + "?";
        const CountQuestion question{"Add keygroups to " + item.name, text, 1, most, 1, ADD_ACTION};
        _controller.askCount(question, [this, item, keygroup = state.keygroup](std::optional<int> count) {
            if (!count.has_value())
                return;  // cancelled: nothing is sent [RQ-APP-061]
            _controller.runOnSampler<mcp::KeygroupChange>(
                ADDING_LABEL + item.name,
                [name = item.name, added = *count](mcp::SamplerGateway& gateway) {
                    if (const auto selected = gateway.selectProgramByName(name); !selected.ok())
                        return mcp::Outcome<mcp::KeygroupChange>::failure(selected.problem);
                    return gateway.addKeygroups(added);
                },
                [this, item, keygroup](const mcp::KeygroupChange& change) { keygroupsChanged(item, change.keygroupCount, keygroup); });
        });
    }

    void EditController::deleteKeygroup()
    {
        const EditState& state = _model.edit();
        if (state.layout == nullptr || !state.keygroups.has_value())
            return;
        const MemoryItem item = *state.item;
        const int keygroup = state.keygroup;
        if (keygroup == ALL_KEYGROUPS)
        {
            _model.setProblem(Problem{DELETING_ALL_LABEL + item.name, CHOOSE_ONE});
            return;  // all keygroups chosen: nothing is sent [RQ-APP-059, RQ-APP-061]
        }
        const std::string object = KEYGROUP_WORD + std::to_string(keygroup);
        if (*state.keygroups <= 1)
        {
            _model.setProblem(Problem{DELETING_LABEL + std::to_string(keygroup) + OF + item.name, "Keygroup 1" + std::string(LAST)});
            return;  // the last keygroup: no question, nothing sent [RQ-APP-061]
        }
        const Confirmation question{"Delete " + object + " of " + item.name + "?", object,
                                    "Keygroup " + std::to_string(keygroup) + " is deleted with its zones and its settings. If " + item.name +
                                        " was not saved to a disk, they are lost."};
        _controller.runOnSamplerConfirmed<mcp::KeygroupChange>(
            question, DELETING_LABEL + std::to_string(keygroup) + OF + item.name,
            [name = item.name, keygroup](mcp::SamplerGateway& gateway) {
                if (const auto selected = gateway.selectProgramByName(name); !selected.ok())
                    return mcp::Outcome<mcp::KeygroupChange>::failure(selected.problem);
                return gateway.deleteKeygroup(keygroup, name);
            },
            [this, item, keygroup](const mcp::KeygroupChange& change) {
                keygroupsChanged(item, change.keygroupCount, std::min(keygroup, change.keygroupCount));
            });
    }

    void EditController::keygroupsChanged(const MemoryItem& item, int keygroups, int keygroup)
    {
        const EditState& now = _model.edit();
        if (now.item != std::optional<MemoryItem>(item))
            return;  // another item is shown: its own reading tells its keygroups
        EditState next = now;
        next.keygroups = keygroups;
        next.keygroup = keygroup == ALL_KEYGROUPS ? ALL_KEYGROUPS : std::clamp(keygroup, 1, std::max(1, keygroups));  // [RQ-APP-059]
        for (EditValue& value : next.values)
            value.pending = false;
        _writing.clear();
        _waiting.clear();
        _model.setEdit(std::move(next));
        read();  // [RQ-APP-058, RQ-APP-061]
    }

    void EditController::read()
    {
        const EditState& state = _model.edit();
        const MemoryItem item = *state.item;
        _controller.runOnSampler<Reading>(
            READING_LABEL + item.name,
            [binding = bindingOf(state), name = item.name, values = definitionsOf(state.values), info = definitionsOf(state.info),
             keygroup = state.keygroup](mcp::SamplerGateway& gateway) { return readOn(*binding, gateway, name, values, info, keygroup); },
            [this, alive = std::weak_ptr<int>(_alive), item, tab = state.tab, keygroup = state.keygroup](const Reading& reading) {
                const EditState& now = _model.edit();
                if (alive.expired() || !showsScreen(now, item, tab, keygroup))
                    return;  // another item, tab or keygroup was chosen meanwhile: its own reading follows
                _model.setEdit(withReading(now, reading));
            });
    }

    void EditController::set(std::size_t index, std::int64_t value)
    {
        const EditState& state = _model.edit();
        if (state.layout == nullptr || index >= state.values.size())
            return;
        const EditValue& shown = state.values[index];
        const std::string label = settingLabel(shown, state.item->name);
        if (isZoneSample(shown.definition) && value == 0)
        {
            _model.setProblem(Problem{label, NO_SAMPLE_REMOVAL});  // the gateway assigns a sample, it cannot remove one [RQ-APP-060]
            return;
        }
        const auto [min, max] = rangeOf(state, shown);
        if (value < min || value > max)
        {
            _model.setProblem(Problem{label, "The value must be from " + std::to_string(min) + " to " + std::to_string(max) + ": nothing was sent."});
            return;
        }
        // A value between two of the catalogue's steps (the attenuation's 6 dB) is refused as the gateway would. [RQ-APP-097, DEC-APP-036]
        if (const auto resolved = mcp::resolveValue(*shown.definition, value); !isZoneSample(shown.definition) && !resolved.value.has_value())
        {
            std::string reason = resolved.problem;
            if (reason.ends_with('.'))
                reason.pop_back();
            _model.setProblem(Problem{label, reason + NOTHING_SENT});
            return;
        }
        EditState next = state;
        next.values[index].pending = true;
        _model.setEdit(std::move(next));
        if (_writing.contains(index))
        {
            _waiting[index] = value;  // sent when the write in flight ends, a later value replacing it [RQ-APP-054]
            return;
        }
        write(index, value);
    }

    void EditController::writeWaiting(std::size_t index)
    {
        const auto waiting = _waiting.find(index);
        const std::int64_t latest = waiting->second;
        _waiting.erase(waiting);
        write(index, latest);  // [RQ-APP-054]
    }

    void EditController::write(std::size_t index, std::int64_t value)
    {
        const EditState& state = _model.edit();
        const MemoryItem item = *state.item;
        const EditValue& shown = state.values[index];
        const std::string label = settingLabel(shown, item.name);
        _writing.insert(index);
        const bool queued = _controller.runOnSampler<Written>(
            label,
            [binding = bindingOf(state), name = item.name, target = Write{shown.definition, shown.zone, state.samples}, value,
             tab = definitionsOf(state.values), keygroup = state.keygroup](mcp::SamplerGateway& gateway) {
                return mcp::Outcome<Written>::success(writeOn(*binding, gateway, name, target, value, tab, keygroup));
            },
            [this, alive = std::weak_ptr<int>(_alive), item, index, label, tab = state.tab, keygroup = state.keygroup](const Written& written) {
                if (alive.expired())
                    return;
                if (const EditState& now = _model.edit(); !showsScreen(now, item, tab, keygroup))
                {
                    if (written.problem && now.item == std::optional<MemoryItem>(item))
                        _model.setProblem(Problem{label, *written.problem});
                    return;  // another item, tab or keygroup is shown: its writes were forgotten when it was chosen
                }
                _writing.erase(index);
                const bool waiting = _waiting.contains(index);
                _model.setEdit(withWritten(_model.edit(), index, written, waiting));
                if (written.problem)
                    _model.setProblem(Problem{label, *written.problem});
                if (waiting)
                    writeWaiting(index);
            });
        if (!queued)
        {
            _writing.erase(index);
            EditState next = _model.edit();
            next.values[index].pending = false;
            _model.setEdit(std::move(next));
        }
    }
}
