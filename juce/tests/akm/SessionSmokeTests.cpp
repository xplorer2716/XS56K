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

// The session smoke test: what the owner runs against the real S5000 (`xs56k_akm_probe --session`), and what
// CI runs first against the simulated sampler, so that its sequence, its log and the state it leaves are
// checked before any hardware is involved. [TASK-AKM-013, RQ-AKM-011 to RQ-AKM-015, RQ-AKM-017, RQ-AKM-018,
// RQ-AKM-019, RQ-AKM-044, ADR-AKM-001 (DEC-AKM-007, DEC-AKM-008, DEC-AKM-011, DEC-AKM-012)]
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <regex>
#include <set>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>

#include "AwkwardSampler.hpp"
#include "TestBytes.hpp"
#include "akm/Command.hpp"
#include "akm/Protocol.hpp"
#include "akm/harness/ScenarioDriver.hpp"
#include "akm/harness/SessionSmokeTest.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"

using Catch::Matchers::ContainsSubstring;
using namespace std::chrono_literals;
using akm::ChecksumMode;
using akm::Command;
using akm::harness::DeliveryMode;
using akm::harness::ManualScenarioDriver;
using akm::harness::OsVersion;
using akm::harness::RealScenarioDriver;
using akm::harness::SamplerBehaviour;
using akm::harness::SamplerConfig;
using akm::harness::SamplerSettings;
using akm::harness::ScenarioTarget;
using akm::harness::SessionSmokeOptions;
using akm::harness::SessionSmokeResult;
using akm::harness::SimulatedMidiBackend;
using akm::harness::SimulatedSampler;
using akm::test::Bytes;
using akm::test::bytes;

namespace
{
    constexpr std::size_t DEFAULT_ECHO_ROUND_TRIPS = 50;

    // The settings the scenario leaves the sampler with: the ones found at the first contact (checksum off,
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

        [[nodiscard]] SessionSmokeOptions options(std::uint32_t targetDeviceId = 0) const
        {
            SessionSmokeOptions smoke;
            smoke.target = ScenarioTarget{backend.inputName(), backend.outputName(), targetDeviceId};
            return smoke;
        }

        ManualScenarioDriver driver;
        SimulatedMidiBackend backend{driver.scheduler()};
        SimulatedSampler& sampler;
    };

    // A stream buffer that notes how many frames the host had sent each time something is written to it, and keeps
    // what was written.
    class ProgressBuffer final : public std::streambuf
    {
    public:
        explicit ProgressBuffer(std::function<std::size_t()> framesSent) : _framesSent(std::move(framesSent)) {}

        [[nodiscard]] const std::set<std::size_t>& framesSentWhenWritten() const { return _moments; }
        [[nodiscard]] const std::string& text() const { return _text; }

    protected:
        int_type overflow(int_type character) override
        {
            _moments.insert(_framesSent());
            _text.push_back(static_cast<char>(character));
            return traits_type::not_eof(character);
        }

    private:
        std::function<std::size_t()> _framesSent;
        std::set<std::size_t> _moments;
        std::string _text;
    };

    std::vector<std::string> linesOf(const std::string& text)
    {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line))
            lines.push_back(line);
        return lines;
    }

    std::size_t countLines(const std::string& log, const std::string& direction)
    {
        const std::regex pattern(R"(^\s*\d+\.\d{3}\s+)" + direction + R"(\s+[0-9A-F]{2}( [0-9A-F]{2})*( \|.*)?$)");
        std::size_t count = 0;
        for (const std::string& line : linesOf(log))
            count += std::regex_match(line, pattern) ? 1 : 0;
        return count;
    }

    void checkKnownState(const SamplerSettings& settings)
    {
        CHECK(settings == knownState());
    }
}

