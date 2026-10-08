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

// The disk tools of the MCP server, browsing: the opt-in flag, the disks, their selection, the contents of the current
// folder and the navigation, over a real session and the simulated sampler. [TASK-MCP-019, RQ-MCP-023, RQ-MCP-024,
// RQ-MCP-028, RQ-MCP-029, ADR-MCP-003 (DEC-MCP-015, DEC-MCP-016, DEC-MCP-017)]
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "SimulatedPrograms.hpp"
#include "TestBytes.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/RealScheduler.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"
#include "mcp/McpServer.hpp"
#include "mcp/ParameterCatalogue.hpp"
#include "mcp/SamplerGateway.hpp"
#include "mcp/Tools.hpp"

using json = nlohmann::json;
using akm::harness::DiskRecord;
using akm::harness::FileRecord;
using akm::harness::FolderRecord;
using akm::harness::SimulatedMidiBackend;
using akm::harness::SimulatedSampler;

namespace
{
    constexpr std::chrono::milliseconds COMMAND_TIMEOUT{300};
    constexpr std::chrono::milliseconds DISK_TIMEOUT{1500};
    constexpr const char* MODERN = "2026-07-28";
    constexpr int INVALID_PARAMS = -32602;
    constexpr std::uint8_t SECTION_DISK = 0x10;
    constexpr std::uint8_t ITEM_UPDATE_DISK_LIST = 0x01;
    constexpr std::uint8_t ITEM_CREATE_FOLDER = 0x16;

    /// A simulated sampler holding the disks HD1 (a hard disk, MSDOS, writable, with the folders DRUMS and SYNTH and the files
    /// INIT.AKP and KICK.WAV at its root, one file in DRUMS) and CD1 (a CD-ROM, ISO9660, read-only).
    struct Rig
    {
        explicit Rig(bool allowDisk = true, bool withDisks = true, bool allowRefresh = false)
        {
            sampler = &backend.addSampler();
            mcp::test::seedThreePrograms(backend);
            if (withDisks)
            {
                DiskRecord hard;
                hard.handle = 0;
                hard.type = 1;
                hard.format = 1;
                hard.writable = true;
                hard.name = "HD1";
                hard.freeBytes = 1000000;
                FolderRecord drums;
                drums.name = "DRUMS";
                drums.files = {FileRecord{"SNARE.WAV", 2500, std::nullopt, std::nullopt, {}}};
                FolderRecord synth;
                synth.name = "SYNTH";
                hard.rootFolder.subFolders = {drums, synth};
                hard.rootFolder.files = {FileRecord{"INIT.AKP", 3000, std::nullopt, std::nullopt, {}},
                                         FileRecord{"KICK.WAV", 143000, std::nullopt, std::nullopt, {}}};
                DiskRecord cd;
                cd.handle = 1;
                cd.type = 2;
                cd.format = 3;
                cd.writable = false;
                cd.name = "CD1";
                sampler->setDisks({hard, cd});
            }
            mcp::ToolOptions options;
            options.allowDisk = allowDisk;
            options.allowDiskRefresh = allowRefresh;
            server = std::make_unique<mcp::McpServer>(mcp::ServerIdentity{"xs56k-mcp", "XS56K", "0.0.1", ""},
                                                      mcp::makeAllTools(gateway, mcp::ParameterCatalogue::standard(), options));
        }

        static mcp::GatewayConfig configFor(const SimulatedMidiBackend& backend)
        {
            mcp::GatewayConfig config;
            config.inputPort = backend.inputName();
            config.outputPort = backend.outputName();
            config.commandTimeout = COMMAND_TIMEOUT;
            config.diskTimeout = DISK_TIMEOUT;
            return config;
        }

        json request(const std::string& method, json params)
        {
            params["_meta"] = {{"io.modelcontextprotocol/protocolVersion", MODERN},
                               {"io.modelcontextprotocol/clientCapabilities", json::object()}};
            const json message{{"jsonrpc", "2.0"}, {"id", ++lastId}, {"method", method}, {"params", std::move(params)}};
            const auto answer = server->handleLine(message.dump());
            REQUIRE(answer.has_value());
            return json::parse(*answer);
        }

        json call(const std::string& tool, json arguments = json::object())
        {
            return request("tools/call", json{{"name", tool}, {"arguments", std::move(arguments)}});
        }

