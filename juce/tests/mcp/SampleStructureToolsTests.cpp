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

// The sample structure tools of the MCP server: rename_sample and delete_sample (confirmed by the sample's name), over a real
// session and the simulated sampler. [TASK-MCP-031, RQ-MCP-036, RQ-MCP-042, ADR-MCP-004 (DEC-MCP-023, DEC-MCP-024)]
#include <catch2/catch_test_macros.hpp>

#include <string>

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

    void selectSample(ToolRig& rig, const char* name)
    {
        REQUIRE_FALSE(toolFailed(rig.call("select_sample", {{"name", name}})));
    }
}

TEST_CASE("Given the server, When the tools are listed, Then rename_sample is not destructive and delete_sample is [RQ-MCP-036, RQ-MCP-042]",
          "[mcp][samplestructure]")
{
    ToolRig rig;
    const json rename = rig.tool("rename_sample");
    const json remove = rig.tool("delete_sample");
    REQUIRE_FALSE(rename.is_null());
    REQUIRE_FALSE(remove.is_null());
    CHECK_FALSE(rename["annotations"]["readOnlyHint"].get<bool>());
    CHECK_FALSE(rename["annotations"]["destructiveHint"].get<bool>());
    CHECK_FALSE(remove["annotations"]["readOnlyHint"].get<bool>());
    CHECK(remove["annotations"]["destructiveHint"].get<bool>());
}

TEST_CASE("Given KICK is the current sample, When rename_sample gives KICK2, Then the sample is renamed once and the list shows it [RQ-MCP-036]",
          "[mcp][samplestructure]")
{
    ToolRig rig;
    selectSample(rig, "KICK");
    const json answer = rig.call("rename_sample", {{"name", "KICK2"}});
    CHECK_FALSE(toolFailed(answer));
    CHECK(hasText(toolText(answer), "Renamed the sample \"KICK\" to \"KICK2\""));
    CHECK(rig.accepted(akm::ItemId::SampleRenameCurrent) == 1);
    const std::string list = toolText(rig.call("list_samples"));
    CHECK(hasText(list, "KICK2"));
    CHECK(hasText(list, "SNARE"));
    CHECK_FALSE(hasText(list, "KICK ("));
    CHECK_FALSE(hasText(list, ": KICK\n"));
}

TEST_CASE("Given a name another sample bears, When rename_sample is called, Then nothing is sent and the answer names the sample [RQ-MCP-036]",
          "[mcp][samplestructure]")
{
    ToolRig rig;
    selectSample(rig, "KICK");
    for (const char* name : {"SNARE", "snare", "Sn-are"})
    {
        const json answer = rig.call("rename_sample", {{"name", name}});
        INFO(name);
        CHECK(toolFailed(answer));
        CHECK(hasText(toolText(answer), "already"));
        CHECK(hasText(toolText(answer), "\"SNARE\""));
    }
    CHECK(rig.accepted(akm::ItemId::SampleRenameCurrent) == 0);
}

TEST_CASE("Given a missing, empty, too long, non-string or non-ASCII name, When rename_sample is called, Then nothing is sent [RQ-MCP-036]",
          "[mcp][samplestructure]")
{
    ToolRig rig;
    selectSample(rig, "KICK");
    CHECK(toolFailed(rig.call("rename_sample")));
    CHECK(toolFailed(rig.call("rename_sample", {{"name", ""}})));
    CHECK(toolFailed(rig.call("rename_sample", {{"name", std::string(TOO_LONG_NAME_LENGTH, 'A')}})));
    CHECK(toolFailed(rig.call("rename_sample", {{"name", 5}})));
    CHECK(toolFailed(rig.call("rename_sample", {{"name", "caf\xc3\xa9"}})));
    CHECK(toolFailed(rig.call("rename_sample", {{"name", "OK"}, {"extra", 1}})));
    CHECK(rig.accepted(akm::ItemId::SampleRenameCurrent) == 0);
}

TEST_CASE("Given no sample is current, When rename_sample or delete_sample is called, Then nothing is sent and the answer says to select one [RQ-MCP-036]",
          "[mcp][samplestructure]")
{
    ToolRig rig;
    const json rename = rig.call("rename_sample", {{"name", "NEW"}});
    CHECK(toolFailed(rename));
    CHECK(hasText(toolText(rename), "select_sample"));
    const json remove = rig.call("delete_sample", {{"confirm", "KICK"}});
    CHECK(toolFailed(remove));
    CHECK(hasText(toolText(remove), "select_sample"));
    CHECK(rig.accepted(akm::ItemId::SampleRenameCurrent) == 0);
    CHECK(rig.accepted(akm::ItemId::SampleDeleteCurrent) == 0);
}

TEST_CASE("Given KICK is the current sample, When delete_sample is called with confirm KICK, Then only SNARE and PAD remain [RQ-MCP-036, RQ-MCP-042]",
          "[mcp][samplestructure]")
{
    ToolRig rig;
    selectSample(rig, "KICK");
    const json answer = rig.call("delete_sample", {{"confirm", "KICK"}});
    CHECK_FALSE(toolFailed(answer));
    CHECK(hasText(toolText(answer), "Deleted the sample \"KICK\""));
    CHECK(hasText(toolText(answer), "2 samples"));
    CHECK(rig.accepted(akm::ItemId::SampleDeleteCurrent) == 1);
    const std::string list = toolText(rig.call("list_samples"));
    CHECK(hasText(list, "SNARE"));
    CHECK(hasText(list, "PAD"));
    CHECK_FALSE(hasText(list, "KICK"));
}

TEST_CASE("Given a missing or wrong confirm, When delete_sample is called, Then nothing is sent and the answer names the current sample [RQ-MCP-042]",
          "[mcp][samplestructure]")
{
    ToolRig rig;
    selectSample(rig, "KICK");
    for (const char* confirm : {"kick", "SNARE", "KICK ", ""})
    {
        const json answer = rig.call("delete_sample", {{"confirm", confirm}});
        INFO(confirm);
        CHECK(toolFailed(answer));
        CHECK(hasText(toolText(answer), "\"KICK\""));
        CHECK(hasText(toolText(answer), "nothing was deleted"));
    }
    CHECK(toolFailed(rig.call("delete_sample")));
    CHECK(toolFailed(rig.call("delete_sample", {{"confirm", 3}})));
    CHECK(rig.accepted(akm::ItemId::SampleDeleteCurrent) == 0);
}
