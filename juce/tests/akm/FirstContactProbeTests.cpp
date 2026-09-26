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

// The first-contact probe: what the owner runs against the real S5000 (`xs56k_akm_probe`), and what CI
// runs first against the simulated sampler, so that the sequence, the log and the state it leaves are
// checked before any hardware is involved. The expected frames are written out byte by byte.
// [TASK-AKM-012, RQ-AKM-017, RQ-AKM-018, RQ-AKM-044, ADR-AKM-001 (DEC-AKM-007, DEC-AKM-009)]
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "HostProbe.hpp"
#include "TestBytes.hpp"
#include "akm/harness/FirstContactProbe.hpp"
#include "akm/harness/ScenarioDriver.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"

using namespace std::chrono_literals;
using akm::harness::ManualScenarioDriver;
using akm::harness::ProbeOptions;
using akm::harness::ProbeResult;
using akm::harness::SamplerBehaviour;
using akm::harness::SamplerConfig;
using akm::harness::ScenarioTarget;
using akm::harness::SimulatedMidiBackend;
using akm::harness::SimulatedSampler;
using akm::test::Bytes;
using akm::test::bytes;

namespace
{
    // The probe's sequence: fifteen frames, user-refs 10 to 1E, one per step.
    constexpr std::size_t stepCount = 15;
    constexpr std::uint8_t firstUserRef = 0x10;

    // Every step but one (the Query without checksum while checksums are on, ERROR 81 alone) is answered
    // by an OK and one more confirmation, on a sampler with notifications on.
    constexpr std::size_t expectedReceivedFrames = 2 * stepCount - 1;

    // The frames of the sequence for a target with DeviceID 0 and the other DeviceID 5, from the spec.
    std::vector<Bytes> expectedFrames()
    {
        return {
            bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x00, 0x04, 0x00, 0x14, 0xF7}),                    // checksums off, everywhere
            bytes({0xF0, 0x47, 0x5E, 0x00, 0x11, 0x00, 0x00, 0xF7}),                                // Query, DeviceID 0
            bytes({0xF0, 0x47, 0x5E, 0x05, 0x12, 0x00, 0x00, 0xF7}),                                // Query, DeviceID 5
            bytes({0xF0, 0x47, 0x5E, 0x00, 0x13, 0x02, 0x00, 0xF7}),                                // OS version
            bytes({0xF0, 0x47, 0x5E, 0x00, 0x14, 0x02, 0x01, 0xF7}),                                // OS sub-version
            bytes({0xF0, 0x47, 0x5E, 0x00, 0x15, 0x00, 0x06, 0x01, 0x02, 0x03, 0x04, 0xF7}),        // Echo
            bytes({0xF0, 0x47, 0x5E, 0x00, 0x16, 0x00, 0x06, 0x01, 0x02, 0x03, 0x04, 0x26, 0xF7}),  // Echo, unexpected checksum
            bytes({0xF0, 0x47, 0x5E, 0x00, 0x17, 0x00, 0x04, 0x01, 0xF7}),                          // checksums on
            bytes({0xF0, 0x47, 0x5E, 0x00, 0x18, 0x00, 0x00, 0x18, 0xF7}),                          // Query with checksum
            bytes({0xF0, 0x47, 0x5E, 0x00, 0x19, 0x00, 0x06, 0x01, 0x02, 0x03, 0x04, 0x29, 0xF7}),  // Echo with checksum
            bytes({0xF0, 0x47, 0x5E, 0x00, 0x1A, 0x00, 0x00, 0xF7}),                                // Query without checksum
            bytes({0xF0, 0x47, 0x5E, 0x00, 0x1B, 0x00, 0x04, 0x00, 0x1F, 0xF7}),                    // checksums off, everywhere
            bytes({0xF0, 0x47, 0x5E, 0x00, 0x1C, 0x00, 0x00, 0xF7}),                                // Query, checksums off again
            bytes({0xF0, 0x47, 0x5E, 0x00, 0x1D, 0x00, 0x07, 0x01, 0xF7}),                          // Still Alive on
            bytes({0xF0, 0x47, 0x5E, 0x00, 0x1E, 0x00, 0x07, 0x00, 0xF7}),                          // Still Alive off
        };
    }

    struct LogLine
    {
        double seconds = 0.0;
        std::string direction;
        Bytes frame;
    };

    // The frame lines of a log: "<seconds> OUT|IN <hex bytes>", optionally followed by " | annotation".
    std::vector<LogLine> frameLines(const std::string& log)
    {
        static const std::regex pattern(R"(^\s*(\d+\.\d{3})\s+(OUT|IN)\s+((?:[0-9A-F]{2})(?: [0-9A-F]{2})*)(?: \|.*)?$)");
        std::vector<LogLine> lines;
        std::istringstream stream(log);
        std::string text;
        while (std::getline(stream, text))
        {
            std::smatch match;
            if (!std::regex_match(text, match, pattern))
                continue;
            LogLine line;
            line.seconds = std::stod(match[1].str());
            line.direction = match[2].str();
            std::istringstream hex(match[3].str());
            std::string token;
            while (hex >> token)
                line.frame.push_back(static_cast<std::uint8_t>(std::stoul(token, nullptr, 16)));
            lines.push_back(std::move(line));
        }
        return lines;
    }

    std::vector<Bytes> framesOf(const std::vector<LogLine>& lines, const std::string& direction)
    {
        std::vector<Bytes> frames;
        for (const LogLine& line : lines)
        {
            if (line.direction == direction)
                frames.push_back(line.frame);
        }
        return frames;
    }

    struct ProbeRig
    {
        explicit ProbeRig(SamplerConfig config = {}) : sampler(backend.addSampler(config))
        {
            options.target = ScenarioTarget{backend.inputName(), backend.outputName(), 0};
        }

        ProbeResult run() { return akm::harness::runFirstContactProbe(backend, driver, options, log); }

        [[nodiscard]] std::string text() const { return log.str(); }

        ManualScenarioDriver driver;
        SimulatedMidiBackend backend{driver.scheduler()};
        SimulatedSampler& sampler;
        ProbeOptions options;
        std::ostringstream log;
    };

    bool contains(const std::string& text, const std::string& part)
    {
        return text.find(part) != std::string::npos;
    }
}

