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

// The keygroup tools of the MCP server: add_keygroups and delete_keygroup (confirmed by the program's name), over a real session
// and the simulated sampler. [TASK-MCP-030, RQ-MCP-035, RQ-MCP-042, ADR-MCP-004 (DEC-MCP-023, DEC-MCP-024)]
#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include "ToolRig.hpp"

using json = nlohmann::json;
using mcp::test::hasText;
using mcp::test::ToolRig;
using mcp::test::toolFailed;
using mcp::test::toolText;

TEST_CASE("Given the server, When the tools are listed, Then add_keygroups is not destructive and delete_keygroup is [RQ-MCP-035, RQ-MCP-042]",
          "[mcp][keygroup]")
{
    ToolRig rig;
    const json add = rig.tool("add_keygroups");
    const json remove = rig.tool("delete_keygroup");
    REQUIRE_FALSE(add.is_null());
    REQUIRE_FALSE(remove.is_null());
    CHECK_FALSE(add["annotations"]["readOnlyHint"].get<bool>());
    CHECK_FALSE(add["annotations"]["destructiveHint"].get<bool>());
    CHECK_FALSE(remove["annotations"]["readOnlyHint"].get<bool>());
    CHECK(remove["annotations"]["destructiveHint"].get<bool>());
}

TEST_CASE("Given BASS with three keygroups, When add_keygroups adds two, Then the program has five [RQ-MCP-035]", "[mcp][keygroup]")
{
    ToolRig rig;
    const std::size_t seeded = rig.accepted(akm::ItemId::ProgramAddKeygroups);  // the rig added keygroups itself to build BASS and LEAD
    const json answer = rig.call("add_keygroups", {{"count", 2}});
    CHECK_FALSE(toolFailed(answer));
    CHECK(hasText(toolText(answer), "Added 2 keygroups"));
    CHECK(hasText(toolText(answer), "\"BASS\""));
    CHECK(hasText(toolText(answer), "now has 5 keygroups"));
    CHECK(rig.accepted(akm::ItemId::ProgramAddKeygroups) == seeded + 1);
    CHECK(hasText(toolText(rig.call("get_status")), "5 keygroups"));
}

TEST_CASE("Given a count the program cannot take or a bad argument, When add_keygroups is called, Then nothing is sent and the answer says why [RQ-MCP-035]",
          "[mcp][keygroup]")
{
    ToolRig rig;
    const std::size_t seeded = rig.accepted(akm::ItemId::ProgramAddKeygroups);  // the rig added keygroups itself to build BASS and LEAD
    const json tooMany = rig.call("add_keygroups", {{"count", 97}});
    CHECK(toolFailed(tooMany));
    CHECK(hasText(toolText(tooMany), "at most 99"));
    CHECK(hasText(toolText(tooMany), "3"));
    CHECK(toolFailed(rig.call("add_keygroups", {{"count", 0}})));
    CHECK(toolFailed(rig.call("add_keygroups", {{"count", -2}})));
    CHECK(toolFailed(rig.call("add_keygroups", {{"count", "two"}})));
    CHECK(toolFailed(rig.call("add_keygroups")));
    CHECK(toolFailed(rig.call("add_keygroups", {{"count", 1}, {"extra", 1}})));
    CHECK(rig.accepted(akm::ItemId::ProgramAddKeygroups) == seeded);
}

TEST_CASE("Given BASS with three keygroups, When delete_keygroup is called for keygroup 3 with confirm BASS, Then the program has two [RQ-MCP-035, RQ-MCP-042]",
          "[mcp][keygroup]")
{
    ToolRig rig;
    const json answer = rig.call("delete_keygroup", {{"keygroup", 3}, {"confirm", "BASS"}});
    CHECK_FALSE(toolFailed(answer));
    CHECK(hasText(toolText(answer), "Deleted keygroup 3"));
    CHECK(hasText(toolText(answer), "now has 2 keygroups"));
    CHECK(rig.accepted(akm::ItemId::ProgramDeleteKeygroup) == 1);
    CHECK(hasText(toolText(rig.call("get_status")), "2 keygroups"));
}

TEST_CASE("Given a missing or wrong confirm, When delete_keygroup is called, Then nothing is sent and the answer names the program to give [RQ-MCP-042]",
          "[mcp][keygroup]")
{
    ToolRig rig;
    for (const char* confirm : {"bass", "PAD", "BASS ", ""})
    {
        const json answer = rig.call("delete_keygroup", {{"keygroup", 3}, {"confirm", confirm}});
        INFO(confirm);
        CHECK(toolFailed(answer));
        CHECK(hasText(toolText(answer), "\"BASS\""));
        CHECK(hasText(toolText(answer), "nothing was deleted"));
    }
    CHECK(toolFailed(rig.call("delete_keygroup", {{"keygroup", 3}})));
    CHECK(toolFailed(rig.call("delete_keygroup", {{"confirm", "BASS"}})));
    CHECK(rig.accepted(akm::ItemId::ProgramDeleteKeygroup) == 0);
}

TEST_CASE("Given a keygroup the program does not have, When delete_keygroup is called, Then nothing is sent and the answer gives the count [RQ-MCP-035]",
          "[mcp][keygroup]")
{
    ToolRig rig;
    for (const int keygroup : {4, 0, -1})
    {
        const json answer = rig.call("delete_keygroup", {{"keygroup", keygroup}, {"confirm", "BASS"}});
        INFO(keygroup);
        CHECK(toolFailed(answer));
    }
    CHECK(hasText(toolText(rig.call("delete_keygroup", {{"keygroup", 4}, {"confirm", "BASS"}})), "3 keygroups"));
    CHECK(rig.accepted(akm::ItemId::ProgramDeleteKeygroup) == 0);
}

TEST_CASE("Given PAD with one keygroup, When delete_keygroup is called for it, Then it is refused as the last keygroup and nothing is sent [RQ-MCP-035]",
          "[mcp][keygroup]")
{
    ToolRig rig;
    REQUIRE_FALSE(toolFailed(rig.call("select_program", {{"name", "PAD"}})));
    const json answer = rig.call("delete_keygroup", {{"keygroup", 1}, {"confirm", "PAD"}});
    CHECK(toolFailed(answer));
    CHECK(hasText(toolText(answer), "last keygroup"));
    CHECK(rig.accepted(akm::ItemId::ProgramDeleteKeygroup) == 0);
}
