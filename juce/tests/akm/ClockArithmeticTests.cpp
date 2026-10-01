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

// The date arithmetic the real-sampler suite restores the sampler's clock with: put back "the time it was, advanced
// by the time elapsed" (RQ-AKM-058). [TASK-AKM-053, RQ-AKM-054, RQ-AKM-058]
#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "akm/SystemSetup.hpp"
#include "akm/harness/ClockArithmetic.hpp"

using akm::ClockDate;
using akm::harness::addSeconds;
using akm::harness::fromEpochSeconds;
using akm::harness::secondsBetween;
using akm::harness::toEpochSeconds;

namespace
{
    constexpr std::int64_t SECONDS_PER_DAY = 24 * 60 * 60;
    constexpr std::int64_t SECONDS_PER_WEEK = 7 * SECONDS_PER_DAY;

    // Day of week 1 = Sunday (spec Table 6).
    constexpr int SATURDAY = 7;
    constexpr int TUESDAY = 3;

    constexpr ClockDate FIRST_DAY{1980, 1, 1, TUESDAY, 0, 0, 0};
    constexpr ClockDate Y2K{2000, 1, 1, SATURDAY, 0, 0, 0};
    constexpr ClockDate LAST_DAY{2079, 12, 31, 1, 23, 59, 59};
    constexpr ClockDate TEST_CLOCK{2030, 6, 15, SATURDAY, 8, 5, 9};
}

TEST_CASE("Given the last second of 28 February of a leap year, When one second is added, Then it is 29 February, a Thursday [RQ-AKM-058]",
          "[akm][clock]")
{
    const ClockDate before{2024, 2, 28, 4, 23, 59, 59};

    CHECK(addSeconds(before, 1) == ClockDate{2024, 2, 29, 5, 0, 0, 0});
}

TEST_CASE("Given the last second of a year, When one second is added, Then it is the first of January of the next, with its weekday [RQ-AKM-058]",
          "[akm][clock]")
{
    const ClockDate before{2023, 12, 31, 1, 23, 59, 59};

    CHECK(addSeconds(before, 1) == ClockDate{2024, 1, 1, 2, 0, 0, 0});
}

TEST_CASE("Given the first day, a leap-year day and the last day of the range the spec allows, When each is turned into seconds and back, Then it is unchanged and its weekday is computed [RQ-AKM-058]",
          "[akm][clock]")
{
    for (const ClockDate& date : {FIRST_DAY, Y2K, TEST_CLOCK, LAST_DAY})
        CHECK(fromEpochSeconds(toEpochSeconds(date)) == date);
}

TEST_CASE("Given a date, When nothing or a whole number of weeks is added, Then the date is the same, or the same weekday [RQ-AKM-058]",
          "[akm][clock]")
{
    CHECK(addSeconds(TEST_CLOCK, 0) == TEST_CLOCK);

    const ClockDate later = addSeconds(TEST_CLOCK, 3 * SECONDS_PER_WEEK);
    CHECK(later.dayOfWeek == TEST_CLOCK.dayOfWeek);
    CHECK(later == ClockDate{2030, 7, 6, SATURDAY, 8, 5, 9});
}

TEST_CASE("Given two dates, When the seconds between them are asked, Then it is the signed difference, and adding it to the first gives the second [RQ-AKM-058]",
          "[akm][clock]")
{
    constexpr std::int64_t ONE_HOUR_TWO_SECONDS = 3602;
    const ClockDate later = addSeconds(TEST_CLOCK, ONE_HOUR_TWO_SECONDS);

    CHECK(later == ClockDate{2030, 6, 15, SATURDAY, 9, 5, 11});
    CHECK(secondsBetween(TEST_CLOCK, later) == ONE_HOUR_TWO_SECONDS);
    CHECK(secondsBetween(later, TEST_CLOCK) == -ONE_HOUR_TWO_SECONDS);
    CHECK(addSeconds(TEST_CLOCK, secondsBetween(TEST_CLOCK, later)) == later);
}
