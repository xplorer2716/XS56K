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
    };

    /// The item edited (none when nothing is selected), its editor (null for a kind that has none yet, RQ-APP-048), the tab shown, the tab's
    /// values in the layout's order and the editor's read-only information.
    struct EditState
    {
        std::optional<MemoryItem> item;
        const layouts::EditorLayout* layout = nullptr;
        std::size_t tab = 0;
        std::vector<EditValue> values;
        std::vector<EditValue> info;
    };
}
