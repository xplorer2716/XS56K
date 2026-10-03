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

// A key held through `holdKey` and not released is released by the session's closing, before the section 00 settings
// are put back. A key is remembered when its Hold is sent, whatever the outcome, and forgotten when a Release of it
// succeeds. [TASK-AKM-072, RQ-AKM-075, RQ-AKM-042, ADR-AKM-001 (DEC-AKM-004, DEC-AKM-019)]
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/DiagnosticSink.hpp"
#include "akm/FrontPanel.hpp"
#include "akm/SamplerError.hpp"
#include "akm/SessionConfig.hpp"

using namespace std::chrono_literals;
using akm::Cancelled;
using akm::CloseResult;
using akm::Done;
using akm::Error;
using akm::FrontPanelKey;
using akm::KeyPressResult;
using akm::SessionConfig;
using akm::SessionState;
using akm::SessionTiming;
using akm::Timeout;
using akm::harness::DeliveryMode;
using akm::harness::ManualScenarioDriver;
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
    constexpr std::uint8_t SECTION_FRONT_PANEL = 0x20;
    constexpr std::uint8_t ITEM_KEY_HOLD = 0x01;
    constexpr std::uint8_t ITEM_KEY_RELEASE = 0x02;
    constexpr std::uint8_t EXIT_CODE = static_cast<std::uint8_t>(FrontPanelKey::Exit);
    constexpr std::uint8_t F1_CODE = static_cast<std::uint8_t>(FrontPanelKey::F1);
    constexpr auto COMMAND_TIMEOUT = 50ms;

    SessionTiming closeTiming()
    {
        SessionTiming timing;
        timing.commandTimeout = COMMAND_TIMEOUT;
        return timing;
    }

    // The section 20 frames sent from `first` on, as (item, keycode): what the close put on the wire for the keys.
    std::vector<std::pair<std::uint8_t, std::uint8_t>> frontPanelSince(const SessionHarness& harness, std::size_t first)
    {
        std::vector<std::pair<std::uint8_t, std::uint8_t>> sent;
        const std::vector<Bytes> frames = harness.sentFrames();
        for (std::size_t index = first; index < frames.size(); ++index)
        {
            if (frames[index].at(akm::test::SENT_SECTION_INDEX) == SECTION_FRONT_PANEL)
                sent.emplace_back(frames[index].at(akm::test::SENT_ITEM_INDEX),
                                  frames[index].at(akm::test::SENT_ITEM_INDEX + 1));
        }
        return sent;
    }

    using Sent = std::vector<std::pair<std::uint8_t, std::uint8_t>>;
    using Keys = std::vector<std::uint8_t>;

    void holdAndWait(SessionHarness& harness, FrontPanelKey key)
    {
        const std::size_t before = harness.recorder().count();
        akm::holdKey(harness.session(), key, harness.recorder().completion());
        REQUIRE(harness.waitForCompletions(before + 1));
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
}

TEST_CASE("Given a session in which EXIT was held and not released, When it is closed, Then a Release of EXIT is sent before the close completes, the sampler has no key down and the result lists it [RQ-AKM-075]",
          "[akm][close][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming()};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const std::size_t sentBefore = harness.sentCount();
    holdAndWait(harness, FrontPanelKey::Exit);
    REQUIRE(harness.sampler().frontPanel().keysDown == Keys{EXIT_CODE});

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(frontPanelSince(harness, sentBefore) == Sent{{ITEM_KEY_HOLD, EXIT_CODE}, {ITEM_KEY_RELEASE, EXIT_CODE}});
    CHECK(harness.sampler().frontPanel().keysDown.empty());
    CHECK(closed->keysReleased == Keys{EXIT_CODE});
    CHECK(closed->keysNotReleased.empty());
    CHECK(closed->restoredAll());
    CHECK(harness.session().state() == SessionState::Closed);
}

