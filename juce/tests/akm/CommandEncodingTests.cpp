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

// Encoding of a command into a SysEx frame: F0 47 5E <dev> <user-ref...> <section> <item> <data...>
// [<checksum>] F7 (spec pp. 3-5).
// [TASK-AKM-004, RQ-AKM-001, RQ-AKM-003, RQ-AKM-041, ADR-AKM-001 (DEC-AKM-002, DEC-AKM-009)]
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include "TestBytes.hpp"
#include "akm/Checksum.hpp"
#include "akm/Command.hpp"

using akm::ChecksumMode;
using akm::Command;
using akm::EncodeError;
using akm::encodeCommand;
using akm::test::Bytes;
using akm::test::bytes;

namespace
{
    // The spec's example (p. 5): DeviceID 5, user-ref 10, section 0C, item 1B, data 35 6D.
    const Command specCommand{0x0C, 0x1B, bytes({0x35, 0x6D})};
    constexpr std::uint32_t specDeviceId = 5;
}

TEST_CASE("Given DeviceID 5, user-ref 10, section 0C, item 1B, data 35 6D and checksum ON, When encoded, Then the frame is the spec's example [RQ-AKM-001]",
          "[akm][command]")
{
    const auto result = encodeCommand(specDeviceId, bytes({0x10}), specCommand, ChecksumMode::On);

    REQUIRE(result.ok());
    CHECK(result.bytes == bytes({0xF0, 0x47, 0x5E, 0x05, 0x10, 0x0C, 0x1B, 0x35, 0x6D, 0x59, 0xF7}));
}

TEST_CASE("Given checksum OFF, When a command is encoded, Then no checksum is appended [RQ-AKM-003]",
          "[akm][command]")
{
    const auto result = encodeCommand(specDeviceId, bytes({0x10}), specCommand, ChecksumMode::Off);

    REQUIRE(result.ok());
    CHECK(result.bytes == bytes({0xF0, 0x47, 0x5E, 0x05, 0x10, 0x0C, 0x1B, 0x35, 0x6D, 0xF7}));
}

TEST_CASE("Given checksum mode unknown, When a command is encoded, Then the checksum is appended [RQ-AKM-041]",
          "[akm][command]")
{
    // The sampler ignores a checksum it does not expect (spec p. 4), so an unknown mode sends one.
    const auto result = encodeCommand(specDeviceId, bytes({0x10}), specCommand, ChecksumMode::Unknown);

    REQUIRE(result.ok());
    CHECK(result.bytes == bytes({0xF0, 0x47, 0x5E, 0x05, 0x10, 0x0C, 0x1B, 0x35, 0x6D, 0x59, 0xF7}));
}

TEST_CASE("Given the spec's tip to turn checksums off on all samplers, When it is encoded, Then the frame is the spec's [RQ-AKM-001]",
          "[akm][command]")
{
    // F0 47 5E 00 00 00 04 00 04 F7: DeviceID 0, user-ref 00, section 00, item 04, data 00, checksum 04.
    const auto result = encodeCommand(0, bytes({0x00}), Command{0x00, 0x04, bytes({0x00})}, ChecksumMode::On);

    REQUIRE(result.ok());
    CHECK(result.bytes == bytes({0xF0, 0x47, 0x5E, 0x00, 0x00, 0x00, 0x04, 0x00, 0x04, 0xF7}));
}

TEST_CASE("Given two user-refs, When encoded, Then bit 5 of the device byte is set and bit 6 clear [RQ-AKM-001]",
          "[akm][command]")
{
    // 10 + 11 + 0C + 1B + 35 + 6D = &EA, AND &7F = &6A.
    const auto result = encodeCommand(specDeviceId, bytes({0x10, 0x11}), specCommand, ChecksumMode::On);

    REQUIRE(result.ok());
    CHECK(result.bytes == bytes({0xF0, 0x47, 0x5E, 0x25, 0x10, 0x11, 0x0C, 0x1B, 0x35, 0x6D, 0x6A, 0xF7}));
}

TEST_CASE("Given three and four user-refs, When encoded, Then the count is held in bits 5 and 6 of the device byte [RQ-AKM-001]",
          "[akm][command]")
{
    const auto three = encodeCommand(specDeviceId, bytes({0x10, 0x11, 0x12}), specCommand, ChecksumMode::Off);
    const auto four = encodeCommand(specDeviceId, bytes({0x10, 0x11, 0x12, 0x13}), specCommand, ChecksumMode::Off);

    REQUIRE(three.ok());
    REQUIRE(four.ok());
    CHECK(three.bytes[3] == 0x45);
    CHECK(four.bytes[3] == 0x65);
    CHECK(three.bytes.size() == 12);
    CHECK(four.bytes.size() == 13);
}

