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

// The value formats of the AKAI SysEx protocol (spec pp. 8-9): word, dword, qword, signed values
// and null-terminated strings, on 7-bit data bytes.
// [TASK-AKM-004, RQ-AKM-002, ADR-AKM-001 (DEC-AKM-002)]
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include "TestBytes.hpp"
#include "akm/ByteReader.hpp"
#include "akm/ByteWriter.hpp"

using akm::ByteReader;
using akm::ByteWriter;
using akm::test::Bytes;
using akm::test::bytes;

namespace
{
    // Largest value of each unsigned width: 128^n - 1, spelled out so the tests do not depend on the
    // production constants.
    constexpr std::uint32_t wordMax = 16383;
    constexpr std::uint32_t dwordMax = 268435455;
    constexpr std::uint64_t qwordMax = (std::uint64_t{1} << 56) - 1;
    constexpr std::int32_t signedByteMax = 127;
}

TEST_CASE("Given the word bytes 03 01, When decoded, Then the value is 385 [RQ-AKM-002]", "[akm][values]")
{
    const Bytes data = bytes({0x03, 0x01});
    ByteReader reader(data);

    const auto value = reader.readWord();

    REQUIRE(value.has_value());
    CHECK(*value == 385);
    CHECK(reader.remaining() == 0);
}

TEST_CASE("Given the spec's word example MSB 5 and LSB 65, When decoded, Then the value is 705 [RQ-AKM-002]",
          "[akm][values]")
{
    const Bytes data = bytes({5, 65});
    ByteReader reader(data);

    const auto value = reader.readWord();

    REQUIRE(value.has_value());
    CHECK(*value == 705);
}

TEST_CASE("Given the dword bytes 01 02 03 04, When decoded, Then the most significant byte comes first [RQ-AKM-002]",
          "[akm][values]")
{
    // 1 * 128^3 + 2 * 128^2 + 3 * 128 + 4 = 2097152 + 32768 + 384 + 4.
    const Bytes data = bytes({0x01, 0x02, 0x03, 0x04});
    ByteReader reader(data);

    const auto value = reader.readDword();

    REQUIRE(value.has_value());
    CHECK(*value == 2130308);
}

TEST_CASE("Given the qword bytes 00 00 00 00 00 00 01 00, When decoded, Then the value is 128 [RQ-AKM-002]",
          "[akm][values]")
{
    const Bytes data = bytes({0, 0, 0, 0, 0, 0, 1, 0});
    ByteReader reader(data);

    const auto value = reader.readQword();

    REQUIRE(value.has_value());
    CHECK(*value == 128);
}

TEST_CASE("Given the signed word -37, When encoded and decoded, Then the bytes are 01 00 25 and it round-trips [RQ-AKM-002]",
          "[akm][values]")
{
    ByteWriter writer;

    REQUIRE(writer.appendSignedWord(-37));
    CHECK(writer.bytes() == bytes({0x01, 0x00, 0x25}));

    ByteReader reader(writer.bytes());
    const auto value = reader.readSignedWord();
    REQUIRE(value.has_value());
    CHECK(*value == -37);
}

TEST_CASE("Given the signed word +37, When encoded, Then the sign byte is 00 [RQ-AKM-002]", "[akm][values]")
{
    ByteWriter writer;

    REQUIRE(writer.appendSignedWord(37));
    CHECK(writer.bytes() == bytes({0x00, 0x00, 0x25}));
}

TEST_CASE("Given a negative zero on the wire, When decoded, Then it is zero [RQ-AKM-002]", "[akm][values]")
{
    // Sign byte 01 with a null magnitude: tolerated on input, never produced on output.
    const Bytes data = bytes({0x01, 0x00, 0x00});
    ByteReader reader(data);

    const auto value = reader.readSignedWord();

    REQUIRE(value.has_value());
    CHECK(*value == 0);

    ByteWriter writer;
    REQUIRE(writer.appendSignedWord(0));
    CHECK(writer.bytes() == bytes({0x00, 0x00, 0x00}));
}

TEST_CASE("Given the string AB, When encoded, Then the bytes are 41 42 00 [RQ-AKM-002]", "[akm][values]")
{
    ByteWriter writer;

    REQUIRE(writer.appendString("AB"));
    CHECK(writer.bytes() == bytes({0x41, 0x42, 0x00}));
}

