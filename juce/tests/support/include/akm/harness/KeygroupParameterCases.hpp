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
    /// One Set/Get pair of RQ-AKM-030's six §08 parameter groups (General Options, Pitch/Amp, Filter,
    /// Filter Envelope, Amplitude Envelope, Aux Envelope), with one representative in-range value:
    /// `values` is the full Set argument list (a selector prefix, if the item has one, followed by the
    /// value); the selector prefix length is however many arguments the paired Get item itself takes.
    /// Named `KeygroupParameterCase` rather than reusing `ProgramParameterCase`: same shape, different
    /// domain, kept independent so a change to one table cannot silently affect the other.
    struct KeygroupParameterCase
    {
        ItemId setId;
        ItemId getId;
        std::vector<std::int64_t> values;
    };

    /// All 39 Set/Get pairs of the six parameter groups, one row each (chosen independently of
    /// TASK-AKM-027 to 032's own mock-test values, so this table is not coupled to theirs): what the
    /// real-sampler suite's keygroup-lifecycle check round-trips on the dedicated test program.
    /// [RQ-AKM-030, RQ-AKM-033]
    [[nodiscard]] const std::vector<KeygroupParameterCase>& allKeygroupParameterCases();
}
