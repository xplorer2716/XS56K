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

// Saving and loading song files, set lists and scenelists through the disk: the kinds song_file, set_list and scenelist of
// save_memory_item and save_all_memory_items, and load_file on their files, over a real session and the simulated sampler. The extensions of
// their files are what a real S5000 wrote (TASK-MCP-059, 2026-10-10): .MID, .SET and .ASC, 34681, 2304 and 26 bytes here.
// [TASK-MCP-049, RQ-MCP-050, RQ-MCP-025, RQ-MCP-026, RQ-MCP-027, ADR-MCP-005 (DEC-MCP-031), ADR-MCP-003 (DEC-MCP-019, DEC-MCP-021)]
#include "DiskRig.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

using json = nlohmann::json;
using mcp::test::contains;
using mcp::test::DiskRig;
using mcp::test::isError;
using mcp::test::standardDisks;
using mcp::test::textOf;

namespace
{
    constexpr std::uint8_t SECTION_DISK = 0x10;
    constexpr std::uint8_t ITEM_SAVE_MEMORY_ITEM = 0x2C;
    constexpr std::uint8_t ITEM_SAVE_ALL_MEMORY_ITEMS = 0x2D;
    constexpr std::uint8_t TYPE_SMF = 4;
    constexpr std::uint8_t TYPE_SETLIST = 5;
    constexpr std::uint8_t TYPE_SCENELIST = 6;
    constexpr const char* KIND_SONG = "song_file";
    constexpr const char* KIND_SET_LIST = "set_list";
    constexpr const char* KIND_SCENELIST = "scenelist";
    constexpr const char* SAVE_ONE = "save_memory_item";
    constexpr const char* SAVE_ALL = "save_all_memory_items";
    constexpr const char* LOAD = "load_file";
    constexpr const char* SONG_FILE = "VERSE.MID";
    constexpr const char* SET_LIST_FILE = "HOME.SET";
    constexpr const char* SCENELIST_FILE = "STUDIO.ASC";

    const std::vector<std::string> SONGS = {"INTRO", "VERSE", "OUTRO"};
    const std::vector<std::string> SET_LISTS = {"TOUR", "HOME"};
    const std::vector<std::string> SCENELISTS = {"LIVE SET", "STUDIO"};

    /// A rig whose sampler holds the song files, set lists and scenelists above, with the writable disk HD1 selected.
    std::unique_ptr<DiskRig> makeRig()
    {
        auto rig = std::make_unique<DiskRig>(standardDisks());
        rig->sampler->setSongNames(SONGS);
        rig->sampler->setSetListNames(SET_LISTS);
        rig->sampler->setSceneListNames(SCENELISTS);
        REQUIRE_FALSE(isError(rig->call("select_disk", {{"name", "HD1"}})));
        return rig;
    }

    json saveOne(DiskRig& rig, const char* kind, const std::string& name, bool overwrite = false)
    {
        return rig.call(SAVE_ONE, {{"kind", kind}, {"name", name}, {"overwrite", overwrite}});
    }

    json saveAll(DiskRig& rig, const char* kind, int confirm)
    {
        return rig.call(SAVE_ALL, {{"kind", kind}, {"confirm", confirm}});
    }

    std::vector<std::vector<std::uint8_t>> sentData(const DiskRig& rig, std::uint8_t item)
    {
        std::vector<std::vector<std::uint8_t>> sent;
        for (const auto& command : rig.sampler->acceptedCommands())
        {
            if (command.section == SECTION_DISK && command.item == item)
                sent.push_back(command.data);
        }
        return sent;
    }
}

TEST_CASE("Given a song file in memory and a writable disk, When it is saved, Then the sampler is sent the save with the song file's position and the song file type, and the file bearing its name is in the folder [RQ-MCP-050]",
          "[mcp][disk][save][namedlists]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    const json answer = saveOne(rig, KIND_SONG, "VERSE");
    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "Saved the song file \"VERSE\" to the disk \"HD1\""));
    CHECK(contains(textOf(answer), SONG_FILE));
    CHECK(contains(textOf(rig.call("list_disk_contents")), SONG_FILE));
    REQUIRE(sentData(rig, ITEM_SAVE_MEMORY_ITEM).size() == 1);
    CHECK(sentData(rig, ITEM_SAVE_MEMORY_ITEM).front() == std::vector<std::uint8_t>{0, 1, TYPE_SMF, 0, 0});
}

