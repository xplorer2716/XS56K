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

// The owner-driven check of the front panel (`xs56k_akm_probe --suite --front-panel`): the mapping of PC keys to
// sampler keys, and the check itself run against the simulated sampler with a scripted source of PC keys. The real
// run is the owner's: the sampler reacts to what the owner presses, on a screen the owner chose. [TASK-AKM-073,
// RQ-AKM-076, RQ-AKM-075, RQ-AKM-018, ADR-AKM-001 (DEC-AKM-008, DEC-AKM-019)]
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <string>
#include <utility>
#include <vector>

#include "akm/FrontPanel.hpp"
#include "akm/harness/FrontPanelRemote.hpp"
#include "akm/harness/RealSamplerSuite.hpp"
#include "akm/harness/ScenarioDriver.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"

using akm::DataWheelDirection;
using akm::FrontPanelKey;
using akm::harness::CheckOutcome;
using akm::harness::FrontPanelEvent;
using akm::harness::FrontPanelEventKind;
using akm::harness::ManualScenarioDriver;
using akm::harness::RealSuiteOptions;
using akm::harness::RealSuiteResult;
using akm::harness::RemoteAction;
using akm::harness::RemoteActionKind;
using akm::harness::ScenarioTarget;
using akm::harness::SimulatedMidiBackend;
using akm::harness::SimulatedSampler;
using akm::harness::remoteAction;
namespace pc_key = akm::harness::pc_key;

namespace
{
    constexpr std::size_t AUTOMATIC_CHECKS = 7;
    constexpr int WHEEL_ONE_CLICK = 1;
    constexpr int WHEEL_PAGE_CLICKS = 8;
    constexpr int ASCII_BACKSPACE = 8;
    constexpr int ASCII_ENTER = 13;
    constexpr int ASCII_SPACE = 32;
    constexpr int ASCII_TILDE = 126;
    constexpr int ASCII_DELETE = 127;

    // The approved mapping of normal mode, one row per PC key: what it stands for (TASK-AKM-073, the owner's choice).
    struct PressRow
    {
        int pcKey;
        FrontPanelKey key;
    };
    const std::vector<PressRow> PRESS_ROWS{
        {pc_key::F1, FrontPanelKey::F1},        {pc_key::F1 + 1, FrontPanelKey::F2},
        {pc_key::F1 + 2, FrontPanelKey::F3},    {pc_key::F1 + 3, FrontPanelKey::F4},
        {pc_key::F1 + 4, FrontPanelKey::F5},    {pc_key::F1 + 5, FrontPanelKey::F6},
        {pc_key::F1 + 6, FrontPanelKey::F7},    {pc_key::F1 + 7, FrontPanelKey::F8},
        {'0', FrontPanelKey::Digit0},           {'1', FrontPanelKey::Digit1},
        {'2', FrontPanelKey::Digit2},           {'3', FrontPanelKey::Digit3},
        {'4', FrontPanelKey::Digit4},           {'5', FrontPanelKey::Digit5},
        {'6', FrontPanelKey::Digit6},           {'7', FrontPanelKey::Digit7},
        {'8', FrontPanelKey::Digit8},           {'9', FrontPanelKey::Digit9},
        {'-', FrontPanelKey::Minus},            {'+', FrontPanelKey::Plus},
        {'=', FrontPanelKey::Plus},             {pc_key::LEFT, FrontPanelKey::CursorLeft},
        {pc_key::RIGHT, FrontPanelKey::CursorRight}, {pc_key::ENTER, FrontPanelKey::EntPlay},
        {pc_key::ESCAPE, FrontPanelKey::Exit},  {'m', FrontPanelKey::Multi},
        {'x', FrontPanelKey::Fx},               {'s', FrontPanelKey::EditSample},
        {'p', FrontPanelKey::EditProgram},      {'r', FrontPanelKey::Record},
        {'u', FrontPanelKey::Utilities},        {'v', FrontPanelKey::Save},
        {'l', FrontPanelKey::Load},             {'w', FrontPanelKey::Window},
        {'k', FrontPanelKey::Mark},             {'j', FrontPanelKey::Jump},
    };

    // A scripted source of PC keys: each call gives the next key, then nothing (the owner's input ends).
    std::function<std::optional<int>()> scripted(std::vector<int> keys)
    {
        auto remaining = std::make_shared<std::vector<int>>(std::move(keys));
        auto next = std::make_shared<std::size_t>(0);
        return [remaining, next]() -> std::optional<int> {
            if (*next >= remaining->size())
                return std::nullopt;
            return (*remaining)[(*next)++];
        };
    }

