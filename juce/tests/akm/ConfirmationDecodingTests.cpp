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

// Decoding of a received SysEx message: F0 47 5E <dev> <user-ref...> <reply ID> <section> <item>
// <data...> [<checksum>] F7 (spec pp. 5-6), in each of the three checksum modes.
// Frames are constructed from the spec's Tables 1 and 2 until captured ones replace them (RQ-AKM-017).
// [TASK-AKM-004, RQ-AKM-003, RQ-AKM-004, RQ-AKM-006, RQ-AKM-041, ADR-AKM-001 (DEC-AKM-002, DEC-AKM-009)]
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <variant>

#include "TestBytes.hpp"
#include "akm/Confirmation.hpp"

using akm::ChecksumMode;
using akm::Confirmation;
using akm::DecodedMessage;
using akm::RejectReason;
using akm::ReplyId;
using akm::test::Bytes;
using akm::test::bytes;

namespace
{
    // Frames, all with DeviceID 01 and user-ref 10 unless stated. The checksums are computed by hand
    // from the bytes between the first user-ref and the last data byte (Reply ID included), in the
    // comment next to each frame, so that they do not depend on the code under test.

    // DONE §00 item 04, no data. Checksum: 10 + 44 + 00 + 04 = 58.
    const Bytes doneFrame = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x44, 0x00, 0x04, 0xF7});
    const Bytes doneFrameChecksummed = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x44, 0x00, 0x04, 0x58, 0xF7});

    // REPLY §0A item 05 with data 01 02, the example of RQ-AKM-003. Checksum: 10 + 52 + 0A + 05 + 01 + 02 = 74.
    const Bytes replyFrame = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x52, 0x0A, 0x05, 0x01, 0x02, 0xF7});
    const Bytes replyFrameChecksummed = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x52, 0x0A, 0x05, 0x01, 0x02, 0x74, 0xF7});

    // ERROR §0A item 07 with error number 385 (data 03 01). Checksum: 10 + 45 + 0A + 07 + 03 + 01 = 6A.
    const Bytes errorFrame = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x45, 0x0A, 0x07, 0x03, 0x01, 0xF7});
    const Bytes errorFrameChecksummed = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x45, 0x0A, 0x07, 0x03, 0x01, 0x6A, 0xF7});

    // REPLY of the Echo Message, §00 item 06, data 01 02 03 04. Checksum: 10 + 52 + 00 + 06 + 01 + 02 + 03 + 04 = 72.
    const Bytes echoFrame = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x52, 0x00, 0x06, 0x01, 0x02, 0x03, 0x04, 0xF7});
    const Bytes echoFrameChecksummed = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x52, 0x00, 0x06, 0x01, 0x02, 0x03, 0x04, 0x72, 0xF7});

    // The Still Alive null message, spec Table 5 footnote b.
    const Bytes stillAliveFrame = bytes({0xF0, 0xF7});

    const Confirmation* confirmationOf(const DecodedMessage& decoded)
    {
        return std::get_if<Confirmation>(&decoded);
    }

    // Decoded value of a frame that must be a confirmation.
    Confirmation decodeConfirmation(const Bytes& frame, ChecksumMode mode)
    {
        const DecodedMessage decoded = akm::decodeMessage(frame, mode);
        const Confirmation* confirmation = confirmationOf(decoded);
        REQUIRE(confirmation != nullptr);
        return *confirmation;
    }

    // Reason of a frame that must be rejected.
    RejectReason rejectionOf(const Bytes& frame, ChecksumMode mode)
    {
        const DecodedMessage decoded = akm::decodeMessage(frame, mode);
        const auto* rejected = std::get_if<akm::Rejected>(&decoded);
        REQUIRE(rejected != nullptr);
        return rejected->reason;
    }
}

