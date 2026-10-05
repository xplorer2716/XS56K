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

// The discovery primitives of section 12 on a session and the simulated sampler: whether an FX board is
// installed, the number of FX channels and the number of modules of a channel. The simulated EB20 is laid out
// as the kb gives Figure 2 (`eb20Layout`), a fixture and not a claim about the hardware. [TASK-AKM-101,
// RQ-AKM-099, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <variant>

#include "SessionHarness.hpp"
#include "akm/MultiFxPrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::FxCard;
using akm::FxCardResult;
using akm::FxCountResult;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::ManualScenarioDriver;
using akm::harness::eb20Layout;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;
using akm::test::bytes;

namespace
{
    FxCardResult getCard(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<FxCardResult>>();
        akm::getFxCard(harness.session(), [latched](const FxCardResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    FxCountResult getChannelCount(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<FxCountResult>>();
        akm::getFxChannelCount(harness.session(), [latched](const FxCountResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    FxCountResult getModuleCount(SessionHarness& harness, int channel)
    {
        auto latched = std::make_shared<Latched<FxCountResult>>();
        akm::getFxModuleCount(harness.session(), channel, [latched](const FxCountResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }
}

TEST_CASE("Given a simulated sampler with an EB20 laid out as Figure 2, When the card, the number of channels and the number of modules of each channel are read, Then the card is the EB20, there are 4 channels and the modules number 6, 6, 2, 2 [RQ-AKM-099]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setFxBoard(eb20Layout());

    const FxCardResult card = getCard(harness);
    CHECK(card.card == FxCard::Eb20);
    // The request is section 12, item 01, with no data byte.
    const Bytes cardFrame = harness.sentFrames().back();
    CHECK(cardFrame[akm::test::SENT_SECTION_INDEX] == 0x12);
    CHECK(cardFrame[akm::test::SENT_ITEM_INDEX] == 0x01);

    CHECK(getChannelCount(harness).count == 4);
    CHECK(harness.sentFrames().back()[akm::test::SENT_ITEM_INDEX] == 0x10);

    CHECK(getModuleCount(harness, 0).count == 6);
    CHECK(getModuleCount(harness, 1).count == 6);
    CHECK(getModuleCount(harness, 2).count == 2);
    CHECK(getModuleCount(harness, 3).count == 2);
}

TEST_CASE("Given a module count requested for channel 2, When the frame is read, Then it is item 11 with the channel as its one data byte [RQ-AKM-099]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setFxBoard(eb20Layout());

    CHECK(getModuleCount(harness, 2).count == 2);

    const Bytes frame = harness.sentFrames().back();
    constexpr std::size_t dataStart = akm::test::SENT_ITEM_INDEX + 1;
    CHECK(frame[akm::test::SENT_ITEM_INDEX] == 0x11);
    CHECK(Bytes(frame.begin() + dataStart, frame.begin() + dataStart + 1) == bytes({0x02}));
}

TEST_CASE("Given a simulated sampler with no board, When the card is read, Then it is none, and the channel count is zero, not a failure [RQ-AKM-099]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    const FxCardResult card = getCard(harness);
    REQUIRE(std::holds_alternative<akm::Reply>(card.outcome));
    CHECK(card.card == FxCard::None);

    const FxCountResult channels = getChannelCount(harness);
    REQUIRE(std::holds_alternative<akm::Reply>(channels.outcome));
    CHECK(channels.count == 0);

    // Observed on a real S5000 with no board (TASK-AKM-104): the modules of channel 0 are counted as 0.
    const FxCountResult modules = getModuleCount(harness, 0);
    REQUIRE(std::holds_alternative<akm::Reply>(modules.outcome));
    CHECK(modules.count == 0);
}

TEST_CASE("Given a channel the board does not have, When its modules are counted, Then the sampler answers a REPLY of 0, as the real S5000 does with no board [RQ-AKM-099]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setFxBoard(eb20Layout());

    const FxCountResult result = getModuleCount(harness, 4);
    REQUIRE(std::holds_alternative<akm::Reply>(result.outcome));
    CHECK(result.count == 0);
}

TEST_CASE("Given a channel of 128, When its modules are counted, Then the request is refused as ArgumentOutOfRange without sending [RQ-AKM-099]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const auto sentBefore = harness.sentCount();

    for (const int channel : {128, -1})
    {
        const FxCountResult result = getModuleCount(harness, channel);
        REQUIRE(std::holds_alternative<Refused>(result.outcome));
        CHECK(std::get<Refused>(result.outcome).reason == RefusalReason::ArgumentOutOfRange);
        CHECK_FALSE(result.count.has_value());
    }
    CHECK(harness.sentCount() == sentBefore);
}

TEST_CASE("Given a card code the layer cannot name, When the card is read, Then the REPLY is not given as a card [RQ-AKM-099]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setFxCardCode(5);

    const FxCardResult card = getCard(harness);
    REQUIRE(std::holds_alternative<akm::Reply>(card.outcome));
    CHECK_FALSE(card.card.has_value());
}
