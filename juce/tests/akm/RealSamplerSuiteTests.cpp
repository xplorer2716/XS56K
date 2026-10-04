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
#include <limits>
#include <optional>
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
    constexpr std::uint16_t ERROR_NOT_SUPPORTED = 0;

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
            // The owner picks the first disk offered, unless a test says otherwise (Disk Tools).
            suite.askOwnerChoice = [](const std::string&, const std::vector<std::string>&) { return std::optional<std::size_t>{0}; };
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
    // TASK-AKM-057 gave the simulated sampler real support for &01: forced to refuse it here, like a
    // sampler whose §10 is not implemented, so this check still exercises the ERROR path it is named
    // for rather than the DONE path TASK-AKM-057's own tests already cover (DiskPrimitivesTests.cpp).
    rig.sampler.setBehaviour(SamplerBehaviour{.itemErrors = {{SECTION_DISK_TOOLS, ITEM_UPDATE_DISK_LIST, ERROR_NOT_SUPPORTED}}});
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
    constexpr std::uint16_t OUT_OF_RANGE = 0x02;
    Rig rig;
    seedSystemSetup(rig.backend, rig.sampler, "OWNER S5000", OWNER_PLAY_MODE_SAMPLE, LOCK_NORMAL);
    rig.sampler.setBehaviour(SamplerBehaviour{.itemErrors = {{SECTION_SYSTEM, ITEM_GET_CLOCK, OUT_OF_RANGE}}});
    RealSuiteOptions options = rig.options();
    options.systemSetup = true;
    std::ostringstream log;
    // &06: Set Clock Time & Date (§02). Written as a literal: a local constant only named inside a lambda is reported unused by GCC.
    const auto isSetClock = [](const auto& command) { return command.section == SECTION_SYSTEM && command.item == 0x06; };
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

// Disk Tools (TASK-AKM-067, RQ-AKM-070, RQ-AKM-071): the safe check and the one guarded long-running item, on the
// simulated sampler. The checks read the sampler's own accepted commands to prove what was created and deleted.
namespace
{
    constexpr std::uint8_t SECTION_DISK_ITEMS = 0x10;
    constexpr std::uint8_t ITEM_DISK_SELECT = 0x02;
    constexpr std::uint8_t ITEM_DISK_GET_TYPE_OF = 0x07;
    constexpr std::uint8_t ITEM_DISK_GET_NAME = 0x0E;
    constexpr std::uint8_t ITEM_DISK_FILE_NAME = 0x21;
    constexpr std::uint8_t ITEM_DISK_FILE_SIZE = 0x23;
    constexpr std::uint8_t ITEM_DISK_RENAME_FILE = 0x28;
    constexpr std::uint8_t ITEM_DISK_DELETE_FILE = 0x29;
    constexpr std::uint8_t ITEM_DISK_START_AUDITION = 0x30;
    constexpr std::uint8_t ITEM_DISK_STOP_AUDITION = 0x31;
    constexpr std::uint8_t ITEM_DISK_CREATE_FOLDER = 0x16;
    constexpr std::uint8_t ITEM_DISK_DELETE_FOLDER = 0x17;
    constexpr std::uint8_t ITEM_DISK_SAVE_MEMORY_ITEM = 0x2C;
    constexpr std::uint8_t ITEM_DISK_LOAD_FILE = 0x2A;

    std::size_t sentCount(const SimulatedSampler& sampler, std::uint8_t section, std::uint8_t item)
    {
        std::size_t count = 0;
        for (const auto& command : sampler.acceptedCommands())
            if (command.section == section && command.item == item)
                ++count;
        return count;
    }

    akm::harness::DiskRecord currentDiskRecord()
    {
        return akm::harness::DiskRecord{.handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA"};
    }
}

TEST_CASE("Given Disk Tools on a sampler with a writable disk and no disk selected, When the suite runs, Then the check selects it, the disposable folder is created, used and deleted, and every check passes [TASK-AKM-067, RQ-AKM-061, RQ-AKM-071]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({currentDiskRecord()});
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 1);
    checkAllPassed(result);
    CHECK(result.knownStateRestored);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_SELECT) == 1);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_CREATE_FOLDER) >= 2);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_DELETE_FOLDER) == 1);
}