TEST_CASE("Given the empty string, When encoded, Then only the terminator is written [RQ-AKM-002]", "[akm][values]")
{
    ByteWriter writer;

    REQUIRE(writer.appendString(""));
    CHECK(writer.bytes() == bytes({0x00}));
}

TEST_CASE("Given a null-terminated string, When decoded, Then the terminator is consumed [RQ-AKM-002]",
          "[akm][values]")
{
    const Bytes data = bytes({0x41, 0x42, 0x00, 0x43});
    ByteReader reader(data);

    const auto text = reader.readString();

    REQUIRE(text.has_value());
    CHECK(*text == "AB");
    CHECK(reader.remaining() == 1);
}

TEST_CASE("Given concatenated null-terminated strings, When decoded as a list, Then each string is returned [RQ-AKM-002]",
          "[akm][values]")
{
    const Bytes data = bytes({0x41, 0x42, 0x00, 0x43, 0x44, 0x00});
    ByteReader reader(data);

    const auto list = reader.readStringList();

    REQUIRE(list.has_value());
    REQUIRE(list->size() == 2);
    CHECK((*list)[0] == "AB");
    CHECK((*list)[1] == "CD");
    CHECK(reader.remaining() == 0);
}

TEST_CASE("Given no bytes, When decoded as a string list, Then the list is empty [RQ-AKM-002]", "[akm][values]")
{
    ByteReader reader(std::span<const std::uint8_t>{});

    const auto list = reader.readStringList();

    REQUIRE(list.has_value());
    CHECK(list->empty());
}

TEST_CASE("Given a list whose last string has no terminator, When decoded, Then it is refused [RQ-AKM-002]",
          "[akm][values]")
{
    const Bytes data = bytes({0x41, 0x00, 0x42});
    ByteReader reader(data);

    CHECK_FALSE(reader.readStringList().has_value());
    CHECK(reader.remaining() == 3);
}

TEST_CASE("Given the minimum and maximum of each unsigned width, When encoded then decoded, Then each is unchanged [RQ-AKM-002]",
          "[akm][values]")
{
    SECTION("byte")
    {
        for (const std::uint32_t value : {0u, 127u})
        {
            ByteWriter writer;
            REQUIRE(writer.appendByte(value));
            ByteReader reader(writer.bytes());
            CHECK(reader.readByte() == value);
        }
    }
    SECTION("word")
    {
        for (const std::uint32_t value : {0u, wordMax})
        {
            ByteWriter writer;
            REQUIRE(writer.appendWord(value));
            CHECK(writer.bytes().size() == 2);
            ByteReader reader(writer.bytes());
            CHECK(reader.readWord() == value);
        }
    }
    SECTION("dword")
    {
        for (const std::uint32_t value : {0u, dwordMax})
        {
            ByteWriter writer;
            REQUIRE(writer.appendDword(value));
            CHECK(writer.bytes().size() == 4);
            ByteReader reader(writer.bytes());
            CHECK(reader.readDword() == value);
        }
    }
    SECTION("qword")
    {
        for (const std::uint64_t value : {std::uint64_t{0}, qwordMax})
        {
            ByteWriter writer;
            REQUIRE(writer.appendQword(value));
            CHECK(writer.bytes().size() == 8);
            ByteReader reader(writer.bytes());
            CHECK(reader.readQword() == value);
        }
    }
}

TEST_CASE("Given the extremes of each signed width, When encoded then decoded, Then each is unchanged [RQ-AKM-002]",
          "[akm][values]")
{
    SECTION("signed byte")
    {
        for (const std::int32_t value : {-signedByteMax, 0, signedByteMax})
        {
            ByteWriter writer;
            REQUIRE(writer.appendSignedByte(value));
            CHECK(writer.bytes().size() == 2);
            ByteReader reader(writer.bytes());
            CHECK(reader.readSignedByte() == value);
        }
    }
    SECTION("signed word")
    {
        const auto limit = static_cast<std::int32_t>(wordMax);
        for (const std::int32_t value : {-limit, 0, limit})
        {
            ByteWriter writer;
            REQUIRE(writer.appendSignedWord(value));
            CHECK(writer.bytes().size() == 3);
            ByteReader reader(writer.bytes());
            CHECK(reader.readSignedWord() == value);
        }
    }
    SECTION("signed dword")
    {
        const auto limit = static_cast<std::int32_t>(dwordMax);
        for (const std::int32_t value : {-limit, 0, limit})
        {
            ByteWriter writer;
            REQUIRE(writer.appendSignedDword(value));
            CHECK(writer.bytes().size() == 5);
            ByteReader reader(writer.bytes());
            CHECK(reader.readSignedDword() == value);
        }
    }
}