        std::size_t accepted(akm::ItemId item) const
        {
            std::size_t count = 0;
            const akm::ItemDescriptor& wanted = akm::descriptor(item);
            for (const auto& command : sampler->acceptedCommands())
            {
                if (command.section == wanted.section && command.item == wanted.item)
                    ++count;
            }
            return count;
        }

        akm::RealScheduler scheduler;
        SimulatedMidiBackend backend{scheduler};
        SimulatedSampler* sampler = nullptr;
        mcp::SamplerGateway gateway{backend, configFor(backend)};
        std::unique_ptr<mcp::McpServer> server;
        int lastId = 0;
    };

    std::string textOf(const json& answer)
    {
        REQUIRE(answer.contains("result"));
        return answer["result"]["content"][0]["text"].get<std::string>();
    }

    bool isError(const json& answer)
    {
        REQUIRE(answer.contains("result"));
        return answer["result"]["isError"].get<bool>();
    }

    bool contains(const std::string& text, const char* part)
    {
        return text.find(part) != std::string::npos;
    }

    std::set<std::string> toolNames(Rig& rig)
    {
        const json list = rig.request("tools/list", json::object());
        std::set<std::string> names;
        for (const json& tool : list["result"]["tools"])
            names.insert(tool["name"].get<std::string>());
        return names;
    }
}

TEST_CASE("Given the server launched without --allow-disk, When the tools are listed and a disk tool is called, Then no disk tool is listed and the call is an invalid-params error [RQ-MCP-023]",
          "[mcp][disk]")
{
    Rig rig(false);
    const std::set<std::string> names = toolNames(rig);
    for (const char* tool : {"list_disks", "select_disk", "list_disk_contents", "open_folder", "close_folder"})
        CHECK(names.count(tool) == 0);
    for (const char* tool : {"list_disks", "select_disk", "list_disk_contents", "open_folder", "close_folder"})
    {
        const json answer = rig.call(tool);
        REQUIRE(answer.contains("error"));
        CHECK(answer["error"]["code"] == INVALID_PARAMS);
    }
    CHECK(rig.accepted(akm::ItemId::DiskGetList) == 0);
}

TEST_CASE("Given the server launched with --allow-disk, When the tools are listed, Then the browsing tools are listed with their tiers [RQ-MCP-023, RQ-MCP-013]",
          "[mcp][disk]")
{
    Rig rig;
    const json list = rig.request("tools/list", json::object());
    std::set<std::string> names;
    for (const json& tool : list["result"]["tools"])
    {
        const std::string name = tool["name"].get<std::string>();
        names.insert(name);
        if (name == "list_disks" || name == "list_disk_contents")
            CHECK(tool["annotations"]["readOnlyHint"].get<bool>());
        if (name == "select_disk" || name == "open_folder" || name == "close_folder")
        {
            CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
            CHECK_FALSE(tool["annotations"]["destructiveHint"].get<bool>());
        }
    }
    for (const char* tool : {"list_disks", "select_disk", "list_disk_contents", "open_folder", "close_folder"})
        CHECK(names.count(tool) == 1);
}

TEST_CASE("Given two disks, When list_disks runs, Then both are listed with their type, format and whether they are writable, and the refresh is not sent [RQ-MCP-024]",
          "[mcp][disk]")
{
    Rig rig;
    const std::string text = textOf(rig.call("list_disks"));
    CHECK(contains(text, "Disks connected (2):"));
    CHECK(contains(text, "HD1 (hard disk, MSDOS, writable)"));
    CHECK(contains(text, "CD1 (CD-ROM, ISO9660, read-only)"));
    CHECK(rig.accepted(akm::ItemId::DiskUpdateList) == 0);
}

TEST_CASE("Given the refresh option and list_disks with refresh true, When it runs, Then the sampler is sent the refresh of its disk list once [RQ-MCP-024, RQ-MCP-031]",
          "[mcp][disk]")
{
    Rig rig(true, true, true);
    const json answer = rig.call("list_disks", {{"refresh", true}});
    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "Disks connected (2):"));
    CHECK(rig.accepted(akm::ItemId::DiskUpdateList) == 1);
}

