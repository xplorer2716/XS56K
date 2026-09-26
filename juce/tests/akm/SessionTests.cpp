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

// The session state machine on manual time: matching, one command in flight, completion, timeout, Still
// Alive, collection windows, the tri-state checksum mode, sequences and closing. Every scenario here costs
// no wall-clock time, whatever the timeouts it exercises.
// [TASK-AKM-006, RQ-AKM-007 to RQ-AKM-013, RQ-AKM-020, RQ-AKM-041, RQ-AKM-043,
// ADR-AKM-001 (DEC-AKM-004, DEC-AKM-005, DEC-AKM-009, DEC-AKM-010, DEC-AKM-011)]
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/SamplerError.hpp"

using akm::Addressing;
using akm::Cancelled;
using akm::ChecksumMode;
using akm::Command;
using akm::CommandOptions;
using akm::CommandRequest;
using akm::CommandResult;
using akm::Confirmation;
using akm::DiagnosticKind;
using akm::Done;
using akm::Error;
using akm::ExpectedReply;
using akm::RefusalReason;
using akm::Refused;
using akm::RejectReason;
using akm::Reply;
using akm::ReplyId;
using akm::SequenceResult;
using akm::SessionTiming;
using akm::Timeout;
using akm::harness::ManualScenarioDriver;
using akm::harness::SamplerBehaviour;
using akm::harness::SamplerConfig;
using akm::test::Bytes;
using akm::test::bytes;
using akm::test::SessionHarness;
using namespace std::chrono_literals;

namespace
{
    // Short enough to keep the manual driver's 1 ms steps few, long enough to order events clearly.
    constexpr auto TIMEOUT = 50ms;
    constexpr auto TOTAL_WAIT = 200ms;
    constexpr auto WINDOW = 100ms;
    constexpr auto STEP = 1ms;

    SessionTiming fastTiming()
    {
        SessionTiming timing;
        timing.commandTimeout = TIMEOUT;
        timing.maxTotalWait = TOTAL_WAIT;
        return timing;
    }

    /// A §00 toggle command: one data byte, answered by DONE.
    Command toggle(std::uint8_t item, std::uint8_t value)
    {
        return Command{akm::test::SECTION_CONFIG, item, bytes({value})};
    }

    Command echoCommand()
    {
        return Command{akm::test::SECTION_CONFIG, akm::test::ITEM_ECHO, bytes({0x01, 0x02, 0x03, 0x04})};
    }

    /// The OS version (§02/&00): a REPLY the codec cannot delimit while the checksum mode is unknown.
    CommandRequest osVersionRequest()
    {
        CommandOptions options;
        options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
        return CommandRequest{Command{akm::test::SECTION_SYSTEM, akm::test::ITEM_OS_VERSION, {}}, options};
    }

    void makeSilent(SessionHarness& harness)
    {
        SamplerBehaviour behaviour;
        behaviour.silent = true;
        harness.sampler().setBehaviour(behaviour);
    }

    template <typename Alternative>
    bool is(const CommandResult& result)
    {
        return std::holds_alternative<Alternative>(result);
    }

    /// Makes a valid checksum wrong while keeping every byte a data byte.
    Bytes withBrokenChecksum(Bytes frame)
    {
        const std::size_t checksumIndex = frame.size() - akm::END_BYTE_SIZE - akm::test::CHECKSUM_SIZE;
        frame[checksumIndex] = static_cast<std::uint8_t>(frame[checksumIndex] ^ 0x01U);
        return frame;
    }

    /// The item byte of the frame the host sent, which tells the commands of a test apart on the wire.
    std::uint8_t itemOf(const Bytes& frame)
    {
        return frame.at(akm::test::SENT_ITEM_INDEX);
    }
}

// --- RQ-AKM-007: user-refs and matching ---

TEST_CASE("Given two commands submitted one after the other, When they are sent, Then their user-refs differ [RQ-AKM-007]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};

    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON));
    harness.settle();
    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_OFF));
    harness.settle();

    REQUIRE(harness.sentCount() == 2);
    CHECK(harness.userRefOf(0) != harness.userRefOf(1));
}

