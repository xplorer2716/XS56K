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
// juce/app/design/layouts/*.json. No JUCE. [RQ-APP-049, RQ-APP-086, ADR-APP-001 (DEC-APP-006), ADR-APP-004 (DEC-APP-024, DEC-APP-027)]
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

    struct LayoutParameter
    {
        std::string_view name;   ///< the catalogue's name
        std::string_view label;  ///< as drawn under the control
        Role role;
        Control control;
    };

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

    struct LayoutGroup
    {
        std::string_view title;
        std::span<const LayoutParameter> parameters;
        Scope scope = Scope::None;
        std::span<const EnvelopeStage> envelope;  ///< empty: no graph
        bool newRow = false;                      ///< the group starts a new row of its tab (DEC-APP-031)
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