TEST_CASE("Given a DONE frame with DeviceID 01, user-ref 10, section 00, item 04 and no data, When decoded, Then every field is exposed [RQ-AKM-004]",
          "[akm][confirmation]")
{
    const Confirmation confirmation = decodeConfirmation(doneFrame, ChecksumMode::Off);

    CHECK(confirmation.replyId == ReplyId::Done);
    CHECK(confirmation.deviceId == 1);
    CHECK(confirmation.userRefs == bytes({0x10}));
    CHECK(confirmation.section == 0x00);
    CHECK(confirmation.item == 0x04);
    CHECK(confirmation.data.empty());
}

TEST_CASE("Given a Reply ID of each of the four kinds, When decoded, Then each is classified correctly [RQ-AKM-004]",
          "[akm][confirmation]")
{
    const auto frameWith = [](unsigned int replyId) {
        return bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, replyId, 0x00, 0x04, 0xF7});
    };

    CHECK(decodeConfirmation(frameWith(0x4F), ChecksumMode::Off).replyId == ReplyId::Ok);
    CHECK(decodeConfirmation(frameWith(0x44), ChecksumMode::Off).replyId == ReplyId::Done);
    CHECK(decodeConfirmation(frameWith(0x52), ChecksumMode::Off).replyId == ReplyId::Reply);
    CHECK(decodeConfirmation(frameWith(0x45), ChecksumMode::Off).replyId == ReplyId::Error);
}

TEST_CASE("Given a device byte whose bits 5-6 are 01, When decoded, Then two user-refs are read and the DeviceID is bits 0-4 [RQ-AKM-004]",
          "[akm][confirmation]")
{
    const Bytes frame = bytes({0xF0, 0x47, 0x5E, 0x25, 0x10, 0x11, 0x44, 0x00, 0x04, 0xF7});

    const Confirmation confirmation = decodeConfirmation(frame, ChecksumMode::Off);

    CHECK(confirmation.deviceId == 5);
    CHECK(confirmation.userRefs == bytes({0x10, 0x11}));
    CHECK(confirmation.replyId == ReplyId::Done);
    CHECK(confirmation.section == 0x00);
    CHECK(confirmation.item == 0x04);
}

TEST_CASE("Given a device byte whose bits 5-6 are 10 and 11, When decoded, Then three and four user-refs are read [RQ-AKM-004]",
          "[akm][confirmation]")
{
    const Bytes three = bytes({0xF0, 0x47, 0x5E, 0x41, 0x10, 0x11, 0x12, 0x44, 0x00, 0x04, 0xF7});
    const Bytes four = bytes({0xF0, 0x47, 0x5E, 0x61, 0x10, 0x11, 0x12, 0x13, 0x44, 0x00, 0x04, 0xF7});

    CHECK(decodeConfirmation(three, ChecksumMode::Off).userRefs == bytes({0x10, 0x11, 0x12}));
    CHECK(decodeConfirmation(four, ChecksumMode::Off).userRefs == bytes({0x10, 0x11, 0x12, 0x13}));
}

TEST_CASE("Given checksum OFF and a REPLY 52 0A 05 01 02, When decoded, Then the data is 01 02 [RQ-AKM-003]",
          "[akm][confirmation]")
{
    const Confirmation confirmation = decodeConfirmation(replyFrame, ChecksumMode::Off);

    CHECK(confirmation.replyId == ReplyId::Reply);
    CHECK(confirmation.section == 0x0A);
    CHECK(confirmation.item == 0x05);
    CHECK(confirmation.data == bytes({0x01, 0x02}));
}

TEST_CASE("Given checksum ON and the same REPLY with a valid checksum appended, When decoded, Then the data is 01 02 [RQ-AKM-003]",
          "[akm][confirmation]")
{
    const Confirmation confirmation = decodeConfirmation(replyFrameChecksummed, ChecksumMode::On);

    CHECK(confirmation.data == bytes({0x01, 0x02}));
}