TEST_CASE("Given Disk Tools on a sampler whose first disk is read-only, When the suite runs, Then it selects the first writable disk and the check passes [TASK-AKM-071, RQ-AKM-061]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({
        akm::harness::DiskRecord{.handle = 5, .type = 2, .format = 3, .scsiId = 1, .writable = false, .name = "CDROM"},
        akm::harness::DiskRecord{.handle = 7, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA"},
    });
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 1);
    checkAllPassed(result);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_SELECT) == 1);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_CREATE_FOLDER) >= 2);
}

TEST_CASE("Given Disk Tools on a sampler with two writable disks, When the owner picks the second, Then the disks offered are both listed, the second is selected and the check passes [TASK-AKM-067, RQ-AKM-061]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({
        akm::harness::DiskRecord{.handle = 4, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "ONE"},
        akm::harness::DiskRecord{.handle = 9, .type = 3, .format = 1, .scsiId = 2, .writable = true, .name = "TWO"},
    });
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    std::vector<std::string> offered;
    options.askOwnerChoice = [&offered](const std::string&, const std::vector<std::string>& choices) {
        offered = choices;
        return std::optional<std::size_t>{1};
    };
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 1);
    checkAllPassed(result);
    REQUIRE(offered.size() == 2);
    CHECK(offered[0].find("handle 4") != std::string::npos);
    CHECK(offered[1].find("handle 9") != std::string::npos);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_SELECT) == 1);
}

TEST_CASE("Given Disk Tools on a sampler with a writable disk, When the owner declines to choose, Then the check is skipped, nothing is selected and nothing is created [TASK-AKM-067, RQ-AKM-061, RQ-AKM-071]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({currentDiskRecord()});
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.askOwnerChoice = [](const std::string&, const std::vector<std::string>&) { return std::optional<std::size_t>{}; };
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 1);
    CHECK(result.checks.back().outcome == CheckOutcome::Skipped);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_SELECT) == 0);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_CREATE_FOLDER) == 0);
}

TEST_CASE("Given Disk Tools and the file items, When the suite runs, Then the disk is read by handle, one save makes a file, the file is read, renamed and deleted, no audition is sent, and every check passes [RQ-AKM-065, RQ-AKM-069, RQ-AKM-071]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({currentDiskRecord()});
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.diskToolsFiles = true;
    options.askOwner = [](const std::string&) { return true; };
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    checkAllPassed(result);
    CHECK(result.knownStateRestored);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_GET_TYPE_OF) >= 1);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_GET_NAME) >= 1);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_SAVE_MEMORY_ITEM) == 1);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_FILE_NAME) >= 1);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_FILE_SIZE) >= 1);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_RENAME_FILE) == 1);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_START_AUDITION) == 0);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_STOP_AUDITION) == 0);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_DELETE_FILE) == 1);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_DELETE_FOLDER) == 2);
}

TEST_CASE("Given Disk Tools and the file items with no way to ask the owner, When the suite runs, Then the check is skipped before anything is saved [RQ-AKM-069]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({currentDiskRecord()});
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.diskToolsFiles = true;
    options.askOwner = nullptr;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    CHECK(result.checks.back().outcome == CheckOutcome::Skipped);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_SAVE_MEMORY_ITEM) == 0);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_START_AUDITION) == 0);
}

TEST_CASE("Given Disk Tools and the audition, When a .WAV file is at the root of the disk, Then it is started with &30, stopped with &31 and every check passes [RQ-AKM-068]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({akm::harness::DiskRecord{
        .handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA",
        .rootFolder = akm::harness::FolderRecord{"", {}, {}, {}, {akm::harness::FileRecord{"TONE.WAV", 4096}}}}});
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.diskToolsAudition = true;
    options.auditionDuration = 0ms;
    options.askOwner = [](const std::string&) { return true; };
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    checkAllPassed(result);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_START_AUDITION) == 1);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_STOP_AUDITION) == 1);
}

