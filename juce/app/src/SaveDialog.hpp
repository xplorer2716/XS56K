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

#include <functional>
#include <optional>
#include <string>

#include "app/Memory.hpp"
#include "app/Save.hpp"

namespace app::ui
{
    class LookAndFeel;

    /// Opens the save dialog without blocking the message thread: the item to save among those of the memory (`proposed` chosen), what to
    /// save (the item alone or with its children, all the items of its kind with or without children, the whole memory), the destination
    /// shown, and "check names". `onSave` gets the request and the state of "check names" when the person presses Save; a cancel calls
    /// nothing. Every visual value comes from the design tokens. [RQ-APP-043 to RQ-APP-045, RQ-APP-002, ADR-APP-005 (DEC-APP-039),
    /// ADR-APP-002, TASK-APP-060]
    void showSaveDialog(LookAndFeel& lookAndFeel, const MemoryState& memory, const std::optional<MemoryItem>& proposed, const std::string& destination,
                        bool checkNames, std::function<void(const SaveRequest&, bool checkNames)> onSave);
}