    struct Rig
    {
        Rig() : sampler(backend.addSampler({})) {}

        [[nodiscard]] RealSuiteOptions options() const
        {
            RealSuiteOptions suite;
            suite.target = ScenarioTarget{backend.inputName(), backend.outputName(), 0};
            suite.frontPanel = true;
            suite.askOwner = [](const std::string&) { return true; };
            return suite;
        }

        RealSuiteResult run(const RealSuiteOptions& options)
        {
            std::ostringstream log;
            return akm::harness::runRealSamplerSuite(backend, driver, options, log);
        }

        ManualScenarioDriver driver;
        SimulatedMidiBackend backend{driver.scheduler()};
        SimulatedSampler& sampler;
    };

    // Every §20 event the sampler recorded, as (kind, first, second).
    struct Seen
    {
        FrontPanelEventKind kind;
        std::uint8_t first;
        std::uint8_t second = 0;

        friend bool operator==(const Seen&, const Seen&) = default;
    };
    std::vector<Seen> seen(const SimulatedSampler& sampler)
    {
        std::vector<Seen> events;
        for (const FrontPanelEvent& event : sampler.frontPanel().events)
            events.push_back({event.kind, event.first, event.second});
        return events;
    }

    constexpr std::uint8_t code(FrontPanelKey key)
    {
        return static_cast<std::uint8_t>(key);
    }

    const akm::harness::CheckReport& remoteCheck(const RealSuiteResult& result)
    {
        REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + 1);
        return result.checks.back();
    }
}

TEST_CASE("Given the approved mapping, When each PC key of normal mode is looked up, Then it stands for its sampler key as a short press [RQ-AKM-076]",
          "[akm][front-panel][remote]")
{
    for (const PressRow& row : PRESS_ROWS)
    {
        const RemoteAction action = remoteAction(false, row.pcKey);
        CAPTURE(row.pcKey);
        CHECK(action.kind == RemoteActionKind::Press);
        CHECK(action.key == row.key);
    }
}

TEST_CASE("Given a letter of the mapping in upper case, When it is looked up, Then it stands for the same key as in lower case [RQ-AKM-076]",
          "[akm][front-panel][remote]")
{
    CHECK(remoteAction(false, 'M').kind == RemoteActionKind::Press);
    CHECK(remoteAction(false, 'M').key == FrontPanelKey::Multi);
    CHECK(remoteAction(false, 'Q').kind == RemoteActionKind::End);
}

TEST_CASE("Given the wheel keys, When they are looked up, Then the arrows turn it one click and the page keys eight, forwards for up [RQ-AKM-076]",
          "[akm][front-panel][remote]")
{
    const auto wheel = [](int pcKey, DataWheelDirection direction, int clicks) {
        const RemoteAction action = remoteAction(false, pcKey);
        CAPTURE(pcKey);
        CHECK(action.kind == RemoteActionKind::Wheel);
        CHECK(action.direction == direction);
        CHECK(action.clicks == clicks);
    };
    wheel(pc_key::UP, DataWheelDirection::Forwards, WHEEL_ONE_CLICK);
    wheel(pc_key::DOWN, DataWheelDirection::Backwards, WHEEL_ONE_CLICK);
    wheel(pc_key::PAGE_UP, DataWheelDirection::Forwards, WHEEL_PAGE_CLICKS);
    wheel(pc_key::PAGE_DOWN, DataWheelDirection::Backwards, WHEEL_PAGE_CLICKS);
}

TEST_CASE("Given Space, Tab and q, When they are looked up in normal mode, Then they toggle the hold of ENT/PLAY, toggle the text mode and end the check [RQ-AKM-076]",
          "[akm][front-panel][remote]")
{
    const RemoteAction hold = remoteAction(false, pc_key::SPACE);
    CHECK(hold.kind == RemoteActionKind::ToggleHold);
    CHECK(hold.key == FrontPanelKey::EntPlay);
    CHECK(remoteAction(false, pc_key::TAB).kind == RemoteActionKind::ToggleTextMode);
    CHECK(remoteAction(false, 'q').kind == RemoteActionKind::End);
}

TEST_CASE("Given a PC key the mapping does not give, When it is looked up, Then it stands for nothing [RQ-AKM-076]",
          "[akm][front-panel][remote]")
{
    for (const int pcKey : {static_cast<int>('z'), pc_key::F1 + 8, pc_key::F1 + 11, static_cast<int>('!'), 0, 1})
    {
        CAPTURE(pcKey);
        CHECK(remoteAction(false, pcKey).kind == RemoteActionKind::None);
    }
}

