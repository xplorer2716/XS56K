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
// A choice whose arrows step it: a click on the left or right end chooses the previous or next item without opening the list, nothing past
// the first or the last; a click between them opens the list. Every choice of the application is one. [RQ-APP-098, ADR-APP-004 (DEC-APP-036),
// TASK-APP-053]
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace app::ui
{
    class ChoiceBox final : public juce::ComboBox
    {
    public:
        /// The step of an arrow, `direction` -1 or +1, when the owner decides it (the edit panel steps through the model's value); empty: the
        /// previous or next item.
        std::function<void(int direction)> onStep;

        void mouseDown(const juce::MouseEvent& event) override;

    private:
        void step(int direction);
    };
}
