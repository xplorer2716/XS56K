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

#include <optional>
#include <vector>

#include "ItemList.hpp"
#include "app/MemoryController.hpp"
#include "app/Model.hpp"

namespace app::ui
{
    class LookAndFeel;

    /// The memory view: the six headings with their counts, the items of each kind in the sampler's order with the current one of each kind
    /// marked, the selection, a context menu (create a program, rename, delete, audition a sample), the free memory and a refresh button. It
    /// draws the model, observes its changes and calls the memory view's controller; it holds no state of the sampler. [RQ-APP-035 to
    /// RQ-APP-042, RQ-APP-001, RQ-APP-004, RQ-APP-005, ADR-APP-001 (DEC-APP-001), ADR-APP-002 (DEC-APP-013, DEC-APP-014, DEC-APP-016),
    /// TASK-APP-019]
    class MemoryView final : public juce::Component, private ModelListener
    {
    public:
        MemoryView(LookAndFeel& lookAndFeel, Model& model, MemoryController& memory);
        ~MemoryView() override;

        void resized() override;

    private:
        void modelChanged(ModelPart part) override;
        void rebuild();
        void showMenu(int row);
        void run(int command, const MemoryItem& item);
        void askNewProgram();

        LookAndFeel& _lookAndFeel;
        Model& _model;
        MemoryController& _memory;
        ItemList _list;
        juce::TextButton _refresh;
        /// The item of each row; none for a heading.
        std::vector<std::optional<MemoryItem>> _items;
        juce::Label _free;
    };
}
