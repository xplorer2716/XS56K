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
#include "ParameterCell.hpp"

#include "LookAndFeel.hpp"
#include "app/DesignTokens.generated.hpp"

namespace app::ui
{
    // [RQ-APP-051, RQ-APP-052, RQ-APP-086, RQ-APP-003, ADR-APP-004 (DEC-APP-027), TASK-APP-038]
    namespace
    {
        constexpr int KNOB_CELLS = 1;
        constexpr int CHOICE_CELLS = 2;  // a choice and a value box alone: two knob cells, as in the mockups
        constexpr int SLIDER_CELLS = 3;
        constexpr int FIRST_ITEM_ID = 1;  // a ComboBox item id is the choice's code plus one: 0 means no selection
        // The knob's arc as the mockups draw it (KNOB_START_DEGREES 135, KNOB_SWEEP_DEGREES 270 of generate_mockups.py, from the right
        // clockwise): from the bottom left, through the top, to the bottom right; JUCE counts from the top.
        constexpr float KNOB_START = juce::MathConstants<float>::pi * 1.25f;
        constexpr float KNOB_END = juce::MathConstants<float>::pi * 2.75f;
    }

    juce::Colour roleColour(layouts::Role role)
    {
        using namespace tokens;
        switch (role)
        {
            case layouts::Role::Sample:
                return colourOf(kColourRoleSample);
            case layouts::Role::Modulation:
                return colourOf(kColourRoleModulation);
            case layouts::Role::Filter:
                return colourOf(kColourRoleFilter);
            case layouts::Role::General:
                break;
        }
        return colourOf(kColourRoleGeneral);
    }

    int ParameterCell::cells(layouts::Control control)
    {
        switch (control)
        {
            case layouts::Control::Choice:
            case layouts::Control::ValueBox:
                return CHOICE_CELLS;
            case layouts::Control::Slider:
                return SLIDER_CELLS;
            case layouts::Control::Knob:
            case layouts::Control::Switch:
                break;
        }
        return KNOB_CELLS;
    }

    int ParameterCell::cellWidth()
    {
        using namespace tokens;
        return pixels(kKnobSizeDefault + kSizeSpace8);
    }

    int ParameterCell::cellHeight()
    {
        using namespace tokens;
        return pixels(kKnobSizeDefault + kSpaceBetween + kFontLabelSize + kSizeSpace1 + kValueboxHeight);
    }

    ParameterCell::ParameterCell(LookAndFeel& lookAndFeel, const layouts::LayoutParameter& parameter)
        : _lookAndFeel(lookAndFeel), _parameter(parameter)
    {
        using namespace tokens;
        _label.setText(juce::String(std::string(parameter.label)).toUpperCase(), juce::dontSendNotification);
        _label.setFont(_lookAndFeel.font(kFontLabelSize, true));
        _label.setColour(juce::Label::textColourId, colourOf(kColourTextSecondary));
        _label.setJustificationType(juce::Justification::centred);
        _label.setBorderSize({});
        addAndMakeVisible(_label);

        // The value box: the exact value, typed. [RQ-APP-051]
        _box.setFont(_lookAndFeel.font(kFontValueSize));
        _box.setJustificationType(juce::Justification::centred);
        _box.setColour(juce::Label::backgroundColourId, colourOf(kValueboxSurface));
        _box.setColour(juce::Label::textColourId, colourOf(kValueboxText));
        _box.setBorderSize({});
        _box.setEditable(true, true, false);
        _box.onTextChange = [this] { valueBoxEdited(); };
        if (parameter.control != layouts::Control::Choice)
            addAndMakeVisible(_box);

        const juce::Colour colour = roleColour(parameter.role);
        _slider.setColour(juce::Slider::rotarySliderFillColourId, colour);
        _slider.setColour(juce::Slider::trackColourId, colour);
        _slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        _slider.setRotaryParameters(KNOB_START, KNOB_END, true);
        _slider.setSliderStyle(parameter.control == layouts::Control::Slider ? juce::Slider::LinearHorizontal
                                                                              : juce::Slider::RotaryHorizontalVerticalDrag);
        _slider.onValueChange = [this] {
            if (!_showing && onValue)
                onValue(static_cast<std::int64_t>(juce::roundToInt(_slider.getValue())));
        };
        if (parameter.control == layouts::Control::Knob || parameter.control == layouts::Control::Slider)
            addAndMakeVisible(_slider);

        _choice.onChange = [this] {
            if (!_showing && onValue && _choice.getSelectedId() >= FIRST_ITEM_ID)
                onValue(static_cast<std::int64_t>(_choice.getSelectedId() - FIRST_ITEM_ID));
        };
        if (parameter.control == layouts::Control::Choice)
            addAndMakeVisible(_choice);
    }

