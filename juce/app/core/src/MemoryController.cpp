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
#include "app/MemoryController.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace app
{
    // [RQ-APP-035 to RQ-APP-037, RQ-APP-042, ADR-APP-001 (DEC-APP-004), TASK-APP-015]
    namespace
    {
        constexpr const char* READING_LABEL = "Reading the sampler's memory";
        constexpr const char* SELECTING_LABEL = "Selecting ";

        std::optional<std::size_t> indexOf(const std::vector<std::string>& names, const std::string& name)
        {
            const auto found = std::find(names.begin(), names.end(), name);
            if (found == names.end())
                return std::nullopt;
            return static_cast<std::size_t>(found - names.begin());
        }

        MemoryList namedList(const mcp::NamedListing& listing)
        {
            MemoryList list;
            for (const mcp::NamedListEntry& entry : listing.entries)
                list.names.push_back(entry.name);
            if (listing.current)
                list.current = static_cast<std::size_t>(*listing.current);
            return list;
        }

        /// The six lists, their current items and the free memory, as the sampler holds them now.
        mcp::Outcome<MemoryState> readMemory(mcp::SamplerGateway& gateway)
        {
            MemoryState state;
            const auto programs = gateway.listPrograms();
            if (!programs.ok())
                return mcp::Outcome<MemoryState>::failure(programs.problem);
            const auto status = gateway.status();
            if (!status.ok())
                return mcp::Outcome<MemoryState>::failure(status.problem);
            MemoryList& programList = state.list(MemoryKind::Program);
            for (const mcp::ProgramEntry& program : *programs.value)
                programList.names.push_back(program.name);
            if (status.value->currentProgram)
                programList.current = indexOf(programList.names, *status.value->currentProgram);

            const auto samples = gateway.listSamples();
            if (!samples.ok())
                return mcp::Outcome<MemoryState>::failure(samples.problem);
            MemoryList& sampleList = state.list(MemoryKind::Sample);
            for (const mcp::SampleEntry& sample : samples.value->samples)
                sampleList.names.push_back(sample.name);
            if (samples.value->current)
                sampleList.current = static_cast<std::size_t>(*samples.value->current);

            const auto multis = gateway.listMultis();
            if (!multis.ok())
                return mcp::Outcome<MemoryState>::failure(multis.problem);
            MemoryList& multiList = state.list(MemoryKind::Multi);
            for (const mcp::MultiEntry& multi : multis.value->multis)
                multiList.names.push_back(multi.name);
            if (multis.value->current)
                multiList.current = static_cast<std::size_t>(*multis.value->current);

            const std::pair<MemoryKind, mcp::NamedListKind> named[] = {{MemoryKind::SongFile, mcp::NamedListKind::SongFile},
                                                                       {MemoryKind::SetList, mcp::NamedListKind::SetList},
                                                                       {MemoryKind::SceneList, mcp::NamedListKind::SceneList}};
            for (const auto& [kind, listKind] : named)
            {
                const auto listing = gateway.listNamedItems(listKind);
                if (!listing.ok())
                    return mcp::Outcome<MemoryState>::failure(listing.problem);
                state.list(kind) = namedList(*listing.value);
            }

            const auto info = gateway.readSystemInfo();
            if (!info.ok())
                return mcp::Outcome<MemoryState>::failure(info.problem);
            state.free = FreeMemory{info.value->freeWaveBytes, info.value->totalWaveBytes, info.value->freeWavePercent,
                                    info.value->freeMpksPercent};
            return mcp::Outcome<MemoryState>::success(std::move(state));
        }

        /// The select by name of the item's kind; the problem when the sampler refused it.
        std::optional<std::string> selectOnSampler(mcp::SamplerGateway& gateway, const MemoryItem& item)
        {
            std::string problem;
            switch (item.kind)
            {
                case MemoryKind::Program:
                    problem = gateway.selectProgramByName(item.name).problem;
                    break;
                case MemoryKind::Sample:
                    problem = gateway.selectSampleByName(item.name).problem;
                    break;
                case MemoryKind::Multi:
                    problem = gateway.selectMultiByName(item.name).problem;
                    break;
                case MemoryKind::SongFile:
                    problem = gateway.selectNamedItemByName(mcp::NamedListKind::SongFile, item.name).problem;
                    break;
                case MemoryKind::SceneList:
                    problem = gateway.selectNamedItemByName(mcp::NamedListKind::SceneList, item.name).problem;
                    break;
                case MemoryKind::SetList:
                    break;
            }
            if (problem.empty())
                return std::nullopt;
            return problem;
        }

        /// What a select did: the problem, if the sampler refused it.
        struct Selected
        {
            MemoryItem item;
            std::optional<std::string> problem;
        };
    }

    MemoryController::MemoryController(Model& model, Controller& controller) : _model(model), _controller(controller)
    {
        _model.addListener(*this);
    }

    MemoryController::~MemoryController()
    {
        _model.removeListener(*this);
    }

    void MemoryController::modelChanged(ModelPart part)
    {
        if (part == ModelPart::MemoryChanged)
        {
            read(_model.lastMemoryChange().select);
            return;
        }
        if (part != ModelPart::Connection)
            return;
        const ConnectionState state = _model.connection().state;
        const ConnectionState last = std::exchange(_lastState, state);
        if (state == ConnectionState::Connected && last != ConnectionState::Connected)
            refresh();
        else if (state != ConnectionState::Connected && last == ConnectionState::Connected)
            _model.setMemory(MemoryState{});
    }

    void MemoryController::refresh()
    {
        read(std::nullopt);
    }

    void MemoryController::read(std::optional<MemoryItem> select)
    {
        _controller.runOnSampler<MemoryState>(READING_LABEL, readMemory,
                                              [this, alive = std::weak_ptr<int>(_alive), select](const MemoryState& read) {
                                                  if (alive.expired())
                                                      return;
                                                  MemoryState state = read;
                                                  // The selection stays on an item that is still there.
                                                  const auto& previous = _model.memory().selection;
                                                  if (previous && indexOf(state.list(previous->kind).names, previous->name))
                                                      state.selection = previous;
                                                  _model.setMemory(std::move(state));
                                                  if (select)
                                                      this->select(*select);
                                              });
    }

    void MemoryController::select(MemoryItem item)
    {
        _controller.runOnSampler<Selected>(
            SELECTING_LABEL + item.name,
            [item](mcp::SamplerGateway& gateway) { return mcp::Outcome<Selected>::success(Selected{item, selectOnSampler(gateway, item)}); },
            [this, alive = std::weak_ptr<int>(_alive)](const Selected& selected) {
                if (alive.expired())
                    return;
                MemoryState state = _model.memory();
                MemoryList& list = state.list(selected.item.kind);
                if (selected.problem)
                {
                    // Back to the sampler's current item of that kind, which did not change. [RQ-APP-037]
                    const auto current = state.current(selected.item.kind);
                    state.selection = current ? std::optional<MemoryItem>(MemoryItem{selected.item.kind, *current}) : std::nullopt;
                    _model.setMemory(std::move(state));
                    _model.setProblem(Problem{SELECTING_LABEL + selected.item.name, *selected.problem});
                    return;
                }
                if (selected.item.kind != MemoryKind::SetList)
                    list.current = indexOf(list.names, selected.item.name);
                state.selection = selected.item;
                _model.setMemory(std::move(state));
            });
    }
}
