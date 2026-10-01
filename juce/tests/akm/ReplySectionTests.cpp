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

// A REPLY that carries another section than the command's, for the one item where the real S5000 does it — Get
// Clock Time & Date (§02/&05), whose REPLY carries section 0B, observed on the hardware (TASK-AKM-053, log
// akm-suite-20261001-215601.log): the catalogue records it, the codec reads the REPLY in every checksum mode and the
// session matches it to its command, for that item and for no other. [TASK-AKM-055, RQ-AKM-059, RQ-AKM-007,
// RQ-AKM-041, ADR-AKM-001 (DEC-AKM-012, DEC-AKM-016)]
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>

#include "SessionHarness.hpp"
#include "TestBytes.hpp"
#include "akm/Confirmation.hpp"
#include "akm/ItemCatalogue.hpp"
#include "akm/SystemSetup.hpp"

using akm::ChecksumMode;
using akm::ClockDate;
using akm::ClockDateResult;
using akm::Confirmation;
using akm::DecodedMessage;
using akm::ItemId;
using akm::RejectReason;
using akm::ReplyId;
using akm::SamplerNameResult;
using akm::Timeout;
using akm::harness::ManualScenarioDriver;
using akm::harness::ReplySectionOverride;
using akm::harness::SamplerBehaviour;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;
using akm::test::bytes;

namespace
{
    constexpr std::uint8_t SECTION_SYSTEM = 0x02;
    constexpr std::uint8_t ITEM_CLOCK = 0x05;
    constexpr std::uint8_t ITEM_NAME = 0x03;
    constexpr std::uint8_t ITEM_MODEL = 0x04;
    // The section byte of the REPLY of the clock on the S5000, and one that no item declares.
    constexpr std::uint8_t CLOCK_REPLY_SECTION = 0x0B;
    constexpr std::uint8_t UNDECLARED_SECTION = 0x0C;
    constexpr std::size_t CLOCK_REPLY_DATA_SIZE = 8;

    // What the S5000 (OS 2.14) sent for Get Clock Time & Date, as captured: user-ref 07, REPLY, section 0B, item 05,
    // then 2026-10-02 (day of week 6) 00:13:44.
    const Bytes CAPTURED_CLOCK_REPLY =
        bytes({0xF0, 0x47, 0x5E, 0x00, 0x07, 0x52, 0x0B, 0x05, 0x0F, 0x6A, 0x0A, 0x02, 0x06, 0x00, 0x0D, 0x2C, 0xF7});
    // The same REPLY for an item the catalogue does not say answers under another section: the model (&04), one byte.
    const Bytes MODEL_REPLY_UNDER_0B = bytes({0xF0, 0x47, 0x5E, 0x00, 0x07, 0x52, 0x0B, 0x04, 0x00, 0xF7});

    constexpr ClockDate THURSDAY{2026, 10, 1, 5, 14, 30, 15};

