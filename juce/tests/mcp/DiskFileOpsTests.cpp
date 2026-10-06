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

// The file and folder operations of the disk tools: rename_file, rename_folder, delete_file and delete_folder (the deletions confirmed
// by the exact name), over a real session and the simulated sampler. [TASK-MCP-033, RQ-MCP-039, RQ-MCP-042, ADR-MCP-004 (DEC-MCP-023,
// DEC-MCP-024)]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "DiskRig.hpp"
#include "akm/harness/SimulatedSampler.hpp"

using json = nlohmann::json;
using mcp::test::contains;
using mcp::test::DiskRig;
using mcp::test::isError;
using mcp::test::programFile;
using mcp::test::sampleFile;
using mcp::test::standardDisks;
using mcp::test::textOf;

namespace
{
    constexpr int INVALID_PARAMS = -32602;

    /// HD1 (writable) holds TEST.AKP, OTHER.AKP and KICK.WAV at its root, a folder EMPTY and a folder OLD with one file and one folder;
    /// HD1 is selected. CD1 is read-only.
    std::unique_ptr<DiskRig> makeRig(bool select = true, bool stale = false)
    {
        akm::harness::FolderRecord inner;
        inner.name = "INNER";
        akm::harness::FolderRecord old;
        old.name = "OLD";
        old.files = {programFile("KEEP.AKP", "KEEP")};
        old.subFolders = {inner};
        akm::harness::FolderRecord empty;
        empty.name = "EMPTY";
        auto rig = std::make_unique<DiskRig>(
            standardDisks({programFile("TEST.AKP", "TEST"), programFile("OTHER.AKP", "OTHER"), sampleFile("KICK.WAV", "KICK")}, {empty, old}),
            true);
        if (stale)
        {
            akm::harness::SamplerBehaviour behaviour;
            behaviour.staleFileListAfterSave = true;
            rig->sampler->setBehaviour(behaviour);
        }
        if (select)
            REQUIRE_FALSE(isError(rig->call("select_disk", {{"name", "HD1"}})));
        return rig;
    }

    std::string listing(DiskRig& rig)
    {
        return textOf(rig.call("list_disk_contents"));
    }
}

TEST_CASE("Given the server with and without --allow-disk, When the tools are listed, Then the four file tools are disk tools, the deleting ones destructive, and absent without the flag [RQ-MCP-039, RQ-MCP-042]",
          "[mcp][disk][fileops]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    const json list = [&] {
        const json message{{"jsonrpc", "2.0"},
                           {"id", 900},
                           {"method", "tools/list"},
                           {"params", {{"_meta", {{"io.modelcontextprotocol/protocolVersion", "2026-07-28"}, {"io.modelcontextprotocol/clientCapabilities", json::object()}}}}}};
        return json::parse(*rig.server->handleLine(message.dump()));
    }();
    int found = 0;
    for (const json& tool : list["result"]["tools"])
    {
        const std::string name = tool["name"].get<std::string>();
        if (name == "rename_file" || name == "rename_folder")
        {
            ++found;
            CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
            CHECK_FALSE(tool["annotations"]["destructiveHint"].get<bool>());
        }
        if (name == "delete_file" || name == "delete_folder")
        {
            ++found;
            CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
            CHECK(tool["annotations"]["destructiveHint"].get<bool>());
        }
    }
    CHECK(found == 4);

    DiskRig without(standardDisks(), false);
    for (const char* tool : {"rename_file", "rename_folder", "delete_file", "delete_folder"})
    {
        const json answer = without.call(tool, {{"name", "X"}, {"new_name", "Y"}, {"confirm", "X"}});
        INFO(tool);
        REQUIRE(answer.contains("error"));
        CHECK(answer["error"]["code"] == INVALID_PARAMS);
    }
}

TEST_CASE("Given TEST.AKP in the root, When rename_file gives the new name NEW, Then the sampler is sent one rename and the file is NEW.AKP, the extension kept [RQ-MCP-039]",
          "[mcp][disk][fileops]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    const json answer = rig.call("rename_file", {{"name", "TEST.AKP"}, {"new_name", "NEW"}});
    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "Renamed the file \"TEST.AKP\" to \"NEW.AKP\""));
    CHECK(rig.accepted(akm::ItemId::DiskRenameFile) == 1);
    const std::string contents = listing(rig);
    CHECK(contains(contents, "NEW.AKP"));
    CHECK_FALSE(contains(contents, "TEST.AKP"));
    CHECK(contains(contents, "OTHER.AKP"));
}

