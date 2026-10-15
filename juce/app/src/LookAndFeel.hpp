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

namespace app::ui
{
    /// The name of the component property a button carries when it confirms a destructive action. [RQ-APP-011]
    inline const juce::Identifier DESTRUCTIVE_PROPERTY{"destructive"};
    /// "…", which ends a menu item that opens a dialog and a state still in progress.
    inline const juce::String ELLIPSIS{juce::CharPointer_UTF8("\xe2\x80\xa6")};

    /// A design-system colour as JUCE needs it.
    [[nodiscard]] juce::Colour colourOf(std::uint32_t argb);
    /// A design-system size rounded to whole logical pixels, for layout.
    [[nodiscard]] int pixels(float size);

    /// The one look-and-feel of the application: it turns the design-system tokens into the colours, fonts, radii and strokes of every
    /// standard component, and embeds Inter (the system's sans-serif when Inter cannot be loaded). The components set state properties and
    /// draw no visual value of their own. [RQ-APP-001, RQ-APP-002, RQ-APP-004, ADR-APP-002 (DEC-APP-013, DEC-APP-015), TASK-APP-017]
    class LookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();

        /// Inter (regular or medium) at a size of the type scale, or the system's sans-serif when Inter could not be loaded.
        [[nodiscard]] juce::Font font(float size, bool medium = false) const;

        juce::Typeface::Ptr getTypefaceForFont(const juce::Font& font) override;
        juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
        juce::Font getLabelFont(juce::Label& label) override;
        juce::Font getComboBoxFont(juce::ComboBox& box) override;
        juce::Font getPopupMenuFont() override;
        juce::Font getMenuBarFont(juce::MenuBarComponent& menuBar, int itemIndex, const juce::String& itemText) override;
        void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour, bool highlighted,
                                  bool down) override;
        void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool highlighted, bool down) override;

    private:
        juce::Typeface::Ptr _regular;
        juce::Typeface::Ptr _medium;
    };
}
