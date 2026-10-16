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
#include "LookAndFeel.hpp"

#include <cmath>

#include "BinaryData.h"
#include "app/DesignTokens.generated.hpp"

namespace app::ui
{
    // [RQ-APP-001, RQ-APP-002, RQ-APP-004, ADR-APP-002 (DEC-APP-013, DEC-APP-015), TASK-APP-017]
    namespace
    {
        constexpr float KNOB_POINTER_LENGTH = 0.55f;  // the pointer's share of the knob's radius: KNOB_POINTER_LENGTH of generate_mockups.py
        constexpr float DIAMETER_PER_RADIUS = 2.0f;
        constexpr const char* CHOICE_PREVIOUS = "<";
        constexpr const char* CHOICE_NEXT = ">";

        juce::Typeface::Ptr loadTypeface(const char* data, int size, const char* name)
        {
            auto typeface = juce::Typeface::createSystemTypefaceFor(data, static_cast<size_t>(size));
            if (typeface == nullptr)
                juce::Logger::writeToLog(juce::String("XS56K: the embedded font ") + name + " could not be loaded; the system's sans-serif is used.");
            return typeface;
        }
    }

    juce::Colour colourOf(std::uint32_t argb)
    {
        return juce::Colour(static_cast<juce::uint32>(argb));
    }

    int pixels(float size)
    {
        return juce::roundToInt(size);
    }

