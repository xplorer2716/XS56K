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

// Deleting every program, every sample or every multi: delete_all_programs, delete_all_samples and delete_all_multis, each only with a
// `confirm` that is the number of items of that kind the sampler holds now, over a real session and the simulated sampler holding the
// programs PAD, BASS and LEAD, the samples KICK, SNARE and PAD and the multis LIVE and STUDIO. [TASK-MCP-050, RQ-MCP-051, RQ-MCP-042,
// ADR-MCP-005 (DEC-MCP-029)]
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
    struct Kind
    {
        const char* deleteTool;
        const char* listTool;
        akm::ItemId item;
        int count;
        const char* noun;
    };

    const Kind PROGRAMS{"delete_all_programs", "list_programs", akm::ItemId::ProgramDeleteAll, 3, "program"};
    const Kind SAMPLES{"delete_all_samples", "list_samples", akm::ItemId::SampleDeleteAll, 3, "sample"};
    const Kind MULTIS{"delete_all_multis", "list_multis", akm::ItemId::MultiDeleteAll, 2, "multi"};

    json deleteAll(ToolRig& rig, const Kind& kind, const json& confirm)
    {
        return rig.call(kind.deleteTool, {{"confirm", confirm}});
    }
}

TEST_CASE("Given the server, When the tools are listed, Then the three delete-all tools declare themselves destructive and not idempotent [RQ-MCP-051, RQ-MCP-042]",
          "[mcp][bulkdelete]")
{
    ToolRig rig;
    for (const Kind& kind : {PROGRAMS, SAMPLES, MULTIS})
    {
        const json tool = rig.tool(kind.deleteTool);
        INFO(kind.deleteTool);
        REQUIRE_FALSE(tool.is_null());
        CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
        CHECK(tool["annotations"]["destructiveHint"].get<bool>());
        CHECK_FALSE(tool["annotations"]["idempotentHint"].get<bool>());
    }
}

TEST_CASE("Given the sampler holds N items of a kind, When its delete-all tool is called with no confirm, a wrong count and the right count, Then the first two send nothing and the answer gives N, and the third deletes them all and the list says the sampler holds none [RQ-MCP-051]",
          "[mcp][bulkdelete]")
{
    for (const Kind& kind : {PROGRAMS, SAMPLES, MULTIS})
    {
        ToolRig rig;
        INFO(kind.deleteTool);
        const json missing = rig.call(kind.deleteTool);
        CHECK(toolFailed(missing));
        CHECK(hasText(toolText(missing), "confirm"));
        const json wrong = deleteAll(rig, kind, kind.count - 1);
        CHECK(toolFailed(wrong));
        CHECK(hasText(toolText(wrong), (std::to_string(kind.count) + " ").c_str()));
        CHECK(hasText(toolText(wrong), "nothing was deleted"));
        CHECK(rig.accepted(kind.item) == 0);

        const json deleted = deleteAll(rig, kind, kind.count);
        CHECK_FALSE(toolFailed(deleted));
        CHECK(hasText(toolText(deleted), (std::string("Deleted all ") + std::to_string(kind.count) + " " + kind.noun).c_str()));
        CHECK(hasText(toolText(deleted), (std::string("holds no ") + kind.noun).c_str()));
        CHECK(rig.accepted(kind.item) == 1);
        CHECK(hasText(toolText(rig.call(kind.listTool)), (std::string("The sampler holds no ") + kind.noun + ".").c_str()));
    }
}

TEST_CASE("Given the programs were deleted, When the other kinds are asked for, Then they are still held: a tool deletes its own kind only [RQ-MCP-051]",
          "[mcp][bulkdelete]")
{
    ToolRig rig;
    REQUIRE_FALSE(toolFailed(deleteAll(rig, PROGRAMS, PROGRAMS.count)));
    CHECK(hasText(toolText(rig.call(SAMPLES.listTool)), "KICK"));
    CHECK(hasText(toolText(rig.call(MULTIS.listTool)), "LIVE"));
    CHECK(rig.accepted(SAMPLES.item) == 0);
    CHECK(rig.accepted(MULTIS.item) == 0);
}

TEST_CASE("Given the sampler holds none of a kind, When its delete-all tool is called, Then nothing is sent and the answer says it holds none [RQ-MCP-051]",
          "[mcp][bulkdelete]")
{
    ToolRig rig;
    REQUIRE_FALSE(toolFailed(deleteAll(rig, MULTIS, MULTIS.count)));
    const json again = deleteAll(rig, MULTIS, MULTIS.count);
    CHECK(toolFailed(again));
    CHECK(hasText(toolText(again), "holds no multi"));
    CHECK(rig.accepted(MULTIS.item) == 1);
}

TEST_CASE("Given a confirm that is not a whole number of at least 1, or an unknown argument, When a delete-all tool is called, Then it is refused and nothing is sent [RQ-MCP-051]",
          "[mcp][bulkdelete]")
{
    ToolRig rig;
    for (const Kind& kind : {PROGRAMS, SAMPLES, MULTIS})
    {
        INFO(kind.deleteTool);
        CHECK(toolFailed(deleteAll(rig, kind, "3")));
        CHECK(toolFailed(deleteAll(rig, kind, 0)));
        CHECK(toolFailed(deleteAll(rig, kind, -1)));
        CHECK(toolFailed(deleteAll(rig, kind, 2.5)));
        CHECK(toolFailed(deleteAll(rig, kind, true)));
        CHECK(toolFailed(rig.call(kind.deleteTool, {{"confirm", kind.count}, {"extra", 1}})));
        CHECK(rig.accepted(kind.item) == 0);
    }
}
