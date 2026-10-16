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

#include <array>

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
        // The items of a stretchable layout: an area, the bar in the gap, the other area. [RQ-APP-008, TASK-APP-031]
        constexpr int FIRST_ITEM = 0;
        constexpr int BAR_ITEM = 1;
        constexpr int SECOND_ITEM = 2;
        constexpr int ITEMS = 3;
        constexpr double WHOLE = -1.0;  // a stretchable layout's proportion: all the room
        constexpr double HALF = -0.5;

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
        : _lookAndFeel(lookAndFeel), _withTabRow(withTabRow)
    {
        using namespace tokens;
        // The title and the hint are labels whose whole text is their tooltip when they are cut short. [RQ-APP-080, TASK-APP-022]
        _title.setText(title, juce::dontSendNotification);
        _title.setFont(_lookAndFeel.font(kFontTitleSize, true));
        _title.setColour(juce::Label::textColourId, colourOf(kColourTextSecondary));
        _title.setBorderSize({});
        _hint.setFont(_lookAndFeel.font(kFontValueSize));
        _hint.setColour(juce::Label::textColourId, colourOf(kColourTextDisabled));
        _hint.setJustificationType(juce::Justification::centred);
        _hint.setBorderSize({});
        addAndMakeVisible(_title);
        addChildComponent(_hint);
    }

    void AreaPanel::setHint(const juce::String& hint)
    {
        if (_content != nullptr)
            _content->setVisible(hint.isEmpty());
        _hint.setText(hint, juce::dontSendNotification);
        _hint.setVisible(hint.isNotEmpty());
    }

    void AreaPanel::setContent(juce::Component* content)
    {
        _content = content;
        addChildComponent(content);
        content->setVisible(_hint.getText().isEmpty());
        resized();
    }

    void AreaPanel::setTabs(const juce::StringArray& titles, int shown)
    {
        _tabs = titles;
        _shownTab = shown;
        repaint();
    }

    std::vector<juce::Rectangle<int>> AreaPanel::tabBounds() const
    {
        using namespace tokens;
        std::vector<juce::Rectangle<int>> bounds;
        if (!_withTabRow)
            return bounds;
        auto content = getLocalBounds().reduced(pixels(kSpaceInside));
        const auto tabRow = content.removeFromBottom(pixels(kTabHeight));
        const juce::Font font = _lookAndFeel.font(kFontLabelSize, true);
        int x = tabRow.getX();
        for (const juce::String& title : _tabs)
        {
            const int width = juce::roundToInt(juce::GlyphArrangement::getStringWidth(font, title.toUpperCase())) + 2 * pixels(kSpaceGroup);
            bounds.emplace_back(x, tabRow.getY(), width, tabRow.getHeight());
            x += width;
        }
        return bounds;
    }

    void AreaPanel::mouseDown(const juce::MouseEvent& event)
    {
        const auto bounds = tabBounds();
        for (std::size_t i = 0; i < bounds.size(); ++i)
        {
            if (bounds[i].contains(event.getPosition()) && onTabChosen)
                onTabChosen(static_cast<int>(i));  // [RQ-APP-049, TASK-APP-044]
        }
    }

    void AreaPanel::resized()
    {
        using namespace tokens;
        auto content = getLocalBounds().reduced(pixels(kSpaceInside));
        _title.setBounds(content.removeFromTop(pixels(kRowHeight)));
        if (_withTabRow)
            content.removeFromBottom(pixels(kTabHeight));
        _hint.setBounds(content);
        if (_content != nullptr)
            _content->setBounds(content);
    }

    void AreaPanel::paint(juce::Graphics& g)
    {
        using namespace tokens;
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(colourOf(kColourSurfacePanel));
        g.fillRoundedRectangle(bounds, kRadiusPanel);

        auto content = getLocalBounds().reduced(pixels(kSpaceInside));
        content.removeFromTop(pixels(kRowHeight));

        if (_withTabRow)
        {
            // The row of tabs along the bottom edge of the edit panel (RQ-APP-049): empty until the edit panel has an editor.
            const auto tabRow = content.removeFromBottom(pixels(kTabHeight));
            g.setColour(colourOf(kColourBorderDefault));
            g.fillRect(tabRow.withHeight(pixels(kStrokeHairline)));
            // The tabs of the editor shown, the one shown underlined in the accent and in the primary text colour. [RQ-APP-049, TASK-APP-039]
            g.setFont(_lookAndFeel.font(kFontLabelSize, true));
            const auto tabs = tabBounds();
            for (int i = 0; i < _tabs.size(); ++i)
            {
                const auto& tab = tabs[static_cast<std::size_t>(i)];
                const bool shown = i == _shownTab;
                g.setColour(colourOf(shown ? kTabTextSelected : kTabText));
                g.drawText(_tabs[i].toUpperCase(), tab, juce::Justification::centred);
                if (shown)
                {
                    g.setColour(colourOf(kTabUnderlineSelected));
                    g.fillRect(tab.withHeight(pixels(kStrokeFocus)));
                }
            }
        }
    }

    MainComponent::MainComponent(LookAndFeel& lookAndFeel, Model& model, Controller& controller, FileBrowser& browser,
                                 MemoryController& memory, EditController& editor, AppSettings& settings, common::midi::MidiBackend& backend)
        : _lookAndFeel(lookAndFeel), _model(model), _controller(controller), _settings(settings), _backend(backend), _menuBar(this),
          _browser(lookAndFeel, BROWSER_TITLE, false), _browserView(lookAndFeel, model, browser), _memory(lookAndFeel, MEMORY_TITLE, false), _memoryView(lookAndFeel, model, memory), _editor(lookAndFeel, EDITOR_TITLE, true),
          _editView(lookAndFeel, model, editor, memory),
          _status(lookAndFeel), _columnBar(&_columns, BAR_ITEM, true), _rowBar(&_rows, BAR_ITEM, false)
    {
        for (juce::Component* component :
             std::initializer_list<juce::Component*>{&_menuBar, &_browser, &_memory, &_editor, &_status, &_columnBar, &_rowBar})
            addAndMakeVisible(*component);
        // The minimums are the design system's; the first layout is RQ-APP-006's: the left column at its minimum width, the browser and
        // the memory view at half its height each. [RQ-APP-008, ADR-APP-002 (DEC-APP-022), TASK-APP-031]
        using namespace tokens;
        const double edge = pixels(kSpaceEdge);
        _columns.setItemLayout(FIRST_ITEM, pixels(kSizeLeftMinWidth), WHOLE, pixels(kSizeLeftMinWidth));
        _columns.setItemLayout(BAR_ITEM, edge, edge, edge);
        _columns.setItemLayout(SECOND_ITEM, pixels(kSizeEditMinWidth), WHOLE, WHOLE);
        _rows.setItemLayout(FIRST_ITEM, pixels(kSizeAreaMinHeight), WHOLE, HALF);
        _rows.setItemLayout(BAR_ITEM, edge, edge, edge);
        _rows.setItemLayout(SECOND_ITEM, pixels(kSizeAreaMinHeight), WHOLE, HALF);
        _browser.setContent(&_browserView);
        _memory.setContent(&_memoryView);
        _editView.onTabs = [this](const juce::StringArray& titles, int shown) { _editor.setTabs(titles, shown); };  // [RQ-APP-049]
        _editor.onTabChosen = [this](int index) { _editView.showTab(index); };
        _editor.setContent(&_editView);
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

        // Three areas: the left column (browser over memory view) and the edit panel over the full height, the gaps between them dragged.
        // [RQ-APP-006, RQ-APP-008, TASK-APP-031]
        const int edge = pixels(kSpaceEdge);
        area.reduce(edge, edge);
        std::array<juce::Component*, ITEMS> columns{nullptr, &_columnBar, &_editor};  // the left column is laid out below
        _columns.layOutComponents(columns.data(), ITEMS, area.getX(), area.getY(), area.getWidth(), area.getHeight(), false, true);
        const auto left = area.withWidth(_columns.getItemCurrentAbsoluteSize(FIRST_ITEM));
        std::array<juce::Component*, ITEMS> rows{&_browser, &_rowBar, &_memory};
        _rows.layOutComponents(rows.data(), ITEMS, left.getX(), left.getY(), left.getWidth(), left.getHeight(), true, true);
    }

    void MainComponent::modelChanged(ModelPart part)
    {
        // The edit panel shows its editor while an item is edited, the hint otherwise. [RQ-APP-048, TASK-APP-039]
        if (part == ModelPart::Edit && _model.connection().state == ConnectionState::Connected)
            _editor.setHint(_model.edit().item ? juce::String() : juce::String(EDITOR_HINT));
        if (part != ModelPart::Connection && part != ModelPart::Busy && part != ModelPart::Problem)
            return;
        _status.show(_model.connection(), _model.busy(), _model.problem());
        if (part == ModelPart::Connection)
        {
            const bool connected = _model.connection().state == ConnectionState::Connected;
            _browser.setHint(connected ? juce::String() : juce::String(NOT_CONNECTED_HINT));
            _memory.setHint(connected ? juce::String() : juce::String(NOT_CONNECTED_HINT));
            _editor.setHint(connected ? (_model.edit().item ? juce::String() : juce::String(EDITOR_HINT)) : juce::String(NOT_CONNECTED_HINT));
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
