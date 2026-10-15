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
#include "MainComponent.hpp"

#include "LookAndFeel.hpp"
#include "MidiSetupDialog.hpp"
#include "app/DesignTokens.generated.hpp"
#include "app/MidiSetup.hpp"

namespace app::ui
{
    // [RQ-APP-006, RQ-APP-007, RQ-APP-012, RQ-APP-015, RQ-APP-019, ADR-APP-001 (DEC-APP-001), ADR-APP-002 (DEC-APP-013), TASK-APP-017]
    namespace
    {
        constexpr const char* BROWSER_TITLE = "Disks";
        constexpr const char* MEMORY_TITLE = "Memory";
        constexpr const char* EDITOR_TITLE = "Editor";
        constexpr const char* NOT_CONNECTED_HINT = "Not connected: choose Sampler > Connect.";
        constexpr const char* EDITOR_HINT = "Select an item of the memory to edit it.";

        enum MenuIndex
        {
            APPLICATION_MENU,
            SAMPLER_MENU,
            MIDI_MENU,
        };

        enum MenuItem
        {
            QUIT_ITEM = 1,
            CONNECT_ITEM,
            DISCONNECT_ITEM,
            MIDI_SETUP_ITEM,
        };

        const juce::StringArray MENU_NAMES{"XS56K", "Sampler", "MIDI"};
    }

    AreaPanel::AreaPanel(LookAndFeel& lookAndFeel, juce::String title, bool withTabRow)
        : _lookAndFeel(lookAndFeel), _title(std::move(title)), _withTabRow(withTabRow)
    {
    }

    void AreaPanel::setHint(const juce::String& hint)
    {
        if (_content != nullptr)
            _content->setVisible(hint.isEmpty());
        if (_hint == hint)
            return;
        _hint = hint;
        repaint();
    }

    void AreaPanel::setContent(juce::Component* content)
    {
        _content = content;
        addChildComponent(content);
        content->setVisible(_hint.isEmpty());
        resized();
    }

    void AreaPanel::resized()
    {
        using namespace tokens;
        if (_content == nullptr)
            return;
        auto content = getLocalBounds().reduced(pixels(kSpaceInside));
        content.removeFromTop(pixels(kRowHeight));
        if (_withTabRow)
            content.removeFromBottom(pixels(kTabHeight));
        _content->setBounds(content);
    }

    void AreaPanel::paint(juce::Graphics& g)
    {
        using namespace tokens;
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(colourOf(kColourSurfacePanel));
        g.fillRoundedRectangle(bounds, kRadiusPanel);

        auto content = getLocalBounds().reduced(pixels(kSpaceInside));
        g.setColour(colourOf(kColourTextSecondary));
        g.setFont(_lookAndFeel.font(kFontTitleSize, true));
        g.drawText(_title, content.removeFromTop(pixels(kRowHeight)), juce::Justification::centredLeft);

        if (_withTabRow)
        {
            // The row of tabs along the bottom edge of the edit panel (RQ-APP-049): empty until the edit panel has an editor.
            const auto tabRow = content.removeFromBottom(pixels(kTabHeight));
            g.setColour(colourOf(kColourBorderDefault));
            g.fillRect(tabRow.withHeight(pixels(kStrokeHairline)));
        }

        if (_hint.isNotEmpty())
        {
            g.setColour(colourOf(kColourTextDisabled));
            g.setFont(_lookAndFeel.font(kFontValueSize));
            g.drawFittedText(_hint, content, juce::Justification::centred, 2);
        }
    }

    MainComponent::MainComponent(LookAndFeel& lookAndFeel, Model& model, Controller& controller, FileBrowser& browser, AppSettings& settings,
                                 common::midi::MidiBackend& backend)
        : _lookAndFeel(lookAndFeel), _model(model), _controller(controller), _settings(settings), _backend(backend), _menuBar(this),
          _browser(lookAndFeel, BROWSER_TITLE, false), _browserView(lookAndFeel, model, browser), _memory(lookAndFeel, MEMORY_TITLE, false), _editor(lookAndFeel, EDITOR_TITLE, true),
          _status(lookAndFeel)
    {
        for (juce::Component* component : std::initializer_list<juce::Component*>{&_menuBar, &_browser, &_memory, &_editor, &_status})
            addAndMakeVisible(*component);
        _browser.setContent(&_browserView);
        _status.onDismiss = [this] { _model.clearProblem(); };
        _model.addListener(*this);
        modelChanged(ModelPart::Connection);
        setSize(pixels(tokens::kSizeWindowMinWidth), pixels(tokens::kSizeWindowMinHeight));
    }