TEST_CASE("Given the simulated sampler, When the smoke test runs, Then discovery finds its DeviceID, the OS version is read, 50 Echo round trips succeed, no step fails, nothing is rejected and the sampler ends in the known state [TASK-AKM-013]",
          "[akm][smoke]")
{
    Rig rig;
    std::ostringstream log;

    const SessionSmokeResult result = akm::harness::runSessionSmokeTest(rig.backend, rig.driver, rig.options(), log);

    CHECK(result.portsOpened);
    CHECK(result.samplerFound);
    CHECK(result.discoveredDeviceIds == std::vector<std::uint8_t>{0});
    REQUIRE(result.osVersion.has_value());
    CHECK(result.osVersion->major == 2);
    CHECK(result.osVersion->minor == 10);
    CHECK(result.osVersion->subVersion == 0);
    CHECK(result.echoRoundTrips == DEFAULT_ECHO_ROUND_TRIPS);
    CHECK(result.echoLatencies.size() == DEFAULT_ECHO_ROUND_TRIPS);
    CHECK(result.failedSteps.empty());
    CHECK(result.rejectedMessages == 0);
    CHECK(result.unsolicitedConfirmations == 0);
    CHECK(result.lateErrors == 0);
    CHECK(result.finalChecksumMode == ChecksumMode::Off);
    CHECK(result.knownStateRestored);
    checkKnownState(rig.sampler.settings());
}

TEST_CASE("Given a sampler left with checksums on, notification off, Sync LCD off, Auto screen update on and Still Alive on, When the smoke test runs, Then it succeeds and ends in the same known state [TASK-AKM-013, RQ-AKM-018]",
          "[akm][smoke]")
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

    const SessionSmokeResult result = akm::harness::runSessionSmokeTest(rig.backend, rig.driver, rig.options(), log);

    CHECK(result.samplerFound);
    CHECK(result.failedSteps.empty());
    CHECK(result.rejectedMessages == 0);
    CHECK(result.echoRoundTrips == DEFAULT_ECHO_ROUND_TRIPS);
    CHECK(result.knownStateRestored);
    checkKnownState(rig.sampler.settings());
}

TEST_CASE("Given no sampler answering, When the smoke test runs, Then it reports that and sends nothing after the discovery [TASK-AKM-013, RQ-AKM-012]",
          "[akm][smoke]")
{
    Rig rig;
    SamplerBehaviour behaviour;
    behaviour.silent = true;
    rig.sampler.setBehaviour(behaviour);
    std::ostringstream log;

    const SessionSmokeResult result = akm::harness::runSessionSmokeTest(rig.backend, rig.driver, rig.options(), log);

    CHECK(result.portsOpened);
    CHECK_FALSE(result.samplerFound);
    CHECK(result.discoveredDeviceIds.empty());
    CHECK_FALSE(result.osVersion.has_value());
    CHECK(result.framesSent == 1);
    CHECK(rig.backend.sentByHost().size() == 1);
    CHECK_THAT(log.str(), ContainsSubstring("no sampler answered"));
}

TEST_CASE("Given a sampler whose DeviceID is not the target's, When the smoke test runs, Then it reports the DeviceID that answered and sends nothing after the discovery [TASK-AKM-013, RQ-AKM-012, ADR-AKM-001 (DEC-AKM-007)]",
          "[akm][smoke]")
{
    Rig rig{config(3)};
    std::ostringstream log;

    const SessionSmokeResult result = akm::harness::runSessionSmokeTest(rig.backend, rig.driver, rig.options(0), log);

    CHECK_FALSE(result.samplerFound);
    CHECK(result.discoveredDeviceIds == std::vector<std::uint8_t>{3});
    CHECK(result.framesSent == 1);
    CHECK_THAT(log.str(), ContainsSubstring("DeviceID 0"));
    CHECK_THAT(log.str(), ContainsSubstring("answered as 3"));
}

