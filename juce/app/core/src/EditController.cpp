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
#include <cctype>
#include <string>
#include <utility>
#include <vector>

#include "app/EditorLayouts.generated.hpp"

namespace app
{
    // [RQ-APP-048, RQ-APP-053, RQ-APP-086 to RQ-APP-088, RQ-APP-090, ADR-APP-004 (DEC-APP-025, DEC-APP-028), TASK-APP-036, TASK-APP-041]
    namespace
    {
        constexpr const char* SAMPLE_EDITOR = "sample";
        constexpr const char* READING_LABEL = "Reading ";
        constexpr const char* SETTING_LABEL = "Setting the ";
        constexpr const char* OF = " of ";
        /// The unit of a position in a sample: bounded by the sample's length rather than the catalogue's whole word. [RQ-APP-088]
        constexpr const char* POSITION_UNIT = "sample points";
        constexpr const char* LENGTH_NAME = "sample length";
        constexpr const char* LOOP_START_NAME = "sample loop start";
        constexpr const char* LOOP_END_NAME = "sample loop end";

        /// The editor layout of a kind, or null while the kind has no editor. [RQ-APP-048]
        const layouts::EditorLayout* layoutOf(MemoryKind kind)
        {
            if (kind != MemoryKind::Sample)
                return nullptr;
            const auto& all = layouts::kEditorLayouts;
            const auto found =
                std::find_if(all.begin(), all.end(), [](const layouts::EditorLayout& layout) { return layout.editor == SAMPLE_EDITOR; });
            return found == all.end() ? nullptr : &*found;
        }

        const mcp::ParameterDefinition* definitionOf(std::string_view name)
        {
            const auto& rows = mcp::ParameterCatalogue::samples().parameters();
            const auto found = std::find_if(rows.begin(), rows.end(), [name](const mcp::ParameterDefinition& row) { return row.name == name; });
            return found == rows.end() ? nullptr : &*found;
        }

        /// The tab's parameters in the layout's order, then the catalogue's read-only rows, each with the catalogue's range; nothing read yet.
        EditState emptyState(const MemoryItem& item, const layouts::EditorLayout* layout)
        {
            EditState state;
            state.item = item;
            state.layout = layout;
            if (layout == nullptr || layout->tabs.empty())
                return state;
            for (const layouts::LayoutGroup& group : layout->tabs[state.tab].groups)
            {
                for (const layouts::LayoutParameter& parameter : group.parameters)
                {
                    if (const mcp::ParameterDefinition* definition = definitionOf(parameter.name))
                        state.values.push_back(EditValue{definition, &parameter, 0, definition->min, definition->max, false});
                }
            }
            for (const mcp::ParameterDefinition& definition : mcp::ParameterCatalogue::samples().parameters())
            {
                if (definition.readOnly)
                    state.info.push_back(EditValue{&definition, nullptr, 0, definition.min, definition.max, false});
            }
            return state;
        }

        /// What one reading brings back: the tab's values and the information, in the state's order.
        struct Reading
        {
            std::vector<std::int64_t> values;
            std::vector<std::int64_t> info;
        };

        using Definitions = std::vector<const mcp::ParameterDefinition*>;

        /// Each parameter of `rows` read from the current sample, into `into`; the problem of the first that fails.
        std::optional<std::string> readInto(mcp::SamplerGateway& gateway, const Definitions& rows, std::vector<std::int64_t>& into)
        {
            for (const mcp::ParameterDefinition* definition : rows)
            {
                const auto read = gateway.readSampleParameter(*definition);
                if (!read.ok())
                    return read.problem;
                into.push_back(*read.value);
            }
            return std::nullopt;
        }