    template <typename Result, typename Ask>
    Result await(SessionHarness& harness, Ask ask)
    {
        auto latched = std::make_shared<Latched<Result>>();
        ask([latched](const Result& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    ClockDateResult getClock(SessionHarness& harness)
    {
        return await<ClockDateResult>(harness, [&](auto done) { akm::getClockDate(harness.session(), done); });
    }

    SamplerNameResult getName(SessionHarness& harness)
    {
        return await<SamplerNameResult>(harness, [&](auto done) { akm::getSamplerName(harness.session(), done); });
    }

    void answerUnder(SessionHarness& harness, std::uint8_t item, std::uint8_t replySection)
    {
        SamplerBehaviour behaviour;
        behaviour.replySectionOverrides = {ReplySectionOverride{SECTION_SYSTEM, item, replySection}};
        harness.sampler().setBehaviour(behaviour);
    }
}

TEST_CASE("Given the catalogue, When the REPLY section of each item is asked, Then only Get Clock Time and Date names one, 0B, and a REPLY lookup finds it under either section while a command lookup does not find 0B [RQ-AKM-059]",
          "[akm][reply-section]")
{
    std::size_t declaring = 0;
    for (const akm::ItemDescriptor& record : akm::ITEM_TABLE)
    {
        if (record.replySection)
            ++declaring;
    }
    CHECK(declaring == 1);
    CHECK(akm::descriptor(ItemId::SystemGetClock).replySection == std::optional<std::uint8_t>(CLOCK_REPLY_SECTION));

    CHECK(akm::findReplyItem(CLOCK_REPLY_SECTION, ITEM_CLOCK) == &akm::descriptor(ItemId::SystemGetClock));
    CHECK(akm::findReplyItem(SECTION_SYSTEM, ITEM_CLOCK) == &akm::descriptor(ItemId::SystemGetClock));
    CHECK(akm::findReplyItem(CLOCK_REPLY_SECTION, ITEM_NAME) == nullptr);
    CHECK(akm::findItem(CLOCK_REPLY_SECTION, ITEM_CLOCK) == nullptr);
}

TEST_CASE("Given the REPLY the S5000 sent to Get Clock Time and Date, When it is decoded in each checksum mode, Then its eight data bytes are read, the checksum mode unknown included [RQ-AKM-059, RQ-AKM-041]",
          "[akm][reply-section]")
{
    for (const ChecksumMode mode : {ChecksumMode::Off, ChecksumMode::Unknown})
    {
        const DecodedMessage decoded = akm::decodeMessage(CAPTURED_CLOCK_REPLY, mode);
        const Confirmation* confirmation = std::get_if<Confirmation>(&decoded);
        REQUIRE(confirmation != nullptr);
        CHECK(confirmation->replyId == ReplyId::Reply);
        CHECK(confirmation->section == CLOCK_REPLY_SECTION);
        CHECK(confirmation->item == ITEM_CLOCK);
        CHECK(confirmation->data.size() == CLOCK_REPLY_DATA_SIZE);
    }
}

TEST_CASE("Given a REPLY under section 0B for an item that declares no such section, When it is decoded with the checksum mode unknown, Then it is refused as of an unknown length [RQ-AKM-059, RQ-AKM-041]",
          "[akm][reply-section]")
{
    const DecodedMessage decoded = akm::decodeMessage(MODEL_REPLY_UNDER_0B, ChecksumMode::Unknown);

    const auto* rejected = std::get_if<akm::Rejected>(&decoded);
    REQUIRE(rejected != nullptr);
    CHECK(rejected->reason == RejectReason::UnknownDataLength);
}

TEST_CASE("Given a sampler that answers the clock under section 0B as the S5000 does, When the clock is set then read, Then the value read is the value set [RQ-AKM-059, RQ-AKM-007]",
          "[akm][reply-section]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    akm::setClockDate(harness.session(), THURSDAY, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));

    const ClockDateResult result = getClock(harness);
    CHECK(result.clock == THURSDAY);
    CHECK(std::holds_alternative<akm::Reply>(result.outcome));
}

TEST_CASE("Given a sampler that answers the clock under section 02 as the spec says, When the clock is read, Then it is read all the same [RQ-AKM-059, RQ-AKM-007]",
          "[akm][reply-section]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    SamplerBehaviour conformant;
    conformant.replySectionOverrides.clear();
    harness.sampler().setBehaviour(conformant);

    CHECK(getClock(harness).clock.has_value());
}

TEST_CASE("Given a sampler that answers the clock under a section the catalogue does not declare, When the clock is read, Then the REPLY completes nothing and the command times out [RQ-AKM-059, RQ-AKM-007]",
          "[akm][reply-section]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    answerUnder(harness, ITEM_CLOCK, UNDECLARED_SECTION);

    const ClockDateResult result = getClock(harness);
    CHECK_FALSE(result.clock.has_value());
    CHECK(std::holds_alternative<Timeout>(result.outcome));
}

TEST_CASE("Given a sampler that answers the name under section 0B, When the name is read, Then the REPLY is not taken for the answer, since the name declares no other section [RQ-AKM-059, RQ-AKM-007]",
          "[akm][reply-section]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    answerUnder(harness, ITEM_NAME, CLOCK_REPLY_SECTION);

    const SamplerNameResult result = getName(harness);
    CHECK_FALSE(result.name.has_value());
    CHECK(std::holds_alternative<Timeout>(result.outcome));
}
