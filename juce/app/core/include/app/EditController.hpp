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

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>

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
        /// The person gives `value` to the tab's parameter at `index`: refused with nothing sent out of its range (a position beyond the
        /// sample's length, RQ-APP-088); otherwise pending until written and read back, with every value of the tab read again (RQ-APP-089).
        /// While a write of that parameter is in flight the value waits, a later one replacing it (RQ-APP-054). [RQ-APP-051, RQ-APP-052,
        /// ADR-APP-004 (DEC-APP-025, DEC-APP-026), TASK-APP-037]
        void set(std::size_t index, std::int64_t value);
        /// Shows the editor's tab at `index` and reads its values; nothing for the tab shown or one the editor does not have. [RQ-APP-049,
        /// TASK-APP-044]
        void showTab(std::size_t index);
        /// Shows the values of the program's keygroup `keygroup` (1 to its count) and reads them; nothing for a keygroup the program does
        /// not have. [RQ-APP-058, RQ-APP-094, TASK-APP-044]
        void chooseKeygroup(int keygroup);

        void modelChanged(ModelPart part) override;

    private:
        void open(std::optional<MemoryItem> item);
        void read();
        void write(std::size_t index, std::int64_t value);
        /// Sends the value that waited behind the write of the parameter at `index`, which has just ended.
        void writeWaiting(std::size_t index);

        Model& _model;
        Controller& _controller;
        /// The tab's parameters with a write in flight, and the value waiting behind each. [RQ-APP-054]
        std::set<std::size_t> _writing;
        std::map<std::size_t, std::int64_t> _waiting;
        /// Expires with the controller: a result that comes back after it is ignored.
        std::shared_ptr<int> _alive = std::make_shared<int>(0);
    };
}
