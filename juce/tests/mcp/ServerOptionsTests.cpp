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

// The launch arguments of the MCP server: the MIDI ports, the DeviceID, the command timeout and the LCD switch, as a
// pure function from the arguments to the configuration or a usage error. [TASK-MCP-006, RQ-MCP-002,
// ADR-MCP-001 (DEC-MCP-008)]
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "mcp/ServerOptions.hpp"

using mcp::ParsedArguments;
using mcp::parseArguments;

namespace
{
    bool contains(const std::string& text, const char* part)
    {
        return text.find(part) != std::string::npos;
    }
}

TEST_CASE("Given the arguments --in A --out B --device-id 2 --timeout-ms 3000, When they are parsed, Then the configuration carries port A, port B, DeviceID 2 and 3000 ms [RQ-MCP-002]",
          "[mcp][options]")
{
    const ParsedArguments parsed = parseArguments({"--in", "A", "--out", "B", "--device-id", "2", "--timeout-ms", "3000"});

    REQUIRE(parsed.ok());
    CHECK(parsed.options.inputPort == "A");
    CHECK(parsed.options.outputPort == "B");
    CHECK(parsed.options.deviceId == 2);
    CHECK(parsed.options.commandTimeout == std::chrono::milliseconds(3000));
    CHECK(parsed.options.screen == mcp::ScreenMode::Independent);
    CHECK_FALSE(parsed.options.listPorts);
    CHECK_FALSE(parsed.options.help);
}

TEST_CASE("Given only the two ports, When they are parsed, Then the DeviceID is 0 and the timeout is the session's default [RQ-MCP-002]",
          "[mcp][options]")
{
    const ParsedArguments parsed = parseArguments({"--in", "MIDIIN2 (ESI M8U eX)", "--out", "MIDIOUT15 (ESI M8U eX)"});

    REQUIRE(parsed.ok());
    CHECK(parsed.options.inputPort == "MIDIIN2 (ESI M8U eX)");
    CHECK(parsed.options.outputPort == "MIDIOUT15 (ESI M8U eX)");
    CHECK(parsed.options.deviceId == 0);
    CHECK(parsed.options.commandTimeout == std::chrono::milliseconds(2000));
}

TEST_CASE("Given the option form --in=A, When it is parsed, Then it is the same as --in A [RQ-MCP-002]",
          "[mcp][options]")
{
    const ParsedArguments parsed = parseArguments({"--in=A", "--out=B", "--device-id=3", "--screen=as-is"});

    REQUIRE(parsed.ok());
    CHECK(parsed.options.inputPort == "A");
    CHECK(parsed.options.outputPort == "B");
    CHECK(parsed.options.deviceId == 3);
    CHECK(parsed.options.screen == mcp::ScreenMode::AsIs);
}

TEST_CASE("Given no --out or no --in, When parsed, Then the result is a usage error naming it [RQ-MCP-002]",
          "[mcp][options]")
{
    const ParsedArguments noOut = parseArguments({"--in", "A"});
    CHECK_FALSE(noOut.ok());
    CHECK(contains(noOut.error, "--out"));

    const ParsedArguments noIn = parseArguments({"--out", "B"});
    CHECK_FALSE(noIn.ok());
    CHECK(contains(noIn.error, "--in"));

    CHECK_FALSE(parseArguments({}).ok());
}

TEST_CASE("Given a DeviceID that is not 0 to 31, When parsed, Then the result is a usage error [RQ-MCP-002]",
          "[mcp][options]")
{
    for (const char* bad : {"99", "32", "-1", "abc", "", "1.5", "2x"})
    {
        CAPTURE(bad);
        const ParsedArguments parsed = parseArguments({"--in", "A", "--out", "B", "--device-id", bad});
        CHECK_FALSE(parsed.ok());
        CHECK(contains(parsed.error, "--device-id"));
    }
    CHECK(parseArguments({"--in", "A", "--out", "B", "--device-id", "31"}).ok());
    CHECK(parseArguments({"--in", "A", "--out", "B", "--device-id", "0"}).ok());
}