TEST_CASE("Given a sampler with DeviceID 3 and the target 3, When the smoke test runs, Then the whole sequence runs on that DeviceID [TASK-AKM-013, RQ-AKM-007]",
          "[akm][smoke]")
{
    Rig rig{config(3)};
    std::ostringstream log;

    const SessionSmokeResult result = akm::harness::runSessionSmokeTest(rig.backend, rig.driver, rig.options(3), log);

    CHECK(result.samplerFound);
    CHECK(result.discoveredDeviceIds == std::vector<std::uint8_t>{3});
    CHECK(result.failedSteps.empty());
    CHECK(result.echoRoundTrips == DEFAULT_ECHO_ROUND_TRIPS);
    CHECK(result.unsolicitedConfirmations == 0);
    checkKnownState(rig.sampler.settings());
}

TEST_CASE("Given a sampler on an OS that lacks Sync LCD and Still Alive, When the smoke test runs, Then their ERROR 0 is an observation and not a failure [TASK-AKM-013, RQ-AKM-014]",
          "[akm][smoke]")
{
    Rig rig{config(0, 1, 30)};
    std::ostringstream log;

    const SessionSmokeResult result = akm::harness::runSessionSmokeTest(rig.backend, rig.driver, rig.options(), log);

    CHECK(result.samplerFound);
    REQUIRE(result.osVersion.has_value());
    CHECK(result.osVersion->major == 1);
    CHECK(result.osVersion->minor == 30);
    CHECK(result.failedSteps.empty());
    CHECK(result.knownStateRestored);
    CHECK_THAT(log.str(), ContainsSubstring("ERROR 0"));
    CHECK_FALSE(rig.sampler.settings().checksum);
    CHECK(rig.sampler.settings().notification);
}

TEST_CASE("Given the option not to touch the LCD settings, When the smoke test runs, Then neither Sync LCD nor Auto screen update is sent [TASK-AKM-013]",
          "[akm][smoke]")
{
    Rig rig;
    SessionSmokeOptions options = rig.options();
    options.touchLcdSettings = false;
    std::ostringstream log;

    const SessionSmokeResult result = akm::harness::runSessionSmokeTest(rig.backend, rig.driver, options, log);

    constexpr std::uint8_t SECTION_SYSEX_CONFIG = 0x00;
    constexpr std::uint8_t ITEM_SYNC_LCD = 0x03;
    constexpr std::uint8_t ITEM_AUTO_SCREEN_UPDATE = 0x05;

    CHECK(result.failedSteps.empty());
    // The OS version is section 02: only section 00 has a Sync LCD and an Auto screen update.
    for (const auto& command : rig.sampler.acceptedCommands())
    {
        const bool lcdSetting =
            command.section == SECTION_SYSEX_CONFIG && (command.item == ITEM_SYNC_LCD || command.item == ITEM_AUTO_SCREEN_UPDATE);
        CHECK_FALSE(lcdSetting);
    }
    CHECK_FALSE(rig.sampler.acceptedCommands().empty());
}

TEST_CASE("Given an Echo the sampler refuses, When the smoke test runs, Then one failed step is reported, the repetitions stop and the sampler still ends in the known state [TASK-AKM-013, RQ-AKM-015, RQ-AKM-018]",
          "[akm][smoke]")
{
    Rig rig;
    SamplerBehaviour behaviour;
    behaviour.itemErrors = {{0x00, 0x06, 0x01}};
    rig.sampler.setBehaviour(behaviour);
    std::ostringstream log;

    const SessionSmokeResult result = akm::harness::runSessionSmokeTest(rig.backend, rig.driver, rig.options(), log);

    CHECK(result.samplerFound);
    CHECK(result.echoRoundTrips == 0);
    REQUIRE_FALSE(result.failedSteps.empty());
    CHECK_THAT(result.failedSteps.front(), ContainsSubstring("Echo"));
    // One failure per Echo step, not one per repetition: the timed round trips are abandoned after the first and
    // count once, beside the four single Echo steps (checksums off, on, off again, Notification off).
    CHECK(result.failedSteps.size() == 5);
    CHECK(std::count_if(result.failedSteps.begin(), result.failedSteps.end(),
                        [](const std::string& title) { return title.find("round trips") != std::string::npos; })
          == 1);
    CHECK(result.knownStateRestored);
    checkKnownState(rig.sampler.settings());
}

