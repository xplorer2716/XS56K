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

#include <memory>
#include <optional>
#include <string>

#include "app/Controller.hpp"
#include "app/Memory.hpp"
#include "app/Model.hpp"

namespace app
{
    /// The memory view's controller: it reads the sampler's six lists, their current items and the free memory through the controller's
    /// jobs, into the model's memory part. It reads them when the connection opens, after an action that changed the memory (the model's
    /// "memory changed", with the item to select) and on request, and empties them when the connection ends. Called on the UI thread only.
    /// [RQ-APP-035 to RQ-APP-037, RQ-APP-042, ADR-APP-001 (DEC-APP-001, DEC-APP-004)]
    class MemoryController final : public ModelListener
    {
    public:
        MemoryController(Model& model, Controller& controller);
        ~MemoryController() override;

        MemoryController(const MemoryController&) = delete;
        MemoryController& operator=(const MemoryController&) = delete;

        /// Reads the six lists and the free memory again. [RQ-APP-036, RQ-APP-042]
        void refresh();
        /// Makes `item` the sampler's current item of its kind (select by name) and the view's selection; when the sampler refuses, the
        /// selection goes back to the sampler's current item and the error is reported. A set list, of which the sampler has no current
        /// one, is only selected in the view. [RQ-APP-037, RQ-APP-010]
        void select(const MemoryItem& item);

        /// Creates a program of `keygroups` keygroups, which becomes current and selected; a name a program already bears (without regard to
        /// case, spaces or hyphens) is refused with nothing sent. [RQ-APP-038, TASK-APP-016]
        void createProgram(const std::string& name, int keygroups);
        /// Selects a program, a sample or a multi on the sampler and renames it; a name another item of the kind bears is refused with
        /// nothing sent. [RQ-APP-039, TASK-APP-016]
        void rename(const MemoryItem& item, const std::string& newName);
        /// After a confirmation naming it, selects a program, a sample or a multi on the sampler and deletes it; a cancel sends nothing.
        /// [RQ-APP-040, RQ-APP-011, TASK-APP-016]
        void remove(const MemoryItem& item);
        /// Selects a sample and starts its audition; it stops on `stopAudition` or when another item is selected. [RQ-APP-041, TASK-APP-016]
        void audition(const MemoryItem& sample);
        /// Stops the audition in progress, if any. [RQ-APP-041, TASK-APP-016]
        void stopAudition();
        /// The space bar: starts the audition of a sample not being auditioned, stops the one being auditioned; sends nothing for any other
        /// item (a song file has no play command in the SysEx). [RQ-APP-083, TASK-APP-028]
        void toggleAudition(const MemoryItem& item);

        void modelChanged(ModelPart part) override;

    private:
        void read(const std::optional<MemoryItem>& select);
        /// True, with the reason shown, when another item of the kind bears `name`. [RQ-APP-038, RQ-APP-039]
        bool refuseTakenName(MemoryKind kind, const std::string& name, const std::string& action);

        Model& _model;
        Controller& _controller;
        ConnectionState _lastState = ConnectionState::Disconnected;
        std::optional<MemoryItem> _auditioning;
        /// Expires with the controller: a result that comes back after it is ignored.
        std::shared_ptr<int> _alive = std::make_shared<int>(0);
    };
}
