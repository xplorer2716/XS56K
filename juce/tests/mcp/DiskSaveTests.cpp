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

// The disk tools that save: save_memory_item and save_all_memory_items, with the writability check, the refusal to overwrite
// unless asked, the count that ties a bulk save to what the client was told, and the check by the listing afterwards, over a
// real session and the simulated sampler. [TASK-MCP-021, RQ-MCP-026, RQ-MCP-027, RQ-MCP-029, ADR-MCP-003 (DEC-MCP-015,
// DEC-MCP-017, DEC-MCP-018)]
#include "DiskRig.hpp"

#include <chrono>
#include <memory>

using json = nlohmann::json;
using mcp::test::contains;
using mcp::test::DiskRig;
using mcp::test::isError;
using mcp::test::programFile;
using mcp::test::standardDisks;
using mcp::test::textOf;

namespace
{
    constexpr std::uint8_t SECTION_DISK = 0x10;
    constexpr std::uint8_t ITEM_SAVE_MEMORY_ITEM = 0x2C;
    constexpr int INVALID_PARAMS = -32602;
    constexpr std::uint8_t TYPE_MULTI = 1;
    constexpr std::uint8_t TYPE_PROGRAM = 2;
    constexpr std::uint8_t TYPE_SAMPLE = 3;

    /// A rig whose sampler holds the programs BASS, LEAD and PAD, the samples KICK and SNARE and the multi LIVE, and whose disk
    /// HD1 (writable) holds `rootFiles`; HD1 is selected unless `select` is false.
    std::unique_ptr<DiskRig> makeRig(std::vector<akm::harness::FileRecord> rootFiles = {}, bool allowDisk = true, bool select = true)
    {
        auto rig = std::make_unique<DiskRig>(standardDisks(std::move(rootFiles)), allowDisk);
        rig->sampler->setSampleNames({"KICK", "SNARE"});
        rig->sampler->setMultiNames({"LIVE"});
        if (select && allowDisk)
            REQUIRE_FALSE(isError(rig->call("select_disk", {{"name", "HD1"}})));
        return rig;
    }

