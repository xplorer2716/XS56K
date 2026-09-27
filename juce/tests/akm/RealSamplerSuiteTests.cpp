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

// The real-sampler suite: what the owner runs against the real S5000 (`xs56k_akm_probe --suite`), and what CI runs
// first against the simulated sampler, so that its checks, its log and the state it leaves are checked before any
// hardware is involved. [TASK-AKM-010, RQ-AKM-010, RQ-AKM-011, RQ-AKM-015, RQ-AKM-017, RQ-AKM-018, RQ-AKM-019,
// RQ-AKM-039, RQ-AKM-040, RQ-AKM-041, RQ-AKM-042, ADR-AKM-001 (DEC-AKM-007, DEC-AKM-008)]
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "AwkwardSampler.hpp"
#include "SeededPrograms.hpp"
#include "akm/harness/RealSamplerSuite.hpp"
#include "akm/harness/ScenarioDriver.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"

using Catch::Matchers::ContainsSubstring;
using namespace std::chrono_literals;
using akm::harness::CheckOutcome;
using akm::harness::CheckReport;
using akm::harness::DeliveryMode;
using akm::harness::ManualScenarioDriver;
using akm::harness::OsVersion;
using akm::harness::RealScenarioDriver;
using akm::harness::RealSuiteOptions;
using akm::harness::RealSuiteResult;
using akm::harness::SamplerBehaviour;
using akm::harness::SamplerConfig;
using akm::harness::SamplerSettings;
using akm::harness::ScenarioTarget;
using akm::harness::SimulatedMidiBackend;
using akm::harness::SimulatedSampler;

namespace
{
    constexpr std::size_t AUTOMATIC_CHECKS = 7;
    constexpr std::size_t ECHO_ROUND_TRIPS = 50;

    constexpr std::uint8_t SECTION_SYSEX_CONFIG = 0x00;
    constexpr std::uint8_t SECTION_SYSTEM = 0x02;
    constexpr std::uint8_t SECTION_PROGRAM = 0x0A;
    constexpr std::uint8_t SECTION_DISK_TOOLS = 0x10;
    constexpr std::uint8_t ITEM_UPDATE_DISK_LIST = 0x01;
    constexpr std::uint8_t ITEM_SYNC_LCD = 0x03;
    constexpr std::uint8_t ITEM_AUTO_SCREEN_UPDATE = 0x05;
    constexpr std::uint8_t ITEM_ECHO = 0x06;
    constexpr std::uint8_t ITEM_CHECKSUM_MODE = 0x04;
    constexpr std::uint16_t ERROR_UNKNOWN = 3;

    // The settings the suite leaves the sampler with: the ones found at the first contact (checksum off,
    // notification on, Still Alive off) and the spec's or assumed defaults for the other two.
    SamplerSettings knownState()
    {
        SamplerSettings settings;
        settings.checksum = false;
        settings.stillAlive = false;
        settings.notification = true;
        settings.syncLcd = true;
        settings.autoScreenUpdate = false;
        return settings;
    }

    SamplerConfig config(std::uint8_t deviceId, int major = 2, int minor = 10)
    {
        SamplerConfig sampler;
        sampler.deviceId = deviceId;
        sampler.osVersion = OsVersion{major, minor};
        return sampler;
    }

    // A manual-time driver, a backend on its scheduler and one simulated sampler.
    struct Rig
    {
        explicit Rig(SamplerConfig samplerConfig = {}) : sampler(backend.addSampler(samplerConfig)) {}

        [[nodiscard]] RealSuiteOptions options(std::uint32_t targetDeviceId = 0) const
        {
            RealSuiteOptions suite;
            suite.target = ScenarioTarget{backend.inputName(), backend.outputName(), targetDeviceId};
            return suite;
        }

        ManualScenarioDriver driver;
        SimulatedMidiBackend backend{driver.scheduler()};
        SimulatedSampler& sampler;
    };

    // The report of the check whose title contains `part`.
    const CheckReport& reportOf(const RealSuiteResult& result, const std::string& part)
    {
        const auto found = std::find_if(result.checks.begin(), result.checks.end(),
                                        [&part](const CheckReport& report) { return report.title.find(part) != std::string::npos; });
        REQUIRE(found != result.checks.end());
        return *found;
    }

    void checkAllPassed(const RealSuiteResult& result)
    {
        for (const CheckReport& report : result.checks)
        {
            CAPTURE(report.title, report.detail);
            CHECK(report.outcome == CheckOutcome::Passed);
        }
    }

