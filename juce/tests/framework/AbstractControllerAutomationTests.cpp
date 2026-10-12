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

// What the controller does with a message from its automation input device, pinned before the function that
// handles it is cut into steps: a mapped control change sets the parameter by the reference's scaling and tells the
// UI, the disabled control change is dropped, anything else is forwarded to the synth on the tone's channel.
// The controller is driven through the in-memory MIDI backend, with no thread started.
// [TASK-QLT-006, RQ-QLT-005, RQ-QLT-004, ADR-QLT-001 (DEC-QLT-004), RQ-FMW-032, RQ-FMW-033, RQ-FMW-050,
// RQ-FMW-051, RQ-FMW-052]
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/midi/MidiMessage.hpp"
#include "common/midi/MockMidiBackend.hpp"
#include "midiapp/controller/AbstractController.hpp"
#include "midiapp/model/AbstractParameter.hpp"
#include "midiapp/model/AbstractTone.hpp"

using common::midi::ChannelCommand;
using common::midi::MidiMessage;
using common::midi::MockMidiBackend;
using midiapp::controller::AbstractController;
using midiapp::model::AbstractParameter;
using midiapp::model::AbstractTone;

namespace
{
    const std::string AUTOMATION_DEVICE = "automation in";
    const std::string SYNTH_DEVICE = "synth out";
    const std::string CUTOFF = "Cutoff";
    const std::string PAN = "Pan";
    const std::string SWITCH = "Switch";
    const std::string LEVEL = "Level";

    constexpr int CC_NUMBER = 74;
    constexpr int OTHER_CC_NUMBER = 20;
    constexpr int CC_MAX = 127;
    constexpr int TONE_CHANNEL = 5;
    constexpr int INCOMING_CHANNEL = 2;
    constexpr int OTHER_INCOMING_CHANNEL = 9;
    constexpr int PAN_MIN = -50;
    constexpr int PAN_MAX = 50;
    constexpr int TWO_STATE_THRESHOLD = 63; // a control change above it counts as "on"
    constexpr int NOTE = 60;
    constexpr int VELOCITY = 100;
    constexpr int SOME_CC_VALUE = 99;
    constexpr int MID_CC_VALUE = 64;
    constexpr std::uint8_t SYSEX_START = 0xF0;
    constexpr std::uint8_t SYSEX_END = 0xF7;

    // A parameter whose message is a tiny SysEx frame carrying its value; nothing here is sent.
    class TestParameter final : public AbstractParameter
    {
    public:
        TestParameter(std::string name, int minValue, int maxValue)
            : AbstractParameter(std::move(name), minValue, maxValue, 1, MidiMessage::sysEx(FRAME))
        {
            initializeValue(minValue);
        }

        [[nodiscard]] std::unique_ptr<AbstractParameter> clone() const override
        {
            return std::make_unique<TestParameter>(*this);
        }

    protected:
        void updateMessageFromValue() override
        {
            rawMessageBytes()[1] = static_cast<std::uint8_t>(rawValue() & 0x7F);
        }

    private:
        static constexpr std::array<std::uint8_t, 3> FRAME{SYSEX_START, 0x00, SYSEX_END};
    };

    class TestTone final : public AbstractTone
    {
    public:
        TestTone()
        {
            parameterMap().add(std::make_unique<TestParameter>(CUTOFF, 0, CC_MAX));
            parameterMap().add(std::make_unique<TestParameter>(PAN, PAN_MIN, PAN_MAX));
            parameterMap().add(std::make_unique<TestParameter>(SWITCH, 0, 1));
            parameterMap().add(std::make_unique<TestParameter>(LEVEL, 0, CC_MAX));
        }

        [[nodiscard]] std::string toneName() const override { return _name; }
        void setToneName(const std::string& name) override { _name = name; }

    private:
        std::string _name;
    };

    class TestController final : public AbstractController
    {
    public:
        explicit TestController(MockMidiBackend& backend)
            : AbstractController(backend, std::make_unique<TestTone>())
        {
        }

        [[nodiscard]] int currentProgramNumber() const override { return 0; }

        [[nodiscard]] std::vector<std::pair<std::string, std::unique_ptr<AbstractTone>>>
        extractSinglePatchesFromAllDataDumpFileToDirectory(const std::string&, const std::string&) override
        {
            return {};
        }
    };

    // A controller wired to the in-memory backend, with its automation input and synth output open, and a
    // handler that records what the UI is told.
    struct Rig
    {
        MockMidiBackend backend;
        TestController controller;
        std::vector<std::pair<std::string, int>> uiChanges;

