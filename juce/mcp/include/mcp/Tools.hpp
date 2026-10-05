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
    /// The catalogues of the other domains, for the `domain` argument of `list_parameters`; a null one is not offered.
    struct ExtraCatalogues
    {
        const ParameterCatalogue* sample = nullptr;
        const ParameterCatalogue* multi = nullptr;
    };

    // The six tools of the program editing server: `get_status`, `list_programs`, `select_program`,
    // `list_parameters`, `get_parameters` and `set_parameter`. They map JSON arguments to the gateway and the
    // catalogue and the answers back to plain text in the musician's vocabulary; none of them creates, renames,
    // deletes or saves anything. The gateway and the catalogue must outlive the tools. [RQ-MCP-004, RQ-MCP-005,
    // RQ-MCP-006, RQ-MCP-007, RQ-MCP-008, RQ-MCP-009, ADR-MCP-001 (DEC-MCP-006, DEC-MCP-007)]
    [[nodiscard]] std::vector<Tool> makeProgramEditingTools(SamplerGateway& gateway, const ParameterCatalogue& catalogue,
                                                            ExtraCatalogues extra = {});

    // The sample tools: `list_samples`, `select_sample`, `get_sample_parameters` and `set_sample_parameter`. They act on the
    // sampler's current sample in memory; none creates, deletes, renames or loads a sample. The gateway and the catalogue
    // must outlive the tools. [RQ-MCP-013, RQ-MCP-020, ADR-MCP-002 (DEC-MCP-010, DEC-MCP-013)]
    [[nodiscard]] std::vector<Tool> makeSampleTools(SamplerGateway& gateway, const ParameterCatalogue& sampleCatalogue);

    // The multi tools: `list_multis`, `select_multi`, `get_multi_parameters` and `set_multi_parameter`. They act on the
    // sampler's current multi in memory, part by part (parts are numbered from 1); none creates, deletes, renames or
    // assigns a multi, and Delete ALL Multis is never sent. The gateway and the catalogue must outlive the tools.
    // [RQ-MCP-013, RQ-MCP-014, RQ-MCP-021, ADR-MCP-002 (DEC-MCP-010, DEC-MCP-013)]
    [[nodiscard]] std::vector<Tool> makeMultiTools(SamplerGateway& gateway, const ParameterCatalogue& multiCatalogue);

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