    std::size_t occurrences(const std::string& text, const std::string& part)
    {
        std::size_t count = 0;
        for (std::size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size()))
            ++count;
        return count;
    }
}

TEST_CASE("Given the simulated sampler, When the suite runs, Then its seven checks pass, the OS version is read, 50 Echo round trips are timed, nothing is rejected and the sampler ends in the known state [TASK-AKM-010]",
          "[akm][suite]")
{
    Rig rig;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, rig.options(), log);

    CHECK(result.portsOpened);
    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS);
    checkAllPassed(result);
    CHECK(result.passed());
    CHECK(result.discoveredDeviceIds == std::vector<std::uint8_t>{0});
    REQUIRE(result.osVersion.has_value());
    CHECK(result.osVersion->major == 2);
    CHECK(result.osVersion->minor == 10);
    CHECK(result.osVersion->subVersion == 0);
    CHECK(result.echoRoundTrips == ECHO_ROUND_TRIPS);
    CHECK(result.echoLatencies.size() == ECHO_ROUND_TRIPS);
    CHECK(result.rejectedMessages == 0);
    CHECK(result.unsolicitedConfirmations == 0);
    CHECK(result.lateErrors == 0);
    CHECK(result.knownStateRestored);
    CHECK(rig.sampler.settings() == knownState());
    CHECK_THAT(log.str(), ContainsSubstring("# observation: 7 checks passed, 0 failed, 0 skipped"));
}

TEST_CASE("Given the suite, When it runs, Then it sends only section 00 items and the two version items of section 02, so no program, multi or sample can have changed [TASK-AKM-010, RQ-AKM-018]",
          "[akm][suite]")
{
    Rig rig;
    std::ostringstream log;

    static_cast<void>(akm::harness::runRealSamplerSuite(rig.backend, rig.driver, rig.options(), log));

    const auto commands = rig.sampler.acceptedCommands();
    REQUIRE_FALSE(commands.empty());
    for (const auto& command : commands)
    {
        CAPTURE(static_cast<int>(command.section), static_cast<int>(command.item));
        const bool sysExConfig = command.section == SECTION_SYSEX_CONFIG;
        const bool osVersion = command.section == SECTION_SYSTEM && (command.item == 0x00 || command.item == 0x01);
        CHECK((sysExConfig || osVersion));
    }
}

TEST_CASE("Given a sampler left with checksums on, notification off, Sync LCD off, Auto screen update on and Still Alive on, When the suite runs, Then every check passes and it ends in the known state [TASK-AKM-010, RQ-AKM-018]",
          "[akm][suite]")
{
    Rig rig;
    akm::test::leaveInAwkwardState(rig.backend);
    const SamplerSettings before = rig.sampler.settings();
    REQUIRE(before.checksum);
    REQUIRE_FALSE(before.notification);
    REQUIRE_FALSE(before.syncLcd);
    REQUIRE(before.autoScreenUpdate);
    REQUIRE(before.stillAlive);
    std::ostringstream log;
    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, rig.options(), log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS);
    checkAllPassed(result);
    CHECK(result.knownStateRestored);
    CHECK(rig.sampler.settings() == knownState());
}

TEST_CASE("Given no sampler answering, When the suite runs, Then the first check fails, the others are skipped and nothing is sent after the discovery [TASK-AKM-010, RQ-AKM-039]",
          "[akm][suite]")
{
    Rig rig;
    SamplerBehaviour behaviour;
    behaviour.silent = true;
    rig.sampler.setBehaviour(behaviour);
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, rig.options(), log);

    CHECK(result.portsOpened);
    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS);
    CHECK(result.checks.front().outcome == CheckOutcome::Failed);
    CHECK_THAT(result.checks.front().detail, ContainsSubstring("no sampler at the target DeviceID"));
    CHECK(result.count(CheckOutcome::Skipped) == AUTOMATIC_CHECKS - 1);
    CHECK_FALSE(result.passed());
    CHECK(result.framesSent == 1);
    CHECK(rig.backend.sentByHost().size() == 1);
    CHECK(result.knownStateRestored);
}

