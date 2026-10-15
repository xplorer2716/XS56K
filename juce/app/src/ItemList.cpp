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
#include "ItemList.hpp"

#include <algorithm>
#include <utility>

#include "LookAndFeel.hpp"
#include "app/DesignTokens.generated.hpp"

namespace app::ui
{
    // [RQ-APP-001, RQ-APP-004, RQ-APP-005, RQ-APP-026, ADR-APP-002 (DEC-APP-013, DEC-APP-014), TASK-APP-018]
    namespace
    {
        constexpr const char* CURRENT_TOOLTIP = " (current)";
        // The parts of an icon, as fractions of its side: the icons are drawn from the icon size of the design system.
        constexpr float THIRD = 1.0f / 3.0f;
        constexpr float HALF = 0.5f;
        constexpr float QUARTER = 0.25f;
        constexpr float WAVE_HEIGHTS[] = {0.4f, 1.0f, 0.6f, 0.8f, 0.3f};
        constexpr int LIST_LINES = 3;
    }

    ItemList::ItemList(LookAndFeel& lookAndFeel) : _lookAndFeel(lookAndFeel)
    {
        using namespace tokens;
        _list.setModel(this);
        _list.setRowHeight(pixels(kRowHeight));
        _list.setColour(juce::ListBox::backgroundColourId, colourOf(kColourSurfacePanel));
        _list.setColour(juce::ListBox::outlineColourId, colourOf(kColourSurfacePanel));
        _list.setOutlineThickness(0);
        _list.addKeyListener(this);
        addAndMakeVisible(_list);
    }

    void ItemList::setRows(std::vector<ItemRow> rows)
    {
        _rows = std::move(rows);
        _list.updateContent();
        const auto selected = std::find_if(_rows.begin(), _rows.end(), [](const ItemRow& row) { return row.selected; });
        _syncing = true;
        if (selected == _rows.end())
            _list.deselectAllRows();
        else
            _list.selectRow(static_cast<int>(selected - _rows.begin()), true, true);
        _syncing = false;
        _list.repaint();
    }

    void ItemList::resized()
    {
        _list.setBounds(getLocalBounds());
    }

    int ItemList::getNumRows()
    {
        return static_cast<int>(_rows.size());
    }