TEST_CASE("Given a session that held no key, When it is closed, Then no section 20 frame is sent [RQ-AKM-075]",
          "[akm][close][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming()};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const std::size_t sentBefore = harness.sentCount();

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(harness.sentCount() == sentBefore);
    CHECK(closed->keysReleased.empty());
    CHECK(closed->keysNotReleased.empty());
}

TEST_CASE("Given a key held then released by the caller, When the session is closed, Then no further Release is sent [RQ-AKM-075]",
          "[akm][close][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming()};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const std::size_t sentBefore = harness.sentCount();
    holdAndWait(harness, FrontPanelKey::Exit);
    akm::releaseKey(harness.session(), FrontPanelKey::Exit, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    const std::size_t sentAfterRelease = harness.sentCount();

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(harness.sentCount() == sentAfterRelease);
    CHECK(frontPanelSince(harness, sentBefore) == Sent{{ITEM_KEY_HOLD, EXIT_CODE}, {ITEM_KEY_RELEASE, EXIT_CODE}});
    CHECK(closed->keysReleased.empty());
}

TEST_CASE("Given two keys held, When the session is closed, Then both are released in ascending keycode order [RQ-AKM-075]",
          "[akm][close][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming()};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const std::size_t sentBefore = harness.sentCount();
    holdAndWait(harness, FrontPanelKey::Exit);
    holdAndWait(harness, FrontPanelKey::F1);

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(frontPanelSince(harness, sentBefore)
          == Sent{{ITEM_KEY_HOLD, EXIT_CODE}, {ITEM_KEY_HOLD, F1_CODE}, {ITEM_KEY_RELEASE, F1_CODE},
                  {ITEM_KEY_RELEASE, EXIT_CODE}});
    CHECK(closed->keysReleased == Keys{F1_CODE, EXIT_CODE});
    CHECK(harness.sampler().frontPanel().keysDown.empty());
}

TEST_CASE("Given the same key held twice, When the session is closed, Then it is released once [RQ-AKM-075]",
          "[akm][close][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming()};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const std::size_t sentBefore = harness.sentCount();
    holdAndWait(harness, FrontPanelKey::Exit);
    holdAndWait(harness, FrontPanelKey::Exit);

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(frontPanelSince(harness, sentBefore)
          == Sent{{ITEM_KEY_HOLD, EXIT_CODE}, {ITEM_KEY_HOLD, EXIT_CODE}, {ITEM_KEY_RELEASE, EXIT_CODE}});
    CHECK(closed->keysReleased == Keys{EXIT_CODE});
}

TEST_CASE("Given a Hold that timed out, When the session is closed, Then the key is released all the same, since the sampler may have carried the Hold out [RQ-AKM-075]",
          "[akm][close][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming()};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    makeSilent(harness);
    holdAndWait(harness, FrontPanelKey::Exit);
    REQUIRE(std::holds_alternative<Timeout>(harness.recorder().results().back()));
    answerAgain(harness);
    const std::size_t sentBefore = harness.sentCount();

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(frontPanelSince(harness, sentBefore) == Sent{{ITEM_KEY_RELEASE, EXIT_CODE}});
    CHECK(closed->keysReleased == Keys{EXIT_CODE});
}

TEST_CASE("Given a Hold answered with an ERROR of any number, When the session is closed, Then the key is forgotten: Table 30 note a says an ERROR means the data was not queued, so nothing is released and nothing is reported [RQ-AKM-075]",
          "[akm][close][front-panel]")
{
    for (const std::uint16_t number : {akm::error_number::UNKNOWN_ERROR, akm::error_number::OUT_OF_RANGE})
    {
        ManualScenarioDriver driver;
        SessionHarness harness{driver, closeTiming()};
        REQUIRE(harness.establishChecksumMode(false).has_value());
        SamplerBehaviour refuses;
        refuses.itemErrors = {{SECTION_FRONT_PANEL, ITEM_KEY_HOLD, number}};
        harness.sampler().setBehaviour(refuses);
        holdAndWait(harness, FrontPanelKey::Exit);
        REQUIRE(std::holds_alternative<Error>(harness.recorder().results().back()));
        answerAgain(harness);
        const std::size_t sentBefore = harness.sentCount();

        const auto closed = harness.closeForResult();

        CAPTURE(number);
        REQUIRE(closed.has_value());
        CHECK(harness.sentCount() == sentBefore);
        CHECK(closed->keysReleased.empty());
        CHECK(closed->keysNotReleased.empty());
        CHECK(closed->restoredAll());
    }
}

TEST_CASE("Given a Release at the close that times out and settings the session changed, When the session is closed, Then the key is reported not released and the settings are still put back [RQ-AKM-075, RQ-AKM-042]",
          "[akm][close][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming(), SamplerConfig{}, false};
    REQUIRE(harness.openAndWait(SessionConfig{})->ready());
    holdAndWait(harness, FrontPanelKey::Exit);
    SamplerBehaviour deaf;
    deaf.silentItems = {{SECTION_FRONT_PANEL, ITEM_KEY_RELEASE}};
    harness.sampler().setBehaviour(deaf);
    const auto before = driver.scheduler().now();

    const auto closed = harness.closeForResult();

    const auto elapsed = driver.scheduler().now() - before;
    REQUIRE(closed.has_value());
    CHECK(closed->keysNotReleased == Keys{EXIT_CODE});
    CHECK(closed->keysReleased.empty());
    CHECK(closed->restored
          == std::vector<akm::SamplerSetting>{akm::SamplerSetting::Checksums, akm::SamplerSetting::StillAlive,
                                              akm::SamplerSetting::SyncLcd});
    CHECK(closed->notRestored.empty());
    CHECK_FALSE(closed->restoredAll());
    // One timeout, the Release's: the settings answer.
    CHECK(elapsed >= COMMAND_TIMEOUT);
    CHECK(elapsed < 2 * COMMAND_TIMEOUT);
}

TEST_CASE("Given two keys held and the first Release timing out at the close, When the session is closed, Then the second is reported not released without being tried and the settings are still put back [RQ-AKM-075, RQ-AKM-042]",
          "[akm][close][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming(), SamplerConfig{}, false};
    REQUIRE(harness.openAndWait(SessionConfig{})->ready());
    holdAndWait(harness, FrontPanelKey::Exit);
    holdAndWait(harness, FrontPanelKey::F1);
    SamplerBehaviour deaf;
    deaf.silentItems = {{SECTION_FRONT_PANEL, ITEM_KEY_RELEASE}};
    harness.sampler().setBehaviour(deaf);
    const std::size_t sentBefore = harness.sentCount();

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(closed->keysNotReleased == Keys{F1_CODE, EXIT_CODE});
    CHECK(closed->keysReleased.empty());
    CHECK(closed->restored.size() == 3);
    CHECK(closed->notRestored.empty());
    // Only the first key's Release went out.
    CHECK(frontPanelSince(harness, sentBefore) == Sent{{ITEM_KEY_RELEASE, F1_CODE}});
}

TEST_CASE("Given a sampler that answers nothing at all, When the session closes with a key held and settings changed, Then the close ends after the key's timeout and the first setting's, and everything is reported not done [RQ-AKM-075, RQ-AKM-042]",
          "[akm][close][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming(), SamplerConfig{}, false};
    REQUIRE(harness.openAndWait(SessionConfig{})->ready());
    holdAndWait(harness, FrontPanelKey::Exit);
    makeSilent(harness);
    const auto before = driver.scheduler().now();

    const auto closed = harness.closeForResult();

    const auto elapsed = driver.scheduler().now() - before;
    REQUIRE(closed.has_value());
    CHECK(closed->keysNotReleased == Keys{EXIT_CODE});
    CHECK(closed->restored.empty());
    CHECK(closed->notRestored.size() == 3);
    CHECK(elapsed >= 2 * COMMAND_TIMEOUT);
    CHECK(elapsed < 2 * COMMAND_TIMEOUT + 10ms);
    CHECK(harness.session().state() == SessionState::Closed);
}

TEST_CASE("Given a Release the sampler refuses at the close, When the session is closed, Then the key is reported not released, the close finishes and the result is not complete [RQ-AKM-075]",
          "[akm][close][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming()};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    holdAndWait(harness, FrontPanelKey::Exit);
    SamplerBehaviour refuses;
    refuses.itemErrors = {{SECTION_FRONT_PANEL, ITEM_KEY_RELEASE, akm::error_number::OUT_OF_RANGE}};
    harness.sampler().setBehaviour(refuses);

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(closed->keysReleased.empty());
    CHECK(closed->keysNotReleased == Keys{EXIT_CODE});
    CHECK_FALSE(closed->restoredAll());
    CHECK(harness.session().state() == SessionState::Closed);
}

TEST_CASE("Given a sampler that no longer answers, When the session closes with a key held, Then the close returns after one timeout and the key is reported not released [RQ-AKM-075, RQ-AKM-042]",
          "[akm][close][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming()};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    holdAndWait(harness, FrontPanelKey::Exit);
    makeSilent(harness);
    const std::size_t sentBefore = harness.sentCount();
    const auto before = driver.scheduler().now();

    const auto closed = harness.closeForResult();

    const auto elapsed = driver.scheduler().now() - before;
    REQUIRE(closed.has_value());
    CHECK(closed->keysNotReleased == Keys{EXIT_CODE});
    CHECK(closed->keysReleased.empty());
    CHECK(elapsed >= COMMAND_TIMEOUT);
    CHECK(elapsed < COMMAND_TIMEOUT + 10ms);
    CHECK(harness.sentCount() == sentBefore + 1);
    CHECK(harness.session().state() == SessionState::Closed);
}

TEST_CASE("Given a press whose Hold is in flight when the session is closed, Then both of its commands complete as cancelled and the key is still released at the close [RQ-AKM-075, RQ-AKM-042]",
          "[akm][close][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming()};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    makeSilent(harness);
    auto pressed = std::make_shared<Latched<KeyPressResult>>();
    akm::pressKey(harness.session(), FrontPanelKey::Exit, [pressed](const KeyPressResult& r) { pressed->set(r); });
    harness.settle();
    REQUIRE(harness.sentCount() > 0);
    answerAgain(harness);
    const std::size_t sentBefore = harness.sentCount();

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    REQUIRE(pressed->isSet());
    CHECK(std::holds_alternative<Cancelled>(pressed->value()->hold));
    CHECK(std::holds_alternative<Cancelled>(pressed->value()->release));
    CHECK(frontPanelSince(harness, sentBefore) == Sent{{ITEM_KEY_RELEASE, EXIT_CODE}});
    CHECK(closed->keysReleased == Keys{EXIT_CODE});
}

TEST_CASE("Given a Hold still queued behind another command and never sent, When the session is closed, Then no Release is sent for it [RQ-AKM-075]",
          "[akm][close][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming()};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    makeSilent(harness);
    harness.submit(akm::Command{akm::test::SECTION_CONFIG, akm::test::ITEM_ECHO, akm::test::bytes({1, 2, 3, 4})});
    akm::holdKey(harness.session(), FrontPanelKey::Exit, harness.recorder().completion());
    harness.settle();
    const std::size_t sentBefore = harness.sentCount();

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(frontPanelSince(harness, sentBefore).empty());
    CHECK(closed->keysReleased.empty());
    CHECK(closed->keysNotReleased.empty());
}

TEST_CASE("Given a Hold answered ERROR 0 because the sampler has no section 20, When the session is closed, Then no Release is sent and nothing is reported not released [RQ-AKM-075]",
          "[akm][close][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming()};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    SamplerBehaviour unsupported;
    unsupported.itemErrors = {{SECTION_FRONT_PANEL, ITEM_KEY_HOLD, akm::error_number::NOT_SUPPORTED}};
    harness.sampler().setBehaviour(unsupported);
    holdAndWait(harness, FrontPanelKey::Exit);
    REQUIRE(std::holds_alternative<Error>(harness.recorder().results().back()));
    answerAgain(harness);
    const std::size_t sentBefore = harness.sentCount();

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(harness.sentCount() == sentBefore);
    CHECK(closed->keysReleased.empty());
    CHECK(closed->keysNotReleased.empty());
    CHECK(closed->restoredAll());
}

TEST_CASE("Given a key held on one device and the target rebound to another since, When the session is closed, Then no Release is sent to the new target and the key is reported not released [RQ-AKM-075]",
          "[akm][close][front-panel]")
{
    constexpr std::uint8_t OTHER_DEVICE = 5;
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming()};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    holdAndWait(harness, FrontPanelKey::Exit);
    harness.session().bindTarget(OTHER_DEVICE);
    harness.settle();
    const std::size_t sentBefore = harness.sentCount();

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(harness.sentCount() == sentBefore);
    CHECK(closed->keysReleased.empty());
    CHECK(closed->keysNotReleased == Keys{EXIT_CODE});
    CHECK_FALSE(closed->restoredAll());
    CHECK(harness.session().state() == SessionState::Closed);
}

