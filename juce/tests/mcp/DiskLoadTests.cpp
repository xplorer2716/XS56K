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

// The disk tools that load: load_file (with or without the files it depends on, and how a sample is loaded) and load_folder,
// with the counts before and after, the check that the name is there and the message of a silent sampler, over a real session
// and the simulated sampler. [TASK-MCP-020, RQ-MCP-025, RQ-MCP-029, ADR-MCP-003 (DEC-MCP-015, DEC-MCP-017)]
#include "DiskRig.hpp"

#include <chrono>
#include <memory>

using akm::harness::FolderRecord;
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
    constexpr std::uint8_t SECTION_DISK = 0x10;
    constexpr std::uint8_t ITEM_LOAD_FILE = 0x2A;
    constexpr int INVALID_PARAMS = -32602;

    /// A disk whose root holds INIT.AKP (a program named INIT), KICK.WAV (a sample named KICK), BIG.AKP (a program BIGPROG that
    /// depends on BIGSAMPLE.WAV, a sample BIGSAMPLE) and the folder SYNTH, which holds a program LEAD2 and a sample WAVE.
    std::unique_ptr<DiskRig> makeRig(bool allowDisk = true)
    {
        FolderRecord synth;
        synth.name = "SYNTH";
        synth.programFiles = {"LEAD2"};
        synth.sampleFiles = {"WAVE"};
        FolderRecord drums;
        drums.name = "DRUMS";
        return std::make_unique<DiskRig>(
            standardDisks({programFile("INIT.AKP", "INIT"), sampleFile("KICK.WAV", "KICK"),
                           programFile("BIG.AKP", "BIGPROG", 9000, {"BIGSAMPLE.WAV"}), sampleFile("BIGSAMPLE.WAV", "BIGSAMPLE")},
                          {drums, synth}),
            allowDisk);
    }
}

TEST_CASE("Given a disk holding a program file, When load_file is called on it, Then the program is in memory, the answer gives the counts before and after and names what was added [RQ-MCP-025]",
          "[mcp][disk][load]")
{
    const auto rigHolder = makeRig();
    DiskRig& rig = *rigHolder;
    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "HD1"}})));

    const json answer = rig.call("load_file", {{"name", "INIT.AKP"}});

    CHECK_FALSE(isError(answer));
    const std::string text = textOf(answer);
    CHECK(contains(text, "Loaded \"INIT.AKP\" from the disk \"HD1\""));
    CHECK(contains(text, "programs 4 (was 3, added INIT)"));
    CHECK(contains(text, "samples 0 (was 0)"));
    CHECK(contains(textOf(rig.call("list_programs")), "INIT"));
    CHECK(rig.accepted(akm::ItemId::DiskLoadFile) == 1);
    CHECK(rig.accepted(akm::ItemId::DiskLoadFileWithDependents) == 0);
}

TEST_CASE("Given a disk holding a sample file, When load_file is called on it, Then the sample is in memory and the list says so [RQ-MCP-025]",
          "[mcp][disk][load]")
{
    const auto rigHolder = makeRig();
    DiskRig& rig = *rigHolder;
    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "HD1"}})));

    const json answer = rig.call("load_file", {{"name", "KICK.WAV"}});

    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "samples 1 (was 0, added KICK)"));
    CHECK(contains(textOf(rig.call("list_samples")), "0: KICK"));
}

TEST_CASE("Given a program that depends on a sample, When it is loaded without and with with_dependents, Then only the program loads, then the sample too through the item that follows the dependents [RQ-MCP-025]",
          "[mcp][disk][load]")
{
    const auto aloneHolder = makeRig();
    DiskRig& alone = *aloneHolder;
    REQUIRE_FALSE(isError(alone.call("select_disk", {{"name", "HD1"}})));
    const json withoutDependents = alone.call("load_file", {{"name", "BIG.AKP"}});
    CHECK_FALSE(isError(withoutDependents));
    CHECK(contains(textOf(withoutDependents), "added BIGPROG"));
    CHECK(contains(textOf(withoutDependents), "samples 0 (was 0)"));
    CHECK(alone.accepted(akm::ItemId::DiskLoadFile) == 1);

    const auto togetherHolder = makeRig();
    DiskRig& together = *togetherHolder;
    REQUIRE_FALSE(isError(together.call("select_disk", {{"name", "HD1"}})));
    const json withDependents = together.call("load_file", {{"name", "BIG.AKP"}, {"with_dependents", true}});
    CHECK_FALSE(isError(withDependents));
    CHECK(contains(textOf(withDependents), "added BIGPROG"));
    CHECK(contains(textOf(withDependents), "samples 1 (was 0, added BIGSAMPLE)"));
    CHECK(together.accepted(akm::ItemId::DiskLoadFileWithDependents) == 1);
    CHECK(together.accepted(akm::ItemId::DiskLoadFile) == 0);
}

TEST_CASE("Given a sample file, When it is loaded as RAM or as virtual, Then the sampler is sent the mode, and another mode is refused [RQ-MCP-025]",
          "[mcp][disk][load]")
{
    const auto rigHolder = makeRig();
    DiskRig& rig = *rigHolder;
    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "HD1"}})));
    const akm::ItemDescriptor& item = akm::descriptor(akm::ItemId::DiskLoadFile);

    CHECK_FALSE(isError(rig.call("load_file", {{"name", "KICK.WAV"}, {"sample_mode", "ram"}})));
    CHECK_FALSE(isError(rig.call("load_file", {{"name", "KICK.WAV"}, {"sample_mode", "virtual"}})));
    CHECK_FALSE(isError(rig.call("load_file", {{"name", "KICK.WAV"}, {"sample_mode", "normal"}})));
    std::vector<std::uint8_t> modes;
    for (const auto& command : rig.sampler->acceptedCommands())
    {
        if (command.section == item.section && command.item == item.item)
            modes.push_back(command.data.back());
    }
    CHECK(modes == std::vector<std::uint8_t>{1, 2, 0});

    const std::size_t sent = rig.accepted(akm::ItemId::DiskLoadFile);
    CHECK(isError(rig.call("load_file", {{"name", "KICK.WAV"}, {"sample_mode", "huge"}})));
    CHECK(rig.accepted(akm::ItemId::DiskLoadFile) == sent);
}

