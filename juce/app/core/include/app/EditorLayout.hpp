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
// The types of the editor layouts: each editor's tabs, each tab's groups, each group's parameters by their catalogue name, with the label drawn,
// the role that colours them and the control that edits them, and the orders their values keep. The tables are generated into EditorLayouts.generated.hpp from
// the JSON files of juce/app/design/layouts/, with the shapes a choice draws. No JUCE. [RQ-APP-049, RQ-APP-086, RQ-APP-096, ADR-APP-001 (DEC-APP-006),
// ADR-APP-004 (DEC-APP-024, DEC-APP-027, DEC-APP-032)]
#pragma once

#include <span>
#include <string_view>

namespace app::layouts
{
    /// The control that edits a parameter; the same names as the mockups' (DEC-APP-027).
    enum class Control
    {
        Knob,
        Choice,
        Switch,
        ValueBox,
        Slider,
        Sample,  ///< a zone's sample, chosen among those in memory; in a zone grid only (DEC-APP-033)
    };

    /// The family of a parameter, which decides its colour (ADR-APP-002 DEC-APP-016).
    enum class Role
    {
        Sample,
        Modulation,
        Filter,
        Amp,
        General,
    };

    /// A point of a choice's shape, in a unit square: x from 0 to 1, y from 0 (bottom) to 1. [RQ-APP-096, ADR-APP-004 (DEC-APP-032)]
    struct ShapePoint
    {
        float x;
        float y;
    };

    /// The shape drawn for one label of a choice (a filter type's response), from juce/app/design/shapes/.
    struct ChoiceShape
    {
        std::string_view label;
        std::span<const ShapePoint> points;
    };

    struct LayoutParameter
    {
        std::string_view name;   ///< the catalogue's name
        std::string_view label;  ///< as drawn under the control
        Role role;
        Control control;
        std::span<const ChoiceShape> shapes{};  ///< a choice's shapes by label; empty: none drawn (DEC-APP-032)
        int cells = 0;                          ///< the knob cells a widened choice or value box takes; 0: its control's (TASK-APP-048)
    };

    /// The shape of `label` among the parameter's shapes; empty when it has none, and the name is shown alone. [RQ-APP-096]
    [[nodiscard]] constexpr std::span<const ShapePoint> shapeOf(const LayoutParameter& parameter, std::string_view label)
    {
        for (const ChoiceShape& shape : parameter.shapes)
        {
            if (shape.label == label)
                return shape.points;
        }
        return {};
    }

    /// Where a group's parameters live, said after its title (RQ-APP-056); `None` for an editor of one kind (the sample's).
    enum class Scope
    {
        None,
        Program,
        Keygroup,
        Zone,
    };

    /// A stage of an envelope drawn as a graph: its time parameter and its end level, a parameter or `fixedLevel` (0 to 100); or the sustain
    /// mark, where the envelope holds until the release. [RQ-APP-095, ADR-APP-004 (DEC-APP-031)]
    struct EnvelopeStage
    {
        std::string_view time;
        std::string_view level;
        int fixedLevel = 0;
        bool sustain = false;
    };

    /// A column of a group of columns: its title and where its cells live, drawn above it (a routing's source and amount). [DEC-APP-035]
    struct LayoutColumn
    {
        std::string_view title;
        Scope scope = Scope::None;
    };

    struct LayoutGroup
    {
        std::string_view title;
        std::span<const LayoutParameter> parameters;
        Scope scope = Scope::None;
        std::span<const EnvelopeStage> envelope;  ///< empty: no graph
        bool newRow = false;                      ///< the group starts a new row of its tab (DEC-APP-031)
        int zones = 0;                            ///< a zone grid's columns, one per zone; 0: no grid (DEC-APP-033)
        std::span<const LayoutColumn> columns{};  ///< a group of columns, its parameters read a row at a time; empty: none (DEC-APP-035)
    };

    struct LayoutTab
    {
        std::string_view title;
        std::span<const LayoutGroup> groups;
    };

    struct EditorLayout
    {
        std::string_view editor;  ///< "sample", later "program"
        std::span<const LayoutTab> tabs;
        /// Chains of parameter names whose values must not decrease: each value is bounded by its neighbours. [RQ-APP-092, DEC-APP-029]
        std::span<const std::span<const std::string_view>> orders;
    };
}