TEST_CASE("Given a checksum-mode command in flight when the session is closed and a key held, When the close runs, Then the Release carries a checksum the sampler now expects and the key is released [RQ-AKM-075, RQ-AKM-013, RQ-AKM-042]",
          "[akm][close][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, closeTiming()};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    holdAndWait(harness, FrontPanelKey::Exit);
    // The sampler carries the command out and answers nothing: it now expects checksums, the session still tracks Off.
    makeSilent(harness);
    akm::CommandOptions switchOn;
    switchOn.checksumModeAfterDone = true;
    harness.submit(akm::Command{akm::test::SECTION_CONFIG, akm::test::ITEM_CHECKSUM, akm::test::bytes({akm::test::TOGGLE_ON})},
                   switchOn);
    harness.settle();
    REQUIRE(harness.sampler().settings().checksum);
    answerAgain(harness);
    const std::size_t sentBefore = harness.sentCount();

    const auto closed = harness.closeForResult();

    REQUIRE(closed.has_value());
    CHECK(closed->keysReleased == Keys{EXIT_CODE});
    CHECK(closed->keysNotReleased.empty());
    CHECK(harness.sampler().frontPanel().keysDown.empty());
    CHECK(frontPanelSince(harness, sentBefore) == Sent{{ITEM_KEY_RELEASE, EXIT_CODE}});
}

