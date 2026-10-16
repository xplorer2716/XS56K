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
#include "EditView.hpp"

#include <algorithm>
#include <set>
#include <string>

#include "LookAndFeel.hpp"
#include "app/DesignTokens.generated.hpp"

namespace app::ui
{
    // [RQ-APP-048 to RQ-APP-053, RQ-APP-056, RQ-APP-058, RQ-APP-060, RQ-APP-083, RQ-APP-086, RQ-APP-087, RQ-APP-094, RQ-APP-095, ADR-APP-004
    // (DEC-APP-025, DEC-APP-027, DEC-APP-031, DEC-APP-033), TASK-APP-039, TASK-APP-044, TASK-APP-046]
    namespace
    {
        constexpr const char* READ_AGAIN_TEXT = "Read again";
        constexpr const char* NO_EDITOR = "This kind has no editor yet.";
        constexpr const char* KEYGROUP_TEXT = "Keygroup ";
        constexpr const char* OF_TEXT = " of ";
        constexpr int READ_AGAIN_SHARE = 6;          // the button takes a sixth of the panel's width
        constexpr float KEYGROUP_SELECTOR_KNOBS = 5;  // the keygroup selector is five knob diameters wide, as in the mockups
        constexpr int GRID_LABEL_CELLS = 2;           // a zone grid's row labels take two knob cells, as in the mockups [DEC-APP-033]
        constexpr int GRID_COLUMN_CELLS = 2;          // a zone's column: a choice's width
        constexpr const char* ZONE_TEXT = "Zone ";
        constexpr const char* ADD_TEXT = "Add...";
        constexpr const char* DELETE_TEXT = "Delete";
        constexpr float HEADER_BUTTON_KNOBS = 3;  // a header button is three knob diameters wide, as in the mockups [DEC-APP-034]

        /// The height of a zone grid's row: a value box and the space under it. [DEC-APP-033]
        int gridPitch()
        {
            using namespace tokens;
            return pixels(kValueboxHeight + kSizeSpace2);
        }

        const char* kindText(MemoryKind kind)
        {
            using enum app::MemoryKind;
            switch (kind)
            {
                case Multi:
                    return "Multi";
                case Program:
                    return "Program";
                case Sample:
                    return "Sample";
                case SongFile:
                    return "Song file";
                case SetList:
                    return "Set list";
                case SceneList:
                    break;
            }
            return "Scenelist";
        }

        /// Where a group's parameters live, written after its title. [RQ-APP-056]
        const char* scopeText(layouts::Scope scope)
        {
            using enum app::layouts::Scope;
            switch (scope)
            {
                case Program:
                    return "Program";
                case Keygroup:
                    return "Keygroup";
                case Zone:
                    return "Zone";
                case None:
                    break;
            }
            return "";
        }

        /// The group's colour: its parameters' when they share one family, the secondary text colour otherwise. [RQ-APP-005]
        juce::Colour groupColour(const layouts::LayoutGroup& group)
        {
            std::set<layouts::Role> roles;
            for (const layouts::LayoutParameter& parameter : group.parameters)
                roles.insert(parameter.role);
            return roles.size() == 1 ? roleColour(*roles.begin()) : colourOf(tokens::kColourTextSecondary);
        }
    }

    EditView::EditView(LookAndFeel& lookAndFeel, Model& model, EditController& editor, MemoryController& memory)
        : _lookAndFeel(lookAndFeel), _model(model), _editor(editor), _memory(memory)
    {
        using namespace tokens;
        _name.setFont(_lookAndFeel.font(kFontTitleSize, true));
        _name.setColour(juce::Label::textColourId, colourOf(kHeaderTitle));
        _kind.setFont(_lookAndFeel.font(kFontLabelSize, true));
        _kind.setColour(juce::Label::textColourId, colourOf(kHeaderKind));
        _info.setFont(_lookAndFeel.font(kFontCaptionSize));
        _info.setColour(juce::Label::textColourId, colourOf(kColourTextSecondary));
        _info.setJustificationType(juce::Justification::centredRight);
        _message.setFont(_lookAndFeel.font(kFontValueSize));
        _message.setColour(juce::Label::textColourId, colourOf(kColourTextDisabled));
        _message.setJustificationType(juce::Justification::centred);
        for (CutLabel* label : {&_name, &_kind, &_info, &_message})
        {
            label->setBorderSize({});
            addAndMakeVisible(*label);
        }
        _readAgain.setButtonText(READ_AGAIN_TEXT);
        _readAgain.onClick = [this] { _editor.readAgain(); };  // [RQ-APP-053]
        addAndMakeVisible(_readAgain);
        // The keygroup whose values a program's tab shows, chosen by the person. [RQ-APP-058]
        _keygroup.onChange = [this] {
            if (_keygroup.getSelectedId() > 0)
                _editor.chooseKeygroup(_keygroup.getSelectedId());
        };
        addChildComponent(_keygroup);
        // Adding keygroups and deleting the one shown, beside the selector. [RQ-APP-061, DEC-APP-034]
        _addKeygroups.setButtonText(ADD_TEXT);
        _addKeygroups.onClick = [this] { _editor.addKeygroups(); };
        _deleteKeygroup.setButtonText(DELETE_TEXT);
        _deleteKeygroup.onClick = [this] { _editor.deleteKeygroup(); };
        addChildComponent(_addKeygroups);
        addChildComponent(_deleteKeygroup);
        _pane.painter = [this](juce::Graphics& g) { paintGroups(g); };
        _pane.addMouseListener(this, false);
        _viewport.setViewedComponent(&_pane, false);
        _viewport.setScrollBarsShown(true, false);  // the groups wrap to the width; only the height scrolls
        addAndMakeVisible(_viewport);
        setWantsKeyboardFocus(true);
        _model.addListener(*this);
        rebuild();
    }