TEST_CASE("Given a sampler whose DeviceID is not the target's, When the suite runs, Then the failure names the DeviceID that answered and nothing is sent after the discovery [TASK-AKM-010, RQ-AKM-039]",
          "[akm][suite]")
{
    Rig rig{config(3)};
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, rig.options(0), log);

    REQUIRE_FALSE(result.checks.empty());
    CHECK(result.checks.front().outcome == CheckOutcome::Failed);
    CHECK_THAT(result.checks.front().detail, ContainsSubstring("DeviceIDs that answered the discovery: 3"));
    CHECK(result.discoveredDeviceIds == std::vector<std::uint8_t>{3});
    CHECK(result.framesSent == 1);
}

TEST_CASE("Given a sampler with DeviceID 3 and the target 3, When the suite runs, Then every check passes on that DeviceID [TASK-AKM-010, RQ-AKM-007, RQ-AKM-039]",
          "[akm][suite]")
{
    Rig rig{config(3)};
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, rig.options(3), log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS);
    checkAllPassed(result);
    CHECK(result.discoveredDeviceIds == std::vector<std::uint8_t>{3});
    CHECK(result.unsolicitedConfirmations == 0);
    CHECK(rig.sampler.settings() == knownState());
}

TEST_CASE("Given a sampler on an OS that lacks Sync LCD and Still Alive, When the suite runs, Then the sessions open degraded, every check passes and only what the sampler has is put back [TASK-AKM-010, RQ-AKM-040, RQ-AKM-042]",
          "[akm][suite]")
{
    Rig rig{config(0, 1, 30)};
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, rig.options(), log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS);
    checkAllPassed(result);
    REQUIRE(result.osVersion.has_value());
    CHECK(result.osVersion->major == 1);
    CHECK(result.osVersion->minor == 30);
    CHECK_THAT(reportOf(result, "open a session").detail, ContainsSubstring("degraded"));
    CHECK_THAT(reportOf(result, "open a session").detail, ContainsSubstring("unsupported"));
    CHECK(result.knownStateRestored);
    CHECK_FALSE(rig.sampler.settings().checksum);
    CHECK(rig.sampler.settings().notification);
}

TEST_CASE("Given the option not to touch the LCD settings, When the suite runs, Then neither Sync LCD nor Auto screen update is sent [TASK-AKM-010]",
          "[akm][suite]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.touchLcdSettings = false;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS);
    checkAllPassed(result);
    REQUIRE_FALSE(rig.sampler.acceptedCommands().empty());
    for (const auto& command : rig.sampler.acceptedCommands())
    {
        const bool lcdSetting = command.section == SECTION_SYSEX_CONFIG
                                && (command.item == ITEM_SYNC_LCD || command.item == ITEM_AUTO_SCREEN_UPDATE);
        CHECK_FALSE(lcdSetting);
    }
}

TEST_CASE("Given an Echo the sampler refuses, When the suite runs, Then the checks that need it fail, the others pass, and the sampler still ends in the known state [TASK-AKM-010, RQ-AKM-015, RQ-AKM-018]",
          "[akm][suite]")
{
    Rig rig;
    SamplerBehaviour behaviour;
    behaviour.itemErrors = {{SECTION_SYSEX_CONFIG, ITEM_ECHO, 0x01}};
    rig.sampler.setBehaviour(behaviour);
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, rig.options(), log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS);
    CHECK(reportOf(result, "open a session").outcome == CheckOutcome::Passed);
    CHECK(reportOf(result, "Echo returns").outcome == CheckOutcome::Failed);
    CHECK(reportOf(result, "round trips").outcome == CheckOutcome::Failed);
    CHECK(reportOf(result, "operating system version").outcome == CheckOutcome::Passed);
    CHECK(reportOf(result, "checksums on and off").outcome == CheckOutcome::Failed);
    CHECK(reportOf(result, "closing puts back").outcome == CheckOutcome::Passed);
    CHECK(reportOf(result, "fails half way").outcome == CheckOutcome::Passed);
    CHECK(result.count(CheckOutcome::Failed) == 3);
    CHECK_FALSE(result.passed());
    CHECK(result.echoRoundTrips == 0);
    CHECK(result.knownStateRestored);
    CHECK(rig.sampler.settings() == knownState());
}