TEST_CASE("Given a command completed by a REPLY and another pending, When an ERROR carrying the completed command's user-ref arrives, Then it is reported as a late ERROR and the pending command stays pending [RQ-AKM-007]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};
    makeSilent(harness);

    harness.submit(echoCommand());
    harness.settle();
    harness.inject(harness.confirmationFor(0, ReplyId::Reply, bytes({0x01, 0x02, 0x03, 0x04})));
    harness.settle();
    REQUIRE(harness.recorder().count() == 1);

    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON));
    harness.settle();
    REQUIRE(harness.sentCount() == 2);

    // The ERROR names the echo, which the REPLY already completed (spec p. 6).
    harness.inject(harness.confirmationFor(0, ReplyId::Error, bytes({0x00, 0x02})));
    harness.settle();

    CHECK(harness.diagnostics().count(DiagnosticKind::LateErrorAfterReply) == 1);
    const auto reported = harness.diagnostics().last(DiagnosticKind::LateErrorAfterReply);
    REQUIRE(reported.has_value());
    REQUIRE(reported->confirmation.has_value());
    CHECK(reported->confirmation->userRefs == bytes({harness.userRefOf(0)}));
    CHECK(reported->confirmation->item == akm::test::ITEM_ECHO);
    // The toggle is untouched: still the only pending command.
    CHECK(harness.recorder().count() == 1);
}

TEST_CASE("Given a confirmation whose DeviceID is not the bound target, When it arrives, Then it is reported as unsolicited and completes nothing [RQ-AKM-007]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming(), SamplerConfig{3}};
    makeSilent(harness);

    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON));
    harness.settle();
    constexpr std::uint8_t OTHER_DEVICE_ID = 4;
    harness.inject(harness.confirmationFor(0, ReplyId::Done, {}, OTHER_DEVICE_ID));
    harness.settle();

    CHECK(harness.diagnostics().count(DiagnosticKind::UnsolicitedConfirmation) == 1);
    CHECK(harness.recorder().count() == 0);

    // The same DONE from the bound target does complete it.
    harness.inject(harness.confirmationFor(0, ReplyId::Done, {}, 3));
    harness.settle();
    REQUIRE(harness.recorder().results().size() == 1);
    CHECK(is<Done>(harness.recorder().results().front()));
}

// --- RQ-AKM-008: one outstanding command ---

TEST_CASE("Given three commands submitted back to back to a sampler that answers only when told to, When they are submitted, Then only the first frame is on the wire and each DONE releases the next [RQ-AKM-008]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};
    makeSilent(harness);

    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON));
    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_OFF));
    harness.submit(toggle(akm::test::ITEM_NOTIFICATION, akm::test::TOGGLE_ON));
    harness.settle();

    CHECK(harness.sentCount() == 1);

    harness.inject(harness.confirmationFor(0, ReplyId::Done));
    harness.settle();
    CHECK(harness.sentCount() == 2);

    harness.inject(harness.confirmationFor(1, ReplyId::Done));
    harness.settle();
    CHECK(harness.sentCount() == 3);

    harness.inject(harness.confirmationFor(2, ReplyId::Done));
    harness.settle();
    const std::vector<CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == 3);
    CHECK(is<Done>(results[0]));
    CHECK(is<Done>(results[1]));
    CHECK(is<Done>(results[2]));
    // Submission order, not completion order, decides what goes on the wire.
    const std::vector<Bytes> frames = harness.sentFrames();
    CHECK(itemOf(frames[0]) == akm::test::ITEM_AUTO_SCREEN_UPDATE);
    CHECK(itemOf(frames[2]) == akm::test::ITEM_NOTIFICATION);
}

// --- RQ-AKM-009: completion ---

TEST_CASE("Given notification off and a DONE without any OK, When it arrives, Then the command completes as successful [RQ-AKM-009]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};

    const auto notificationOff = harness.submitAndWait(
        CommandRequest{toggle(akm::test::ITEM_NOTIFICATION, akm::test::TOGGLE_OFF), {}});
    REQUIRE(notificationOff.has_value());
    REQUIRE(is<Done>(*notificationOff));

    const auto result = harness.submitAndWait(
        CommandRequest{toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON), {}});
    REQUIRE(result.has_value());
    CHECK(is<Done>(*result));
    // The sampler sent one confirmation only, the DONE.
    CHECK(harness.sampler().settings().notification == false);
}

