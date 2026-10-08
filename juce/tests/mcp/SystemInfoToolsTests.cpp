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

// The information tools of the MCP server: get_system_info (model, operating system, free memory) and get_disk_space (the free
// space of the current disk, with --allow-disk), over a real session and the simulated sampler. [TASK-MCP-034, RQ-MCP-040,
// ADR-MCP-004 (DEC-MCP-024)]
#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include <nlohmann/json.hpp>

#include "DiskRig.hpp"
#include "ToolRig.hpp"

using json = nlohmann::json;
using mcp::test::DiskRig;
using mcp::test::hasText;
using mcp::test::standardDisks;
using mcp::test::ToolRig;
using mcp::test::toolFailed;
using mcp::test::toolText;

namespace
{
    constexpr std::uint32_t WAVE_TOTAL_BYTES = 64u * 1024 * 1024;
    constexpr std::uint32_t WAVE_FREE_BYTES = 16u * 1024 * 1024;
    constexpr std::uint8_t MPKS_FREE_PERCENT = 80;
    constexpr std::uint8_t S5000_CODE = 0;
    constexpr std::uint8_t S6000_CODE = 1;
    constexpr std::uint8_t UNKNOWN_MODEL_CODE = 7;
}

TEST_CASE("Given the server, When the tools are listed, Then get_system_info is a read, and get_disk_space is a read that exists only with --allow-disk [RQ-MCP-040, RQ-MCP-013]",
          "[mcp][sysinfo]")
{
    ToolRig rig;
    const json info = rig.tool("get_system_info");
    REQUIRE_FALSE(info.is_null());
    CHECK(info["annotations"]["readOnlyHint"].get<bool>());
    CHECK(rig.tool("get_disk_space").is_null());

    ToolRig withDisk(true);
    const json space = withDisk.tool("get_disk_space");
    REQUIRE_FALSE(space.is_null());
    CHECK(space["annotations"]["readOnlyHint"].get<bool>());
    const json answer = rig.call("get_disk_space");
    REQUIRE(answer.contains("error"));
    CHECK(answer["error"]["code"] == mcp::test::TOOL_RIG_INVALID_PARAMS);
}

TEST_CASE("Given a sampler with 16 MiB of 64 MiB of wave memory free and 80 percent of the other memory, When get_system_info is called, Then the answer gives the model, the operating system and both memories [RQ-MCP-040]",
          "[mcp][sysinfo]")
{
    ToolRig rig;
    rig.sampler->setModel(S5000_CODE);
    rig.sampler->setMemory(WAVE_TOTAL_BYTES, WAVE_FREE_BYTES, MPKS_FREE_PERCENT);
    const json answer = rig.call("get_system_info");
    CHECK_FALSE(toolFailed(answer));
    const std::string text = toolText(answer);
    CHECK(hasText(text, "Model: AKAI S5000"));
    CHECK(hasText(text, "Operating system: "));
    CHECK(hasText(text, "Free wave memory: "));
    CHECK(hasText(text, "16777216 bytes of 67108864"));
    CHECK(hasText(text, "%"));
    CHECK(hasText(text, "Free program, keygroup, sample and multi memory: 80%"));
}

TEST_CASE("Given an S6000 or a model code that is none of them, When get_system_info is called, Then the model is named or said not to be recognised [RQ-MCP-040]",
          "[mcp][sysinfo]")
{
    ToolRig rig;
    rig.sampler->setModel(S6000_CODE);
    CHECK(hasText(toolText(rig.call("get_system_info")), "Model: AKAI S6000"));
    rig.sampler->setModel(UNKNOWN_MODEL_CODE);
    const std::string text = toolText(rig.call("get_system_info"));
    CHECK(hasText(text, "Model: not recognised"));
    CHECK(hasText(text, "Free wave memory: "));
}

TEST_CASE("Given an unexpected argument, When get_system_info or get_disk_space is called, Then it is refused [RQ-MCP-040]",
          "[mcp][sysinfo]")
{
    ToolRig rig(true);
    CHECK(toolFailed(rig.call("get_system_info", {{"extra", 1}})));
    CHECK(toolFailed(rig.call("get_disk_space", {{"extra", 1}})));
}

TEST_CASE("Given a disk for which the sampler reports 0 bytes free, When get_disk_space is called, Then the answer says the figure is probably not reported for this kind of disk and gives no size [RQ-MCP-040]",
          "[mcp][sysinfo]")
{
    auto disks = standardDisks();
    disks.front().freeBytes = 0;
    DiskRig rig(std::move(disks), true);
    REQUIRE_FALSE(mcp::test::isError(rig.call("select_disk", {{"name", "HD1"}})));

    const json answer = rig.call("get_disk_space");

    CHECK_FALSE(mcp::test::isError(answer));
    const std::string text = mcp::test::textOf(answer);
    CHECK(mcp::test::contains(text, "reports 0 bytes free"));
    CHECK(mcp::test::contains(text, "\"HD1\""));
    CHECK(mcp::test::contains(text, "probably does not report"));
    CHECK_FALSE(mcp::test::contains(text, "MB"));
}

TEST_CASE("Given a disk with 1000000 bytes free, When get_disk_space is called, Then the answer gives them for that disk; with no disk selected it says to select one [RQ-MCP-040]",
          "[mcp][sysinfo]")
{
    DiskRig rig(standardDisks(), true);
    const json noDisk = rig.call("get_disk_space");
    CHECK(mcp::test::isError(noDisk));
    CHECK(mcp::test::contains(mcp::test::textOf(noDisk), "select_disk"));

    REQUIRE_FALSE(mcp::test::isError(rig.call("select_disk", {{"name", "HD1"}})));
    const json answer = rig.call("get_disk_space");
    CHECK_FALSE(mcp::test::isError(answer));
    CHECK(mcp::test::contains(mcp::test::textOf(answer), "The disk \"HD1\" has 1000000 bytes free"));
    CHECK(mcp::test::contains(mcp::test::textOf(answer), "MB"));
}