TEST_CASE("Given a sampler that refuses to switch the checksum mode, When the suite runs, Then every check fails at its opening and the run says the known state is not confirmed [TASK-AKM-010, RQ-AKM-018, RQ-AKM-042]",
          "[akm][suite]")
{
    Rig rig;
    SamplerBehaviour behaviour;
    behaviour.itemErrors = {{SECTION_SYSEX_CONFIG, ITEM_CHECKSUM_MODE, ERROR_UNKNOWN}};
    rig.sampler.setBehaviour(behaviour);
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, rig.options(), log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS);
    CHECK(result.count(CheckOutcome::Failed) == AUTOMATIC_CHECKS);
    CHECK_THAT(result.checks.front().detail, ContainsSubstring("a setting failed or timed out"));
    CHECK_FALSE(result.knownStateRestored);
    CHECK_THAT(log.str(), ContainsSubstring("NOT put back: checksum mode"));
    CHECK_THAT(log.str(), ContainsSubstring("# observation: sampler NOT confirmed in the known state"));
}

TEST_CASE("Given a sampler that answers after 10 ms, When the suite runs, Then the Echo latencies are 10 ms and the log says so [TASK-AKM-010, RQ-AKM-017]",
          "[akm][suite]")
{
    Rig rig;
    SamplerBehaviour behaviour;
    behaviour.replyDelay = 10ms;
    rig.sampler.setBehaviour(behaviour);
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, rig.options(), log);

    REQUIRE(result.echoLatencies.size() == ECHO_ROUND_TRIPS);
    for (const auto latency : result.echoLatencies)
    {
        CHECK(latency >= 10ms);
        CHECK(latency <= 12ms);
    }
    CHECK_THAT(log.str(), ContainsSubstring("50 Echo round trips of 50: min 10 ms"));
    CHECK_THAT(log.str(), ContainsSubstring("95th percentile"));
}

TEST_CASE("Given a sampler slower than the command timeout, When the suite runs, Then the latency check fails and says why [TASK-AKM-010, RQ-AKM-010]",
          "[akm][suite]")
{
    Rig rig;
    SamplerBehaviour behaviour;
    behaviour.replyDelay = 40ms;
    rig.sampler.setBehaviour(behaviour);
    RealSuiteOptions options = rig.options();
    options.commandTimeout = 30ms;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    // Every command of the opening times out: the first check fails, the sampler is never confirmed in the known state.
    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS);
    CHECK(result.count(CheckOutcome::Failed) == AUTOMATIC_CHECKS);
    CHECK(result.echoRoundTrips == 0);
}

TEST_CASE("Given a run, When its log is read, Then it holds the header, one line per check, one OUT line per frame sent and one IN line per message received, then the observations [TASK-AKM-010, RQ-AKM-017]",
          "[akm][suite]")
{
    Rig rig;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, rig.options(), log);

    const std::string text = log.str();
    CHECK(result.framesSent == rig.backend.sentByHost().size());
    CHECK(result.framesReceived == rig.backend.emittedBySamplers().size());
    CHECK(occurrences(text, "  OUT  ") == result.framesSent);
    CHECK(occurrences(text, "  IN   ") == result.framesReceived);
    CHECK_THAT(text, ContainsSubstring("# XS56K AKM real-sampler suite"));
    CHECK_THAT(text, ContainsSubstring("# check 1: open a session and close it"));
    CHECK_THAT(text, ContainsSubstring("# check 7: a check that fails half way leaves the sampler in the known state"));
    const std::size_t observations = text.find("# observations");
    REQUIRE(observations != std::string::npos);
    CHECK(text.rfind("  OUT  ") < observations);
    CHECK(text.rfind("  IN   ") < observations);
    CHECK_THAT(text, ContainsSubstring("# observation: discovery answered by DeviceIDs: 0"));
    CHECK_THAT(text, ContainsSubstring("# observation: OS version 2.10 (sub-version 0)"));
    CHECK_THAT(text, ContainsSubstring("# observation: F0 F7 messages seen: 0"));
    CHECK_THAT(text, ContainsSubstring("# observation: checksum mode as the sessions followed it: unknown -> off"));
    CHECK_THAT(text, ContainsSubstring("# observation: sampler left in the known state"));
}

TEST_CASE("Given a run, When it ends, Then the last frames it sent are the closing commands of the last session, checksums off first [TASK-AKM-010, RQ-AKM-018, RQ-AKM-042]",
          "[akm][suite]")
{
    Rig rig;
    std::ostringstream log;

    static_cast<void>(akm::harness::runRealSamplerSuite(rig.backend, rig.driver, rig.options(), log));

    const auto commands = rig.sampler.acceptedCommands();
    REQUIRE(commands.size() >= 3);
    // The last session of the run is the one of the check that fails half way: it changed the checksum mode and the
    // defaults of its open (Sync LCD off, Still Alive on); its close puts back the checksum mode, then Still Alive, then
    // Sync LCD.
    const auto& last = commands[commands.size() - 3];
    CHECK(last.section == SECTION_SYSEX_CONFIG);
    CHECK(last.item == ITEM_CHECKSUM_MODE);
    REQUIRE(last.data.size() == 1);
    CHECK(last.data.front() == 0);
    CHECK(commands.back().item == ITEM_SYNC_LCD);
    CHECK(commands.back().data.front() == 1);
}

