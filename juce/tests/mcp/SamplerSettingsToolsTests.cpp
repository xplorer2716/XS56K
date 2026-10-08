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

// The sampler's own settings: get_sampler_settings and set_sampler_setting (name, clock, play mode, front-panel lock), over a real
// session and the simulated sampler. [TASK-MCP-045, RQ-MCP-046, ADR-MCP-005 (DEC-MCP-030)]
#include <catch2/catch_test_macros.hpp>

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
    constexpr const char* SETTING_NAME = "name";
    constexpr const char* SETTING_CLOCK = "clock";
    constexpr const char* SETTING_PLAY_MODE = "play_mode";
    constexpr const char* SETTING_FRONT_PANEL = "front_panel";
    constexpr std::size_t MAX_SAMPLER_NAME_LENGTH = 20;

    json set(ToolRig& rig, const char* setting, const std::string& value)
    {
        return rig.call("set_sampler_setting", {{"setting", setting}, {"value", value}});
    }
}

TEST_CASE("Given the server, When the tools are listed, Then get_sampler_settings is a read and set_sampler_setting changes the sampler without deleting anything [RQ-MCP-046, RQ-MCP-013]",
          "[mcp][settings]")
{
    ToolRig rig;
    const json get = rig.tool("get_sampler_settings");
    REQUIRE_FALSE(get.is_null());
    CHECK(get["annotations"]["readOnlyHint"].get<bool>());
    const json setter = rig.tool("set_sampler_setting");
    REQUIRE_FALSE(setter.is_null());
    CHECK_FALSE(setter["annotations"]["readOnlyHint"].get<bool>());
    CHECK_FALSE(setter["annotations"]["destructiveHint"].get<bool>());
    CHECK(setter["annotations"]["idempotentHint"].get<bool>());
}

TEST_CASE("Given the name TESTSAMP, When it is set, Then it is sent once, read back in the answer and given by get_sampler_settings [RQ-MCP-046]",
          "[mcp][settings]")
{
    ToolRig rig;
    const json answer = set(rig, SETTING_NAME, "TESTSAMP");
    CHECK_FALSE(toolFailed(answer));
    CHECK(hasText(toolText(answer), "\"TESTSAMP\""));
    CHECK(rig.accepted(akm::ItemId::SystemSetName) == 1);
    CHECK(hasText(toolText(rig.call("get_sampler_settings")), "Name: TESTSAMP"));
}

TEST_CASE("Given a name of 21 characters, an empty name or a name with a character beyond plain ASCII, When it is set, Then nothing is sent and the answer says what is accepted [RQ-MCP-046]",
          "[mcp][settings]")
{
    ToolRig rig;
    const std::vector<std::string> refused = {std::string(MAX_SAMPLER_NAME_LENGTH + 1, 'A'), "", "caf\xC3\xA9"};
    for (const std::string& name : refused)
    {
        const json answer = set(rig, SETTING_NAME, name);
        CHECK(toolFailed(answer));
        CHECK(hasText(toolText(answer), "1 to 20 characters"));
    }
    CHECK(rig.accepted(akm::ItemId::SystemSetName) == 0);

    CHECK_FALSE(toolFailed(set(rig, SETTING_NAME, std::string(MAX_SAMPLER_NAME_LENGTH, 'B'))));
    CHECK(rig.accepted(akm::ItemId::SystemSetName) == 1);
}

TEST_CASE("Given a date and a time, When the clock is set, Then the sampler is sent that clock with the day of the week worked out, and the answer and get_sampler_settings give it back [RQ-MCP-046]",
          "[mcp][settings]")
{
    struct Case
    {
        const char* value;
        const char* shown;
    };
    const std::vector<Case> cases = {
        {"2026-10-07 22:15:30", "2026-10-07 22:15:30 (Wednesday)"},
        {"2026-10-07T22:15:30", "2026-10-07 22:15:30 (Wednesday)"},
        {"1980-01-01 00:00:00", "1980-01-01 00:00:00 (Tuesday)"},
        {"2079-12-31 23:59:59", "2079-12-31 23:59:59 (Sunday)"},
        {"2028-02-29 12:00:00", "2028-02-29 12:00:00 (Tuesday)"},
    };
    for (const Case& test : cases)
    {
        ToolRig rig;
        const json answer = set(rig, SETTING_CLOCK, test.value);
        INFO(test.value);
        CHECK_FALSE(toolFailed(answer));
        CHECK(hasText(toolText(answer), test.shown));
        CHECK(rig.accepted(akm::ItemId::SystemSetClock) == 1);
        CHECK(hasText(toolText(rig.call("get_sampler_settings")), (std::string("Clock: ") + test.shown).c_str()));
    }
}