TEST_CASE("Given a set list and a scenelist in memory, When each is saved, Then each is sent with its own type and position and its file is listed [RQ-MCP-050]",
          "[mcp][disk][save][namedlists]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    const json setList = saveOne(rig, KIND_SET_LIST, "HOME");
    CHECK_FALSE(isError(setList));
    CHECK(contains(textOf(setList), SET_LIST_FILE));
    const json scenelist = saveOne(rig, KIND_SCENELIST, "STUDIO");
    CHECK_FALSE(isError(scenelist));
    CHECK(contains(textOf(scenelist), SCENELIST_FILE));
    const auto sent = sentData(rig, ITEM_SAVE_MEMORY_ITEM);
    REQUIRE(sent.size() == 2);
    CHECK(sent[0] == std::vector<std::uint8_t>{0, 1, TYPE_SETLIST, 0, 0});
    CHECK(sent[1] == std::vector<std::uint8_t>{0, 1, TYPE_SCENELIST, 0, 0});
}

TEST_CASE("Given a song file already saved, When it is saved again without overwrite, Then nothing is sent and the answer names the file; with overwrite the save is sent and the file is replaced [RQ-MCP-050, RQ-MCP-026]",
          "[mcp][disk][save][namedlists]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    REQUIRE_FALSE(isError(saveOne(rig, KIND_SONG, "VERSE")));
    const json refused = saveOne(rig, KIND_SONG, "VERSE");
    CHECK(isError(refused));
    CHECK(contains(textOf(refused), SONG_FILE));
    CHECK(contains(textOf(refused), "overwrite"));
    CHECK(rig.accepted(akm::ItemId::DiskSaveMemoryItem) == 1);

    const json replaced = saveOne(rig, KIND_SONG, "VERSE", true);
    CHECK_FALSE(isError(replaced));
    CHECK(contains(textOf(replaced), "replacing the file of that name"));
    CHECK(rig.accepted(akm::ItemId::DiskSaveMemoryItem) == 2);
}

TEST_CASE("Given an item the sampler does not hold, When it is saved, Then nothing is sent and the answer lists what the memory holds [RQ-MCP-050]",
          "[mcp][disk][save][namedlists]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    const json answer = saveOne(rig, KIND_SONG, "MISSING");
    CHECK(isError(answer));
    CHECK(contains(textOf(answer), "No song file is named \"MISSING\""));
    CHECK(contains(textOf(answer), "\"INTRO\""));
    CHECK(rig.accepted(akm::ItemId::DiskSaveMemoryItem) == 0);
}

TEST_CASE("Given a kind that is not one of the six, When an item is saved, Then the answer lists the six kinds; a kind may be given in other letters or with a space [RQ-MCP-050]",
          "[mcp][disk][save][namedlists]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    const json unknown = saveOne(rig, "folder", "X");
    CHECK(isError(unknown));
    for (const char* kind : {"program", "sample", "multi", "song_file", "set_list", "scenelist"})
        CHECK(contains(textOf(unknown), kind));
    CHECK(rig.accepted(akm::ItemId::DiskSaveMemoryItem) == 0);
    CHECK_FALSE(isError(rig.call(SAVE_ONE, {{"kind", "Song File"}, {"name", "INTRO"}})));
    CHECK_FALSE(isError(rig.call(SAVE_ONE, {{"kind", "SET-LIST"}, {"name", "TOUR"}})));
    CHECK(rig.accepted(akm::ItemId::DiskSaveMemoryItem) == 2);
}

TEST_CASE("Given three song files, two set lists and two scenelists, When each kind is saved in bulk with its count as confirm, Then the save is sent with the kind's type and the folder gains one file per item; a wrong count sends nothing [RQ-MCP-050, RQ-MCP-027]",
          "[mcp][disk][save][namedlists]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    const json wrong = saveAll(rig, KIND_SONG, 2);
    CHECK(isError(wrong));
    CHECK(contains(textOf(wrong), "There are 3 song files"));
    CHECK(rig.accepted(akm::ItemId::DiskSaveAllMemoryItems) == 0);

    const json songs = saveAll(rig, KIND_SONG, 3);
    CHECK_FALSE(isError(songs));
    CHECK(contains(textOf(songs), "the folder gained 3 files"));
    CHECK_FALSE(isError(saveAll(rig, KIND_SET_LIST, 2)));
    CHECK_FALSE(isError(saveAll(rig, KIND_SCENELIST, 2)));
    const auto sent = sentData(rig, ITEM_SAVE_ALL_MEMORY_ITEMS);
    REQUIRE(sent.size() == 3);
    CHECK(sent[0].front() == TYPE_SMF);
    CHECK(sent[1].front() == TYPE_SETLIST);
    CHECK(sent[2].front() == TYPE_SCENELIST);
    const std::string folder = textOf(rig.call("list_disk_contents"));
    for (const char* file : {"INTRO.MID", "VERSE.MID", "OUTRO.MID", "TOUR.SET", "HOME.SET", "LIVE SET.ASC", "STUDIO.ASC"})
        CHECK(contains(folder, file));
}