    EditView::~EditView()
    {
        _model.removeListener(*this);
    }

    void EditView::showTab(int index)
    {
        if (index >= 0)
            _editor.showTab(static_cast<std::size_t>(index));  // [RQ-APP-049]
    }

    void EditView::modelChanged(ModelPart part)
    {
        if (part != ModelPart::Edit)
            return;
        const EditState& state = _model.edit();
        if (state.item != _item || state.layout != _layout || state.tab != _tab)
            rebuild();
        else
            update();
    }

    void EditView::rebuild()
    {
        const EditState& state = _model.edit();
        _item = state.item;
        _layout = state.layout;
        _tab = state.tab;
        _groups.clear();
        juce::StringArray titles;
        if (_layout != nullptr && _tab < _layout->tabs.size())
        {
            for (const layouts::LayoutTab& tab : _layout->tabs)
                titles.add(juce::String(std::string(tab.title)));
            const auto indexOf = [this](std::string_view name) -> std::optional<std::size_t> {
                const auto& values = _model.edit().values;
                for (std::size_t i = 0; i < values.size(); ++i)
                {
                    if (values[i].definition->name == name)
                        return i;
                }
                return std::nullopt;
            };
            std::size_t index = 0;
            for (const layouts::LayoutGroup& layout : _layout->tabs[_tab].groups)
            {
                Group group;
                group.layout = &layout;
                if (!layout.envelope.empty())
                {
                    group.graph = std::make_unique<EnvelopeGraph>(layout.envelope, indexOf, groupColour(layout));
                    group.graph->onValue = [this](std::size_t at, std::int64_t value) { _editor.set(at, value); };  // [RQ-APP-095]
                    _pane.addAndMakeVisible(*group.graph);
                }
                addCells(group, index);
                _groups.push_back(std::move(group));
            }
        }
        if (onTabs)
            onTabs(titles, static_cast<int>(_tab));
        _name.setText(_item ? juce::String(_item->name) : juce::String(), juce::dontSendNotification);
        _kind.setText(_item ? juce::String(kindText(_item->kind)).toUpperCase() : juce::String(), juce::dontSendNotification);
        _message.setText(_item && _layout == nullptr ? juce::String(NO_EDITOR) : juce::String(), juce::dontSendNotification);
        _readAgain.setVisible(_layout != nullptr);
        _viewport.setViewPosition(0, 0);
        update();
        resized();
        repaint();
    }

    void EditView::addCells(Group& group, std::size_t& index)
    {
        // A zone grid has a cell per parameter and zone, parameter by parameter, as the edit state's values. [DEC-APP-033]
        const layouts::LayoutGroup& layout = *group.layout;
        const int perParameter = std::max(1, layout.zones);
        for (const layouts::LayoutParameter& parameter : layout.parameters)
        {
            for (int zone = 0; zone < perParameter; ++zone)
            {
                auto cell = std::make_unique<ParameterCell>(_lookAndFeel, parameter, layout.zones > 0);
                cell->onValue = [this, at = index](std::int64_t value) { _editor.set(at, value); };  // [RQ-APP-052]
                _pane.addAndMakeVisible(*cell);
                group.cells.push_back(std::move(cell));
                ++index;
            }
        }
    }

