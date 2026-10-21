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
// The edit panel's part of the model: the item edited, its editor's layout, the tab shown, the values read and the read-only information.
// No JUCE. [RQ-APP-048, RQ-APP-050 to RQ-APP-053, RQ-APP-086, RQ-APP-087, ADR-APP-001 (DEC-APP-004), ADR-APP-004 (DEC-APP-025)]
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/EditorLayout.hpp"
#include "app/Memory.hpp"
#include "mcp/ParameterCatalogue.hpp"

namespace app
{
    /// One parameter as read from the sampler: its catalogue row (name, kind, labels, unit), its place in the layout (null for the read-only
    /// information), the value read, its range for the person (a position bounded by the sample's length, RQ-APP-088) and whether a write of
    /// it is waiting for the sampler (RQ-APP-052).
    struct EditValue
    {
        const mcp::ParameterDefinition* definition = nullptr;
        const layouts::LayoutParameter* layout = nullptr;
        std::int64_t value = 0;
        std::int64_t min = 0;
        std::int64_t max = 0;
        bool pending = false;
        int zone = 0;  ///< the zone (1 to 4) of a zone grid's value; 0 outside a grid [RQ-APP-060, DEC-APP-033]
        /// With all keygroups chosen: the keygroups do not share the value, `value` being the first's. [RQ-APP-059, TASK-APP-050]
        bool differs = false;
    };

    /// The keygroup "all keygroups" in `EditState::keygroup`: a change goes to every keygroup. [RQ-APP-059, TASK-APP-050]
    inline constexpr int ALL_KEYGROUPS = -1;

    /// The item edited (none when nothing is selected), its editor (null for a kind that has none yet, RQ-APP-048), the tab shown, the tab's
    /// values in the layout's order and the editor's read-only information.
    struct EditState
    {
        std::optional<MemoryItem> item;
        const layouts::EditorLayout* layout = nullptr;
        std::size_t tab = 0;
        std::vector<EditValue> values;
        std::vector<EditValue> info;
        std::optional<int> keygroups;  ///< a program's keygroup count, read when it is made current [RQ-APP-093]
        int keygroup = 1;  ///< the keygroup whose values a program's tab shows, 1 to `keygroups`, or ALL_KEYGROUPS [RQ-APP-058, RQ-APP-059]
        /// The samples in memory, read with a tab that holds a zone's sample: that value is 1 plus the sample's place here, 0 none.
        /// [RQ-APP-060, DEC-APP-033]
        std::vector<std::string> samples;
    };

    /// A value as the sampler's screen writes it: a root as a note name ("C3"), a signed tune with its sign ("+10"), a choice by its label
    /// ("ONE SHOT"), a switch as "ON" or "OFF", any other number as it is; "VARIES" when the keygroups do not share it (RQ-APP-059). [RQ-APP-051, RQ-APP-086, ADR-APP-004 (DEC-APP-027), TASK-APP-038]
    [[nodiscard]] std::string valueText(const EditValue& value);
    /// What the person typed for a parameter: a number (a leading + allowed), a note name for a root, a label for a choice, without regard
    /// to case or surrounding spaces; nothing when it is none of them. The range is checked by the edit controller. [RQ-APP-051, TASK-APP-038]
    [[nodiscard]] std::optional<std::int64_t> valueFrom(const EditValue& value, std::string_view text);
    /// The value a control steps to from the value shown, `direction` -1 or +1: the previous or next code of a choice or a switch, the
    /// previous or next step of a number; nothing past the value's range. [RQ-APP-097, RQ-APP-098, ADR-APP-004 (DEC-APP-036), TASK-APP-053]
    [[nodiscard]] std::optional<std::int64_t> steppedValue(const EditValue& value, int direction);
    /// The information line: a sample's "RAM, mono, 22050 points, 44100 Hz", a program's "2 keygroups". [RQ-APP-087, RQ-APP-093, TASK-APP-038]
    [[nodiscard]] std::string infoText(const EditState& state);
}