TEST_CASE("Given a new name that carries the extension, one a file already bears, or a name that is not there, When rename_file is called, Then nothing is sent and the answer says why [RQ-MCP-039]",
          "[mcp][disk][fileops]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    const json doubled = rig.call("rename_file", {{"name", "TEST.AKP"}, {"new_name", "NEW.AKP"}});
    CHECK(isError(doubled));
    CHECK(contains(textOf(doubled), "extension"));
    const json clash = rig.call("rename_file", {{"name", "TEST.AKP"}, {"new_name", "other"}});
    CHECK(isError(clash));
    CHECK(contains(textOf(clash), "OTHER.AKP"));
    const json missing = rig.call("rename_file", {{"name", "NOPE.AKP"}, {"new_name", "NEW"}});
    CHECK(isError(missing));
    CHECK(contains(textOf(missing), "TEST.AKP"));
    CHECK(rig.accepted(akm::ItemId::DiskRenameFile) == 0);
}

TEST_CASE("Given missing or bad arguments, no disk or a read-only disk, When a file or folder tool is called, Then nothing is sent [RQ-MCP-039]",
          "[mcp][disk][fileops]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    CHECK(isError(rig.call("rename_file")));
    CHECK(isError(rig.call("rename_file", {{"name", "TEST.AKP"}})));
    CHECK(isError(rig.call("rename_file", {{"name", "TEST.AKP"}, {"new_name", ""}})));
    CHECK(isError(rig.call("rename_file", {{"name", "TEST.AKP"}, {"new_name", "A/B"}})));
    CHECK(isError(rig.call("rename_file", {{"name", 3}, {"new_name", "B"}})));
    CHECK(isError(rig.call("rename_folder", {{"name", "OLD"}, {"new_name", "A\\B"}})));
    CHECK(isError(rig.call("rename_folder", {{"name", "OLD"}})));
    CHECK(isError(rig.call("delete_file", {{"name", "TEST.AKP"}})));
    CHECK(isError(rig.call("delete_folder", {{"name", "EMPTY"}, {"confirm", "EMPTY"}, {"delete_contents", "yes"}})));
    CHECK(isError(rig.call("delete_folder", {{"name", "EMPTY"}, {"confirm", "EMPTY"}, {"extra", 1}})));

    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "CD1"}})));
    for (const json& call : {json{{"rename_file", {{"name", "X.AKP"}, {"new_name", "Y"}}}}, json{{"rename_folder", {{"name", "X"}, {"new_name", "Y"}}}},
                             json{{"delete_file", {{"name", "X.AKP"}, {"confirm", "X.AKP"}}}}, json{{"delete_folder", {{"name", "X"}, {"confirm", "X"}}}}})
    {
        const std::string tool = call.begin().key();
        const json answer = rig.call(tool, call.begin().value());
        INFO(tool);
        CHECK(isError(answer));
        CHECK(contains(textOf(answer), "read-only"));
    }

    DiskRig noDisk(standardDisks(), true);
    CHECK(contains(textOf(noDisk.call("delete_file", {{"name", "X.AKP"}, {"confirm", "X.AKP"}})), "select_disk"));

    CHECK(rig.accepted(akm::ItemId::DiskRenameFile) == 0);
    CHECK(rig.accepted(akm::ItemId::DiskRenameFolder) == 0);
    CHECK(rig.accepted(akm::ItemId::DiskDeleteFile) == 0);
    CHECK(rig.accepted(akm::ItemId::DiskDeleteSubFolder) == 0);
}

TEST_CASE("Given the folder OLD, When rename_folder gives ARCHIVE, Then the folder is renamed once and listed; a name a folder or file bears is refused [RQ-MCP-039]",
          "[mcp][disk][fileops]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    CHECK(isError(rig.call("rename_folder", {{"name", "OLD"}, {"new_name", "empty"}})));
    CHECK(isError(rig.call("rename_folder", {{"name", "OLD"}, {"new_name", "KICK.WAV"}})));
    CHECK(isError(rig.call("rename_folder", {{"name", "NOPE"}, {"new_name", "X"}})));
    CHECK(rig.accepted(akm::ItemId::DiskRenameFolder) == 0);

    const json answer = rig.call("rename_folder", {{"name", "OLD"}, {"new_name", "ARCHIVE"}});
    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "Renamed the folder \"OLD\" to \"ARCHIVE\""));
    CHECK(rig.accepted(akm::ItemId::DiskRenameFolder) == 1);
    const std::string contents = listing(rig);
    CHECK(contains(contents, "ARCHIVE"));
    CHECK_FALSE(contains(contents, "  OLD\n"));
}