TEST_CASE("Given checksum OFF and a trailing byte that looks like a checksum, When decoded, Then it is data [RQ-AKM-003]",
          "[akm][confirmation]")
{
    // Every byte between the item and F7 is data when the mode is off.
    const Confirmation confirmation = decodeConfirmation(replyFrameChecksummed, ChecksumMode::Off);

    CHECK(confirmation.data == bytes({0x01, 0x02, 0x74}));
}

TEST_CASE("Given checksum ON and a wrong checksum, When decoded, Then it is rejected [RQ-AKM-003, RQ-AKM-006]",
          "[akm][confirmation]")
{
    const Bytes wrong = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x52, 0x0A, 0x05, 0x01, 0x02, 0x75, 0xF7});

    CHECK(rejectionOf(wrong, ChecksumMode::On) == RejectReason::BadChecksum);
}

TEST_CASE("Given checksum ON and a frame without any checksum, When decoded, Then it is rejected [RQ-AKM-003, RQ-AKM-006]",
          "[akm][confirmation]")
{
    // Nothing follows the item: no byte can be the checksum. Also what a sampler with checksums off sends.
    CHECK(rejectionOf(doneFrame, ChecksumMode::On) == RejectReason::BadChecksum);
}

TEST_CASE("Given checksum ON and a DONE with a valid checksum, When decoded, Then the data is empty [RQ-AKM-003]",
          "[akm][confirmation]")
{
    const Confirmation confirmation = decodeConfirmation(doneFrameChecksummed, ChecksumMode::On);

    CHECK(confirmation.replyId == ReplyId::Done);
    CHECK(confirmation.data.empty());
}

TEST_CASE("Given the mode unknown and a DONE without a checksum, When decoded, Then it is a DONE [RQ-AKM-041]",
          "[akm][confirmation]")
{
    const Confirmation confirmation = decodeConfirmation(doneFrame, ChecksumMode::Unknown);

    CHECK(confirmation.replyId == ReplyId::Done);
    CHECK(confirmation.data.empty());
}

TEST_CASE("Given the mode unknown and the same DONE with a valid checksum byte appended, When decoded, Then it is a DONE [RQ-AKM-041]",
          "[akm][confirmation]")
{
    const Confirmation confirmation = decodeConfirmation(doneFrameChecksummed, ChecksumMode::Unknown);

    CHECK(confirmation.replyId == ReplyId::Done);
    CHECK(confirmation.data.empty());
}

TEST_CASE("Given the mode unknown and the same DONE with a wrong extra byte, When decoded, Then it is rejected [RQ-AKM-041, RQ-AKM-006]",
          "[akm][confirmation]")
{
    const Bytes wrong = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x44, 0x00, 0x04, 0x59, 0xF7});

    CHECK(rejectionOf(wrong, ChecksumMode::Unknown) == RejectReason::BadChecksum);
}

TEST_CASE("Given the mode unknown and a DONE with two extra bytes, When decoded, Then it is rejected [RQ-AKM-041, RQ-AKM-006]",
          "[akm][confirmation]")
{
    const Bytes twoExtra = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x44, 0x00, 0x04, 0x01, 0x02, 0xF7});

    CHECK(rejectionOf(twoExtra, ChecksumMode::Unknown) == RejectReason::BadDataLength);
}

TEST_CASE("Given the mode unknown and an OK, When decoded with and without a checksum, Then it is an OK with no data [RQ-AKM-041]",
          "[akm][confirmation]")
{
    // OK carries no data either. Checksum: 10 + 4F + 00 + 04 = 63.
    const Bytes plain = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x4F, 0x00, 0x04, 0xF7});
    const Bytes checksummed = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x4F, 0x00, 0x04, 0x63, 0xF7});

    CHECK(decodeConfirmation(plain, ChecksumMode::Unknown).replyId == ReplyId::Ok);
    CHECK(decodeConfirmation(checksummed, ChecksumMode::Unknown).replyId == ReplyId::Ok);
    CHECK(decodeConfirmation(checksummed, ChecksumMode::Unknown).data.empty());
}