        explicit Rig(bool withSynthOutput = true)
            : controller(withDevices(backend))
        {
            REQUIRE(controller.setAutomationInputDevice(AUTOMATION_DEVICE));
            if (withSynthOutput)
                REQUIRE(controller.setSynthOutputDevice(SYNTH_DEVICE));
            controller.setAutomationParameterChangeHandler(
                [this](const std::string& name, int value) { uiChanges.emplace_back(name, value); });
        }

        static MockMidiBackend& withDevices(MockMidiBackend& backend)
        {
            backend.addInputDevice(AUTOMATION_DEVICE);
            backend.addOutputDevice(SYNTH_DEVICE);
            return backend;
        }

        void controlChange(int channel, int number, int value)
        {
            backend.injectIncoming(AUTOMATION_DEVICE, MidiMessage::channelMessage(ChannelCommand::Controller, channel, number, value));
        }

        [[nodiscard]] int valueOf(const std::string& name) { return controller.getParameter(name)->value(); }

        [[nodiscard]] std::vector<MidiMessage> sent() const { return backend.sentMessages(SYNTH_DEVICE); }
    };

    // The reference's scaling of a control change value onto a parameter's range, written out here as the
    // behaviour to keep: an integer-truncated ratio of the range, shifted down by the minimum's magnitude.
    int scaled(int ccValue, int minValue, int maxValue)
    {
        const double interleave = maxValue + (minValue < 0 ? -minValue : minValue);
        const double ratio = ccValue / static_cast<double>(CC_MAX);
        return static_cast<int>(ratio * interleave) - (minValue < 0 ? -minValue : minValue);
    }
}

TEST_CASE("Given a control change mapped to a parameter of 0 to 127, When it arrives at the full value and at 0, Then the parameter takes its maximum and its minimum [RQ-FMW-050]",
          "[framework][controller]")
{
    Rig rig;
    rig.controller.controlChangeAutomationTable().add(CUTOFF, CC_NUMBER);

    rig.controlChange(INCOMING_CHANNEL, CC_NUMBER, CC_MAX);
    CHECK(rig.valueOf(CUTOFF) == CC_MAX);

    rig.controlChange(INCOMING_CHANNEL, CC_NUMBER, 0);
    CHECK(rig.valueOf(CUTOFF) == 0);
}

TEST_CASE("Given a control change mapped to a parameter of 0 to 127, When a middle value arrives, Then the parameter takes the reference's scaled value [RQ-FMW-050]",
          "[framework][controller]")
{
    Rig rig;
    rig.controller.controlChangeAutomationTable().add(CUTOFF, CC_NUMBER);

    rig.controlChange(INCOMING_CHANNEL, CC_NUMBER, MID_CC_VALUE);

    CHECK(rig.valueOf(CUTOFF) == scaled(MID_CC_VALUE, 0, CC_MAX));
}

TEST_CASE("Given a control change mapped to a parameter of -50 to 50, When 0 and the full value arrive, Then the parameter takes -50 and 50 [RQ-FMW-050]",
          "[framework][controller]")
{
    Rig rig;
    rig.controller.controlChangeAutomationTable().add(PAN, CC_NUMBER);

    rig.controlChange(INCOMING_CHANNEL, CC_NUMBER, 0);
    CHECK(rig.valueOf(PAN) == PAN_MIN);

    rig.controlChange(INCOMING_CHANNEL, CC_NUMBER, CC_MAX);
    CHECK(rig.valueOf(PAN) == PAN_MAX);
}

TEST_CASE("Given a control change mapped to a two-state parameter, When a value above 63 and one of 63 arrive, Then the parameter is 1 and then 0 [RQ-FMW-050]",
          "[framework][controller]")
{
    Rig rig;
    rig.controller.controlChangeAutomationTable().add(SWITCH, CC_NUMBER);

    rig.controlChange(INCOMING_CHANNEL, CC_NUMBER, TWO_STATE_THRESHOLD + 1);
    CHECK(rig.valueOf(SWITCH) == 1);

    rig.controlChange(INCOMING_CHANNEL, CC_NUMBER, TWO_STATE_THRESHOLD);
    CHECK(rig.valueOf(SWITCH) == 0);
}

TEST_CASE("Given a mapped control change, When it arrives, Then the UI is told the parameter's name and new value, and nothing is sent to the synth [RQ-FMW-050, RQ-FMW-060]",
          "[framework][controller]")
{
    Rig rig;
    rig.controller.controlChangeAutomationTable().add(CUTOFF, CC_NUMBER);

    rig.controlChange(INCOMING_CHANNEL, CC_NUMBER, CC_MAX);

    REQUIRE(rig.uiChanges.size() == 1);
    CHECK(rig.uiChanges[0].first == CUTOFF);
    CHECK(rig.uiChanges[0].second == CC_MAX);
    CHECK(rig.sent().empty());
}