TEST_CASE("Given Disk Tools and the audition, When no .WAV file is at the root of the disk, Then the check fails and nothing is started [RQ-AKM-068]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({akm::harness::DiskRecord{
        .handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA",
        .rootFolder = akm::harness::FolderRecord{"", {}, {}, {}, {akm::harness::FileRecord{"LEAD.AKP", 10}}}}});
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.diskToolsAudition = true;
    options.auditionDuration = 0ms;
    options.askOwner = [](const std::string&) { return true; };
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    CHECK(result.checks.back().outcome == CheckOutcome::Failed);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_START_AUDITION) == 0);
}

TEST_CASE("Given Disk Tools and the audition with no way to ask the owner, When the suite runs, Then the check is skipped before any disk is selected [RQ-AKM-068]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({currentDiskRecord()});
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.diskToolsAudition = true;
    options.askOwner = nullptr;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    CHECK(result.checks.back().outcome == CheckOutcome::Skipped);
    // Only the safe check's own selection: the audition selects nothing once it is skipped.
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_SELECT) == 1);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_START_AUDITION) == 0);
}

TEST_CASE("Given Disk Tools and the file items with a rename the sampler refuses, When the owner asks to keep the folder, Then the check fails, the owner is asked to look first and the folder is left in place [RQ-AKM-069, RQ-AKM-071]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({currentDiskRecord()});
    rig.sampler.setBehaviour(SamplerBehaviour{.itemErrors = {{SECTION_DISK_ITEMS, ITEM_DISK_RENAME_FILE, ERROR_UNKNOWN}}});
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.diskToolsFiles = true;
    std::vector<std::string> asked;
    options.askOwner = [&asked](const std::string& instruction) {
        asked.push_back(instruction);
        return instruction.find("did not finish") == std::string::npos;
    };
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    CHECK(result.checks.back().outcome == CheckOutcome::Failed);
    REQUIRE_FALSE(asked.empty());
    CHECK(asked.back().find("did not finish") != std::string::npos);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_DELETE_FOLDER) == 1);
}

TEST_CASE("Given Disk Tools with no way to ask the owner which disk to select, When the suite runs, Then the check is skipped before any disk is selected [TASK-AKM-067, RQ-AKM-061]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({currentDiskRecord()});
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.askOwnerChoice = nullptr;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 1);
    CHECK(result.checks.back().outcome == CheckOutcome::Skipped);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_SELECT) == 0);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_CREATE_FOLDER) == 0);
}

TEST_CASE("Given Disk Tools on a sampler that lists no writable disk, When the suite runs, Then the check is skipped, nothing is selected and nothing is created [TASK-AKM-067, RQ-AKM-061, RQ-AKM-071]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({
        akm::harness::DiskRecord{.handle = 5, .type = 2, .format = 3, .scsiId = 1, .writable = false, .name = "CDROM"},
    });
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 1);
    CHECK(result.checks.back().outcome == CheckOutcome::Skipped);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_SELECT) == 0);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_CREATE_FOLDER) == 0);
}

TEST_CASE("Given Disk Tools and the update-list slow item, When the suite runs, Then exactly one &01 is sent inside the disposable folder and every check passes [TASK-AKM-067, RQ-AKM-070]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({currentDiskRecord()});
    rig.sampler.setCurrentDisk(0);
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.diskToolsSlow = akm::harness::DiskSlowOperation::UpdateList;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    checkAllPassed(result);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_UPDATE_DISK_LIST) == 1);
    CHECK(result.knownStateRestored);
}

TEST_CASE("Given Disk Tools and the save-memory-item slow item, When the suite runs, Then one &2C is sent into the folder, which is then deleted [TASK-AKM-067, RQ-AKM-070]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({currentDiskRecord()});
    rig.sampler.setCurrentDisk(0);
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.diskToolsSlow = akm::harness::DiskSlowOperation::SaveMemoryItem;
    options.askOwner = [](const std::string&) { return true; };
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    checkAllPassed(result);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_SAVE_MEMORY_ITEM) == 1);
    // One deletion from the safe check that runs with it, one from this check's own folder.
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_DELETE_FOLDER) == 2);
}

