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

#include <string>
#include <vector>

#include "mcp/McpServer.hpp"
#include "mcp/ParameterCatalogue.hpp"
#include "mcp/SamplerGateway.hpp"

namespace mcp
{
    // The six tools of the program editing server: `get_status`, `list_programs`, `select_program`,
    // `list_parameters`, `get_parameters` and `set_parameter`. They map JSON arguments to the gateway and the
    // catalogue and the answers back to plain text in the musician's vocabulary; none of them creates, renames,
    // deletes or saves anything. The gateway and the catalogue must outlive the tools. [RQ-MCP-004, RQ-MCP-005,
    // RQ-MCP-006, RQ-MCP-007, RQ-MCP-008, RQ-MCP-009, ADR-MCP-001 (DEC-MCP-006, DEC-MCP-007)]
    [[nodiscard]] std::vector<Tool> makeProgramEditingTools(SamplerGateway& gateway, const ParameterCatalogue& catalogue);

    // The structure tools of a program, in the sampler's memory: `create_program`, `rename_program` and `delete_program`
    // (which deletes the current program only when `confirm` is its name). They never save, load or touch the disk.
    // The gateway must outlive the tools. [RQ-MCP-013, RQ-MCP-015, RQ-MCP-016, RQ-MCP-017, ADR-MCP-002 (DEC-MCP-010,
    // DEC-MCP-011)]
    [[nodiscard]] std::vector<Tool> makeProgramStructureTools(SamplerGateway& gateway);

    /// Every tool the server offers: the editing tools then the structure tools. [ADR-MCP-002 (DEC-MCP-010)]
    [[nodiscard]] std::vector<Tool> makeAllTools(SamplerGateway& gateway, const ParameterCatalogue& catalogue);

    /// The guidance the server gives the model with its first answer: what the server is for and how to use the tools.
    [[nodiscard]] std::string programEditingInstructions();
}
