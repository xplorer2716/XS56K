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

#include <set>

#include "LookAndFeel.hpp"
#include "app/DesignTokens.generated.hpp"

namespace app::ui
{
    // [RQ-APP-048 to RQ-APP-053, RQ-APP-083, RQ-APP-086, RQ-APP-087, ADR-APP-004 (DEC-APP-025, DEC-APP-027), TASK-APP-039]
    namespace
    {
        constexpr const char* READ_AGAIN_TEXT = "Read again";
        constexpr const char* NO_EDITOR = "This kind has no editor yet.";
        constexpr int READ_AGAIN_SHARE = 6;  // the button takes a sixth of the panel's width

        const char* kindText(MemoryKind kind)
        {
            switch (kind)
            {
                case MemoryKind::Multi:
                    return "Multi";
                case MemoryKind::Program:
                    return "Program";
                case MemoryKind::Sample:
                    return "Sample";
                case MemoryKind::SongFile:
                    return "Song file";
                case MemoryKind::SetList:
                    return "Set list";
                case MemoryKind::SceneList:
                    break;
            }
            return "Scenelist";
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
        setWantsKeyboardFocus(true);
        _model.addListener(*this);
        rebuild();
    }

    EditView::~EditView()
    {
        _model.removeListener(*this);
    }

    void EditView::modelChanged(ModelPart part)
    {
        if (part != ModelPart::Edit)
            return;
        const EditState& state = _model.edit();
        if (state.item != _item || state.layout != _layout)
            rebuild();
        else
            update();
    }

    void EditView::rebuild()
    {
        const EditState& state = _model.edit();
        _item = state.item;
        _layout = state.layout;
        _groups.clear();
        juce::StringArray titles;
        if (_layout != nullptr)
        {
            for (const layouts::LayoutTab& tab : _layout->tabs)
                titles.add(juce::String(std::string(tab.title)));
            std::size_t index = 0;
            for (const layouts::LayoutGroup& layout : _layout->tabs[state.tab].groups)
            {
                Group group;
                group.layout = &layout;
                for (const layouts::LayoutParameter& parameter : layout.parameters)
                {
                    auto cell = std::make_unique<ParameterCell>(_lookAndFeel, parameter);
                    cell->onValue = [this, index](std::int64_t value) { _editor.set(index, value); };  // [RQ-APP-052]
                    addAndMakeVisible(*cell);
                    group.cells.push_back(std::move(cell));
                    ++index;
                }
                _groups.push_back(std::move(group));
            }
        }
        if (onTabs)
            onTabs(titles, static_cast<int>(state.tab));
        _name.setText(_item ? juce::String(_item->name) : juce::String(), juce::dontSendNotification);
        _kind.setText(_item ? juce::String(kindText(_item->kind)).toUpperCase() : juce::String(), juce::dontSendNotification);
        _message.setText(_item && _layout == nullptr ? juce::String(NO_EDITOR) : juce::String(), juce::dontSendNotification);
        _readAgain.setVisible(_layout != nullptr);
        update();
        resized();
        repaint();
    }

    void EditView::update()
    {
        const EditState& state = _model.edit();
        _info.setText(juce::String(infoText(state)), juce::dontSendNotification);
        std::size_t index = 0;
        for (Group& group : _groups)
        {
            for (auto& cell : group.cells)
            {
                if (index < state.values.size())
                    cell->show(state.values[index]);
                ++index;
            }
        }
    }

    juce::Point<int> EditView::sizeOf(const layouts::LayoutGroup& group)
    {
        using namespace tokens;
        int cells = 0;
        for (const layouts::LayoutParameter& parameter : group.parameters)
            cells += ParameterCell::cells(parameter.control);
        const int inside = pixels(kSpaceInside);
        return {cells * ParameterCell::cellWidth() + 2 * inside,
                inside + pixels(kFontLabelSize) + pixels(kSpaceBetween) + ParameterCell::cellHeight() + inside};
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
        _info.setBounds(header);
        area.removeFromTop(pixels(kSpaceGroup));
        _message.setBounds(area);

        // The groups flow left to right, then wrap, as in the mockups. [RQ-APP-086]
        const int gap = pixels(kSpaceGroup);
        int x = area.getX();
        int y = area.getY();
        int rowHeight = 0;
        for (Group& group : _groups)
        {
            const auto size = sizeOf(*group.layout);
            if (x > area.getX() && x + size.x > area.getRight())
            {
                x = area.getX();
                y += rowHeight + gap;
                rowHeight = 0;
            }
            group.bounds = {x, y, size.x, size.y};
            const int inside = pixels(kSpaceInside);
            int cellX = x + inside;
            const int cellY = y + inside + pixels(kFontLabelSize) + pixels(kSpaceBetween);
            for (std::size_t i = 0; i < group.cells.size(); ++i)
            {
                const int width = ParameterCell::cells(group.layout->parameters[i].control) * ParameterCell::cellWidth();
                group.cells[i]->setBounds(cellX, cellY, width, ParameterCell::cellHeight());
                cellX += width;
            }
            x += size.x + gap;
            rowHeight = std::max(rowHeight, size.y);
        }
    }

    void EditView::paint(juce::Graphics& g)
    {
        using namespace tokens;
        const int inside = pixels(kSpaceInside);
        for (const Group& group : _groups)
        {
            g.setColour(colourOf(kColourSurfaceRaised));
            g.fillRoundedRectangle(group.bounds.toFloat(), kRadiusPanel);
            // The group's mark: its parameters' colour when they share one family, the secondary text colour otherwise. [RQ-APP-005]
            std::set<layouts::Role> roles;
            for (const layouts::LayoutParameter& parameter : group.layout->parameters)
                roles.insert(parameter.role);
            const auto title = juce::Rectangle<int>(group.bounds.getX() + inside, group.bounds.getY() + inside, group.bounds.getWidth() - 2 * inside,
                                                    pixels(kFontLabelSize));
            g.setColour(roles.size() == 1 ? roleColour(*roles.begin()) : colourOf(kColourTextSecondary));
            g.fillRect(title.withWidth(pixels(kStrokeArc)));
            g.setColour(colourOf(kColourTextSecondary));
            g.setFont(_lookAndFeel.font(kFontLabelSize, true));
            g.drawText(juce::String(std::string(group.layout->title)).toUpperCase(), title.withTrimmedLeft(pixels(kStrokeArc) + pixels(kSizeSpace2)),
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
