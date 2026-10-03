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

// The front panel primitives of section §20 (spec Tables 30-31): on a session and the simulated sampler. Section
// §20 has no Get and no REPLY, every item completes on DONE, which only means "queued" (Table 30, note a): the
// simulated sampler's record of what it received is what proves a primitive. This file grows with each task of
// PLAN-AKM-008 — for now the keys, Hold and Release (&01, &02). Real-sampler verification is TASK-AKM-073's.
// [TASK-AKM-070, RQ-AKM-073, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/FrontPanel.hpp"
#include "akm/SamplerError.hpp"

using akm::CommandResult;
using akm::Done;
using akm::Error;
using akm::FrontPanelKey;
using akm::KeyPressResult;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::FrontPanelEvent;
using akm::harness::FrontPanelEventKind;
using akm::harness::ManualScenarioDriver;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;
using akm::test::bytes;

namespace
{
    // Section §20 and its two key items, spec Table 30.
    constexpr std::uint8_t SECTION_FRONT_PANEL = 0x20;
    constexpr std::uint8_t ITEM_KEY_HOLD = 0x01;
    constexpr std::uint8_t ITEM_KEY_RELEASE = 0x02;
    constexpr std::size_t DATA_START = akm::test::SENT_ITEM_INDEX + 1;

    // Table 31, written out from the spec (not from the primitive under test): the 43 keys and their keycodes.
    struct SpecKey
    {
        FrontPanelKey key;
        std::uint8_t code;
    };
    constexpr std::array<SpecKey, 43> TABLE_31{{
        {FrontPanelKey::Multi, 0x44},       {FrontPanelKey::Fx, 0x40},          {FrontPanelKey::EditSample, 0x42},
        {FrontPanelKey::EditProgram, 0x43}, {FrontPanelKey::Record, 0x41},      {FrontPanelKey::Utilities, 0x45},
        {FrontPanelKey::Save, 0x46},        {FrontPanelKey::Load, 0x47},        {FrontPanelKey::F1, 0x48},
        {FrontPanelKey::F2, 0x49},          {FrontPanelKey::F3, 0x4A},          {FrontPanelKey::F4, 0x4B},
        {FrontPanelKey::F5, 0x4C},          {FrontPanelKey::F6, 0x4D},          {FrontPanelKey::F7, 0x4E},
        {FrontPanelKey::F8, 0x4F},          {FrontPanelKey::F9, 0x50},          {FrontPanelKey::F10, 0x51},
        {FrontPanelKey::F11, 0x52},         {FrontPanelKey::F12, 0x53},         {FrontPanelKey::F13, 0x54},
        {FrontPanelKey::F14, 0x55},         {FrontPanelKey::F15, 0x56},         {FrontPanelKey::F16, 0x57},
        {FrontPanelKey::Digit0, 0x58},      {FrontPanelKey::Digit1, 0x59},      {FrontPanelKey::Digit2, 0x5A},
        {FrontPanelKey::Digit3, 0x5B},      {FrontPanelKey::Digit4, 0x5C},      {FrontPanelKey::Digit5, 0x5D},
        {FrontPanelKey::Digit6, 0x5E},      {FrontPanelKey::Digit7, 0x5F},      {FrontPanelKey::Digit8, 0x60},
        {FrontPanelKey::Digit9, 0x61},      {FrontPanelKey::Minus, 0x62},       {FrontPanelKey::Plus, 0x63},
        {FrontPanelKey::CursorLeft, 0x64},  {FrontPanelKey::CursorRight, 0x65}, {FrontPanelKey::Window, 0x67},
        {FrontPanelKey::Mark, 0x68},        {FrontPanelKey::Jump, 0x69},        {FrontPanelKey::Exit, 0x6A},
        {FrontPanelKey::EntPlay, 0x6B},
    }};

    constexpr std::uint8_t EXIT_CODE = 0x6A;
    // Inside the item's range 64-107 but listed by no row of Table 31, and one just below and just above it.
    constexpr int UNLISTED_INSIDE_RANGE = 0x66;
    constexpr int BELOW_RANGE = 0x3F;
    constexpr int ABOVE_RANGE = 0x6C;
    constexpr int DATA_BYTE_VALUES = 128;

