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

// The session closing: what is in flight or queued is cancelled, the section 00 settings the session changed are put
// back to the sampler's documented defaults, the input is stopped, and a sampler that no longer answers does not
// hold the close up. [TASK-AKM-011, RQ-AKM-013, RQ-AKM-018, RQ-AKM-040, RQ-AKM-042,
// ADR-AKM-001 (DEC-AKM-004, DEC-AKM-007, DEC-AKM-011)]
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <thread>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/DiagnosticSink.hpp"
#include "akm/SamplerError.hpp"
#include "akm/SessionConfig.hpp"
#include "akm/SysExConfig.hpp"

using namespace std::chrono_literals;
using akm::Cancelled;
using akm::ChecksumMode;
using akm::CloseResult;
using akm::Command;
using akm::CommandRequest;
using akm::OpenResult;
using akm::RefusalReason;
using akm::Refused;
using akm::SamplerSetting;
using akm::SessionConfig;
using akm::SessionState;
using akm::SessionTiming;
using akm::SettingChoice;
using akm::harness::DeliveryMode;
using akm::harness::ManualScenarioDriver;
using akm::harness::OsVersion;
using akm::harness::RealScenarioDriver;
using akm::harness::SamplerBehaviour;
using akm::harness::SamplerConfig;
using akm::harness::SimulatedMidiBackend;
using akm::harness::SimulatedSampler;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;

namespace
{
    constexpr auto COMMAND_TIMEOUT = 50ms;
    constexpr auto DISCOVERY_WINDOW = 100ms;
    constexpr auto TOTAL_WAIT = 200ms;

    SessionTiming closeTiming()
    {
        SessionTiming timing;
        timing.commandTimeout = COMMAND_TIMEOUT;
        timing.maxTotalWait = TOTAL_WAIT;
        timing.discoveryWindow = DISCOVERY_WINDOW;
        return timing;
    }

    SamplerConfig sampler(std::uint8_t deviceId)
    {
        SamplerConfig config;
        config.deviceId = deviceId;
        return config;
    }

    SessionConfig configFor(std::uint32_t deviceId)
    {
        SessionConfig config;
        config.targetDeviceId = deviceId;
        return config;
    }

    // The item and the data byte of the frames sent from `first` on: what the close put on the wire.
    std::vector<std::pair<std::uint8_t, std::uint8_t>> sentSince(const SessionHarness& harness, std::size_t first)
    {
        std::vector<std::pair<std::uint8_t, std::uint8_t>> sent;
        const std::vector<Bytes> frames = harness.sentFrames();
        for (std::size_t index = first; index < frames.size(); ++index)
            sent.emplace_back(frames[index].at(akm::test::SENT_ITEM_INDEX), frames[index].at(akm::test::SENT_ITEM_INDEX + 1));
        return sent;
    }

    void makeSilent(SessionHarness& harness)
    {
        SamplerBehaviour behaviour;
        behaviour.silent = true;
        harness.sampler().setBehaviour(behaviour);
    }

    void answerAgain(SessionHarness& harness)
    {
        harness.sampler().setBehaviour(SamplerBehaviour{});
    }

    CommandRequest anApplicationCommand()
    {
        return CommandRequest{Command{akm::test::SECTION_CONFIG, akm::test::ITEM_ECHO, akm::test::bytes({1, 2, 3, 4})}, {}};
    }

    using Sent = std::vector<std::pair<std::uint8_t, std::uint8_t>>;
    constexpr std::uint8_t CHECKSUM_ITEM = akm::test::ITEM_CHECKSUM;
    constexpr std::uint8_t NOTIFICATION_ITEM = akm::test::ITEM_NOTIFICATION;
    constexpr std::uint8_t SYNC_LCD_ITEM = akm::test::ITEM_SYNC_LCD;
    constexpr std::uint8_t AUTO_SCREEN_UPDATE_ITEM = akm::test::ITEM_AUTO_SCREEN_UPDATE;
    constexpr std::uint8_t STILL_ALIVE_ITEM = akm::test::ITEM_STILL_ALIVE;
    constexpr std::uint8_t ON = akm::test::TOGGLE_ON;
    constexpr std::uint8_t OFF = akm::test::TOGGLE_OFF;
}