TEST_CASE("Given TEST.AKP in the root, When delete_file is called with confirm TEST.AKP, Then the file is gone and the others stay [RQ-MCP-039, RQ-MCP-042]",
          "[mcp][disk][fileops]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    const json answer = rig.call("delete_file", {{"name", "TEST.AKP"}, {"confirm", "TEST.AKP"}});
    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "Deleted the file \"TEST.AKP\""));
    CHECK(rig.accepted(akm::ItemId::DiskDeleteFile) == 1);
    const std::string contents = listing(rig);
    CHECK_FALSE(contains(contents, "TEST.AKP"));
    CHECK(contains(contents, "OTHER.AKP"));
    CHECK(contains(contents, "KICK.WAV"));
}

TEST_CASE("Given a missing or wrong confirm or a file that is not there, When delete_file is called, Then nothing is sent and the answer gives the exact name to confirm [RQ-MCP-042]",
          "[mcp][disk][fileops]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    for (const char* confirm : {"test.akp", "OTHER.AKP", "TEST.AKP ", "TEST", ""})
    {
        const json answer = rig.call("delete_file", {{"name", "TEST.AKP"}, {"confirm", confirm}});
        INFO(confirm);
        CHECK(isError(answer));
        CHECK(contains(textOf(answer), "\"TEST.AKP\""));
        CHECK(contains(textOf(answer), "nothing was deleted"));
    }
    CHECK(isError(rig.call("delete_file", {{"name", "NOPE.AKP"}, {"confirm", "NOPE.AKP"}})));
    CHECK(rig.accepted(akm::ItemId::DiskDeleteFile) == 0);
}

TEST_CASE("Given an empty folder, When delete_folder is called with confirm EMPTY, Then it is deleted without delete_contents [RQ-MCP-039, RQ-MCP-042]",
          "[mcp][disk][fileops]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    const json answer = rig.call("delete_folder", {{"name", "EMPTY"}, {"confirm", "EMPTY"}});
    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "Deleted the empty folder \"EMPTY\""));
    CHECK(rig.accepted(akm::ItemId::DiskDeleteSubFolder) == 1);
    CHECK_FALSE(contains(listing(rig), "EMPTY"));
}

TEST_CASE("Given the folder OLD holding one file and one folder, When delete_folder is called, Then it is refused with the count unless delete_contents is true, and the current folder is unchanged [RQ-MCP-039]",
          "[mcp][disk][fileops]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    const json refused = rig.call("delete_folder", {{"name", "OLD"}, {"confirm", "OLD"}});
    CHECK(isError(refused));
    CHECK(contains(textOf(refused), "2 items"));
    CHECK(contains(textOf(refused), "delete_contents"));
    CHECK(rig.accepted(akm::ItemId::DiskDeleteSubFolder) == 0);
    CHECK(contains(listing(rig), ": (root)"));

    const json answer = rig.call("delete_folder", {{"name", "OLD"}, {"confirm", "OLD"}, {"delete_contents", true}});
    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "Deleted the folder \"OLD\" and the 2 items it held"));
    CHECK(rig.accepted(akm::ItemId::DiskDeleteSubFolder) == 1);
    CHECK_FALSE(contains(listing(rig), "OLD"));
}

TEST_CASE("Given a wrong confirm for a folder, When delete_folder is called, Then nothing is sent and the answer gives the exact name [RQ-MCP-042]",
          "[mcp][disk][fileops]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    for (const char* confirm : {"old", "EMPTY", "OLD ", ""})
    {
        const json answer = rig.call("delete_folder", {{"name", "OLD"}, {"confirm", confirm}, {"delete_contents", true}});
        INFO(confirm);
        CHECK(isError(answer));
        CHECK(contains(textOf(answer), "\"OLD\""));
        CHECK(contains(textOf(answer), "nothing was deleted"));
    }
    CHECK(isError(rig.call("delete_folder", {{"name", "NOPE"}, {"confirm", "NOPE"}})));
    CHECK(rig.accepted(akm::ItemId::DiskDeleteSubFolder) == 0);
}

TEST_CASE("Given a sampler that keeps a stale file list, When a file is deleted or renamed, Then the answer comes from a refreshed listing [RQ-MCP-039, RQ-MCP-033]",
          "[mcp][disk][fileops]")
{
    const auto holder = makeRig(true, true);
    DiskRig& rig = *holder;
    const json deleted = rig.call("delete_file", {{"name", "TEST.AKP"}, {"confirm", "TEST.AKP"}});
    INFO(textOf(deleted));
    CHECK_FALSE(isError(deleted));
    CHECK_FALSE(contains(listing(rig), "TEST.AKP"));
    const json renamed = rig.call("rename_file", {{"name", "OTHER.AKP"}, {"new_name", "RENAMED"}});
    INFO(textOf(renamed));
    CHECK_FALSE(isError(renamed));
    CHECK(contains(textOf(renamed), "\"RENAMED.AKP\""));
    CHECK(contains(listing(rig), "RENAMED.AKP"));
}
