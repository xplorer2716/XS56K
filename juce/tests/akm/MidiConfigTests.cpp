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

// The MIDI configuration primitives of section §04 (spec Table 8): on a session and the simulated sampler. Section
// §04 has no Get and no REPLY, every item completes on DONE: the simulated sampler's record of what it received is
// what proves a primitive. This file grows with each task of PLAN-AKM-009 — for now the five switches (&01 to &05).
// Real-sampler verification is TASK-AKM-079's.
// [TASK-AKM-077, RQ-AKM-078, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/MidiConfig.hpp"
#include "akm/SamplerError.hpp"

using akm::AftertouchType;
using akm::CommandResult;
using akm::Done;
using akm::Error;
using akm::MultiSelectMode;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::MidiConfigEvent;
using akm::harness::ManualScenarioDriver;
using akm::test::Bytes;
using akm::test::SessionHarness;
using akm::test::bytes;

namespace
{
    // Section §04 and its five switch items, spec Table 8.
    constexpr std::uint8_t SECTION_MIDI_CONFIG = 0x04;
    constexpr std::uint8_t ITEM_PROGRAM_CHANGE = 0x01;
    constexpr std::uint8_t ITEM_MULTI_SELECT = 0x02;
    constexpr std::uint8_t ITEM_MULTI_SELECT_CHANNEL = 0x03;
    constexpr std::uint8_t ITEM_EXTERNAL_APM = 0x04;
    constexpr std::uint8_t ITEM_AFTERTOUCH = 0x05;
    constexpr std::size_t DATA_START = akm::test::SENT_ITEM_INDEX + 1;

    // The ranges of Table 8 and the values just outside them.
    constexpr int MULTI_SELECT_MODE_BEYOND_RANGE = 3;
    constexpr int AFTERTOUCH_BEYOND_RANGE = 2;
    constexpr int CHANNEL_FIRST = 0;
    constexpr int CHANNEL_LAST = 31;
    constexpr int CHANNEL_BEYOND_RANGE = 32;
    constexpr int CONTROLLER_FIRST = 0;
    constexpr int CONTROLLER_LAST = 127;
    constexpr int CONTROLLER_BEYOND_RANGE = 128;
    constexpr int BELOW_RANGE = -1;
    constexpr std::int64_t SWITCH_BEYOND_RANGE = 2;

    MultiSelectMode multiSelectOf(int value)
    {
        return static_cast<MultiSelectMode>(value);
    }

    AftertouchType aftertouchOf(int value)
    {
        return static_cast<AftertouchType>(value);
    }

    Bytes dataOf(const Bytes& frame)
    {
        return Bytes(frame.begin() + DATA_START, frame.end() - 1);
    }

    // The data bytes of the one frame sent since `sentBefore`, after checking it is the §04 item `item`.
    Bytes sentDataOf(SessionHarness& harness, std::size_t sentBefore, std::uint8_t item)
    {
        const std::vector<Bytes> frames = harness.sentFrames();
        REQUIRE(frames.size() == sentBefore + 1);
        CHECK(frames[sentBefore].at(akm::test::SENT_SECTION_INDEX) == SECTION_MIDI_CONFIG);
        CHECK(frames[sentBefore].at(akm::test::SENT_ITEM_INDEX) == item);
        return dataOf(frames[sentBefore]);
    }
}

TEST_CASE("Given a simulated sampler, When multi select is set to BANK, Then the frame carries 04 02 02, it completes on DONE and the sampler records multi select 2 [RQ-AKM-078]",
          "[akm][midi-config]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const std::size_t sentBefore = harness.sentCount();

    akm::setMultiSelect(harness.session(), MultiSelectMode::Bank, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));

    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(sentDataOf(harness, sentBefore, ITEM_MULTI_SELECT) == bytes({2}));
    CHECK(harness.sampler().midiConfig().multiSelect == 2);
    CHECK(harness.sampler().midiConfig().events == std::vector<MidiConfigEvent>{{ITEM_MULTI_SELECT, 2, 0}});
}