TEST_CASE("Given the documented defaults, When asked for each setting, Then they are checksums off, Notification on, Sync LCD on, Auto screen update off and Still Alive off [RQ-AKM-042, RQ-AKM-018]",
          "[akm][close]")
{
    CHECK_FALSE(akm::samplerDefault(SamplerSetting::Checksums));
    CHECK(akm::samplerDefault(SamplerSetting::Notification));
    CHECK(akm::samplerDefault(SamplerSetting::SyncLcd));
    CHECK_FALSE(akm::samplerDefault(SamplerSetting::AutoScreenUpdate));
    CHECK_FALSE(akm::samplerDefault(SamplerSetting::StillAlive));
}

TEST_CASE("Given a close result, When it lists nothing that was left, Then everything was restored [RQ-AKM-042]", "[akm][close]")
{
    CloseResult none;
    CHECK(none.restoredAll());

    CloseResult left;
    left.notRestored.push_back(SamplerSetting::SyncLcd);
    CHECK_FALSE(left.restoredAll());
}

TEST_CASE("Given a session opened with checksums on and two commands queued behind one in flight, When it is closed, Then the three complete as cancelled, then a checksum-mode command turning it off is sent with a valid checksum and completes on DONE [RQ-AKM-042, RQ-AKM-013]",
          "[akm][close]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming(), sampler(0), false};
    SessionConfig config;
    config.checksums = true;
    const auto opened = harness.openAndWait(config);
    REQUIRE(opened.has_value());
    REQUIRE(opened->ready());
    REQUIRE(harness.session().checksumMode() == ChecksumMode::On);
    makeSilent(harness);
    harness.submit(anApplicationCommand());
    harness.submit(anApplicationCommand());
    harness.submit(anApplicationCommand());
    harness.settle();
    const std::size_t framesBeforeClose = harness.sentCount();
    answerAgain(harness);

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    const std::vector<akm::CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == 3);
    for (const akm::CommandResult& result : results)
        CHECK(std::holds_alternative<Cancelled>(result));
    // What the session had changed, put back in the order of the spec's items: the checksum mode first.
    CHECK(sentSince(harness, framesBeforeClose) == Sent{{CHECKSUM_ITEM, OFF}, {STILL_ALIVE_ITEM, OFF}, {SYNC_LCD_ITEM, ON}});
    CHECK(harness.carriesChecksum(framesBeforeClose, 1));
    CHECK_FALSE(harness.carriesChecksum(framesBeforeClose + 1, 1));
    CHECK(closed->restored == std::vector<SamplerSetting>{SamplerSetting::Checksums, SamplerSetting::StillAlive, SamplerSetting::SyncLcd});
    CHECK(closed->restoredAll());
    const auto settings = harness.sampler().settings();
    CHECK_FALSE(settings.checksum);
    CHECK_FALSE(settings.stillAlive);
    CHECK(settings.syncLcd);
    CHECK(harness.session().state() == SessionState::Closed);
    CHECK_FALSE(harness.inputStarted());
}

TEST_CASE("Given every setting changed by the open, When the session is closed, Then each goes back to its documented default, in the order of the spec's items [RQ-AKM-042, RQ-AKM-040]",
          "[akm][close]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming(), sampler(0), false};
    SessionConfig config;
    config.checksums = true;
    config.notification = SettingChoice::Off;
    config.syncLcd = SettingChoice::Off;
    config.autoScreenUpdate = SettingChoice::On;
    config.stillAlive = SettingChoice::On;
    REQUIRE(harness.openAndWait(config)->ready());
    const std::size_t framesBeforeClose = harness.sentCount();

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(sentSince(harness, framesBeforeClose)
          == Sent{{CHECKSUM_ITEM, OFF}, {STILL_ALIVE_ITEM, OFF}, {NOTIFICATION_ITEM, ON}, {SYNC_LCD_ITEM, ON},
                  {AUTO_SCREEN_UPDATE_ITEM, OFF}});
    CHECK(closed->restoredAll());
    CHECK(closed->restored.size() == 5);
    const auto settings = harness.sampler().settings();
    CHECK_FALSE(settings.checksum);
    CHECK(settings.notification);
    CHECK(settings.syncLcd);
    CHECK_FALSE(settings.autoScreenUpdate);
    CHECK_FALSE(settings.stillAlive);
}

TEST_CASE("Given settings the open left unchanged, When the session is closed, Then they are not sent [RQ-AKM-042]",
          "[akm][close]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming(), sampler(0), false};
    // The defaults: Notification and Auto screen update are left alone.
    REQUIRE(harness.openAndWait(SessionConfig{})->ready());
    const std::size_t framesBeforeClose = harness.sentCount();

    static_cast<void>(harness.closeForResult());

    CHECK(sentSince(harness, framesBeforeClose) == Sent{{CHECKSUM_ITEM, OFF}, {STILL_ALIVE_ITEM, OFF}, {SYNC_LCD_ITEM, ON}});
}

