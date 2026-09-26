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

// The error numbers of an ERROR confirmation (spec Table 3, pp. 6-7): Data1 * 128 + Data2, with the
// meaning the spec gives, and the raw number kept when it is not in the table.
// [TASK-AKM-004, RQ-AKM-005, ADR-AKM-001 (DEC-AKM-002)]
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <initializer_list>

#include "TestBytes.hpp"
#include "akm/Confirmation.hpp"
#include "akm/SamplerError.hpp"

using akm::Confirmation;
using akm::ReplyId;
using akm::describeError;
using akm::errorNumber;
using akm::test::bytes;

namespace
{
    Confirmation confirmationWith(ReplyId replyId, std::initializer_list<unsigned int> data)
    {
        return Confirmation{1, bytes({0x10}), replyId, 0x0A, 0x07, bytes(data)};
    }
}

TEST_CASE("Given an ERROR with Data1 = 03 and Data2 = 01, When decoded, Then the number is 385 and its meaning is the missing keygroup [RQ-AKM-005]",
          "[akm][error]")
{
    const auto number = errorNumber(confirmationWith(ReplyId::Error, {0x03, 0x01}));

    REQUIRE(number.has_value());
    CHECK(*number == 385);

    const akm::ErrorInfo info = describeError(*number);
    CHECK(info.known);
    CHECK(info.number == 385);
    CHECK(info.meaning == "requested keygroup does not exist in the current program");
}

TEST_CASE("Given an ERROR with Data1 = 7F and Data2 = 7F, When decoded, Then the number 16383 is kept with the meaning unknown [RQ-AKM-005]",
          "[akm][error]")
{
    const auto number = errorNumber(confirmationWith(ReplyId::Error, {0x7F, 0x7F}));

    REQUIRE(number.has_value());
    CHECK(*number == 16383);

    const akm::ErrorInfo info = describeError(*number);
    CHECK_FALSE(info.known);
    CHECK(info.number == 16383);
    CHECK(info.meaning == "unknown error number");
}

TEST_CASE("Given a confirmation that is not an ERROR, When its error number is requested, Then there is none [RQ-AKM-005]",
          "[akm][error]")
{
    CHECK_FALSE(errorNumber(confirmationWith(ReplyId::Done, {})).has_value());
    CHECK_FALSE(errorNumber(confirmationWith(ReplyId::Reply, {0x03, 0x01})).has_value());
    CHECK_FALSE(errorNumber(confirmationWith(ReplyId::Ok, {})).has_value());
}

TEST_CASE("Given an ERROR with fewer than two data bytes, When its error number is requested, Then there is none [RQ-AKM-005]",
          "[akm][error]")
{
    CHECK_FALSE(errorNumber(confirmationWith(ReplyId::Error, {})).has_value());
    CHECK_FALSE(errorNumber(confirmationWith(ReplyId::Error, {0x03})).has_value());
}

TEST_CASE("Given an ERROR with a trailing byte, When its error number is requested, Then the first two bytes give it [RQ-AKM-005]",
          "[akm][error]")
{
    // A checksum the session did not expect follows the number: the two bytes before it still are.
    const auto number = errorNumber(confirmationWith(ReplyId::Error, {0x00, 0x02, 0x6A}));

    REQUIRE(number.has_value());
    CHECK(*number == 2);
}

TEST_CASE("Given every error number of Table 3, When described, Then each is known and keeps its number [RQ-AKM-005]",
          "[akm][error]")
{
    // Table 3: 00-06, 81, 101-112 and 181 (hexadecimal).
    std::uint16_t count = 0;
    const auto expectKnown = [&count](std::uint16_t number) {
        const akm::ErrorInfo info = describeError(number);
        CHECK(info.known);
        CHECK(info.number == number);
        CHECK_FALSE(info.meaning.empty());
        ++count;
    };

    for (std::uint16_t number = 0x00; number <= 0x06; ++number)
        expectKnown(number);
    expectKnown(0x81);
    for (std::uint16_t number = 0x101; number <= 0x112; ++number)
        expectKnown(number);
    expectKnown(0x181);

    CHECK(count == 27);
}

TEST_CASE("Given numbers next to those of Table 3, When described, Then they are unknown [RQ-AKM-005]",
          "[akm][error]")
{
    for (const std::uint16_t number : std::initializer_list<std::uint16_t>{0x07, 0x80, 0x82, 0x100, 0x113, 0x180, 0x182, 0x3FFF})
    {
        const akm::ErrorInfo info = describeError(number);
        CHECK_FALSE(info.known);
        CHECK(info.number == number);
    }
}

TEST_CASE("Given two error numbers, When described, Then their meanings differ [RQ-AKM-005]", "[akm][error]")
{
    // The caller must be able to tell "not found" from "out of range" (RQ-AKM-005's rationale).
    CHECK(describeError(akm::error_number::NOT_FOUND).meaning != describeError(akm::error_number::OUT_OF_RANGE).meaning);
    CHECK(describeError(akm::error_number::NOT_SUPPORTED).meaning != describeError(akm::error_number::CHECKSUM_INVALID).meaning);
}