TEST_CASE("Given an OK then a REPLY with data, When they arrive, Then the OK completes nothing and the command completes with the data [RQ-AKM-009]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};
    makeSilent(harness);

    harness.submit(echoCommand());
    harness.settle();

    harness.inject(harness.confirmationFor(0, ReplyId::Ok));
    harness.settle();
    CHECK(harness.recorder().count() == 0);

    harness.inject(harness.confirmationFor(0, ReplyId::Reply, bytes({0x01, 0x02, 0x03, 0x04})));
    harness.settle();
    const std::vector<CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == 1);
    REQUIRE(is<Reply>(results.front()));
    CHECK(std::get<Reply>(results.front()).data == bytes({0x01, 0x02, 0x03, 0x04}));
}

TEST_CASE("Given an OK then an ERROR 00 02, When they arrive, Then the command completes as failed with error number 2 [RQ-AKM-005, RQ-AKM-009]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};
    makeSilent(harness);

    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON));
    harness.settle();
    harness.inject(harness.confirmationFor(0, ReplyId::Ok));
    harness.inject(harness.confirmationFor(0, ReplyId::Error, bytes({0x00, 0x02})));
    harness.settle();

    const std::vector<CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == 1);
    REQUIRE(is<Error>(results.front()));
    CHECK(std::get<Error>(results.front()).number == akm::error_number::OUT_OF_RANGE);
}

// --- RQ-AKM-010, RQ-AKM-011: timeout, maximum total wait and Still Alive ---

TEST_CASE("Given a sampler that never answers, When the timeout elapses, Then the command times out and the next queued command is sent [RQ-AKM-010]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};
    makeSilent(harness);

    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON));
    harness.submit(toggle(akm::test::ITEM_NOTIFICATION, akm::test::TOGGLE_ON));
    harness.elapse(TIMEOUT - STEP);
    CHECK(harness.recorder().count() == 0);
    CHECK(harness.sentCount() == 1);

    harness.elapse(STEP);
    const std::vector<CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == 1);
    CHECK(is<Timeout>(results.front()));
    CHECK(harness.sentCount() == 2);
}

TEST_CASE("Given a per-command timeout of twice the session's and an answer at one and a half, When it arrives, Then the command succeeds [RQ-AKM-010]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};
    SamplerBehaviour behaviour;
    behaviour.replyDelay = TIMEOUT + TIMEOUT / 2;
    harness.sampler().setBehaviour(behaviour);

    CommandOptions options;
    options.timeout = 2 * TIMEOUT;
    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON), options);
    harness.elapse(2 * TIMEOUT);

    const std::vector<CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == 1);
    CHECK(is<Done>(results.front()));
}

TEST_CASE("Given Still Alive on and F0 F7 arriving twice per timeout until a DONE, When they arrive, Then the command succeeds and never times out [RQ-AKM-011]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};

    const auto stillAlive = harness.establishStillAlive(true);
    REQUIRE(stillAlive.has_value());
    REQUIRE(is<Done>(*stillAlive));
    REQUIRE(harness.session().stillAliveMonitoring());
    REQUIRE(harness.sampler().settings().stillAlive);

    SamplerBehaviour behaviour;
    behaviour.replyDelay = 3 * TIMEOUT;
    behaviour.stillAliveInterval = TIMEOUT / 2;
    harness.sampler().setBehaviour(behaviour);

    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON));
    harness.elapse(4 * TIMEOUT);

    const std::vector<CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == 1);
    CHECK(is<Done>(results.front()));
}

TEST_CASE("Given Still Alive off, When F0 F7 arrives, Then it does not restart the timeout [RQ-AKM-011]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};
    makeSilent(harness);
    REQUIRE_FALSE(harness.session().stillAliveMonitoring());

    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON));
    harness.elapse(TIMEOUT / 2);
    harness.inject(bytes({common::midi::SYSEX_START, common::midi::SYSEX_END}));
    harness.elapse(TIMEOUT / 2);

    const std::vector<CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == 1);
    CHECK(is<Timeout>(results.front()));
}

TEST_CASE("Given a sampler that only sends F0 F7, When the maximum total wait elapses, Then the command times out [RQ-AKM-010, RQ-AKM-011]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};

    REQUIRE(harness.establishStillAlive(true).has_value());
    SamplerBehaviour behaviour;
    // Far beyond the maximum total wait: the reply never comes, only the Still Alive messages.
    behaviour.replyDelay = 10 * TOTAL_WAIT;
    behaviour.stillAliveInterval = TIMEOUT / 2;
    harness.sampler().setBehaviour(behaviour);

    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON));
    harness.elapse(TOTAL_WAIT - STEP);
    CHECK(harness.recorder().count() == 0);

    harness.elapse(STEP);
    const std::vector<CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == 1);
    CHECK(is<Timeout>(results.front()));
}