TEST_CASE("Given --allow-disk without --allow-disk-refresh, When the tools are listed, Then list_disks has no refresh argument; with the option it has one [RQ-MCP-031]",
          "[mcp][disk]")
{
    const auto listDisksSchema = [](Rig& rig) {
        const json list = rig.request("tools/list", json::object());
        for (const json& tool : list["result"]["tools"])
            if (tool["name"] == "list_disks")
                return tool["inputSchema"]["properties"];
        return json{{"list_disks_is_not_listed", true}};
    };
    Rig without;
    const json withoutProperties = listDisksSchema(without);
    CHECK_FALSE(withoutProperties.contains("list_disks_is_not_listed"));
    CHECK_FALSE(withoutProperties.contains("refresh"));
    Rig with(true, true, true);
    CHECK(listDisksSchema(with).contains("refresh"));
}

TEST_CASE("Given --allow-disk without --allow-disk-refresh, When list_disks is called with refresh true, Then the answer is an error that names --allow-disk-refresh and the sampler is sent nothing [RQ-MCP-031]",
          "[mcp][disk]")
{
    Rig rig;
    const json answer = rig.call("list_disks", {{"refresh", true}});
    CHECK(isError(answer));
    CHECK(contains(textOf(answer), "--allow-disk-refresh"));
    CHECK(rig.accepted(akm::ItemId::DiskUpdateList) == 0);
    CHECK(rig.accepted(akm::ItemId::DiskGetList) == 0);
}

TEST_CASE("Given --allow-disk without --allow-disk-refresh, When list_disks is called with refresh false or a non-boolean, Then false lists the disks without the refresh and a non-boolean is refused [RQ-MCP-031]",
          "[mcp][disk]")
{
    Rig rig;
    const json plain = rig.call("list_disks", {{"refresh", false}});
    CHECK_FALSE(isError(plain));
    CHECK(contains(textOf(plain), "Disks connected (2):"));
    CHECK(isError(rig.call("list_disks", {{"refresh", "yes"}})));
    CHECK(rig.accepted(akm::ItemId::DiskUpdateList) == 0);
}

TEST_CASE("Given the refresh option and no disk listed, When list_disks runs, Then the answer offers the refresh; without the option it does not [RQ-MCP-031]",
          "[mcp][disk]")
{
    Rig with(true, false, true);
    CHECK(contains(textOf(with.call("list_disks")), "refresh true"));
    Rig without(true, false);
    const std::string text = textOf(without.call("list_disks"));
    CHECK(contains(text, "No disk is connected to the sampler."));
    CHECK_FALSE(contains(text, "refresh true"));
}

TEST_CASE("Given a sampler with no disk, When list_disks runs, Then it says no disk is connected [RQ-MCP-024]",
          "[mcp][disk]")
{
    Rig rig(true, false);
    const json answer = rig.call("list_disks");
    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "No disk is connected to the sampler."));
}

TEST_CASE("Given two disks, When select_disk is given a name, a handle, an unknown name or nothing, Then the disk is selected or the answer names the disks there are [RQ-MCP-024]",
          "[mcp][disk]")
{
    Rig rig;
    const json byName = rig.call("select_disk", {{"name", "HD1"}});
    CHECK_FALSE(isError(byName));
    CHECK(contains(textOf(byName), "Selected the disk \"HD1\" (hard disk, writable)"));
    CHECK(contains(textOf(rig.call("list_disks")), "HD1 (hard disk, MSDOS, writable, current)"));

    const json byHandle = rig.call("select_disk", {{"handle", 1}});
    CHECK_FALSE(isError(byHandle));
    CHECK(contains(textOf(byHandle), "Selected the disk \"CD1\" (CD-ROM, read-only)"));

    const json unknown = rig.call("select_disk", {{"name", "NOPE"}});
    CHECK(isError(unknown));
    CHECK(contains(textOf(unknown), "HD1"));
    CHECK(contains(textOf(unknown), "CD1"));
    CHECK(isError(rig.call("select_disk")));
    CHECK(isError(rig.call("select_disk", {{"name", "HD1"}, {"handle", 0}})));
    CHECK(isError(rig.call("select_disk", {{"handle", 9}})));
}

