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

// Clearing the sampler's memory: clear_sampler_memory, only with a `confirm` that is the total number of programs, samples, multis, song files,
// set lists and scenelists the sampler holds now, over a real session and the simulated sampler. Never run on a real sampler by a test. The song
// files, set lists and scenelists are cleared by the command, as a real S5000 showed (TASK-MCP-062, 2026-10-10), and the owner decided the same day
// that `confirm` counts them (TASK-MCP-068). [TASK-MCP-051, TASK-MCP-062, TASK-MCP-068, RQ-MCP-052, RQ-MCP-042, ADR-MCP-005 (DEC-MCP-029,
// DEC-MCP-033)]
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
    constexpr int MEMORY_ITEMS = 4;  // 2 programs, 1 sample and 1 multi: what `confirm` counted before the owner's decision of 2026-10-10
    constexpr int LIST_ITEMS = 7;    // 3 song files, 2 set lists and 2 scenelists
    constexpr int TOTAL = MEMORY_ITEMS + LIST_ITEMS;

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

TEST_CASE("Given 2 programs, 1 sample, 1 multi, 3 song files, 2 set lists and 2 scenelists, When clear_sampler_memory is called with no confirm, with 4 (the old count) and with 11, Then the first two send nothing and the answer says 11 with the six counts, and the third empties everything [RQ-MCP-052, TASK-MCP-068]",
          "[mcp][clearmemory]")
{
    SmallMemoryRig rig;
    const json missing = rig.call(CLEAR);
    CHECK(toolFailed(missing));
    CHECK(hasText(toolText(missing), "confirm"));
    const json wrong = clear(rig, MEMORY_ITEMS);
    CHECK(toolFailed(wrong));
    CHECK(hasText(toolText(wrong), "holds 11 items"));
    CHECK(hasText(toolText(wrong), "2 programs, 1 sample, 1 multi, 3 song files, 2 set lists and 2 scenelists"));
    CHECK(hasText(toolText(wrong), "nothing was cleared"));
    CHECK(rig.accepted(akm::ItemId::SystemClearMemory) == 0);
    CHECK(hasText(toolText(rig.call("list_programs")), "BASS"));

    const json cleared = clear(rig, TOTAL);
    CHECK_FALSE(toolFailed(cleared));
    CHECK(hasText(toolText(cleared), "Cleared the sampler's memory"));
    CHECK(hasText(toolText(cleared), "2 programs, 1 sample, 1 multi, 3 song files, 2 set lists and 2 scenelists are gone (11 items)"));
    CHECK_FALSE(hasText(toolText(cleared), "went too"));
    CHECK_FALSE(hasText(toolText(cleared), "still held"));
    CHECK(rig.accepted(akm::ItemId::SystemClearMemory) == 1);
    CHECK(hasText(toolText(rig.call("list_programs")), "The sampler holds no program."));
    CHECK(hasText(toolText(rig.call("list_samples")), "The sampler holds no sample."));
    CHECK(hasText(toolText(rig.call("list_multis")), "The sampler holds no multi."));
    CHECK(hasText(toolText(rig.call("list_song_files")), "The sampler holds no song file."));
}

TEST_CASE("Given the memory was cleared, When clear_sampler_memory is called again, Then nothing is sent and the answer says the memory holds none [RQ-MCP-052]",
          "[mcp][clearmemory]")
{
    SmallMemoryRig rig;
    REQUIRE_FALSE(toolFailed(clear(rig, TOTAL)));
    const json again = clear(rig, TOTAL);
    CHECK(toolFailed(again));
    CHECK(hasText(toolText(again), "holds no program, sample, multi, song file, set list or scenelist"));
    CHECK(rig.accepted(akm::ItemId::SystemClearMemory) == 1);
}

TEST_CASE("Given song files, set lists and scenelists, When the memory is cleared, Then they are cleared too, as a real S5000 did, and the total counts them [RQ-MCP-052, TASK-MCP-062, TASK-MCP-068]",
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

TEST_CASE("Given a memory with no song file, set list or scenelist, When it is cleared, Then the total is the programs, samples and multis and the answers do not speak of the lists [RQ-MCP-052, TASK-MCP-062, TASK-MCP-068]",
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

TEST_CASE("Given a memory that holds song files, set lists and scenelists and no program, sample or multi, When clear_sampler_memory is called with their number, Then they are cleared; the count is the number of them [RQ-MCP-052, TASK-MCP-068]",
          "[mcp][clearmemory]")
{
    SmallMemoryRig rig;
    REQUIRE_FALSE(toolFailed(rig.call("delete_all_programs", {{"confirm", 2}})));
    REQUIRE_FALSE(toolFailed(rig.call("delete_all_samples", {{"confirm", 1}})));
    REQUIRE_FALSE(toolFailed(rig.call("delete_all_multis", {{"confirm", 1}})));

    const json wrong = clear(rig, MEMORY_ITEMS);
    CHECK(toolFailed(wrong));
    CHECK(hasText(toolText(wrong), "holds 7 items"));
    CHECK(hasText(toolText(wrong), "3 song files, 2 set lists and 2 scenelists"));
    CHECK(rig.accepted(akm::ItemId::SystemClearMemory) == 0);

    const json cleared = clear(rig, LIST_ITEMS);
    CHECK_FALSE(toolFailed(cleared));
    CHECK(rig.accepted(akm::ItemId::SystemClearMemory) == 1);
    CHECK(hasText(toolText(rig.call("list_set_lists")), "The sampler holds no set list."));
}
