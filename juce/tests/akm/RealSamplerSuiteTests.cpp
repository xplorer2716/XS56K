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
#include "HostProbe.hpp"
#include "SeededPrograms.hpp"
#include "TestBytes.hpp"
#include "akm/Command.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/SystemSetup.hpp"
#include "akm/harness/ClockArithmetic.hpp"
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
    constexpr std::uint8_t SECTION_SAMPLE = 0x0E;
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

    // Seeds a sample's settable §0E parameters (RQ-AKM-048) via raw frames, so the sampler ends exactly
    // as a real one holding this sample, with these values already set, would, before any session is
    // opened against it — mirroring SeededPrograms.hpp's own approach for §0A. Values distinct from
    // allSampleParameterCases()'s own test values, and Original Pitch inside its 21-127 range: unlike
    // every other settable item, its range excludes 0, so the mock's own zero-filled "unset" default is
    // not a value GuardedTestSample's restore step could legitimately resend, a case a real sample
    // never presents (its pitch is always a real value already, wherever it came from).
    void seedSampleParameters(akm::harness::SimulatedMidiBackend& backend, const std::string& name)
    {
        akm::test::HostProbe host(backend, backend.inputName(), backend.outputName());
        std::uint8_t userRef = 0x01;
        const auto send = [&](const akm::CommandRequest& request) {
            const akm::EncodeResult frame = akm::encodeCommand(0, akm::test::Bytes{userRef++}, request.command, akm::ChecksumMode::Off);
            host.send(frame.bytes);
        };
        send(akm::makeStringRequest(akm::ItemId::SampleSelectByName, name));
        send(akm::makeRequest(akm::ItemId::SampleSetStartPosition, {1, 1, 1, 1}));
        send(akm::makeRequest(akm::ItemId::SampleSetEndPosition, {2, 2, 2, 2}));
        send(akm::makeRequest(akm::ItemId::SampleSetOriginalPitch, {50}));
        send(akm::makeRequest(akm::ItemId::SampleSetSemitoneTune, {0, 5}));
        send(akm::makeRequest(akm::ItemId::SampleSetFineTune, {1, 10}));
        send(akm::makeRequest(akm::ItemId::SampleSetPlaybackMode, {0}));
        send(akm::makeRequest(akm::ItemId::SampleSetLoopStart, {0, 0, 0, 0}));
        send(akm::makeRequest(akm::ItemId::SampleSetLoopEnd, {0, 0, 0, 1}));
    }

    // What the owner's sampler holds of its system setup before any session is opened (RQ-AKM-058): a name, a clock
    // — Saturday 14 March 2026, 09:26:53 — a Play Mode (2, Sample) and a lock; seeded through raw frames and the
    // simulated sampler's own setters, the way `seedSampleParameters` does for a sample.
    constexpr akm::ClockDate OWNER_CLOCK{2026, 3, 14, 7, 9, 26, 53};
    constexpr std::uint8_t OWNER_PLAY_MODE_SAMPLE = 2;
    constexpr std::uint8_t LOCK_NORMAL = 0;
    constexpr std::uint8_t LOCK_LOCKED = 1;
    constexpr std::uint8_t HIGHEST_PLAY_MODE_OF_THE_SPEC_COLUMN = 2;
    // The suite puts the clock back advanced by the time it measured, and reads it back to a few seconds.
    constexpr std::int64_t CLOCK_RESTORE_TOLERANCE_SECONDS = 3;
    constexpr std::uint8_t ITEM_CLEAR_MEMORY = 0x32;
    constexpr std::uint8_t ITEM_OS_VERSION = 0x00;
    constexpr std::uint8_t ITEM_OS_SUB_VERSION = 0x01;

    void seedSystemSetup(SimulatedMidiBackend& backend, SimulatedSampler& sampler, const std::string& name,
                         std::uint8_t playMode, std::uint8_t lock)
    {
        akm::test::HostProbe host(backend, backend.inputName(), backend.outputName());
        std::uint8_t userRef = 0x01;
        const auto send = [&](const akm::CommandRequest& request) {
            const akm::EncodeResult frame = akm::encodeCommand(0, akm::test::Bytes{userRef++}, request.command, akm::ChecksumMode::Off);
            host.send(frame.bytes);
        };
        send(akm::makeStringRequest(akm::ItemId::SystemSetName, name));
        send(akm::makeRequest(akm::ItemId::SystemSetClock,
                              {OWNER_CLOCK.year, OWNER_CLOCK.month, OWNER_CLOCK.day, OWNER_CLOCK.dayOfWeek,
                               OWNER_CLOCK.hours, OWNER_CLOCK.minutes, OWNER_CLOCK.seconds}));
        sampler.setPlayMode(playMode);
        sampler.setFrontPanelLock(lock);
    }

    // The clock the simulated sampler holds, as a date.
    akm::ClockDate clockOf(const SimulatedSampler& sampler)
    {
        const auto& bytes = sampler.systemSetup().clock;
        constexpr int BITS_PER_DATA_BYTE = 7;
        return akm::ClockDate{(bytes[0] << BITS_PER_DATA_BYTE) | bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]};
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

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 4);
    checkAllPassed(result);
    // expect()'s "as expected" lines go to the log, not to a check's own detail (built from finding()
    // calls only) - matching how the existing power-cycle tests read the same kind of assertion.
    CHECK_THAT(log.str(), ContainsSubstring("navigated away to the program that was current before"));
    CHECK_THAT(log.str(), ContainsSubstring("refused before sending"));
    CHECK_THAT(log.str(), ContainsSubstring("the programs it held before (2)"));
    CHECK_THAT(log.str(), ContainsSubstring("the program that was current before is current again"));
}

