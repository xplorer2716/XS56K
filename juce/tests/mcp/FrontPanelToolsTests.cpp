/*
 * XS56K - Editor for AKAI S5000/S6000 samplers
 * Copyright (C) 2026 xplorer2716
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// The front-panel keys, behind --allow-front-panel: press_key, hold_key, release_key, turn_data_wheel and send_ascii_key, over a real session and
// the simulated sampler. A key can answer "ENT" to a delete or save screen, which no `confirm` of this server guards: the tools exist only when the
// person switched them on. [TASK-MCP-053, RQ-MCP-054, RQ-MCP-057, ADR-MCP-005 (DEC-MCP-032)]
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "ToolRig.hpp"

using json = nlohmann::json;
using mcp::test::hasText;
using mcp::test::ToolRig;
using mcp::test::toolFailed;
using mcp::test::toolText;

namespace
{
    constexpr const char* PRESS = "press_key";
    constexpr const char* HOLD = "hold_key";
    constexpr const char* RELEASE = "release_key";
    constexpr const char* WHEEL = "turn_data_wheel";
    constexpr const char* ASCII = "send_ascii_key";
    constexpr std::uint8_t KEYCODE_F1 = 0x48;
    constexpr std::uint8_t KEYCODE_ENT_PLAY = 0x6B;
    constexpr std::uint8_t KEYCODE_DIGIT_5 = 0x5D;
    constexpr std::uint8_t KEYCODE_EXIT = 0x6A;
    constexpr int WHEEL_FORWARDS = 0;
    constexpr int WHEEL_BACKWARDS = 1;
    constexpr int ASCII_A = 65;
    constexpr int ASCII_LAST = 127;
    constexpr int WHEEL_CLICKS_LAST = 8;

    const std::vector<const char*> KEY_TOOLS = {PRESS, HOLD, RELEASE, WHEEL, ASCII};

    struct PanelRig : ToolRig
    {
        PanelRig() : ToolRig(false, true) {}
    };

    bool listed(ToolRig& rig, const char* name)
    {
        return !rig.tool(name).is_null();
    }

    std::size_t panelEvents(const ToolRig& rig)
    {
        return rig.sampler->frontPanel().events.size();
    }
}

TEST_CASE("Given the server without --allow-front-panel, When the tools are listed, Then none of the key tools is there and a call to one is refused as an unknown tool [RQ-MCP-054]",
          "[mcp][frontpanel]")
{
    ToolRig rig;
    for (const char* name : KEY_TOOLS)
    {
        INFO(name);
        CHECK_FALSE(listed(rig, name));
        const json answer = rig.call(name, {{"key", "f1"}});
        CHECK(answer.contains("error"));
    }
    CHECK(panelEvents(rig) == 0);
}

TEST_CASE("Given the server with --allow-front-panel, When the tools are listed, Then the five key tools are there, destructive and not idempotent, and their descriptions warn that a key can answer a delete or save screen [RQ-MCP-054]",
          "[mcp][frontpanel]")
{
    PanelRig rig;
    for (const char* name : KEY_TOOLS)
    {
        const json tool = rig.tool(name);
        INFO(name);
        REQUIRE_FALSE(tool.is_null());
        CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
        CHECK(tool["annotations"]["destructiveHint"].get<bool>());
        CHECK_FALSE(tool["annotations"]["idempotentHint"].get<bool>());
        CHECK(hasText(tool["description"].get<std::string>(), "ENT"));
    }
}

TEST_CASE("Given the F1 key, When it is pressed, Then the simulated sampler received a hold then a release of its keycode and no key is down; the answer says the sampler only queued it [RQ-MCP-054]",
          "[mcp][frontpanel]")
{
    PanelRig rig;
    const json answer = rig.call(PRESS, {{"key", "f1"}});
    CHECK_FALSE(toolFailed(answer));
    CHECK(hasText(toolText(answer), "Pressed the key \"f1\""));
    CHECK(hasText(toolText(answer), "queued"));
    const auto state = rig.sampler->frontPanel();
    REQUIRE(state.events.size() == 2);
    CHECK(state.events[0].kind == akm::harness::FrontPanelEventKind::KeyHold);
    CHECK(state.events[0].first == KEYCODE_F1);
    CHECK(state.events[1].kind == akm::harness::FrontPanelEventKind::KeyRelease);
    CHECK(state.events[1].first == KEYCODE_F1);
    CHECK(state.keysDown.empty());
}

TEST_CASE("Given a key held then released, When hold_key and release_key are called, Then the simulated sampler holds it between the two [RQ-MCP-054]",
          "[mcp][frontpanel]")
{
    PanelRig rig;
    const json held = rig.call(HOLD, {{"key", "ent_play"}});
    CHECK_FALSE(toolFailed(held));
    CHECK(hasText(toolText(held), "Holding the key \"ent_play\""));
    CHECK(hasText(toolText(held), "release_key"));
    CHECK(rig.sampler->frontPanel().keysDown == std::vector<std::uint8_t>{KEYCODE_ENT_PLAY});
    const json released = rig.call(RELEASE, {{"key", "ent_play"}});
    CHECK_FALSE(toolFailed(released));
    CHECK(hasText(toolText(released), "Released the key \"ent_play\""));
    CHECK(rig.sampler->frontPanel().keysDown.empty());
}

TEST_CASE("Given a key held, When the session closes, Then the simulated sampler received the release of that key [RQ-MCP-054]",
          "[mcp][frontpanel]")
{
    PanelRig rig;
    REQUIRE_FALSE(toolFailed(rig.call(HOLD, {{"key", "exit"}})));
    REQUIRE(rig.sampler->frontPanel().keysDown == std::vector<std::uint8_t>{KEYCODE_EXIT});
    CHECK(rig.accepted(akm::ItemId::FrontPanelKeyRelease) == 0);
    rig.gateway.close();
    CHECK(rig.accepted(akm::ItemId::FrontPanelKeyRelease) == 1);
    CHECK(rig.sampler->frontPanel().keysDown.empty());
}

TEST_CASE("Given a key name in other letters, with a space or a hyphen, or a digit, When a key is pressed, Then it is found; an unknown name sends nothing and the answer lists the keys [RQ-MCP-054]",
          "[mcp][frontpanel]")
{
    PanelRig rig;
    CHECK_FALSE(toolFailed(rig.call(PRESS, {{"key", "Ent Play"}})));
    CHECK_FALSE(toolFailed(rig.call(PRESS, {{"key", "CURSOR-LEFT"}})));
    CHECK_FALSE(toolFailed(rig.call(PRESS, {{"key", "5"}})));
    CHECK(rig.sampler->frontPanel().events[4].first == KEYCODE_DIGIT_5);
    const std::size_t before = panelEvents(rig);
    const json unknown = rig.call(PRESS, {{"key", "enter"}});
    CHECK(toolFailed(unknown));
    for (const char* name : {"multi", "fx", "edit_sample", "f16", "0", "9", "minus", "plus", "cursor_right", "window", "mark", "jump", "exit", "ent_play"})
        CHECK(hasText(toolText(unknown), name));
    CHECK(panelEvents(rig) == before);
}

TEST_CASE("Given the data wheel, When it is turned forwards by 3 clicks and backwards by 8, Then the simulated sampler received each; 0 and 9 clicks and a direction that is neither send nothing [RQ-MCP-054]",
          "[mcp][frontpanel]")
{
    PanelRig rig;
    const json forwards = rig.call(WHEEL, {{"direction", "forwards"}, {"clicks", 3}});
    CHECK_FALSE(toolFailed(forwards));
    CHECK(hasText(toolText(forwards), "forwards by 3 clicks"));
    CHECK_FALSE(toolFailed(rig.call(WHEEL, {{"direction", "Backwards"}, {"clicks", WHEEL_CLICKS_LAST}})));
    const auto events = rig.sampler->frontPanel().events;
    REQUIRE(events.size() == 2);
    CHECK(events[0].kind == akm::harness::FrontPanelEventKind::DataWheel);
    CHECK(events[0].first == WHEEL_FORWARDS);
    CHECK(events[0].second == 3);
    CHECK(events[1].first == WHEEL_BACKWARDS);
    CHECK(events[1].second == WHEEL_CLICKS_LAST);

    CHECK(toolFailed(rig.call(WHEEL, {{"direction", "forwards"}, {"clicks", 0}})));
    CHECK(toolFailed(rig.call(WHEEL, {{"direction", "forwards"}, {"clicks", WHEEL_CLICKS_LAST + 1}})));
    const json sideways = rig.call(WHEEL, {{"direction", "sideways"}, {"clicks", 1}});
    CHECK(toolFailed(sideways));
    CHECK(hasText(toolText(sideways), "forwards, backwards"));
    CHECK(panelEvents(rig) == 2);
}

TEST_CASE("Given an ASCII character as a number or as one character, When it is sent, Then the simulated sampler received its value; a value beyond 127 or a longer text sends nothing [RQ-MCP-054]",
          "[mcp][frontpanel]")
{
    PanelRig rig;
    CHECK_FALSE(toolFailed(rig.call(ASCII, {{"ascii", ASCII_A}})));
    CHECK_FALSE(toolFailed(rig.call(ASCII, {{"ascii", "A"}})));
    CHECK_FALSE(toolFailed(rig.call(ASCII, {{"ascii", 0}})));
    CHECK_FALSE(toolFailed(rig.call(ASCII, {{"ascii", ASCII_LAST}})));
    const auto events = rig.sampler->frontPanel().events;
    REQUIRE(events.size() == 4);
    CHECK(events[0].kind == akm::harness::FrontPanelEventKind::AsciiKey);
    CHECK(events[0].first == ASCII_A);
    CHECK(events[1].first == ASCII_A);
    CHECK(events[2].first == 0);
    CHECK(events[3].first == ASCII_LAST);

    CHECK(toolFailed(rig.call(ASCII, {{"ascii", ASCII_LAST + 1}})));
    CHECK(toolFailed(rig.call(ASCII, {{"ascii", -1}})));
    CHECK(toolFailed(rig.call(ASCII, {{"ascii", "AB"}})));
    CHECK(toolFailed(rig.call(ASCII, {{"ascii", ""}})));
    CHECK(toolFailed(rig.call(ASCII, {{"ascii", "\xC3\xA9"}})));
    CHECK(toolFailed(rig.call(ASCII, {{"ascii", true}})));
    CHECK(panelEvents(rig) == 4);
}

TEST_CASE("Given a missing, an unknown or a badly typed argument, When a key tool is called, Then it is refused and nothing is sent [RQ-MCP-054]",
          "[mcp][frontpanel]")
{
    PanelRig rig;
    for (const char* name : {PRESS, HOLD, RELEASE})
    {
        INFO(name);
        CHECK(toolFailed(rig.call(name)));
        CHECK(toolFailed(rig.call(name, {{"key", 5}})));
        CHECK(toolFailed(rig.call(name, {{"key", "f1"}, {"extra", 1}})));
    }
    CHECK(toolFailed(rig.call(WHEEL)));
    CHECK(toolFailed(rig.call(WHEEL, {{"direction", "forwards"}})));
    CHECK(toolFailed(rig.call(WHEEL, {{"clicks", 1}})));
    CHECK(toolFailed(rig.call(WHEEL, {{"direction", "forwards"}, {"clicks", "1"}})));
    CHECK(toolFailed(rig.call(WHEEL, {{"direction", 1}, {"clicks", 1}})));
    CHECK(toolFailed(rig.call(ASCII)));
    CHECK(toolFailed(rig.call(ASCII, {{"ascii", 65}, {"extra", 1}})));
    CHECK(panelEvents(rig) == 0);
}