TEST_CASE("Given the simulated sampler, When the probe runs, Then it sends the fifteen frames of the sequence, in order [RQ-AKM-017, RQ-AKM-044]",
          "[akm][probe]")
{
    ProbeRig rig;

    rig.run();

    const auto sent = rig.backend.sentByHost();
    const auto expected = expectedFrames();
    REQUIRE(sent.size() == expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
        CHECK(sent[index].toBytes() == expected[index]);
}

TEST_CASE("Given the probe's sequence, When it runs, Then each frame has a user-ref of its own [RQ-AKM-017, RQ-AKM-007]",
          "[akm][probe]")
{
    ProbeRig rig;

    rig.run();

    std::set<std::uint8_t> userRefs;
    for (const auto& message : rig.backend.sentByHost())
        userRefs.insert(message.toBytes()[4]);
    CHECK(userRefs.size() == stepCount);
    CHECK(*userRefs.begin() == firstUserRef);
}

TEST_CASE("Given the simulated sampler, When the probe runs, Then the log has one line per frame with its direction, time and bytes [RQ-AKM-017]",
          "[akm][probe]")
{
    ProbeRig rig;

    const ProbeResult result = rig.run();

    const auto lines = frameLines(rig.text());
    CHECK(framesOf(lines, "OUT").size() == stepCount);
    CHECK(framesOf(lines, "IN").size() == expectedReceivedFrames);
    CHECK(result.framesSent == stepCount);
    CHECK(result.framesReceived == expectedReceivedFrames);
    for (std::size_t index = 1; index < lines.size(); ++index)
        CHECK(lines[index].seconds >= lines[index - 1].seconds);
    // What the log records as sent is what the sampler received.
    const auto sent = framesOf(lines, "OUT");
    const auto expected = expectedFrames();
    REQUIRE(sent.size() == expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
        CHECK(sent[index] == expected[index]);
}

TEST_CASE("Given the log, When read, Then it shows an Echo REPLY, a Query DONE and the answers with checksums on and off [RQ-AKM-017]",
          "[akm][probe]")
{
    ProbeRig rig;

    rig.run();

    const auto received = framesOf(frameLines(rig.text()), "IN");
    const auto contains_frame = [&received](const Bytes& frame) {
        return std::find(received.begin(), received.end(), frame) != received.end();
    };
    // Echo REPLY with checksums off (step 6), a Query DONE (step 2), and the same with checksums on.
    CHECK(contains_frame(bytes({0xF0, 0x47, 0x5E, 0x00, 0x15, 0x52, 0x00, 0x06, 0x01, 0x02, 0x03, 0x04, 0xF7})));
    CHECK(contains_frame(bytes({0xF0, 0x47, 0x5E, 0x00, 0x11, 0x44, 0x00, 0x00, 0xF7})));
    // Checksums on: the Echo REPLY of step 10 (checksum 19 + 52 + 00 + 06 + 01 + 02 + 03 + 04 = 7B) and the Query DONE of step 9 (18 + 44 + 00 + 00 = 5C).
    CHECK(contains_frame(bytes({0xF0, 0x47, 0x5E, 0x00, 0x19, 0x52, 0x00, 0x06, 0x01, 0x02, 0x03, 0x04, 0x7B, 0xF7})));
    CHECK(contains_frame(bytes({0xF0, 0x47, 0x5E, 0x00, 0x18, 0x44, 0x00, 0x00, 0x5C, 0xF7})));
}

TEST_CASE("Given the log, When read, Then a confirmation is shown as read with checksums off and on, ERROR 129 included [RQ-AKM-017, RQ-AKM-005]",
          "[akm][probe]")
{
    ProbeRig rig;

    rig.run();

    const std::string text = rig.text();
    CHECK(contains(text, "| off: REPLY"));
    CHECK(contains(text, "on: rejected"));
    CHECK(contains(text, "ERROR 129 (checksum invalid)"));
    CHECK_FALSE(contains(text, "ERROR 0 ("));
}

TEST_CASE("Given the simulated sampler, When the probe runs, Then the result gives its OS version, its DeviceID and its latency [RQ-AKM-017, RQ-AKM-044]",
          "[akm][probe]")
{
    ProbeRig rig(SamplerConfig{.deviceId = 0, .osVersion = {2, 10}});

    const ProbeResult result = rig.run();

    CHECK(result.portsOpened);
    CHECK(result.anySamplerAnswered);
    REQUIRE(result.osVersion.has_value());
    CHECK(result.osVersion->major == 2);
    CHECK(result.osVersion->minor == 10);
    CHECK(result.osVersion->subVersion == 0);
    CHECK(result.confirmationDeviceIds == std::set<std::uint8_t>{0});
    CHECK(result.maxLatency == 0ms);
    CHECK(contains(rig.text(), "# observation: OS version 2.10 (sub-version 0)"));
}

TEST_CASE("Given a sampler on an older OS, When the probe runs, Then it reads that version and records ERROR 0 for the items it lacks [RQ-AKM-017, RQ-AKM-013, RQ-AKM-044]",
          "[akm][probe]")
{
    ProbeRig rig(SamplerConfig{.deviceId = 0, .osVersion = {1, 30}});

    const ProbeResult result = rig.run();

    REQUIRE(result.osVersion.has_value());
    CHECK(result.osVersion->major == 1);
    CHECK(result.osVersion->minor == 30);
    CHECK(contains(rig.text(), "ERROR 0 (not supported"));
    CHECK(contains(rig.text(), "# observation: OS version 1.30 (sub-version 0)"));
}

TEST_CASE("Given a sampler that started with checksums and Still Alive on, When the probe ends, Then both are off [RQ-AKM-018]",
          "[akm][probe]")
{
    ProbeRig rig;
    {
        akm::test::HostProbe before(rig.backend, rig.backend.inputName(), rig.backend.outputName());
        before.send(bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x00, 0x07, 0x01, 0xF7}));  // Still Alive on
        before.send(bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x00, 0x04, 0x01, 0xF7}));  // checksums on
    }
    REQUIRE(rig.sampler.settings().checksum);
    REQUIRE(rig.sampler.settings().stillAlive);

    rig.run();

    CHECK_FALSE(rig.sampler.settings().checksum);
    CHECK_FALSE(rig.sampler.settings().stillAlive);
}

