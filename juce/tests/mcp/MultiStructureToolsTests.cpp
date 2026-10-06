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

// The multi structure tools of the MCP server: create_multi, rename_multi, delete_multi, set_part_program, clear_part,
// set_multi_program_number and get_part_programs, over a real session and the simulated sampler. [TASK-MCP-032, RQ-MCP-037,
// RQ-MCP-038, RQ-MCP-042, ADR-MCP-004 (DEC-MCP-023, DEC-MCP-024)]
#include <catch2/catch_test_macros.hpp>

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
    constexpr std::size_t TOO_LONG_NAME_LENGTH = 21;
    constexpr int PART_BEYOND_ANY_MULTI = 1000;

    void selectMulti(ToolRig& rig, const char* name)
    {
        REQUIRE_FALSE(toolFailed(rig.call("select_multi", {{"name", name}})));
    }
}

TEST_CASE("Given the server, When the tools are listed, Then the multi tools carry their tiers: the deleting ones are destructive, get_part_programs is a read [RQ-MCP-037, RQ-MCP-038, RQ-MCP-042]",
          "[mcp][multistructure]")
{
    ToolRig rig;
    for (const char* name : {"create_multi", "rename_multi", "set_part_program", "set_multi_program_number"})
    {
        const json tool = rig.tool(name);
        INFO(name);
        REQUIRE_FALSE(tool.is_null());
        CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
        CHECK_FALSE(tool["annotations"]["destructiveHint"].get<bool>());
    }
    for (const char* name : {"delete_multi", "clear_part"})
    {
        const json tool = rig.tool(name);
        INFO(name);
        REQUIRE_FALSE(tool.is_null());
        CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
        CHECK(tool["annotations"]["destructiveHint"].get<bool>());
    }
    const json read = rig.tool("get_part_programs");
    REQUIRE_FALSE(read.is_null());
    CHECK(read["annotations"]["readOnlyHint"].get<bool>());
}

TEST_CASE("Given the multis LIVE and STUDIO, When create_multi gives STAGE, Then it is created once, listed and current [RQ-MCP-037]",
          "[mcp][multistructure]")
{
    ToolRig rig;
    const json answer = rig.call("create_multi", {{"name", "STAGE"}});
    CHECK_FALSE(toolFailed(answer));
    CHECK(hasText(toolText(answer), "Created the multi \"STAGE\""));
    CHECK(hasText(toolText(answer), "3 multis"));
    CHECK(rig.accepted(akm::ItemId::MultiCreate) == 1);
    const std::string list = toolText(rig.call("list_multis"));
    CHECK(hasText(list, "STAGE (current"));
    CHECK(hasText(list, "LIVE"));
}

// The real S5000 gave a new multi 32 parts (observed 2026-10-06, OBSERVATIONS-RQ-MCP-012-real-sampler.md). [TASK-MCP-041, RQ-MCP-044]
TEST_CASE("Given a new multi, When create_multi and then get_part_programs are called, Then the multi has 32 parts, as on the real sampler [RQ-MCP-044]",
          "[mcp][multistructure]")
{
    ToolRig rig;
    const json answer = rig.call("create_multi", {{"name", "STAGE"}});
    CHECK(hasText(toolText(answer), "with 32 parts"));
    CHECK(hasText(toolText(rig.call("get_part_programs")), "32 parts"));
}

TEST_CASE("Given a multi name that is taken or not acceptable, When create_multi or rename_multi is called, Then nothing is sent and the answer says why [RQ-MCP-037]",
          "[mcp][multistructure]")
{
    ToolRig rig;
    selectMulti(rig, "LIVE");
    for (const char* name : {"LIVE", "live", "Stu-dio"})
    {
        INFO(name);
        const json create = rig.call("create_multi", {{"name", name}});
        CHECK(toolFailed(create));
        CHECK(hasText(toolText(create), "already"));
    }
    const json rename = rig.call("rename_multi", {{"name", "STUDIO"}});
    CHECK(toolFailed(rename));
    CHECK(hasText(toolText(rename), "already"));
    for (const char* tool : {"create_multi", "rename_multi"})
    {
        CHECK(toolFailed(rig.call(tool)));
        CHECK(toolFailed(rig.call(tool, {{"name", ""}})));
        CHECK(toolFailed(rig.call(tool, {{"name", std::string(TOO_LONG_NAME_LENGTH, 'A')}})));
        CHECK(toolFailed(rig.call(tool, {{"name", 7}})));
        CHECK(toolFailed(rig.call(tool, {{"name", "OK"}, {"extra", 1}})));
    }
    CHECK(rig.accepted(akm::ItemId::MultiCreate) == 0);
    CHECK(rig.accepted(akm::ItemId::MultiRename) == 0);
}

