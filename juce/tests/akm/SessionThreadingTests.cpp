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

// The session on real threads: confirmations delivered by the simulation's own thread, even before send()
// returns, a confirmation racing its timeout, and a close while a command is on the wire. What these prove
// cannot be proved on manual time, where everything runs on one thread.
// [TASK-AKM-006, RQ-AKM-010, RQ-AKM-016, RQ-AKM-020, ADR-AKM-001 (DEC-AKM-004, DEC-AKM-005)]
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <thread>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"

using akm::Cancelled;
using akm::Command;
using akm::CommandRequest;
using akm::CommandResult;
using akm::Done;
using akm::RefusalReason;
using akm::Refused;
using akm::SessionTiming;
using akm::Timeout;
using akm::harness::DeliveryMode;
using akm::harness::RealScenarioDriver;
using akm::harness::SamplerBehaviour;
using akm::test::bytes;
using akm::test::SessionHarness;
using namespace std::chrono_literals;

namespace
{
    // Real threads need real time: long enough for a thousand round trips on a loaded CI runner.
    constexpr auto REAL_WAIT = 30s;
    constexpr std::size_t ORDERED_COMMANDS = 1000;
    constexpr std::size_t CONCURRENT_COMMANDS = 50;
    constexpr int RACE_REPEATS = 50;
    constexpr auto RACE_TIMEOUT = 5ms;

    Command toggle(std::uint8_t value)
    {
        return Command{akm::test::SECTION_CONFIG, akm::test::ITEM_AUTO_SCREEN_UPDATE, bytes({value})};
    }

    template <typename Alternative>
    bool is(const CommandResult& result)
    {
        return std::holds_alternative<Alternative>(result);
    }
}

TEST_CASE("Given a thousand commands answered from the simulation's own thread, When they complete, Then the completions arrive in submission order, on one thread that is not the submitting one [RQ-AKM-020]",
          "[akm][session][threads]")
{
    RealScenarioDriver driver;
    // Declared before the harness, so that no completion can touch them after they are gone.
    std::mutex mutex;
    std::vector<std::size_t> order;
    std::vector<std::thread::id> threads;

    SessionHarness harness{driver};
    harness.backend().setDeliveryMode(DeliveryMode::OnOtherThread);

    for (std::size_t index = 0; index < ORDERED_COMMANDS; ++index)
    {
        harness.session().submit(CommandRequest{toggle(akm::test::TOGGLE_ON), {}},
                                 [&mutex, &order, &threads, index](const CommandResult&) {
                                     const std::lock_guard lock(mutex);
                                     order.push_back(index);
                                     threads.push_back(std::this_thread::get_id());
                                 });
    }

    REQUIRE(harness.waitUntil(
        [&mutex, &order] {
            const std::lock_guard lock(mutex);
            return order.size() >= ORDERED_COMMANDS;
        },
        REAL_WAIT));

    const std::lock_guard lock(mutex);
    REQUIRE(order.size() == ORDERED_COMMANDS);
    CHECK(std::is_sorted(order.begin(), order.end()));
    CHECK(std::adjacent_find(order.begin(), order.end()) == order.end());
    // One session thread, and never the test's own.
    CHECK(std::count(threads.begin(), threads.end(), threads.front()) == static_cast<long>(ORDERED_COMMANDS));
    CHECK(threads.front() != std::this_thread::get_id());
}

TEST_CASE("Given a simulation that delivers its confirmations before send() returns, When commands are sent, Then each completes exactly once [RQ-AKM-016, RQ-AKM-020]",
          "[akm][session][threads]")
{
    RealScenarioDriver driver;
    SessionHarness harness{driver};
    harness.backend().setDeliveryMode(DeliveryMode::OnOtherThreadBeforeSendReturns);

    for (std::size_t index = 0; index < CONCURRENT_COMMANDS; ++index)
        harness.submit(toggle(akm::test::TOGGLE_ON));

    REQUIRE(harness.waitForCompletions(CONCURRENT_COMMANDS, REAL_WAIT));
    const std::vector<CommandResult> results = harness.recorder().results();
    REQUIRE(results.size() == CONCURRENT_COMMANDS);
    CHECK(std::all_of(results.begin(), results.end(), [](const CommandResult& result) { return is<Done>(result); }));
}

TEST_CASE("Given a confirmation arriving just as the timeout falls due, When they race, Then the command completes exactly once [RQ-AKM-010, RQ-AKM-020]",
          "[akm][session][threads]")
{
    for (int repeat = 0; repeat < RACE_REPEATS; ++repeat)
    {
        RealScenarioDriver driver;
        SessionTiming timing;
        timing.commandTimeout = RACE_TIMEOUT;
        SessionHarness harness{driver, timing};
        harness.backend().setDeliveryMode(DeliveryMode::OnOtherThread);

        SamplerBehaviour behaviour;
        behaviour.replyDelay = RACE_TIMEOUT;
        harness.sampler().setBehaviour(behaviour);

        harness.submit(toggle(akm::test::TOGGLE_ON));
        REQUIRE(harness.waitForCompletions(1, REAL_WAIT));
        // Let the loser of the race arrive, if it has not already.
        harness.elapse(4 * RACE_TIMEOUT);

        const std::vector<CommandResult> results = harness.recorder().results();
        REQUIRE(results.size() == 1);
        CHECK((is<Done>(results.front()) || is<Timeout>(results.front())));
    }
}

TEST_CASE("Given commands in flight and queued on real threads, When the session is closed, Then close returns and every command completed exactly once [RQ-AKM-042]",
          "[akm][session][threads]")
{
    RealScenarioDriver driver;
    SessionTiming timing;
    timing.commandTimeout = RACE_TIMEOUT;
    SessionHarness harness{driver, timing};
    harness.backend().setDeliveryMode(DeliveryMode::OnOtherThread);

    SamplerBehaviour behaviour;
    behaviour.replyDelay = 2 * RACE_TIMEOUT;
    harness.sampler().setBehaviour(behaviour);

    for (std::size_t index = 0; index < CONCURRENT_COMMANDS; ++index)
        harness.submit(toggle(akm::test::TOGGLE_ON));

    harness.closeAndWait();

    // Every command completed exactly once and none was left pending. A command submitted just before the
    // close may reach the queue after it, and is then refused rather than cancelled: it never went out
    // either way.
    const std::vector<CommandResult> results = harness.recorder().results();
    CHECK(results.size() == CONCURRENT_COMMANDS);
    CHECK(std::all_of(results.begin(), results.end(), [](const CommandResult& result) {
        return is<Done>(result) || is<Timeout>(result) || is<Cancelled>(result)
               || (std::holds_alternative<Refused>(result)
                   && std::get<Refused>(result).reason == RefusalReason::SessionClosed);
    }));
    CHECK_FALSE(harness.inputStarted());
}
