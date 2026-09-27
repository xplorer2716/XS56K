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

// The primitives of section 00 on a session and the simulated sampler: discovery, the checksum mode, the
// four other toggles and Echo. Discovery, the mode command and Echo are what the first real-sampler
// exchange rests on. All of it runs on manual time.
// [TASK-AKM-008, RQ-AKM-012, RQ-AKM-013, RQ-AKM-014, RQ-AKM-015, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-007,
// DEC-AKM-012)]
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/ItemCatalogue.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/SamplerError.hpp"
#include "akm/SysExConfig.hpp"

using akm::ChecksumMode;
using akm::CommandCompletion;
using akm::CommandResult;
using akm::DiscoveryResult;
using akm::Done;
using akm::EchoResult;
using akm::Error;
using akm::ItemId;
using akm::RefusalReason;
using akm::Refused;
using akm::Reply;
using akm::ReplyId;
using akm::Session;
using akm::harness::ManualScenarioDriver;
using akm::harness::SamplerBehaviour;
using akm::harness::SamplerConfig;
using akm::harness::SamplerSettings;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;
using akm::test::bytes;
using namespace std::chrono_literals;

namespace
{
    constexpr auto STEP = 1ms;
    constexpr auto SHORT_WINDOW = 100ms;

    // The sampler answers nothing by itself: the tests inject the confirmations they want.
    void makeSilent(SessionHarness& harness)
    {
        SamplerBehaviour behaviour;
        behaviour.silent = true;
        harness.sampler().setBehaviour(behaviour);
    }

    const std::array<std::uint8_t, akm::ECHO_DATA_SIZE> ECHO_BYTES{{0x01, 0x23, 0x45, 0x67}};

    // The four toggles that are not the checksum mode: the helper, the item code and the model's own record.
    struct ToggleCase
    {
        const char* name;
        std::uint8_t item;
        void (*set)(Session&, bool, CommandCompletion);
        bool SamplerSettings::*setting;
    };

    const std::array<ToggleCase, 4> TOGGLES{{
        {"notification", 0x01, &akm::setNotification, &SamplerSettings::notification},
        {"sync LCD", 0x03, &akm::setSyncLcd, &SamplerSettings::syncLcd},
        {"auto screen update", 0x05, &akm::setAutoScreenUpdate, &SamplerSettings::autoScreenUpdate},
        {"still alive", 0x07, &akm::setStillAlive, &SamplerSettings::stillAlive},
    }};
}

// --- RQ-AKM-012: discovery ---

TEST_CASE("Given two samplers with DeviceIDs 3 and 7, When discovery runs, Then the result is {3, 7} [RQ-AKM-012]",
          "[akm][sysexconfig]")
{
    ManualScenarioDriver driver;
    Latched<DiscoveryResult> discovered;
    SessionHarness harness{driver, {}, SamplerConfig{3}, false};
    harness.backend().addSampler(SamplerConfig{7});

    akm::discover(harness.session(), [&discovered](const DiscoveryResult& result) { discovered.set(result); });
    REQUIRE(harness.waitUntil([&discovered] { return discovered.isSet(); }));

    const auto result = discovered.value();
    REQUIRE(result.has_value());
    // Each sampler answered twice, an OK and a DONE, and is reported once.
    CHECK(result->deviceIds == std::vector<std::uint8_t>{3, 7});
    CHECK(std::holds_alternative<Done>(result->outcome));
}

TEST_CASE("Given discovery, When it runs, Then the Query goes out addressed to every sampler with a checksum appended, the mode being unknown [RQ-AKM-012, RQ-AKM-041]",
          "[akm][sysexconfig]")
{
    ManualScenarioDriver driver;
    Latched<DiscoveryResult> discovered;
    SessionHarness harness{driver, {}, SamplerConfig{3}, false};

    akm::discover(harness.session(), [&discovered](const DiscoveryResult& result) { discovered.set(result); });
    REQUIRE(harness.waitUntil([&discovered] { return discovered.isSet(); }));

    REQUIRE(harness.sentCount() == 1);
    const Bytes frame = harness.sentFrames().front();
    CHECK(frame.at(akm::DEVICE_BYTE_INDEX) == 0x00);
    CHECK(frame.at(akm::test::SENT_SECTION_INDEX) == 0x00);
    CHECK(frame.at(akm::test::SENT_ITEM_INDEX) == 0x00);
    CHECK(harness.carriesChecksum(0, 0));
}

