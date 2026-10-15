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
#include "app/FileBrowser.hpp"
#include "app/Model.hpp"

namespace app::ui
{
    class LookAndFeel;

    /// The file browser: the disks as the roots of a tree, the current disk's folders and files listed so far under it, each kind with its icon,
    /// the sampler's current folder marked and its path always shown, the selected file's name, kind and size (or the current disk's type,
    /// format and free space), and a context menu of the commands the core says apply. It draws the model, observes its changes and calls the
    /// browser's controller; it holds no state of the sampler. [RQ-APP-024 to RQ-APP-034, RQ-APP-001, RQ-APP-004, RQ-APP-005,
    /// ADR-APP-001 (DEC-APP-001), ADR-APP-002 (DEC-APP-013, DEC-APP-014, DEC-APP-016), TASK-APP-018]
    class BrowserView final : public juce::Component, private ModelListener
    {
    public:
        BrowserView(LookAndFeel& lookAndFeel, Model& model, FileBrowser& browser);
        ~BrowserView() override;

        void resized() override;

    private:
        /// What a row stands for: a disk by its handle, or a folder or a file.
        struct Entry
        {
            BrowserItem item;
            std::optional<int> diskHandle;
        };

        void modelChanged(ModelPart part) override;
        void rebuild();
        void addFolder(const BrowserState& state, const std::vector<std::string>& folder, int depth, std::vector<ItemRow>& rows);
        void clicked(int row);
        void doubleClicked(int row);
        void showMenu(int row);
        void run(int command, const Entry& entry);

        LookAndFeel& _lookAndFeel;
        Model& _model;
        FileBrowser& _browser;
        ItemList _list;
        std::vector<Entry> _entries;
        juce::Label _path;
        juce::Label _details;
    };
}