TEST_CASE("Given a timeout that is not a positive number of milliseconds up to a minute, When parsed, Then the result is a usage error [RQ-MCP-002]",
          "[mcp][options]")
{
    for (const char* bad : {"0", "-5", "abc", "60001", ""})
    {
        CAPTURE(bad);
        const ParsedArguments parsed = parseArguments({"--in", "A", "--out", "B", "--timeout-ms", bad});
        CHECK_FALSE(parsed.ok());
        CHECK(contains(parsed.error, "--timeout-ms"));
    }
    CHECK(parseArguments({"--in", "A", "--out", "B", "--timeout-ms", "60000"}).ok());
}

TEST_CASE("Given --screen with each of its three values, When parsed, Then the mode is carried to the gateway's configuration [RQ-MCP-045]",
          "[mcp][options]")
{
    const std::vector<std::pair<const char*, mcp::ScreenMode>> modes{{"independent", mcp::ScreenMode::Independent},
                                                                     {"follow", mcp::ScreenMode::Follow},
                                                                     {"as-is", mcp::ScreenMode::AsIs}};
    for (const auto& [name, mode] : modes)
    {
        CAPTURE(name);
        const ParsedArguments spaced = parseArguments({"--in", "A", "--out", "B", "--screen", name});
        REQUIRE(spaced.ok());
        CHECK(spaced.options.screen == mode);
        CHECK(mcp::gatewayConfigFrom(spaced.options).screen == mode);
        const ParsedArguments joined = parseArguments({"--in", "A", "--out", "B", std::string("--screen=") + name});
        REQUIRE(joined.ok());
        CHECK(joined.options.screen == mode);
    }
}

TEST_CASE("Given a --screen value that is none of the three, or none, When parsed, Then the result is a usage error naming the three values [RQ-MCP-045]",
          "[mcp][options]")
{
    for (const char* bad : {"sideways", "", "Follow", "asis", "off", "1"})
    {
        CAPTURE(bad);
        const ParsedArguments parsed = parseArguments({"--in", "A", "--out", "B", "--screen", bad});
        CHECK_FALSE(parsed.ok());
        CHECK(contains(parsed.error, "--screen"));
        CHECK(contains(parsed.error, "independent"));
        CHECK(contains(parsed.error, "follow"));
        CHECK(contains(parsed.error, "as-is"));
    }
    const ParsedArguments missing = parseArguments({"--in", "A", "--out", "B", "--screen"});
    CHECK_FALSE(missing.ok());
    CHECK(contains(missing.error, "--screen"));
}

TEST_CASE("Given the former --no-lcd, When parsed, Then it is refused and the message names --screen as-is [RQ-MCP-045]",
          "[mcp][options]")
{
    const ParsedArguments parsed = parseArguments({"--in", "A", "--out", "B", "--no-lcd"});

    CHECK_FALSE(parsed.ok());
    CHECK(contains(parsed.error, "--no-lcd"));
    CHECK(contains(parsed.error, "--screen as-is"));
}

TEST_CASE("Given --list-ports or --help alone, When parsed, Then the ports are not required [RQ-MCP-002]",
          "[mcp][options]")
{
    const ParsedArguments list = parseArguments({"--list-ports"});
    REQUIRE(list.ok());
    CHECK(list.options.listPorts);

    const ParsedArguments help = parseArguments({"--help"});
    REQUIRE(help.ok());
    CHECK(help.options.help);
    CHECK(parseArguments({"-h"}).options.help);
}