// --- RQ-AKM-013, RQ-AKM-041: the tri-state checksum mode ---

TEST_CASE("Given the checksum mode unknown at session start, When the first command is sent, Then it carries a checksum [RQ-AKM-041]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};
    CHECK(harness.session().checksumMode() == ChecksumMode::Unknown);

    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON));
    harness.settle();

    REQUIRE(harness.sentCount() == 1);
    CHECK(harness.carriesChecksum(0, 1));
}

TEST_CASE("Given the checksum mode off, When mode on is set and its DONE arrives, Then the next command carries a checksum [RQ-AKM-013]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};

    REQUIRE(harness.establishChecksumMode(false).has_value());
    REQUIRE(harness.session().checksumMode() == ChecksumMode::Off);

    const std::size_t modeFrame = harness.sentCount();
    const auto switched = harness.establishChecksumMode(true);
    REQUIRE(switched.has_value());
    REQUIRE(is<Done>(*switched));
    // The mode command carries a checksum although the mode in force says none is expected (RQ-AKM-013).
    CHECK(harness.carriesChecksum(modeFrame, 1));
    CHECK(harness.session().checksumMode() == ChecksumMode::On);
    CHECK(harness.diagnostics().count(DiagnosticKind::ChecksumModeChanged) >= 1);

    const auto next = harness.submitAndWait(
        CommandRequest{toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON), {}});
    REQUIRE(next.has_value());
    CHECK(is<Done>(*next));
    CHECK(harness.carriesChecksum(harness.sentCount() - 1, 1));
}

TEST_CASE("Given the checksum mode on, When mode off is set and its DONE arrives, Then the next command carries none [RQ-AKM-013]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};

    REQUIRE(harness.establishChecksumMode(true).has_value());
    REQUIRE(harness.session().checksumMode() == ChecksumMode::On);

    const auto switched = harness.establishChecksumMode(false);
    REQUIRE(switched.has_value());
    REQUIRE(is<Done>(*switched));
    REQUIRE(harness.session().checksumMode() == ChecksumMode::Off);

    const auto next = harness.submitAndWait(
        CommandRequest{toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON), {}});
    REQUIRE(next.has_value());
    CHECK(is<Done>(*next));
    CHECK_FALSE(harness.carriesChecksum(harness.sentCount() - 1, 1));
}

TEST_CASE("Given a checksum-mode command that times out, When it completes, Then the mode is reported as unknown [RQ-AKM-013, RQ-AKM-041]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};

    REQUIRE(harness.establishChecksumMode(true).has_value());
    REQUIRE(harness.session().checksumMode() == ChecksumMode::On);

    makeSilent(harness);
    const auto timedOut = harness.establishChecksumMode(false);
    REQUIRE(timedOut.has_value());
    CHECK(is<Timeout>(*timedOut));
    CHECK(harness.session().checksumMode() == ChecksumMode::Unknown);

    const auto reported = harness.diagnostics().last(DiagnosticKind::ChecksumModeChanged);
    REQUIRE(reported.has_value());
    CHECK(reported->checksumMode == ChecksumMode::Unknown);
}

TEST_CASE("Given the checksum mode unknown, When a command whose reply needs a known mode is submitted, Then it is refused without any frame being sent [RQ-AKM-041]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};
    REQUIRE(harness.session().checksumMode() == ChecksumMode::Unknown);

    const auto refused = harness.submitAndWait(osVersionRequest());
    REQUIRE(refused.has_value());
    REQUIRE(is<Refused>(*refused));
    CHECK(std::get<Refused>(*refused).reason == RefusalReason::ChecksumModeUnknown);
    CHECK(harness.sentCount() == 0);

    // Once the mode is known, the same command goes out and returns the sampler's OS version.
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const auto version = harness.submitAndWait(osVersionRequest());
    REQUIRE(version.has_value());
    REQUIRE(is<Reply>(*version));
    CHECK(std::get<Reply>(*version).data == bytes({0x02, 0x0A}));
}

