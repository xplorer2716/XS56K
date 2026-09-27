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

// Frames captured from a real S5000 running OS 2.14 by the first-contact probe (TASK-AKM-012, run by the
// owner on 2026-09-26, log in process/2.architecture/OBSERVATIONS-RQ-AKM-017-first-contact.md). They replace
// the constructed frames of the acceptance criteria of RQ-AKM-004 and pin down what the codec must read.
// [TASK-AKM-012, RQ-AKM-003, RQ-AKM-004, RQ-AKM-005, RQ-AKM-017, RQ-AKM-041, ADR-AKM-001 (DEC-AKM-009)]
#include <catch2/catch_test_macros.hpp>

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
    Confirmation confirmationOf(const Bytes& frame, ChecksumMode mode)
    {
        const DecodedMessage decoded = akm::decodeMessage(frame, mode);
        const Confirmation* confirmation = std::get_if<Confirmation>(&decoded);
        REQUIRE(confirmation != nullptr);
        return *confirmation;
    }

    bool isRejected(const Bytes& frame, ChecksumMode mode, RejectReason reason)
    {
        const DecodedMessage decoded = akm::decodeMessage(frame, mode);
        const auto* rejected = std::get_if<akm::Rejected>(&decoded);
        return rejected != nullptr && rejected->reason == reason;
    }

    // Checksums off, as the sampler was found (steps 1 to 7 of the probe).
    const Bytes okChecksumOff = bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x4F, 0x00, 0x04, 0xF7});
    const Bytes doneChecksumOff = bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x44, 0x00, 0x04, 0xF7});
    const Bytes osVersionReply = bytes({0xF0, 0x47, 0x5E, 0x00, 0x13, 0x52, 0x02, 0x00, 0x02, 0x0E, 0xF7});
    const Bytes osSubVersionReply = bytes({0xF0, 0x47, 0x5E, 0x00, 0x14, 0x52, 0x02, 0x01, 0x00, 0xF7});
    const Bytes echoReply = bytes({0xF0, 0x47, 0x5E, 0x00, 0x15, 0x52, 0x00, 0x06, 0x01, 0x02, 0x03, 0x04, 0xF7});

    // The DONE of the command that switched checksums on already carries one (5F = 17 + 44 + 00 + 04).
    const Bytes doneSwitchingOn = bytes({0xF0, 0x47, 0x5E, 0x00, 0x17, 0x44, 0x00, 0x04, 0x5F, 0xF7});

    // Checksums on: every confirmation carries one, the OK and the ERROR included.
    const Bytes okChecksumOn = bytes({0xF0, 0x47, 0x5E, 0x00, 0x18, 0x4F, 0x00, 0x00, 0x67, 0xF7});
    const Bytes doneChecksumOn = bytes({0xF0, 0x47, 0x5E, 0x00, 0x18, 0x44, 0x00, 0x00, 0x5C, 0xF7});
    const Bytes echoReplyChecksumOn = bytes({0xF0, 0x47, 0x5E, 0x00, 0x19, 0x52, 0x00, 0x06, 0x01, 0x02, 0x03, 0x04, 0x7B, 0xF7});
    // The answer to a command without checksum while they are on: ERROR 129, after an OK.
    const Bytes errorChecksumInvalid = bytes({0xF0, 0x47, 0x5E, 0x00, 0x1A, 0x45, 0x00, 0x00, 0x01, 0x01, 0x61, 0xF7});

    // The command that switched checksums off: its OK still carries one, its DONE has none.
    const Bytes okSwitchingOff = bytes({0xF0, 0x47, 0x5E, 0x00, 0x1B, 0x4F, 0x00, 0x04, 0x6E, 0xF7});
    const Bytes doneSwitchingOff = bytes({0xF0, 0x47, 0x5E, 0x00, 0x1B, 0x44, 0x00, 0x04, 0xF7});
}

TEST_CASE("Given the OK and DONE captured with checksums off, When decoded, Then they carry the DeviceID 0, the user-ref, section 00 and item 04 [RQ-AKM-004, RQ-AKM-017]",
          "[akm][captured]")
{
    const Confirmation ok = confirmationOf(okChecksumOff, ChecksumMode::Off);
    const Confirmation done = confirmationOf(doneChecksumOff, ChecksumMode::Off);

    CHECK(ok.replyId == ReplyId::Ok);
    CHECK(done.replyId == ReplyId::Done);
    for (const Confirmation& confirmation : {ok, done})
    {
        CHECK(confirmation.deviceId == 0);
        CHECK(confirmation.userRefs == bytes({0x10}));
        CHECK(confirmation.section == 0x00);
        CHECK(confirmation.item == 0x04);
        CHECK(confirmation.data.empty());
    }
}