TEST_CASE("Given a selected disk, When list_disk_contents runs, Then the folders and the files with their sizes come with the path; with no disk selected it says to select one [RQ-MCP-024]",
          "[mcp][disk]")
{
    Rig rig;
    const json before = rig.call("list_disk_contents");
    CHECK(isError(before));
    CHECK(contains(textOf(before), "select_disk"));

    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "HD1"}})));
    const std::string text = textOf(rig.call("list_disk_contents"));
    CHECK(contains(text, "disk \"HD1\""));
    CHECK(contains(text, "(root)"));
    CHECK(contains(text, "Folders (2):"));
    CHECK(contains(text, "DRUMS"));
    CHECK(contains(text, "SYNTH"));
    CHECK(contains(text, "Files (2):"));
    CHECK(contains(text, "INIT.AKP (3000 bytes)"));
    CHECK(contains(text, "KICK.WAV (143000 bytes)"));
}

TEST_CASE("Given the root of a disk, When a folder is opened and closed, Then the listing follows it and a missing folder or the root cannot be left or opened [RQ-MCP-024, RQ-MCP-009]",
          "[mcp][disk]")
{
    Rig rig;
    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "HD1"}})));

    const json opened = rig.call("open_folder", {{"name", "DRUMS"}});
    INFO("opened: " << textOf(opened));
    CHECK_FALSE(isError(opened));
    CHECK(contains(textOf(opened), "DRUMS"));
    CHECK(contains(textOf(opened), "SNARE.WAV (2500 bytes)"));
    CHECK(contains(textOf(opened), "Folders: none"));
    CHECK(contains(textOf(rig.call("list_disk_contents")), "SNARE.WAV"));

    const json closed = rig.call("close_folder");
    INFO("closed: " << textOf(closed));
    CHECK_FALSE(isError(closed));
    CHECK(contains(textOf(closed), "(root)"));
    CHECK(contains(textOf(closed), "INIT.AKP"));

    const std::size_t opensBefore = rig.accepted(akm::ItemId::DiskOpenFolder);
    const json missing = rig.call("open_folder", {{"name", "NOPE"}});
    INFO("missing: " << textOf(missing));
    CHECK(isError(missing));
    CHECK(contains(textOf(missing), "DRUMS"));
    CHECK(rig.accepted(akm::ItemId::DiskOpenFolder) == opensBefore);

    const std::size_t closesBefore = rig.accepted(akm::ItemId::DiskCloseFolder);
    const json atRoot = rig.call("close_folder");
    CHECK(isError(atRoot));
    CHECK(contains(textOf(atRoot), "already the root"));
    CHECK(rig.accepted(akm::ItemId::DiskCloseFolder) == closesBefore);
    CHECK(isError(rig.call("open_folder")));
}

TEST_CASE("Given the server launched with and without --allow-disk, When the tools are listed, Then create_folder is a disk tool that is neither read-only nor destructive, and is absent without the flag [RQ-MCP-032]",
          "[mcp][disk]")
{
    Rig with;
    const json list = with.request("tools/list", json::object());
    int found = 0;
    for (const json& tool : list["result"]["tools"])
        if (tool["name"] == "create_folder")
        {
            ++found;
            CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
            CHECK_FALSE(tool["annotations"]["destructiveHint"].get<bool>());
            CHECK_FALSE(tool["annotations"]["idempotentHint"].get<bool>());
            CHECK(contains(tool["description"].get<std::string>(), "sampler's own disks"));
        }
    CHECK(found == 1);

    Rig without(false);
    CHECK(toolNames(without).count("create_folder") == 0);
    const json answer = without.call("create_folder", {{"name", "MCPTEST"}});
    REQUIRE(answer.contains("error"));
    CHECK(answer["error"]["code"] == INVALID_PARAMS);
}

TEST_CASE("Given a writable disk, When create_folder is given a new name, Then the folder is created once, listed, empty, and not opened [RQ-MCP-032]",
          "[mcp][disk]")
{
    Rig rig;
    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "HD1"}})));
    const json answer = rig.call("create_folder", {{"name", "MCPTEST"}});
    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "Created the folder \"MCPTEST\""));
    CHECK(contains(textOf(answer), "HD1"));
    CHECK(rig.accepted(akm::ItemId::DiskCreateFolder) == 1);

    const std::string listing = textOf(rig.call("list_disk_contents"));
    CHECK(contains(listing, "MCPTEST"));
    CHECK(contains(listing, "DRUMS"));
    CHECK(contains(listing, ": (root)"));  // created, not opened

    const std::string opened = textOf(rig.call("open_folder", {{"name", "MCPTEST"}}));
    CHECK(contains(opened, "MCPTEST"));
    CHECK(contains(opened, "Folders: none"));
    CHECK(contains(opened, "Files: none"));
}