TEST_CASE("Given an unknown option, a value that is missing or a stray word, When parsed, Then the result is a usage error naming it [RQ-MCP-002]",
          "[mcp][options]")
{
    const ParsedArguments unknown = parseArguments({"--in", "A", "--out", "B", "--loud"});
    CHECK_FALSE(unknown.ok());
    CHECK(contains(unknown.error, "--loud"));

    const ParsedArguments missing = parseArguments({"--out", "B", "--in"});
    CHECK_FALSE(missing.ok());
    CHECK(contains(missing.error, "--in"));

    const ParsedArguments stray = parseArguments({"--in", "A", "--out", "B", "extra"});
    CHECK_FALSE(stray.ok());
    CHECK(contains(stray.error, "extra"));

    CHECK_FALSE(parseArguments({"--in", "", "--out", "B"}).ok());
}

TEST_CASE("Given parsed options, When the gateway configuration is built, Then it carries the ports, the DeviceID and the timeout [RQ-MCP-002]",
          "[mcp][options]")
{
    const ParsedArguments parsed = parseArguments({"--in", "A", "--out", "B", "--device-id", "4", "--timeout-ms", "1500"});
    REQUIRE(parsed.ok());

    const mcp::GatewayConfig config = mcp::gatewayConfigFrom(parsed.options);

    CHECK(config.inputPort == "A");
    CHECK(config.outputPort == "B");
    CHECK(config.deviceId == 4);
    CHECK(config.commandTimeout == std::chrono::milliseconds(1500));
    CHECK(config.screen == mcp::ScreenMode::Independent);
}

TEST_CASE("Given the usage text, When it is read, Then it names every option and says that the ports are the configuration [RQ-MCP-002]",
          "[mcp][options]")
{
    const std::string usage = mcp::usageText();

    for (const char* option : {"--in", "--out", "--device-id", "--timeout-ms", "--screen", "--list-ports", "--help"})
    {
        CAPTURE(option);
        CHECK(contains(usage, option));
    }
    CHECK_FALSE(contains(usage, "--no-lcd"));
}

TEST_CASE("Given the usage text, When it is read, Then it gives the three values of --screen, the default, and what the close puts back [RQ-MCP-045]",
          "[mcp][options]")
{
    const std::string usage = mcp::usageText();

    for (const char* word : {"independent", "follow", "as-is", "default", "puts"})
    {
        CAPTURE(word);
        CHECK(contains(usage, word));
    }
}

TEST_CASE("Given no disk argument, When the arguments are parsed, Then the disk tools are off and the disk timeout is 120000 ms [RQ-MCP-023, RQ-MCP-029]",
          "[mcp][options]")
{
    const ParsedArguments parsed = parseArguments({"--in", "A", "--out", "B"});

    REQUIRE(parsed.ok());
    CHECK_FALSE(parsed.options.allowDisk);
    CHECK_FALSE(parsed.options.allowDiskRefresh);
    CHECK(parsed.options.diskTimeout == std::chrono::milliseconds(120000));
    CHECK(mcp::gatewayConfigFrom(parsed.options).diskTimeout == std::chrono::milliseconds(120000));
}

TEST_CASE("Given --allow-disk and --disk-timeout-ms 500, When the arguments are parsed, Then the disk tools are on, with a 500 ms disk timeout the gateway carries [RQ-MCP-023, RQ-MCP-029]",
          "[mcp][options]")
{
    const ParsedArguments parsed = parseArguments({"--in", "A", "--out", "B", "--allow-disk", "--disk-timeout-ms", "500"});

    REQUIRE(parsed.ok());
    CHECK(parsed.options.allowDisk);
    CHECK(parsed.options.diskTimeout == std::chrono::milliseconds(500));
    CHECK(mcp::gatewayConfigFrom(parsed.options).diskTimeout == std::chrono::milliseconds(500));
    CHECK(parseArguments({"--in=A", "--out=B", "--allow-disk", "--disk-timeout-ms=900000"}).options.diskTimeout ==
          std::chrono::milliseconds(900000));
}

