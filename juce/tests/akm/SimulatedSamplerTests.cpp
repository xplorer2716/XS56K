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

// The simulated sampler: a MidiBackend that models what the spec says of the S5000 (addressing, §00
// state, checksum handling, the OK / DONE / REPLY / ERROR flows, OS versions) with knobs for what a real
// bus does badly. Frames are written out byte by byte, from the spec, not built by the codec.
// [TASK-AKM-007, RQ-AKM-016, RQ-AKM-019, ADR-AKM-001 (DEC-AKM-008)]
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <variant>

#include "HostProbe.hpp"
#include "TestBytes.hpp"
#include "akm/Confirmation.hpp"
#include "akm/ManualScheduler.hpp"
#include "akm/RealScheduler.hpp"
#include "akm/testing/SimulatedMidiBackend.hpp"

using namespace std::chrono_literals;
using akm::ChecksumMode;
using akm::Confirmation;
using akm::test::Bytes;
using akm::test::HostProbe;
using akm::test::bytes;
using akm::testing::ConfirmationDeviceId;
using akm::testing::DeliveryMode;
using akm::testing::SamplerBehaviour;
using akm::testing::SamplerConfig;
using akm::testing::SimulatedMidiBackend;

namespace
{
    // The Reply ID bytes of Table 2.
    constexpr unsigned int OK = 0x4F;
    constexpr unsigned int DONE = 0x44;
    constexpr unsigned int REPLY = 0x52;
    constexpr unsigned int ERR = 0x45;

    // A command frame of §00 (SysEx configuration), user-ref 10, no checksum.
    Bytes sysexConfig(unsigned int deviceByte, unsigned int item, std::initializer_list<unsigned int> data = {})
    {
        Bytes frame = bytes({0xF0, 0x47, 0x5E, deviceByte, 0x10, 0x00, item});
        for (const unsigned int value : data)
            frame.push_back(static_cast<std::uint8_t>(value));
        frame.push_back(0xF7);
        return frame;
    }

    Bytes query(unsigned int deviceByte = 0x00)
    {
        return sysexConfig(deviceByte, 0x00);
    }

    // What a host, a manual scheduler and a simulated sampler of DeviceID 0 make: the usual rig.
    struct Rig
    {
        akm::ManualScheduler scheduler;
        SimulatedMidiBackend backend{scheduler};
        akm::testing::SimulatedSampler& sampler = backend.addSampler();
        HostProbe host{backend, backend.inputName(), backend.outputName()};
    };

    const Confirmation& confirmationAt(const std::vector<akm::DecodedMessage>& messages, std::size_t index)
    {
        REQUIRE(index < messages.size());
        const auto* confirmation = std::get_if<Confirmation>(&messages[index]);
        REQUIRE(confirmation != nullptr);
        return *confirmation;
    }
}

TEST_CASE("Given the simulated sampler in its default mode, When a frame is sent through its output port, Then the confirmations arrive on its input port with the user-refs of the frame [RQ-AKM-016]",
          "[akm][simulated]")
{
    Rig rig;

    rig.host.send(query());

    const auto messages = rig.host.decoded(ChecksumMode::Off);
    REQUIRE(messages.size() == 2);
    CHECK(confirmationAt(messages, 0).replyId == akm::ReplyId::Ok);
    CHECK(confirmationAt(messages, 1).replyId == akm::ReplyId::Done);
    for (std::size_t index = 0; index < messages.size(); ++index)
    {
        CHECK(confirmationAt(messages, index).userRefs == bytes({0x10}));
        CHECK(confirmationAt(messages, index).section == 0x00);
        CHECK(confirmationAt(messages, index).item == 0x00);
    }
}

TEST_CASE("Given the simulated sampler scripted to stay silent, When a frame is sent, Then nothing is delivered but the command still executes [RQ-AKM-016]",
          "[akm][simulated]")
{
    Rig rig;
    rig.sampler.setBehaviour(SamplerBehaviour{.silent = true});

    rig.host.send(query());

    CHECK(rig.host.arrivalCount() == 0);
    CHECK(rig.sampler.acceptedCommands().size() == 1);
}