TEST_CASE("Given a name that the current folder does not hold, or no disk selected, When load_file is called, Then nothing is sent and the answer lists the files, or says to select a disk [RQ-MCP-025, RQ-MCP-009]",
          "[mcp][disk][load]")
{
    const auto rigHolder = makeRig();
    DiskRig& rig = *rigHolder;
    const json none = rig.call("load_file", {{"name", "INIT.AKP"}});
    CHECK(isError(none));
    CHECK(contains(textOf(none), "select_disk"));

    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "HD1"}})));
    const json missing = rig.call("load_file", {{"name", "NOPE.AKP"}});
    CHECK(isError(missing));
    CHECK(contains(textOf(missing), "INIT.AKP"));
    CHECK(contains(textOf(missing), "KICK.WAV"));
    CHECK(rig.accepted(akm::ItemId::DiskLoadFile) == 0);
    CHECK(isError(rig.call("load_file")));
    CHECK(isError(rig.call("load_file", {{"name", 12}})));
    CHECK(isError(rig.call("load_file", {{"name", "INIT.AKP"}, {"with_dependents", "yes"}})));
}

TEST_CASE("Given a sub-folder, When load_folder is called, Then everything it holds is loaded and the answer gives the counts; a folder that is not there sends nothing [RQ-MCP-025]",
          "[mcp][disk][load]")
{
    const auto rigHolder = makeRig();
    DiskRig& rig = *rigHolder;
    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "HD1"}})));

    const json answer = rig.call("load_folder", {{"name", "SYNTH"}});

    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "Loaded the folder \"SYNTH\" from the disk \"HD1\""));
    CHECK(contains(textOf(answer), "programs 4 (was 3, added LEAD2)"));
    CHECK(contains(textOf(answer), "samples 1 (was 0, added WAVE)"));
    CHECK(rig.accepted(akm::ItemId::DiskLoadFolder) == 1);

    const std::size_t sent = rig.accepted(akm::ItemId::DiskLoadFolder);
    const json missing = rig.call("load_folder", {{"name", "NOPE"}});
    CHECK(isError(missing));
    CHECK(contains(textOf(missing), "SYNTH"));
    CHECK(rig.accepted(akm::ItemId::DiskLoadFolder) == sent);
}

TEST_CASE("Given a sampler that never answers a load, When load_file is called, Then it waits for the disk timeout and the answer mentions the power cycle and no retry [RQ-MCP-029, RQ-MCP-025]",
          "[mcp][disk][load]")
{
    const auto rigHolder = makeRig();
    DiskRig& rig = *rigHolder;
    REQUIRE_FALSE(isError(rig.call("select_disk", {{"name", "HD1"}})));
    akm::harness::SamplerBehaviour behaviour;
    behaviour.silentItems.push_back(akm::harness::SilentItem{SECTION_DISK, ITEM_LOAD_FILE});
    rig.sampler->setBehaviour(behaviour);

    const auto started = std::chrono::steady_clock::now();
    const json answer = rig.call("load_file", {{"name", "INIT.AKP"}});
    const auto waited = std::chrono::steady_clock::now() - started;

    CHECK(isError(answer));
    CHECK(contains(textOf(answer), "switched off and on"));
    CHECK(contains(textOf(answer), "not retried"));
    CHECK(waited >= mcp::test::DISK_RIG_DISK_TIMEOUT - std::chrono::milliseconds(200));
    CHECK(waited < mcp::test::DISK_RIG_DISK_TIMEOUT + std::chrono::seconds(6));
    CHECK(rig.accepted(akm::ItemId::DiskLoadFile) == 1);
}

TEST_CASE("Given the load tools, When the server is launched without --allow-disk or with it, Then they are absent, or listed with their tier: not read-only, not destructive [RQ-MCP-023, RQ-MCP-013]",
          "[mcp][disk][load]")
{
    const auto offHolder = makeRig(false);
    DiskRig& off = *offHolder;
    for (const char* tool : {"load_file", "load_folder"})
    {
        const json answer = off.call(tool, {{"name", "INIT.AKP"}});
        REQUIRE(answer.contains("error"));
        CHECK(answer["error"]["code"] == INVALID_PARAMS);
    }

    const auto onHolder = makeRig();
    DiskRig& on = *onHolder;
    const json message{{"jsonrpc", "2.0"},
                       {"id", 1},
                       {"method", "tools/list"},
                       {"params",
                        {{"_meta",
                          {{"io.modelcontextprotocol/protocolVersion", "2026-07-28"},
                           {"io.modelcontextprotocol/clientCapabilities", json::object()}}}}}};
    const json list = json::parse(*on.server->handleLine(message.dump()));
    int found = 0;
    for (const json& tool : list["result"]["tools"])
    {
        const std::string name = tool["name"].get<std::string>();
        if (name == "load_file" || name == "load_folder")
        {
            ++found;
            CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
            CHECK_FALSE(tool["annotations"]["destructiveHint"].get<bool>());
            CHECK_FALSE(tool["annotations"]["idempotentHint"].get<bool>());
            CHECK(contains(tool["description"].get<std::string>(), "sampler's own disks"));
        }
    }
    CHECK(found == 2);
}