TEST_CASE("Given a date that does not exist, a time outside its range, a year the sampler does not take or a text that is not a date, When the clock is set, Then nothing is sent and the answer names the problem and the format [RQ-MCP-046]",
          "[mcp][settings]")
{
    struct Case
    {
        const char* value;
        const char* problem;
    };
    const std::vector<Case> cases = {
        {"2027-02-29 00:00:00", "day"},    // 2027 is not a leap year
        {"2026-04-31 10:00:00", "day"},    // April has 30 days
        {"2026-13-01 00:00:00", "month"},
        {"2026-00-10 00:00:00", "month"},
        {"1979-12-31 23:59:59", "year"},
        {"2080-01-01 00:00:00", "year"},
        {"2026-10-07 24:00:00", "hours"},
        {"2026-10-07 12:60:00", "minutes"},
        {"2026-10-07 12:00:60", "seconds"},
        {"yesterday", "YYYY-MM-DD HH:MM:SS"},
        {"2026-10-07", "YYYY-MM-DD HH:MM:SS"},
        {"2026-1-7 1:2:3", "YYYY-MM-DD HH:MM:SS"},
    };
    ToolRig rig;
    for (const Case& test : cases)
    {
        const json answer = set(rig, SETTING_CLOCK, test.value);
        INFO(test.value);
        CHECK(toolFailed(answer));
        CHECK(hasText(toolText(answer), test.problem));
        CHECK(hasText(toolText(answer), "YYYY-MM-DD HH:MM:SS"));
    }
    CHECK(rig.accepted(akm::ItemId::SystemSetClock) == 0);
}

TEST_CASE("Given each play mode, When it is set, Then it is read back as set, and the muted mode is said to play nothing [RQ-MCP-046]",
          "[mcp][settings]")
{
    ToolRig rig;
    for (const char* mode : {"multi", "program", "sample", "muted"})
    {
        const json answer = set(rig, SETTING_PLAY_MODE, mode);
        INFO(mode);
        CHECK_FALSE(toolFailed(answer));
        CHECK(hasText(toolText(answer), (std::string("\"") + mode + "\"").c_str()));
        CHECK(hasText(toolText(rig.call("get_sampler_settings")), (std::string("Play mode: ") + mode).c_str()));
    }
    CHECK(rig.accepted(akm::ItemId::SystemSetPlayMode) == 4);

    const std::string muted = toolText(set(rig, SETTING_PLAY_MODE, "muted"));
    CHECK(hasText(muted, "plays nothing"));
    CHECK(hasText(muted, "play_mode"));
    const std::string multi = toolText(set(rig, SETTING_PLAY_MODE, "Multi"));
    CHECK_FALSE(hasText(multi, "plays nothing"));
}

TEST_CASE("Given a play mode the sampler does not have, When it is set, Then nothing is sent and the answer lists the four modes [RQ-MCP-046]",
          "[mcp][settings]")
{
    ToolRig rig;
    const json answer = set(rig, SETTING_PLAY_MODE, "loud");
    CHECK(toolFailed(answer));
    CHECK(hasText(toolText(answer), "multi, program, sample, muted"));
    CHECK(rig.accepted(akm::ItemId::SystemSetPlayMode) == 0);
}

TEST_CASE("Given the front panel, When it is locked, Then get_sampler_settings says locked and the answer says how to unlock it; when it is set to normal, Then it is normal again [RQ-MCP-046]",
          "[mcp][settings]")
{
    ToolRig rig;
    const std::string locked = toolText(set(rig, SETTING_FRONT_PANEL, "locked"));
    CHECK(hasText(locked, "\"locked\""));
    CHECK(hasText(locked, "set_sampler_setting"));
    CHECK(hasText(locked, "\"front_panel\""));
    CHECK(hasText(locked, "\"normal\""));
    CHECK(hasText(toolText(rig.call("get_sampler_settings")), "Front panel: locked"));

    const std::string normal = toolText(set(rig, SETTING_FRONT_PANEL, "normal"));
    CHECK(hasText(normal, "\"normal\""));
    CHECK_FALSE(hasText(normal, "To unlock"));
    CHECK(hasText(toolText(rig.call("get_sampler_settings")), "Front panel: normal"));
    CHECK(rig.accepted(akm::ItemId::SystemSetFrontPanelLock) == 2);

    const json refused = set(rig, SETTING_FRONT_PANEL, "half");
    CHECK(toolFailed(refused));
    CHECK(hasText(toolText(refused), "normal, locked"));
    CHECK(rig.accepted(akm::ItemId::SystemSetFrontPanelLock) == 2);
}

TEST_CASE("Given a missing, an unknown or a badly typed argument, When set_sampler_setting or get_sampler_settings is called, Then it is refused and nothing is sent [RQ-MCP-046]",
          "[mcp][settings]")
{
    ToolRig rig;
    CHECK(toolFailed(rig.call("set_sampler_setting")));
    CHECK(toolFailed(rig.call("set_sampler_setting", {{"setting", SETTING_NAME}})));
    CHECK(toolFailed(rig.call("set_sampler_setting", {{"value", "X"}})));
    CHECK(toolFailed(rig.call("set_sampler_setting", {{"setting", SETTING_NAME}, {"value", 5}})));
    CHECK(toolFailed(rig.call("set_sampler_setting", {{"setting", SETTING_NAME}, {"value", "X"}, {"extra", 1}})));
    CHECK(toolFailed(rig.call("get_sampler_settings", {{"extra", 1}})));

    const json unknown = rig.call("set_sampler_setting", {{"setting", "volume"}, {"value", "10"}});
    CHECK(toolFailed(unknown));
    CHECK(hasText(toolText(unknown), "name, clock, play_mode, front_panel"));

    CHECK(rig.accepted(akm::ItemId::SystemSetName) == 0);
    CHECK(rig.accepted(akm::ItemId::SystemSetClock) == 0);
    CHECK(rig.accepted(akm::ItemId::SystemSetPlayMode) == 0);
    CHECK(rig.accepted(akm::ItemId::SystemSetFrontPanelLock) == 0);
}