    void EditView::update()
    {
        const EditState& state = _model.edit();
        // A program shows its keygroup selector where a sample shows its information. [RQ-APP-058, RQ-APP-087]
        const int keygroups = state.keygroups.value_or(0);
        if (keygroups != _keygroupItems)
        {
            _keygroup.clear(juce::dontSendNotification);
            for (int keygroup = 1; keygroup <= keygroups; ++keygroup)
                _keygroup.addItem(KEYGROUP_TEXT + juce::String(keygroup) + OF_TEXT + juce::String(keygroups), keygroup);
            _keygroupItems = keygroups;
        }
        _keygroup.setSelectedId(keygroups > 0 ? state.keygroup : 0, juce::dontSendNotification);
        _keygroup.setVisible(keygroups > 0 && _layout != nullptr);
        _addKeygroups.setVisible(_keygroup.isVisible());
        _deleteKeygroup.setVisible(_keygroup.isVisible());
        _deleteKeygroup.setEnabled(keygroups > 1);  // a program keeps one keygroup [RQ-APP-061]
        _info.setText(keygroups > 0 ? juce::String() : juce::String(infoText(state)), juce::dontSendNotification);
        std::size_t index = 0;
        for (Group& group : _groups)
        {
            if (group.graph)
                group.graph->show(state.values);
            for (auto& cell : group.cells)
            {
                if (index < state.values.size())
                {
                    if (state.values[index].layout->control == layouts::Control::Sample)
                        cell->setSamples(state.samples);  // [RQ-APP-060]
                    cell->show(state.values[index]);
                }
                ++index;
            }
        }
    }

    juce::Point<int> EditView::sizeOf(const layouts::LayoutGroup& group)
    {
        using namespace tokens;
        int cells = 0;
        for (const layouts::LayoutParameter& parameter : group.parameters)
            cells += ParameterCell::cells(parameter);
        const int inside = pixels(kSpaceInside);
        if (group.zones > 0)
        {
            // A zone grid: a head row of zone numbers, then a row per parameter. [DEC-APP-033]
            const int rows = static_cast<int>(group.parameters.size()) + 1;
            return {(GRID_LABEL_CELLS + group.zones * GRID_COLUMN_CELLS) * ParameterCell::cellWidth() + 2 * inside,
                    inside + pixels(kFontLabelSize) + pixels(kSpaceBetween) + rows * gridPitch() + inside};
        }
        const int graph = group.envelope.empty() ? 0 : pixels(kEnvelopeHeight) + pixels(kSpaceBetween);  // [RQ-APP-095]
        return {cells * ParameterCell::cellWidth() + 2 * inside,
                inside + pixels(kFontLabelSize) + pixels(kSpaceBetween) + graph + ParameterCell::cellHeight() + inside};
    }

    void EditView::resized()
    {
        using namespace tokens;
        auto area = getLocalBounds();
        auto header = area.removeFromTop(pixels(kSizeControlLarge));
        _readAgain.setBounds(header.removeFromRight(header.getWidth() / READ_AGAIN_SHARE));
        header.removeFromRight(pixels(kSpaceGroup));
        const int nameWidth = juce::roundToInt(juce::GlyphArrangement::getStringWidth(_name.getFont(), _name.getText())) + pixels(kSpaceGroup);
        _name.setBounds(header.removeFromLeft(nameWidth));
        const int kindWidth = juce::roundToInt(juce::GlyphArrangement::getStringWidth(_kind.getFont(), _kind.getText())) + pixels(kSpaceGroup);
        _kind.setBounds(header.removeFromLeft(kindWidth));
        _keygroup.setBounds(header.removeFromRight(pixels(KEYGROUP_SELECTOR_KNOBS * kKnobSizeDefault))
                                .withSizeKeepingCentre(pixels(KEYGROUP_SELECTOR_KNOBS * kKnobSizeDefault), pixels(kSizeControlMedium)));
        const int buttonWidth = pixels(HEADER_BUTTON_KNOBS * kKnobSizeDefault);
        for (juce::TextButton* button : {&_deleteKeygroup, &_addKeygroups})
        {
            header.removeFromRight(pixels(kSpaceInside));
            button->setBounds(header.removeFromRight(buttonWidth).withSizeKeepingCentre(buttonWidth, pixels(kSizeControlMedium)));
        }
        _info.setBounds(header);
        area.removeFromTop(pixels(kSpaceGroup));
        _message.setBounds(area);
        _viewport.setBounds(area);
        layoutGroups();
    }