TEST_CASE("Given a bad disk timeout or a value given to --allow-disk, When the arguments are parsed, Then the error names the argument [RQ-MCP-029]",
          "[mcp][options]")
{
    for (const char* bad : {"0", "-5", "abc", "1800001", ""})
    {
        const ParsedArguments parsed = parseArguments({"--in", "A", "--out", "B", "--disk-timeout-ms", bad});
        INFO(bad);
        CHECK_FALSE(parsed.ok());
        CHECK(contains(parsed.error, "--disk-timeout-ms"));
    }
    const ParsedArguments value = parseArguments({"--in", "A", "--out", "B", "--allow-disk=yes"});
    CHECK_FALSE(value.ok());
    CHECK(contains(value.error, "--allow-disk"));
    CHECK_FALSE(parseArguments({"--in", "A", "--out", "B", "--disk-timeout-ms"}).ok());
}

TEST_CASE("Given --allow-disk and --allow-disk-refresh, When the arguments are parsed, Then both are on [RQ-MCP-031]",
          "[mcp][options]")
{
    const ParsedArguments parsed = parseArguments({"--in", "A", "--out", "B", "--allow-disk", "--allow-disk-refresh"});

    REQUIRE(parsed.ok());
    CHECK(parsed.options.allowDisk);
    CHECK(parsed.options.allowDiskRefresh);
    // either order
    CHECK(parseArguments({"--in", "A", "--out", "B", "--allow-disk-refresh", "--allow-disk"}).options.allowDiskRefresh);
}

TEST_CASE("Given --allow-disk-refresh without --allow-disk or with a value, When the arguments are parsed, Then the error names the options [RQ-MCP-031]",
          "[mcp][options]")
{
    const ParsedArguments alone = parseArguments({"--in", "A", "--out", "B", "--allow-disk-refresh"});
    CHECK_FALSE(alone.ok());
    CHECK(contains(alone.error, "--allow-disk-refresh"));
    CHECK(contains(alone.error, "--allow-disk"));

    const ParsedArguments value = parseArguments({"--in", "A", "--out", "B", "--allow-disk", "--allow-disk-refresh=yes"});
    CHECK_FALSE(value.ok());
    CHECK(contains(value.error, "--allow-disk-refresh"));
}

TEST_CASE("Given the usage text, When it is read, Then it names --allow-disk-refresh and says what the refresh did to a real sampler [RQ-MCP-031]",
          "[mcp][options]")
{
    const std::string usage = mcp::usageText();
    CHECK(contains(usage, "--allow-disk-refresh"));
    CHECK(contains(usage, "SCSI2SD"));
}

TEST_CASE("Given the usage text, When it is read, Then it names --allow-disk and --disk-timeout-ms and warns about the hang [RQ-MCP-023, RQ-MCP-029]",
          "[mcp][options]")
{
    const std::string usage = mcp::usageText();
    CHECK(contains(usage, "--allow-disk"));
    CHECK(contains(usage, "--disk-timeout-ms"));
    CHECK(contains(usage, "switched off and on"));
}

TEST_CASE("Given no front-panel argument, When the arguments are parsed, Then the key tools are off; with --allow-front-panel they are on, and a value given to it is an error [RQ-MCP-054]",
          "[mcp][options]")
{
    CHECK_FALSE(parseArguments({"--in", "A", "--out", "B"}).options.allowFrontPanel);
    const ParsedArguments parsed = parseArguments({"--in", "A", "--out", "B", "--allow-front-panel"});
    REQUIRE(parsed.ok());
    CHECK(parsed.options.allowFrontPanel);
    CHECK_FALSE(parsed.options.allowDisk);
    const ParsedArguments value = parseArguments({"--in", "A", "--out", "B", "--allow-front-panel=yes"});
    CHECK_FALSE(value.ok());
    CHECK(contains(value.error, "--allow-front-panel"));
    CHECK(contains(mcp::usageText(), "--allow-front-panel"));
}
