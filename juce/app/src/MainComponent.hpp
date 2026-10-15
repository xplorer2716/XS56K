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

#include "BrowserView.hpp"
#include "CutLabel.hpp"
#include "MemoryView.hpp"
#include "StatusBar.hpp"
#include "app/AppSettings.hpp"
#include "app/Controller.hpp"
#include "app/FileBrowser.hpp"
#include "app/MemoryController.hpp"
#include "app/Model.hpp"

namespace app::ui
{
    class LookAndFeel;

    /// One area of the main window: a panel with its title, and the hint shown while it has nothing to show. The browser, the memory view
    /// and the edit panel are drawn into these areas by their own components (TASK-APP-018, TASK-APP-019, FTR-APP-007). [RQ-APP-006]
    class AreaPanel final : public juce::Component
    {
    public:
        AreaPanel(LookAndFeel& lookAndFeel, juce::String title, bool withTabRow);

        void setHint(const juce::String& hint);
        /// The component drawn under the title, shown only while it has something to show (no hint). [TASK-APP-018]
        void setContent(juce::Component* content);
        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        LookAndFeel& _lookAndFeel;
        CutLabel _title;
        CutLabel _hint;
        bool _withTabRow;
        juce::Component* _content = nullptr;
    };

    /// The content of the main window: the menu bar, the file browser at the top left, the memory view under it, the edit panel on the right
    /// with its row of tabs along its bottom edge, and the status bar. It draws the model and calls the controller; it holds no state of the
    /// sampler. [RQ-APP-006, RQ-APP-007, RQ-APP-012, RQ-APP-015, RQ-APP-019, ADR-APP-001 (DEC-APP-001), TASK-APP-017]
    class MainComponent final : public juce::Component, public juce::MenuBarModel, private ModelListener
    {
    public:
        MainComponent(LookAndFeel& lookAndFeel, Model& model, Controller& controller, FileBrowser& browser, MemoryController& memory,
                      AppSettings& settings, common::midi::MidiBackend& backend);
        ~MainComponent() override;

        void resized() override;

        juce::StringArray getMenuBarNames() override;
        juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& menuName) override;
        void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    private:
        void modelChanged(ModelPart part) override;
        void openMidiSetup(bool connectAfter);
        void connect();

        LookAndFeel& _lookAndFeel;
        Model& _model;
        Controller& _controller;
        AppSettings& _settings;
        common::midi::MidiBackend& _backend;
        juce::MenuBarComponent _menuBar;
        AreaPanel _browser;
        BrowserView _browserView;
        AreaPanel _memory;
        MemoryView _memoryView;
        AreaPanel _editor;
        StatusBar _status;
        /// The left column and the edit panel, then the browser and the memory view, each pair with the bar in its gap that the person
        /// drags. [RQ-APP-008, ADR-APP-002 (DEC-APP-022), TASK-APP-031]
        juce::StretchableLayoutManager _columns;
        juce::StretchableLayoutManager _rows;
        juce::StretchableLayoutResizerBar _columnBar;
        juce::StretchableLayoutResizerBar _rowBar;
    };
}