TEST_CASE("Given the checksum mode on and three confirmations in a row failing verification, When the third arrives, Then the mode becomes unknown [RQ-AKM-041]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};

    REQUIRE(harness.establishChecksumMode(true).has_value());
    REQUIRE(harness.session().checksumMode() == ChecksumMode::On);
    makeSilent(harness);

    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON));
    harness.settle();
    const std::size_t inFlight = harness.sentCount() - 1;
    const Bytes broken = withBrokenChecksum(harness.confirmationFor(inFlight, ReplyId::Ok, {}, 0, true));

    for (int failures = 1; failures <= akm::DEFAULT_CHECKSUM_FAILURES_BEFORE_UNKNOWN; ++failures)
    {
        harness.inject(broken);
        harness.settle();
        const bool expectedUnknown = failures >= akm::DEFAULT_CHECKSUM_FAILURES_BEFORE_UNKNOWN;
        CHECK((harness.session().checksumMode() == ChecksumMode::Unknown) == expectedUnknown);
    }

    CHECK(harness.diagnostics().count(DiagnosticKind::RejectedMessage)
          == static_cast<std::size_t>(akm::DEFAULT_CHECKSUM_FAILURES_BEFORE_UNKNOWN));
    const auto reported = harness.diagnostics().last(DiagnosticKind::ChecksumModeChanged);
    REQUIRE(reported.has_value());
    CHECK(reported->checksumMode == ChecksumMode::Unknown);
}

// --- RQ-AKM-012: the collection window discovery is built on ---

TEST_CASE("Given two samplers with DeviceIDs 3 and 7 and a broadcast command with a collection window, When the window ends, Then both answers were collected and the command completed as successful [RQ-AKM-012]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    // Declared before the harness, so that no callback of the session can touch it after it is gone.
    std::vector<std::uint8_t> responders;
    SessionHarness harness{driver, fastTiming(), SamplerConfig{3}};
    harness.backend().addSampler(SamplerConfig{7});

    CommandOptions options;
    options.addressing = Addressing::Broadcast;
    options.collectionWindow = WINDOW;
    options.onConfirmation = [&responders](const Confirmation& confirmation) {
        responders.push_back(confirmation.deviceId);
    };

    harness.submit(Command{akm::test::SECTION_CONFIG, akm::test::ITEM_QUERY, {}}, options);
    harness.elapse(WINDOW - STEP);
    CHECK(harness.recorder().count() == 0);
    CHECK_FALSE(responders.empty());

    harness.elapse(STEP);
    const std::vector<CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == 1);
    CHECK(is<Done>(results.front()));

    // Both samplers answered, each with its own DeviceID (OK and DONE from each).
    CHECK(std::count(responders.begin(), responders.end(), 3) == 2);
    CHECK(std::count(responders.begin(), responders.end(), 7) == 2);
    // The frame went out addressed to everyone.
    CHECK(harness.sentFrames().at(0).at(akm::DEVICE_BYTE_INDEX) == 0);
}

TEST_CASE("Given no sampler answering a windowed command, When the window ends, Then it completes as successful with nothing collected [RQ-AKM-012]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    std::vector<std::uint8_t> responders;
    SessionHarness harness{driver, fastTiming()};
    makeSilent(harness);

    CommandOptions options;
    options.addressing = Addressing::Broadcast;
    options.collectionWindow = WINDOW;
    options.onConfirmation = [&responders](const Confirmation& confirmation) {
        responders.push_back(confirmation.deviceId);
    };

    harness.submit(Command{akm::test::SECTION_CONFIG, akm::test::ITEM_QUERY, {}}, options);
    harness.elapse(WINDOW);

    const std::vector<CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == 1);
    CHECK(is<Done>(results.front()));
    CHECK(responders.empty());
}

// --- RQ-AKM-043: sequences ---

TEST_CASE("Given a sequence of three commands and an ERROR answering the first, When it runs, Then the others are never sent, complete as cancelled, and the failure index is the first [RQ-AKM-043]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};

    constexpr std::uint8_t OUT_OF_RANGE_VALUE = 5;
    const auto outcome = harness.submitSequenceAndWait(
        {CommandRequest{toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, OUT_OF_RANGE_VALUE), {}},
         CommandRequest{toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON), {}},
         CommandRequest{toggle(akm::test::ITEM_NOTIFICATION, akm::test::TOGGLE_ON), {}}});

    REQUIRE(outcome.has_value());
    REQUIRE(outcome->results.size() == 3);
    REQUIRE(is<Error>(outcome->results[0]));
    CHECK(std::get<Error>(outcome->results[0]).number == akm::error_number::OUT_OF_RANGE);
    CHECK(is<Cancelled>(outcome->results[1]));
    CHECK(is<Cancelled>(outcome->results[2]));
    CHECK(outcome->failureIndex == 0);
    CHECK_FALSE(outcome->allSucceeded());
    CHECK(harness.sentCount() == 1);
}

