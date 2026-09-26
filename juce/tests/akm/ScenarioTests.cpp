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

// The ScenarioDriver hides how time and idleness are waited for, so that one scenario, written against
// `MidiBackend&`, runs on the simulated sampler in CI (manual time) and on the real one (real time).
// The Echo scenario is that first shared scenario; `juce/tests/compile_checks` builds the same source
// against JuceMidiBackend. [TASK-AKM-007, RQ-AKM-016, RQ-AKM-019, ADR-AKM-001 (DEC-AKM-008)]
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <utility>
#include <vector>

#include "akm/testing/EchoScenario.hpp"
#include "akm/testing/ScenarioDriver.hpp"
#include "akm/testing/SimulatedMidiBackend.hpp"

using namespace std::chrono_literals;
using akm::testing::DeliveryMode;
using akm::testing::EchoResult;
using akm::testing::ManualScenarioDriver;
using akm::testing::RealScenarioDriver;
using akm::testing::SamplerBehaviour;
using akm::testing::SamplerConfig;
using akm::testing::ScenarioTarget;
using akm::testing::SimulatedMidiBackend;

namespace
{
    constexpr std::array<std::uint8_t, 4> echoPayload{0x12, 0x34, 0x56, 0x78};

    ScenarioTarget targetOf(const SimulatedMidiBackend& backend, std::uint32_t deviceId = 0)
    {
        return ScenarioTarget{backend.inputName(), backend.outputName(), deviceId};
    }
}

TEST_CASE("Given the manual driver, When it lets 100 ms elapse, Then the tasks that fall due run, and what they post to the executor, in time order [RQ-AKM-016]",
          "[akm][scenario][manual]")
{
    ManualScenarioDriver driver;
    std::vector<int> order;
    driver.scheduler().scheduleAfter(50ms, [&] {
        order.push_back(50);
        driver.executor().post([&order] { order.push_back(51); });
    });
    driver.scheduler().scheduleAfter(80ms, [&order] { order.push_back(80); });
    driver.scheduler().scheduleAfter(200ms, [&order] { order.push_back(200); });
    const auto start = driver.scheduler().now();

    driver.elapse(100ms);

    CHECK(order == std::vector<int>{50, 51, 80});
    CHECK(driver.scheduler().now() == start + 100ms);
}

TEST_CASE("Given a condition that already holds, When the manual driver waits for it, Then it returns at once without letting time pass [RQ-AKM-016]",
          "[akm][scenario][manual]")
{
    ManualScenarioDriver driver;
    const auto start = driver.scheduler().now();

    const bool held = driver.waitUntil([] { return true; }, 5s);

    CHECK(held);
    CHECK(driver.scheduler().now() == start);
}

TEST_CASE("Given a condition that a task makes true after 300 ms, When the manual driver waits for it, Then it returns as soon as it holds [RQ-AKM-016]",
          "[akm][scenario][manual]")
{
    ManualScenarioDriver driver;
    bool flag = false;
    driver.scheduler().scheduleAfter(300ms, [&flag] { flag = true; });
    const auto start = driver.scheduler().now();

    const bool held = driver.waitUntil([&flag] { return flag; }, 5s);

    CHECK(held);
    const auto elapsed = driver.scheduler().now() - start;
    CHECK(elapsed >= 300ms);
    CHECK(elapsed < 310ms);
}

TEST_CASE("Given a condition that never holds, When the manual driver waits for it, Then it gives up after exactly the timeout of scenario time, in no real time [RQ-AKM-016, RQ-AKM-010]",
          "[akm][scenario][manual]")
{
    ManualScenarioDriver driver;
    const auto start = driver.scheduler().now();
    const auto realStart = std::chrono::steady_clock::now();

    const bool held = driver.waitUntil([] { return false; }, 30s);

    CHECK_FALSE(held);
    CHECK(driver.scheduler().now() - start == 30s);
    // Thirty seconds of scenario time cost well under a second of test time (RQ-AKM-016).
    CHECK(std::chrono::steady_clock::now() - realStart < 5s);
}

TEST_CASE("Given the real driver, When a condition becomes true from another thread, Then waitUntil returns true [RQ-AKM-016]",
          "[akm][scenario][real]")
{
    RealScenarioDriver driver;
    std::atomic<bool> flag{false};
    std::thread setter([&flag] {
        std::this_thread::sleep_for(50ms);
        flag = true;
    });

    const bool held = driver.waitUntil([&flag] { return flag.load(); }, 10s);
    setter.join();

    CHECK(held);
}

TEST_CASE("Given the real driver and a condition that never holds, When it waits, Then it gives up after the timeout [RQ-AKM-016]",
          "[akm][scenario][real]")
{
    RealScenarioDriver driver;
    const auto start = std::chrono::steady_clock::now();

    const bool held = driver.waitUntil([] { return false; }, 100ms);

    CHECK_FALSE(held);
    CHECK(std::chrono::steady_clock::now() - start >= 100ms);
}