TEST_CASE("Given two simulated samplers with DeviceIDs 3 and 7, When a Query with DeviceID 0 is sent, Then both answer, each with its own DeviceID [RQ-AKM-016, RQ-AKM-012]",
          "[akm][simulated]")
{
    akm::ManualScheduler scheduler;
    SimulatedMidiBackend backend(scheduler);
    backend.addSampler(SamplerConfig{.deviceId = 3});
    backend.addSampler(SamplerConfig{.deviceId = 7});
    HostProbe host(backend, backend.inputName(), backend.outputName());

    host.send(query(0x00));

    const auto messages = host.decoded(ChecksumMode::Off);
    REQUIRE(messages.size() == 4);
    CHECK(confirmationAt(messages, 0).deviceId == 3);
    CHECK(confirmationAt(messages, 1).deviceId == 3);
    CHECK(confirmationAt(messages, 2).deviceId == 7);
    CHECK(confirmationAt(messages, 3).deviceId == 7);
}

TEST_CASE("Given two simulated samplers with DeviceIDs 3 and 7, When a Query with DeviceID 5 is sent, Then neither answers [RQ-AKM-016]",
          "[akm][simulated]")
{
    akm::ManualScheduler scheduler;
    SimulatedMidiBackend backend(scheduler);
    backend.addSampler(SamplerConfig{.deviceId = 3});
    backend.addSampler(SamplerConfig{.deviceId = 7});
    HostProbe host(backend, backend.inputName(), backend.outputName());

    host.send(query(0x05));

    CHECK(host.arrivalCount() == 0);
}

TEST_CASE("Given two simulated samplers with DeviceIDs 3 and 7, When a Query with DeviceID 3 is sent, Then only that one answers [RQ-AKM-016]",
          "[akm][simulated]")
{
    akm::ManualScheduler scheduler;
    SimulatedMidiBackend backend(scheduler);
    backend.addSampler(SamplerConfig{.deviceId = 3});
    backend.addSampler(SamplerConfig{.deviceId = 7});
    HostProbe host(backend, backend.inputName(), backend.outputName());

    host.send(query(0x03));

    const auto messages = host.decoded(ChecksumMode::Off);
    REQUIRE(messages.size() == 2);
    CHECK(confirmationAt(messages, 0).deviceId == 3);
    CHECK(confirmationAt(messages, 1).deviceId == 3);
}

TEST_CASE("Given a simulated sampler with DeviceID 0, When a command with DeviceID 5 is sent, Then it executes it [RQ-AKM-016]",
          "[akm][simulated]")
{
    Rig rig;

    rig.host.send(query(0x05));

    const auto executed = rig.sampler.acceptedCommands();
    REQUIRE(executed.size() == 1);
    CHECK(executed[0].deviceId == 5);
    CHECK(rig.host.arrivalCount() == 2);
}

TEST_CASE("Given a sampler that answers with the DeviceID of the message, When a command with DeviceID 5 is sent to a sampler with DeviceID 0, Then the confirmation carries 5 [RQ-AKM-016, RQ-AKM-007]",
          "[akm][simulated]")
{
    Rig rig;
    rig.sampler.setBehaviour(SamplerBehaviour{.confirmationDeviceId = ConfirmationDeviceId::Echoed});

    rig.host.send(query(0x05));

    const auto messages = rig.host.decoded(ChecksumMode::Off);
    REQUIRE(messages.size() == 2);
    CHECK(confirmationAt(messages, 0).deviceId == 5);
}

TEST_CASE("Given the user-ref count in the device byte, When a command with two user-refs is sent, Then the confirmations carry both and the same count bits [RQ-AKM-016]",
          "[akm][simulated]")
{
    Rig rig;

    rig.host.send(bytes({0xF0, 0x47, 0x5E, 0x20, 0x10, 0x11, 0x00, 0x00, 0xF7}));

    const auto arrivals = rig.host.arrivals();
    REQUIRE(arrivals.size() == 2);
    CHECK(arrivals[0].bytes[3] == 0x20);
    const auto messages = rig.host.decoded(ChecksumMode::Off);
    CHECK(confirmationAt(messages, 1).userRefs == bytes({0x10, 0x11}));
}

