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
        constexpr const char* NO_SAMPLE = "NONE";  // a zone that plays no sample, as the mockups write it [RQ-APP-060]
        // The knob's arc as the mockups draw it (KNOB_START_DEGREES 135, KNOB_SWEEP_DEGREES 270 of generate_mockups.py, from the right
        // clockwise): from the bottom left, through the top, to the bottom right; JUCE counts from the top.
        constexpr float KNOB_START = juce::MathConstants<float>::pi * 1.25f;
        constexpr float KNOB_END = juce::MathConstants<float>::pi * 2.75f;
    }

    juce::Colour roleColour(layouts::Role role)
    {
        using enum app::layouts::Role;
        using namespace tokens;
        switch (role)
        {
            case Sample:
                return colourOf(kColourRoleSample);
            case Modulation:
                return colourOf(kColourRoleModulation);
            case Filter:
                return colourOf(kColourRoleFilter);
            case Amp:
                return colourOf(kColourRoleAmp);
            case General:
                break;
        }
        return colourOf(kColourRoleGeneral);
    }

    int ParameterCell::cells(layouts::Control control)
    {
        using enum app::layouts::Control;
        switch (control)
        {
            case Choice:
            case ValueBox:
            case Sample:
                return CHOICE_CELLS;
            case Slider:
                return SLIDER_CELLS;
            case Knob:
            case Switch:
                break;
        }
        return KNOB_CELLS;
    }

    int ParameterCell::cells(const layouts::LayoutParameter& parameter)
    {
        return parameter.cells > 0 ? parameter.cells : cells(parameter.control);
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

    ParameterCell::ParameterCell(LookAndFeel& lookAndFeel, const layouts::LayoutParameter& parameter, bool inGrid)
        : _lookAndFeel(lookAndFeel), _parameter(parameter), _inGrid(inGrid)
    {
        using namespace tokens;
        _label.setText(juce::String(std::string(parameter.label)).toUpperCase(), juce::dontSendNotification);
        _label.setFont(_lookAndFeel.font(kFontLabelSize, true));
        _label.setColour(juce::Label::textColourId, colourOf(kColourTextSecondary));
        _label.setJustificationType(juce::Justification::centred);
        _label.setBorderSize({});
        if (!inGrid)
            addAndMakeVisible(_label);  // a grid labels its row once [DEC-APP-033]

        // The value box: the exact value, typed. [RQ-APP-051]
        _box.setFont(_lookAndFeel.font(kFontValueSize));
        _box.setJustificationType(juce::Justification::centred);
        _box.setColour(juce::Label::backgroundColourId, colourOf(kValueboxSurface));
        _box.setColour(juce::Label::textColourId, colourOf(kValueboxText));
        _box.setBorderSize({});
        _box.setEditable(true, true, false);
        _box.onTextChange = [this] { valueBoxEdited(); };
        const bool chosen = parameter.control == layouts::Control::Choice || parameter.control == layouts::Control::Sample;
        if (!chosen)
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
        _slider.onDragEnd = [this] { show(_shown); };  // a refused value leaves the thumb where the model's value is [RQ-APP-051]
        if (parameter.control == layouts::Control::Knob || parameter.control == layouts::Control::Slider)
            addAndMakeVisible(_slider);

        _choice.onChange = [this] {
            if (!_showing && onValue && _choice.getSelectedId() >= FIRST_ITEM_ID)
            {
                onValue(static_cast<std::int64_t>(_choice.getSelectedId() - FIRST_ITEM_ID));
                show(_shown);
            }
        };
        if (chosen)
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

        if (_choice.getNumItems() == 0 && _parameter.control != layouts::Control::Sample)
        {
            int id = FIRST_ITEM_ID;
            for (const std::string& label : value.definition->labels)
                _choice.addItem(juce::String(label).toUpperCase(), id++);  // as the sampler's screen and valueText write it
        }
        _choice.setSelectedId(static_cast<int>(value.value) + FIRST_ITEM_ID, juce::dontSendNotification);
        _choice.setColour(juce::ComboBox::textColourId, colourOf(value.pending ? kColourStatusPending : kChoiceText));
        if (!_parameter.shapes.empty())
            repaint();  // the shape follows the value shown [RQ-APP-096]
    }

    void ParameterCell::setSamples(const std::vector<std::string>& samples)
    {
        if (samples == _samples && _choice.getNumItems() > 0)
            return;
        _samples = samples;
        const juce::ScopedValueSetter<bool> showing(_showing, true);
        _choice.clear(juce::dontSendNotification);
        _choice.addItem(NO_SAMPLE, FIRST_ITEM_ID);  // value 0 [RQ-APP-060, DEC-APP-033]
        int id = FIRST_ITEM_ID + 1;
        for (const std::string& sample : samples)
            _choice.addItem(juce::String(sample), id++);
        _choice.setSelectedId(static_cast<int>(_shown.value) + FIRST_ITEM_ID, juce::dontSendNotification);
    }

    void ParameterCell::paint(juce::Graphics& g)
    {
        using namespace tokens;
        if (_shapeArea.isEmpty() || _shown.definition == nullptr)
            return;
        const auto& labels = _shown.definition->labels;
        if (_shown.value < 0 || static_cast<std::size_t>(_shown.value) >= labels.size())
            return;
        const auto points = layouts::shapeOf(_parameter, labels[static_cast<std::size_t>(_shown.value)]);
        if (points.empty())
            return;  // a type the manual does not draw: its name alone [RQ-APP-096]
        const auto area = _shapeArea.toFloat();
        g.setColour(colourOf(kEnvelopeSurface));
        g.fillRoundedRectangle(area, kRadiusControl);
        const auto inner = area.reduced(kSizeSpace1);
        const auto at = [&inner](const layouts::ShapePoint& point) {
            return juce::Point<float>(inner.getX() + inner.getWidth() * point.x, inner.getBottom() - inner.getHeight() * point.y);
        };
        juce::Path line;
        line.startNewSubPath(at(points.front()));
        for (const layouts::ShapePoint& point : points.subspan(1))
            line.lineTo(at(point));
        g.setColour(_shown.pending ? colourOf(kColourStatusPending) : roleColour(_parameter.role));
        g.strokePath(line, juce::PathStrokeType(kStrokeArc, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void ParameterCell::valueBoxEdited()
    {
        if (_showing || _shown.definition == nullptr)
            return;
        if (const auto value = valueFrom(_shown, _box.getText().toStdString()); value && onValue)
            onValue(*value);
        show(_shown);  // a text that is not a value, or a value refused, leaves the box with what the model holds [RQ-APP-051]
    }

    void ParameterCell::resized()
    {
        using enum app::layouts::Control;
        using namespace tokens;
        auto area = getLocalBounds().reduced(pixels(kSizeSpace1), 0);
        if (_inGrid)
        {
            // A zone grid's cell: its control alone, a row high. [DEC-APP-033]
            _box.setBounds(area);
            _choice.setBounds(area);
            return;
        }
        auto top = area.removeFromTop(pixels(kKnobSizeDefault));
        area.removeFromTop(pixels(kSpaceBetween));
        const auto label = area.removeFromTop(pixels(kFontLabelSize));
        area.removeFromTop(pixels(kSizeSpace1));
        const auto bottom = area.removeFromTop(pixels(kValueboxHeight));
        switch (_parameter.control)
        {
            case Knob:
            case Switch:
                _slider.setBounds(top.withSizeKeepingCentre(pixels(kKnobSizeDefault), pixels(kKnobSizeDefault)));
                _box.setBounds(bottom);
                break;
            case Sample:
            case Choice:
                if (_parameter.shapes.empty())
                {
                    _choice.setBounds(top.withSizeKeepingCentre(top.getWidth(), pixels(kSizeControlMedium)));
                    break;
                }
                // The shape where a knob has its dial, the choice where it has its value box. [RQ-APP-096, DEC-APP-032]
                _shapeArea = top;
                _choice.setBounds(bottom);
                break;
            case ValueBox:
                _box.setBounds(top.withSizeKeepingCentre(top.getWidth(), pixels(kValueboxHeight)));
                break;
            case Slider:
                // The value box on top, the slider under the label. [RQ-APP-086, owner 2026-10-15]
                _box.setBounds(top.withSizeKeepingCentre(top.getWidth(), pixels(kValueboxHeight)));
                _slider.setBounds(bottom);
                break;
        }
        _label.setBounds(label);
    }
}