TEST_CASE("Given the mode unknown and an ERROR, When decoded with and without a checksum, Then its two data bytes are kept [RQ-AKM-041, RQ-AKM-005]",
          "[akm][confirmation]")
{
    CHECK(decodeConfirmation(errorFrame, ChecksumMode::Unknown).data == bytes({0x03, 0x01}));
    CHECK(decodeConfirmation(errorFrameChecksummed, ChecksumMode::Unknown).data == bytes({0x03, 0x01}));
}

TEST_CASE("Given the mode unknown and the REPLY of an Echo, When decoded with and without a checksum, Then its four data bytes are kept [RQ-AKM-041]",
          "[akm][confirmation]")
{
    CHECK(decodeConfirmation(echoFrame, ChecksumMode::Unknown).data == bytes({0x01, 0x02, 0x03, 0x04}));
    CHECK(decodeConfirmation(echoFrameChecksummed, ChecksumMode::Unknown).data == bytes({0x01, 0x02, 0x03, 0x04}));
}

TEST_CASE("Given the mode unknown and a REPLY of variable length, When decoded, Then it is rejected [RQ-AKM-041]",
          "[akm][confirmation]")
{
    // Its length cannot tell a checksum from data: the session refuses such commands, and a stray
    // one is discarded.
    CHECK(rejectionOf(replyFrame, ChecksumMode::Unknown) == RejectReason::UnknownDataLength);
    CHECK(rejectionOf(replyFrameChecksummed, ChecksumMode::Unknown) == RejectReason::UnknownDataLength);
}

TEST_CASE("Given the Still Alive message F0 F7, When decoded in any checksum mode, Then it is not malformed [RQ-AKM-006]",
          "[akm][confirmation]")
{
    for (const ChecksumMode mode : {ChecksumMode::On, ChecksumMode::Off, ChecksumMode::Unknown})
    {
        const DecodedMessage decoded = akm::decodeMessage(stillAliveFrame, mode);

        CHECK(std::holds_alternative<akm::StillAliveMessage>(decoded));
    }
}

TEST_CASE("Given a message from another manufacturer, When decoded, Then it is rejected as foreign [RQ-AKM-006]",
          "[akm][confirmation]")
{
    const Bytes otherManufacturer = bytes({0xF0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xF7});
    const Bytes otherModel = bytes({0xF0, 0x47, 0x5F, 0x01, 0x10, 0x44, 0x00, 0x04, 0xF7});
    const Bytes channelMessage = bytes({0x90, 0x3C, 0x7F});

    for (const ChecksumMode mode : {ChecksumMode::On, ChecksumMode::Off, ChecksumMode::Unknown})
    {
        CHECK(rejectionOf(otherManufacturer, mode) == RejectReason::Foreign);
        CHECK(rejectionOf(otherModel, mode) == RejectReason::Foreign);
        CHECK(rejectionOf(channelMessage, mode) == RejectReason::Foreign);
    }
}

TEST_CASE("Given a Reply ID of 58, When decoded, Then it is rejected as an unknown reply ID [RQ-AKM-006]",
          "[akm][confirmation]")
{
    const Bytes frame = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x58, 0x00, 0x04, 0xF7});

    CHECK(rejectionOf(frame, ChecksumMode::Off) == RejectReason::UnknownReplyId);
}

TEST_CASE("Given a message shorter than the minimum confirmation, When decoded, Then it is rejected as truncated [RQ-AKM-006]",
          "[akm][confirmation]")
{
    // F0 47 5E <dev> <user-ref> <reply> <section> <item> F7 is the shortest one: 9 bytes.
    const Bytes noItem = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x44, 0x00, 0xF7});
    const Bytes noEnd = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x44, 0x00, 0x04});
    const Bytes headerOnly = bytes({0xF0, 0x47, 0x5E, 0xF7});
    // The device byte announces four user-refs, but the message is too short to hold them.
    const Bytes missingUserRefs = bytes({0xF0, 0x47, 0x5E, 0x61, 0x10, 0x11, 0x44, 0x00, 0x04, 0xF7});

    CHECK(rejectionOf(noItem, ChecksumMode::Off) == RejectReason::Truncated);
    CHECK(rejectionOf(noEnd, ChecksumMode::Off) == RejectReason::Truncated);
    CHECK(rejectionOf(headerOnly, ChecksumMode::Off) == RejectReason::Truncated);
    CHECK(rejectionOf(missingUserRefs, ChecksumMode::Off) == RejectReason::Truncated);
    CHECK(rejectionOf(Bytes{}, ChecksumMode::Off) == RejectReason::Truncated);
}