TEST_CASE("Given a session destroyed without a close while a key is held, When it is destroyed, Then the key is released [RQ-AKM-075, RQ-AKM-042]",
          "[akm][close][front-panel][threads]")
{
    constexpr int REPEATS = 5;
    constexpr std::uint8_t DEVICE_ID = 3;
    for (int repeat = 0; repeat < REPEATS; ++repeat)
    {
        RealScenarioDriver driver;
        SimulatedMidiBackend backend(driver.scheduler());
        SamplerConfig config;
        config.deviceId = DEVICE_ID;
        SimulatedSampler& simulated = backend.addSampler(config);
        backend.setDeliveryMode(DeliveryMode::OnOtherThread);
        const auto input = backend.openInput(backend.inputName());
        const auto output = backend.openOutput(backend.outputName());
        akm::NullDiagnosticSink diagnostics;
        SessionTiming timing;
        timing.discoveryWindow = 30ms;
        {
            akm::Session session(timing, driver.executor(), driver.scheduler(), *input, *output, diagnostics);
            session.bindTarget(DEVICE_ID);
            auto held = std::make_shared<Latched<akm::CommandResult>>();
            akm::holdKey(session, FrontPanelKey::Exit, [held](const akm::CommandResult& r) { held->set(r); });
            REQUIRE(driver.waitUntil([held] { return held->isSet(); }, 30s));
            REQUIRE(simulated.frontPanel().keysDown == Keys{EXIT_CODE});
            // No close: the destructor does it.
        }

        CHECK(simulated.frontPanel().keysDown.empty());
    }
}