TEST_CASE("Given a value beyond its width, When appended, Then it is refused and nothing is written [RQ-AKM-002]",
          "[akm][values]")
{
    ByteWriter writer;
    bool accepted = true;

    SECTION("byte above 127") { accepted = writer.appendByte(128); }
    SECTION("word above 16383") { accepted = writer.appendWord(wordMax + 1); }
    SECTION("dword above 128^4 - 1") { accepted = writer.appendDword(dwordMax + 1); }
    SECTION("qword above 128^8 - 1") { accepted = writer.appendQword(qwordMax + 1); }
    SECTION("signed byte above 127") { accepted = writer.appendSignedByte(signedByteMax + 1); }
    SECTION("signed byte below -127") { accepted = writer.appendSignedByte(-signedByteMax - 1); }
    SECTION("signed word above 16383") { accepted = writer.appendSignedWord(static_cast<std::int32_t>(wordMax) + 1); }
    SECTION("signed dword above 128^4 - 1") { accepted = writer.appendSignedDword(static_cast<std::int32_t>(dwordMax) + 1); }
    SECTION("signed dword at the minimum of int32")
    {
        accepted = writer.appendSignedDword(std::numeric_limits<std::int32_t>::min());
    }
    SECTION("string with a byte above 127") { accepted = writer.appendString(std::string("A\x80")); }
    SECTION("string with an embedded terminator") { accepted = writer.appendString(std::string("A\0B", 3)); }

    CHECK_FALSE(accepted);
    CHECK_FALSE(writer.ok());
    CHECK(writer.bytes().empty());
}

TEST_CASE("Given a refused value, When more values are appended, Then the writer stays failed and writes nothing [RQ-AKM-002]",
          "[akm][values]")
{
    ByteWriter writer;
    REQUIRE(writer.appendByte(1));

    CHECK_FALSE(writer.appendWord(wordMax + 1));
    CHECK_FALSE(writer.appendByte(2));

    CHECK_FALSE(writer.ok());
    CHECK(writer.bytes() == bytes({0x01}));
}

TEST_CASE("Given too few bytes, When a value is decoded, Then it is refused and the position is unchanged [RQ-AKM-002]",
          "[akm][values]")
{
    const Bytes data = bytes({0x01, 0x02, 0x03});
    ByteReader reader(data);

    CHECK_FALSE(reader.readDword().has_value());
    CHECK_FALSE(reader.readQword().has_value());
    CHECK(reader.remaining() == 3);
    // The failed reads consumed nothing: a word can still be read from the start (1 * 128 + 2).
    CHECK(reader.readWord() == 130);
}

TEST_CASE("Given a byte above 127, When decoded, Then it is refused and the position is unchanged [RQ-AKM-002]",
          "[akm][values]")
{
    const Bytes data = bytes({0x80});
    ByteReader reader(data);

    CHECK_FALSE(reader.readByte().has_value());
    CHECK_FALSE(reader.readWord().has_value());
    CHECK(reader.remaining() == 1);
}

TEST_CASE("Given a sign byte other than 00 or 01, When decoded, Then it is refused [RQ-AKM-002]", "[akm][values]")
{
    const Bytes data = bytes({0x02, 0x00, 0x25});
    ByteReader reader(data);

    CHECK_FALSE(reader.readSignedWord().has_value());
    CHECK(reader.remaining() == 3);
}

TEST_CASE("Given a string without terminator, When decoded, Then it is refused and the position is unchanged [RQ-AKM-002]",
          "[akm][values]")
{
    const Bytes data = bytes({0x41, 0x42});
    ByteReader reader(data);

    CHECK_FALSE(reader.readString().has_value());
    CHECK(reader.remaining() == 2);
}
