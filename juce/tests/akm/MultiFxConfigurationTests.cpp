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

// The configuration primitives of section 12 on a session and the simulated sampler: the mute status of a
// channel, the type of a module and its enabled state, each as a Set and a Get. The effects belong to the
// current multi, so a multi is made current first. [TASK-AKM-102, RQ-AKM-100, ADR-AKM-001 (DEC-AKM-003,
// DEC-AKM-012)]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <variant>

#include "SessionHarness.hpp"
#include "akm/MultiFxPrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::CommandResult;
using akm::Done;
using akm::Error;
using akm::FxEnabledResult;
using akm::FxModuleType;
using akm::FxModuleTypeResult;
using akm::FxMuteResult;
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
    constexpr std::uint8_t SECTION_MULTI_FX = 0x12;
    constexpr std::size_t DATA_START = akm::test::SENT_ITEM_INDEX + 1;

    // A sampler with an EB20 and one multi, which is current: the state in which a multi's effects can be reached.
    void seedEb20OnACurrentMulti(SessionHarness& harness)
    {
        harness.sampler().setMultiNames({"FX MULTI"});
        harness.sampler().setCurrentMulti(0);
        harness.sampler().setFxBoard(eb20Layout());
    }

    Bytes dataOfLastFrame(SessionHarness& harness, std::size_t length)
    {
        const Bytes frame = harness.sentFrames().back();
        return Bytes(frame.begin() + DATA_START, frame.begin() + DATA_START + static_cast<std::ptrdiff_t>(length));
    }

    FxMuteResult getMute(SessionHarness& harness, int channel)
    {
        auto latched = std::make_shared<Latched<FxMuteResult>>();
        akm::getFxChannelMute(harness.session(), channel, [latched](const FxMuteResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    FxModuleTypeResult getType(SessionHarness& harness, int channel, int module)
    {
        auto latched = std::make_shared<Latched<FxModuleTypeResult>>();
        akm::getFxModuleType(harness.session(), channel, module, [latched](const FxModuleTypeResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    FxEnabledResult getEnabled(SessionHarness& harness, int channel, int module)
    {
        auto latched = std::make_shared<Latched<FxEnabledResult>>();
        akm::getFxModuleEnabled(harness.session(), channel, module, [latched](const FxEnabledResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    void requireNotFound(const CommandResult& result)
    {
        REQUIRE(std::holds_alternative<Error>(result));
        CHECK(std::get<Error>(result).number == akm::error_number::NOT_FOUND);
    }

    // What a real S5000 answers to an item naming a channel or a module it does not have (observed with no board,
    // TASK-AKM-104).
    void requireOutOfRange(const CommandResult& result)
    {
        REQUIRE(std::holds_alternative<Error>(result));
        CHECK(std::get<Error>(result).number == akm::error_number::OUT_OF_RANGE);
    }

    void requireRefused(const CommandResult& result, RefusalReason reason)
    {
        REQUIRE(std::holds_alternative<Refused>(result));
        CHECK(std::get<Refused>(result).reason == reason);
    }
}

TEST_CASE("Given a simulated EB20 on a current multi, When channel 1 is muted, Then the frame carries section 12, item 20, then 01 01 and the status reads MUTE, and set back it reads ON with 01 00 [RQ-AKM-100]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    seedEb20OnACurrentMulti(harness);

    CHECK(getMute(harness, 1).muted == false);

    akm::setFxChannelMute(harness.session(), 1, true, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(harness.sentFrames().back()[akm::test::SENT_SECTION_INDEX] == SECTION_MULTI_FX);
    CHECK(harness.sentFrames().back()[akm::test::SENT_ITEM_INDEX] == 0x20);
    CHECK(dataOfLastFrame(harness, 2) == bytes({0x01, 0x01}));
    CHECK(getMute(harness, 1).muted == true);
    CHECK(getMute(harness, 0).muted == false);
    CHECK(harness.sentFrames().back()[akm::test::SENT_ITEM_INDEX] == 0x21);

    akm::setFxChannelMute(harness.session(), 1, false, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(dataOfLastFrame(harness, 2) == bytes({0x01, 0x00}));
    CHECK(getMute(harness, 1).muted == false);
}

TEST_CASE("Given module 2 of channel 0 set to the type Flange, When its type is read, Then it is Flange, the frame having carried 00 02 03 [RQ-AKM-100]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    seedEb20OnACurrentMulti(harness);

    CHECK(getType(harness, 0, 2).type == FxModuleType::Chorus);

    akm::setFxModuleType(harness.session(), 0, 2, FxModuleType::Flange, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(harness.sentFrames().back()[akm::test::SENT_ITEM_INDEX] == 0x30);
    CHECK(dataOfLastFrame(harness, 3) == bytes({0x00, 0x02, 0x03}));

    const FxModuleTypeResult result = getType(harness, 0, 2);
    CHECK(result.type == FxModuleType::Flange);
    CHECK(harness.sentFrames().back()[akm::test::SENT_ITEM_INDEX] == 0x31);
    CHECK(dataOfLastFrame(harness, 2) == bytes({0x00, 0x02}));
    CHECK(getType(harness, 1, 2).type == FxModuleType::Chorus);
}

TEST_CASE("Given a module type code that Table 24 does not name, When it is read, Then it is passed through unchanged [RQ-AKM-100]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setMultiNames({"FX MULTI"});
    harness.sampler().setCurrentMulti(0);
    harness.sampler().setFxBoard({{0x11}});

    CHECK(getType(harness, 0, 0).type == static_cast<FxModuleType>(0x11));
}

TEST_CASE("Given module 3 of channel 0, When it is disabled then read, Then it is disabled with the frame 00 03 00, and enabled again it is enabled [RQ-AKM-100]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    seedEb20OnACurrentMulti(harness);

    CHECK(getEnabled(harness, 0, 3).enabled == true);

    akm::setFxModuleEnabled(harness.session(), 0, 3, false, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(harness.sentFrames().back()[akm::test::SENT_ITEM_INDEX] == 0x40);
    CHECK(dataOfLastFrame(harness, 3) == bytes({0x00, 0x03, 0x00}));
    CHECK(getEnabled(harness, 0, 3).enabled == false);
    CHECK(harness.sentFrames().back()[akm::test::SENT_ITEM_INDEX] == 0x41);
    CHECK(getEnabled(harness, 0, 2).enabled == true);

    akm::setFxModuleEnabled(harness.session(), 0, 3, true, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(dataOfLastFrame(harness, 3) == bytes({0x00, 0x03, 0x01}));
    CHECK(getEnabled(harness, 0, 3).enabled == true);
}

TEST_CASE("Given a channel or a module the board does not have, When any configuration item is sent, Then the sampler's ERROR 02 (out of range) is reported unchanged and nothing changes [RQ-AKM-100]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    seedEb20OnACurrentMulti(harness);

    akm::setFxChannelMute(harness.session(), 4, true, harness.recorder().completion());
    akm::setFxModuleType(harness.session(), 0, 6, FxModuleType::Chorus, harness.recorder().completion());
    akm::setFxModuleEnabled(harness.session(), 2, 2, false, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    for (const CommandResult& result : harness.recorder().results())
        requireOutOfRange(result);

    requireOutOfRange(getMute(harness, 4).outcome);
    CHECK_FALSE(getMute(harness, 4).muted.has_value());
    requireOutOfRange(getType(harness, 0, 6).outcome);
    requireOutOfRange(getEnabled(harness, 3, 2).outcome);
    CHECK(getEnabled(harness, 2, 1).enabled == true);
}

TEST_CASE("Given no multi is current, When a configuration item is sent, Then the sampler's ERROR 04 is reported, the effects being the current multi's [RQ-AKM-100]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setFxBoard(eb20Layout());

    akm::setFxChannelMute(harness.session(), 0, true, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    requireNotFound(harness.recorder().results().back());
    requireNotFound(getType(harness, 0, 2).outcome);
}

TEST_CASE("Given a channel or a module of 128, or a type code of 128, When the item is sent, Then the request is refused as ArgumentOutOfRange without sending [RQ-AKM-100]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    seedEb20OnACurrentMulti(harness);
    const auto sentBefore = harness.sentCount();

    akm::setFxChannelMute(harness.session(), 128, true, harness.recorder().completion());
    akm::setFxModuleType(harness.session(), 0, 128, FxModuleType::Chorus, harness.recorder().completion());
    akm::setFxModuleType(harness.session(), 0, 2, static_cast<FxModuleType>(128), harness.recorder().completion());
    akm::setFxModuleEnabled(harness.session(), -1, 0, true, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(4));
    for (const CommandResult& result : harness.recorder().results())
        requireRefused(result, RefusalReason::ArgumentOutOfRange);
    requireRefused(getMute(harness, 128).outcome, RefusalReason::ArgumentOutOfRange);
    requireRefused(getType(harness, 0, 128).outcome, RefusalReason::ArgumentOutOfRange);
    requireRefused(getEnabled(harness, 128, 0).outcome, RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == sentBefore);
}