    void ItemList::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool)
    {
        using namespace tokens;
        if (rowNumber < 0 || rowNumber >= getNumRows())
            return;
        const ItemRow& row = _rows[static_cast<std::size_t>(rowNumber)];
        auto area = juce::Rectangle<int>(width, height);
        const int inside = pixels(kSpaceInside);

        if (row.heading)
        {
            g.setColour(colourOf(kColourTextSecondary));
            g.setFont(_lookAndFeel.font(kFontLabelSize, true).withExtraKerningFactor(kFontLabelTracking / kFontLabelSize));
            g.drawText(row.name.toUpperCase(), area.reduced(inside, 0), juce::Justification::centredLeft);
            return;
        }

        if (row.selected)
        {
            g.setColour(colourOf(kRowSurfaceHover));
            g.fillRect(area);
            g.setColour(colourOf(kRowBarSelected));
            g.fillRect(area.withWidth(pixels(kStrokeArc)));
        }

        auto content = area.reduced(inside, 0);
        content.removeFromLeft(row.depth * pixels(kSizeSpace4));
        if (row.current)
        {
            const auto dot = content.removeFromRight(pixels(kSizeIconSmall)).toFloat().withSizeKeepingCentre(kRadiusControl * 2, kRadiusControl * 2);
            g.setColour(colourOf(kRowDotCurrent));
            g.fillEllipse(dot);
            content.removeFromRight(pixels(kSpaceBetween));
        }
        if (row.icon != RowIcon::None)
        {
            const auto icon = content.removeFromLeft(pixels(kSizeIconSmall)).toFloat().withSizeKeepingCentre(kSizeIconSmall, kSizeIconSmall);
            paintIcon(g, row.icon, icon);
            content.removeFromLeft(pixels(kSizeSpace2));
        }
        if (row.detail.isNotEmpty())
        {
            g.setColour(colourOf(kColourTextSecondary));
            g.setFont(_lookAndFeel.font(kFontCaptionSize));
            const int detailWidth = juce::GlyphArrangement::getStringWidthInt(_lookAndFeel.font(kFontCaptionSize), row.detail);
            g.drawText(row.detail, content.removeFromRight(detailWidth), juce::Justification::centredRight);
            content.removeFromRight(pixels(kSpaceBetween));
        }
        g.setColour(colourOf(kColourTextPrimary));
        g.setFont(_lookAndFeel.font(kFontValueSize));
        g.drawText(row.name, content, juce::Justification::centredLeft, true);
    }

    void ItemList::paintIcon(juce::Graphics& g, RowIcon icon, juce::Rectangle<float> area) const
    {
        using namespace tokens;
        const float side = area.getWidth();
        const float stroke = kStrokeHairline;
        g.setColour(colourOf(icon == RowIcon::Sample ? kColourRoleSample : kColourTextSecondary));
        const auto page = area.withSizeKeepingCentre(side * HALF + side * QUARTER, side).reduced(stroke * HALF);
        switch (icon)
        {
            case RowIcon::Disk:
                g.drawRoundedRectangle(area.reduced(stroke * HALF), kRadiusControl, stroke);
                g.fillEllipse(area.withSizeKeepingCentre(side * QUARTER, side * QUARTER).translated(side * QUARTER, side * QUARTER));
                break;
            case RowIcon::Folder:
                g.drawRoundedRectangle(area.reduced(stroke * HALF), kRadiusControl, stroke);
                g.drawLine(area.getX(), area.getY() + side * THIRD, area.getRight(), area.getY() + side * THIRD, stroke);
                break;
            case RowIcon::Program:
                // A page with lines: a program, a set of parameters.
                g.drawRoundedRectangle(page, kRadiusControl, stroke);
                for (int line = 1; line <= LIST_LINES; ++line)
                {
                    const float y = page.getY() + page.getHeight() * static_cast<float>(line) / static_cast<float>(LIST_LINES + 1);
                    g.drawLine(page.getX() + side * QUARTER * HALF, y, page.getRight() - side * QUARTER * HALF, y, stroke);
                }
                break;
            case RowIcon::Sample:
            {
                // A waveform.
                const auto count = static_cast<float>(std::size(WAVE_HEIGHTS));
                const float step = side / count;
                float x = area.getX() + step * HALF;
                for (const float height : WAVE_HEIGHTS)
                {
                    g.drawLine(x, area.getCentreY() - side * height * HALF, x, area.getCentreY() + side * height * HALF, kStrokeFocus);
                    x += step;
                }
                break;
            }
            case RowIcon::Multi:
                // Two pages, one over the other: several programs.
                g.drawRoundedRectangle(page.translated(-side * QUARTER * HALF, side * QUARTER * HALF), kRadiusControl, stroke);
                g.drawRoundedRectangle(page.translated(side * QUARTER * HALF, -side * QUARTER * HALF), kRadiusControl, stroke);
                break;
            case RowIcon::SongFile:
                // A note.
                g.fillEllipse(area.getX(), area.getBottom() - side * THIRD, side * HALF, side * THIRD);
                g.drawLine(area.getX() + side * HALF, area.getY(), area.getX() + side * HALF, area.getBottom() - side * QUARTER, kStrokeFocus);
                break;
            case RowIcon::SetList:
                // A list of lines with bullets.
                for (int line = 0; line < LIST_LINES; ++line)
                {
                    const float y = area.getY() + side * (static_cast<float>(line) + HALF) / static_cast<float>(LIST_LINES);
                    g.fillEllipse(area.getX(), y - stroke, stroke * 2, stroke * 2);
                    g.drawLine(area.getX() + side * THIRD, y, area.getRight(), y, stroke);
                }
                break;
            case RowIcon::SceneList:
                // A frame with two dots: scenes.
                g.drawRoundedRectangle(area.reduced(stroke * HALF), kRadiusControl, stroke);
                g.fillEllipse(area.withSizeKeepingCentre(side * QUARTER, side * QUARTER).translated(-side * QUARTER, 0.0f));
                g.fillEllipse(area.withSizeKeepingCentre(side * QUARTER, side * QUARTER).translated(side * QUARTER, 0.0f));
                break;
            case RowIcon::Unknown:
                g.drawRoundedRectangle(page, kRadiusControl, stroke);
                break;
            case RowIcon::None:
                break;
        }
    }

    bool ItemList::isItem(int row) const
    {
        return row >= 0 && row < static_cast<int>(_rows.size()) && !_rows[static_cast<std::size_t>(row)].heading;
    }

    void ItemList::selectedRowsChanged(int lastRowSelected)
    {
        if (_syncing || !isItem(lastRowSelected))
            return;
        // A row chosen by the person, through the accessibility tree too: the keys go to the list from then on (Return, Shift+F10).
        if (!_list.hasKeyboardFocus(true))
            _list.grabKeyboardFocus();
        if (onClick)
            onClick(lastRowSelected);
    }

    void ItemList::listBoxItemClicked(int row, const juce::MouseEvent& event)
    {
        if (isItem(row) && event.mods.isPopupMenu() && onMenu)
            onMenu(row);
    }

    void ItemList::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
    {
        if (isItem(row) && onDoubleClick)
            onDoubleClick(row);
    }

    void ItemList::returnKeyPressed(int lastRowSelected)
    {
        if (isItem(lastRowSelected) && onDoubleClick)
            onDoubleClick(lastRowSelected);
    }

    bool ItemList::keyPressed(const juce::KeyPress& key, juce::Component*)
    {
        const int row = _list.getSelectedRow();
        // The space bar starts or stops the audition of the selected row. [RQ-APP-083, TASK-APP-028]
        if (key == juce::KeyPress(juce::KeyPress::spaceKey))
        {
            if (isItem(row) && onSpace)
                onSpace(row);
            return true;
        }
        if (key != juce::KeyPress(juce::KeyPress::F10Key, juce::ModifierKeys::shiftModifier, 0))
            return false;
        if (isItem(row) && onMenu)
            onMenu(row);
        return true;
    }

    juce::String ItemList::getTooltipForRow(int row)
    {
        if (row < 0 || row >= getNumRows())
            return {};
        const ItemRow& item = _rows[static_cast<std::size_t>(row)];
        return item.name + (item.detail.isNotEmpty() ? " - " + item.detail : juce::String()) + (item.current ? CURRENT_TOOLTIP : "");
    }

    juce::String ItemList::getNameForRow(int row)
    {
        // The row's text in the accessibility tree, with the same cues as its tooltip. [RQ-APP-005]
        return getTooltipForRow(row);
    }
}
