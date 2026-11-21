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
#include "ChoiceBox.hpp"

#include "LookAndFeel.hpp"

namespace app::ui
{
    // [RQ-APP-098, DEC-APP-036, TASK-APP-053]
    namespace
    {
        constexpr int PREVIOUS = -1;
        constexpr int NEXT = 1;
    }

    void ChoiceBox::mouseDown(const juce::MouseEvent& event)
    {
        const int arrow = LookAndFeel::choiceArrowWidth();
        const int x = event.getEventRelativeTo(this).x;
        if (!isEnabled() || event.mods.isPopupMenu() || (x >= arrow && x < getWidth() - arrow))
        {
            juce::ComboBox::mouseDown(event);
            return;
        }
        step(x < arrow ? PREVIOUS : NEXT);
    }

    void ChoiceBox::step(int direction)
    {
        if (onStep)
        {
            onStep(direction);
            return;
        }
        const int next = getSelectedItemIndex() + direction;
        if (next >= 0 && next < getNumItems())
            setSelectedItemIndex(next, juce::sendNotification);
    }
}