TEST_CASE("Given a sampler holding programs KEEP1 and KEEP2 with KEEP1 selected, When the suite runs with the program lifecycle checks, Then the keygroup check round-trips every section 08 item and only KEEP1 and KEEP2 remain [TASK-AKM-033, RQ-AKM-030, RQ-AKM-031, RQ-AKM-033]",
          "[akm][suite]")
{
    Rig rig;
    akm::test::seedPrograms(rig.backend, {"KEEP1", "KEEP2"}, 0);
    RealSuiteOptions options = rig.options();
    options.programLifecycle = true;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 4);
    checkAllPassed(result);
    CHECK_THAT(reportOf(result, "round-trip every §08 parameter item").detail,
              ContainsSubstring("keygroup parameter items of the six groups round-tripped"));
    // expect()'s "as expected" lines go to the log, not to a check's own detail, as above.
    CHECK_THAT(log.str(), ContainsSubstring("all 3 keygroups read back Low Note 50"));
    // Catches, on the mock too, the real-S5000 bug (TASK-AKM-033) where the test program's guard ran
    // its cleanup after closeAndVerify had already closed the session it needs, leaving the test
    // program undeleted: this asserts the program count the check itself verifies is restored.
    CHECK_THAT(log.str(), ContainsSubstring("the number of programs is back to what it was before (2)"));
}

TEST_CASE("Given a sampler holding programs KEEP1 and KEEP2 with KEEP1 selected and no --sample-name, When the suite runs with the program lifecycle checks, Then the zone check round-trips every section 06 item, the zone-0 and keygroup-0+zone-0 shapes pass, sample assignment is reported as skipped, and only KEEP1 and KEEP2 remain [TASK-AKM-038, RQ-AKM-034, RQ-AKM-036, RQ-AKM-038]",
          "[akm][suite]")
{
    Rig rig;
    akm::test::seedPrograms(rig.backend, {"KEEP1", "KEEP2"}, 0);
    RealSuiteOptions options = rig.options();
    options.programLifecycle = true;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 4);
    checkAllPassed(result);
    const std::string& zoneCheckDetail = reportOf(result, "round-trip every §06 parameter item").detail;
    CHECK_THAT(zoneCheckDetail, ContainsSubstring("zone parameter items round-tripped"));
    CHECK_THAT(zoneCheckDetail, ContainsSubstring("sample assignment: skipped"));
    // expect()'s "as expected" lines go to the log, not to a check's own detail, as above.
    CHECK_THAT(log.str(), ContainsSubstring("all 4 zones of keygroup 2 read back Level 77"));
    CHECK_THAT(log.str(), ContainsSubstring("2 keygroups' 4 zones read back Level 88"));
    CHECK_THAT(log.str(), ContainsSubstring("the number of programs is back to what it was before (2)"));
}

TEST_CASE("Given a sample name the simulated sampler holds, When the suite runs with the program lifecycle checks and --sample-name, Then it is assigned to zone 1 and read back [TASK-AKM-038, RQ-AKM-035, RQ-AKM-038]",
          "[akm][suite]")
{
    Rig rig;
    rig.sampler.setSampleNames({"KICK"});
    RealSuiteOptions options = rig.options();
    options.programLifecycle = true;
    options.sampleName = "KICK";
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 4);
    checkAllPassed(result);
    CHECK_THAT(reportOf(result, "round-trip every §06 parameter item").detail,
              ContainsSubstring("\"KICK\" assigned to zone 1"));
}

