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

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "CutLabel.hpp"
#include "app/Edit.hpp"
#include "app/EditorLayout.hpp"

namespace app::ui
{
    class LookAndFeel;

    /// One parameter of an editor's tab, as the mockups draw it (the layout's `control` is the contract, DEC-APP-027): a knob with its label and
    /// value box under it; a choice; a value box alone; or a value box with its label and a slider under it. A value given by the person (a
    /// drag, a choice, a text typed) is reported through `onValue`; the value shown is always the one the model holds, with the pending state in
    /// the pending colour and an ellipsis (RQ-APP-005). Every visual value comes from the tokens. In a zone grid the cell is its control alone,
    /// a row high, its label drawn once by the row (DEC-APP-033). [RQ-APP-051, RQ-APP-052, RQ-APP-086, RQ-APP-003, RQ-APP-004, RQ-APP-060,
    /// ADR-APP-004 (DEC-APP-027, DEC-APP-033), TASK-APP-038, TASK-APP-046]
    class ParameterCell final : public juce::Component
    {
    public:
        ParameterCell(LookAndFeel& lookAndFeel, const layouts::LayoutParameter& parameter, bool inGrid = false);

        /// How many knob cells wide the control is: a knob one, a choice or a value box two, a slider three.
        [[nodiscard]] static int cells(layouts::Control control);
        /// The cells of a parameter: those its layout gives it (a choice with long labels), or its control's. [TASK-APP-048]
        [[nodiscard]] static int cells(const layouts::LayoutParameter& parameter);
        /// The width of one knob cell and the height of every cell.
        [[nodiscard]] static int cellWidth();
        [[nodiscard]] static int cellHeight();

        /// Shows what the model holds; a value the person is dragging is not moved under the pointer.
        void show(const EditValue& value);
        /// A zone's sample: the samples in memory it is chosen among, after NONE (value 0). [RQ-APP-060, DEC-APP-033]
        void setSamples(const std::vector<std::string>& samples);

        std::function<void(std::int64_t)> onValue;

        void resized() override;
        /// A choice with shapes draws the shape of the value shown above it, in its role's colour. [RQ-APP-096, ADR-APP-004 (DEC-APP-032)]
        void paint(juce::Graphics& g) override;

    private:
        void valueBoxEdited();

        LookAndFeel& _lookAndFeel;
        const layouts::LayoutParameter& _parameter;
        CutLabel _label;
        juce::Label _box;
        juce::Slider _slider;
        juce::ComboBox _choice;
        juce::Rectangle<int> _shapeArea;  ///< where a choice's shape is drawn; empty: none
        bool _inGrid = false;
        std::vector<std::string> _samples;  ///< the choices of a zone's sample, as last given
        EditValue _shown;
        /// True while the controls are set from the model: that is no value the person gave.
        bool _showing = false;
    };

    /// The colour of a parameter's family. [RQ-APP-005, ADR-APP-002 (DEC-APP-016)]
    [[nodiscard]] juce::Colour roleColour(layouts::Role role);
}
