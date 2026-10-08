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

// Deleting a song file, a set list or a scenelist: delete_song_file, delete_set_list and delete_scenelist, each only with a `confirm` that is
// the exact name of what is deleted, over a real session and the simulated sampler. [TASK-MCP-048, RQ-MCP-049, RQ-MCP-042,
// ADR-MCP-005 (DEC-MCP-031), ADR-MCP-004 (DEC-MCP-023)]
#include <catch2/catch_test_macros.hpp>

#include <optional>
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
    constexpr const char* DELETE_SONG = "delete_song_file";
    constexpr const char* DELETE_SET_LIST = "delete_set_list";
    constexpr const char* DELETE_SCENELIST = "delete_scenelist";
    constexpr const char* SELECT_SONG = "select_song_file";
    constexpr const char* SELECT_SCENELIST = "select_scenelist";

    const std::vector<std::string> SONGS = {"INTRO", "VERSE", "OUTRO"};
    const std::vector<std::string> SET_LISTS = {"TOUR", "HOME"};
    const std::vector<std::string> SCENELISTS = {"LIVE SET", "STUDIO"};

    struct ListRig : ToolRig
    {
        ListRig()
        {
            sampler->setSongNames(SONGS);
            sampler->setSetListNames(SET_LISTS);
            sampler->setSceneListNames(SCENELISTS);
        }
    };

    json remove(ListRig& rig, const char* tool, const json& confirm)
    {
        return rig.call(tool, {{"confirm", confirm}});
    }

    json removeSetList(ListRig& rig, const std::string& name, const std::string& confirm)
    {
        return rig.call(DELETE_SET_LIST, {{"name", name}, {"confirm", confirm}});
    }
}

TEST_CASE("Given the server, When the tools are listed, Then the three delete tools declare themselves destructive and not idempotent [RQ-MCP-049, RQ-MCP-042]",
          "[mcp][deletelists]")
{
    ListRig rig;
    for (const char* name : {DELETE_SONG, DELETE_SET_LIST, DELETE_SCENELIST})
    {
        const json tool = rig.tool(name);
        INFO(name);
        REQUIRE_FALSE(tool.is_null());
        CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
        CHECK(tool["annotations"]["destructiveHint"].get<bool>());
        CHECK_FALSE(tool["annotations"]["idempotentHint"].get<bool>());
    }
}

TEST_CASE("Given the current song file VERSE, When delete_song_file is called with no confirm, a wrong one and the right one, Then the first two send nothing and name the confirm expected, and the third deletes it and answers the names that remain [RQ-MCP-049]",
          "[mcp][deletelists]")
{
    ListRig rig;
    REQUIRE_FALSE(toolFailed(rig.call(SELECT_SONG, {{"name", "VERSE"}})));

    const json missing = rig.call(DELETE_SONG);
    CHECK(toolFailed(missing));
    CHECK(hasText(toolText(missing), "confirm"));
    const json wrong = remove(rig, DELETE_SONG, "verse");
    CHECK(toolFailed(wrong));
    CHECK(hasText(toolText(wrong), "\"VERSE\""));
    CHECK(hasText(toolText(wrong), "nothing was deleted"));
    CHECK(rig.accepted(akm::ItemId::SongDeleteCurrent) == 0);
    CHECK(rig.sampler->songNames() == SONGS);

    const json deleted = remove(rig, DELETE_SONG, "VERSE");
    CHECK_FALSE(toolFailed(deleted));
    CHECK(hasText(toolText(deleted), "Deleted the song file \"VERSE\""));
    CHECK(hasText(toolText(deleted), "INTRO"));
    CHECK(hasText(toolText(deleted), "OUTRO"));
    CHECK(rig.accepted(akm::ItemId::SongDeleteCurrent) == 1);
    CHECK(rig.sampler->songNames() == std::vector<std::string>{"INTRO", "OUTRO"});
    CHECK_FALSE(rig.sampler->currentSong().has_value());
}

TEST_CASE("Given the current scenelist STUDIO, When delete_scenelist is called with no confirm, a wrong one and the right one, Then the first two send nothing and the third deletes it [RQ-MCP-049]",
          "[mcp][deletelists]")
{
    ListRig rig;
    REQUIRE_FALSE(toolFailed(rig.call(SELECT_SCENELIST, {{"name", "STUDIO"}})));
    CHECK(toolFailed(rig.call(DELETE_SCENELIST)));
    const json wrong = remove(rig, DELETE_SCENELIST, "LIVE SET");
    CHECK(toolFailed(wrong));
    CHECK(hasText(toolText(wrong), "\"STUDIO\""));
    CHECK(rig.accepted(akm::ItemId::SceneListDeleteCurrent) == 0);
    CHECK(rig.sampler->sceneListNames() == SCENELISTS);

    const json deleted = remove(rig, DELETE_SCENELIST, "STUDIO");
    CHECK_FALSE(toolFailed(deleted));
    CHECK(hasText(toolText(deleted), "Deleted the scenelist \"STUDIO\""));
    CHECK(hasText(toolText(deleted), "LIVE SET"));
    CHECK(rig.accepted(akm::ItemId::SceneListDeleteCurrent) == 1);
    CHECK(rig.sampler->sceneListNames() == std::vector<std::string>{"LIVE SET"});
}

