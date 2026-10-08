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

// The zone sample tools of the MCP server: set_zone_sample and get_zone_samples, over a real session and the simulated sampler.
// [TASK-MCP-029, RQ-MCP-034, ADR-MCP-004 (DEC-MCP-024)]
#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include "ToolRig.hpp"

using json = nlohmann::json;
using mcp::test::hasText;
using mcp::test::ToolRig;
using mcp::test::toolFailed;
using mcp::test::toolText;

TEST_CASE("Given the server, When the tools are listed, Then set_zone_sample is an edit and get_zone_samples is a read [RQ-MCP-034, RQ-MCP-013]",
          "[mcp][zonesample]")
{
    ToolRig rig;
    const json set = rig.tool("set_zone_sample");
    const json get = rig.tool("get_zone_samples");
    REQUIRE_FALSE(set.is_null());
    REQUIRE_FALSE(get.is_null());
    CHECK_FALSE(set["annotations"]["readOnlyHint"].get<bool>());
    CHECK_FALSE(set["annotations"]["destructiveHint"].get<bool>());
    CHECK(get["annotations"]["readOnlyHint"].get<bool>());
}

TEST_CASE("Given BASS with three keygroups and the sample KICK, When set_zone_sample assigns KICK to zone 1 of keygroup 2, Then it is sent once, read back and listed for that keygroup only [RQ-MCP-034]",
          "[mcp][zonesample]")
{
    ToolRig rig;
    const json answer = rig.call("set_zone_sample", {{"sample", "KICK"}, {"zone", 1}, {"keygroup", 2}});
    CHECK_FALSE(toolFailed(answer));
    const std::string text = toolText(answer);
    CHECK(hasText(text, "zone 1 of keygroup 2"));
    CHECK(hasText(text, "\"BASS\""));
    CHECK(hasText(text, "\"KICK\""));
    CHECK(rig.accepted(akm::ItemId::ZoneSetSample) == 1);

    const std::string keygroupTwo = toolText(rig.call("get_zone_samples", {{"keygroup", 2}}));
    CHECK(hasText(keygroupTwo, "keygroup 2, zone 1: KICK"));
    CHECK(hasText(keygroupTwo, "keygroup 2, zone 2: no sample"));
    CHECK(hasText(keygroupTwo, "keygroup 2, zone 4: no sample"));
    CHECK_FALSE(hasText(keygroupTwo, "keygroup 1"));
    CHECK_FALSE(hasText(keygroupTwo, "keygroup 3"));

    const std::string all = toolText(rig.call("get_zone_samples"));
    CHECK(hasText(all, "keygroup 1, zone 1: no sample"));
    CHECK(hasText(all, "keygroup 2, zone 1: KICK"));
    CHECK(hasText(all, "keygroup 3, zone 4: no sample"));
}

TEST_CASE("Given the sample KICK, When it is named kick in lower case, Then it is assigned under the name the sampler lists [RQ-MCP-034]",
          "[mcp][zonesample]")
{
    ToolRig rig;
    const json answer = rig.call("set_zone_sample", {{"sample", "kick"}, {"zone", 2}, {"keygroup", 1}});
    CHECK_FALSE(toolFailed(answer));
    CHECK(hasText(toolText(answer), "\"KICK\""));
    CHECK(hasText(toolText(rig.call("get_zone_samples", {{"keygroup", 1}})), "keygroup 1, zone 2: KICK"));
}

TEST_CASE("Given a sample that is not in memory, When set_zone_sample is called, Then nothing is sent and the answer lists the samples [RQ-MCP-034]",
          "[mcp][zonesample]")
{
    ToolRig rig;
    const json answer = rig.call("set_zone_sample", {{"sample", "NOPE"}, {"zone", 1}, {"keygroup", 1}});
    CHECK(toolFailed(answer));
    CHECK(hasText(toolText(answer), "\"NOPE\""));
    CHECK(hasText(toolText(answer), "\"KICK\""));
    CHECK(hasText(toolText(answer), "\"SNARE\""));
    CHECK(rig.accepted(akm::ItemId::ZoneSetSample) == 0);
}

TEST_CASE("Given a zone outside 1 to 4 or a keygroup the program does not have, When set_zone_sample or get_zone_samples is called, Then nothing is sent and the answer says what exists [RQ-MCP-034]",
          "[mcp][zonesample]")
{
    ToolRig rig;
    for (const int zone : {0, 5, -1})
    {
        const json answer = rig.call("set_zone_sample", {{"sample", "KICK"}, {"zone", zone}, {"keygroup", 1}});
        INFO(zone);
        CHECK(toolFailed(answer));
        CHECK(hasText(toolText(answer), "zones 1 to 4"));
    }
    const json beyond = rig.call("set_zone_sample", {{"sample", "KICK"}, {"zone", 1}, {"keygroup", 4}});
    CHECK(toolFailed(beyond));
    CHECK(hasText(toolText(beyond), "3 keygroups"));
    const json readBeyond = rig.call("get_zone_samples", {{"keygroup", 4}});
    CHECK(toolFailed(readBeyond));
    CHECK(hasText(toolText(readBeyond), "3 keygroups"));
    CHECK(rig.accepted(akm::ItemId::ZoneSetSample) == 0);
}

TEST_CASE("Given missing or badly typed arguments, When set_zone_sample is called, Then nothing is sent [RQ-MCP-034]",
          "[mcp][zonesample]")
{
    ToolRig rig;
    CHECK(toolFailed(rig.call("set_zone_sample")));
    CHECK(toolFailed(rig.call("set_zone_sample", {{"zone", 1}, {"keygroup", 1}})));
    CHECK(toolFailed(rig.call("set_zone_sample", {{"sample", "KICK"}, {"keygroup", 1}})));
    CHECK(toolFailed(rig.call("set_zone_sample", {{"sample", "KICK"}, {"zone", 1}})));
    CHECK(toolFailed(rig.call("set_zone_sample", {{"sample", 3}, {"zone", 1}, {"keygroup", 1}})));
    CHECK(toolFailed(rig.call("set_zone_sample", {{"sample", "KICK"}, {"zone", "one"}, {"keygroup", 1}})));
    CHECK(toolFailed(rig.call("set_zone_sample", {{"sample", "KICK"}, {"zone", 1}, {"keygroup", 1}, {"extra", true}})));
    CHECK(toolFailed(rig.call("get_zone_samples", {{"keygroup", "two"}})));
    CHECK(rig.accepted(akm::ItemId::ZoneSetSample) == 0);
}

TEST_CASE("Given no program in memory, When get_zone_samples is called, Then the answer says there is none [RQ-MCP-034]",
          "[mcp][zonesample]")
{
    ToolRig rig;
    for (const char* name : {"PAD", "BASS", "LEAD"})
    {
        REQUIRE_FALSE(toolFailed(rig.call("select_program", {{"name", name}})));
        REQUIRE_FALSE(toolFailed(rig.call("delete_program", {{"confirm", name}})));
    }
    const json answer = rig.call("get_zone_samples");
    CHECK(toolFailed(answer));
    CHECK(hasText(toolText(answer), "program"));
}
