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

// Clearing the sampler's memory: clear_sampler_memory, only with a `confirm` that is the total number of programs, samples and multis the
// sampler holds now, over a real session and the simulated sampler. Never run on a real sampler by a test. The song files, set lists and
// scenelists are not counted by `confirm` but are cleared too, as a real S5000 did (TASK-MCP-062, 2026-10-10). [TASK-MCP-051, TASK-MCP-062,
// RQ-MCP-052, RQ-MCP-042, ADR-MCP-005 (DEC-MCP-029, DEC-MCP-033)]
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
    constexpr const char* CLEAR = "clear_sampler_memory";
    constexpr int TOTAL = 4;

    /// A rig whose sampler holds 2 programs, 1 sample and 1 multi (4 in all), and also 3 song files, 2 set lists and 2 scenelists.
    struct SmallMemoryRig : ToolRig
    {
        SmallMemoryRig()
        {
            sampler->setSampleNames({"KICK"});
            sampler->setMultiNames({"LIVE"});
            sampler->setSongNames({"INTRO", "VERSE", "OUTRO"});
            sampler->setSetListNames({"TOUR", "HOME"});
            sampler->setSceneListNames({"LIVE SET", "STUDIO"});
            REQUIRE_FALSE(toolFailed(call("select_program", {{"name", "LEAD"}})));
            REQUIRE_FALSE(toolFailed(call("delete_program", {{"confirm", "LEAD"}})));
        }
    };

    json clear(ToolRig& rig, const json& confirm)
    {
        return rig.call(CLEAR, {{"confirm", confirm}});
    }
}

TEST_CASE("Given the server, When the tools are listed, Then clear_sampler_memory declares itself destructive and not idempotent [RQ-MCP-052, RQ-MCP-042]",
          "[mcp][clearmemory]")
{
    ToolRig rig;
    const json tool = rig.tool(CLEAR);
    REQUIRE_FALSE(tool.is_null());
    CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
    CHECK(tool["annotations"]["destructiveHint"].get<bool>());
    CHECK_FALSE(tool["annotations"]["idempotentHint"].get<bool>());
}

TEST_CASE("Given 2 programs, 1 sample and 1 multi, When clear_sampler_memory is called with no confirm, with 3 and with 4, Then the first two send nothing and the answer says 4, and the third empties the three lists [RQ-MCP-052]",
          "[mcp][clearmemory]")
{
    SmallMemoryRig rig;
    const json missing = rig.call(CLEAR);
    CHECK(toolFailed(missing));
    CHECK(hasText(toolText(missing), "confirm"));
    const json wrong = clear(rig, TOTAL - 1);
    CHECK(toolFailed(wrong));
    CHECK(hasText(toolText(wrong), "holds 4 items"));
    CHECK(hasText(toolText(wrong), "2 programs, 1 sample and 1 multi"));
    CHECK(hasText(toolText(wrong), "nothing was cleared"));
    // What the count does not cover is said before anything is sent: the three lists go too.
    CHECK(hasText(toolText(wrong), "also clears the song files, set lists and scenelists"));
    CHECK(hasText(toolText(wrong), "3 song files, 2 set lists and 2 scenelists"));
    CHECK(rig.accepted(akm::ItemId::SystemClearMemory) == 0);
    CHECK(hasText(toolText(rig.call("list_programs")), "BASS"));

    const json cleared = clear(rig, TOTAL);
    CHECK_FALSE(toolFailed(cleared));
    CHECK(hasText(toolText(cleared), "Cleared the sampler's memory"));
    CHECK(hasText(toolText(cleared), "2 programs, 1 sample and 1 multi"));
    CHECK(hasText(toolText(cleared), "went too"));
    CHECK(hasText(toolText(cleared), "3 song files, 2 set lists and 2 scenelists are lost"));
    CHECK_FALSE(hasText(toolText(cleared), "still held"));
    CHECK(rig.accepted(akm::ItemId::SystemClearMemory) == 1);
    CHECK(hasText(toolText(rig.call("list_programs")), "The sampler holds no program."));
    CHECK(hasText(toolText(rig.call("list_samples")), "The sampler holds no sample."));
    CHECK(hasText(toolText(rig.call("list_multis")), "The sampler holds no multi."));
}

TEST_CASE("Given the memory was cleared, When clear_sampler_memory is called again, Then nothing is sent and the answer says the memory holds none [RQ-MCP-052]",
          "[mcp][clearmemory]")
{
    SmallMemoryRig rig;
    REQUIRE_FALSE(toolFailed(clear(rig, TOTAL)));
    const json again = clear(rig, TOTAL);
    CHECK(toolFailed(again));
    CHECK(hasText(toolText(again), "holds no program, sample or multi"));
    CHECK(rig.accepted(akm::ItemId::SystemClearMemory) == 1);
}

TEST_CASE("Given song files, set lists and scenelists, When the memory is cleared, Then they are cleared too although the total counts programs, samples and multis only, as a real S5000 did [RQ-MCP-052, TASK-MCP-062]",
          "[mcp][clearmemory]")
{
    SmallMemoryRig rig;
    REQUIRE_FALSE(toolFailed(clear(rig, TOTAL)));
    CHECK(hasText(toolText(rig.call("list_song_files")), "The sampler holds no song file."));
    CHECK(hasText(toolText(rig.call("list_set_lists")), "The sampler holds no set list."));
    CHECK(hasText(toolText(rig.call("list_scenelists")), "The sampler holds no scenelist."));
    CHECK(rig.sampler->songNames().empty());
    CHECK(rig.sampler->setListNames().empty());
    CHECK(rig.sampler->sceneListNames().empty());
    CHECK_FALSE(rig.sampler->currentSong().has_value());
    CHECK_FALSE(rig.sampler->currentSceneList().has_value());
}

TEST_CASE("Given a memory with no song file, set list or scenelist, When it is cleared, Then the answers do not speak of them [RQ-MCP-052, TASK-MCP-062]",
          "[mcp][clearmemory]")
{
    ToolRig rig;
    const std::string held = toolText(rig.call("list_programs"));
    REQUIRE(hasText(held, "BASS"));
    const json wrong = clear(rig, 1);
    CHECK(toolFailed(wrong));
    CHECK_FALSE(hasText(toolText(wrong), "song files"));
    const int total = 3 + 3 + 2;  // PAD, BASS and LEAD, KICK, SNARE and PAD, LIVE and STUDIO
    const json cleared = clear(rig, total);
    CHECK_FALSE(toolFailed(cleared));
    CHECK_FALSE(hasText(toolText(cleared), "song files"));
    CHECK_FALSE(hasText(toolText(cleared), "went too"));
}

TEST_CASE("Given a confirm that is not a whole number from 1, or an unknown argument, When clear_sampler_memory is called, Then it is refused and nothing is sent [RQ-MCP-052]",
          "[mcp][clearmemory]")
{
    SmallMemoryRig rig;
    CHECK(toolFailed(clear(rig, "4")));
    CHECK(toolFailed(clear(rig, 0)));
    CHECK(toolFailed(clear(rig, -4)));
    CHECK(toolFailed(clear(rig, 4.5)));
    CHECK(toolFailed(clear(rig, true)));
    CHECK(toolFailed(rig.call(CLEAR, {{"confirm", TOTAL}, {"extra", 1}})));
    CHECK(rig.accepted(akm::ItemId::SystemClearMemory) == 0);
}
