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
#include "akm/harness/ClockArithmetic.hpp"

namespace akm::harness
{
    namespace
    {
        constexpr std::int64_t SECONDS_PER_MINUTE = 60;
        constexpr std::int64_t SECONDS_PER_HOUR = 60 * SECONDS_PER_MINUTE;
        constexpr std::int64_t SECONDS_PER_DAY = 24 * SECONDS_PER_HOUR;
        constexpr std::int64_t DAYS_PER_WEEK = 7;

        // 1970-01-01 was a Thursday: the sampler's day of week 5 (1 = Sunday).
        constexpr std::int64_t EPOCH_DAY_OF_WEEK = 5;

        // The civil-calendar conversions below are Howard Hinnant's days_from_civil and civil_from_days: the days
        // are counted from 1970-01-01, in eras of 400 years (146097 days) that start on 1 March, so that the leap
        // day closes the year.
        constexpr std::int64_t DAYS_PER_ERA = 146097;
        constexpr std::int64_t YEARS_PER_ERA = 400;
        constexpr std::int64_t DAYS_FROM_ERA_START_TO_EPOCH = 719468;
        constexpr int MARCH = 3;
        constexpr int FIRST_DAY_OF_WEEK = 1;  // Sunday, the sampler's day of week 1
        constexpr int MONTHS_PER_YEAR = 12;
        // Counted from March, the months are 0 (March) to 11 (February): the last of them to fall in the same
        // calendar year as March are 0-9 (to December).
        constexpr int FIRST_SHIFTED_MONTH_OF_NEXT_YEAR = MONTHS_PER_YEAR - MARCH + 1;
        constexpr int MONTH_TERM_NUMERATOR = 153;
        constexpr int MONTH_TERM_DENOMINATOR = 5;
        constexpr int MONTH_TERM_OFFSET = 2;
        constexpr int DAYS_PER_FOUR_YEARS = 1461;
        constexpr int DAYS_PER_YEAR = 365;
        constexpr int DAYS_PER_CENTURY_APPROX = 36524;
        constexpr int YEAR_LENGTH_OFFSET = 1;

        std::int64_t floorDivide(std::int64_t value, std::int64_t divisor)
        {
            std::int64_t quotient = value / divisor;
            if (value % divisor != 0 && (value < 0) != (divisor < 0))
                --quotient;
            return quotient;
        }

        std::int64_t daysFromCivil(std::int64_t year, int month, int day)
        {
            year -= month < MARCH ? YEAR_LENGTH_OFFSET : 0;
            const std::int64_t era = floorDivide(year, YEARS_PER_ERA);
            const std::int64_t yearOfEra = year - era * YEARS_PER_ERA;
            const std::int64_t shiftedMonth = month >= MARCH ? month - MARCH : month + MONTHS_PER_YEAR - MARCH;
            const std::int64_t dayOfYear =
                (MONTH_TERM_NUMERATOR * shiftedMonth + MONTH_TERM_OFFSET) / MONTH_TERM_DENOMINATOR + day - 1;
            const std::int64_t dayOfEra = yearOfEra * DAYS_PER_YEAR + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
            return era * DAYS_PER_ERA + dayOfEra - DAYS_FROM_ERA_START_TO_EPOCH;
        }

        struct Civil
        {
            std::int64_t year;
            int month;
            int day;
        };

        Civil civilFromDays(std::int64_t days)
        {
            days += DAYS_FROM_ERA_START_TO_EPOCH;
            const std::int64_t era = floorDivide(days, DAYS_PER_ERA);
            const std::int64_t dayOfEra = days - era * DAYS_PER_ERA;
            const std::int64_t yearOfEra =
                (dayOfEra - dayOfEra / (DAYS_PER_FOUR_YEARS - 1) + dayOfEra / DAYS_PER_CENTURY_APPROX
                 - dayOfEra / (DAYS_PER_ERA - 1))
                / DAYS_PER_YEAR;
            const std::int64_t dayOfYear = dayOfEra - (DAYS_PER_YEAR * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
            const std::int64_t shiftedMonth = (MONTH_TERM_DENOMINATOR * dayOfYear + MONTH_TERM_OFFSET) / MONTH_TERM_NUMERATOR;
            const int day = static_cast<int>(dayOfYear - (MONTH_TERM_NUMERATOR * shiftedMonth + MONTH_TERM_OFFSET) / MONTH_TERM_DENOMINATOR + 1);
            const int month = static_cast<int>(shiftedMonth < FIRST_SHIFTED_MONTH_OF_NEXT_YEAR ? shiftedMonth + MARCH
                                                                                      : shiftedMonth - (MONTHS_PER_YEAR - MARCH));
            const std::int64_t year = yearOfEra + era * YEARS_PER_ERA + (month < MARCH ? YEAR_LENGTH_OFFSET : 0);
            return Civil{year, month, day};
        }
    }

    std::int64_t toEpochSeconds(const ClockDate& date)
    {
        return daysFromCivil(date.year, date.month, date.day) * SECONDS_PER_DAY + date.hours * SECONDS_PER_HOUR
               + date.minutes * SECONDS_PER_MINUTE + date.seconds;
    }

    ClockDate fromEpochSeconds(std::int64_t seconds)
    {
        const std::int64_t days = floorDivide(seconds, SECONDS_PER_DAY);
        const std::int64_t secondOfDay = seconds - days * SECONDS_PER_DAY;
        const Civil civil = civilFromDays(days);

        ClockDate date;
        date.year = static_cast<int>(civil.year);
        date.month = civil.month;
        date.day = civil.day;
        // 0-based weekday from the epoch's, then back to 1-based with 1 = Sunday.
        const std::int64_t weekdayIndex =
            ((days + EPOCH_DAY_OF_WEEK - FIRST_DAY_OF_WEEK) % DAYS_PER_WEEK + DAYS_PER_WEEK) % DAYS_PER_WEEK;
        date.dayOfWeek = static_cast<int>(weekdayIndex) + FIRST_DAY_OF_WEEK;
        date.hours = static_cast<int>(secondOfDay / SECONDS_PER_HOUR);
        date.minutes = static_cast<int>(secondOfDay % SECONDS_PER_HOUR / SECONDS_PER_MINUTE);
        date.seconds = static_cast<int>(secondOfDay % SECONDS_PER_MINUTE);
        return date;
    }

    ClockDate addSeconds(const ClockDate& date, std::int64_t seconds)
    {
        return fromEpochSeconds(toEpochSeconds(date) + seconds);
    }

    std::int64_t secondsBetween(const ClockDate& from, const ClockDate& to)
    {
        return toEpochSeconds(to) - toEpochSeconds(from);
    }
}