TEST_CASE("Given a sequence running and another command submitted meanwhile, When the sequence ends, Then the other command ran after it and not between its commands [RQ-AKM-043]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};

    harness.session().submitSequence({CommandRequest{toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_OFF), {}},
                                      CommandRequest{toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON), {}},
                                      CommandRequest{toggle(akm::test::ITEM_SYNC_LCD, akm::test::TOGGLE_ON), {}}},
                                     harness.recorder().sequenceCompletion());
    harness.submit(toggle(akm::test::ITEM_NOTIFICATION, akm::test::TOGGLE_ON));
    REQUIRE(harness.waitForCompletions(2));

    const std::vector<Bytes> frames = harness.sentFrames();
    REQUIRE(frames.size() == 4);
    CHECK(itemOf(frames[0]) == akm::test::ITEM_AUTO_SCREEN_UPDATE);
    CHECK(itemOf(frames[1]) == akm::test::ITEM_AUTO_SCREEN_UPDATE);
    CHECK(itemOf(frames[2]) == akm::test::ITEM_SYNC_LCD);
    CHECK(itemOf(frames[3]) == akm::test::ITEM_NOTIFICATION);
}

TEST_CASE("Given a sequence whose commands all succeed, When it ends, Then each result is reported in order [RQ-AKM-043]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};

    const auto outcome = harness.submitSequenceAndWait(
        {CommandRequest{toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON), {}},
         CommandRequest{echoCommand(), {}},
         CommandRequest{toggle(akm::test::ITEM_NOTIFICATION, akm::test::TOGGLE_ON), {}}});

    REQUIRE(outcome.has_value());
    REQUIRE(outcome->results.size() == 3);
    CHECK(is<Done>(outcome->results[0]));
    REQUIRE(is<Reply>(outcome->results[1]));
    CHECK(std::get<Reply>(outcome->results[1]).data == bytes({0x01, 0x02, 0x03, 0x04}));
    CHECK(is<Done>(outcome->results[2]));
    CHECK(outcome->allSucceeded());
}

// --- RQ-AKM-020, RQ-AKM-042: queueing, refusals and closing ---

TEST_CASE("Given a completion that submits a command while others are queued, When it runs, Then the new command is queued behind them [RQ-AKM-020]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};
    makeSilent(harness);

    harness.session().submit(CommandRequest{toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON), {}},
                             [&harness](const CommandResult&) {
                                 harness.submit(toggle(akm::test::ITEM_SYNC_LCD, akm::test::TOGGLE_ON));
                             });
    harness.submit(toggle(akm::test::ITEM_NOTIFICATION, akm::test::TOGGLE_ON));
    harness.submit(toggle(akm::test::ITEM_ECHO, akm::test::TOGGLE_ON));
    harness.settle();

    for (std::size_t sent = 0; sent < 3; ++sent)
    {
        harness.inject(harness.confirmationFor(sent, ReplyId::Done));
        harness.settle();
    }

    const std::vector<Bytes> frames = harness.sentFrames();
    REQUIRE(frames.size() == 4);
    CHECK(itemOf(frames[1]) == akm::test::ITEM_NOTIFICATION);
    CHECK(itemOf(frames[2]) == akm::test::ITEM_ECHO);
    // What the first command's completion submitted went last, behind what was already queued.
    CHECK(itemOf(frames[3]) == akm::test::ITEM_SYNC_LCD);
}

