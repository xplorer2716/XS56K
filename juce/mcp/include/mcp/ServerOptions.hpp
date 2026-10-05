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
#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "mcp/SamplerGateway.hpp"

namespace mcp
{
    // The configuration of the server: its launch arguments and nothing else, so that an MCP client's server entry (the
    // command and its arguments) is the one place the MIDI ports are written. [RQ-MCP-002, ADR-MCP-001 (DEC-MCP-008)]

    struct ServerOptions
    {
        std::string inputPort;   ///< --in: what the sampler sends on
        std::string outputPort;  ///< --out: what the sampler receives on
        std::uint32_t deviceId = 0;
        std::chrono::milliseconds commandTimeout = std::chrono::duration_cast<std::chrono::milliseconds>(akm::DEFAULT_COMMAND_TIMEOUT);
        bool touchLcdSettings = true;  ///< false with --no-lcd
        bool allowDisk = false;        ///< --allow-disk: the disk tools are offered (ADR-MCP-003 DEC-MCP-015)
        bool allowDiskRefresh = false;  ///< --allow-disk-refresh: list_disks may send the refresh of the disk list (DEC-MCP-020)
        std::chrono::milliseconds diskTimeout{120000};  ///< --disk-timeout-ms: how long a slow section 10 command waits
        bool listPorts = false;
        bool help = false;
    };

    /// The options, or a usage error that names the argument at fault.
    struct ParsedArguments
    {
        ServerOptions options;
        std::string error;

        [[nodiscard]] bool ok() const { return error.empty(); }
    };

    /// Reads the arguments (without the program's name): `--in <port>` and `--out <port>` (required, except with
    /// `--list-ports` or `--help`), `--device-id <0-31>`, `--timeout-ms <1-60000>`, `--no-lcd`, `--allow-disk`, `--allow-disk-refresh` (an error
    /// without `--allow-disk`), `--disk-timeout-ms <1-1800000>`, `--list-ports`, `--help`.
    /// Each option also takes the `--name=value` form. [RQ-MCP-002]
    [[nodiscard]] ParsedArguments parseArguments(const std::vector<std::string>& arguments);

    [[nodiscard]] GatewayConfig gatewayConfigFrom(const ServerOptions& options);

    [[nodiscard]] std::string usageText();
}
