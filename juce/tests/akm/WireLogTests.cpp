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

// The wire log of the real-sampler scenarios: the text formats shared with the first-contact probe, and the
// two port decorators that log every frame on its way out and in, whichever thread it is on, without the
// session or the sampler knowing. [TASK-AKM-013, RQ-AKM-017, RQ-AKM-019, ADR-AKM-001 (DEC-AKM-008)]
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "TestBytes.hpp"
#include "akm/ManualScheduler.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"
#include "akm/harness/WireFormat.hpp"
#include "akm/harness/WireLog.hpp"

using Catch::Matchers::ContainsSubstring;
using namespace std::chrono_literals;
using akm::ChecksumMode;
using akm::harness::LoggingInputPort;
using akm::harness::LoggingOutputPort;
using akm::harness::SimulatedMidiBackend;
using akm::harness::WireLog;
using akm::test::Bytes;
using akm::test::bytes;
using common::midi::MidiInputCallbacks;
using common::midi::MidiMessage;

namespace
{
    const Bytes QUERY = bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x00, 0x00, 0xF7});
    const Bytes STILL_ALIVE = bytes({0xF0, 0xF7});

    std::vector<std::string> linesOf(const std::string& text)
    {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line))
            lines.push_back(line);
        return lines;
    }

    // An input port that a test drives by hand: it records what it is asked and raises what it is told to.
    class FakeInputPort final : public common::midi::MidiInputPort
    {
    public:
        [[nodiscard]] std::string deviceName() const override { return "fake input"; }
        void setCallbacks(MidiInputCallbacks registered) override { callbacks = std::move(registered); }
        void start() override { ++starts; started = true; }
        void stop() override { ++stops; started = false; }
        [[nodiscard]] bool isStarted() const override { return started; }

        MidiInputCallbacks callbacks;
        int starts = 0;
        int stops = 0;
        bool started = false;
    };
}

TEST_CASE("Given bytes, When written as hexadecimal, Then they read as pairs of upper-case digits separated by spaces, and a dash stands for none where one is wanted [TASK-AKM-013]",
          "[akm][wirelog]")
{
    CHECK(akm::harness::hex(bytes({0xF0, 0x47, 0x5E, 0x00, 0xF7})) == "F0 47 5E 00 F7");
    CHECK(akm::harness::hex(Bytes{}).empty());
    CHECK(akm::harness::hexOrDash(Bytes{}) == "-");
    CHECK(akm::harness::hexOrDash(bytes({0x0A})) == "0A");
}

TEST_CASE("Given a duration, When written as seconds and as milliseconds, Then the seconds have three decimals in a width of nine and the milliseconds are whole [TASK-AKM-013]",
          "[akm][wirelog]")
{
    CHECK(akm::harness::secondsText(1500ms) == "    1.500");
    CHECK(akm::harness::secondsText(0ms) == "    0.000");
    CHECK(akm::harness::secondsText(12345ms) == "   12.345");
    CHECK(akm::harness::millisecondsOf(1500us) == 1);
    CHECK(akm::harness::millisecondsOf(2s) == 2000);
}

TEST_CASE("Given a received message, When it is read under a checksum mode, Then the reading names its kind, DeviceID, user-ref, section, item and data, or why it was refused [TASK-AKM-013]",
          "[akm][wirelog]")
{
    const Bytes done = bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x44, 0x00, 0x04, 0xF7});
    const Bytes osVersion = bytes({0xF0, 0x47, 0x5E, 0x00, 0x13, 0x52, 0x02, 0x00, 0x02, 0x0E, 0xF7});
    // ERROR 129 with its checksum: 1A + 45 + 00 + 00 + 01 + 01 = 61.
    const Bytes checksumError = bytes({0xF0, 0x47, 0x5E, 0x00, 0x1A, 0x45, 0x00, 0x00, 0x01, 0x01, 0x61, 0xF7});
    const Bytes foreign = bytes({0xF0, 0x43, 0x00, 0x01, 0xF7});

    CHECK(akm::harness::reading(done, ChecksumMode::Off) == "DONE dev 0 ref 10 sec 00 item 04 data -");
    CHECK(akm::harness::reading(osVersion, ChecksumMode::Off) == "REPLY dev 0 ref 13 sec 02 item 00 data 02 0E");
    CHECK_THAT(akm::harness::reading(checksumError, ChecksumMode::On), ContainsSubstring("ERROR 129"));
    CHECK_THAT(akm::harness::reading(checksumError, ChecksumMode::On), ContainsSubstring("ref 1A"));
    CHECK(akm::harness::reading(STILL_ALIVE, ChecksumMode::Off) == "still alive (F0 F7)");
    CHECK_THAT(akm::harness::reading(foreign, ChecksumMode::Off), ContainsSubstring("rejected: foreign"));
    // The same frame reads differently when a checksum is expected: the mode that is wrong is visible.
    CHECK_THAT(akm::harness::reading(done, ChecksumMode::On), ContainsSubstring("rejected"));
}

TEST_CASE("Given a logging output port, When frames are sent through it, Then each reaches the sampler, is logged as an OUT line with its time, in order, and is counted [TASK-AKM-013]",
          "[akm][wirelog]")
{
    akm::ManualScheduler scheduler;
    SimulatedMidiBackend backend(scheduler);
    auto& sampler = backend.addSampler();
    std::ostringstream out;
    WireLog log(out, scheduler);
    const auto output = backend.openOutput(backend.outputName());
    LoggingOutputPort logged(*output, log);

    scheduler.advance(1500ms);
    logged.send(MidiMessage::sysEx(QUERY));
    scheduler.advance(20ms);
    const Bytes second = bytes({0xF0, 0x47, 0x5E, 0x00, 0x11, 0x00, 0x01, 0x00, 0xF7});
    logged.send(MidiMessage::sysEx(second));

    CHECK(logged.deviceName() == output->deviceName());
    CHECK(sampler.receivedFrames().size() == 2);
    CHECK(log.framesSent() == 2);
    const std::vector<std::string> lines = linesOf(out.str());
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "    1.500  OUT  F0 47 5E 00 10 00 00 F7");
    CHECK(lines[1] == "    1.520  OUT  F0 47 5E 00 11 00 01 00 F7");
}

