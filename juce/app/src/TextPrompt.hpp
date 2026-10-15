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

#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>
#include <string>
#include <vector>

namespace app::ui
{
    /// One field of a text prompt: its label and the text it starts with.
    struct TextField
    {
        juce::String label;
        juce::String initial;
    };

    /// Asks for one or more texts in a dialog that does not block the message thread; `onOk` gets them, in the order of the fields, when the
    /// person validates, and nothing happens on a cancel. Used for the names of new and renamed items. [RQ-APP-030, RQ-APP-031, RQ-APP-038,
    /// RQ-APP-039, ADR-APP-001 (DEC-APP-001), ADR-APP-002 (DEC-APP-013), TASK-APP-018]
    void askTexts(const juce::String& title, const juce::String& message, std::vector<TextField> fields,
                  std::function<void(const std::vector<std::string>&)> onOk);
}