TEST_CASE("Given Disk Tools and the load-file slow item, When the suite runs, Then the save that makes the file is followed by one &2A [TASK-AKM-067, RQ-AKM-070]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({currentDiskRecord()});
    rig.sampler.setCurrentDisk(0);
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.diskToolsSlow = akm::harness::DiskSlowOperation::LoadFile;
    options.askOwner = [](const std::string&) { return true; };
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    checkAllPassed(result);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_SAVE_MEMORY_ITEM) == 1);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_LOAD_FILE) == 1);
}

TEST_CASE("Given Disk Tools and the load-folder slow item, When the suite runs, Then an empty sub-folder is created and loaded, then the test folder deleted [TASK-AKM-067, RQ-AKM-070]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({currentDiskRecord()});
    rig.sampler.setCurrentDisk(0);
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.diskToolsSlow = akm::harness::DiskSlowOperation::LoadFolder;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    checkAllPassed(result);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, 0x15) == 1);
    CHECK(result.knownStateRestored);
}

TEST_CASE("Given Disk Tools and the load-file-with-dependents slow item, When the suite runs, Then the save and the &2B load are each sent once [TASK-AKM-067, RQ-AKM-070]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({currentDiskRecord()});
    rig.sampler.setCurrentDisk(0);
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.diskToolsSlow = akm::harness::DiskSlowOperation::LoadFileWithDependents;
    options.askOwner = [](const std::string&) { return true; };
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    checkAllPassed(result);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_SAVE_MEMORY_ITEM) == 1);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, 0x2B) == 1);
}

TEST_CASE("Given Disk Tools and the save-all-memory-items slow item, When the suite runs, Then one &2D is sent and the folder is deleted afterwards [TASK-AKM-067, RQ-AKM-070]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({currentDiskRecord()});
    rig.sampler.setCurrentDisk(0);
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.diskToolsSlow = akm::harness::DiskSlowOperation::SaveAllMemoryItems;
    options.askOwner = [](const std::string&) { return true; };
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    checkAllPassed(result);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, 0x2D) == 1);
    CHECK(result.knownStateRestored);
}

TEST_CASE("Given Disk Tools and a file-saving slow item with no way to ask the owner, When the suite runs, Then the check is skipped before anything is saved [TASK-AKM-067, RQ-AKM-070]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({currentDiskRecord()});
    rig.sampler.setCurrentDisk(0);
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.diskToolsSlow = akm::harness::DiskSlowOperation::LoadFile;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    CHECK(result.checks.back().outcome == CheckOutcome::Skipped);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_SAVE_MEMORY_ITEM) == 0);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_LOAD_FILE) == 0);
}

TEST_CASE("Given Disk Tools and the owner declining to confirm the saved file, When the suite runs, Then the check is skipped, nothing is loaded and the folder is still deleted [TASK-AKM-067, RQ-AKM-070]",
          "[akm][suite][disk-tools]")
{
    Rig rig;
    rig.sampler.setDisks({currentDiskRecord()});
    rig.sampler.setCurrentDisk(0);
    RealSuiteOptions options = rig.options();
    options.diskTools = true;
    options.diskToolsSlow = akm::harness::DiskSlowOperation::LoadFile;
    // The owner declines the saved file, and presses Enter on the question asked when the check ends early.
    options.askOwner = [](const std::string& instruction) { return instruction.find("did not finish") != std::string::npos; };
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 2);
    CHECK(result.checks.back().outcome == CheckOutcome::Skipped);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_SAVE_MEMORY_ITEM) == 1);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_LOAD_FILE) == 0);
    CHECK(sentCount(rig.sampler, SECTION_DISK_ITEMS, ITEM_DISK_DELETE_FOLDER) == 2);
    CHECK(result.knownStateRestored);
}

// ---- Section §04, the MIDI configuration check guided by the owner (TASK-AKM-079, RQ-AKM-080) ----

