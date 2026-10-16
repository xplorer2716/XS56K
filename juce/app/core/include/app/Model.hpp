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
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "app/Browser.hpp"
#include "app/Edit.hpp"
#include "app/Memory.hpp"

namespace app
{
    /// Which part of the model changed, for the views to redraw only that. [ADR-APP-001 (DEC-APP-004)]
    enum class ModelPart
    {
        Busy,
        Problem,
        Connection,
        Browser,
        /// An action changed the sampler's memory: the memory view reads it again. [RQ-APP-036]
        MemoryChanged,
        /// The memory view's lists, free memory or selection. [RQ-APP-035]
        Memory,
        /// The edit panel: the item edited, its values and information. [RQ-APP-048, ADR-APP-004 (DEC-APP-025)]
        Edit,
    };

    /// Where the connection with the sampler stands. [RQ-APP-019]
    enum class ConnectionState
    {
        Disconnected,
        Connecting,
        Connected,
        Failed,
    };

    /// Which sampler answered: its model, its name, its operating system and the DeviceID it was reached at. [RQ-APP-017, RQ-APP-019]
    struct SamplerIdentity
    {
        std::string model;
        std::string name;
        std::string osVersion;
        int deviceId = 0;
    };

    /// The connection state, the reason of a failure (empty otherwise) and, once connected, the sampler. [RQ-APP-018, RQ-APP-019]
    struct ConnectionStatus
    {
        ConnectionState state = ConnectionState::Disconnected;
        std::string reason;
        std::optional<SamplerIdentity> sampler;
    };

    /// A command to the sampler that failed: the action, in the words of the job's label, and the reason, in the sampler's own words when it
    /// gave an error number. [RQ-APP-010]
    struct Problem
    {
        std::string action;
        std::string reason;
    };

    /// Told, on the UI thread, which part of the model changed. [ADR-APP-001 (DEC-APP-004)]
    class ModelListener
    {
    public:
        virtual ~ModelListener() = default;

        virtual void modelChanged(ModelPart part) = 0;
    };

    /// What the application has read from the sampler and what it is doing, as plain C++: changed only on the UI thread, by the controller's
    /// continuations, so it needs no lock. A value shown is a value read, never one the person asked for. [RQ-APP-010, RQ-APP-012,
    /// RQ-APP-014, ADR-APP-001 (DEC-APP-004)]
    class Model
    {
    public:
        /// The label of the job in progress, or nothing when the sampler is not busy. [RQ-APP-012]
        [[nodiscard]] const std::optional<std::string>& busy() const { return _busy; }
        /// The last failure, until the person dismisses it. [RQ-APP-010]
        [[nodiscard]] const std::optional<Problem>& problem() const { return _problem; }
        /// Always known: disconnected until the person connects. [RQ-APP-019]
        [[nodiscard]] const ConnectionStatus& connection() const { return _connection; }
        /// The file browser: the disks, the current folder, the tree listed so far and the selection. [RQ-APP-024 to RQ-APP-026]
        [[nodiscard]] const BrowserState& browser() const { return _browser; }
        /// The last change of the memory an action made. [RQ-APP-028, RQ-APP-036]
        [[nodiscard]] const MemoryChange& lastMemoryChange() const { return _memoryChange; }
        /// The memory view: the six lists, the free memory and the selection. [RQ-APP-035, RQ-APP-042]
        [[nodiscard]] const MemoryState& memory() const { return _memory; }
        /// The edit panel. [RQ-APP-048, RQ-APP-086, TASK-APP-036]
        [[nodiscard]] const EditState& edit() const { return _edit; }

        void setBusy(std::optional<std::string> label);
        void setProblem(Problem problem);
        void clearProblem();
        void setConnection(ConnectionStatus status);
        void setBrowser(BrowserState browser);
        /// Says that an action changed the memory, with the item to select afterwards, if any. [RQ-APP-028, RQ-APP-036]
        void memoryChanged(const std::optional<MemoryItem>& select);
        void setMemory(MemoryState memory);
        void setEdit(EditState edit);

        void addListener(ModelListener& listener);
        void removeListener(ModelListener& listener);

    private:
        void notify(ModelPart part) const;

        std::optional<std::string> _busy;
        std::optional<Problem> _problem;
        ConnectionStatus _connection;
        BrowserState _browser;
        MemoryChange _memoryChange;
        MemoryState _memory;
        EditState _edit;
        std::vector<ModelListener*> _listeners;
    };
}