TEST_CASE("Given no --sample-name, When the suite runs with the sample lifecycle check, Then it is reported as skipped and no section 0E command is sent [TASK-AKM-045, RQ-AKM-051]",
          "[akm][suite]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.sampleLifecycle = true;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 1);
    CHECK(reportOf(result, "round-trip every §0E lifecycle").outcome == CheckOutcome::Skipped);
    for (const auto& command : rig.sampler.acceptedCommands())
        CHECK(command.section != SECTION_SAMPLE);
}

TEST_CASE("Given a sample name the simulated sampler holds, When the suite runs with the sample lifecycle check, Then it renames it and back, starts and stops auditioning it, round-trips every settable item, confirms the grouped replies, and restores its name and parameters, without --program-lifecycle [TASK-AKM-045, RQ-AKM-048, RQ-AKM-049, RQ-AKM-051]",
          "[akm][suite]")
{
    Rig rig;
    rig.sampler.setSampleNames({"KICK"});
    seedSampleParameters(rig.backend, "KICK");
    RealSuiteOptions options = rig.options();
    options.sampleLifecycle = true;
    options.sampleName = "KICK";
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 1);
    checkAllPassed(result);
    CHECK_THAT(reportOf(result, "round-trip every §0E lifecycle").detail,
              ContainsSubstring("settable sample parameter items round-tripped"));
    for (const auto& command : rig.sampler.acceptedCommands())
        CHECK_FALSE((command.section == SECTION_SAMPLE && (command.item == 0x07 || command.item == 0x08)));
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

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 4);
    checkAllPassed(result);
    CHECK_THAT(reportOf(result, "reserved test name").detail,
              ContainsSubstring("wrong-program refusal is not exercised"));
}


TEST_CASE("Given a sampler with a name, a clock, a Play Mode and a lock of its own, When the suite runs with the system setup checks, Then it round-trips all four Play Modes, the lock, the name and the clock, puts every one back, and never sends Clear Sampler Memory [TASK-AKM-053, RQ-AKM-052, RQ-AKM-054, RQ-AKM-055, RQ-AKM-056, RQ-AKM-058]",
          "[akm][suite]")
{
    Rig rig;
    seedSystemSetup(rig.backend, rig.sampler, "OWNER S5000", OWNER_PLAY_MODE_SAMPLE, LOCK_NORMAL);
    RealSuiteOptions options = rig.options();
    options.systemSetup = true;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    checkAllPassed(result);
    const std::string& detail = reportOf(result, "front-panel lock and clock").detail;
    CHECK_THAT(detail, ContainsSubstring("play mode 3 (Muted) accepted"));
    CHECK_THAT(detail, ContainsSubstring("clock restored"));
    CHECK_THAT(log.str(), ContainsSubstring("the sampler's name is back to \"OWNER S5000\""));

    const akm::harness::SystemSetupState after = rig.sampler.systemSetup();
    CHECK(after.name == "OWNER S5000");
    CHECK(after.playMode == OWNER_PLAY_MODE_SAMPLE);
    CHECK(after.frontPanelLock == LOCK_NORMAL);
    const std::int64_t drift = akm::harness::secondsBetween(OWNER_CLOCK, clockOf(rig.sampler));
    CHECK(drift >= 0);
    CHECK(drift <= CLOCK_RESTORE_TOLERANCE_SECONDS);
    for (const auto& command : rig.sampler.acceptedCommands())
        CHECK_FALSE((command.section == SECTION_SYSTEM && command.item == ITEM_CLEAR_MEMORY));
}

TEST_CASE("Given a sampler that follows the spec's column and refuses Play Mode 3, When the suite runs with the system setup checks, Then the check still passes, the refusal is an observation, and the Play Mode is back [TASK-AKM-053, RQ-AKM-055, RQ-AKM-057]",
          "[akm][suite]")
{
    Rig rig;
    seedSystemSetup(rig.backend, rig.sampler, "OWNER S5000", OWNER_PLAY_MODE_SAMPLE, LOCK_NORMAL);
    rig.sampler.setHighestPlayMode(HIGHEST_PLAY_MODE_OF_THE_SPEC_COLUMN);
    RealSuiteOptions options = rig.options();
    options.systemSetup = true;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    checkAllPassed(result);
    CHECK_THAT(reportOf(result, "front-panel lock and clock").detail, ContainsSubstring("play mode 3 (Muted) refused"));
    CHECK(rig.sampler.systemSetup().playMode == OWNER_PLAY_MODE_SAMPLE);
}