    /// The data of every command of the section 10 item `item` the sampler accepted.
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

TEST_CASE("Given a program in memory and a writable disk, When save_memory_item is called, Then the sampler is sent the save with the program's position and both flags off, and the answer says the file is in the folder [RQ-MCP-026]",
          "[mcp][disk][save]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;

    const json answer = rig.call("save_memory_item", {{"kind", "program"}, {"name", "BASS"}});

    CHECK_FALSE(isError(answer));
    const std::string text = textOf(answer);
    CHECK(contains(text, "Saved the program \"BASS\" to the disk \"HD1\""));
    CHECK(contains(text, "BASS.AKP"));
    CHECK(contains(textOf(rig.call("list_disk_contents")), "BASS.AKP"));
    REQUIRE(rig.accepted(akm::ItemId::DiskSaveMemoryItem) == 1);
    // The programs are kept in alphabetical order: BASS is at position 0 (word 0, 0), type program, overwrite 0, children 0.
    CHECK(sentData(rig, ITEM_SAVE_MEMORY_ITEM).front() == std::vector<std::uint8_t>{0, 0, TYPE_PROGRAM, 0, 0});
}

TEST_CASE("Given a sample and a multi in memory, When they are saved, Then each is sent with its own type and position and its file is listed [RQ-MCP-026]",
          "[mcp][disk][save]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;

    const json sample = rig.call("save_memory_item", {{"kind", "sample"}, {"name", "SNARE"}});
    CHECK_FALSE(isError(sample));
    CHECK(contains(textOf(sample), "SNARE.AKS"));
    const json multi = rig.call("save_memory_item", {{"kind", "multi"}, {"name", "LIVE"}});
    CHECK_FALSE(isError(multi));
    CHECK(contains(textOf(multi), "LIVE.AKM"));

    const auto sent = sentData(rig, ITEM_SAVE_MEMORY_ITEM);
    REQUIRE(sent.size() == 2);
    CHECK(sent[0] == std::vector<std::uint8_t>{0, 1, TYPE_SAMPLE, 0, 0});
    CHECK(sent[1] == std::vector<std::uint8_t>{0, 0, TYPE_MULTI, 0, 0});
}

TEST_CASE("Given a file of the item's name already in the folder, When it is saved without overwrite, Then nothing is sent and the answer names the file; with overwrite the save is sent [RQ-MCP-026]",
          "[mcp][disk][save]")
{
    const auto holder = makeRig({programFile("BASS.AKP", "BASS", 777)});
    DiskRig& rig = *holder;

    const json refused = rig.call("save_memory_item", {{"kind", "program"}, {"name", "BASS"}});
    CHECK(isError(refused));
    CHECK(contains(textOf(refused), "BASS.AKP"));
    CHECK(contains(textOf(refused), "overwrite"));
    CHECK(rig.accepted(akm::ItemId::DiskSaveMemoryItem) == 0);
    CHECK(isError(rig.call("save_memory_item", {{"kind", "program"}, {"name", "BASS"}, {"overwrite", false}})));
    CHECK(rig.accepted(akm::ItemId::DiskSaveMemoryItem) == 0);

    const json replaced = rig.call("save_memory_item", {{"kind", "program"}, {"name", "BASS"}, {"overwrite", true}});
    CHECK_FALSE(isError(replaced));
    CHECK(contains(textOf(replaced), "BASS.AKP"));
    REQUIRE(rig.accepted(akm::ItemId::DiskSaveMemoryItem) == 1);
    CHECK(sentData(rig, ITEM_SAVE_MEMORY_ITEM).front() == std::vector<std::uint8_t>{0, 0, TYPE_PROGRAM, 1, 0});
}

TEST_CASE("Given save_children true, When a program is saved, Then the children flag is sent on [RQ-MCP-026]",
          "[mcp][disk][save]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    CHECK_FALSE(isError(rig.call("save_memory_item", {{"kind", "program"}, {"name", "LEAD"}, {"save_children", true}})));
    REQUIRE(rig.accepted(akm::ItemId::DiskSaveMemoryItem) == 1);
    CHECK(sentData(rig, ITEM_SAVE_MEMORY_ITEM).front() == std::vector<std::uint8_t>{0, 1, TYPE_PROGRAM, 0, 1});
}

TEST_CASE("Given a read-only disk, no disk selected, an item not in memory or bad arguments, When save_memory_item is called, Then nothing is sent and the answer says why [RQ-MCP-026, RQ-MCP-009]",
          "[mcp][disk][save]")
{
    SECTION("a read-only disk")
    {
        const auto holder = makeRig();
        DiskRig& rig = *holder;
        REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "CD1"}})));
        const json answer = rig.call("save_memory_item", {{"kind", "program"}, {"name", "BASS"}});
        CHECK(isError(answer));
        CHECK(contains(textOf(answer), "read-only"));
        CHECK(rig.accepted(akm::ItemId::DiskSaveMemoryItem) == 0);
    }
    SECTION("no disk selected")
    {
        const auto holder = makeRig({}, true, false);
        DiskRig& rig = *holder;
        const json answer = rig.call("save_memory_item", {{"kind", "program"}, {"name", "BASS"}});
        CHECK(isError(answer));
        CHECK(contains(textOf(answer), "select_disk"));
        CHECK(rig.accepted(akm::ItemId::DiskSaveMemoryItem) == 0);
    }
    SECTION("an item that is not in memory")
    {
        const auto holder = makeRig();
        DiskRig& rig = *holder;
        const json answer = rig.call("save_memory_item", {{"kind", "program"}, {"name", "NOPE"}});
        CHECK(isError(answer));
        CHECK(contains(textOf(answer), "BASS"));
        CHECK(contains(textOf(answer), "PAD"));
        CHECK(rig.accepted(akm::ItemId::DiskSaveMemoryItem) == 0);
    }
    SECTION("bad arguments")
    {
        const auto holder = makeRig();
        DiskRig& rig = *holder;
        CHECK(isError(rig.call("save_memory_item", {{"kind", "song"}, {"name", "BASS"}})));
        CHECK(isError(rig.call("save_memory_item", {{"kind", "program"}})));
        CHECK(isError(rig.call("save_memory_item", {{"name", "BASS"}})));
        CHECK(isError(rig.call("save_memory_item", {{"kind", "program"}, {"name", "BASS"}, {"overwrite", "yes"}})));
        CHECK(isError(rig.call("save_memory_item", {{"kind", "program"}, {"name", "BASS"}, {"save_children", 1}})));
        CHECK(rig.accepted(akm::ItemId::DiskSaveMemoryItem) == 0);
    }
}

TEST_CASE("Given three programs in memory, When save_all_memory_items is called with the wrong count, Then nothing is sent and the answer gives the count; with the right count the folder gains three files [RQ-MCP-027]",
          "[mcp][disk][save]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;

    const json wrong = rig.call("save_all_memory_items", {{"kind", "program"}, {"confirm", 2}});
    CHECK(isError(wrong));
    CHECK(contains(textOf(wrong), "3 programs"));
    CHECK(contains(textOf(wrong), "nothing was saved"));
    CHECK(rig.accepted(akm::ItemId::DiskSaveAllMemoryItems) == 0);

    const json right = rig.call("save_all_memory_items", {{"kind", "program"}, {"confirm", 3}});
    CHECK_FALSE(isError(right));
    CHECK(contains(textOf(right), "Saved the 3 programs to the disk \"HD1\""));
    CHECK(contains(textOf(right), "gained 3 files"));
    CHECK(rig.accepted(akm::ItemId::DiskSaveAllMemoryItems) == 1);
    const std::string contents = textOf(rig.call("list_disk_contents"));
    CHECK(contains(contents, "BASS.AKP"));
    CHECK(contains(contents, "LEAD.AKP"));
    CHECK(contains(contents, "PAD.AKP"));

    CHECK(isError(rig.call("save_all_memory_items", {{"kind", "program"}})));
    CHECK(isError(rig.call("save_all_memory_items", {{"kind", "program"}, {"confirm", "3"}})));
    CHECK(isError(rig.call("save_all_memory_items", {{"confirm", 3}})));
}