TEST_CASE("Given the OS version REPLY captured from the S5000, When decoded, Then it is 2.14 and the sub-version 0 [RQ-AKM-004, RQ-AKM-044]",
          "[akm][captured]")
{
    const Confirmation version = confirmationOf(osVersionReply, ChecksumMode::Off);
    const Confirmation subVersion = confirmationOf(osSubVersionReply, ChecksumMode::Off);

    CHECK(version.replyId == ReplyId::Reply);
    CHECK(version.section == 0x02);
    CHECK(version.item == 0x00);
    CHECK(version.data == bytes({0x02, 0x0E}));
    CHECK(subVersion.data == bytes({0x00}));
}

TEST_CASE("Given the Echo REPLY captured with checksums off, When decoded in mode off and unknown, Then the four bytes come back [RQ-AKM-004, RQ-AKM-041]",
          "[akm][captured]")
{
    CHECK(confirmationOf(echoReply, ChecksumMode::Off).data == bytes({0x01, 0x02, 0x03, 0x04}));
    CHECK(confirmationOf(echoReply, ChecksumMode::Unknown).data == bytes({0x01, 0x02, 0x03, 0x04}));
}

TEST_CASE("Given the confirmations captured with checksums on, When decoded in mode on and unknown, Then the checksum is verified and stripped, the Reply ID included in it [RQ-AKM-003, RQ-AKM-017]",
          "[akm][captured]")
{
    for (const ChecksumMode mode : {ChecksumMode::On, ChecksumMode::Unknown})
    {
        CHECK(confirmationOf(okChecksumOn, mode).replyId == ReplyId::Ok);
        CHECK(confirmationOf(okChecksumOn, mode).data.empty());
        CHECK(confirmationOf(doneChecksumOn, mode).replyId == ReplyId::Done);
        CHECK(confirmationOf(echoReplyChecksumOn, mode).data == bytes({0x01, 0x02, 0x03, 0x04}));
    }
}

TEST_CASE("Given the ERROR captured for a command without checksum while they are on, When decoded, Then it is ERROR 129 in mode on and unknown [RQ-AKM-005, RQ-AKM-017]",
          "[akm][captured]")
{
    for (const ChecksumMode mode : {ChecksumMode::On, ChecksumMode::Unknown})
    {
        const Confirmation error = confirmationOf(errorChecksumInvalid, mode);

        CHECK(error.replyId == ReplyId::Error);
        CHECK(akm::errorNumber(error) == 129);
        CHECK(error.data == bytes({0x01, 0x01}));
        CHECK(error.section == 0x00);
        CHECK(error.item == 0x00);
    }
}

TEST_CASE("Given the confirmations of the command that switched checksums on, When decoded in mode unknown, Then the OK without and the DONE with a checksum both decode [RQ-AKM-041, ADR-AKM-001 (DEC-AKM-009)]",
          "[akm][captured]")
{
    // Its OK goes out before the command runs (no checksum), its DONE after (a checksum).
    CHECK(confirmationOf(okChecksumOff, ChecksumMode::Unknown).replyId == ReplyId::Ok);
    const Confirmation done = confirmationOf(doneSwitchingOn, ChecksumMode::Unknown);

    CHECK(done.replyId == ReplyId::Done);
    CHECK(done.data.empty());
    // Read with the mode still off, the checksum would be taken for data: the reason for mode unknown.
    CHECK(confirmationOf(doneSwitchingOn, ChecksumMode::Off).data == bytes({0x5F}));
}

TEST_CASE("Given the confirmations of the command that switched checksums off, When decoded in mode on, Then the DONE is rejected, and in mode unknown both decode [RQ-AKM-041, ADR-AKM-001 (DEC-AKM-009)]",
          "[akm][captured]")
{
    // Its OK still carries a checksum (sent under the old mode), its DONE has none (the new mode).
    CHECK(confirmationOf(okSwitchingOff, ChecksumMode::On).replyId == ReplyId::Ok);
    CHECK(isRejected(doneSwitchingOff, ChecksumMode::On, RejectReason::BadChecksum));

    CHECK(confirmationOf(okSwitchingOff, ChecksumMode::Unknown).replyId == ReplyId::Ok);
    CHECK(confirmationOf(doneSwitchingOff, ChecksumMode::Unknown).replyId == ReplyId::Done);
}