TEST_CASE("Given LIVE is the current multi, When rename_multi gives LIVE2, Then it is renamed once and listed [RQ-MCP-037]",
          "[mcp][multistructure]")
{
    ToolRig rig;
    selectMulti(rig, "LIVE");
    const json answer = rig.call("rename_multi", {{"name", "LIVE2"}});
    CHECK_FALSE(toolFailed(answer));
    CHECK(hasText(toolText(answer), "Renamed the multi \"LIVE\" to \"LIVE2\""));
    CHECK(rig.accepted(akm::ItemId::MultiRename) == 1);
    const std::string list = toolText(rig.call("list_multis"));
    CHECK(hasText(list, "LIVE2"));
    CHECK_FALSE(hasText(list, "LIVE ("));
    CHECK_FALSE(hasText(list, ": LIVE\n"));
}

TEST_CASE("Given LIVE is the current multi, When delete_multi is called with confirm LIVE, Then only STUDIO remains [RQ-MCP-037, RQ-MCP-042]",
          "[mcp][multistructure]")
{
    ToolRig rig;
    selectMulti(rig, "LIVE");
    const json answer = rig.call("delete_multi", {{"confirm", "LIVE"}});
    CHECK_FALSE(toolFailed(answer));
    CHECK(hasText(toolText(answer), "Deleted the multi \"LIVE\""));
    CHECK(hasText(toolText(answer), "1 multi"));
    CHECK(rig.accepted(akm::ItemId::MultiDeleteCurrent) == 1);
    const std::string list = toolText(rig.call("list_multis"));
    CHECK(hasText(list, "STUDIO"));
    CHECK_FALSE(hasText(list, "LIVE"));
}

TEST_CASE("Given a missing or wrong confirm, When delete_multi or clear_part is called, Then nothing is sent and the answer names the current multi [RQ-MCP-042]",
          "[mcp][multistructure]")
{
    ToolRig rig;
    selectMulti(rig, "LIVE");
    for (const char* confirm : {"live", "STUDIO", "LIVE ", ""})
    {
        INFO(confirm);
        const json remove = rig.call("delete_multi", {{"confirm", confirm}});
        CHECK(toolFailed(remove));
        CHECK(hasText(toolText(remove), "\"LIVE\""));
        CHECK(hasText(toolText(remove), "nothing was deleted"));
        const json clear = rig.call("clear_part", {{"part", 2}, {"confirm", confirm}});
        CHECK(toolFailed(clear));
        CHECK(hasText(toolText(clear), "\"LIVE\""));
    }
    CHECK(toolFailed(rig.call("delete_multi")));
    CHECK(toolFailed(rig.call("clear_part", {{"part", 2}})));
    CHECK(toolFailed(rig.call("clear_part", {{"confirm", "LIVE"}})));
    CHECK(rig.accepted(akm::ItemId::MultiDeleteCurrent) == 0);
    CHECK(rig.accepted(akm::ItemId::MultiDeletePart) == 0);
}

TEST_CASE("Given no multi is current, When a tool that acts on the current multi is called, Then nothing is sent and the answer says to select one [RQ-MCP-037, RQ-MCP-038]",
          "[mcp][multistructure]")
{
    ToolRig rig;
    CHECK(hasText(toolText(rig.call("rename_multi", {{"name", "NEW"}})), "select_multi"));
    CHECK(hasText(toolText(rig.call("delete_multi", {{"confirm", "LIVE"}})), "select_multi"));
    CHECK(hasText(toolText(rig.call("set_part_program", {{"part", 1}, {"program", "BASS"}})), "select_multi"));
    CHECK(hasText(toolText(rig.call("clear_part", {{"part", 1}, {"confirm", "LIVE"}})), "select_multi"));
    CHECK(hasText(toolText(rig.call("set_multi_program_number", {{"number", 5}})), "select_multi"));
    CHECK(hasText(toolText(rig.call("get_part_programs")), "select_multi"));
    CHECK(rig.accepted(akm::ItemId::MultiRename) == 0);
    CHECK(rig.accepted(akm::ItemId::MultiDeleteCurrent) == 0);
    CHECK(rig.accepted(akm::ItemId::MultiSetPartByName) == 0);
}

TEST_CASE("Given LIVE is current, When set_part_program gives part 2 and the program LEAD, Then the part index 1 is assigned by name and read back [RQ-MCP-038]",
          "[mcp][multistructure]")
{
    ToolRig rig;
    selectMulti(rig, "LIVE");
    const json answer = rig.call("set_part_program", {{"part", 2}, {"program", "lead"}});
    CHECK_FALSE(toolFailed(answer));
    CHECK(hasText(toolText(answer), "part 2 of the multi \"LIVE\" now plays the program \"LEAD\""));
    const auto sent = rig.sentData(akm::ItemId::MultiSetPartByName);
    REQUIRE(sent.size() == 1);
    CHECK(sent.front().front() == 1);  // the part is numbered from 1 and sent minus one
    const std::string parts = toolText(rig.call("get_part_programs"));
    CHECK(hasText(parts, "part 2: LEAD"));
    CHECK_FALSE(hasText(parts, "part 1:"));
}