        mcp::Outcome<Reading> readOn(mcp::SamplerGateway& gateway, const std::string& sample, const Definitions& values, const Definitions& info)
        {
            // The edited sample is made the sampler's current one first: the front panel may have moved it. [DEC-APP-025]
            const auto selected = gateway.selectSampleByName(sample);
            if (!selected.ok())
                return mcp::Outcome<Reading>::failure(selected.problem);
            Reading reading;
            for (const auto& [rows, into] : {std::pair{&values, &reading.values}, std::pair{&info, &reading.info}})
            {
                if (const auto problem = readInto(gateway, *rows, *into))
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
        };

        Written writeOn(mcp::SamplerGateway& gateway, const std::string& sample, const mcp::ParameterDefinition& parameter, std::int64_t value,
                        const Definitions& tab)
        {
            Written written;
            const auto selected = gateway.selectSampleByName(sample);
            if (!selected.ok())
            {
                written.problem = selected.problem;
                return written;
            }
            // The S5000's Set Loop End moves the loop start to keep the loop length: the loop start held before is sent again after it, Set
            // Loop Start moving nothing else. A start beyond the new end (2^28 - 1 after a start pushed below 0) is not. [RQ-APP-090, DEC-APP-028]
            std::optional<std::int64_t> loopStart;
            const mcp::ParameterDefinition* loopStartParameter = definitionOf(LOOP_START_NAME);
            if (parameter.name == LOOP_END_NAME && loopStartParameter != nullptr)
            {
                if (const auto read = gateway.readSampleParameter(*loopStartParameter); read.ok() && *read.value <= value)
                    loopStart = *read.value;
            }
            if (const auto set = gateway.writeSampleParameter(parameter, value); !set.ok())
                written.problem = set.problem;
            else if (loopStart)
            {
                if (const auto again = gateway.writeSampleParameter(*loopStartParameter, *loopStart); !again.ok())
                    written.problem = again.problem;
            }
            if (const auto problem = readInto(gateway, tab, written.values))
            {
                written.problem = written.problem ? *written.problem + " " + *problem : *problem;
                written.values.clear();
            }
            return written;
        }

        std::string lowerCase(std::string_view text)
        {
            std::string lower(text);
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lower;
        }

        /// The value shown for `name`, if the tab has it.
        std::optional<std::int64_t> shownValue(const EditState& state, std::string_view name)
        {
            for (const EditValue& value : state.values)
            {
                if (value.definition->name == name)
                    return value.value;
            }
            return std::nullopt;
        }

        /// The range a value may take now: its own, narrowed by its nearest neighbours in each order of the layout; a neighbour outside the
        /// value's own range (2^28 - 1, a loop start the S5000 pushed below 0) bounds nothing. [RQ-APP-088, RQ-APP-092, DEC-APP-029]
        std::pair<std::int64_t, std::int64_t> rangeOf(const EditState& state, const EditValue& shown)
        {
            std::int64_t min = shown.min;
            std::int64_t max = shown.max;
            const auto within = [&shown](std::int64_t value) { return value >= shown.min && value <= shown.max; };
            for (const std::span<const std::string_view> order : state.layout->orders)
            {
                const auto at = std::find(order.begin(), order.end(), shown.definition->name);
                if (at == order.end())
                    continue;
                for (auto before = at; before != order.begin();)
                {
                    if (const auto value = shownValue(state, *--before); value && within(*value))
                    {
                        min = std::max(min, *value);
                        break;
                    }
                }
                for (auto after = at + 1; after != order.end(); ++after)
                {
                    if (const auto value = shownValue(state, *after); value && within(*value))
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
                definitions.push_back(value.definition);
            return definitions;
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

    void EditController::read()
    {
        const EditState& state = _model.edit();
        const MemoryItem item = *state.item;
        _controller.runOnSampler<Reading>(
            READING_LABEL + item.name,
            [sample = item.name, values = definitionsOf(state.values), info = definitionsOf(state.info)](mcp::SamplerGateway& gateway) {
                return readOn(gateway, sample, values, info);
            },
            [this, alive = std::weak_ptr<int>(_alive), item](const Reading& reading) {
                if (alive.expired() || _model.edit().item != std::optional<MemoryItem>(item))
                    return;  // another item was selected meanwhile: its own reading follows
                EditState next = _model.edit();
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
                    if (value.definition->unit == POSITION_UNIT)
                        value.max = length;  // [RQ-APP-088]
                }
                _model.setEdit(std::move(next));
            });
    }

    void EditController::set(std::size_t index, std::int64_t value)
    {
        const EditState& state = _model.edit();
        if (state.layout == nullptr || index >= state.values.size())
            return;
        const EditValue& shown = state.values[index];
        const std::string label = SETTING_LABEL + lowerCase(shown.layout->label) + OF + state.item->name;
        const auto [min, max] = rangeOf(state, shown);
        if (value < min || value > max)
        {
            _model.setProblem(Problem{label, "The value must be from " + std::to_string(min) + " to " + std::to_string(max) + ": nothing was sent."});
            return;
        }
        EditState next = state;
        next.values[index].pending = true;
        _model.setEdit(std::move(next));
        if (_writing.count(index) != 0)
        {
            _waiting[index] = value;  // sent when the write in flight ends, a later value replacing it [RQ-APP-054]
            return;
        }
        write(index, value);
    }

    void EditController::write(std::size_t index, std::int64_t value)
    {
        const EditState& state = _model.edit();
        const MemoryItem item = *state.item;
        const EditValue& shown = state.values[index];
        const std::string label = SETTING_LABEL + lowerCase(shown.layout->label) + OF + item.name;
        _writing.insert(index);
        const bool queued = _controller.runOnSampler<Written>(
            label,
            [sample = item.name, parameter = shown.definition, value, tab = definitionsOf(state.values)](mcp::SamplerGateway& gateway) {
                return mcp::Outcome<Written>::success(writeOn(gateway, sample, *parameter, value, tab));
            },
            [this, alive = std::weak_ptr<int>(_alive), item, index, label](const Written& written) {
                if (alive.expired() || _model.edit().item != std::optional<MemoryItem>(item))
                    return;  // another item is edited: open() forgot the writes of this one
                _writing.erase(index);
                EditState next = _model.edit();
                for (std::size_t i = 0; i < next.values.size() && i < written.values.size(); ++i)
                    next.values[i].value = written.values[i];
                const auto waiting = _waiting.find(index);
                next.values[index].pending = waiting != _waiting.end();
                _model.setEdit(std::move(next));
                if (written.problem)
                    _model.setProblem(Problem{label, *written.problem});
                if (waiting != _waiting.end())
                {
                    const std::int64_t latest = waiting->second;
                    _waiting.erase(waiting);
                    write(index, latest);
                }
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
