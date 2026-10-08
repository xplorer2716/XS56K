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

// The song files, the set lists and the scenelists: list, select and rename, over a real session and the simulated sampler holding
// two song files and more, two set lists and two scenelists. [TASK-MCP-047, RQ-MCP-048, ADR-MCP-005 (DEC-MCP-031)]
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
    constexpr const char* LIST_SONGS = "list_song_files";
    constexpr const char* SELECT_SONG = "select_song_file";
    constexpr const char* RENAME_SONG = "rename_song_file";
    constexpr const char* LIST_SET_LISTS = "list_set_lists";
    constexpr const char* RENAME_SET_LIST = "rename_set_list";
    constexpr const char* LIST_SCENELISTS = "list_scenelists";
    constexpr const char* SELECT_SCENELIST = "select_scenelist";
    constexpr const char* RENAME_SCENELIST = "rename_scenelist";
    constexpr std::size_t MAX_LIST_NAME_LENGTH = 20;

    const std::vector<std::string> SONGS = {"INTRO", "VERSE", "OUTRO"};
    const std::vector<std::string> SET_LISTS = {"TOUR", "HOME"};
    const std::vector<std::string> SCENELISTS = {"LIVE SET", "STUDIO"};

    /// The tool rig with a sampler that also holds song files, set lists and scenelists, in that order.
    struct ListRig : ToolRig
    {
        ListRig()
        {
            sampler->setSongNames(SONGS);
            sampler->setSetListNames(SET_LISTS);
            sampler->setSceneListNames(SCENELISTS);
        }
    };

    json byName(ListRig& rig, const char* tool, const std::string& name)
    {
        return rig.call(tool, {{"name", name}});
    }

    json byIndex(ListRig& rig, const char* tool, const json& index)
    {
        return rig.call(tool, {{"index", index}});
    }
}

TEST_CASE("Given the server, When the tools are listed, Then the list tools read, and the select and rename tools change the sampler without deleting anything [RQ-MCP-048, RQ-MCP-013]",
          "[mcp][namedlists]")
{
    ListRig rig;
    for (const char* name : {LIST_SONGS, LIST_SET_LISTS, LIST_SCENELISTS})
    {
        const json tool = rig.tool(name);
        INFO(name);
        REQUIRE_FALSE(tool.is_null());
        CHECK(tool["annotations"]["readOnlyHint"].get<bool>());
    }
    for (const char* name : {SELECT_SONG, RENAME_SONG, RENAME_SET_LIST, SELECT_SCENELIST, RENAME_SCENELIST})
    {
        const json tool = rig.tool(name);
        INFO(name);
        REQUIRE_FALSE(tool.is_null());
        CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
        CHECK_FALSE(tool["annotations"]["destructiveHint"].get<bool>());
        CHECK(tool["annotations"]["idempotentHint"].get<bool>());
    }
}

TEST_CASE("Given three song files with none selected, When they are listed, Then the names come in order and the answer says none is selected; after a selection the current one is marked [RQ-MCP-048]",
          "[mcp][namedlists]")
{
    ListRig rig;
    const std::string before = toolText(rig.call(LIST_SONGS));
    CHECK(hasText(before, "Song files in memory (3)"));
    CHECK(hasText(before, "0: INTRO\n1: VERSE\n2: OUTRO\n"));
    CHECK(hasText(before, "No song file is selected"));
    CHECK_FALSE(hasText(before, "(current)"));

    REQUIRE_FALSE(toolFailed(byName(rig, SELECT_SONG, "VERSE")));
    const std::string after = toolText(rig.call(LIST_SONGS));
    CHECK(hasText(after, "1: VERSE (current)\n"));
    CHECK_FALSE(hasText(after, "No song file is selected"));
}

TEST_CASE("Given two scenelists, When they are listed before and after a selection, Then the names come in order and the current one is marked [RQ-MCP-048]",
          "[mcp][namedlists]")
{
    ListRig rig;
    const std::string before = toolText(rig.call(LIST_SCENELISTS));
    CHECK(hasText(before, "Scenelists in memory (2)"));
    CHECK(hasText(before, "0: LIVE SET\n1: STUDIO\n"));
    CHECK(hasText(before, "No scenelist is selected"));

    REQUIRE_FALSE(toolFailed(byIndex(rig, SELECT_SCENELIST, 1)));
    CHECK(hasText(toolText(rig.call(LIST_SCENELISTS)), "1: STUDIO (current)\n"));
}

TEST_CASE("Given two set lists, When they are listed, Then the names come in order and no current one is spoken of, the sampler having none [RQ-MCP-048]",
          "[mcp][namedlists]")
{
    ListRig rig;
    const std::string text = toolText(rig.call(LIST_SET_LISTS));
    CHECK(hasText(text, "Set lists in memory (2)"));
    CHECK(hasText(text, "0: TOUR\n1: HOME\n"));
    CHECK_FALSE(hasText(text, "current"));
    CHECK_FALSE(hasText(text, "selected"));
}

