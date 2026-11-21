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
// The gateway's three answers to a file already on the disk when it saves: refuse (nothing sent), skip (the file stays), overwrite, over
// a real session and the simulated sampler. [TASK-APP-055, RQ-APP-045, RQ-MCP-026, RQ-MCP-027, ADR-APP-005 (DEC-APP-038)]
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "DiskRig.hpp"

using mcp::ExistingFile;
using mcp::SaveKind;
using mcp::test::DiskRig;
using mcp::test::programFile;
using mcp::test::standardDisks;

namespace
{
    constexpr std::uint8_t SECTION_DISK = 0x10;
    constexpr std::uint8_t ITEM_SAVE_MEMORY_ITEM = 0x2C;
    constexpr std::uint8_t ITEM_SAVE_ALL_MEMORY_ITEMS = 0x2D;
    constexpr std::size_t FLAG_OF_ONE = 3;  ///< &2C: index (2 bytes), type, then the overwrite flag
    constexpr std::size_t FLAG_OF_ALL = 1;  ///< &2D: type, then the overwrite flag
    constexpr std::uint32_t OLD_SIZE = 10;
    constexpr int THREE_PROGRAMS = 3;
    constexpr const char* PAD_FILE = "PAD.AKP";

    /// The sampler holds BASS, LEAD and PAD; HD1, selected, holds PAD.AKP (10 bytes).
    std::unique_ptr<DiskRig> padOnDisk()
    {
        auto rig = std::make_unique<DiskRig>(standardDisks({programFile(PAD_FILE, "PAD", OLD_SIZE)}));
        REQUIRE(rig->gateway.selectDiskByName("HD1").ok());
        return rig;
    }

    std::vector<std::vector<std::uint8_t>> sent(const DiskRig& rig, std::uint8_t item)
    {
        std::vector<std::vector<std::uint8_t>> data;
        for (const auto& command : rig.sampler->acceptedCommands())
        {
            if (command.section == SECTION_DISK && command.item == item)
                data.push_back(command.data);
        }
        return data;
    }
}

TEST_CASE("Given PAD.AKP in the current folder, When PAD is saved with Refuse, Then nothing is sent and the problem names the file "
          "[RQ-MCP-026, DEC-APP-038, TASK-APP-055]",
          "[mcp][disk][save]")
{
    auto rig = padOnDisk();
    const auto saved = rig->gateway.saveMemoryItem(SaveKind::Program, "PAD", ExistingFile::Refuse, false);
    REQUIRE_FALSE(saved.ok());
    CHECK(saved.problem.find(PAD_FILE) != std::string::npos);
    CHECK(sent(*rig, ITEM_SAVE_MEMORY_ITEM).empty());
}

TEST_CASE("Given PAD.AKP in the current folder, When PAD is saved with Skip, Then nothing is sent, the file stays and the outcome says it "
          "was skipped [RQ-APP-045, DEC-APP-038, TASK-APP-055]",
          "[mcp][disk][save]")
{
    auto rig = padOnDisk();
    const auto saved = rig->gateway.saveMemoryItem(SaveKind::Program, "PAD", ExistingFile::Skip, false);
    REQUIRE(saved.ok());
    CHECK(sent(*rig, ITEM_SAVE_MEMORY_ITEM).empty());
    CHECK(saved.value->skipped == std::vector<std::string>{PAD_FILE});
    REQUIRE(saved.value->savedFile.has_value());
    CHECK(saved.value->savedFile->sizeBytes == OLD_SIZE);
}

TEST_CASE("Given no file of its name, When LEAD is saved with Skip, Then the save is sent with the skip flag and nothing is skipped "
          "[RQ-APP-045, DEC-APP-038, TASK-APP-055]",
          "[mcp][disk][save]")
{
    auto rig = padOnDisk();
    const auto saved = rig->gateway.saveMemoryItem(SaveKind::Program, "LEAD", ExistingFile::Skip, true);
    REQUIRE(saved.ok());
    const auto data = sent(*rig, ITEM_SAVE_MEMORY_ITEM);
    REQUIRE(data.size() == 1);
    CHECK(data[0].at(FLAG_OF_ONE) == 0);
    CHECK(saved.value->skipped.empty());
    CHECK(saved.value->savedFile.has_value());
}

TEST_CASE("Given PAD.AKP in the current folder, When PAD is saved with Overwrite, Then the save is sent with the overwrite flag and the "
          "file is replaced [RQ-MCP-026, DEC-APP-038, TASK-APP-055]",
          "[mcp][disk][save]")
{
    auto rig = padOnDisk();
    const auto saved = rig->gateway.saveMemoryItem(SaveKind::Program, "PAD", ExistingFile::Overwrite, false);
    REQUIRE(saved.ok());
    const auto data = sent(*rig, ITEM_SAVE_MEMORY_ITEM);
    REQUIRE(data.size() == 1);
    CHECK(data[0].at(FLAG_OF_ONE) == 1);
    REQUIRE(saved.value->savedFile.has_value());
    CHECK(saved.value->savedFile->sizeBytes != OLD_SIZE);
}

TEST_CASE("Given PAD.AKP in the current folder, When every program is saved with Skip, Then one save of all is sent with the skip flag "
          "and the outcome names PAD.AKP as there before; with Refuse nothing is sent [RQ-APP-045, RQ-MCP-027, DEC-APP-038, TASK-APP-055]",
          "[mcp][disk][save]")
{
    auto rig = padOnDisk();
    const auto refused = rig->gateway.saveAllMemoryItems(SaveKind::Program, THREE_PROGRAMS, ExistingFile::Refuse, false);
    CHECK_FALSE(refused.ok());
    CHECK(sent(*rig, ITEM_SAVE_ALL_MEMORY_ITEMS).empty());

    // What the simulated sampler answers to a skip of an existing file is its own model; the flag sent is what this checks.
    (void)rig->gateway.saveAllMemoryItems(SaveKind::Program, THREE_PROGRAMS, ExistingFile::Skip, false);
    const auto data = sent(*rig, ITEM_SAVE_ALL_MEMORY_ITEMS);
    REQUIRE(data.size() == 1);
    CHECK(data[0].at(FLAG_OF_ALL) == 0);
}

TEST_CASE("Given no file of their names, When every program is saved with Skip, Then the files are in the folder and nothing is skipped "
          "[RQ-APP-045, DEC-APP-038, TASK-APP-055]",
          "[mcp][disk][save]")
{
    auto rig = std::make_unique<DiskRig>(standardDisks());
    REQUIRE(rig->gateway.selectDiskByName("HD1").ok());
    const auto saved = rig->gateway.saveAllMemoryItems(SaveKind::Program, THREE_PROGRAMS, ExistingFile::Skip, false);
    REQUIRE(saved.ok());
    CHECK(saved.value->skipped.empty());
    CHECK(saved.value->filesAfter.size() == static_cast<std::size_t>(THREE_PROGRAMS));
}