TEST_CASE("Given a saved song file, set list and scenelist that were deleted from memory, When their files are loaded, Then the sampler holds them again and the answer gives the counts before and after [RQ-MCP-050, RQ-MCP-025]",
          "[mcp][disk][load][namedlists]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    REQUIRE_FALSE(isError(saveOne(rig, KIND_SONG, "VERSE")));
    REQUIRE_FALSE(isError(saveOne(rig, KIND_SET_LIST, "HOME")));
    REQUIRE_FALSE(isError(saveOne(rig, KIND_SCENELIST, "STUDIO")));
    REQUIRE_FALSE(isError(rig.call("select_song_file", {{"name", "VERSE"}})));
    REQUIRE_FALSE(isError(rig.call("delete_song_file", {{"confirm", "VERSE"}})));
    REQUIRE_FALSE(isError(rig.call("delete_set_list", {{"name", "HOME"}, {"confirm", "HOME"}})));
    REQUIRE_FALSE(isError(rig.call("select_scenelist", {{"name", "STUDIO"}})));
    REQUIRE_FALSE(isError(rig.call("delete_scenelist", {{"confirm", "STUDIO"}})));
    REQUIRE(rig.sampler->songNames() == std::vector<std::string>{"INTRO", "OUTRO"});

    const json song = rig.call(LOAD, {{"name", SONG_FILE}});
    CHECK_FALSE(isError(song));
    CHECK(contains(textOf(song), "song files 3 (was 2, added VERSE)"));
    const json setList = rig.call(LOAD, {{"name", SET_LIST_FILE}});
    CHECK_FALSE(isError(setList));
    // The sampler holds one set list at a time: loading one replaces the one in memory (a real S5000, TASK-MCP-058, 2026-10-10).
    CHECK(contains(textOf(setList), "set lists 1 (was 1, added HOME, no longer held TOUR)"));
    const json scenelist = rig.call(LOAD, {{"name", SCENELIST_FILE}});
    CHECK_FALSE(isError(scenelist));
    CHECK(contains(textOf(scenelist), "scenelists 2 (was 1, added STUDIO)"));
    CHECK(rig.sampler->songNames() == std::vector<std::string>{"INTRO", "OUTRO", "VERSE"});
    CHECK(rig.sampler->setListNames() == std::vector<std::string>{"HOME"});
    CHECK(rig.sampler->sceneListNames() == std::vector<std::string>{"LIVE SET", "STUDIO"});
}

TEST_CASE("Given a program file, When it is loaded, Then the answer does not speak of song files, set lists or scenelists, which did not change [RQ-MCP-025]",
          "[mcp][disk][load][namedlists]")
{
    DiskRig rig(standardDisks({mcp::test::programFile("INIT.AKP", "INIT", 500)}));
    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "HD1"}})));
    const json loaded = rig.call(LOAD, {{"name", "INIT.AKP"}});
    CHECK_FALSE(isError(loaded));
    CHECK_FALSE(contains(textOf(loaded), "song files"));
    CHECK_FALSE(contains(textOf(loaded), "set lists"));
    CHECK_FALSE(contains(textOf(loaded), "scenelists"));
}

TEST_CASE("Given a set list in memory and the file of another set list, When it is loaded, Then the sampler holds only the loaded one and the answer says which set list is no longer held; with none in memory nothing is said to be gone [RQ-MCP-025, RQ-MCP-050, TASK-MCP-058]",
          "[mcp][disk][load][namedlists]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    REQUIRE_FALSE(isError(saveOne(rig, KIND_SET_LIST, "HOME")));

    const json replaced = rig.call(LOAD, {{"name", SET_LIST_FILE}});
    CHECK_FALSE(isError(replaced));
    CHECK(rig.sampler->setListNames() == std::vector<std::string>{"HOME"});
    CHECK(contains(textOf(replaced), "no longer held TOUR"));

    REQUIRE_FALSE(isError(rig.call("delete_set_list", {{"name", "HOME"}, {"confirm", "HOME"}})));
    const json afterNone = rig.call(LOAD, {{"name", SET_LIST_FILE}});
    CHECK_FALSE(isError(afterNone));
    CHECK(rig.sampler->setListNames() == std::vector<std::string>{"HOME"});
    CHECK(contains(textOf(afterNone), "set lists 1 (was 0, added HOME)"));
    CHECK_FALSE(contains(textOf(afterNone), "no longer held"));
}