TEST_CASE("Given checksums on in the simulation and a frame without checksum, When it is sent, Then it answers ERROR 81 [RQ-AKM-016, RQ-AKM-003]",
          "[akm][simulated]")
{
    Rig rig;
    rig.host.send(sysexConfig(0x00, 0x04, {0x01}));  // &04: checksum ON; the sampler was in mode off
    rig.host.clear();

    rig.host.send(query());

    // The sampler is now in mode on, so its confirmations carry a checksum.
    const auto messages = rig.host.decoded(ChecksumMode::On);
    REQUIRE(messages.size() == 1);
    const Confirmation& error = confirmationAt(messages, 0);
    CHECK(error.replyId == akm::ReplyId::Error);
    CHECK(error.data == bytes({0x01, 0x01}));
    CHECK(akm::errorNumber(error) == 0x81);
}

TEST_CASE("Given checksums on in the simulation and a frame with a valid checksum, When it is sent, Then it is executed and the confirmations carry a checksum [RQ-AKM-016, RQ-AKM-003]",
          "[akm][simulated]")
{
    Rig rig;
    rig.host.send(sysexConfig(0x00, 0x04, {0x01}));
    rig.host.clear();

    // 10 + 00 + 00 = 10.
    rig.host.send(bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x00, 0x00, 0x10, 0xF7}));

    CHECK(rig.host.replyIds(ChecksumMode::On) == bytes({OK, DONE}));
    // OK carries 10 + 4F + 00 + 00 = 5F as its checksum.
    CHECK(rig.host.arrivals()[0].bytes == bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x4F, 0x00, 0x00, 0x5F, 0xF7}));
}

TEST_CASE("Given checksums off and a frame with a trailing checksum, When it is sent, Then the extra byte is ignored [RQ-AKM-016, RQ-AKM-003]",
          "[akm][simulated]")
{
    // Spec p. 4: with checksums disabled, a checksum that is sent anyway is ignored.
    Rig rig;

    rig.host.send(bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x00, 0x00, 0x10, 0xF7}));

    CHECK(rig.host.replyIds(ChecksumMode::Off) == bytes({OK, DONE}));
}

TEST_CASE("Given a command that changes the checksum mode, When the sampler confirms it, Then the confirmation uses the previous mode unless the knob says otherwise [RQ-AKM-016, RQ-AKM-041]",
          "[akm][simulated]")
{
    SECTION("previous mode by default")
    {
        Rig rig;
        rig.host.send(sysexConfig(0x00, 0x04, {0x01}));

        // OK and DONE, without a checksum: 9 bytes each.
        REQUIRE(rig.host.arrivalCount() == 2);
        CHECK(rig.host.arrivals()[1].bytes == bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x44, 0x00, 0x04, 0xF7}));
    }
    SECTION("new mode when the knob is set")
    {
        Rig rig;
        rig.sampler.setBehaviour(SamplerBehaviour{.checksumChangeAppliesToOwnConfirmation = true});
        rig.host.send(sysexConfig(0x00, 0x04, {0x01}));

        // OK, sent before the change, has none; DONE has one: 10 + 44 + 00 + 04 = 58.
        REQUIRE(rig.host.arrivalCount() == 2);
        CHECK(rig.host.arrivals()[1].bytes == bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x44, 0x00, 0x04, 0x58, 0xF7}));
    }
}

TEST_CASE("Given the mode where the confirmation is delivered from another thread before send() returns, When a frame is sent, Then the confirmation can arrive before send() returns [RQ-AKM-016, RQ-AKM-020]",
          "[akm][simulated]")
{
    Rig rig;
    rig.backend.setDeliveryMode(DeliveryMode::OnOtherThreadBeforeSendReturns);

    rig.host.send(query());

    const auto arrivals = rig.host.arrivals();
    REQUIRE(arrivals.size() == 2);
    for (const auto& arrival : arrivals)
    {
        CHECK(arrival.thread != std::this_thread::get_id());
        CHECK_FALSE(arrival.sendHadReturned);
    }
}