TEST_CASE("Given each switch at both ends of its range, When it is set, Then each goes out as its own item and byte and the sampler records it [RQ-AKM-078]",
          "[akm][midi-config]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    struct Step
    {
        std::uint8_t item;
        std::uint8_t value;
    };
    std::vector<Step> expected;
    std::size_t completed = 0;
    const auto submit = [&](std::uint8_t item, std::uint8_t value, auto send) {
        send();
        expected.push_back({item, value});
        REQUIRE(harness.waitForCompletions(++completed));
        CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    };
    auto completion = [&] { return harness.recorder().completion(); };
    auto& session = harness.session();

    submit(ITEM_PROGRAM_CHANGE, 0, [&] { akm::setProgramChangeEnabled(session, false, completion()); });
    submit(ITEM_PROGRAM_CHANGE, 1, [&] { akm::setProgramChangeEnabled(session, true, completion()); });
    submit(ITEM_MULTI_SELECT, 0, [&] { akm::setMultiSelect(session, MultiSelectMode::Off, completion()); });
    submit(ITEM_MULTI_SELECT, 1, [&] { akm::setMultiSelect(session, MultiSelectMode::ProgramChange, completion()); });
    submit(ITEM_MULTI_SELECT, 2, [&] { akm::setMultiSelect(session, MultiSelectMode::Bank, completion()); });
    submit(ITEM_MULTI_SELECT_CHANNEL, CHANNEL_FIRST, [&] { akm::setMultiSelectChannel(session, CHANNEL_FIRST, completion()); });
    submit(ITEM_MULTI_SELECT_CHANNEL, CHANNEL_LAST, [&] { akm::setMultiSelectChannel(session, CHANNEL_LAST, completion()); });
    submit(ITEM_EXTERNAL_APM, CONTROLLER_FIRST, [&] { akm::setExternalApmController(session, CONTROLLER_FIRST, completion()); });
    submit(ITEM_EXTERNAL_APM, CONTROLLER_LAST, [&] { akm::setExternalApmController(session, CONTROLLER_LAST, completion()); });
    submit(ITEM_AFTERTOUCH, 0, [&] { akm::setAftertouch(session, AftertouchType::Channel, completion()); });
    submit(ITEM_AFTERTOUCH, 1, [&] { akm::setAftertouch(session, AftertouchType::Polyphonic, completion()); });

    const std::vector<Bytes> frames = harness.sentFrames();
    REQUIRE(frames.size() >= expected.size());
    const std::size_t firstFrame = frames.size() - expected.size();
    const std::vector<MidiConfigEvent> events = harness.sampler().midiConfig().events;
    REQUIRE(events.size() == expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        const Bytes& frame = frames[firstFrame + index];
        CHECK(frame.at(akm::test::SENT_SECTION_INDEX) == SECTION_MIDI_CONFIG);
        CHECK(frame.at(akm::test::SENT_ITEM_INDEX) == expected[index].item);
        CHECK(dataOf(frame) == bytes({expected[index].value}));
        CHECK(events[index] == MidiConfigEvent{expected[index].item, expected[index].value, 0});
    }

    // The sampler ends holding the last value of each switch.
    const akm::harness::MidiConfigState state = harness.sampler().midiConfig();
    CHECK(state.programChangeEnable == 1);
    CHECK(state.multiSelect == 2);
    CHECK(state.multiSelectChannel == CHANNEL_LAST);
    CHECK(state.externalApmController == CONTROLLER_LAST);
    CHECK(state.aftertouch == 1);
}

TEST_CASE("Given a value outside the range of a switch, When it is sent, Then nothing is sent and it is refused as ArgumentOutOfRange [RQ-AKM-078]",
          "[akm][midi-config]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const std::size_t sentBefore = harness.sentCount();
    auto completion = [&] { return harness.recorder().completion(); };
    auto& session = harness.session();

    akm::setMultiSelect(session, multiSelectOf(MULTI_SELECT_MODE_BEYOND_RANGE), completion());
    akm::setMultiSelectChannel(session, CHANNEL_BEYOND_RANGE, completion());
    akm::setMultiSelectChannel(session, BELOW_RANGE, completion());
    akm::setExternalApmController(session, CONTROLLER_BEYOND_RANGE, completion());
    akm::setExternalApmController(session, BELOW_RANGE, completion());
    akm::setAftertouch(session, aftertouchOf(AFTERTOUCH_BEYOND_RANGE), completion());
    constexpr std::size_t REFUSED_COUNT = 6;
    REQUIRE(harness.waitForCompletions(REFUSED_COUNT));

    for (const CommandResult& result : harness.recorder().results())
    {
        REQUIRE(std::holds_alternative<Refused>(result));
        CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
    }
    CHECK(harness.sentCount() == sentBefore);
    CHECK(harness.sampler().midiConfig().events.empty());
}

TEST_CASE("Given a program change enable that is neither 0 nor 1, When the request is built, Then the catalogue refuses it as ArgumentOutOfRange [RQ-AKM-078]",
          "[akm][midi-config]")
{
    // `setProgramChangeEnabled` takes a bool, so the refusal is the catalogue's own, as for the §00 toggles.
    const akm::CommandRequest request = akm::makeRequest(akm::ItemId::MidiProgramChangeEnable, {SWITCH_BEYOND_RANGE});

    REQUIRE(request.refusal.has_value());
    CHECK(*request.refusal == RefusalReason::ArgumentOutOfRange);
}

TEST_CASE("Given a sampler that answers the multi select with an ERROR, When it is set, Then the error is reported and the sampler keeps its previous value [RQ-AKM-078]",
          "[akm][midi-config]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    akm::harness::SamplerBehaviour behaviour = harness.sampler().behaviour();
    behaviour.itemErrors.push_back({SECTION_MIDI_CONFIG, ITEM_MULTI_SELECT, akm::error_number::UNKNOWN_ERROR});
    harness.sampler().setBehaviour(behaviour);

    akm::setMultiSelect(harness.session(), MultiSelectMode::ProgramChange, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));

    REQUIRE(std::holds_alternative<Error>(harness.recorder().results().back()));
    CHECK(std::get<Error>(harness.recorder().results().back()).number == akm::error_number::UNKNOWN_ERROR);
    CHECK(harness.sampler().midiConfig().multiSelect == 0);
}
