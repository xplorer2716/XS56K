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

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "app/Edit.hpp"
#include "app/EditorLayout.hpp"

namespace app::ui
{
    /// An envelope drawn from the values shown, as the mockups draw it (DEC-APP-031): from level 0, each stage as wide as its time's part of one
    /// share of the width (a share per stage, the sustain plateau one share), ending at its level; the area under the line in the group's
    /// colour, a breakpoint at the end of each stage that has a time. Dragging a breakpoint gives its stage's time from the horizontal move and
    /// its level from the vertical move when the level is a parameter, through `onValue`; while dragged, the point follows the pointer, and the
    /// graph shows what the model holds again when it is let go (RQ-APP-091). [RQ-APP-095, ADR-APP-004 (DEC-APP-031), TASK-APP-044]
    class EnvelopeGraph final : public juce::Component
    {
    public:
        /// `indexOf` gives the place in the tab's values of a parameter named by the envelope.
        EnvelopeGraph(std::span<const layouts::EnvelopeStage> stages, const std::function<std::optional<std::size_t>(std::string_view)>& indexOf,
                      juce::Colour colour);

        /// Shows what the model holds; a stage being dragged stays under the pointer.
        void show(const std::vector<EditValue>& values);

        /// The person gives `value` to the tab's parameter at `index`.
        std::function<void(std::size_t index, std::int64_t value)> onValue;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        struct Stage
        {
            std::optional<std::size_t> time;   ///< the time's index in the tab's values; none for the sustain mark
            std::optional<std::size_t> level;  ///< the level's index, when the level is a parameter
            float fixedLevel = 0.0f;           ///< the level, 0 to 1, when it is not
            bool sustain = false;
        };

        /// One stage's shape: its time and its end level, 0 to 1 each.
        struct Shape
        {
            float time = 0.0f;
            float level = 0.0f;
        };

        [[nodiscard]] juce::Rectangle<float> plot() const;
        [[nodiscard]] std::vector<juce::Point<float>> points(std::vector<std::optional<std::size_t>>* handles = nullptr) const;
        [[nodiscard]] Shape shapeOf(std::size_t stage) const;
        [[nodiscard]] float fractionOf(std::size_t index) const;
        [[nodiscard]] std::int64_t valueAt(std::size_t index, float fraction) const;

        std::vector<Stage> _stages;
        juce::Colour _colour;
        std::vector<EditValue> _values;
        std::optional<std::size_t> _dragged;  ///< the stage whose breakpoint is dragged
        Shape _preview;                       ///< its shape under the pointer
    };
}