    void EditView::layoutGroups()
    {
        using namespace tokens;
        // The groups flow left to right, then wrap, as in the mockups; a group may start its own row. [RQ-APP-086, RQ-APP-094, DEC-APP-031]
        const int width = _viewport.getWidth() - _viewport.getScrollBarThickness();
        const int gap = pixels(kSpaceGroup);
        const int inside = pixels(kSpaceInside);
        int x = 0;
        int y = 0;
        int rowHeight = 0;
        int bottom = 0;
        for (Group& group : _groups)
        {
            const auto size = sizeOf(*group.layout);
            if (x > 0 && (x + size.x > width || group.layout->newRow))
            {
                x = 0;
                y += rowHeight + gap;
                rowHeight = 0;
            }
            group.bounds = {x, y, size.x, size.y};
            int top = y + inside + pixels(kFontLabelSize) + pixels(kSpaceBetween);
            if (group.graph)
            {
                group.graph->setBounds(x + inside, top, size.x - 2 * inside, pixels(kEnvelopeHeight));
                top += pixels(kEnvelopeHeight) + pixels(kSpaceBetween);
            }
            if (group.layout->zones > 0)
                layoutGrid(group, x + inside, top);
            int cellX = x + inside;
            for (std::size_t i = 0; group.layout->zones == 0 && i < group.cells.size(); ++i)
            {
                const int cellWidth = ParameterCell::cells(group.layout->parameters[i]) * ParameterCell::cellWidth();
                group.cells[i]->setBounds(cellX, top, cellWidth, ParameterCell::cellHeight());
                cellX += cellWidth;
            }
            x += size.x + gap;
            rowHeight = std::max(rowHeight, size.y);
            bottom = std::max(bottom, y + size.y);
        }
        _pane.setSize(std::max(0, width), bottom);
    }

    void EditView::paintGroups(juce::Graphics& g)
    {
        using namespace tokens;
        const int inside = pixels(kSpaceInside);
        for (const Group& group : _groups)
        {
            g.setColour(colourOf(kColourSurfaceRaised));
            g.fillRoundedRectangle(group.bounds.toFloat(), kRadiusPanel);
            const auto title = juce::Rectangle<int>(group.bounds.getX() + inside, group.bounds.getY() + inside, group.bounds.getWidth() - 2 * inside,
                                                    pixels(kFontLabelSize));
            g.setColour(groupColour(*group.layout));
            g.fillRect(title.withWidth(pixels(kStrokeArc)));
            g.setColour(colourOf(kColourTextSecondary));
            g.setFont(_lookAndFeel.font(kFontLabelSize, true));
            g.drawText(juce::String(std::string(group.layout->title)).toUpperCase(), title.withTrimmedLeft(pixels(kStrokeArc) + pixels(kSizeSpace2)),
                       juce::Justification::centredLeft);
            // Where the group's parameters live: the program, the keygroup chosen, or a zone of it. [RQ-APP-056, DEC-APP-031]
            g.drawText(juce::String(scopeText(group.layout->scope)).toUpperCase(), title, juce::Justification::centredRight);
            if (group.layout->zones > 0)
                paintGridLabels(g, group);
        }
    }

    void EditView::layoutGrid(Group& group, int left, int top) const
    {
        // A column per zone under its number, a row per parameter: as the sampler's ZONE page. [RQ-APP-057, DEC-APP-033]
        const int columnWidth = GRID_COLUMN_CELLS * ParameterCell::cellWidth();
        const int first = left + GRID_LABEL_CELLS * ParameterCell::cellWidth();
        const auto zones = static_cast<std::size_t>(group.layout->zones);
        for (std::size_t i = 0; i < group.cells.size(); ++i)
        {
            const int row = static_cast<int>(i / zones) + 1;
            const int zone = static_cast<int>(i % zones);
            group.cells[i]->setBounds(first + zone * columnWidth, top + row * gridPitch(), columnWidth, pixels(tokens::kValueboxHeight));
        }
    }

    void EditView::paintGridLabels(juce::Graphics& g, const Group& group) const
    {
        using namespace tokens;
        const int inside = pixels(kSpaceInside);
        const int left = group.bounds.getX() + inside;
        const int top = group.bounds.getY() + inside + pixels(kFontLabelSize) + pixels(kSpaceBetween);
        const int columnWidth = GRID_COLUMN_CELLS * ParameterCell::cellWidth();
        const int first = left + GRID_LABEL_CELLS * ParameterCell::cellWidth();
        const int height = pixels(kValueboxHeight);
        g.setColour(colourOf(kColourTextSecondary));
        g.setFont(_lookAndFeel.font(kFontLabelSize, true));
        for (int zone = 0; zone < group.layout->zones; ++zone)
            g.drawText(juce::String(ZONE_TEXT + std::to_string(zone + 1)).toUpperCase(), first + zone * columnWidth, top, columnWidth, height,
                       juce::Justification::centred);
        int y = top;
        for (const layouts::LayoutParameter& parameter : group.layout->parameters)
        {
            y += gridPitch();
            g.drawText(juce::String(std::string(parameter.label)).toUpperCase(), left + pixels(kSizeSpace1), y, first - left, height,
                       juce::Justification::centredLeft);
        }
    }

    bool EditView::keyPressed(const juce::KeyPress& key)
    {
        // The space bar auditions the edited sample; a value being typed has the focus and keeps the key. [RQ-APP-083]
        if (key == juce::KeyPress(juce::KeyPress::spaceKey) && _item)
        {
            _memory.toggleAudition(*_item);
            return true;
        }
        return false;
    }

    void EditView::mouseDown(const juce::MouseEvent&)
    {
        grabKeyboardFocus();
    }
}