namespace
{
    // What the owner's sampler holds of its MIDI setup before any session is opened, and what the scripted owner then
    // declares it holds: §04 has no Get, so the suite can only put back what it is told (RQ-AKM-080).
    constexpr std::uint8_t ITEM_PROGRAM_CHANGE = 0x01;
    constexpr std::uint8_t ITEM_MULTI_SELECT = 0x02;
    constexpr std::uint8_t ITEM_MULTI_SELECT_CHANNEL = 0x03;
    constexpr std::uint8_t ITEM_EXTERNAL_APM = 0x04;
    constexpr std::uint8_t ITEM_AFTERTOUCH = 0x05;
    constexpr std::uint8_t ITEM_FILTER_ALLOW = 0x06;
    constexpr std::uint8_t ITEM_FILTER_IGNORE = 0x07;

    constexpr int OWNER_PROGRAM_CHANGE = 0;
    constexpr int OWNER_MULTI_SELECT = 2;
    constexpr int OWNER_MULTI_SELECT_CHANNEL = 5;
    constexpr int OWNER_EXTERNAL_APM = 74;
    constexpr int OWNER_AFTERTOUCH = 1;
    constexpr int OWNER_FILTER_EVENT = 2;
    constexpr int OWNER_FILTER_CHANNEL = 17;
    // The question about program change offers ON then OFF; the seeded sampler has it off.
    constexpr std::size_t CHOICE_PROGRAM_CHANGE_OFF = 1;
    // The seeded filter ignores its messages; the question about it offers "allows" then "ignores".
    constexpr std::size_t CHOICE_FILTER_IGNORES = 1;
    constexpr std::size_t CHOICE_YES = 0;
    constexpr std::size_t CHOICE_NO = 1;
    constexpr std::size_t MIDI_CONFIG_EXTRA_CHECKS = 2;

    using akm::harness::MidiConfigEvent;
    using akm::harness::MidiConfigState;

    void seedMidiConfig(SimulatedMidiBackend& backend)
    {
        akm::test::HostProbe host(backend, backend.inputName(), backend.outputName());
        std::uint8_t userRef = 0x01;
        const auto send = [&](const akm::CommandRequest& request) {
            const akm::EncodeResult frame = akm::encodeCommand(0, akm::test::Bytes{userRef++}, request.command, akm::ChecksumMode::Off);
            host.send(frame.bytes);
        };
        send(akm::makeRequest(akm::ItemId::MidiProgramChangeEnable, {OWNER_PROGRAM_CHANGE}));
        send(akm::makeRequest(akm::ItemId::MidiMultiSelect, {OWNER_MULTI_SELECT}));
        send(akm::makeRequest(akm::ItemId::MidiMultiSelectChannel, {OWNER_MULTI_SELECT_CHANNEL}));
        send(akm::makeRequest(akm::ItemId::MidiExternalApmController, {OWNER_EXTERNAL_APM}));
        send(akm::makeRequest(akm::ItemId::MidiAftertouch, {OWNER_AFTERTOUCH}));
        send(akm::makeRequest(akm::ItemId::MidiFilterIgnore, {OWNER_FILTER_EVENT, OWNER_FILTER_CHANNEL}));
    }

    // The owner of the tests: it answers the declaration with the values the sampler was seeded with, and what it
    // sees on the screen with `seen`. `eventsAtFirstAsk` is how many §04 items the sampler had been sent when the
    // owner was first asked anything.
    struct ScriptedOwner
    {
        std::size_t seen = CHOICE_YES;
        std::size_t eventsAtFirstAsk = std::numeric_limits<std::size_t>::max();
        std::vector<std::string> questions;
    };

