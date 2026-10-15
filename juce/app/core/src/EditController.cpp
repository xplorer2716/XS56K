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
#include <string>
#include <utility>
#include <vector>

#include "app/EditorLayouts.generated.hpp"

namespace app
{
    // [RQ-APP-048, RQ-APP-053, RQ-APP-086 to RQ-APP-088, ADR-APP-004 (DEC-APP-025), TASK-APP-036]
    namespace
    {
        constexpr const char* SAMPLE_EDITOR = "sample";
        constexpr const char* READING_LABEL = "Reading ";
        /// The unit of a position in a sample: bounded by the sample's length rather than the catalogue's whole word. [RQ-APP-088]
        constexpr const char* POSITION_UNIT = "sample points";
        constexpr const char* LENGTH_NAME = "sample length";

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

        mcp::Outcome<Reading> readOn(mcp::SamplerGateway& gateway, const std::string& sample, const Definitions& values, const Definitions& info)
        {
            // The edited sample is made the sampler's current one first: the front panel may have moved it. [DEC-APP-025]
            const auto selected = gateway.selectSampleByName(sample);
            if (!selected.ok())
                return mcp::Outcome<Reading>::failure(selected.problem);
            Reading reading;
            for (const auto& [rows, into] : {std::pair{&values, &reading.values}, std::pair{&info, &reading.info}})
            {
                for (const mcp::ParameterDefinition* definition : *rows)
                {
                    const auto read = gateway.readSampleParameter(*definition);
                    if (!read.ok())
                        return mcp::Outcome<Reading>::failure(read.problem);
                    into->push_back(*read.value);
                }
            }
            return mcp::Outcome<Reading>::success(std::move(reading));
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
}