TEST_CASE("Given the option for the slow operation, When the suite runs on a sampler without a disk section, Then one section 10 command is sent, the sampler's ERROR is an observation and the check passes [TASK-AKM-010, RQ-AKM-011]",
          "[akm][suite]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.slowOperation = true;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 1);
    checkAllPassed(result);
    CHECK_THAT(result.checks.back().detail, ContainsSubstring("ERROR 0"));
    CHECK_THAT(result.checks.back().detail, ContainsSubstring("the sampler refused the command"));
    std::size_t diskCommands = 0;
    for (const auto& command : rig.sampler.acceptedCommands())
    {
        if (command.section == SECTION_DISK_TOOLS)
        {
            ++diskCommands;
            CHECK(command.item == ITEM_UPDATE_DISK_LIST);
        }
    }
    CHECK(diskCommands == 1);
    CHECK(rig.sampler.settings() == knownState());
}

TEST_CASE("Given a sampler that takes 2.5 seconds to answer, When the suite runs with the slow operation, Then the F0 F7 messages reach the host, the session does not time out and the check says the backend delivers them [TASK-AKM-010, RQ-AKM-010, RQ-AKM-011]",
          "[akm][suite]")
{
    Rig rig;
    SamplerBehaviour behaviour;
    behaviour.replyDelay = 2500ms;
    rig.sampler.setBehaviour(behaviour);
    RealSuiteOptions options = rig.options();
    options.slowOperation = true;
    options.commandTimeout = 3s;
    options.discoveryWindow = 3s;  // longer than the sampler takes to answer the discovery
    options.echoRepeats = 2;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 1);
    const CheckReport& slow = result.checks.back();
    CAPTURE(slow.detail);
    CHECK(slow.outcome == CheckOutcome::Passed);
    CHECK_THAT(slow.detail, ContainsSubstring("the backend delivers them"));
    CHECK(result.stillAliveMessagesSeen > 0);
}

TEST_CASE("Given the option for the power cycle and an owner who power-cycles the sampler, When the suite runs, Then the session recovers from the sampler's own settings and the check says the checksum mode did not survive [TASK-AKM-010, RQ-AKM-017, RQ-AKM-041]",
          "[akm][suite]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.powerCycle = true;
    std::vector<std::string> asked;
    options.askOwner = [&rig, &asked](const std::string& instruction) {
        asked.push_back(instruction);
        rig.sampler.powerCycle();
        return true;
    };
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 1);
    const CheckReport& cycle = result.checks.back();
    CAPTURE(cycle.detail);
    CHECK(cycle.outcome == CheckOutcome::Passed);
    CHECK_THAT(cycle.detail, ContainsSubstring("did not survive"));
    CHECK(result.rejectedMessages > 0);
    // The session lost the mode after three confirmations that failed verification, which a normal run never does.
    CHECK_THAT(log.str(), ContainsSubstring("-> unknown"));
    REQUIRE(asked.size() == 1);
    CHECK_THAT(asked.front(), ContainsSubstring("Switch the sampler off and on"));
    CHECK(result.knownStateRestored);
    CHECK(rig.sampler.settings() == knownState());
}

TEST_CASE("Given the option for the power cycle and an owner who does nothing to the sampler, When the suite runs, Then the first Echo is answered with checksums still on and the check says they survived [TASK-AKM-010, RQ-AKM-017]",
          "[akm][suite]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.powerCycle = true;
    options.askOwner = [](const std::string&) { return true; };
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 1);
    CHECK(result.checks.back().outcome == CheckOutcome::Passed);
    CHECK_THAT(result.checks.back().detail, ContainsSubstring("survived the power cycle"));
    CHECK(result.rejectedMessages == 0);
    CHECK(rig.sampler.settings() == knownState());
}