TEST_CASE("Given the default delivery, When a frame is sent, Then the confirmations arrive on the sending thread before send() returns [RQ-AKM-016]",
          "[akm][simulated]")
{
    Rig rig;

    rig.host.send(query());

    const auto arrivals = rig.host.arrivals();
    REQUIRE(arrivals.size() == 2);
    for (const auto& arrival : arrivals)
    {
        CHECK(arrival.thread == std::this_thread::get_id());
        CHECK_FALSE(arrival.sendHadReturned);
    }
}

TEST_CASE("Given the mode where the confirmation is delivered from another thread, When a frame is sent, Then it arrives on a thread other than the sender's [RQ-AKM-016, RQ-AKM-020]",
          "[akm][simulated]")
{
    akm::ManualScheduler scheduler;
    SimulatedMidiBackend backend(scheduler);
    backend.addSampler();
    backend.setDeliveryMode(DeliveryMode::OnOtherThread);
    HostProbe host(backend, backend.inputName(), backend.outputName());

    host.send(query());

    // No ordering with send() is promised, so wait for the delivery, with a bound.
    for (int attempt = 0; attempt < 10000 && host.arrivalCount() < 2; ++attempt)
        std::this_thread::sleep_for(1ms);
    const auto arrivals = host.arrivals();
    REQUIRE(arrivals.size() == 2);
    for (const auto& arrival : arrivals)
        CHECK(arrival.thread != std::this_thread::get_id());
}

TEST_CASE("Given notifications turned off, When a Query is sent, Then only the DONE arrives [RQ-AKM-016, RQ-AKM-009]",
          "[akm][simulated]")
{
    Rig rig;
    rig.host.send(sysexConfig(0x00, 0x01, {0x00}));  // &01: notification OFF
    rig.host.clear();

    rig.host.send(query());

    CHECK(rig.host.replyIds(ChecksumMode::Off) == bytes({DONE}));
    CHECK_FALSE(rig.sampler.settings().notification);
}

TEST_CASE("Given the Echo Message with four data bytes, When it is sent, Then the REPLY carries the four bytes [RQ-AKM-016, RQ-AKM-015]",
          "[akm][simulated]")
{
    Rig rig;

    rig.host.send(sysexConfig(0x00, 0x06, {0x01, 0x02, 0x03, 0x04}));

    const auto messages = rig.host.decoded(ChecksumMode::Off);
    REQUIRE(messages.size() == 2);
    CHECK(confirmationAt(messages, 0).replyId == akm::ReplyId::Ok);
    CHECK(confirmationAt(messages, 1).replyId == akm::ReplyId::Reply);
    CHECK(confirmationAt(messages, 1).data == bytes({0x01, 0x02, 0x03, 0x04}));
}

TEST_CASE("Given the Echo Message with too few data bytes, When it is sent, Then it answers ERROR 1 [RQ-AKM-016]",
          "[akm][simulated]")
{
    Rig rig;

    rig.host.send(sysexConfig(0x00, 0x06, {0x01, 0x02}));

    const auto messages = rig.host.decoded(ChecksumMode::Off);
    REQUIRE(messages.size() == 2);
    CHECK(akm::errorNumber(confirmationAt(messages, 1)) == 0x01);
}

TEST_CASE("Given each toggle of section 00, When it is sent, Then the sampler's settings follow [RQ-AKM-016, RQ-AKM-014]",
          "[akm][simulated]")
{
    Rig rig;

    rig.host.send(sysexConfig(0x00, 0x03, {0x00}));
    rig.host.send(sysexConfig(0x00, 0x05, {0x01}));
    rig.host.send(sysexConfig(0x00, 0x07, {0x01}));

    const auto settings = rig.sampler.settings();
    CHECK_FALSE(settings.syncLcd);
    CHECK(settings.autoScreenUpdate);
    CHECK(settings.stillAlive);
    CHECK(settings.notification);
    CHECK_FALSE(settings.checksum);
}