    RealSuiteOptions midiConfigOptions(const Rig& rig, SimulatedSampler& sampler, ScriptedOwner& owner)
    {
        RealSuiteOptions options = rig.options();
        options.midiConfig = true;
        const auto noteFirstAsk = [&owner, &sampler] {
            if (owner.eventsAtFirstAsk == std::numeric_limits<std::size_t>::max())
                owner.eventsAtFirstAsk = sampler.midiConfig().events.size();
        };
        options.askOwner = [&owner, noteFirstAsk](const std::string& instruction) {
            noteFirstAsk();
            owner.questions.push_back(instruction);
            return true;
        };
        options.askOwnerNumber = [&owner, noteFirstAsk](const std::string& question, int, int) -> std::optional<int> {
            noteFirstAsk();
            owner.questions.push_back(question);
            return OWNER_EXTERNAL_APM;
        };
        options.askOwnerChoice = [&owner, noteFirstAsk](const std::string& question,
                                                        const std::vector<std::string>&) -> std::optional<std::size_t> {
            noteFirstAsk();
            owner.questions.push_back(question);
            const auto asks = [&question](const char* part) { return question.find(part) != std::string::npos; };
            if (asks("NOW LOOK AT THE SAMPLER"))
                return owner.seen;
            if (asks("PROGRAM CHANGE"))
                return CHOICE_PROGRAM_CHANGE_OFF;
            if (asks("MULTI SLCT CH"))
                return static_cast<std::size_t>(OWNER_MULTI_SELECT_CHANNEL);
            if (asks("MULTI SELECT"))
                return static_cast<std::size_t>(OWNER_MULTI_SELECT);
            if (asks("AFTERTOUCH"))
                return static_cast<std::size_t>(OWNER_AFTERTOUCH);
            if (asks("MIDI FILTER, event type"))
                return static_cast<std::size_t>(OWNER_FILTER_EVENT);
            if (asks("MIDI FILTER, channel"))
                return static_cast<std::size_t>(OWNER_FILTER_CHANNEL);
            if (asks("MIDI FILTER, that filter"))
                return CHOICE_FILTER_IGNORES;
            return std::nullopt;
        };
        return options;
    }

    // The §04 state the owner's sampler started in, without the record of what it was sent.
    void checkSeededState(const MidiConfigState& state)
    {
        CHECK(state.programChangeEnable == OWNER_PROGRAM_CHANGE);
        CHECK(state.multiSelect == OWNER_MULTI_SELECT);
        CHECK(state.multiSelectChannel == OWNER_MULTI_SELECT_CHANNEL);
        CHECK(state.externalApmController == OWNER_EXTERNAL_APM);
        CHECK(state.aftertouch == OWNER_AFTERTOUCH);
        for (std::size_t type = 0; type < akm::harness::MIDI_FILTER_EVENT_TYPES; ++type)
            for (std::size_t channel = 0; channel < akm::harness::MIDI_FILTER_CHANNELS; ++channel)
                CHECK(state.filterAllowed[type][channel]
                      == !(type == static_cast<std::size_t>(OWNER_FILTER_EVENT)
                           && channel == static_cast<std::size_t>(OWNER_FILTER_CHANNEL)));
    }
}

TEST_CASE("Given an owner who declares the sampler's real MIDI setup, When the suite runs with the MIDI config checks, Then each setting is changed to another value then put back, the failed check restores too, and nothing is sent before the owner has declared [TASK-AKM-079, RQ-AKM-080]",
          "[akm][suite][midi-config]")
{
    Rig rig;
    seedMidiConfig(rig.backend);
    const std::size_t seedEvents = rig.sampler.midiConfig().events.size();
    ScriptedOwner owner;
    RealSuiteOptions options = midiConfigOptions(rig, rig.sampler, owner);
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + MIDI_CONFIG_EXTRA_CHECKS);
    checkAllPassed(result);
    CHECK(result.knownStateRestored);
    CHECK(owner.eventsAtFirstAsk == seedEvents);

    const MidiConfigState after = rig.sampler.midiConfig();
    checkSeededState(after);
    // Six changes to another value, the six restores in the opposite order, then the second check's one change and
    // its restore.
    const auto event = [](std::uint8_t item, int first, int second) {
        return MidiConfigEvent{item, static_cast<std::uint8_t>(first), static_cast<std::uint8_t>(second)};
    };
    const std::vector<MidiConfigEvent> expected{
        event(ITEM_PROGRAM_CHANGE, 1, 0),
        event(ITEM_MULTI_SELECT, 0, 0),
        event(ITEM_MULTI_SELECT_CHANNEL, OWNER_MULTI_SELECT_CHANNEL + 1, 0),
        event(ITEM_EXTERNAL_APM, OWNER_EXTERNAL_APM + 1, 0),
        event(ITEM_AFTERTOUCH, 0, 0),
        event(ITEM_FILTER_ALLOW, OWNER_FILTER_EVENT, OWNER_FILTER_CHANNEL),
        event(ITEM_FILTER_IGNORE, OWNER_FILTER_EVENT, OWNER_FILTER_CHANNEL),
        event(ITEM_AFTERTOUCH, OWNER_AFTERTOUCH, 0),
        event(ITEM_EXTERNAL_APM, OWNER_EXTERNAL_APM, 0),
        event(ITEM_MULTI_SELECT_CHANNEL, OWNER_MULTI_SELECT_CHANNEL, 0),
        event(ITEM_MULTI_SELECT, OWNER_MULTI_SELECT, 0),
        event(ITEM_PROGRAM_CHANGE, OWNER_PROGRAM_CHANGE, 0),
        event(ITEM_MULTI_SELECT, 0, 0),
        event(ITEM_MULTI_SELECT, OWNER_MULTI_SELECT, 0),
    };
    REQUIRE(after.events.size() == seedEvents + expected.size());
    const std::vector<MidiConfigEvent> sent(after.events.begin() + static_cast<std::ptrdiff_t>(seedEvents), after.events.end());
    CHECK(sent == expected);
    CHECK_THAT(log.str(), ContainsSubstring("this check fails on purpose, with MULTI SELECT changed"));
}

