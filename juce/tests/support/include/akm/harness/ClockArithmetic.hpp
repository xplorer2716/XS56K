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

#include "akm/SystemSetup.hpp"

namespace akm::harness
{
    /// Date arithmetic on the sampler's clock (spec Table 6, &05/&06) in the proleptic Gregorian calendar, with
    /// no time zone: what the real-sampler suite puts the clock back with — the time it read, advanced by the time
    /// that elapsed. Test support, not a library feature: the AKM layer reads and sets the clock and does not
    /// compute with it. [RQ-AKM-054, RQ-AKM-058]

    /// Seconds since 1970-01-01 00:00:00 of `date`; its `dayOfWeek` is not read.
    [[nodiscard]] std::int64_t toEpochSeconds(const ClockDate& date);

    /// The date `seconds` after 1970-01-01 00:00:00, with its day of week (1 = Sunday).
    [[nodiscard]] ClockDate fromEpochSeconds(std::int64_t seconds);

    /// `date` advanced by `seconds` (negative to go back), with the day of week that follows.
    [[nodiscard]] ClockDate addSeconds(const ClockDate& date, std::int64_t seconds);

    /// The signed number of seconds from `from` to `to`.
    [[nodiscard]] std::int64_t secondsBetween(const ClockDate& from, const ClockDate& to);
}