TEST_CASE("Given a setting changed through a primitive of section 00, When the session is closed, Then it is put back, opened or not [RQ-AKM-042, RQ-AKM-014]",
          "[akm][close]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming(), sampler(0)};
    akm::setAutoScreenUpdate(harness.session(), true, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    REQUIRE(harness.sampler().settings().autoScreenUpdate);
    const std::size_t framesBeforeClose = harness.sentCount();

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(sentSince(harness, framesBeforeClose) == Sent{{AUTO_SCREEN_UPDATE_ITEM, OFF}});
    CHECK(closed->restored == std::vector<SamplerSetting>{SamplerSetting::AutoScreenUpdate});
    CHECK_FALSE(harness.sampler().settings().autoScreenUpdate);
}

TEST_CASE("Given a session that changed nothing, When it is closed, Then nothing is sent and the close completes at once [RQ-AKM-042]",
          "[akm][close]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming(), sampler(0)};
    const auto works = harness.submitAndWait(anApplicationCommand());
    REQUIRE(works.has_value());
    // A raw command that changes a setting without saying so is the caller's to put back: the session tracks
    // what it is told, not what it can guess from an item code.
    REQUIRE(harness.establishChecksumMode(true).has_value());
    const std::size_t framesBeforeClose = harness.sentCount();

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(harness.sentCount() == framesBeforeClose);
    CHECK(closed->restored.empty());
    CHECK(closed->restoredAll());
}

TEST_CASE("Given a sampler that no longer answers, When the session is closed, Then the close returns once the first restoring command has timed out, and none of the settings is reported restored [RQ-AKM-042]",
          "[akm][close]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming(), sampler(0), false};
    REQUIRE(harness.openAndWait(SessionConfig{})->ready());
    makeSilent(harness);
    const std::size_t framesBeforeClose = harness.sentCount();
    const auto before = driver.scheduler().now();

    const auto closed = harness.closeForResult();

    const auto elapsed = driver.scheduler().now() - before;
    REQUIRE(closed.has_value());
    CHECK(closed->restored.empty());
    CHECK(closed->notRestored == std::vector<SamplerSetting>{SamplerSetting::Checksums, SamplerSetting::StillAlive, SamplerSetting::SyncLcd});
    // One timeout, not one per setting: a port that does not answer the first will not answer the others.
    CHECK(elapsed >= COMMAND_TIMEOUT);
    CHECK(elapsed < COMMAND_TIMEOUT + 10ms);
    CHECK(harness.sentCount() == framesBeforeClose + 1);
    CHECK(harness.session().state() == SessionState::Closed);
    CHECK_FALSE(harness.inputStarted());
}

TEST_CASE("Given a sampler that refuses one of the restoring commands, When the session is closed, Then the others are still sent, the refusal is reported and the close finishes [RQ-AKM-042]",
          "[akm][close]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming(), sampler(0), false};
    REQUIRE(harness.openAndWait(SessionConfig{})->ready());
    SamplerBehaviour behaviour;
    behaviour.itemErrors = {{akm::test::SECTION_CONFIG, akm::test::ITEM_STILL_ALIVE, akm::error_number::OUT_OF_RANGE}};
    harness.sampler().setBehaviour(behaviour);

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(closed->restored == std::vector<SamplerSetting>{SamplerSetting::Checksums, SamplerSetting::SyncLcd});
    CHECK(closed->notRestored == std::vector<SamplerSetting>{SamplerSetting::StillAlive});
    CHECK_FALSE(closed->restoredAll());
    CHECK(harness.session().state() == SessionState::Closed);
}

TEST_CASE("Given an open that has changed the checksum mode and is still going, When the session is closed, Then the open completes as cancelled and the checksum mode is put back [RQ-AKM-042, RQ-AKM-039]",
          "[akm][close]")
{
    ManualScenarioDriver driver;
    Latched<OpenResult> opened;
    SessionHarness harness{driver, closeTiming(), sampler(0), false};
    SamplerBehaviour slow;
    slow.replyDelay = 20ms;
    harness.sampler().setBehaviour(slow);

    harness.session().open(SessionConfig{}, [&opened](const OpenResult& result) { opened.set(result); });
    // The discovery ends at 100 ms, the checksum command is answered at 120 ms and the next one is under way.
    harness.elapse(DISCOVERY_WINDOW + 30ms);
    REQUIRE_FALSE(opened.isSet());
    REQUIRE(harness.session().state() == SessionState::Opening);

    const auto closed = harness.closeForResult();

    REQUIRE(opened.isSet());
    CHECK(opened.value()->status == akm::OpenStatus::Cancelled);
    REQUIRE(closed.has_value());
    CHECK(closed->restored == std::vector<SamplerSetting>{SamplerSetting::Checksums, SamplerSetting::SyncLcd});
    CHECK_FALSE(harness.sampler().settings().checksum);
}

TEST_CASE("Given a closed session, When it is closed again or a command is submitted, Then the second close is refused and the command is refused as closed [RQ-AKM-042]",
          "[akm][close]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming(), sampler(0)};
    static_cast<void>(harness.closeForResult());

    CHECK_FALSE(harness.session().close(akm::CloseCompletion{}));
    const auto refused = harness.submitAndWait(anApplicationCommand());

    REQUIRE(refused.has_value());
    REQUIRE(std::holds_alternative<Refused>(*refused));
    CHECK(std::get<Refused>(*refused).reason == RefusalReason::SessionClosed);
}

TEST_CASE("Given a close requested from a completion, When it is called, Then it is refused and nothing changes [RQ-AKM-042]",
          "[akm][close]")
{
    ManualScenarioDriver driver;
    bool refused = false;
    SessionHarness harness{driver, closeTiming(), sampler(0)};

    harness.session().submit(anApplicationCommand(), [&harness, &refused](const akm::CommandResult&) {
        refused = !harness.session().close(akm::CloseCompletion{});
    });
    REQUIRE(harness.waitUntil([&refused] { return refused; }));

    CHECK(harness.session().state() == SessionState::Unopened);
    CHECK(harness.inputStarted());
}

TEST_CASE("Given a session destroyed on a manual executor without a close, When it is destroyed, Then it does not wait for tasks that only its owner can run [RQ-AKM-042]",
          "[akm][close]")
{
    ManualScenarioDriver driver;
    SimulatedMidiBackend backend(driver.scheduler());
    backend.addSampler(sampler(0));
    const auto input = backend.openInput(backend.inputName());
    const auto output = backend.openOutput(backend.outputName());
    akm::NullDiagnosticSink diagnostics;
    const auto start = std::chrono::steady_clock::now();
    {
        akm::Session session(closeTiming(), driver.executor(), driver.scheduler(), *input, *output, diagnostics);
    }

    // Real time: waiting for the executor to run a close would take the whole timeout.
    CHECK(std::chrono::steady_clock::now() - start < 300ms);
    CHECK_FALSE(input->isStarted());
}

TEST_CASE("Given a session destroyed without an explicit close, When it is destroyed, Then it performs the same shutdown: what it changed is put back and nothing races [RQ-AKM-042, RQ-AKM-020]",
          "[akm][close][threads]")
{
    constexpr int REPEATS = 20;
    for (int repeat = 0; repeat < REPEATS; ++repeat)
    {
        RealScenarioDriver driver;
        SimulatedMidiBackend backend(driver.scheduler());
        SimulatedSampler& simulated = backend.addSampler(sampler(3));
        backend.setDeliveryMode(DeliveryMode::OnOtherThread);
        const auto input = backend.openInput(backend.inputName());
        const auto output = backend.openOutput(backend.outputName());
        akm::NullDiagnosticSink diagnostics;
        SessionTiming timing;
        timing.discoveryWindow = 30ms;
        Latched<OpenResult> opened;
        {
            akm::Session session(timing, driver.executor(), driver.scheduler(), *input, *output, diagnostics);
            SessionConfig config = configFor(3);
            config.checksums = true;
            session.open(config, [&opened](const OpenResult& result) { opened.set(result); });
            REQUIRE(driver.waitUntil([&opened] { return opened.isSet(); }, 30s));
            REQUIRE(opened.value()->ready());
            REQUIRE(simulated.settings().checksum);
            // No close: the destructor does it.
        }

        const auto settings = simulated.settings();
        CHECK_FALSE(settings.checksum);
        CHECK_FALSE(settings.stillAlive);
        CHECK(settings.syncLcd);
        CHECK_FALSE(input->isStarted());
    }
}