TEST_CASE("Given an owner who declines to declare the sampler's MIDI setup, When the suite runs with the MIDI config checks, Then both checks are skipped and no section 04 item is sent [TASK-AKM-079, RQ-AKM-080]",
          "[akm][suite][midi-config]")
{
    Rig rig;
    ScriptedOwner owner;
    RealSuiteOptions options = midiConfigOptions(rig, rig.sampler, owner);
    options.askOwnerChoice = [](const std::string&, const std::vector<std::string>&) { return std::optional<std::size_t>{}; };
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + MIDI_CONFIG_EXTRA_CHECKS);
    CHECK(result.checks[AUTOMATIC_CHECKS].outcome == CheckOutcome::Skipped);
    CHECK(result.checks[AUTOMATIC_CHECKS + 1].outcome == CheckOutcome::Skipped);
    CHECK(rig.sampler.midiConfig().events.empty());
    CHECK(result.knownStateRestored);
}

TEST_CASE("Given no way to ask the owner for a number, When the suite runs with the MIDI config checks, Then both checks are skipped and no section 04 item is sent [TASK-AKM-079, RQ-AKM-080]",
          "[akm][suite][midi-config]")
{
    Rig rig;
    ScriptedOwner owner;
    RealSuiteOptions options = midiConfigOptions(rig, rig.sampler, owner);
    options.askOwnerNumber = nullptr;
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + MIDI_CONFIG_EXTRA_CHECKS);
    CHECK(result.checks[AUTOMATIC_CHECKS].outcome == CheckOutcome::Skipped);
    CHECK(result.checks[AUTOMATIC_CHECKS + 1].outcome == CheckOutcome::Skipped);
    CHECK(rig.sampler.midiConfig().events.empty());
}

TEST_CASE("Given an owner who sees that the sampler did not change, When the suite runs with the MIDI config checks, Then the check fails and the setting it had changed is put back all the same [TASK-AKM-079, RQ-AKM-080]",
          "[akm][suite][midi-config]")
{
    Rig rig;
    seedMidiConfig(rig.backend);
    ScriptedOwner owner;
    owner.seen = CHOICE_NO;
    RealSuiteOptions options = midiConfigOptions(rig, rig.sampler, owner);
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + MIDI_CONFIG_EXTRA_CHECKS);
    CHECK(result.checks[AUTOMATIC_CHECKS].outcome == CheckOutcome::Failed);
    checkSeededState(rig.sampler.midiConfig());
    CHECK_THAT(log.str(), ContainsSubstring("NOT MET"));
}

TEST_CASE("Given the default options, When the suite runs, Then no section 04 item is sent [TASK-AKM-079, RQ-AKM-080]",
          "[akm][suite][midi-config]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    std::ostringstream log;

    const RealSuiteResult result = akm::harness::runRealSamplerSuite(rig.backend, rig.driver, options, log);

    REQUIRE(result.checks.size() == AUTOMATIC_CHECKS);
    CHECK(rig.sampler.midiConfig().events.empty());
}