    FrontPanelKey keyOf(int code)
    {
        return static_cast<FrontPanelKey>(code);
    }

    // Runs `ask`, which starts a primitive with its completion, and returns the result it reports.
    template <typename Result, typename Ask>
    Result await(SessionHarness& harness, Ask ask)
    {
        auto latched = std::make_shared<Latched<Result>>();
        ask([latched](const Result& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    Bytes dataOf(const Bytes& frame)
    {
        return Bytes(frame.begin() + DATA_START, frame.end() - 1);
    }
}

TEST_CASE("Given a simulated sampler, When the key EXIT is held then released, Then the frames carry 20 01 6A then 20 02 6A, each completes on DONE and the sampler records the key down then up [RQ-AKM-073]",
          "[akm][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const std::size_t sentBefore = harness.sentCount();

    akm::holdKey(harness.session(), FrontPanelKey::Exit, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(harness.sampler().frontPanel().keysDown == std::vector<std::uint8_t>{EXIT_CODE});

    akm::releaseKey(harness.session(), FrontPanelKey::Exit, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    const std::vector<Bytes> frames = harness.sentFrames();
    REQUIRE(frames.size() == sentBefore + 2);
    const Bytes& hold = frames[sentBefore];
    const Bytes& release = frames[sentBefore + 1];
    CHECK(hold.at(akm::test::SENT_SECTION_INDEX) == SECTION_FRONT_PANEL);
    CHECK(hold.at(akm::test::SENT_ITEM_INDEX) == ITEM_KEY_HOLD);
    CHECK(dataOf(hold) == bytes({EXIT_CODE}));
    CHECK(release.at(akm::test::SENT_SECTION_INDEX) == SECTION_FRONT_PANEL);
    CHECK(release.at(akm::test::SENT_ITEM_INDEX) == ITEM_KEY_RELEASE);
    CHECK(dataOf(release) == bytes({EXIT_CODE}));

    const std::vector<FrontPanelEvent> events = harness.sampler().frontPanel().events;
    REQUIRE(events.size() == 2);
    CHECK(events[0].kind == FrontPanelEventKind::KeyHold);
    CHECK(events[0].first == EXIT_CODE);
    CHECK(events[1].kind == FrontPanelEventKind::KeyRelease);
    CHECK(events[1].first == EXIT_CODE);
    CHECK(harness.sampler().frontPanel().keysDown.empty());
}

TEST_CASE("Given each of the 43 keycodes of Table 31, When the key is held then released, Then the byte on the wire is its spec keycode and the sampler records that code [RQ-AKM-073]",
          "[akm][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    std::size_t completed = 0;
    for (const SpecKey& entry : TABLE_31)
    {
        CHECK(static_cast<std::uint8_t>(entry.key) == entry.code);

        const std::size_t sentBefore = harness.sentCount();
        akm::holdKey(harness.session(), entry.key, harness.recorder().completion());
        akm::releaseKey(harness.session(), entry.key, harness.recorder().completion());
        completed += 2;
        REQUIRE(harness.waitForCompletions(completed));

        const std::vector<Bytes> frames = harness.sentFrames();
        REQUIRE(frames.size() == sentBefore + 2);
        CHECK(dataOf(frames[sentBefore]) == bytes({entry.code}));
        CHECK(dataOf(frames[sentBefore + 1]) == bytes({entry.code}));
    }

    const std::vector<FrontPanelEvent> events = harness.sampler().frontPanel().events;
    REQUIRE(events.size() == TABLE_31.size() * 2);
    for (std::size_t index = 0; index < TABLE_31.size(); ++index)
    {
        CHECK(events[index * 2].kind == FrontPanelEventKind::KeyHold);
        CHECK(events[index * 2].first == TABLE_31[index].code);
        CHECK(events[index * 2 + 1].kind == FrontPanelEventKind::KeyRelease);
        CHECK(events[index * 2 + 1].first == TABLE_31[index].code);
    }
}

TEST_CASE("Given a code that no row of Table 31 lists, When it is converted, held or released, Then it is no key, and it is refused as ArgumentOutOfRange without sending [RQ-AKM-073]",
          "[akm][front-panel]")
{
    for (int code = 0; code < DATA_BYTE_VALUES; ++code)
    {
        const bool listed = std::any_of(TABLE_31.begin(), TABLE_31.end(), [code](const SpecKey& entry) {
            return entry.code == code;
        });
        CHECK(akm::frontPanelKeyFromCode(code).has_value() == listed);
    }
    CHECK(!akm::frontPanelKeyFromCode(-1).has_value());
    CHECK(!akm::frontPanelKeyFromCode(DATA_BYTE_VALUES).has_value());

    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const std::size_t sentBefore = harness.sentCount();

    std::size_t expected = 0;
    for (const int code : {UNLISTED_INSIDE_RANGE, BELOW_RANGE, ABOVE_RANGE})
    {
        akm::holdKey(harness.session(), keyOf(code), harness.recorder().completion());
        akm::releaseKey(harness.session(), keyOf(code), harness.recorder().completion());
        expected += 2;
    }
    REQUIRE(harness.waitForCompletions(expected));

    for (const CommandResult& result : harness.recorder().results())
    {
        REQUIRE(std::holds_alternative<Refused>(result));
        CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
    }
    CHECK(harness.sentCount() == sentBefore);
    CHECK(harness.sampler().frontPanel().events.empty());
}

TEST_CASE("Given a simulated sampler, When a key is pressed, Then a Hold then a Release of that key are sent, the press completes once with both results, and no key is left down [RQ-AKM-073]",
          "[akm][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const std::size_t sentBefore = harness.sentCount();

    const KeyPressResult result = await<KeyPressResult>(
        harness, [&](auto done) { akm::pressKey(harness.session(), FrontPanelKey::F3, done); });

    CHECK(std::holds_alternative<Done>(result.hold));
    CHECK(std::holds_alternative<Done>(result.release));

    const std::vector<Bytes> frames = harness.sentFrames();
    REQUIRE(frames.size() == sentBefore + 2);
    CHECK(frames[sentBefore].at(akm::test::SENT_ITEM_INDEX) == ITEM_KEY_HOLD);
    CHECK(frames[sentBefore + 1].at(akm::test::SENT_ITEM_INDEX) == ITEM_KEY_RELEASE);
    CHECK(harness.sampler().frontPanel().keysDown.empty());
    CHECK(harness.sampler().frontPanel().events.size() == 2);
}

TEST_CASE("Given a sampler that answers the Hold with an ERROR, When a key is pressed, Then the Release is sent all the same and both results are reported [RQ-AKM-073]",
          "[akm][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    akm::harness::SamplerBehaviour behaviour = harness.sampler().behaviour();
    behaviour.itemErrors.push_back({SECTION_FRONT_PANEL, ITEM_KEY_HOLD, akm::error_number::UNKNOWN_ERROR});
    harness.sampler().setBehaviour(behaviour);
    const std::size_t sentBefore = harness.sentCount();

    const KeyPressResult result = await<KeyPressResult>(
        harness, [&](auto done) { akm::pressKey(harness.session(), FrontPanelKey::Exit, done); });

    REQUIRE(std::holds_alternative<Error>(result.hold));
    CHECK(std::get<Error>(result.hold).number == akm::error_number::UNKNOWN_ERROR);
    CHECK(std::holds_alternative<Done>(result.release));

    const std::vector<Bytes> frames = harness.sentFrames();
    REQUIRE(frames.size() == sentBefore + 2);
    CHECK(frames[sentBefore + 1].at(akm::test::SENT_ITEM_INDEX) == ITEM_KEY_RELEASE);
    CHECK(dataOf(frames[sentBefore + 1]) == bytes({EXIT_CODE}));
}

TEST_CASE("Given a value that is not a key of Table 31, When it is pressed, Then both the Hold and the Release are refused and nothing is sent [RQ-AKM-073]",
          "[akm][front-panel]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const std::size_t sentBefore = harness.sentCount();

    const KeyPressResult result = await<KeyPressResult>(
        harness, [&](auto done) { akm::pressKey(harness.session(), keyOf(UNLISTED_INSIDE_RANGE), done); });

    REQUIRE(std::holds_alternative<Refused>(result.hold));
    CHECK(std::get<Refused>(result.hold).reason == RefusalReason::ArgumentOutOfRange);
    REQUIRE(std::holds_alternative<Refused>(result.release));
    CHECK(std::get<Refused>(result.release).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == sentBefore);
}
