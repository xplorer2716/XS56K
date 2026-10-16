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
#include <array>
#include <cctype>
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
            const auto found = std::ranges::find(names, name);
            if (found == names.end())
                return std::nullopt;
            return static_cast<std::size_t>(found - names.begin());
        }

        MemoryList namedList(const mcp::NamedListing& listing)
        {
            MemoryList list;
            for (const mcp::NamedListEntry& entry : listing.entries)
                list.names.push_back(entry.name);
            if (listing.current.has_value())
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
            if (samples.value->current.has_value())
                sampleList.current = static_cast<std::size_t>(*samples.value->current);

            const auto multis = gateway.listMultis();
            if (!multis.ok())
                return mcp::Outcome<MemoryState>::failure(multis.problem);
            MemoryList& multiList = state.list(MemoryKind::Multi);
            for (const mcp::MultiEntry& multi : multis.value->multis)
                multiList.names.push_back(multi.name);
            if (multis.value->current.has_value())
                multiList.current = static_cast<std::size_t>(*multis.value->current);

            const std::array<std::pair<MemoryKind, mcp::NamedListKind>, 3> named{{{MemoryKind::SongFile, mcp::NamedListKind::SongFile},
                                                                                   {MemoryKind::SetList, mcp::NamedListKind::SetList},
                                                                                   {MemoryKind::SceneList, mcp::NamedListKind::SceneList}}};
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

    // [RQ-APP-011, RQ-APP-038 to RQ-APP-041, ADR-APP-001 (DEC-APP-005, DEC-APP-007), TASK-APP-016]
    namespace
    {
        constexpr const char* CREATING_LABEL = "Creating the program ";
        constexpr const char* RENAMING_LABEL = "Renaming ";
        constexpr const char* DELETING_LABEL = "Deleting ";
        constexpr const char* AUDITIONING_LABEL = "Auditioning ";
        constexpr const char* STOPPING_LABEL = "Stopping the audition";
        constexpr const char* NOT_DONE = "The sampler was sent nothing: the item to delete was not the current one.";
        constexpr const char* SET_LIST_RENAME = "A set list is renamed by its name, not from the memory view's current item.";

        /// The key the sampler compares names by: upper case, without spaces or hyphens.
        std::string nameKey(const std::string& name)
        {
            std::string key;
            for (const char c : name)
                if (c != ' ' && c != '-')
                    key.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            return key;
        }

        std::string kindName(MemoryKind kind)
        {
            using enum app::MemoryKind;
            switch (kind)
            {
                case Program:
                    return "program";
                case Sample:
                    return "sample";
                case Multi:
                    return "multi";
                case SongFile:
                    return "song file";
                case SetList:
                    return "set list";
                case SceneList:
                    break;
            }
            return "scenelist";
        }

        template <typename T>
        mcp::Outcome<bool> doneOf(const mcp::Outcome<T>& outcome)
        {
            return outcome.ok() ? mcp::Outcome<bool>::success(true) : mcp::Outcome<bool>::failure(outcome.problem);
        }

        mcp::Outcome<bool> deletedOf(bool deleted, const std::string& problem)
        {
            if (!problem.empty())
                return mcp::Outcome<bool>::failure(problem);
            return deleted ? mcp::Outcome<bool>::success(true) : mcp::Outcome<bool>::failure(NOT_DONE);
        }

        /// Renames the item: the current one of its kind once selected, a set list by its name.
        mcp::Outcome<bool> renameItem(mcp::SamplerGateway& gateway, const MemoryItem& item, const std::string& newName)
        {
            if (item.kind == MemoryKind::SetList)
                return doneOf(gateway.renameSetList(item.name, newName));
            if (const auto problem = selectOnSampler(gateway, item))
                return mcp::Outcome<bool>::failure(*problem);
            switch (item.kind)
            {
                case MemoryKind::Program:
                    return doneOf(gateway.renameCurrentProgram(newName));
                case MemoryKind::Sample:
                    return doneOf(gateway.renameCurrentSample(newName));
                case MemoryKind::Multi:
                    return doneOf(gateway.renameCurrentMulti(newName));
                case MemoryKind::SongFile:
                    return doneOf(gateway.renameCurrentNamedItem(mcp::NamedListKind::SongFile, newName));
                case MemoryKind::SceneList:
                    return doneOf(gateway.renameCurrentNamedItem(mcp::NamedListKind::SceneList, newName));
                case MemoryKind::SetList:
                    break;
            }
            return mcp::Outcome<bool>::failure(SET_LIST_RENAME);
        }

        /// Deletes the item: the current one of its kind once selected, a set list by its name. The person confirmed this very name,
        /// which is the gateway's typed confirmation. [DEC-APP-007]
        mcp::Outcome<bool> deleteItem(mcp::SamplerGateway& gateway, const MemoryItem& item)
        {
            if (item.kind == MemoryKind::SetList)
            {
                const auto deleted = gateway.deleteSetList(item.name, item.name);
                return deletedOf(deleted.ok() && deleted.value->done, deleted.problem);
            }
            if (const auto problem = selectOnSampler(gateway, item))
                return mcp::Outcome<bool>::failure(*problem);
            switch (item.kind)
            {
                case MemoryKind::Program:
                {
                    const auto deleted = gateway.deleteCurrentProgram(item.name);
                    return deletedOf(deleted.ok() && deleted.value->deleted, deleted.problem);
                }
                case MemoryKind::Sample:
                {
                    const auto deleted = gateway.deleteCurrentSample(item.name);
                    return deletedOf(deleted.ok() && deleted.value->done, deleted.problem);
                }
                case MemoryKind::Multi:
                {
                    const auto deleted = gateway.deleteCurrentMulti(item.name);
                    return deletedOf(deleted.ok() && deleted.value->done, deleted.problem);
                }
                case MemoryKind::SongFile:
                case MemoryKind::SceneList:
                case MemoryKind::SetList:
                    break;
            }
            const auto kind = item.kind == MemoryKind::SongFile ? mcp::NamedListKind::SongFile : mcp::NamedListKind::SceneList;
            const auto deleted = gateway.deleteCurrentNamedItem(kind, item.name);
            return deletedOf(deleted.ok() && deleted.value->done, deleted.problem);
        }
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
        using enum app::ConnectionState;
        if (part == ModelPart::MemoryChanged)
        {
            read(_model.lastMemoryChange().select);
            return;
        }
        if (part != ModelPart::Connection)
            return;
        const ConnectionState state = _model.connection().state;
        const ConnectionState last = std::exchange(_lastState, state);
        if (state == Connected && last != Connected)
            refresh();
        else if (state != Connected && last == Connected)
            _model.setMemory(MemoryState{});
    }

    void MemoryController::refresh()
    {
        read(std::nullopt);
    }

    void MemoryController::read(const std::optional<MemoryItem>& select)
    {
        _controller.runOnSampler<MemoryState>(READING_LABEL, readMemory,
                                              [this, alive = std::weak_ptr<int>(_alive), select](const MemoryState& read) {
                                                  if (alive.expired())
                                                      return;
                                                  MemoryState state = read;
                                                  // The selection stays on an item that is still there.
                                                  if (const auto& previous = _model.memory().selection;
                                                      previous.has_value() && indexOf(state.list(previous->kind).names, previous->name).has_value())
                                                      state.selection = previous;
                                                  _model.setMemory(std::move(state));
                                                  if (select.has_value())
                                                      this->select(*select);
                                              });
    }

    void MemoryController::select(const MemoryItem& item)
    {
        if (_auditioning && !(*_auditioning == item))
            stopAudition();
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

    bool MemoryController::refuseTakenName(MemoryKind kind, const std::string& name, const std::string& action)
    {
        const auto& names = _model.memory().list(kind).names;
        const auto taken =
            std::ranges::find_if(names, [key = nameKey(name)](const std::string& other) { return nameKey(other) == key; });
        if (taken == names.end())
            return false;
        _model.setProblem(Problem{action, "A " + kindName(kind) + " named \"" + *taken + "\" is already in memory."});
        return true;
    }

    void MemoryController::createProgram(const std::string& name, int keygroups)
    {
        const std::string label = CREATING_LABEL + name;
        if (refuseTakenName(MemoryKind::Program, name, label))
            return;
        _controller.runOnSampler<bool>(
            label, [name, keygroups](mcp::SamplerGateway& gateway) { return doneOf(gateway.createProgram(name, keygroups)); },
            [this, alive = std::weak_ptr<int>(_alive), name](const bool&) {
                if (!alive.expired())
                    _model.memoryChanged(MemoryItem{MemoryKind::Program, name});
            });
    }

    void MemoryController::rename(const MemoryItem& item, const std::string& newName)
    {
        const std::string label = RENAMING_LABEL + item.name;
        if (nameKey(newName) != nameKey(item.name) && refuseTakenName(item.kind, newName, label))
            return;
        _controller.runOnSampler<bool>(
            label, [item, newName](mcp::SamplerGateway& gateway) { return renameItem(gateway, item, newName); },
            [this, alive = std::weak_ptr<int>(_alive), renamed = MemoryItem{item.kind, newName}](const bool&) {
                if (!alive.expired())
                    _model.memoryChanged(renamed);
            });
    }

    void MemoryController::remove(const MemoryItem& item)
    {
        const Confirmation question{"Delete the " + kindName(item.kind) + "?", item.name,
                                    "The " + kindName(item.kind) + " \"" + item.name +
                                        "\" is deleted from the sampler's memory; unless it was saved to a disk, it is lost."};
        if (_auditioning && *_auditioning == item)
            stopAudition();
        _controller.runOnSamplerConfirmed<bool>(
            question, DELETING_LABEL + item.name, [item](mcp::SamplerGateway& gateway) { return deleteItem(gateway, item); },
            [this, alive = std::weak_ptr<int>(_alive)](const bool&) {
                if (!alive.expired())
                    _model.memoryChanged(std::nullopt);
            });
    }

    void MemoryController::audition(const MemoryItem& sample)
    {
        if (_auditioning && !(*_auditioning == sample))
            stopAudition();
        const bool queued = _controller.runOnSampler<std::string>(
            AUDITIONING_LABEL + sample.name,
            [sample](mcp::SamplerGateway& gateway) {
                if (const auto problem = selectOnSampler(gateway, sample))
                    return mcp::Outcome<std::string>::failure(*problem);
                return gateway.startSampleAudition();
            },
            [this, alive = std::weak_ptr<int>(_alive), sample](const std::string&) {
                if (alive.expired())
                    return;
                MemoryState state = _model.memory();
                MemoryList& list = state.list(sample.kind);
                list.current = indexOf(list.names, sample.name);
                state.selection = sample;
                _model.setMemory(std::move(state));
            });
        if (queued)
            _auditioning = sample;
    }

    void MemoryController::stopAudition()
    {
        if (!_auditioning)
            return;
        _auditioning.reset();
        _controller.runOnSampler<bool>(STOPPING_LABEL, [](mcp::SamplerGateway& gateway) { return gateway.stopSampleAudition(); },
                                       [](const bool&) { /* stopping has no result to show */ });
    }

    void MemoryController::toggleAudition(const MemoryItem& item)
    {
        if (item.kind != MemoryKind::Sample)
            return;
        if (_auditioning && *_auditioning == item)
            stopAudition();
        else
            audition(item);
    }
}