TEST_CASE("Given the text mode, When printable keys, Backspace and Enter are looked up, Then they are ASCII, and Tab and Escape leave the mode [RQ-AKM-076]",
          "[akm][front-panel][remote]")
{
    for (int character = ASCII_SPACE; character <= ASCII_TILDE; ++character)
    {
        const RemoteAction action = remoteAction(true, character);
        CAPTURE(character);
        CHECK(action.kind == RemoteActionKind::Ascii);
        CHECK(action.ascii == character);
    }
    CHECK(remoteAction(true, pc_key::BACKSPACE).kind == RemoteActionKind::Ascii);
    CHECK(remoteAction(true, pc_key::BACKSPACE).ascii == ASCII_BACKSPACE);
    CHECK(remoteAction(true, pc_key::ENTER).kind == RemoteActionKind::Ascii);
    CHECK(remoteAction(true, pc_key::ENTER).ascii == ASCII_ENTER);
    CHECK(remoteAction(true, pc_key::TAB).kind == RemoteActionKind::LeaveTextMode);
    CHECK(remoteAction(true, pc_key::ESCAPE).kind == RemoteActionKind::LeaveTextMode);
    // Neither a delete character nor a key with no character is text.
    CHECK(remoteAction(true, ASCII_DELETE).kind == RemoteActionKind::None);
    CHECK(remoteAction(true, pc_key::UP).kind == RemoteActionKind::None);
    CHECK(remoteAction(true, pc_key::F1).kind == RemoteActionKind::None);
}

TEST_CASE("Given the mapping, When it is described, Then every sampler key of the approved mapping is named and the end key is given [RQ-AKM-076]",
          "[akm][front-panel][remote]")
{
    const std::vector<std::string>& lines = akm::harness::remoteMappingLines();
    REQUIRE_FALSE(lines.empty());
    std::string text;
    for (const std::string& line : lines)
        text += line + "\n";
    for (const char* mention : {"F1", "ENT/PLAY", "EXIT", "CURSOR", "MULTI", "EDIT SAMPLE", "EDIT PROGRAM", "RECORD", "UTILITIES",
                                "SAVE", "LOAD", "WINDOW", "MARK", "JUMP", "FX", "wheel", "Tab", "Space", "q"})
    {
        CAPTURE(mention);
        CHECK(text.find(mention) != std::string::npos);
    }
}

TEST_CASE("Given a scripted source of PC keys and the owner's confirmation, When the check runs, Then each key sends exactly the frames its mapping names and q ends it [RQ-AKM-076]",
          "[akm][suite][front-panel][remote]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.readOwnerKey = scripted({pc_key::F1, '5', pc_key::LEFT, pc_key::UP, pc_key::PAGE_DOWN, 'q', pc_key::F1 + 1});

    const RealSuiteResult result = rig.run(options);

    CHECK(remoteCheck(result).outcome == CheckOutcome::Passed);
    CHECK(seen(rig.sampler)
          == std::vector<Seen>{{FrontPanelEventKind::KeyHold, code(FrontPanelKey::F1)},
                               {FrontPanelEventKind::KeyRelease, code(FrontPanelKey::F1)},
                               {FrontPanelEventKind::KeyHold, code(FrontPanelKey::Digit5)},
                               {FrontPanelEventKind::KeyRelease, code(FrontPanelKey::Digit5)},
                               {FrontPanelEventKind::KeyHold, code(FrontPanelKey::CursorLeft)},
                               {FrontPanelEventKind::KeyRelease, code(FrontPanelKey::CursorLeft)},
                               {FrontPanelEventKind::DataWheel, 0, WHEEL_ONE_CLICK},
                               {FrontPanelEventKind::DataWheel, 1, WHEEL_PAGE_CLICKS}});
    CHECK(rig.sampler.frontPanel().keysDown.empty());
    CHECK(result.knownStateRestored);
}

TEST_CASE("Given Space pressed once, When the check ends, Then ENT/PLAY was held and is released at the end [RQ-AKM-076, RQ-AKM-075]",
          "[akm][suite][front-panel][remote]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.readOwnerKey = scripted({pc_key::SPACE, 'q'});

    const RealSuiteResult result = rig.run(options);

    CHECK(remoteCheck(result).outcome == CheckOutcome::Passed);
    CHECK(seen(rig.sampler)
          == std::vector<Seen>{{FrontPanelEventKind::KeyHold, code(FrontPanelKey::EntPlay)},
                               {FrontPanelEventKind::KeyRelease, code(FrontPanelKey::EntPlay)}});
    CHECK(rig.sampler.frontPanel().keysDown.empty());
}

