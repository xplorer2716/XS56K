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
#include "ParameterCell.hpp"
#include "app/EditController.hpp"
#include "app/MemoryController.hpp"
#include "app/Model.hpp"

namespace app::ui
{
    class LookAndFeel;

    /// The edit panel's content: the edited item's name, kind and information with "Read again" above, the tab's groups of parameters drawn as
    /// the mockups draw them, each group in a panel with its title, flowing left to right then wrapping; for a kind with no editor, a message.
    /// The space bar starts or stops the audition of the edited sample, except while a value is typed (the text box takes the key). The tab row
    /// is drawn by the area, which `onTabs` tells. [RQ-APP-048 to RQ-APP-053, RQ-APP-083, RQ-APP-086, RQ-APP-087, ADR-APP-004 (DEC-APP-025,
    /// DEC-APP-027), ADR-APP-001 (DEC-APP-001), TASK-APP-039]
    class EditView final : public juce::Component, private ModelListener
    {
    public:
        EditView(LookAndFeel& lookAndFeel, Model& model, EditController& editor, MemoryController& memory);
        ~EditView() override;

        /// The titles of the tabs and the one shown, for the area's tab row.
        std::function<void(const juce::StringArray& titles, int shown)> onTabs;

        void paint(juce::Graphics& g) override;
        void resized() override;
        bool keyPressed(const juce::KeyPress& key) override;
        void mouseDown(const juce::MouseEvent& event) override;

    private:
        struct Group
        {
            const layouts::LayoutGroup* layout = nullptr;
            std::vector<std::unique_ptr<ParameterCell>> cells;
            juce::Rectangle<int> bounds;
        };

        void modelChanged(ModelPart part) override;
        void rebuild();
        void update();
        [[nodiscard]] static juce::Point<int> sizeOf(const layouts::LayoutGroup& group);

        LookAndFeel& _lookAndFeel;
        Model& _model;
        EditController& _editor;
        MemoryController& _memory;
        CutLabel _name;
        CutLabel _kind;
        CutLabel _info;
        CutLabel _message;
        juce::TextButton _readAgain;
        std::vector<Group> _groups;
        std::optional<MemoryItem> _item;
        const layouts::EditorLayout* _layout = nullptr;
    };
}