TEST_CASE("Given no sampler answering, When the window ends, Then the result is empty and no error is raised [RQ-AKM-012]",
          "[akm][sysexconfig]")
{
    ManualScenarioDriver driver;
    Latched<DiscoveryResult> discovered;
    SessionHarness harness{driver, {}, SamplerConfig{3}, false};
    makeSilent(harness);

    akm::discover(harness.session(), [&discovered](const DiscoveryResult& result) { discovered.set(result); });
    REQUIRE(harness.waitUntil([&discovered] { return discovered.isSet(); }));

    const auto result = discovered.value();
    REQUIRE(result.has_value());
    CHECK(result->deviceIds.empty());
    CHECK(std::holds_alternative<Done>(result->outcome));
}

TEST_CASE("Given a window of a given length, When time passes, Then discovery ends when the window does and not before [RQ-AKM-012]",
          "[akm][sysexconfig]")
{
    ManualScenarioDriver driver;
    Latched<DiscoveryResult> discovered;
    SessionHarness harness{driver, {}, SamplerConfig{3}, false};

    akm::discover(harness.session(), [&discovered](const DiscoveryResult& result) { discovered.set(result); },
                  SHORT_WINDOW);
    harness.elapse(SHORT_WINDOW - STEP);
    CHECK_FALSE(discovered.isSet());

    harness.elapse(STEP);
    CHECK(discovered.isSet());
}

TEST_CASE("Given a sampler that answers the Query with an ERROR, When discovery runs, Then it is still reported as present [RQ-AKM-012, ADR-AKM-001 (DEC-AKM-007)]",
          "[akm][sysexconfig]")
{
    ManualScenarioDriver driver;
    Latched<DiscoveryResult> discovered;
    SessionHarness harness{driver, {}, SamplerConfig{5}, false};
    SamplerBehaviour behaviour;
    behaviour.itemErrors = {{0x00, 0x00, akm::error_number::NOT_SUPPORTED}};
    harness.sampler().setBehaviour(behaviour);

    akm::discover(harness.session(), [&discovered](const DiscoveryResult& result) { discovered.set(result); });
    REQUIRE(harness.waitUntil([&discovered] { return discovered.isSet(); }));

    CHECK(discovered.value()->deviceIds == std::vector<std::uint8_t>{5});
}

// --- RQ-AKM-013: the checksum mode command ---

