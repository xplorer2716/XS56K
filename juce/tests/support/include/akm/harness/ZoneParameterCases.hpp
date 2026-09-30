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

#include <cstdint>
#include <vector>

#include "akm/ItemCatalogue.hpp"

namespace akm::harness
{
    /// One Set/Get pair of RQ-AKM-034's 13 non-sample §06 items, with one representative in-range value
    /// on zone 3: `values` is the full Set argument list (zone, then the value); the selector prefix is
    /// however many arguments the paired Get item itself takes (1: the zone). Named `ZoneParameterCase`
    /// rather than reusing `KeygroupParameterCase`/`ProgramParameterCase`: same shape, different domain,
    /// kept independent so a change to one table cannot silently affect the others.
    struct ZoneParameterCase
    {
        ItemId setId;
        ItemId getId;
        std::vector<std::int64_t> values;
    };

    /// All 13 Set/Get pairs of the non-sample §06 items, one row each (chosen independently of
    /// TASK-AKM-035's own mock-test values): what the real-sampler suite's zone check round-trips on a
    /// keygroup of the dedicated test program. [RQ-AKM-034, RQ-AKM-038]
    [[nodiscard]] const std::vector<ZoneParameterCase>& allZoneParameterCases();
}
