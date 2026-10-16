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

namespace app::ui
{
    /// A label whose tooltip is its whole text when, and only when, the text does not fit its bounds; a tooltip set on it (a text that says
    /// more, such as an exact count of bytes) is given instead whenever there is one. It never squeezes its text. [RQ-APP-080, RQ-APP-001,
    /// ADR-APP-002 (DEC-APP-013), TASK-APP-022]
    class CutLabel final : public juce::Label
    {
    public:
        CutLabel();

        /// True when the text, in the label's font, needs more room than the label's lines give it.
        [[nodiscard]] bool isCut() const;
        juce::String getTooltip() override;
    };
}