TEST_CASE("Given the checksum mode command, When set on and then off, Then the framing of the following command changes with it [RQ-AKM-013]",
          "[akm][sysexconfig]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    akm::setChecksumMode(harness.session(), true, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(harness.session().checksumMode() == ChecksumMode::On);
    // The mode command itself carries a checksum, whatever the mode the session assumed.
    CHECK(harness.carriesChecksum(0, 1));

    harness.submit(akm::makeRequest(ItemId::SysExNotification, {1}));
    REQUIRE(harness.waitForCompletions(2));
    CHECK(harness.carriesChecksum(1, 1));

    akm::setChecksumMode(harness.session(), false, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    CHECK(harness.session().checksumMode() == ChecksumMode::Off);
    CHECK(harness.carriesChecksum(2, 1));

    harness.submit(akm::makeRequest(ItemId::SysExNotification, {1}));
    REQUIRE(harness.waitForCompletions(4));
    CHECK_FALSE(harness.carriesChecksum(3, 1));
    for (const CommandResult& result : harness.recorder().results())
        CHECK(std::holds_alternative<Done>(result));
    CHECK_FALSE(harness.sampler().settings().checksum);
}

// --- RQ-AKM-014: the other toggles ---

TEST_CASE("Given each of the four toggles and each value, When set, Then the frame carries section 00, that item code and that data byte, and the command completes on DONE [RQ-AKM-014]",
          "[akm][sysexconfig]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    std::size_t completions = 0;
    for (const ToggleCase& toggle : TOGGLES)
    {
        for (const bool value : {true, false})
        {
            INFO(toggle.name << " " << value);
            toggle.set(harness.session(), value, harness.recorder().completion());
            REQUIRE(harness.waitForCompletions(++completions));

            const Bytes frame = harness.sentFrames().back();
            CHECK(frame.at(akm::test::SENT_SECTION_INDEX) == 0x00);
            CHECK(frame.at(akm::test::SENT_ITEM_INDEX) == toggle.item);
            CHECK(frame.at(akm::test::SENT_ITEM_INDEX + 1) == (value ? 1 : 0));
            CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
            CHECK((harness.sampler().settings().*(toggle.setting)) == value);
        }
    }
}

TEST_CASE("Given the Still Alive command, When it completes, Then the session reads F0 F7 as the spec says while it is on [RQ-AKM-011, RQ-AKM-014]",
          "[akm][sysexconfig]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    CHECK_FALSE(harness.session().stillAliveMonitoring());

    akm::setStillAlive(harness.session(), true, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(harness.session().stillAliveMonitoring());

    akm::setStillAlive(harness.session(), false, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK_FALSE(harness.session().stillAliveMonitoring());
}

TEST_CASE("Given the value 2 for each toggle, When submitted, Then it is refused without any frame being sent and not from the submitting thread [RQ-AKM-014]",
          "[akm][sysexconfig]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    for (const ItemId id : {ItemId::SysExNotification, ItemId::SysExSyncLcd, ItemId::SysExChecksum,
                            ItemId::SysExAutoScreenUpdate, ItemId::SysExStillAlive})
    {
        const std::size_t before = harness.recorder().count();
        harness.submit(akm::makeRequest(id, {2}));
        // submit() only queued it.
        CHECK(harness.recorder().count() == before);

        harness.settle();
        REQUIRE(harness.recorder().count() == before + 1);
        REQUIRE(std::holds_alternative<Refused>(harness.recorder().results().back()));
        CHECK(std::get<Refused>(harness.recorder().results().back()).reason == RefusalReason::ArgumentOutOfRange);
    }
    CHECK(harness.sentCount() == 0);
}

// --- RQ-AKM-015: Echo ---

TEST_CASE("Given the bytes 01 23 45 67 and a sampler that echoes them, When the Echo runs, Then it succeeds [RQ-AKM-015]",
          "[akm][sysexconfig]")
{
    ManualScenarioDriver driver;
    Latched<EchoResult> echoed;
    SessionHarness harness{driver};

    akm::echo(harness.session(), ECHO_BYTES, [&echoed](const EchoResult& result) { echoed.set(result); });
    REQUIRE(harness.waitUntil([&echoed] { return echoed.isSet(); }));

    const auto result = echoed.value();
    REQUIRE(result.has_value());
    CHECK(result->succeeded());
    CHECK_FALSE(result->mismatch.has_value());
    REQUIRE(std::holds_alternative<Reply>(result->outcome));
    CHECK(std::get<Reply>(result->outcome).data == bytes({0x01, 0x23, 0x45, 0x67}));
    // The frame carries the four bytes after section 00 and item 06.
    const Bytes frame = harness.sentFrames().front();
    CHECK(frame.at(akm::test::SENT_ITEM_INDEX) == 0x06);
    CHECK(Bytes(frame.begin() + akm::test::SENT_ITEM_INDEX + 1, frame.begin() + akm::test::SENT_ITEM_INDEX + 5)
          == bytes({0x01, 0x23, 0x45, 0x67}));
}

TEST_CASE("Given a sampler that answers 01 23 45 66 to an Echo of 01 23 45 67, When the Echo runs, Then it fails with a mismatch naming both byte sequences [RQ-AKM-015]",
          "[akm][sysexconfig]")
{
    ManualScenarioDriver driver;
    Latched<EchoResult> echoed;
    SessionHarness harness{driver};
    makeSilent(harness);

    akm::echo(harness.session(), ECHO_BYTES, [&echoed](const EchoResult& result) { echoed.set(result); });
    harness.settle();
    REQUIRE(harness.sentCount() == 1);
    harness.inject(harness.confirmationFor(0, ReplyId::Reply, bytes({0x01, 0x23, 0x45, 0x66})));
    harness.settle();

    const auto result = echoed.value();
    REQUIRE(result.has_value());
    CHECK_FALSE(result->succeeded());
    REQUIRE(result->mismatch.has_value());
    CHECK(result->mismatch->sent == bytes({0x01, 0x23, 0x45, 0x67}));
    CHECK(result->mismatch->received == bytes({0x01, 0x23, 0x45, 0x66}));
}

TEST_CASE("Given an Echo REPLY that does not hold four bytes, When the Echo runs in a known mode, Then it fails with a mismatch showing what came back [RQ-AKM-015]",
          "[akm][sysexconfig]")
{
    ManualScenarioDriver driver;
    Latched<EchoResult> echoed;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    makeSilent(harness);

    akm::echo(harness.session(), ECHO_BYTES, [&echoed](const EchoResult& result) { echoed.set(result); });
    harness.settle();
    harness.inject(harness.confirmationFor(harness.sentCount() - 1, ReplyId::Reply, bytes({0x01, 0x02, 0x03})));
    harness.settle();

    const auto result = echoed.value();
    REQUIRE(result.has_value());
    CHECK_FALSE(result->succeeded());
    REQUIRE(result->mismatch.has_value());
    CHECK(result->mismatch->received == bytes({0x01, 0x02, 0x03}));
}

TEST_CASE("Given a byte 80, When the Echo is requested, Then it is refused without any frame being sent [RQ-AKM-015]",
          "[akm][sysexconfig]")
{
    ManualScenarioDriver driver;
    Latched<EchoResult> echoed;
    SessionHarness harness{driver};

    constexpr std::uint8_t NOT_A_DATA_BYTE = 0x80;
    akm::echo(harness.session(), {0x01, 0x23, 0x45, NOT_A_DATA_BYTE},
              [&echoed](const EchoResult& result) { echoed.set(result); });
    harness.settle();

    const auto result = echoed.value();
    REQUIRE(result.has_value());
    CHECK_FALSE(result->succeeded());
    CHECK_FALSE(result->mismatch.has_value());
    REQUIRE(std::holds_alternative<Refused>(result->outcome));
    CHECK(std::get<Refused>(result->outcome).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 0);
}

TEST_CASE("Given a sampler that answers the Echo with an ERROR, When the Echo runs, Then it fails with that error and no mismatch [RQ-AKM-015]",
          "[akm][sysexconfig]")
{
    ManualScenarioDriver driver;
    Latched<EchoResult> echoed;
    SessionHarness harness{driver};
    makeSilent(harness);

    akm::echo(harness.session(), ECHO_BYTES, [&echoed](const EchoResult& result) { echoed.set(result); });
    harness.settle();
    harness.inject(harness.confirmationFor(0, ReplyId::Error, bytes({0x00, 0x01})));
    harness.settle();

    const auto result = echoed.value();
    REQUIRE(result.has_value());
    CHECK_FALSE(result->succeeded());
    CHECK_FALSE(result->mismatch.has_value());
    REQUIRE(std::holds_alternative<Error>(result->outcome));
    CHECK(std::get<Error>(result->outcome).number == akm::error_number::INVALID_FORMAT);
}