TEST_CASE("Given a song file, a set list and a scenelist in memory, When each is saved, Then the folder lists the extension and the size a real S5000 wrote: .MID, .SET and .ASC [RQ-MCP-050, TASK-MCP-059]",
          "[mcp][disk][namedlists]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    REQUIRE_FALSE(isError(saveOne(rig, KIND_SONG, "VERSE")));
    REQUIRE_FALSE(isError(saveOne(rig, KIND_SET_LIST, "HOME")));
    REQUIRE_FALSE(isError(saveOne(rig, KIND_SCENELIST, "STUDIO")));

    const std::string folder = textOf(rig.call("list_disk_contents"));
    CHECK(contains(folder, "VERSE.MID (34681 bytes)"));
    CHECK(contains(folder, "HOME.SET (2304 bytes)"));
    CHECK(contains(folder, "STUDIO.ASC (26 bytes)"));
}

TEST_CASE("Given a song file and a scenelist saved and still in memory, When each file is loaded, Then the sampler replaces the item instead of adding one: the song keeps its place in the sorted list, the scenelist goes to the end, the position selected stays selected and the answer speaks of no change of names [RQ-MCP-025, TASK-MCP-059]",
          "[mcp][disk][load][namedlists]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    rig.sampler->setSongNames({"INTRO", "OUTRO", "VERSE"});
    REQUIRE_FALSE(isError(saveOne(rig, KIND_SONG, "OUTRO")));
    REQUIRE_FALSE(isError(saveOne(rig, KIND_SCENELIST, "LIVE SET")));
    REQUIRE_FALSE(isError(rig.call("select_song_file", {{"name", "OUTRO"}})));
    REQUIRE_FALSE(isError(rig.call("select_scenelist", {{"name", "STUDIO"}})));

    const json song = rig.call(LOAD, {{"name", "OUTRO.MID"}});
    CHECK_FALSE(isError(song));
    CHECK_FALSE(contains(textOf(song), "song files"));
    CHECK(rig.sampler->songNames() == std::vector<std::string>{"INTRO", "OUTRO", "VERSE"});
    CHECK(rig.sampler->currentSong() == std::optional<std::size_t>{1});

    const json scenelist = rig.call(LOAD, {{"name", "LIVE SET.ASC"}});
    CHECK_FALSE(isError(scenelist));
    CHECK(contains(textOf(scenelist), "scenelists 2 (was 2)"));
    CHECK(rig.sampler->sceneListNames() == std::vector<std::string>{"STUDIO", "LIVE SET"});
    CHECK(rig.sampler->currentSceneList() == std::optional<std::size_t>{1});
}

TEST_CASE("Given a song file and a scenelist renamed aside after being saved, When their files are loaded, Then the song is added in its sorted place and the scenelist at the end, and the position selected stays selected [RQ-MCP-025, TASK-MCP-059]",
          "[mcp][disk][load][namedlists]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    rig.sampler->setSongNames({"a_woman_in_love", "AIMER", "alors_regarde", "assassine"});
    REQUIRE_FALSE(isError(saveOne(rig, KIND_SONG, "assassine")));
    REQUIRE_FALSE(isError(saveOne(rig, KIND_SCENELIST, "STUDIO")));
    REQUIRE_FALSE(isError(rig.call("select_song_file", {{"name", "assassine"}})));
    REQUIRE_FALSE(isError(rig.call("rename_song_file", {{"name", "assassine-ORIG"}})));
    REQUIRE_FALSE(isError(rig.call("select_scenelist", {{"name", "STUDIO"}})));
    REQUIRE_FALSE(isError(rig.call("rename_scenelist", {{"name", "STUDIO-ORIG"}})));

    const json song = rig.call(LOAD, {{"name", "assassine.MID"}});
    CHECK_FALSE(isError(song));
    CHECK(contains(textOf(song), "song files 5 (was 4, added assassine)"));
    CHECK(rig.sampler->songNames() ==
          std::vector<std::string>{"a_woman_in_love", "AIMER", "alors_regarde", "assassine", "assassine-ORIG"});
    CHECK(rig.sampler->currentSong() == std::optional<std::size_t>{3});

    const json scenelist = rig.call(LOAD, {{"name", SCENELIST_FILE}});
    CHECK_FALSE(isError(scenelist));
    CHECK(contains(textOf(scenelist), "scenelists 3 (was 2, added STUDIO)"));
    CHECK(rig.sampler->sceneListNames() == std::vector<std::string>{"LIVE SET", "STUDIO-ORIG", "STUDIO"});
    CHECK(rig.sampler->currentSceneList() == std::optional<std::size_t>{1});
}