TEST_CASE("Given a sampler that stays silent, When the probe runs, Then it reports no answer for every step and still finishes [RQ-AKM-017]",
          "[akm][probe]")
{
    ProbeRig rig;
    rig.sampler.setBehaviour(SamplerBehaviour{.silent = true});
    const auto start = rig.driver.scheduler().now();

    const ProbeResult result = rig.run();

    CHECK(result.portsOpened);
    CHECK_FALSE(result.anySamplerAnswered);
    CHECK_FALSE(result.osVersion.has_value());
    CHECK(result.framesSent == stepCount);
    CHECK(result.framesReceived == 0);
    // Each step waited for its whole timeout: fifteen times the default three seconds.
    CHECK(rig.driver.scheduler().now() - start == 45s);
    const std::string text = rig.text();
    std::size_t noAnswers = 0;
    for (std::size_t at = text.find("no answer within"); at != std::string::npos; at = text.find("no answer within", at + 1))
        ++noAnswers;
    CHECK(noAnswers == stepCount);
}

TEST_CASE("Given a sampler answering after 200 ms, When the probe runs, Then the latency is measured and logged [RQ-AKM-017, RQ-AKM-010]",
          "[akm][probe]")
{
    ProbeRig rig;
    rig.sampler.setBehaviour(SamplerBehaviour{.replyDelay = 200ms});

    const ProbeResult result = rig.run();

    CHECK(result.anySamplerAnswered);
    CHECK(result.maxLatency == 200ms);
    CHECK(contains(rig.text(), "answered after 200 ms"));
}