TEST_CASE("Given a byte above 7F inside the message, When decoded, Then it is rejected as an invalid data byte [RQ-AKM-006]",
          "[akm][confirmation]")
{
    const Bytes frame = bytes({0xF0, 0x47, 0x5E, 0x01, 0x10, 0x44, 0x00, 0x84, 0xF7});

    CHECK(rejectionOf(frame, ChecksumMode::Off) == RejectReason::InvalidDataByte);
}

TEST_CASE("Given every prefix of a valid frame, When decoded in any checksum mode, Then it is rejected without throwing [RQ-AKM-006]",
          "[akm][confirmation]")
{
    for (const Bytes* frame : {&doneFrame, &replyFrameChecksummed, &errorFrame, &echoFrameChecksummed})
    {
        for (const ChecksumMode mode : {ChecksumMode::On, ChecksumMode::Off, ChecksumMode::Unknown})
        {
            for (std::size_t length = 0; length < frame->size(); ++length)
            {
                const std::span<const std::uint8_t> prefix(frame->data(), length);

                DecodedMessage decoded;
                REQUIRE_NOTHROW(decoded = akm::decodeMessage(prefix, mode));
                CHECK(std::holds_alternative<akm::Rejected>(decoded));
            }
        }
    }
}

TEST_CASE("Given a valid frame with any single byte changed, When decoded in any checksum mode, Then it never throws [RQ-AKM-006]",
          "[akm][confirmation]")
{
    for (const Bytes* original : {&doneFrameChecksummed, &replyFrameChecksummed, &errorFrameChecksummed})
    {
        for (const ChecksumMode mode : {ChecksumMode::On, ChecksumMode::Off, ChecksumMode::Unknown})
        {
            for (std::size_t index = 0; index < original->size(); ++index)
            {
                for (const unsigned int replacement : {0x00u, 0x47u, 0x5Eu, 0x7Fu, 0x80u, 0xF0u, 0xF7u, 0xFFu})
                {
                    Bytes mutated = *original;
                    mutated[index] = static_cast<std::uint8_t>(replacement);

                    REQUIRE_NOTHROW(akm::decodeMessage(mutated, mode));
                }
            }
        }
    }
}

TEST_CASE("Given each rejection reason, When described, Then the text names the reason [RQ-AKM-006]",
          "[akm][confirmation]")
{
    using Catch::Matchers::ContainsSubstring;

    CHECK_THAT(std::string(akm::describe(RejectReason::Foreign)), ContainsSubstring("foreign message"));
    CHECK_THAT(std::string(akm::describe(RejectReason::UnknownReplyId)), ContainsSubstring("unknown reply ID"));
    CHECK_THAT(std::string(akm::describe(RejectReason::Truncated)), ContainsSubstring("truncated"));
    CHECK_THAT(std::string(akm::describe(RejectReason::InvalidDataByte)), ContainsSubstring("data byte"));
    CHECK_THAT(std::string(akm::describe(RejectReason::BadChecksum)), ContainsSubstring("checksum"));
    CHECK_THAT(std::string(akm::describe(RejectReason::UnknownDataLength)), ContainsSubstring("checksum mode unknown"));
    CHECK_THAT(std::string(akm::describe(RejectReason::BadDataLength)), ContainsSubstring("data length"));
}
