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

// The audition tools of the MCP server: audition_sample (the current sample) and audition_file (a file of the current folder of the
// current disk, with --allow-disk), start and stop, over a real session and the simulated sampler. [TASK-MCP-035, RQ-MCP-041,
// ADR-MCP-004 (DEC-MCP-024)]
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include <nlohmann/json.hpp>

#include "DiskRig.hpp"
#include "ToolRig.hpp"

using json = nlohmann::json;
using mcp::test::DiskRig;
using mcp::test::hasText;
using mcp::test::programFile;
using mcp::test::sampleFile;
using mcp::test::standardDisks;
using mcp::test::ToolRig;
using mcp::test::toolFailed;
using mcp::test::toolText;

namespace
{
    constexpr std::uint8_t FILE_INDEX_OF_KICK = 1;

    /// A rig whose disk HD1 (selected) holds INIT.AKP and KICK.WAV.
    std::unique_ptr<DiskRig> makeDiskRig(bool select = true)
    {
        auto rig = std::make_unique<DiskRig>(standardDisks({programFile("INIT.AKP", "INIT"), sampleFile("KICK.WAV", "KICK")}), true);
        if (select)
            REQUIRE_FALSE(mcp::test::isError(rig->call("select_disk", {{"name", "HD1"}})));
        return rig;
    }
}

TEST_CASE("Given the server, When the tools are listed, Then audition_sample is an edit and audition_file exists only with --allow-disk [RQ-MCP-041, RQ-MCP-013]",
          "[mcp][audition]")
{
    ToolRig rig;
    const json sample = rig.tool("audition_sample");
    REQUIRE_FALSE(sample.is_null());
    CHECK_FALSE(sample["annotations"]["readOnlyHint"].get<bool>());
    CHECK_FALSE(sample["annotations"]["destructiveHint"].get<bool>());
    CHECK(rig.tool("audition_file").is_null());
    const json refused = rig.call("audition_file", {{"action", "stop"}});
    REQUIRE(refused.contains("error"));
    CHECK(refused["error"]["code"] == mcp::test::TOOL_RIG_INVALID_PARAMS);

    ToolRig withDisk(true);
    const json file = withDisk.tool("audition_file");
    REQUIRE_FALSE(file.is_null());
    CHECK_FALSE(file["annotations"]["destructiveHint"].get<bool>());
}

TEST_CASE("Given KICK is the current sample, When audition_sample starts and then stops, Then the sampler is sent the start and then the stop [RQ-MCP-041]",
          "[mcp][audition]")
{
    ToolRig rig;
    REQUIRE_FALSE(toolFailed(rig.call("select_sample", {{"name", "KICK"}})));
    const json start = rig.call("audition_sample", {{"action", "start"}});
    CHECK_FALSE(toolFailed(start));
    CHECK(hasText(toolText(start), "Started the audition of the sample \"KICK\""));
    CHECK(hasText(toolText(start), "until it is stopped"));
    CHECK(rig.accepted(akm::ItemId::SampleStartAudition) == 1);
    CHECK(rig.accepted(akm::ItemId::SampleStopAudition) == 0);
    const json stop = rig.call("audition_sample", {{"action", "stop"}});
    CHECK_FALSE(toolFailed(stop));
    CHECK(hasText(toolText(stop), "Stopped the audition"));
    CHECK(rig.accepted(akm::ItemId::SampleStopAudition) == 1);
}

TEST_CASE("Given no current sample or a bad action, When audition_sample is called, Then nothing is sent [RQ-MCP-041]", "[mcp][audition]")
{
    ToolRig rig;
    const json noSample = rig.call("audition_sample", {{"action", "start"}});
    CHECK(toolFailed(noSample));
    CHECK(hasText(toolText(noSample), "select_sample"));
    REQUIRE_FALSE(toolFailed(rig.call("select_sample", {{"name", "KICK"}})));
    for (const json& bad : {json("play"), json(""), json(1), json(nullptr)})
    {
        INFO(bad.dump());
        CHECK(toolFailed(rig.call("audition_sample", {{"action", bad}})));
    }
    CHECK(toolFailed(rig.call("audition_sample")));
    CHECK(toolFailed(rig.call("audition_sample", {{"action", "start"}, {"extra", 1}})));
    CHECK(rig.accepted(akm::ItemId::SampleStartAudition) == 0);
    CHECK(rig.accepted(akm::ItemId::SampleStopAudition) == 0);
}

TEST_CASE("Given KICK.WAV in the current folder, When audition_file starts it and then stops, Then the sampler is sent the start with the file's position and then the stop [RQ-MCP-041]",
          "[mcp][audition]")
{
    const auto holder = makeDiskRig();
    DiskRig& rig = *holder;
    const json start = rig.call("audition_file", {{"action", "start"}, {"name", "kick.wav"}});
    CHECK_FALSE(mcp::test::isError(start));
    CHECK(mcp::test::contains(mcp::test::textOf(start), "Started the audition of the file \"KICK.WAV\""));
    CHECK(mcp::test::contains(mcp::test::textOf(start), "until it is stopped"));
    REQUIRE(rig.accepted(akm::ItemId::DiskStartFileAudition) == 1);
    std::vector<std::uint8_t> data;
    const akm::ItemDescriptor& wanted = akm::descriptor(akm::ItemId::DiskStartFileAudition);
    for (const auto& command : rig.sampler->acceptedCommands())
    {
        if (command.section == wanted.section && command.item == wanted.item)
            data = command.data;
    }
    REQUIRE_FALSE(data.empty());
    CHECK(data.back() == FILE_INDEX_OF_KICK);
    const json stop = rig.call("audition_file", {{"action", "stop"}});
    CHECK_FALSE(mcp::test::isError(stop));
    CHECK(mcp::test::contains(mcp::test::textOf(stop), "Stopped the audition"));
    CHECK(rig.accepted(akm::ItemId::DiskStopFileAudition) == 1);
}

TEST_CASE("Given a file that is not there, no name, no disk or a bad action, When audition_file is called, Then nothing is sent and the answer says why [RQ-MCP-041]",
          "[mcp][audition]")
{
    const auto holder = makeDiskRig();
    DiskRig& rig = *holder;
    const json unknown = rig.call("audition_file", {{"action", "start"}, {"name", "NOPE.WAV"}});
    CHECK(mcp::test::isError(unknown));
    CHECK(mcp::test::contains(mcp::test::textOf(unknown), "KICK.WAV"));
    CHECK(mcp::test::isError(rig.call("audition_file", {{"action", "start"}})));
    CHECK(mcp::test::isError(rig.call("audition_file", {{"action", "start"}, {"name", 5}})));
    CHECK(mcp::test::isError(rig.call("audition_file", {{"action", "play"}, {"name", "KICK.WAV"}})));
    CHECK(mcp::test::isError(rig.call("audition_file")));
    CHECK(rig.accepted(akm::ItemId::DiskStartFileAudition) == 0);

    const auto bare = makeDiskRig(false);
    const json noDisk = bare->call("audition_file", {{"action", "start"}, {"name", "KICK.WAV"}});
    CHECK(mcp::test::isError(noDisk));
    CHECK(mcp::test::contains(mcp::test::textOf(noDisk), "select_disk"));
    CHECK(bare->accepted(akm::ItemId::DiskStartFileAudition) == 0);
}