TEST_CASE("Given a sampler that answers after 10 ms, When the smoke test runs, Then the Echo latencies seen by the session are 10 ms and the log says so [TASK-AKM-013, RQ-AKM-017]",
          "[akm][smoke]")
{
    Rig rig;
    SamplerBehaviour behaviour;
    behaviour.replyDelay = 10ms;
    rig.sampler.setBehaviour(behaviour);
    std::ostringstream log;

    const SessionSmokeResult result = akm::harness::runSessionSmokeTest(rig.backend, rig.driver, rig.options(), log);

    REQUIRE(result.echoLatencies.size() == DEFAULT_ECHO_ROUND_TRIPS);
    for (const auto latency : result.echoLatencies)
    {
        CHECK(latency >= 10ms);
        CHECK(latency <= 12ms);
    }
    CHECK_THAT(log.str(), ContainsSubstring("50 Echo round trips"));
    CHECK_THAT(log.str(), ContainsSubstring("min 10 ms"));
}

TEST_CASE("Given a run, When its log is read, Then it holds one OUT line per frame sent and one IN line per message received, then the observations [TASK-AKM-013, RQ-AKM-017]",
          "[akm][smoke]")
{
    Rig rig;
    std::ostringstream log;

    const SessionSmokeResult result = akm::harness::runSessionSmokeTest(rig.backend, rig.driver, rig.options(), log);

    const std::string text = log.str();
    CHECK(result.framesSent == rig.backend.sentByHost().size());
    CHECK(result.framesReceived == rig.backend.emittedBySamplers().size());
    CHECK(countLines(text, "OUT") == result.framesSent);
    CHECK(countLines(text, "IN") == result.framesReceived);
    // In order: a confirmation is logged after the frame that it answers, whichever thread it arrived on.
    constexpr std::size_t USER_REF_INDEX = akm::FIRST_USER_REF_INDEX;
    const std::regex frameLine(R"(^\s*\d+\.\d{3}\s+(OUT|IN)\s+((?:[0-9A-F]{2})(?: [0-9A-F]{2})*)(?: \|.*)?$)");
    std::set<std::string> sentUserRefs;
    for (const std::string& line : linesOf(text))
    {
        std::smatch match;
        if (!std::regex_match(line, match, frameLine))
            continue;
        const std::string userRef = match[2].str().substr(USER_REF_INDEX * 3, 2);
        if (match[1].str() == "OUT")
            sentUserRefs.insert(userRef);
        else
            CHECK(sentUserRefs.count(userRef) == 1);
    }
    CHECK_THAT(text, ContainsSubstring("# XS56K AKM session smoke test"));
    CHECK_THAT(text, ContainsSubstring("# step 1:"));
    const std::size_t observations = text.find("# observations");
    REQUIRE(observations != std::string::npos);
    CHECK(text.rfind("  OUT  ") < observations);
    CHECK(text.rfind("  IN   ") < observations);
    CHECK_THAT(text, ContainsSubstring("# observation: discovery answered by DeviceIDs: 0"));
    CHECK_THAT(text, ContainsSubstring("# observation: OS version 2.10 (sub-version 0)"));
    CHECK_THAT(text, ContainsSubstring("# observation: messages rejected by the session: 0"));
    CHECK_THAT(text, ContainsSubstring("# observation: sampler left in the known state"));
}