TEST_CASE("Given a control change mapped to two parameters, When it arrives, Then both are set and the UI is told of each in the order they were mapped [RQ-FMW-032, RQ-FMW-050]",
          "[framework][controller]")
{
    Rig rig;
    rig.controller.controlChangeAutomationTable().add(CUTOFF, CC_NUMBER);
    rig.controller.controlChangeAutomationTable().add(LEVEL, CC_NUMBER);

    rig.controlChange(INCOMING_CHANNEL, CC_NUMBER, CC_MAX);

    CHECK(rig.valueOf(CUTOFF) == CC_MAX);
    CHECK(rig.valueOf(LEVEL) == CC_MAX);
    REQUIRE(rig.uiChanges.size() == 2);
    CHECK(rig.uiChanges[0].first == CUTOFF);
    CHECK(rig.uiChanges[1].first == LEVEL);
}

TEST_CASE("Given a mapped control change, When it arrives on a different channel, Then it applies all the same [RQ-FMW-050]",
          "[framework][controller]")
{
    Rig rig;
    rig.controller.controlChangeAutomationTable().add(CUTOFF, CC_NUMBER);

    rig.controlChange(OTHER_INCOMING_CHANNEL, CC_NUMBER, CC_MAX);

    CHECK(rig.valueOf(CUTOFF) == CC_MAX);
}

TEST_CASE("Given the disabled control change number, When that control change arrives, Then nothing is set, nothing is told to the UI and nothing is forwarded [RQ-FMW-033]",
          "[framework][controller]")
{
    Rig rig;
    rig.controller.controlChangeAutomationTable().add(CUTOFF, CC_NUMBER);
    rig.controller.setDisabledControlChangeNumber(CC_NUMBER);

    rig.controlChange(INCOMING_CHANNEL, CC_NUMBER, CC_MAX);

    CHECK(rig.valueOf(CUTOFF) == 0);
    CHECK(rig.uiChanges.empty());
    CHECK(rig.sent().empty());
}

TEST_CASE("Given a control change that no parameter is mapped to, When it arrives, Then it is forwarded to the synth on the tone's channel and the UI is not told [RQ-FMW-051]",
          "[framework][controller]")
{
    Rig rig;
    rig.controller.setMidiChannel(TONE_CHANNEL);

    rig.controlChange(INCOMING_CHANNEL, OTHER_CC_NUMBER, SOME_CC_VALUE);

    const auto sent = rig.sent();
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].command() == ChannelCommand::Controller);
    CHECK(sent[0].channel() == TONE_CHANNEL);
    CHECK(sent[0].data1() == OTHER_CC_NUMBER);
    CHECK(sent[0].data2() == SOME_CC_VALUE);
    CHECK(rig.uiChanges.empty());
}

TEST_CASE("Given a channel message that is not a control change, When it arrives, Then it is forwarded to the synth on the tone's channel [RQ-FMW-051]",
          "[framework][controller]")
{
    Rig rig;
    rig.controller.setMidiChannel(TONE_CHANNEL);

    rig.backend.injectIncoming(AUTOMATION_DEVICE, MidiMessage::channelMessage(ChannelCommand::NoteOn, INCOMING_CHANNEL, NOTE, VELOCITY));

    const auto sent = rig.sent();
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].command() == ChannelCommand::NoteOn);
    CHECK(sent[0].channel() == TONE_CHANNEL);
    CHECK(sent[0].data1() == NOTE);
    CHECK(sent[0].data2() == VELOCITY);
}

TEST_CASE("Given a SysEx message from the automation device, When it arrives, Then it is forwarded to the synth unchanged [RQ-FMW-052]",
          "[framework][controller]")
{
    Rig rig;
    const std::vector<std::uint8_t> frame{SYSEX_START, 0x01, 0x02, 0x03, SYSEX_END};

    rig.backend.injectIncoming(AUTOMATION_DEVICE, MidiMessage::sysEx(frame));

    const auto sent = rig.sent();
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].toBytes() == frame);
}

TEST_CASE("Given no synth output device, When a message that is to be forwarded arrives, Then it is dropped without an error [RQ-FMW-051]",
          "[framework][controller]")
{
    Rig rig(false);

    CHECK_NOTHROW(rig.controlChange(INCOMING_CHANNEL, OTHER_CC_NUMBER, SOME_CC_VALUE));
    CHECK(rig.sent().empty());
}
