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
// Minimal placeholder application entry point — see juce/app/CMakeLists.txt
// for why this exists and what it deliberately is not.
// [RQ-BLD-002, RQ-BLD-007, TASK-BLD-005]
#include <juce_gui_extra/juce_gui_extra.h>

namespace
{
class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow (const juce::String& name)
        : DocumentWindow (name,
                           juce::Desktop::getInstance().getDefaultLookAndFeel()
                               .findColour (juce::ResizableWindow::backgroundColourId),
                           DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new juce::Label ({}, "XS56K -- placeholder window (TASK-BLD-005 stub)"), true);
        centreWithSize (400, 200);
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class XS56KApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return VERSION_FULL_STRING; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String&) override { mainWindow = std::make_unique<MainWindow> (getApplicationName()); }
    void shutdown() override { mainWindow = nullptr; }

private:
    std::unique_ptr<juce::DocumentWindow> mainWindow;
};
} // namespace

START_JUCE_APPLICATION (XS56KApplication)