TEST_CASE("Given a run followed while its log is written, When the lines come out, Then they come out along the run, between the steps, and the last ones at its end [TASK-AKM-013, RQ-AKM-017]",
          "[akm][smoke]")
{
    // The first real run showed what writing the log while an exchange is in flight costs: about 12 ms a line, on
    // the very callback the sampler's answers arrive on. The scenario writes between its steps instead.
    // About one moment per step (24) and one per timed Echo round trip (50), out of the 75 frames.
    constexpr std::size_t ENOUGH_DISTINCT_MOMENTS = 60;
    constexpr std::size_t EARLY = 3;
    Rig rig;
    ProgressBuffer buffer([&rig] { return rig.backend.sentByHost().size(); });
    std::ostream log(&buffer);

    const SessionSmokeResult result = akm::harness::runSessionSmokeTest(rig.backend, rig.driver, rig.options(), log);

    const std::set<std::size_t>& moments = buffer.framesSentWhenWritten();
    CHECK(moments.size() >= ENOUGH_DISTINCT_MOMENTS);
    REQUIRE_FALSE(moments.empty());
    CHECK(*moments.begin() <= EARLY);
    CHECK(*moments.rbegin() == result.framesSent);
    CHECK_THAT(buffer.text(), ContainsSubstring("# observation: frames sent"));
}

TEST_CASE("Given a run, When it ends, Then the last frames it sent are the closing commands, checksums off first, then Still Alive off, Notification on, Sync LCD on and Auto screen update off [TASK-AKM-013, RQ-AKM-018]",
          "[akm][smoke]")
{
    // A closing command that the steps before it already made redundant is still sent: the closing exists for
    // the run that went wrong half way, so what it sends does not depend on how the run went.
    constexpr std::size_t ITEM_INDEX = akm::FIRST_USER_REF_INDEX + 2;
    constexpr std::size_t DATA_INDEX = ITEM_INDEX + 1;
    struct Closing
    {
        std::uint8_t item;
        std::uint8_t data;
    };
    const std::vector<Closing> closing{{0x04, 0}, {0x07, 0}, {0x01, 1}, {0x03, 1}, {0x05, 0}};
    Rig rig;
    std::ostringstream log;

    static_cast<void>(akm::harness::runSessionSmokeTest(rig.backend, rig.driver, rig.options(), log));

    const auto sent = rig.backend.sentByHost();
    REQUIRE(sent.size() >= closing.size());
    for (std::size_t index = 0; index < closing.size(); ++index)
    {
        const Bytes frame = sent[sent.size() - closing.size() + index].toBytes();
        CHECK(frame.at(ITEM_INDEX) == closing[index].item);
        CHECK(frame.at(DATA_INDEX) == closing[index].data);
    }
}

TEST_CASE("Given the real scheduler and a sampler that answers from its own thread, When the smoke test runs, Then it runs to the end and leaves the known state [TASK-AKM-013, RQ-AKM-019, RQ-AKM-020]",
          "[akm][smoke][threads]")
{
    RealScenarioDriver driver;
    SimulatedMidiBackend backend(driver.scheduler());
    SimulatedSampler& sampler = backend.addSampler();
    backend.setDeliveryMode(DeliveryMode::OnOtherThread);
    SessionSmokeOptions options;
    options.target = ScenarioTarget{backend.inputName(), backend.outputName(), 0};
    options.echoRepeats = 5;
    options.stepTimeout = 5s;
    std::ostringstream log;

    const SessionSmokeResult result = akm::harness::runSessionSmokeTest(backend, driver, options, log);

    CHECK(result.samplerFound);
    CHECK(result.failedSteps.empty());
    CHECK(result.echoRoundTrips == 5);
    CHECK(result.knownStateRestored);
    CHECK(result.rejectedMessages == 0);
    checkKnownState(sampler.settings());
}

TEST_CASE("Given a target that is not on the backend, When the smoke test runs, Then it reports that the ports could not be opened [TASK-AKM-013]",
          "[akm][smoke]")
{
    Rig rig;
    SessionSmokeOptions options = rig.options();
    options.target.inputPortName = "no such port";
    std::ostringstream log;

    const SessionSmokeResult result = akm::harness::runSessionSmokeTest(rig.backend, rig.driver, options, log);

    CHECK_FALSE(result.portsOpened);
    CHECK_FALSE(result.samplerFound);
    CHECK(result.framesSent == 0);
    CHECK_THAT(log.str(), ContainsSubstring("input port not found: no such port"));
}