TEST_CASE("Given a check made to fail after the front panel was locked, When the suite runs with the system setup checks, Then the panel reads normal again, and so do the name and the Play Mode [TASK-AKM-053, RQ-AKM-058]",
          "[akm][suite]")
{
    Rig rig;
    seedSystemSetup(rig.backend, rig.sampler, "OWNER S5000", OWNER_PLAY_MODE_SAMPLE, LOCK_NORMAL);
    RealSuiteOptions options = rig.options();
    options.systemSetup = true;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    const CheckReport& failedHalfWay = reportOf(result, "fails half way and still puts back");
    CHECK(failedHalfWay.outcome == CheckOutcome::Passed);
    CHECK_THAT(log.str(), ContainsSubstring("this check fails on purpose, with the front panel locked"));
    CHECK_THAT(log.str(), ContainsSubstring("the front panel is back to normal"));
    CHECK(rig.sampler.systemSetup().frontPanelLock == LOCK_NORMAL);
}

TEST_CASE("Given a front panel that was locked before the suite ran, When it runs with the system setup checks, Then the panel is left as it was found, locked [TASK-AKM-053, RQ-AKM-058]",
          "[akm][suite]")
{
    Rig rig;
    seedSystemSetup(rig.backend, rig.sampler, "OWNER S5000", OWNER_PLAY_MODE_SAMPLE, LOCK_LOCKED);
    RealSuiteOptions options = rig.options();
    options.systemSetup = true;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    checkAllPassed(result);
    CHECK(rig.sampler.systemSetup().frontPanelLock == LOCK_LOCKED);
}

TEST_CASE("Given the default options, When the suite runs, Then the system setup checks do not run and only the two version items of section 02 are sent [TASK-AKM-053, RQ-AKM-058]",
          "[akm][suite]")
{
    Rig rig;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, rig.options(), log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS);
    for (const auto& command : rig.sampler.acceptedCommands())
        CHECK_FALSE((command.section == SECTION_SYSTEM && command.item != ITEM_OS_VERSION && command.item != ITEM_OS_SUB_VERSION));
}

TEST_CASE("Given a sampler whose clock cannot be read, When the suite runs with the system setup checks, Then the clock is reported as not tested and is never set, and everything else is round-tripped and put back [TASK-AKM-053, RQ-AKM-054, RQ-AKM-058]",
          "[akm][suite]")
{
    constexpr std::uint8_t ITEM_GET_CLOCK = 0x05;
    constexpr std::uint8_t ITEM_SET_CLOCK = 0x06;
    constexpr std::uint16_t OUT_OF_RANGE = 0x02;
    Rig rig;
    seedSystemSetup(rig.backend, rig.sampler, "OWNER S5000", OWNER_PLAY_MODE_SAMPLE, LOCK_NORMAL);
    rig.sampler.setBehaviour(SamplerBehaviour{.itemErrors = {{SECTION_SYSTEM, ITEM_GET_CLOCK, OUT_OF_RANGE}}});
    RealSuiteOptions options = rig.options();
    options.systemSetup = true;
    std::ostringstream log;
    const auto isSetClock = [](const auto& command) { return command.section == SECTION_SYSTEM && command.item == ITEM_SET_CLOCK; };
    const auto acceptedBefore = rig.sampler.acceptedCommands();
    const auto clockSetsBefore = std::count_if(acceptedBefore.begin(), acceptedBefore.end(), isSetClock);

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    checkAllPassed(result);
    CHECK_THAT(reportOf(result, "front-panel lock and clock").detail, ContainsSubstring("clock NOT TESTED"));
    CHECK_THAT(reportOf(result, "front-panel lock and clock").detail, ContainsSubstring("play mode 3 (Muted) accepted"));
    CHECK_THAT(log.str(), ContainsSubstring("the clock was not read, so it was never changed and is not restored"));
    const auto acceptedAfter = rig.sampler.acceptedCommands();
    const auto clockSetsAfter = std::count_if(acceptedAfter.begin(), acceptedAfter.end(), isSetClock);
    // The seeding sent the one Set Clock there was; the suite sent none.
    CHECK(clockSetsAfter == clockSetsBefore);
    CHECK(rig.sampler.systemSetup().name == "OWNER S5000");
    CHECK(rig.sampler.systemSetup().playMode == OWNER_PLAY_MODE_SAMPLE);
}