TEST_CASE("Given a logging input port, When the sampler answers, Then the callbacks registered through it receive every message, each is logged as an IN line with its readings and the F0 F7 are counted apart [TASK-AKM-013]",
          "[akm][wirelog]")
{
    akm::ManualScheduler scheduler;
    SimulatedMidiBackend backend(scheduler);
    backend.addSampler();
    std::ostringstream out;
    WireLog log(out, scheduler);
    const auto input = backend.openInput(backend.inputName());
    LoggingInputPort logged(*input, log);
    std::vector<Bytes> received;
    MidiInputCallbacks callbacks;
    callbacks.onSysExMessage = [&received](const MidiMessage& message) { received.push_back(message.toBytes()); };
    logged.setCallbacks(std::move(callbacks));
    logged.start();
    CHECK(logged.isStarted());
    CHECK(logged.deviceName() == input->deviceName());

    const auto output = backend.openOutput(backend.outputName());
    output->send(MidiMessage::sysEx(QUERY));  // the sampler answers OK, then DONE
    backend.injectToHost(STILL_ALIVE);
    logged.stop();
    CHECK_FALSE(logged.isStarted());

    REQUIRE(received.size() == 3);
    CHECK(received[2] == STILL_ALIVE);
    CHECK(log.framesReceived() == 3);
    CHECK(log.stillAliveMessages() == 1);
    const std::vector<std::string> lines = linesOf(out.str());
    REQUIRE(lines.size() == 3);
    CHECK_THAT(lines[0], ContainsSubstring("  IN   F0 47 5E 00 10 4F 00 00 F7 | off: OK dev 0 ref 10 sec 00 item 00 data -"));
    CHECK_THAT(lines[0], ContainsSubstring("| on: rejected"));
    CHECK_THAT(lines[1], ContainsSubstring("DONE"));
    CHECK_THAT(lines[2], ContainsSubstring("IN   F0 F7 | off: still alive (F0 F7)"));
}

TEST_CASE("Given an input port that reports an error, When it does, Then the error is logged and passed on to the callback registered through the decorator [TASK-AKM-013]",
          "[akm][wirelog]")
{
    akm::ManualScheduler scheduler;
    std::ostringstream out;
    WireLog log(out, scheduler);
    FakeInputPort inner;
    LoggingInputPort logged(inner, log);
    std::string passedOn;
    MidiInputCallbacks callbacks;
    callbacks.onError = [&passedOn](const std::string& description) { passedOn = description; };
    logged.setCallbacks(std::move(callbacks));

    logged.start();
    inner.callbacks.onError("buffer overrun");
    logged.stop();

    CHECK(passedOn == "buffer overrun");
    CHECK_THAT(out.str(), ContainsSubstring("# input error: buffer overrun"));
    CHECK(inner.starts == 1);
    CHECK(inner.stops == 1);
    CHECK(logged.deviceName() == "fake input");
}

TEST_CASE("Given callbacks without an error handler, When the port reports an error, Then it is still logged and nothing fails [TASK-AKM-013]",
          "[akm][wirelog]")
{
    akm::ManualScheduler scheduler;
    std::ostringstream out;
    WireLog log(out, scheduler);
    FakeInputPort inner;
    LoggingInputPort logged(inner, log);
    logged.setCallbacks(MidiInputCallbacks{});

    CHECK_NOTHROW(inner.callbacks.onError("driver reset"));
    CHECK_NOTHROW(inner.callbacks.onSysExMessage(MidiMessage::sysEx(QUERY)));

    CHECK_THAT(out.str(), ContainsSubstring("# input error: driver reset"));
    CHECK(log.framesReceived() == 1);
}

TEST_CASE("Given notes written from two threads at once, When the log is read, Then every line is whole [TASK-AKM-013]",
          "[akm][wirelog][threads]")
{
    constexpr int NOTES_PER_THREAD = 300;
    akm::ManualScheduler scheduler;
    std::ostringstream out;
    WireLog log(out, scheduler);
    const auto write = [&log](const std::string& tag) {
        for (int index = 0; index < NOTES_PER_THREAD; ++index)
            log.note(tag + " note " + std::to_string(index) + " with enough text after it to be worth interleaving");
    };

    std::thread first(write, "alpha");
    std::thread second(write, "beta");
    first.join();
    second.join();

    const std::vector<std::string> lines = linesOf(out.str());
    REQUIRE(lines.size() == 2 * NOTES_PER_THREAD);
    const std::regex whole(R"(^# (alpha|beta) note \d+ with enough text after it to be worth interleaving$)");
    for (const std::string& line : lines)
        CHECK(std::regex_match(line, whole));
}

TEST_CASE("Given the log, When time passes on the scheduler, Then the time elapsed since it was opened follows it [TASK-AKM-013]",
          "[akm][wirelog]")
{
    akm::ManualScheduler scheduler;
    scheduler.advance(10s);
    std::ostringstream out;
    WireLog log(out, scheduler);
    CHECK(log.elapsed() == 0ms);

    scheduler.advance(250ms);

    CHECK(log.elapsed() == 250ms);
}
