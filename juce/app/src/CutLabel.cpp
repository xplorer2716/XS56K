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
#include "CutLabel.hpp"

#include <algorithm>

namespace app::ui
{
    // [RQ-APP-080, TASK-APP-022]
    namespace
    {
        constexpr float NO_SQUEEZE = 1.0f;
    }

    CutLabel::CutLabel()
    {
        setMinimumHorizontalScale(NO_SQUEEZE);
    }

    bool CutLabel::isCut() const
    {
        const juce::Font textFont = getFont();
        const auto area = getBorderSize().subtractedFrom(getLocalBounds());
        if (area.getWidth() <= 0 || area.getHeight() <= 0)
            return getText().isNotEmpty();
        // The lines the label has room for, and the lines the text takes once broken between its words, as the label breaks it.
        const int lines = std::max(1, static_cast<int>(static_cast<float>(area.getHeight()) / textFont.getHeight()));
        if (lines == 1)
            return juce::GlyphArrangement::getStringWidth(textFont, getText()) > static_cast<float>(area.getWidth());
        juce::GlyphArrangement glyphs;
        glyphs.addJustifiedText(textFont, getText(), 0.0f, 0.0f, static_cast<float>(area.getWidth()), juce::Justification::left);
        const float height = glyphs.getBoundingBox(0, -1, true).getHeight();
        return juce::roundToInt(height / textFont.getHeight()) > lines;
    }

    juce::String CutLabel::getTooltip()
    {
        const juce::String set = juce::Label::getTooltip();
        if (set.isNotEmpty())
            return set;
        return isCut() ? getText() : juce::String();
    }
}