TEST_CASE("Given an older OS version, When an item introduced later is sent, Then the sampler answers ERROR 0 [RQ-AKM-016, RQ-AKM-013]",
          "[akm][simulated]")
{
    // Sync LCD (&03) exists since OS 2.00 and Still Alive (&07) since OS 2.10 (spec, modification history).
    akm::ManualScheduler scheduler;
    SimulatedMidiBackend backend(scheduler);
    backend.addSampler(SamplerConfig{.deviceId = 1, .osVersion = {1, 30}});
    backend.addSampler(SamplerConfig{.deviceId = 2, .osVersion = {2, 0}});
    backend.addSampler(SamplerConfig{.deviceId = 3, .osVersion = {2, 10}});
    HostProbe host(backend, backend.inputName(), backend.outputName());

    SECTION("Sync LCD")
    {
        host.send(sysexConfig(0x01, 0x03, {0x00}));
        host.send(sysexConfig(0x02, 0x03, {0x00}));
        host.send(sysexConfig(0x03, 0x03, {0x00}));
        const auto messages = host.decoded(ChecksumMode::Off);
        REQUIRE(messages.size() == 6);
        CHECK(akm::errorNumber(confirmationAt(messages, 1)) == 0x00);
        CHECK(confirmationAt(messages, 3).replyId == akm::ReplyId::Done);
        CHECK(confirmationAt(messages, 5).replyId == akm::ReplyId::Done);
    }
    SECTION("Still Alive")
    {
        host.send(sysexConfig(0x01, 0x07, {0x01}));
        host.send(sysexConfig(0x02, 0x07, {0x01}));
        host.send(sysexConfig(0x03, 0x07, {0x01}));
        const auto messages = host.decoded(ChecksumMode::Off);
        REQUIRE(messages.size() == 6);
        CHECK(akm::errorNumber(confirmationAt(messages, 1)) == 0x00);
        CHECK(akm::errorNumber(confirmationAt(messages, 3)) == 0x00);
        CHECK(confirmationAt(messages, 5).replyId == akm::ReplyId::Done);
    }
}

TEST_CASE("Given an unsupported section, item or value, When it is sent, Then the sampler answers the matching ERROR [RQ-AKM-016]",
          "[akm][simulated]")
{
    Rig rig;

    // Section 0A, which the simulation does not model (yet): not supported.
    rig.host.send(bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x0A, 0x00, 0xF7}));
    // §00 has no item 02 (the spec skips it): not supported.
    rig.host.send(sysexConfig(0x00, 0x02));
    // Checksum mode 5: out of range. And no value at all: invalid format.
    rig.host.send(sysexConfig(0x00, 0x04, {0x05}));
    rig.host.send(sysexConfig(0x00, 0x04));

    const auto messages = rig.host.decoded(ChecksumMode::Off);
    REQUIRE(messages.size() == 8);
    CHECK(akm::errorNumber(confirmationAt(messages, 1)) == 0x00);
    CHECK(akm::errorNumber(confirmationAt(messages, 3)) == 0x00);
    CHECK(akm::errorNumber(confirmationAt(messages, 5)) == 0x02);
    CHECK(akm::errorNumber(confirmationAt(messages, 7)) == 0x01);
    CHECK_FALSE(rig.sampler.settings().checksum);
}

TEST_CASE("Given messages that are not for it, When they are sent, Then the sampler ignores them [RQ-AKM-016]",
          "[akm][simulated]")
{
    Rig rig;

    rig.host.send(bytes({0xF0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xF7}));  // another manufacturer
    rig.host.send(bytes({0xF0, 0x47, 0x5F, 0x00, 0x10, 0x00, 0x00, 0xF7}));        // another model
    rig.host.send(bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0xF7}));                    // no section or item

    CHECK(rig.host.arrivalCount() == 0);
    CHECK(rig.sampler.acceptedCommands().empty());
    CHECK(rig.sampler.receivedFrames().size() == 3);
}

TEST_CASE("Given settings changed in one session, When the ports are closed and reopened, Then the settings are kept until the sampler is power-cycled [RQ-AKM-016, RQ-AKM-040]",
          "[akm][simulated]")
{
    akm::ManualScheduler scheduler;
    SimulatedMidiBackend backend(scheduler);
    auto& sampler = backend.addSampler();
    {
        HostProbe first(backend, backend.inputName(), backend.outputName());
        first.send(sysexConfig(0x00, 0x04, {0x01}));
    }
    CHECK(sampler.settings().checksum);
    {
        HostProbe second(backend, backend.inputName(), backend.outputName());
        CHECK(sampler.settings().checksum);
    }

    sampler.powerCycle();

    CHECK_FALSE(sampler.settings().checksum);
    CHECK(sampler.settings().notification);
}