TEST_CASE("Given the option for the power cycle and an owner who declines, When the suite runs, Then the check is skipped and the sampler still ends in the known state [TASK-AKM-010, RQ-AKM-018]",
          "[akm][suite]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.powerCycle = true;
    options.askOwner = [](const std::string&) { return false; };
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 1);
    CHECK(result.checks.back().outcome == CheckOutcome::Skipped);
    CHECK_THAT(result.checks.back().detail, ContainsSubstring("declined"));
    CHECK(result.passed());
    CHECK(result.knownStateRestored);
    CHECK(rig.sampler.settings() == knownState());
}

TEST_CASE("Given the option for the power cycle and no way to ask the owner, When the suite runs, Then the check is skipped and nothing is changed for it [TASK-AKM-010]",
          "[akm][suite]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.powerCycle = true;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 1);
    CHECK(result.checks.back().outcome == CheckOutcome::Skipped);
    CHECK_THAT(result.checks.back().detail, ContainsSubstring("no way to ask"));
    CHECK(rig.sampler.settings() == knownState());
}

TEST_CASE("Given no Echo round trips asked for, When the suite runs, Then the latency check is skipped [TASK-AKM-010]",
          "[akm][suite]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.echoRepeats = 0;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS);
    CHECK(result.count(CheckOutcome::Skipped) == 1);
    CHECK(result.count(CheckOutcome::Passed) == AUTOMATIC_CHECKS - 1);
    CHECK(result.passed());
    CHECK(result.echoLatencies.empty());
}

TEST_CASE("Given the real scheduler and a sampler that answers from its own thread, When the suite runs, Then every check passes and the sampler ends in the known state [TASK-AKM-010, RQ-AKM-019, RQ-AKM-020]",
          "[akm][suite][threads]")
{
    RealScenarioDriver driver;
    SimulatedMidiBackend backend(driver.scheduler());
    SimulatedSampler& sampler = backend.addSampler();
    backend.setDeliveryMode(DeliveryMode::OnOtherThread);
    RealSuiteOptions options;
    options.target = ScenarioTarget{backend.inputName(), backend.outputName(), 0};
    options.echoRepeats = 5;
    options.commandTimeout = 5s;
    options.discoveryWindow = 50ms;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(backend, driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS);
    checkAllPassed(result);
    CHECK(result.echoRoundTrips == 5);
    CHECK(result.knownStateRestored);
    CHECK(result.rejectedMessages == 0);
    CHECK(sampler.settings() == knownState());
}

TEST_CASE("Given a target that is not on the backend, When the suite runs, Then it reports that the ports could not be opened [TASK-AKM-010]",
          "[akm][suite]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.target.inputPortName = "no such port";
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    CHECK_FALSE(result.portsOpened);
    CHECK(result.checks.empty());
    CHECK_THAT(log.str(), ContainsSubstring("input port not found"));
}

TEST_CASE("Given a sampler holding programs KEEP1 and KEEP2 with KEEP1 selected, When the suite runs with the program lifecycle checks, Then both pass, KEEP1 is current again and only KEEP1 and KEEP2 remain [TASK-AKM-024, RQ-AKM-027]",
          "[akm][suite]")
{
    Rig rig;
    akm::test::seedPrograms(rig.backend, {"KEEP1", "KEEP2"}, 0);
    RealSuiteOptions options = rig.options();
    options.programLifecycle = true;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    checkAllPassed(result);
    // expect()'s "as expected" lines go to the log, not to a check's own detail (built from finding()
    // calls only) - matching how the existing power-cycle tests read the same kind of assertion.
    CHECK_THAT(log.str(), ContainsSubstring("navigated away to the program that was current before"));
    CHECK_THAT(log.str(), ContainsSubstring("refused before sending"));
    CHECK_THAT(log.str(), ContainsSubstring("the programs it held before (2)"));
    CHECK_THAT(log.str(), ContainsSubstring("the program that was current before is current again"));
}

TEST_CASE("Given the default options, When the suite runs, Then it never sends a section 0A command, and the program lifecycle checks do not run [TASK-AKM-024, RQ-AKM-027]",
          "[akm][suite]")
{
    Rig rig;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, rig.options(), log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS);
    for (const auto& command : rig.sampler.acceptedCommands())
        CHECK(command.section != SECTION_PROGRAM);
}

TEST_CASE("Given no program current when the suite runs with the program lifecycle checks, Then the wrong-program refusal is skipped and no program is current again afterward [TASK-AKM-024, RQ-AKM-027]",
          "[akm][suite]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.programLifecycle = true;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    checkAllPassed(result);
    CHECK_THAT(reportOf(result, "reserved test name").detail,
              ContainsSubstring("wrong-program refusal is not exercised"));
}