    LookAndFeel::LookAndFeel()
        : _regular(loadTypeface(BinaryData::InterRegular_woff2, BinaryData::InterRegular_woff2Size, "Inter Regular")),
          _medium(loadTypeface(BinaryData::InterMedium_woff2, BinaryData::InterMedium_woff2Size, "Inter Medium"))
    {
        using namespace tokens;
        setColourScheme(juce::LookAndFeel_V4::ColourScheme(
            colourOf(kColourSurfaceBase), colourOf(kColourSurfaceRecessed), colourOf(kColourSurfaceRaised), colourOf(kColourBorderDefault),
            colourOf(kColourTextPrimary), colourOf(kButtonSurface), colourOf(kColourTextPrimary), colourOf(kRowSurfaceHover),
            colourOf(kColourTextPrimary)));

        setColour(juce::ResizableWindow::backgroundColourId, colourOf(kColourSurfaceBase));
        setColour(juce::DocumentWindow::textColourId, colourOf(kColourTextPrimary));
        setColour(juce::TextButton::buttonColourId, colourOf(kButtonSurface));
        setColour(juce::TextButton::textColourOffId, colourOf(kButtonText));
        setColour(juce::TextButton::textColourOnId, colourOf(kButtonText));
        setColour(juce::Label::textColourId, colourOf(kColourTextPrimary));
        setColour(juce::ComboBox::backgroundColourId, colourOf(kChoiceSurface));
        setColour(juce::ComboBox::textColourId, colourOf(kChoiceText));
        setColour(juce::ComboBox::arrowColourId, colourOf(kChoiceArrow));
        setColour(juce::ComboBox::outlineColourId, colourOf(kColourBorderDefault));
        setColour(juce::ComboBox::focusedOutlineColourId, colourOf(kColourAccentDefault));
        setColour(juce::TextEditor::backgroundColourId, colourOf(kValueboxSurface));
        setColour(juce::TextEditor::textColourId, colourOf(kValueboxText));
        setColour(juce::TextEditor::outlineColourId, colourOf(kColourBorderDefault));
        setColour(juce::TextEditor::focusedOutlineColourId, colourOf(kColourAccentDefault));
        setColour(juce::PopupMenu::backgroundColourId, colourOf(kDialogSurface));
        setColour(juce::PopupMenu::textColourId, colourOf(kColourTextPrimary));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, colourOf(kRowSurfaceHover));
        setColour(juce::PopupMenu::highlightedTextColourId, colourOf(kColourTextPrimary));
        setColour(juce::DialogWindow::backgroundColourId, colourOf(kDialogSurface));
        setColour(juce::TooltipWindow::backgroundColourId, colourOf(kDialogSurface));
        setColour(juce::TooltipWindow::textColourId, colourOf(kColourTextPrimary));
        setColour(juce::TooltipWindow::outlineColourId, colourOf(kDialogBorder));
    }

    juce::Font LookAndFeel::font(float size, bool medium) const
    {
        const juce::Typeface::Ptr& typeface = medium ? _medium : _regular;
        if (typeface == nullptr)
            return juce::Font(juce::FontOptions(size, medium ? juce::Font::bold : juce::Font::plain));
        return juce::Font(juce::FontOptions(typeface).withHeight(size));
    }

    juce::Typeface::Ptr LookAndFeel::getTypefaceForFont(const juce::Font& font)
    {
        // A font that names no typeface of its own is drawn in Inter. [DEC-APP-015]
        if (font.getTypefaceName() == juce::Font::getDefaultSansSerifFontName() && _regular != nullptr)
            return font.isBold() && _medium != nullptr ? _medium : _regular;
        return juce::LookAndFeel_V4::getTypefaceForFont(font);
    }

    juce::Font LookAndFeel::getTextButtonFont(juce::TextButton&, int)
    {
        return font(tokens::kFontValueSize, true);
    }

    juce::Font LookAndFeel::getLabelFont(juce::Label& label)
    {
        return label.getFont().getHeight() > 0 ? label.getFont() : font(tokens::kFontValueSize);
    }

    juce::Font LookAndFeel::getComboBoxFont(juce::ComboBox&)
    {
        return font(tokens::kFontValueSize);
    }

    juce::Font LookAndFeel::getPopupMenuFont()
    {
        return font(tokens::kFontValueSize);
    }

    juce::Font LookAndFeel::getMenuBarFont(juce::MenuBarComponent&, int, const juce::String&)
    {
        return font(tokens::kFontValueSize);
    }

    void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool down)
    {
        using namespace tokens;
        const bool destructive = button.getProperties()[DESTRUCTIVE_PROPERTY];
        juce::Colour surface = colourOf(destructive ? kButtonDestructiveSurface : kButtonSurface);
        if (!button.isEnabled())
            surface = colourOf(kColourSurfacePanel);
        else if (!destructive && (highlighted || down))
            surface = colourOf(kColourBorderDefault);
        const auto bounds = button.getLocalBounds().toFloat().reduced(kStrokeHairline);
        g.setColour(surface);
        g.fillRoundedRectangle(bounds, kRadiusControl);
        if (button.hasKeyboardFocus(true))
        {
            g.setColour(colourOf(kColourAccentDefault));
            g.drawRoundedRectangle(bounds, kRadiusControl, kStrokeFocus);
        }
    }

    void LookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
    {
        using namespace tokens;
        const bool destructive = button.getProperties()[DESTRUCTIVE_PROPERTY];
        g.setFont(getTextButtonFont(button, button.getHeight()));
        g.setColour(!button.isEnabled() ? colourOf(kColourTextDisabled) : colourOf(destructive ? kButtonDestructiveText : kButtonText));
        g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(pixels(kSpaceInside), 0), juce::Justification::centred, 1);
    }

    void LookAndFeel::drawStretchableLayoutResizerBar(juce::Graphics& g, int width, int height, bool isVerticalBar, bool isMouseOver,
                                                      bool isMouseDragging)
    {
        using namespace tokens;
        g.fillAll(colourOf(kResizerBar));
        if (!isMouseOver && !isMouseDragging)
            return;
        // The accent says where to drag; the resize cursor is the second cue. [RQ-APP-005, ADR-APP-002 (DEC-APP-022), TASK-APP-031]
        const auto bar = juce::Rectangle<int>(0, 0, width, height).toFloat();
        g.setColour(colourOf(kResizerBarActive));
        g.fillRect(isVerticalBar ? bar.withSizeKeepingCentre(kStrokeFocus, bar.getHeight()) : bar.withSizeKeepingCentre(bar.getWidth(), kStrokeFocus));
    }

    // [RQ-APP-086, RQ-APP-005, ADR-APP-004 (DEC-APP-027), TASK-APP-038]
    void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position, float startAngle, float endAngle,
                                       juce::Slider& slider)
    {
        using namespace tokens;
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - kStrokeArc;
        const auto centre = bounds.getCentre();
        g.setColour(colourOf(kColourSurfaceRecessed));
        const float inner = (radius - kStrokeArc) * DIAMETER_PER_RADIUS;
        g.fillEllipse(juce::Rectangle<float>(inner, inner).withCentre(centre));
        const auto arc = [&](float to, juce::Colour colour) {
            juce::Path path;
            path.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, to, true);
            g.setColour(colour);
            g.strokePath(path, juce::PathStrokeType(kStrokeArc, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        arc(endAngle, colourOf(kKnobArcTrack));
        const float angle = std::lerp(startAngle, endAngle, position);
        if (position > 0.0f)
            arc(angle, slider.findColour(juce::Slider::rotarySliderFillColourId));
        const auto tip = centre.getPointOnCircumference(radius * KNOB_POINTER_LENGTH, angle);
        g.setColour(colourOf(kKnobPointer));
        g.drawLine(juce::Line<float>(centre, tip), kStrokeFocus);
    }

    void LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float position, float, float,
                                       juce::Slider::SliderStyle, juce::Slider& slider)
    {
        using namespace tokens;
        const float middle = static_cast<float>(y) + static_cast<float>(height) / 2.0f;
        const auto left = static_cast<float>(x);
        const auto right = static_cast<float>(x + width);
        g.setColour(colourOf(kSliderTrack));
        g.drawLine(left, middle, right, middle, kStrokeArc);
        g.setColour(slider.findColour(juce::Slider::trackColourId));
        g.drawLine(left, middle, position, middle, kStrokeArc);
        g.setColour(colourOf(kSliderThumb));
        g.fillEllipse(juce::Rectangle<float>(kSliderThumbSize, kSliderThumbSize).withCentre({position, middle}));
    }

    void LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
    {
        using namespace tokens;
        const auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
        g.setColour(colourOf(kChoiceSurface));
        g.fillRoundedRectangle(bounds, kRadiusControl);
        if (box.hasKeyboardFocus(true))
        {
            g.setColour(colourOf(kColourAccentDefault));
            g.drawRoundedRectangle(bounds.reduced(kStrokeHairline), kRadiusControl, kStrokeFocus);
        }
        g.setColour(colourOf(kChoiceArrow));
        g.setFont(font(kFontValueSize));
        const auto inside = bounds.reduced(kSpaceInside, 0).toNearestInt();
        g.drawText(CHOICE_PREVIOUS, inside, juce::Justification::centredLeft);
        g.drawText(CHOICE_NEXT, inside, juce::Justification::centredRight);
    }

    void LookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
    {
        using namespace tokens;
        label.setBounds(box.getLocalBounds().reduced(pixels(kSpaceInside + kFontValueSize), 0));
        label.setFont(font(kFontValueSize));
        label.setJustificationType(juce::Justification::centred);
    }
}