    void ParameterCell::show(const EditValue& value)
    {
        using namespace tokens;
        const juce::ScopedValueSetter<bool> showing(_showing, true);
        _shown = value;
        const juce::String text(valueText(value));
        // Pending: the pending colour and an ellipsis, so that the state is not told by colour alone. [RQ-APP-003, RQ-APP-005, RQ-APP-052]
        _box.setText(value.pending ? text + ELLIPSIS : text, juce::dontSendNotification);
        _box.setColour(juce::Label::textColourId, colourOf(value.pending ? kColourStatusPending : kValueboxText));

        if (value.max > value.min)
            _slider.setRange(static_cast<double>(value.min), static_cast<double>(value.max), 1.0);
        if (!_slider.isMouseButtonDown())
            _slider.setValue(static_cast<double>(value.value), juce::dontSendNotification);
        const juce::Colour colour = value.pending ? colourOf(kColourStatusPending) : roleColour(_parameter.role);
        _slider.setColour(juce::Slider::rotarySliderFillColourId, colour);
        _slider.setColour(juce::Slider::trackColourId, colour);

        if (_choice.getNumItems() == 0)
        {
            int id = FIRST_ITEM_ID;
            for (const std::string& label : value.definition->labels)
                _choice.addItem(juce::String(label), id++);
        }
        _choice.setSelectedId(static_cast<int>(value.value) + FIRST_ITEM_ID, juce::dontSendNotification);
        _choice.setColour(juce::ComboBox::textColourId, colourOf(value.pending ? kColourStatusPending : kChoiceText));
    }

    void ParameterCell::valueBoxEdited()
    {
        if (_showing || _shown.definition == nullptr)
            return;
        if (const auto value = valueFrom(_shown, _box.getText().toStdString()); value && onValue)
            onValue(*value);
        else
            show(_shown);  // not a value: the box shows again what the sampler holds
    }

    void ParameterCell::resized()
    {
        using namespace tokens;
        auto area = getLocalBounds().reduced(pixels(kSizeSpace1), 0);
        auto top = area.removeFromTop(pixels(kKnobSizeDefault));
        area.removeFromTop(pixels(kSpaceBetween));
        const auto label = area.removeFromTop(pixels(kFontLabelSize));
        area.removeFromTop(pixels(kSizeSpace1));
        const auto bottom = area.removeFromTop(pixels(kValueboxHeight));
        switch (_parameter.control)
        {
            case layouts::Control::Knob:
            case layouts::Control::Switch:
                _slider.setBounds(top.withSizeKeepingCentre(pixels(kKnobSizeDefault), pixels(kKnobSizeDefault)));
                _box.setBounds(bottom);
                break;
            case layouts::Control::Choice:
                _choice.setBounds(top.withSizeKeepingCentre(top.getWidth(), pixels(kSizeControlMedium)));
                break;
            case layouts::Control::ValueBox:
                _box.setBounds(top.withSizeKeepingCentre(top.getWidth(), pixels(kValueboxHeight)));
                break;
            case layouts::Control::Slider:
                // The value box on top, the slider under the label. [RQ-APP-086, owner 2026-10-15]
                _box.setBounds(top.withSizeKeepingCentre(top.getWidth(), pixels(kValueboxHeight)));
                _slider.setBounds(bottom);
                break;
        }
        _label.setBounds(label);
    }
}
