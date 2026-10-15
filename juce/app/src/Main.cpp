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
// The editor application: the JUCE adapters of the core's interfaces, the core (model, controller, browser and memory view controllers) and
// the main window. Quitting closes the session with the sampler first, without blocking the message thread.
// [RQ-APP-006, RQ-APP-007, RQ-APP-021, RQ-BLD-007, ADR-APP-001 (DEC-APP-001, DEC-APP-008, DEC-APP-010), TASK-APP-017]
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

#include "JuceAdapters.hpp"
#include "LookAndFeel.hpp"
#include "MainComponent.hpp"
#include "app/Controller.hpp"
#include "app/DesignTokens.generated.hpp"
#include "app/FileBrowser.hpp"
#include "app/MemoryController.hpp"
#include "app/Model.hpp"
#include "common/midi/JuceMidiBackend.hpp"

namespace
{
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow(const juce::String& name, std::unique_ptr<juce::Component> content)
            : DocumentWindow(name, app::ui::colourOf(app::tokens::kColourSurfaceBase), DocumentWindow::allButtons)
        {
            using namespace app::tokens;
            setUsingNativeTitleBar(true);
            setContentOwned(content.release(), true);
            setResizable(true, false);
            getConstrainer()->setMinimumSize(app::ui::pixels(kSizeWindowMinWidth), app::ui::pixels(kSizeWindowMinHeight));
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    /// What the application is made of, in the order it is built; destroyed in the reverse order, the window first and the MIDI backend
    /// last, after the controller that reaches the sampler through it.
    struct Application
    {
        Application()
        {
            juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);
            window = std::make_unique<MainWindow>(
                JUCE_APPLICATION_NAME_STRING, std::make_unique<app::ui::MainComponent>(lookAndFeel, model, controller, browser, settings, backend));
        }

        ~Application()
        {
            window = nullptr;
            juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
        }

        app::ui::LookAndFeel lookAndFeel;
        common::midi::JuceMidiBackend backend;
        app::ui::PropertiesSettings settings;
        app::ui::MessageThreadDispatcher dispatcher;
        app::ui::DialogPrompts prompts{lookAndFeel};
        app::Model model;
        app::Controller controller{model, dispatcher, prompts, settings, backend};
        app::FileBrowser browser{model, controller};
        app::MemoryController memory{model, controller};
        juce::TooltipWindow tooltips;
        std::unique_ptr<MainWindow> window;
    };

    class XS56KApplication final : public juce::JUCEApplication
    {
    public:
        const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
        const juce::String getApplicationVersion() override { return VERSION_FULL_STRING; }
        bool moreThanOneInstanceAllowed() override { return true; }

        void initialise(const juce::String&) override { _application = std::make_unique<Application>(); }
        void shutdown() override { _application = nullptr; }

        /// The session is closed and the ports released on the sampler worker; the application quits when that is done, the status bar
        /// saying what it waits for. A second request while it waits changes nothing. [RQ-APP-021]
        void systemRequestedQuit() override
        {
            if (_application == nullptr)
            {
                quit();
                return;
            }
            if (_quitting)
                return;
            _quitting = true;
            _application->controller.quit([] { juce::JUCEApplication::quit(); });
        }

    private:
        std::unique_ptr<Application> _application;
        bool _quitting = false;
    };
}

START_JUCE_APPLICATION(XS56KApplication)