TEST_CASE("Given a reply delay of 500 ms on the manual scheduler, When time is advanced, Then the confirmations arrive at 500 ms [RQ-AKM-016, RQ-AKM-010]",
          "[akm][simulated]")
{
    Rig rig;
    rig.sampler.setBehaviour(SamplerBehaviour{.replyDelay = 500ms});

    rig.host.send(query());
    CHECK(rig.host.arrivalCount() == 0);
    rig.scheduler.advance(499ms);
    CHECK(rig.host.arrivalCount() == 0);
    rig.scheduler.advance(1ms);

    CHECK(rig.host.replyIds(ChecksumMode::Off) == bytes({OK, DONE}));
}

TEST_CASE("Given the sampler scripted to answer twice, When a Query is sent, Then each confirmation arrives twice [RQ-AKM-016, RQ-AKM-009]",
          "[akm][simulated]")
{
    Rig rig;
    rig.sampler.setBehaviour(SamplerBehaviour{.timesEachReply = 2});

    rig.host.send(query());

    CHECK(rig.host.replyIds(ChecksumMode::Off) == bytes({OK, OK, DONE, DONE}));
}

TEST_CASE("Given the sampler scripted to follow a REPLY with an ERROR, When an Echo is sent, Then the ERROR arrives after the REPLY [RQ-AKM-016, RQ-AKM-007]",
          "[akm][simulated]")
{
    Rig rig;
    rig.sampler.setBehaviour(SamplerBehaviour{.errorAfterReply = std::uint16_t{0x03}});

    rig.host.send(sysexConfig(0x00, 0x06, {0x01, 0x02, 0x03, 0x04}));

    CHECK(rig.host.replyIds(ChecksumMode::Off) == bytes({OK, REPLY, ERR}));
    const auto messages = rig.host.decoded(ChecksumMode::Off);
    CHECK(akm::errorNumber(confirmationAt(messages, 2)) == 0x03);
}

TEST_CASE("Given foreign, malformed and Still Alive messages scripted before the reply, When a Query is sent, Then they arrive first, in order [RQ-AKM-016, RQ-AKM-006]",
          "[akm][simulated]")
{
    Rig rig;
    rig.sampler.setBehaviour(SamplerBehaviour{.junkBeforeReply = {
        bytes({0xF0, 0x43, 0x10, 0x4C, 0x00, 0xF7}),            // foreign
        bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x44, 0xF7}),      // malformed: truncated confirmation
        bytes({0xF0, 0xF7}),                                    // Still Alive
    }});

    rig.host.send(query());

    const auto arrivals = rig.host.arrivals();
    REQUIRE(arrivals.size() == 5);
    CHECK(arrivals[0].bytes == bytes({0xF0, 0x43, 0x10, 0x4C, 0x00, 0xF7}));
    CHECK(arrivals[1].bytes == bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x44, 0xF7}));
    CHECK(arrivals[2].bytes == bytes({0xF0, 0xF7}));
    CHECK(rig.host.replyIds(ChecksumMode::Off) == bytes({0, 0, 0, OK, DONE}));
}

TEST_CASE("Given Still Alive on and a reply delayed by 2500 ms, When time is advanced, Then F0 F7 arrives every second until the reply [RQ-AKM-016, RQ-AKM-011]",
          "[akm][simulated]")
{
    Rig rig;
    rig.host.send(sysexConfig(0x00, 0x07, {0x01}));  // &07: Still Alive ON
    rig.host.clear();
    rig.sampler.setBehaviour(SamplerBehaviour{.replyDelay = 2500ms});

    rig.host.send(query());
    rig.scheduler.advance(999ms);
    CHECK(rig.host.arrivalCount() == 0);
    rig.scheduler.advance(1ms);
    CHECK(rig.host.arrivalCount() == 1);
    rig.scheduler.advance(1s);
    CHECK(rig.host.arrivalCount() == 2);
    rig.scheduler.advance(499ms);
    CHECK(rig.host.arrivalCount() == 2);
    rig.scheduler.advance(1ms);

    const auto arrivals = rig.host.arrivals();
    REQUIRE(arrivals.size() == 4);
    CHECK(arrivals[0].bytes == bytes({0xF0, 0xF7}));
    CHECK(arrivals[1].bytes == bytes({0xF0, 0xF7}));
    CHECK(rig.host.replyIds(ChecksumMode::Off) == bytes({0, 0, OK, DONE}));
}

