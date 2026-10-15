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

#include "app/Controller.hpp"
#include "app/Edit.hpp"
#include "app/Model.hpp"

namespace app
{
    /// The edit panel's controller: it follows the memory view's selection and, for a kind that has an editor layout, reads the tab's parameters
    /// and the editor's read-only information through the controller's jobs, into the model's edit part; a kind with no layout is only named.
    /// It reads again on request and empties the panel when the connection ends. Called on the UI thread only. [RQ-APP-048, RQ-APP-050,
    /// RQ-APP-053, RQ-APP-086, RQ-APP-087, ADR-APP-004 (DEC-APP-025), TASK-APP-036]
    class EditController final : public ModelListener
    {
    public:
        EditController(Model& model, Controller& controller);
        ~EditController() override;

        EditController(const EditController&) = delete;
        EditController& operator=(const EditController&) = delete;

        /// Reads every parameter of the tab shown and the information again. [RQ-APP-053]
        void readAgain();

        void modelChanged(ModelPart part) override;

    private:
        void open(std::optional<MemoryItem> item);
        void read();

        Model& _model;
        Controller& _controller;
        /// Expires with the controller: a result that comes back after it is ignored.
        std::shared_ptr<int> _alive = std::make_shared<int>(0);
    };
}