TEST_CASE("Given a folder of that name already there, When create_folder runs, Then nothing is sent and the answer says it exists, whatever the case [RQ-MCP-032]",
          "[mcp][disk]")
{
    Rig rig;
    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "HD1"}})));
    for (const char* name : {"DRUMS", "drums", "Drums "})
    {
        const json answer = rig.call("create_folder", {{"name", name}});
        INFO(name);
        CHECK(isError(answer));
        CHECK(contains(textOf(answer), "already holds"));
        CHECK(contains(textOf(answer), "DRUMS"));
    }
    CHECK(rig.accepted(akm::ItemId::DiskCreateFolder) == 0);
}

TEST_CASE("Given a file bearing the name, no disk selected or a read-only disk, When create_folder runs, Then nothing is sent and the answer says why [RQ-MCP-032]",
          "[mcp][disk]")
{
    Rig rig;
    const json noDisk = rig.call("create_folder", {{"name", "MCPTEST"}});
    CHECK(isError(noDisk));
    CHECK(contains(textOf(noDisk), "select_disk"));

    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "HD1"}})));
    const json file = rig.call("create_folder", {{"name", "INIT.AKP"}});
    CHECK(isError(file));
    CHECK(contains(textOf(file), "INIT.AKP"));

    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "CD1"}})));
    const json readOnly = rig.call("create_folder", {{"name", "MCPTEST"}});
    CHECK(isError(readOnly));
    CHECK(contains(textOf(readOnly), "read-only"));
    CHECK(rig.accepted(akm::ItemId::DiskCreateFolder) == 0);
}

TEST_CASE("Given a missing, empty, non-string or path-like name, When create_folder runs, Then nothing is sent [RQ-MCP-032]",
          "[mcp][disk]")
{
    Rig rig;
    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "HD1"}})));
    CHECK(isError(rig.call("create_folder")));
    CHECK(isError(rig.call("create_folder", {{"name", ""}})));
    CHECK(isError(rig.call("create_folder", {{"name", "   "}})));
    CHECK(isError(rig.call("create_folder", {{"name", 12}})));
    for (const char* name : {"A/B", "A\\B", "..\\X"})
    {
        const json answer = rig.call("create_folder", {{"name", name}});
        INFO(name);
        CHECK(isError(answer));
        CHECK(contains(textOf(answer), "one folder name"));
    }
    CHECK(isError(rig.call("create_folder", {{"name", "OK"}, {"extra", 1}})));
    CHECK(rig.accepted(akm::ItemId::DiskCreateFolder) == 0);
}

TEST_CASE("Given a sampler that never answers the creation, When create_folder runs, Then the answer is an error and nothing else is sent [RQ-MCP-032]",
          "[mcp][disk]")
{
    Rig rig;
    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "HD1"}})));
    akm::harness::SamplerBehaviour behaviour;
    behaviour.silentItems.push_back(akm::harness::SilentItem{SECTION_DISK, ITEM_CREATE_FOLDER});
    rig.sampler->setBehaviour(behaviour);
    const json answer = rig.call("create_folder", {{"name", "MCPTEST"}});
    CHECK(isError(answer));
    CHECK(rig.accepted(akm::ItemId::DiskCreateFolder) == 1);
}

TEST_CASE("Given a refresh the sampler never answers, When list_disks refresh runs, Then it waits for the disk timeout, not the command timeout, and the answer mentions the power cycle and no retry [RQ-MCP-029]",
          "[mcp][disk]")
{
    Rig rig(true, true, true);
    akm::harness::SamplerBehaviour behaviour;
    behaviour.silentItems.push_back(akm::harness::SilentItem{SECTION_DISK, ITEM_UPDATE_DISK_LIST});
    rig.sampler->setBehaviour(behaviour);

    const auto started = std::chrono::steady_clock::now();
    const json answer = rig.call("list_disks", {{"refresh", true}});
    const auto waited = std::chrono::steady_clock::now() - started;

    CHECK(isError(answer));
    CHECK(contains(textOf(answer), "switched off and on"));
    CHECK(contains(textOf(answer), "not retried"));
    CHECK(waited >= DISK_TIMEOUT - std::chrono::milliseconds(200));
    CHECK(waited < DISK_TIMEOUT + std::chrono::seconds(6));
    CHECK(rig.accepted(akm::ItemId::DiskUpdateList) == 1);
}