TEST_CASE("Given Space pressed twice, When the check runs, Then ENT/PLAY is held then released by the owner [RQ-AKM-076]",
          "[akm][suite][front-panel][remote]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.readOwnerKey = scripted({pc_key::SPACE, pc_key::F1, pc_key::SPACE, 'q'});

    const RealSuiteResult result = rig.run(options);

    CHECK(remoteCheck(result).outcome == CheckOutcome::Passed);
    CHECK(seen(rig.sampler)
          == std::vector<Seen>{{FrontPanelEventKind::KeyHold, code(FrontPanelKey::EntPlay)},
                               {FrontPanelEventKind::KeyHold, code(FrontPanelKey::F1)},
                               {FrontPanelEventKind::KeyRelease, code(FrontPanelKey::F1)},
                               {FrontPanelEventKind::KeyRelease, code(FrontPanelKey::EntPlay)}});
}

TEST_CASE("Given the text mode, When keys are typed and Escape leaves it, Then they go as ASCII, Escape sends no EXIT, and the normal mode is back [RQ-AKM-076]",
          "[akm][suite][front-panel][remote]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.readOwnerKey = scripted({pc_key::TAB, 'H', 'i', pc_key::BACKSPACE, pc_key::ESCAPE, 'm', 'q'});

    const RealSuiteResult result = rig.run(options);

    CHECK(remoteCheck(result).outcome == CheckOutcome::Passed);
    CHECK(seen(rig.sampler)
          == std::vector<Seen>{{FrontPanelEventKind::AsciiKey, 'H'},
                               {FrontPanelEventKind::AsciiKey, 'i'},
                               {FrontPanelEventKind::AsciiKey, ASCII_BACKSPACE},
                               {FrontPanelEventKind::KeyHold, code(FrontPanelKey::Multi)},
                               {FrontPanelEventKind::KeyRelease, code(FrontPanelKey::Multi)}});
}

TEST_CASE("Given PC keys the mapping does not give, When they are pressed, Then nothing is sent and the owner is told [RQ-AKM-076]",
          "[akm][suite][front-panel][remote]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    std::vector<std::string> told;
    options.tellOwner = [&told](const std::string& line) { told.push_back(line); };
    options.readOwnerKey = scripted({'z', pc_key::F1 + 8, 'q'});

    const RealSuiteResult result = rig.run(options);

    CHECK(remoteCheck(result).outcome == CheckOutcome::Passed);
    CHECK(seen(rig.sampler).empty());
    CHECK_FALSE(told.empty());
}

TEST_CASE("Given the owner's input ending with a key held, When the check runs, Then the key is released and the check passes [RQ-AKM-076, RQ-AKM-075]",
          "[akm][suite][front-panel][remote]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.readOwnerKey = scripted({pc_key::SPACE});

    const RealSuiteResult result = rig.run(options);

    CHECK(remoteCheck(result).outcome == CheckOutcome::Passed);
    CHECK(rig.sampler.frontPanel().keysDown.empty());
    CHECK(seen(rig.sampler).size() == 2);
}

TEST_CASE("Given a check that throws after a key was held, When it ends, Then the session's close released the key and the sampler is in the known state [RQ-AKM-076, RQ-AKM-075, RQ-AKM-018]",
          "[akm][suite][front-panel][remote]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    auto calls = std::make_shared<int>(0);
    options.readOwnerKey = [calls]() -> std::optional<int> {
        if ((*calls)++ == 0)
            return pc_key::SPACE;
        throw std::runtime_error("the console broke");
    };

    const RealSuiteResult result = rig.run(options);

    CHECK(remoteCheck(result).outcome == CheckOutcome::Failed);
    CHECK(rig.sampler.frontPanel().keysDown.empty());
    CHECK(result.knownStateRestored);
}

TEST_CASE("Given the owner does not confirm the screen, When the check runs, Then it is skipped and no front panel frame is sent [RQ-AKM-076]",
          "[akm][suite][front-panel][remote]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.askOwner = [](const std::string&) { return false; };
    options.readOwnerKey = scripted({pc_key::F1, 'q'});

    const RealSuiteResult result = rig.run(options);

    CHECK(remoteCheck(result).outcome == CheckOutcome::Skipped);
    CHECK(seen(rig.sampler).empty());
}

