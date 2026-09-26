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

// The checksum of the AKAI SysEx protocol (spec p. 4): unsigned 8-bit wrapping sum, high bit cleared.
// [TASK-AKM-004, RQ-AKM-003, ADR-AKM-001 (DEC-AKM-002, DEC-AKM-009)]
#include <catch2/catch_test_macros.hpp>

#include "TestBytes.hpp"
#include "akm/Checksum.hpp"

using akm::test::bytes;

TEST_CASE("Given the bytes 10 0C 1B 35 6D, When the checksum is computed, Then it is 59 [RQ-AKM-003]",
          "[akm][checksum]")
{
    // The spec's own example: (&10 + &0C + &1B + &35 + &6D) = &D9, AND &7F = &59.
    CHECK(akm::checksum(bytes({0x10, 0x0C, 0x1B, 0x35, 0x6D})) == 0x59);
}

TEST_CASE("Given a sum that overflows 8 bits, When the checksum is computed, Then it wraps [RQ-AKM-003]",
          "[akm][checksum]")
{
    // The spec: "unsigned 8-bit addition ... wraps on overflow (i.e., 255+1 = 0)".
    CHECK(akm::checksum(bytes({0xFF, 0x01})) == 0x00);
    // 3 * 0x7F = 0x17D, which wraps to 0x7D on 8 bits.
    CHECK(akm::checksum(bytes({0x7F, 0x7F, 0x7F})) == 0x7D);
}

TEST_CASE("Given a sum whose bit 7 is set, When the checksum is computed, Then the high bit is cleared [RQ-AKM-003]",
          "[akm][checksum]")
{
    CHECK(akm::checksum(bytes({0x7F, 0x01})) == 0x00);
    for (unsigned int value = 0; value <= 0xFF; ++value)
        CHECK(akm::checksum(bytes({value})) == (value & 0x7F));
}

TEST_CASE("Given no bytes, When the checksum is computed, Then it is zero [RQ-AKM-003]", "[akm][checksum]")
{
    CHECK(akm::checksum({}) == 0x00);
}
