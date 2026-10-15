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
#include <vector>

namespace app::ui
{
    class LookAndFeel;

    /// The icon of a row: a disk, a folder, or the kind of a file or of an item of the memory, each with its own shape. [RQ-APP-026,
    /// RQ-APP-005]
    enum class RowIcon
    {
        None,
        Disk,
        Folder,
        Program,
        Sample,
        Multi,
        SongFile,
        SetList,
        SceneList,
        Unknown,
    };

    /// One row of a list: a heading (the label of a group of rows), or an item with its icon, its name, a detail on the right, its depth in
    /// a tree, and whether it is the sampler's current one and the selected one. [RQ-APP-025, RQ-APP-035, RQ-APP-037]
    struct ItemRow
    {
        RowIcon icon = RowIcon::None;
        juce::String name;
        juce::String detail;
        int depth = 0;
        bool heading = false;
        bool current = false;
        bool selected = false;
    };

    /// The list of rows of the browser and of the memory view, drawn as the design system's rows: the selected row on the raised surface with
    /// the accent bar at its left, the current one with a dot at its right and "current" in its tooltip, so that no state is told by colour
    /// alone. It reports a row selected (by the mouse, the keyboard or the accessibility tree), a row opened (double click or Return) and
    /// the context menu's requests (right click, or Shift+F10 on the selected row) and the space bar, by row. [RQ-APP-001, RQ-APP-004, RQ-APP-005,
    /// ADR-APP-002 (DEC-APP-013, DEC-APP-014), TASK-APP-018]
    class ItemList final : public juce::Component, private juce::ListBoxModel, private juce::KeyListener
    {
    public:
        explicit ItemList(LookAndFeel& lookAndFeel);

        void setRows(std::vector<ItemRow> rows);
        [[nodiscard]] const std::vector<ItemRow>& rows() const { return _rows; }

        std::function<void(int row)> onClick;
        std::function<void(int row)> onDoubleClick;
        std::function<void(int row)> onMenu;
        /// The space bar on the selected row. [RQ-APP-083, TASK-APP-028]
        std::function<void(int row)> onSpace;

        void resized() override;

    private:
        int getNumRows() override;
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent& event) override;
        void listBoxItemDoubleClicked(int row, const juce::MouseEvent& event) override;
        void selectedRowsChanged(int lastRowSelected) override;
        void returnKeyPressed(int lastRowSelected) override;
        using juce::Component::keyPressed;  // the component's own, which the key listener's would hide
        bool keyPressed(const juce::KeyPress& key, juce::Component* origin) override;
        [[nodiscard]] bool isItem(int row) const;
        juce::String getTooltipForRow(int row) override;
        juce::String getNameForRow(int row) override;

        void paintIcon(juce::Graphics& g, RowIcon icon, juce::Rectangle<float> area) const;

        LookAndFeel& _lookAndFeel;
        std::vector<ItemRow> _rows;
        juce::ListBox _list;
        /// True while the list's selection is set from the rows: that is no person's choice.
        bool _syncing = false;
    };
}