TEST_CASE("Given the Echo scenario on the simulated sampler, When it runs, Then the sampler echoes the four bytes [RQ-AKM-016, RQ-AKM-015, RQ-AKM-019]",
          "[akm][scenario][echo]")
{
    ManualScenarioDriver driver;
    SimulatedMidiBackend backend(driver.scheduler());
    backend.addSampler();

    const EchoResult result = akm::testing::runEchoScenario(backend, driver, targetOf(backend), echoPayload);

    CHECK(result.answered);
    CHECK(result.echoed == echoPayload);
    CHECK(result.failure.empty());
}

TEST_CASE("Given the Echo scenario, When it runs in each checksum mode, Then the sampler in mode off echoes it every time [RQ-AKM-016, RQ-AKM-041]",
          "[akm][scenario][echo]")
{
    for (const akm::ChecksumMode mode : {akm::ChecksumMode::Off, akm::ChecksumMode::Unknown})
    {
        ManualScenarioDriver driver;
        SimulatedMidiBackend backend(driver.scheduler());
        backend.addSampler();

        const EchoResult result = akm::testing::runEchoScenario(backend, driver, targetOf(backend), echoPayload, mode);

        CHECK(result.answered);
        CHECK(result.echoed == echoPayload);
    }
}

TEST_CASE("Given a sampler that stays silent, When the Echo scenario runs, Then it reports no answer after its timeout [RQ-AKM-016, RQ-AKM-010]",
          "[akm][scenario][echo]")
{
    ManualScenarioDriver driver;
    SimulatedMidiBackend backend(driver.scheduler());
    backend.addSampler().setBehaviour(SamplerBehaviour{.silent = true});
    const auto start = driver.scheduler().now();

    const EchoResult result = akm::testing::runEchoScenario(backend, driver, targetOf(backend), echoPayload,
                                                            akm::ChecksumMode::Unknown, 2s);

    CHECK_FALSE(result.answered);
    CHECK_FALSE(result.failure.empty());
    CHECK(driver.scheduler().now() - start == 2s);
}

TEST_CASE("Given a sampler that answers after 1500 ms, When the Echo scenario runs with a timeout of 2 s and of 1 s, Then it answers in the first case only [RQ-AKM-016, RQ-AKM-010]",
          "[akm][scenario][echo]")
{
    for (const auto& [timeout, expectAnswer] : {std::pair{2s, true}, std::pair{1s, false}})
    {
        ManualScenarioDriver driver;
        SimulatedMidiBackend backend(driver.scheduler());
        backend.addSampler().setBehaviour(SamplerBehaviour{.replyDelay = 1500ms});

        const EchoResult result = akm::testing::runEchoScenario(backend, driver, targetOf(backend), echoPayload,
                                                                akm::ChecksumMode::Unknown, timeout);

        CHECK(result.answered == expectAnswer);
    }
}

TEST_CASE("Given a sampler with another DeviceID, When the Echo scenario targets DeviceID 5, Then it gets no answer [RQ-AKM-016]",
          "[akm][scenario][echo]")
{
    ManualScenarioDriver driver;
    SimulatedMidiBackend backend(driver.scheduler());
    backend.addSampler(SamplerConfig{.deviceId = 3});

    const EchoResult result = akm::testing::runEchoScenario(backend, driver, targetOf(backend, 5), echoPayload);

    CHECK_FALSE(result.answered);
}

TEST_CASE("Given a target whose ports do not exist, When the Echo scenario runs, Then it reports it without waiting [RQ-AKM-016]",
          "[akm][scenario][echo]")
{
    ManualScenarioDriver driver;
    SimulatedMidiBackend backend(driver.scheduler());
    backend.addSampler();
    const auto start = driver.scheduler().now();

    const EchoResult result = akm::testing::runEchoScenario(backend, driver, ScenarioTarget{"nowhere in", "nowhere out", 0},
                                                            echoPayload);

    CHECK_FALSE(result.answered);
    CHECK_FALSE(result.failure.empty());
    CHECK(driver.scheduler().now() == start);
}

TEST_CASE("Given the Echo scenario, When it runs on the real driver against a sampler answering from another thread, Then it is answered [RQ-AKM-016, RQ-AKM-020]",
          "[akm][scenario][echo][real]")
{
    RealScenarioDriver driver;
    SimulatedMidiBackend backend(driver.scheduler());
    backend.addSampler();
    backend.setDeliveryMode(DeliveryMode::OnOtherThread);

    const EchoResult result = akm::testing::runEchoScenario(backend, driver, targetOf(backend), echoPayload,
                                                            akm::ChecksumMode::Unknown, 10s);

    CHECK(result.answered);
    CHECK(result.echoed == echoPayload);
}