TEST_CASE("Given Still Alive off and a delayed reply, When time is advanced, Then no F0 F7 arrives [RQ-AKM-016, RQ-AKM-011]",
          "[akm][simulated]")
{
    Rig rig;
    rig.sampler.setBehaviour(SamplerBehaviour{.replyDelay = 2500ms});

    rig.host.send(query());
    rig.scheduler.advance(2499ms);

    CHECK(rig.host.arrivalCount() == 0);
}

TEST_CASE("Given an unsolicited message injected on the input, When it is delivered, Then the host receives it as sent [RQ-AKM-016, RQ-AKM-007]",
          "[akm][simulated]")
{
    Rig rig;

    rig.backend.injectToHost(bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x44, 0x00, 0x04, 0xF7}));

    REQUIRE(rig.host.arrivalCount() == 1);
    CHECK(rig.host.arrivals()[0].bytes == bytes({0xF0, 0x47, 0x5E, 0x00, 0x10, 0x44, 0x00, 0x04, 0xF7}));
}

TEST_CASE("Given the frames the host sent, When they are read back, Then they are logged in order [RQ-AKM-016, RQ-AKM-008]",
          "[akm][simulated]")
{
    Rig rig;
    rig.host.send(query());
    rig.host.send(sysexConfig(0x00, 0x01, {0x00}));

    const auto sent = rig.backend.sentByHost();

    REQUIRE(sent.size() == 2);
    CHECK(sent[0].toBytes() == query());
    CHECK(sent[1].toBytes() == sysexConfig(0x00, 0x01, {0x00}));
}

TEST_CASE("Given a device name that does not exist, When a port is opened, Then it is refused as by any backend [RQ-AKM-019]",
          "[akm][simulated]")
{
    akm::ManualScheduler scheduler;
    SimulatedMidiBackend backend(scheduler, "sim in", "sim out");

    CHECK(backend.inputDeviceNames() == std::vector<std::string>{"sim in"});
    CHECK(backend.outputDeviceNames() == std::vector<std::string>{"sim out"});
    CHECK(backend.openInput("sim in") != nullptr);
    CHECK(backend.openOutput("sim out") != nullptr);
    CHECK(backend.openInput("elsewhere") == nullptr);
    CHECK(backend.openOutput("elsewhere") == nullptr);
}

TEST_CASE("Given a backend destroyed while a delayed reply is scheduled, When time is advanced, Then nothing happens [RQ-AKM-016, RQ-AKM-042]",
          "[akm][simulated]")
{
    akm::ManualScheduler scheduler;
    {
        SimulatedMidiBackend backend(scheduler);
        backend.addSampler().setBehaviour(SamplerBehaviour{.replyDelay = 100ms});
        HostProbe host(backend, backend.inputName(), backend.outputName());
        host.send(query());
    }

    scheduler.advance(1s);

    SUCCEED();
}

TEST_CASE("Given the real scheduler and a reply delay, When the delay has passed, Then the confirmations arrive from the scheduler's thread [RQ-AKM-016, RQ-AKM-020]",
          "[akm][simulated]")
{
    akm::RealScheduler scheduler;
    SimulatedMidiBackend backend(scheduler);
    backend.addSampler().setBehaviour(SamplerBehaviour{.replyDelay = 50ms});
    HostProbe host(backend, backend.inputName(), backend.outputName());

    host.send(query());

    for (int attempt = 0; attempt < 10000 && host.arrivalCount() < 2; ++attempt)
        std::this_thread::sleep_for(1ms);
    const auto arrivals = host.arrivals();
    REQUIRE(arrivals.size() == 2);
    CHECK(arrivals[0].thread != std::this_thread::get_id());
}