TEST_CASE("Given no current song file or scenelist, When one is deleted, Then the answer says to select one first and nothing is sent [RQ-MCP-049]",
          "[mcp][deletelists]")
{
    ListRig rig;
    const json song = remove(rig, DELETE_SONG, "VERSE");
    CHECK(toolFailed(song));
    CHECK(hasText(toolText(song), SELECT_SONG));
    const json scenelist = remove(rig, DELETE_SCENELIST, "STUDIO");
    CHECK(toolFailed(scenelist));
    CHECK(hasText(toolText(scenelist), SELECT_SCENELIST));
    CHECK(rig.accepted(akm::ItemId::SongDeleteCurrent) == 0);
    CHECK(rig.accepted(akm::ItemId::SceneListDeleteCurrent) == 0);
}

TEST_CASE("Given the set lists TOUR and HOME, When delete_set_list is called for HOME with no confirm, a wrong one and the right one, Then the first two send nothing and name the confirm expected, and the third deletes it [RQ-MCP-049]",
          "[mcp][deletelists]")
{
    ListRig rig;
    const json missing = rig.call(DELETE_SET_LIST, {{"name", "HOME"}});
    CHECK(toolFailed(missing));
    CHECK(hasText(toolText(missing), "confirm"));
    const json wrong = removeSetList(rig, "HOME", "TOUR");
    CHECK(toolFailed(wrong));
    CHECK(hasText(toolText(wrong), "\"HOME\""));
    CHECK(hasText(toolText(wrong), "nothing was deleted"));
    CHECK(rig.accepted(akm::ItemId::SetListDelete) == 0);
    CHECK(rig.sampler->setListNames() == SET_LISTS);

    const json deleted = removeSetList(rig, "HOME", "HOME");
    CHECK_FALSE(toolFailed(deleted));
    CHECK(hasText(toolText(deleted), "Deleted the set list \"HOME\" (position 1)"));
    CHECK(hasText(toolText(deleted), "TOUR"));
    CHECK(rig.accepted(akm::ItemId::SetListDelete) == 1);
    CHECK(rig.sampler->setListNames() == std::vector<std::string>{"TOUR"});
}

TEST_CASE("Given a set list asked for in other letters, When it is deleted, Then confirm must still be its exact name, and an unknown or doubled name sends nothing [RQ-MCP-049]",
          "[mcp][deletelists]")
{
    ListRig rig;
    const json lower = removeSetList(rig, "home", "home");
    CHECK(toolFailed(lower));
    CHECK(hasText(toolText(lower), "\"HOME\""));
    CHECK(rig.accepted(akm::ItemId::SetListDelete) == 0);
    CHECK_FALSE(toolFailed(removeSetList(rig, "home", "HOME")));
    CHECK(rig.sampler->setListNames() == std::vector<std::string>{"TOUR"});

    const json unknown = removeSetList(rig, "MISSING", "MISSING");
    CHECK(toolFailed(unknown));
    CHECK(hasText(toolText(unknown), "No set list is named \"MISSING\""));

    rig.sampler->setSetListNames({"TWIN", "TWIN"});
    const json twin = removeSetList(rig, "TWIN", "TWIN");
    CHECK(toolFailed(twin));
    CHECK(hasText(toolText(twin), "positions 0 and 1"));
    CHECK(rig.sampler->setListNames() == std::vector<std::string>{"TWIN", "TWIN"});
    CHECK(rig.accepted(akm::ItemId::SetListDelete) == 1);
}

TEST_CASE("Given an unknown, a missing or a badly typed argument, When a delete tool is called, Then it is refused and nothing is sent [RQ-MCP-049]",
          "[mcp][deletelists]")
{
    ListRig rig;
    REQUIRE_FALSE(toolFailed(rig.call(SELECT_SONG, {{"name", "VERSE"}})));
    CHECK(toolFailed(remove(rig, DELETE_SONG, 5)));
    CHECK(toolFailed(rig.call(DELETE_SONG, {{"confirm", "VERSE"}, {"extra", 1}})));
    CHECK(toolFailed(remove(rig, DELETE_SCENELIST, 5)));
    CHECK(toolFailed(rig.call(DELETE_SCENELIST, {{"confirm", "STUDIO"}, {"extra", 1}})));
    CHECK(toolFailed(rig.call(DELETE_SET_LIST)));
    CHECK(toolFailed(rig.call(DELETE_SET_LIST, {{"confirm", "HOME"}})));
    CHECK(toolFailed(rig.call(DELETE_SET_LIST, {{"name", 1}, {"confirm", "HOME"}})));
    CHECK(toolFailed(rig.call(DELETE_SET_LIST, {{"name", "HOME"}, {"confirm", 1}})));
    CHECK(toolFailed(rig.call(DELETE_SET_LIST, {{"name", "HOME"}, {"confirm", "HOME"}, {"extra", 1}})));
    CHECK(rig.accepted(akm::ItemId::SongDeleteCurrent) == 0);
    CHECK(rig.accepted(akm::ItemId::SceneListDeleteCurrent) == 0);
    CHECK(rig.accepted(akm::ItemId::SetListDelete) == 0);
}