TEST_CASE("Given no way to ask the owner or no source of PC keys, When the check runs, Then it is skipped and no front panel frame is sent [RQ-AKM-076]",
          "[akm][suite][front-panel][remote]")
{
    {
        Rig rig;
        RealSuiteOptions options = rig.options();
        options.askOwner = nullptr;
        options.readOwnerKey = scripted({pc_key::F1, 'q'});
        const RealSuiteResult result = rig.run(options);
        CHECK(remoteCheck(result).outcome == CheckOutcome::Skipped);
        CHECK(seen(rig.sampler).empty());
    }
    {
        Rig rig;
        RealSuiteOptions options = rig.options();
        options.readOwnerKey = nullptr;
        const RealSuiteResult result = rig.run(options);
        CHECK(remoteCheck(result).outcome == CheckOutcome::Skipped);
        CHECK(seen(rig.sampler).empty());
    }
}

TEST_CASE("Given the suite run without --front-panel, When it runs, Then there is no front panel check and nothing of section 20 is sent [RQ-AKM-076]",
          "[akm][suite][front-panel][remote]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    options.frontPanel = false;
    options.readOwnerKey = scripted({pc_key::F1, 'q'});

    const RealSuiteResult result = rig.run(options);

    CHECK(result.checks.size() == AUTOMATIC_CHECKS);
    CHECK(seen(rig.sampler).empty());
}

TEST_CASE("Given the mapping's keys pressed against the real item catalogue, When every normal-mode row is pressed once, Then every key reaches the sampler as its own keycode [RQ-AKM-076, RQ-AKM-073]",
          "[akm][suite][front-panel][remote]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    std::vector<int> keys;
    for (const PressRow& row : PRESS_ROWS)
        keys.push_back(row.pcKey);
    keys.push_back('q');
    options.readOwnerKey = scripted(keys);

    const RealSuiteResult result = rig.run(options);

    CHECK(remoteCheck(result).outcome == CheckOutcome::Passed);
    const std::vector<FrontPanelEvent> events = rig.sampler.frontPanel().events;
    REQUIRE(events.size() == PRESS_ROWS.size() * 2);
    for (std::size_t index = 0; index < PRESS_ROWS.size(); ++index)
    {
        CHECK(events[index * 2].kind == FrontPanelEventKind::KeyHold);
        CHECK(events[index * 2].first == code(PRESS_ROWS[index].key));
        CHECK(events[index * 2 + 1].kind == FrontPanelEventKind::KeyRelease);
        CHECK(events[index * 2 + 1].first == code(PRESS_ROWS[index].key));
    }
}

TEST_CASE("Given the 43 keys of Table 31, When their names are asked for, Then each has the name of the front panel and no other value has one [RQ-AKM-076]",
          "[akm][front-panel][remote]")
{
    constexpr int DATA_BYTE_VALUES = 128;
    std::size_t named = 0;
    for (int value = 0; value < DATA_BYTE_VALUES; ++value)
    {
        const std::optional<FrontPanelKey> key = akm::frontPanelKeyFromCode(value);
        const std::string_view name = akm::harness::remoteKeyName(static_cast<FrontPanelKey>(value));
        CAPTURE(value);
        CHECK(name.empty() == !key.has_value());
        if (key)
            ++named;
    }
    CHECK(named == 43);
    CHECK(akm::harness::remoteKeyName(FrontPanelKey::EntPlay) == "ENT/PLAY");
    CHECK(akm::harness::remoteKeyName(FrontPanelKey::EditSample) == "EDIT SAMPLE");
    CHECK(akm::harness::remoteKeyName(FrontPanelKey::CursorLeft) == "CURSOR <");
}

TEST_CASE("Given the owner is about to be given the keyboard, When the check starts, Then the whole mapping is shown to the owner before the first key is read and before the confirmation is asked [RQ-AKM-076]",
          "[akm][suite][front-panel][remote]")
{
    Rig rig;
    RealSuiteOptions options = rig.options();
    std::vector<std::string> told;
    options.tellOwner = [&told](const std::string& line) { told.push_back(line); };
    std::size_t toldAtConfirmation = 0;
    options.askOwner = [&told, &toldAtConfirmation](const std::string&) {
        toldAtConfirmation = told.size();
        return true;
    };
    std::size_t toldAtFirstKey = 0;
    auto keys = scripted({'q'});
    options.readOwnerKey = [&told, &toldAtFirstKey, keys]() {
        if (toldAtFirstKey == 0)
            toldAtFirstKey = told.size();
        return keys();
    };

    const RealSuiteResult result = rig.run(options);

    CHECK(remoteCheck(result).outcome == CheckOutcome::Passed);
    const std::vector<std::string>& mapping = akm::harness::remoteMappingLines();
    REQUIRE(toldAtConfirmation >= mapping.size());
    for (std::size_t index = 0; index < mapping.size(); ++index)
        CHECK(told[index + (toldAtConfirmation - mapping.size())] == mapping[index]);
    CHECK(toldAtFirstKey >= toldAtConfirmation);
}