TEST_CASE("Given files of the items' names already in the folder, When all are saved without overwrite, Then nothing is sent and the answer lists the files; with overwrite they are saved [RQ-MCP-027]",
          "[mcp][disk][save]")
{
    const auto holder = makeRig({programFile("BASS.AKP", "BASS"), programFile("PAD.AKP", "PAD")});
    DiskRig& rig = *holder;

    const json refused = rig.call("save_all_memory_items", {{"kind", "program"}, {"confirm", 3}});
    CHECK(isError(refused));
    CHECK(contains(textOf(refused), "BASS.AKP"));
    CHECK(contains(textOf(refused), "PAD.AKP"));
    CHECK(contains(textOf(refused), "overwrite"));
    CHECK(rig.accepted(akm::ItemId::DiskSaveAllMemoryItems) == 0);

    const json replaced = rig.call("save_all_memory_items", {{"kind", "program"}, {"confirm", 3}, {"overwrite", true}});
    CHECK_FALSE(isError(replaced));
    CHECK(rig.accepted(akm::ItemId::DiskSaveAllMemoryItems) == 1);
    CHECK(contains(textOf(replaced), "gained 1 file"));
}

TEST_CASE("Given a read-only disk or none selected, When all are saved, Then nothing is sent [RQ-MCP-027, RQ-MCP-009]",
          "[mcp][disk][save]")
{
    const auto holder = makeRig({}, true, false);
    DiskRig& rig = *holder;
    CHECK(isError(rig.call("save_all_memory_items", {{"kind", "sample"}, {"confirm", 2}})));
    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "CD1"}})));
    const json answer = rig.call("save_all_memory_items", {{"kind", "sample"}, {"confirm", 2}});
    CHECK(isError(answer));
    CHECK(contains(textOf(answer), "read-only"));
    CHECK(rig.accepted(akm::ItemId::DiskSaveAllMemoryItems) == 0);
}

TEST_CASE("Given a sampler that never answers a save, When save_memory_item is called, Then it waits for the disk timeout and the answer mentions the power cycle and no retry [RQ-MCP-029, RQ-MCP-026]",
          "[mcp][disk][save]")
{
    const auto holder = makeRig();
    DiskRig& rig = *holder;
    akm::harness::SamplerBehaviour behaviour;
    behaviour.silentItems.push_back(akm::harness::SilentItem{SECTION_DISK, ITEM_SAVE_MEMORY_ITEM});
    rig.sampler->setBehaviour(behaviour);

    const auto started = std::chrono::steady_clock::now();
    const json answer = rig.call("save_memory_item", {{"kind", "program"}, {"name", "BASS"}});
    const auto waited = std::chrono::steady_clock::now() - started;

    CHECK(isError(answer));
    CHECK(contains(textOf(answer), "switched off and on"));
    CHECK(contains(textOf(answer), "not retried"));
    CHECK(waited >= mcp::test::DISK_RIG_DISK_TIMEOUT - std::chrono::milliseconds(200));
    CHECK(waited < mcp::test::DISK_RIG_DISK_TIMEOUT + std::chrono::seconds(6));
    CHECK(rig.accepted(akm::ItemId::DiskSaveMemoryItem) == 1);
}

TEST_CASE("Given the save tools, When the server is launched without --allow-disk or with it, Then they are absent, or listed as writing the disk: not read-only, destructive because a file can be replaced [RQ-MCP-023, RQ-MCP-013]",
          "[mcp][disk][save]")
{
    const auto offHolder = makeRig({}, false);
    for (const char* tool : {"save_memory_item", "save_all_memory_items"})
    {
        const json answer = offHolder->call(tool, {{"kind", "program"}});
        REQUIRE(answer.contains("error"));
        CHECK(answer["error"]["code"] == INVALID_PARAMS);
    }

    const auto onHolder = makeRig();
    const json message{{"jsonrpc", "2.0"},
                       {"id", 1},
                       {"method", "tools/list"},
                       {"params",
                        {{"_meta",
                          {{"io.modelcontextprotocol/protocolVersion", "2026-07-28"},
                           {"io.modelcontextprotocol/clientCapabilities", json::object()}}}}}};
    const json list = json::parse(*onHolder->server->handleLine(message.dump()));
    int found = 0;
    for (const json& tool : list["result"]["tools"])
    {
        const std::string name = tool["name"].get<std::string>();
        CHECK(name.find("delete_file") == std::string::npos);
        CHECK(name.find("delete_folder") == std::string::npos);
        CHECK(name.find("rename_file") == std::string::npos);
        CHECK(name.find("eject") == std::string::npos);
        CHECK(name.find("format") == std::string::npos);
        if (name == "save_memory_item" || name == "save_all_memory_items")
        {
            ++found;
            CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
            CHECK(tool["annotations"]["destructiveHint"].get<bool>());
            CHECK(contains(tool["description"].get<std::string>(), "overwrite"));
            CHECK(contains(tool["description"].get<std::string>(), "sampler's own disks"));
        }
    }
    CHECK(found == 2);
}