TEST_CASE("Given a sampler that holds none, When each list is asked for, Then the answer says it holds none [RQ-MCP-048]",
          "[mcp][namedlists]")
{
    ToolRig rig;
    CHECK(hasText(toolText(rig.call(LIST_SONGS)), "The sampler holds no song file."));
    CHECK(hasText(toolText(rig.call(LIST_SET_LISTS)), "The sampler holds no set list."));
    CHECK(hasText(toolText(rig.call(LIST_SCENELISTS)), "The sampler holds no scenelist."));
}

TEST_CASE("Given a song file and a scenelist, When each is selected by name and by index, Then the simulated sampler holds the selection and the answer gives its name and position [RQ-MCP-048]",
          "[mcp][namedlists]")
{
    ListRig rig;
    const json song = byName(rig, SELECT_SONG, "OUTRO");
    CHECK_FALSE(toolFailed(song));
    CHECK(hasText(toolText(song), "\"OUTRO\""));
    CHECK(hasText(toolText(song), "position 2"));
    CHECK(rig.sampler->currentSong() == std::optional<std::size_t>(2));
    CHECK_FALSE(toolFailed(byIndex(rig, SELECT_SONG, 0)));
    CHECK(rig.sampler->currentSong() == std::optional<std::size_t>(0));

    const json scenelist = byName(rig, SELECT_SCENELIST, "STUDIO");
    CHECK_FALSE(toolFailed(scenelist));
    CHECK(hasText(toolText(scenelist), "\"STUDIO\""));
    CHECK(rig.sampler->currentSceneList() == std::optional<std::size_t>(1));
    CHECK_FALSE(toolFailed(byIndex(rig, SELECT_SCENELIST, 0)));
    CHECK(rig.sampler->currentSceneList() == std::optional<std::size_t>(0));

    CHECK(rig.accepted(akm::ItemId::SongSelectByName) == 1);
    CHECK(rig.accepted(akm::ItemId::SongSelectByIndex) == 1);
    CHECK(rig.accepted(akm::ItemId::SceneListSelectByName) == 1);
    CHECK(rig.accepted(akm::ItemId::SceneListSelectByIndex) == 1);
}

TEST_CASE("Given a name or a position that no song file or scenelist has, When it is selected, Then the answer says which and points to the list, and the selection is unchanged [RQ-MCP-048]",
          "[mcp][namedlists]")
{
    ListRig rig;
    const json unknownSong = byName(rig, SELECT_SONG, "MISSING");
    CHECK(toolFailed(unknownSong));
    CHECK(hasText(toolText(unknownSong), "No song file is named \"MISSING\""));
    CHECK(hasText(toolText(unknownSong), LIST_SONGS));
    const json farSong = byIndex(rig, SELECT_SONG, 3);
    CHECK(toolFailed(farSong));
    CHECK(hasText(toolText(farSong), "No song file is at position 3"));
    CHECK_FALSE(rig.sampler->currentSong().has_value());

    const json unknownScenelist = byName(rig, SELECT_SCENELIST, "MISSING");
    CHECK(toolFailed(unknownScenelist));
    CHECK(hasText(toolText(unknownScenelist), "No scenelist is named \"MISSING\""));
    CHECK(toolFailed(byIndex(rig, SELECT_SCENELIST, 2)));
    CHECK_FALSE(rig.sampler->currentSceneList().has_value());
}

TEST_CASE("Given a missing, a double, an unknown or a badly typed argument, When a select tool is called, Then it is refused and nothing is sent [RQ-MCP-048]",
          "[mcp][namedlists]")
{
    ListRig rig;
    for (const char* tool : {SELECT_SONG, SELECT_SCENELIST})
    {
        INFO(tool);
        CHECK(toolFailed(rig.call(tool)));
        CHECK(toolFailed(rig.call(tool, {{"name", "INTRO"}, {"index", 0}})));
        CHECK(toolFailed(rig.call(tool, {{"name", 5}})));
        CHECK(toolFailed(rig.call(tool, {{"index", "0"}})));
        CHECK(toolFailed(rig.call(tool, {{"index", -1}})));
        CHECK(toolFailed(rig.call(tool, {{"index", 16384}})));
        CHECK(toolFailed(rig.call(tool, {{"name", "INTRO"}, {"extra", 1}})));
    }
    CHECK(rig.accepted(akm::ItemId::SongSelectByName) == 0);
    CHECK(rig.accepted(akm::ItemId::SongSelectByIndex) == 0);
    CHECK(rig.accepted(akm::ItemId::SceneListSelectByName) == 0);
    CHECK(rig.accepted(akm::ItemId::SceneListSelectByIndex) == 0);
}

