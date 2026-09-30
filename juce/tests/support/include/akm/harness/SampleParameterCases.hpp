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
    /// One Set/Get pair of RQ-AKM-048's 8 settable §0E items, with one representative in-range value on
    /// the current sample: `values` is the full Set argument list (no selector — these items take
    /// none); the paired Get takes no selector either. Named `SampleParameterCase` rather than reusing
    /// `ZoneParameterCase`/`ProgramParameterCase`/`KeygroupParameterCase`: same shape, different domain,
    /// kept independent so a change to one table cannot silently affect the others.
    struct SampleParameterCase
    {
        ItemId setId;
        ItemId getId;
        std::vector<std::int64_t> values;
    };

    /// All 8 Set/Get pairs of the settable §0E items, one row each (chosen independently of
    /// TASK-AKM-043's own mock-test values): what the real-sampler suite's sample check round-trips on
    /// the dedicated test sample. [RQ-AKM-048, RQ-AKM-051]
    [[nodiscard]] const std::vector<SampleParameterCase>& allSampleParameterCases();
}