TEST_CASE("Given a sampler with DeviceID 3, When the probe targets it, Then the Query to DeviceID 5 goes unanswered and the confirmations carry 3 [RQ-AKM-017, RQ-AKM-007]",
          "[akm][probe]")
{
    ProbeRig rig(SamplerConfig{.deviceId = 3});
    rig.options.target.deviceId = 3;

    const ProbeResult result = rig.run();

    CHECK(result.anySamplerAnswered);
    CHECK(result.confirmationDeviceIds == std::set<std::uint8_t>{3});
    const std::string text = rig.text();
    std::size_t noAnswers = 0;
    for (std::size_t at = text.find("no answer within"); at != std::string::npos; at = text.find("no answer within", at + 1))
        ++noAnswers;
    CHECK(noAnswers == 1);
    CHECK(contains(text, "# observation: DeviceIDs carried by confirmations: 3"));
}

TEST_CASE("Given a sampler that echoes the DeviceID of the message, When the probe runs, Then the log shows both DeviceIDs [RQ-AKM-017, RQ-AKM-007]",
          "[akm][probe]")
{
    ProbeRig rig;
    rig.sampler.setBehaviour(SamplerBehaviour{.confirmationDeviceId = akm::harness::ConfirmationDeviceId::Echoed});

    const ProbeResult result = rig.run();

    CHECK(result.confirmationDeviceIds == std::set<std::uint8_t>{0, 5});
    CHECK(contains(rig.text(), "# observation: DeviceIDs carried by confirmations: 0 5"));
}

TEST_CASE("Given a sampler that sends F0 F7 before each answer, When the probe runs, Then it counts them and reads them as Still Alive [RQ-AKM-017, RQ-AKM-011]",
          "[akm][probe]")
{
    ProbeRig rig;
    rig.sampler.setBehaviour(SamplerBehaviour{.junkBeforeReply = {bytes({0xF0, 0xF7})}});

    const ProbeResult result = rig.run();

    CHECK(result.stillAliveMessagesSeen == stepCount);
    CHECK(contains(rig.text(), "still alive (F0 F7)"));
    CHECK(contains(rig.text(), "# observation: F0 F7 messages seen: 15"));
}

TEST_CASE("Given ports that do not exist, When the probe runs, Then it reports it and sends nothing [RQ-AKM-017]",
          "[akm][probe]")
{
    ProbeRig rig;
    rig.options.target = ScenarioTarget{"nowhere in", "nowhere out", 0};

    const ProbeResult result = rig.run();

    CHECK_FALSE(result.portsOpened);
    CHECK_FALSE(result.anySamplerAnswered);
    CHECK(result.framesSent == 0);
    CHECK(rig.backend.sentByHost().empty());
    CHECK(contains(rig.text(), "port not found"));
}

TEST_CASE("Given a start time in the options, When the probe runs, Then the log header carries it with the target [RQ-AKM-017]",
          "[akm][probe]")
{
    ProbeRig rig;
    rig.options.startedAt = "2026-01-01T00:00:00Z";

    rig.run();

    const std::string text = rig.text();
    CHECK(contains(text, "# started 2026-01-01T00:00:00Z"));
    CHECK(contains(text, "# target: in=\"Simulated S5000 In\" out=\"Simulated S5000 Out\" device-id=0"));
    CHECK(contains(text, "# step 1:"));
    CHECK(contains(text, "# step 15:"));
}