    MainComponent::~MainComponent()
    {
        _model.removeListener(*this);
    }

    void MainComponent::resized()
    {
        using namespace tokens;
        auto area = getLocalBounds();
        _menuBar.setBounds(area.removeFromTop(getLookAndFeel().getDefaultMenuBarHeight()));
        _status.setBounds(area.removeFromBottom(pixels(kStatusHeight)));

        // Three areas: the left column (browser over memory view) and the edit panel over the full height. [RQ-APP-006]
        const int edge = pixels(kSpaceEdge);
        area.reduce(edge, edge);
        auto left = area.removeFromLeft(pixels(kSizeLeftMinWidth));
        area.removeFromLeft(edge);
        _browser.setBounds(left.removeFromTop((left.getHeight() - edge) / 2));
        left.removeFromTop(edge);
        _memory.setBounds(left);
        _editor.setBounds(area);
    }

    void MainComponent::modelChanged(ModelPart part)
    {
        if (part != ModelPart::Connection && part != ModelPart::Busy && part != ModelPart::Problem)
            return;
        _status.show(_model.connection(), _model.busy(), _model.problem());
        if (part == ModelPart::Connection)
        {
            const bool connected = _model.connection().state == ConnectionState::Connected;
            _browser.setHint(connected ? juce::String() : juce::String(NOT_CONNECTED_HINT));
            _memory.setHint(connected ? juce::String() : juce::String(NOT_CONNECTED_HINT));
            _editor.setHint(connected ? juce::String(EDITOR_HINT) : juce::String(NOT_CONNECTED_HINT));
            menuItemsChanged();
        }
    }

    juce::StringArray MainComponent::getMenuBarNames()
    {
        return MENU_NAMES;
    }

    juce::PopupMenu MainComponent::getMenuForIndex(int menuIndex, const juce::String&)
    {
        juce::PopupMenu menu;
        switch (menuIndex)
        {
            case APPLICATION_MENU:
                menu.addItem(QUIT_ITEM, "Quit");
                break;
            case SAMPLER_MENU:
                // "Connect" only while disconnected, "Disconnect" only while connected or connecting. [RQ-APP-007]
                if (_controller.canConnect())
                    menu.addItem(CONNECT_ITEM, "Connect");
                else
                    menu.addItem(DISCONNECT_ITEM, "Disconnect");
                break;
            case MIDI_MENU:
                menu.addItem(MIDI_SETUP_ITEM, "MIDI setup" + ELLIPSIS);
                break;
            default:
                break;
        }
        return menu;
    }

    void MainComponent::menuItemSelected(int menuItemID, int)
    {
        switch (menuItemID)
        {
            case QUIT_ITEM:
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
                break;
            case CONNECT_ITEM:
                connect();
                break;
            case DISCONNECT_ITEM:
                _controller.disconnect();
                break;
            case MIDI_SETUP_ITEM:
                openMidiSetup(false);
                break;
            default:
                break;
        }
    }

    void MainComponent::connect()
    {
        // With no setup stored yet, the MIDI setup comes first. [RQ-APP-015, RQ-APP-017]
        if (const auto setup = loadMidiSetup(_settings))
            _controller.connect(*setup);
        else
            openMidiSetup(true);
    }

    void MainComponent::openMidiSetup(bool connectAfter)
    {
        showMidiSetupDialog(_lookAndFeel, _backend, loadMidiSetup(_settings),
                            [this, connectAfter, alive = juce::Component::SafePointer<MainComponent>(this)](const MidiSetup& setup) {
                                if (alive == nullptr)
                                    return;
                                _controller.applyMidiSetup(setup);
                                if (connectAfter)
                                    _controller.connect(setup);
                            });
    }
}