TEST_CASE("Given the current song file and the current scenelist, When each is renamed, Then the simulated sampler holds the new name, the answer gives the old and the new, and the name read back is the new one [RQ-MCP-048]",
          "[mcp][namedlists]")
{
    ListRig rig;
    REQUIRE_FALSE(toolFailed(byName(rig, SELECT_SONG, "VERSE")));
    const json song = byName(rig, RENAME_SONG, "VERSE TWO");
    CHECK_FALSE(toolFailed(song));
    CHECK(hasText(toolText(song), "Renamed the song file \"VERSE\" to \"VERSE TWO\"."));
    CHECK(rig.sampler->songNames() == std::vector<std::string>{"INTRO", "VERSE TWO", "OUTRO"});
    CHECK(hasText(toolText(rig.call(LIST_SONGS)), "1: VERSE TWO (current)"));

    REQUIRE_FALSE(toolFailed(byName(rig, SELECT_SCENELIST, "LIVE SET")));
    const json scenelist = byName(rig, RENAME_SCENELIST, "GIG");
    CHECK_FALSE(toolFailed(scenelist));
    CHECK(hasText(toolText(scenelist), "Renamed the scenelist \"LIVE SET\" to \"GIG\"."));
    CHECK(rig.sampler->sceneListNames() == std::vector<std::string>{"GIG", "STUDIO"});

    CHECK(rig.accepted(akm::ItemId::SongRenameCurrent) == 1);
    CHECK(rig.accepted(akm::ItemId::SceneListRenameCurrent) == 1);
}

TEST_CASE("Given no current song file or scenelist, When one is renamed, Then the answer says to select one first and nothing is sent [RQ-MCP-048]",
          "[mcp][namedlists]")
{
    ListRig rig;
    const json song = byName(rig, RENAME_SONG, "NEW");
    CHECK(toolFailed(song));
    CHECK(hasText(toolText(song), SELECT_SONG));
    const json scenelist = byName(rig, RENAME_SCENELIST, "NEW");
    CHECK(toolFailed(scenelist));
    CHECK(hasText(toolText(scenelist), SELECT_SCENELIST));
    CHECK(rig.accepted(akm::ItemId::SongRenameCurrent) == 0);
    CHECK(rig.accepted(akm::ItemId::SceneListRenameCurrent) == 0);
    CHECK(rig.sampler->songNames() == SONGS);
}

TEST_CASE("Given a name another song file or scenelist bears, even in other letters, When the current one is renamed to it, Then nothing is sent and nothing changes; renaming it to its own name is accepted [RQ-MCP-048]",
          "[mcp][namedlists]")
{
    ListRig rig;
    REQUIRE_FALSE(toolFailed(byName(rig, SELECT_SONG, "VERSE")));
    const json song = byName(rig, RENAME_SONG, "intro");
    CHECK(toolFailed(song));
    CHECK(hasText(toolText(song), "already holds a song file named \"INTRO\""));
    CHECK(rig.sampler->songNames() == SONGS);
    CHECK(rig.accepted(akm::ItemId::SongRenameCurrent) == 0);
    CHECK_FALSE(toolFailed(byName(rig, RENAME_SONG, "VERSE")));
    CHECK(rig.accepted(akm::ItemId::SongRenameCurrent) == 1);

    REQUIRE_FALSE(toolFailed(byName(rig, SELECT_SCENELIST, "STUDIO")));
    const json scenelist = byName(rig, RENAME_SCENELIST, "LIVE SET");
    CHECK(toolFailed(scenelist));
    CHECK(hasText(toolText(scenelist), "already holds a scenelist named \"LIVE SET\""));
    CHECK(rig.sampler->sceneListNames() == SCENELISTS);
    CHECK(rig.accepted(akm::ItemId::SceneListRenameCurrent) == 0);
}

TEST_CASE("Given a new name that is empty, longer than 20 characters or beyond plain ASCII, When a song file, a scenelist or a set list is renamed, Then nothing is sent and the answer says what is accepted [RQ-MCP-048]",
          "[mcp][namedlists]")
{
    ListRig rig;
    REQUIRE_FALSE(toolFailed(byName(rig, SELECT_SONG, "VERSE")));
    REQUIRE_FALSE(toolFailed(byName(rig, SELECT_SCENELIST, "STUDIO")));
    const std::vector<std::string> refused = {"", std::string(MAX_LIST_NAME_LENGTH + 1, 'A'), "caf\xC3\xA9"};
    for (const std::string& name : refused)
    {
        INFO(name);
        const json song = byName(rig, RENAME_SONG, name);
        CHECK(toolFailed(song));
        CHECK(hasText(toolText(song), "1 to 20 characters"));
        CHECK(toolFailed(byName(rig, RENAME_SCENELIST, name)));
        CHECK(toolFailed(rig.call(RENAME_SET_LIST, {{"name", "TOUR"}, {"new_name", name}})));
    }
    CHECK(rig.accepted(akm::ItemId::SongRenameCurrent) == 0);
    CHECK(rig.accepted(akm::ItemId::SceneListRenameCurrent) == 0);
    CHECK(rig.accepted(akm::ItemId::SetListRename) == 0);

    CHECK_FALSE(toolFailed(byName(rig, RENAME_SONG, std::string(MAX_LIST_NAME_LENGTH, 'B'))));
}

