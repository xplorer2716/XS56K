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

#include <cstddef>
#include <cstdint>

#include "akm/ItemDescriptor.hpp"
#include "akm/ItemTable.generated.hpp"

namespace akm
{
    /// The record of an item, by its enumerator. [ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]
    [[nodiscard]] constexpr const ItemDescriptor& descriptor(ItemId id)
    {
        return ITEM_TABLE[static_cast<std::size_t>(id)];
    }

    /// The record for a section and an item code, or null when the catalogue does not list it. Both
    /// numbers are needed: an item code means something else in each section. [RQ-AKM-041]
    [[nodiscard]] const ItemDescriptor* findItem(std::uint8_t section, std::uint8_t item);
}