TEST_CASE("Given DeviceID 0 and DeviceID 31, When encoded, Then both are accepted [RQ-AKM-001]", "[akm][command]")
{
    // 0 addresses every sampler (spec p. 4); 31 is the highest DeviceID since OS 1.30.
    const auto broadcast = encodeCommand(0, bytes({0x10}), specCommand, ChecksumMode::Off);
    const auto highest = encodeCommand(31, bytes({0x10}), specCommand, ChecksumMode::Off);

    REQUIRE(broadcast.ok());
    REQUIRE(highest.ok());
    CHECK(broadcast.bytes[3] == 0x00);
    CHECK(highest.bytes[3] == 0x1F);
}

TEST_CASE("Given DeviceID 32, When encoding is requested, Then it is refused and no bytes are produced [RQ-AKM-001]",
          "[akm][command]")
{
    for (const std::uint32_t deviceId : {32u, 127u, 300u})
    {
        const auto result = encodeCommand(deviceId, bytes({0x10}), specCommand, ChecksumMode::On);

        CHECK_FALSE(result.ok());
        CHECK(result.error == EncodeError::InvalidDeviceId);
        CHECK(result.bytes.empty());
    }
}

TEST_CASE("Given no user-ref or five user-refs, When encoding is requested, Then it is refused [RQ-AKM-001]",
          "[akm][command]")
{
    const auto none = encodeCommand(specDeviceId, Bytes{}, specCommand, ChecksumMode::On);
    const auto five = encodeCommand(specDeviceId, bytes({1, 2, 3, 4, 5}), specCommand, ChecksumMode::On);

    CHECK(none.error == EncodeError::InvalidUserRefCount);
    CHECK(none.bytes.empty());
    CHECK(five.error == EncodeError::InvalidUserRefCount);
    CHECK(five.bytes.empty());
}

TEST_CASE("Given a data byte above 7F, When encoding is requested, Then it is refused and no bytes are produced [RQ-AKM-001]",
          "[akm][command]")
{
    const auto data = encodeCommand(specDeviceId, bytes({0x10}), Command{0x0C, 0x1B, bytes({0x35, 0x80})}, ChecksumMode::On);
    const auto userRef = encodeCommand(specDeviceId, bytes({0x80}), specCommand, ChecksumMode::On);
    const auto section = encodeCommand(specDeviceId, bytes({0x10}), Command{0x80, 0x1B, {}}, ChecksumMode::On);
    const auto item = encodeCommand(specDeviceId, bytes({0x10}), Command{0x0C, 0x80, {}}, ChecksumMode::On);

    for (const auto* result : {&data, &userRef, &section, &item})
    {
        CHECK(result->error == EncodeError::InvalidDataByte);
        CHECK(result->bytes.empty());
    }
}

TEST_CASE("Given a command without data, When encoded, Then the frame ends with the item [RQ-AKM-001]",
          "[akm][command]")
{
    const auto result = encodeCommand(specDeviceId, bytes({0x10}), Command{0x00, 0x00, {}}, ChecksumMode::Off);

    REQUIRE(result.ok());
    CHECK(result.bytes == bytes({0xF0, 0x47, 0x5E, 0x05, 0x10, 0x00, 0x00, 0xF7}));
}

TEST_CASE("Given the largest legal values, When encoded in every checksum mode, Then every byte between 5E and F7 is at most 7F [RQ-AKM-001]",
          "[akm][command]")
{
    const Command largest{0x7F, 0x7F, Bytes(16, 0x7F)};

    for (const ChecksumMode mode : {ChecksumMode::On, ChecksumMode::Off, ChecksumMode::Unknown})
    {
        const auto result = encodeCommand(31, bytes({0x7F, 0x7F, 0x7F, 0x7F}), largest, mode);

        REQUIRE(result.ok());
        REQUIRE(result.bytes.size() > 5);
        CHECK(result.bytes.back() == 0xF7);
        for (std::size_t index = 3; index + 1 < result.bytes.size(); ++index)
            CHECK(result.bytes[index] <= 0x7F);
    }
}