TEST_CASE("Given the set list TOUR, When it is renamed by name, Then the simulated sampler holds the new name at the same position and the answer gives the position [RQ-MCP-048]",
          "[mcp][namedlists]")
{
    ListRig rig;
    const json renamed = rig.call(RENAME_SET_LIST, {{"name", "HOME"}, {"new_name", "STUDIO DAYS"}});
    CHECK_FALSE(toolFailed(renamed));
    CHECK(hasText(toolText(renamed), "Renamed the set list \"HOME\" (position 1) to \"STUDIO DAYS\"."));
    CHECK(rig.sampler->setListNames() == std::vector<std::string>{"TOUR", "STUDIO DAYS"});
    CHECK(rig.accepted(akm::ItemId::SetListRename) == 1);
    CHECK(rig.sentData(akm::ItemId::SetListRename).front().size() > 2);  // the two index bytes, then the name
}

TEST_CASE("Given an unknown set list, a name another set list bears, or two set lists of the same name, When one is renamed, Then nothing is sent and the answer says why [RQ-MCP-048]",
          "[mcp][namedlists]")
{
    ListRig rig;
    const json unknown = rig.call(RENAME_SET_LIST, {{"name", "MISSING"}, {"new_name", "X"}});
    CHECK(toolFailed(unknown));
    CHECK(hasText(toolText(unknown), "No set list is named \"MISSING\""));
    CHECK(hasText(toolText(unknown), LIST_SET_LISTS));

    const json used = rig.call(RENAME_SET_LIST, {{"name", "TOUR"}, {"new_name", "home"}});
    CHECK(toolFailed(used));
    CHECK(hasText(toolText(used), "already holds a set list named \"HOME\""));
    CHECK(rig.sampler->setListNames() == SET_LISTS);
    CHECK(rig.accepted(akm::ItemId::SetListRename) == 0);

    rig.sampler->setSetListNames({"TWIN", "TWIN", "OTHER"});
    const json ambiguous = rig.call(RENAME_SET_LIST, {{"name", "TWIN"}, {"new_name", "SINGLE"}});
    CHECK(toolFailed(ambiguous));
    CHECK(hasText(toolText(ambiguous), "positions 0 and 1"));
    CHECK(rig.accepted(akm::ItemId::SetListRename) == 0);
    CHECK_FALSE(toolFailed(rig.call(RENAME_SET_LIST, {{"name", "OTHER"}, {"new_name", "THIRD"}})));
    CHECK(rig.sampler->setListNames() == std::vector<std::string>{"TWIN", "TWIN", "THIRD"});
}

TEST_CASE("Given a missing, an unknown or a badly typed argument, When a rename or a list tool is called, Then it is refused and nothing is sent [RQ-MCP-048]",
          "[mcp][namedlists]")
{
    ListRig rig;
    CHECK(toolFailed(rig.call(RENAME_SONG)));
    CHECK(toolFailed(rig.call(RENAME_SONG, {{"name", 5}})));
    CHECK(toolFailed(rig.call(RENAME_SONG, {{"name", "X"}, {"extra", 1}})));
    CHECK(toolFailed(rig.call(RENAME_SCENELIST)));
    CHECK(toolFailed(rig.call(RENAME_SET_LIST)));
    CHECK(toolFailed(rig.call(RENAME_SET_LIST, {{"name", "TOUR"}})));
    CHECK(toolFailed(rig.call(RENAME_SET_LIST, {{"new_name", "X"}})));
    CHECK(toolFailed(rig.call(RENAME_SET_LIST, {{"name", 1}, {"new_name", "X"}})));
    CHECK(toolFailed(rig.call(RENAME_SET_LIST, {{"name", "TOUR"}, {"new_name", "X"}, {"extra", 1}})));
    for (const char* tool : {LIST_SONGS, LIST_SET_LISTS, LIST_SCENELISTS})
        CHECK(toolFailed(rig.call(tool, {{"extra", 1}})));
    CHECK(rig.accepted(akm::ItemId::SongRenameCurrent) == 0);
    CHECK(rig.accepted(akm::ItemId::SceneListRenameCurrent) == 0);
    CHECK(rig.accepted(akm::ItemId::SetListRename) == 0);
}