TEST_CASE("Given LIVE is current, When set_part_program gives a position, Then the part is assigned by index with the position of the list_programs [RQ-MCP-038]",
          "[mcp][multistructure]")
{
    ToolRig rig;
    selectMulti(rig, "LIVE");
    const json answer = rig.call("set_part_program", {{"part", 1}, {"position", 2}});
    CHECK_FALSE(toolFailed(answer));
    CHECK(hasText(toolText(answer), "part 1 of the multi \"LIVE\" now plays the program \"PAD\""));
    REQUIRE(rig.sentData(akm::ItemId::MultiSetPartByIndex).size() == 1);
    CHECK(rig.accepted(akm::ItemId::MultiSetPartByName) == 0);
}

TEST_CASE("Given a program or a part that does not exist, or arguments that do not say which program, When set_part_program is called, Then nothing is sent [RQ-MCP-038]",
          "[mcp][multistructure]")
{
    ToolRig rig;
    selectMulti(rig, "LIVE");
    const json unknown = rig.call("set_part_program", {{"part", 1}, {"program", "NOPE"}});
    CHECK(toolFailed(unknown));
    CHECK(hasText(toolText(unknown), "\"NOPE\""));
    CHECK(hasText(toolText(unknown), "\"BASS\""));
    CHECK(toolFailed(rig.call("set_part_program", {{"part", 1}, {"position", 3}})));
    CHECK(toolFailed(rig.call("set_part_program", {{"part", 1}, {"position", -1}})));
    CHECK(toolFailed(rig.call("set_part_program", {{"part", 0}, {"program", "BASS"}})));
    CHECK(toolFailed(rig.call("set_part_program", {{"part", PART_BEYOND_ANY_MULTI}, {"program", "BASS"}})));
    CHECK(toolFailed(rig.call("set_part_program", {{"part", 1}})));
    CHECK(toolFailed(rig.call("set_part_program", {{"part", 1}, {"program", "BASS"}, {"position", 0}})));
    CHECK(toolFailed(rig.call("set_part_program", {{"program", "BASS"}})));
    CHECK(toolFailed(rig.call("set_part_program", {{"part", "one"}, {"program", "BASS"}})));
    CHECK(rig.accepted(akm::ItemId::MultiSetPartByName) == 0);
    CHECK(rig.accepted(akm::ItemId::MultiSetPartByIndex) == 0);
}

TEST_CASE("Given part 2 plays LEAD, When clear_part is called for part 2 with confirm LIVE, Then the part plays nothing [RQ-MCP-038, RQ-MCP-042]",
          "[mcp][multistructure]")
{
    ToolRig rig;
    selectMulti(rig, "LIVE");
    REQUIRE_FALSE(toolFailed(rig.call("set_part_program", {{"part", 2}, {"program", "LEAD"}})));
    const json answer = rig.call("clear_part", {{"part", 2}, {"confirm", "LIVE"}});
    CHECK_FALSE(toolFailed(answer));
    CHECK(hasText(toolText(answer), "part 2 of the multi \"LIVE\" no longer plays \"LEAD\""));
    const auto sent = rig.sentData(akm::ItemId::MultiDeletePart);
    REQUIRE(sent.size() == 1);
    CHECK(sent.front().front() == 1);
    CHECK_FALSE(hasText(toolText(rig.call("get_part_programs")), "part 2:"));
}

TEST_CASE("Given a part that plays nothing, When clear_part is called for it, Then nothing is sent and the answer says it plays nothing [RQ-MCP-038]",
          "[mcp][multistructure]")
{
    ToolRig rig;
    selectMulti(rig, "LIVE");
    const json answer = rig.call("clear_part", {{"part", 3}, {"confirm", "LIVE"}});
    CHECK(toolFailed(answer));
    CHECK(hasText(toolText(answer), "plays no program"));
    CHECK(rig.accepted(akm::ItemId::MultiDeletePart) == 0);
}

TEST_CASE("Given LIVE is current, When set_multi_program_number gives 5 and then null, Then the number is set and then switched off, read back each time [RQ-MCP-038]",
          "[mcp][multistructure]")
{
    ToolRig rig;
    selectMulti(rig, "LIVE");
    const json set = rig.call("set_multi_program_number", {{"number", 5}});
    CHECK_FALSE(toolFailed(set));
    CHECK(hasText(toolText(set), "program number 5"));
    const json off = rig.call("set_multi_program_number", {{"number", nullptr}});
    CHECK_FALSE(toolFailed(off));
    CHECK(hasText(toolText(off), "no program number"));
    CHECK(rig.accepted(akm::ItemId::MultiSetProgramNumber) == 2);
    for (const json& bad : {json(0), json(129), json("five"), json(-3)})
    {
        INFO(bad.dump());
        CHECK(toolFailed(rig.call("set_multi_program_number", {{"number", bad}})));
    }
    CHECK(toolFailed(rig.call("set_multi_program_number")));
    CHECK(rig.accepted(akm::ItemId::MultiSetProgramNumber) == 2);
}

TEST_CASE("Given a multi whose parts play nothing, When get_part_programs is called, Then the answer says none plays a program [RQ-MCP-038]",
          "[mcp][multistructure]")
{
    ToolRig rig;
    selectMulti(rig, "LIVE");
    const std::string text = toolText(rig.call("get_part_programs"));
    CHECK(hasText(text, "\"LIVE\""));
    CHECK(hasText(text, "no part plays a program"));
}
