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
#include <memory>
#include <optional>
#include <vector>

#include "CutLabel.hpp"
#include "EnvelopeGraph.hpp"
#include "ParameterCell.hpp"
#include "app/EditController.hpp"
#include "app/MemoryController.hpp"
#include "app/Model.hpp"

namespace app::ui
{
    class LookAndFeel;

    /// The edit panel's content: the edited item's name and kind, with its information or a program's keygroup selector, and "Read again"
    /// above; the tab's groups of parameters drawn as the mockups draw them, each group in a panel with its title and its scope, an envelope
    /// group with its graph above its knobs, a zone group as a grid of a column per zone, flowing left to right then wrapping (or starting a
    /// row where the layout says), in a pane that scrolls when the panel is too short; for a kind with no editor, a message. The space bar
    /// starts or stops the audition of the edited sample, except while a value is typed (the text box takes the key). The tab row is drawn by
    /// the area, which `onTabs` tells, and a tab chosen there comes back through `showTab`. [RQ-APP-048 to RQ-APP-053, RQ-APP-056, RQ-APP-058,
    /// RQ-APP-060, RQ-APP-061, RQ-APP-083, RQ-APP-086, RQ-APP-087, RQ-APP-094, RQ-APP-095, ADR-APP-004 (DEC-APP-025, DEC-APP-027, DEC-APP-031,
    /// DEC-APP-033, DEC-APP-034), ADR-APP-001 (DEC-APP-001), TASK-APP-039, TASK-APP-044, TASK-APP-046, TASK-APP-047]
    class EditView final : public juce::Component, private ModelListener
    {
    public:
        EditView(LookAndFeel& lookAndFeel, Model& model, EditController& editor, MemoryController& memory);
        ~EditView() override;

        /// The titles of the tabs and the one shown, for the area's tab row.
        std::function<void(const juce::StringArray& titles, int shown)> onTabs;
        /// The person chose the tab at `index` in the area's tab row.
        void showTab(int index);

        void resized() override;
        bool keyPressed(const juce::KeyPress& key) override;
        void mouseDown(const juce::MouseEvent& event) override;

    private:
        struct Group
        {
            const layouts::LayoutGroup* layout = nullptr;
            std::vector<std::unique_ptr<ParameterCell>> cells;
            std::unique_ptr<EnvelopeGraph> graph;
            juce::Rectangle<int> bounds;
        };

        /// The groups' panels, scrolled by the viewport; their cells and graphs are its children.
        class Pane final : public juce::Component
        {
        public:
            std::function<void(juce::Graphics&)> painter;
            void paint(juce::Graphics& g) override { painter(g); }
        };

        void modelChanged(ModelPart part) override;
        void rebuild();
        void update();
        void layoutGroups();
        /// The group's cells, numbered from `index` on in the edit state's order.
        void addCells(Group& group, std::size_t& index);
        void paintGroups(juce::Graphics& g);
        /// A zone grid's cells, a column per zone and a row per parameter, and its zone numbers and row labels. [DEC-APP-033]
        void layoutGrid(Group& group, int left, int top) const;
        void paintGridLabels(juce::Graphics& g, const Group& group) const;
        /// A grid column's head: its zone number, or its title with its scope above it on the group's title line. [DEC-APP-035]
        void paintGridHead(juce::Graphics& g, const Group& group, int column, int x, int top) const;
        [[nodiscard]] static juce::Point<int> sizeOf(const layouts::LayoutGroup& group);

        LookAndFeel& _lookAndFeel;
        Model& _model;
        EditController& _editor;
        MemoryController& _memory;
        CutLabel _name;
        CutLabel _kind;
        CutLabel _info;
        CutLabel _message;
        ChoiceBox _keygroup;
        juce::TextButton _addKeygroups;    ///< a program's keygroups added, how many asked [RQ-APP-061]
        juce::TextButton _deleteKeygroup;  ///< the keygroup shown deleted after a confirmation; disabled for the last one
        juce::TextButton _readAgain;
        juce::Viewport _viewport;
        Pane _pane;
        std::vector<Group> _groups;
        std::optional<MemoryItem> _item;
        const layouts::EditorLayout* _layout = nullptr;
        std::size_t _tab = 0;
        int _keygroupItems = 0;  ///< the keygroup count the selector holds
    };
}