TEST_CASE("Given a command carrying a byte above 7F, When it is submitted, Then it is refused without any frame being sent and never from the submitting thread [RQ-AKM-001, RQ-AKM-020]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};

    constexpr std::uint8_t NOT_A_DATA_BYTE = 0x80;
    harness.submit(Command{akm::test::SECTION_CONFIG, akm::test::ITEM_AUTO_SCREEN_UPDATE, bytes({NOT_A_DATA_BYTE})});
    // Nothing has run yet: submit() only posted a task.
    CHECK(harness.recorder().count() == 0);

    harness.settle();
    const std::vector<CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == 1);
    REQUIRE(is<Refused>(results.front()));
    CHECK(std::get<Refused>(results.front()).reason == RefusalReason::NotEncodable);
    CHECK(harness.sentCount() == 0);
}

TEST_CASE("Given a session with no target bound, When a command addressed to the target is submitted, Then it is refused, while a broadcast command still goes out [RQ-AKM-007, RQ-AKM-039]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming(), SamplerConfig{}, false};
    CHECK_FALSE(harness.session().boundTarget().has_value());

    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON));
    harness.settle();
    const std::vector<CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == 1);
    REQUIRE(is<Refused>(results.front()));
    CHECK(std::get<Refused>(results.front()).reason == RefusalReason::NoTargetBound);
    CHECK(harness.sentCount() == 0);

    CommandOptions broadcast;
    broadcast.addressing = Addressing::Broadcast;
    harness.submit(Command{akm::test::SECTION_CONFIG, akm::test::ITEM_QUERY, {}}, broadcast);
    harness.settle();
    CHECK(harness.sentCount() == 1);
}

TEST_CASE("Given a command in flight and two queued, When the session is closed, Then all three complete as cancelled and the input port is stopped [RQ-AKM-042]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};
    makeSilent(harness);

    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON));
    harness.submit(toggle(akm::test::ITEM_NOTIFICATION, akm::test::TOGGLE_ON));
    harness.submit(toggle(akm::test::ITEM_SYNC_LCD, akm::test::TOGGLE_ON));
    harness.settle();
    REQUIRE(harness.sentCount() == 1);
    REQUIRE(harness.inputStarted());

    harness.closeAndWait();

    const std::vector<CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == 3);
    CHECK(is<Cancelled>(results[0]));
    CHECK(is<Cancelled>(results[1]));
    CHECK(is<Cancelled>(results[2]));
    CHECK_FALSE(harness.inputStarted());
    CHECK(harness.sentCount() == 1);
}

TEST_CASE("Given a close requested from a completion, When it is called, Then it is refused [RQ-AKM-042]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    bool refused = false;
    SessionHarness harness{driver, fastTiming()};

    harness.session().submit(CommandRequest{toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON), {}},
                             [&harness, &refused](const CommandResult&) {
                                 refused = !harness.session().close([] {});
                             });
    REQUIRE(harness.waitUntil([&refused] { return refused; }));

    CHECK(refused);
    CHECK(harness.inputStarted());
}

TEST_CASE("Given a closed session, When a command is submitted, Then it is refused [RQ-AKM-042]", "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};
    harness.closeAndWait();

    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON));
    harness.settle();

    const std::vector<CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == 1);
    REQUIRE(is<Refused>(results.front()));
    CHECK(std::get<Refused>(results.front()).reason == RefusalReason::SessionClosed);
    CHECK(harness.sentCount() == 0);
}

// --- RQ-AKM-006: what is not a confirmation for us ---

TEST_CASE("Given a foreign message and a truncated one, When they arrive, Then each is reported as rejected and nothing completes [RQ-AKM-006]",
          "[akm][session]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver, fastTiming()};
    makeSilent(harness);

    harness.submit(toggle(akm::test::ITEM_AUTO_SCREEN_UPDATE, akm::test::TOGGLE_ON));
    harness.settle();

    constexpr std::uint8_t OTHER_MANUFACTURER = 0x43;
    harness.inject(bytes({common::midi::SYSEX_START, OTHER_MANUFACTURER, 0x00, 0x01, common::midi::SYSEX_END}));
    harness.inject(bytes({common::midi::SYSEX_START, akm::AKAI_MANUFACTURER_ID, akm::SAMPLER_MODEL_ID,
                          common::midi::SYSEX_END}));
    harness.settle();

    CHECK(harness.diagnostics().count(DiagnosticKind::RejectedMessage) == 2);
    const std::vector<akm::Diagnostic> events = harness.diagnostics().events();
    CHECK(events.at(0).rejection == RejectReason::Foreign);
    CHECK(events.at(1).rejection == RejectReason::Truncated);
    CHECK(harness.recorder().count() == 0);
}
