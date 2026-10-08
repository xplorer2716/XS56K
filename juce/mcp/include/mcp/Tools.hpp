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

    // The disk tools: `list_disks`, `select_disk`, `list_disk_contents`, `open_folder` and `close_folder` (browsing the
    // sampler's own disks), offered only when the server is launched with `--allow-disk`. The gateway must outlive the
    // tools. [RQ-MCP-023, RQ-MCP-024, ADR-MCP-003 (DEC-MCP-015, DEC-MCP-016)]
    // `offerRefresh` (`--allow-disk-refresh`) puts a `refresh` argument in `list_disks`; without it the refresh of the disk list,
    // which hung a real S5000, is never sent. [RQ-MCP-031, ADR-MCP-003 (DEC-MCP-020)]
    [[nodiscard]] std::vector<Tool> makeDiskTools(SamplerGateway& gateway, bool offerRefresh = false);

    // The tools of the memory that complete the editing: the zone samples (`set_zone_sample`, `get_zone_samples`) and, as
    // the tasks of PLAN-MCP-004 add them, the keygroups, the samples, the multis and the information of the sampler. Every tool that
    // deletes asks for `confirm`, the exact name of what is deleted. The gateway must outlive the tools. [RQ-MCP-034 to RQ-MCP-042,
    // ADR-MCP-004 (DEC-MCP-023, DEC-MCP-024)]
    [[nodiscard]] std::vector<Tool> makeMemoryExtraTools(SamplerGateway& gateway);

    // The sampler's own settings: `get_sampler_settings` reads its name, clock, play mode and front-panel lock, and
    // `set_sampler_setting` sets one of them, reading it back. Nothing is deleted. The gateway must outlive the tools.
    // [RQ-MCP-046, ADR-MCP-005 (DEC-MCP-030)]
    [[nodiscard]] std::vector<Tool> makeSamplerSettingsTools(SamplerGateway& gateway);

    // The sampler's MIDI setup: `set_midi_setting` sets one of its five switches and `set_midi_filter` allows or ignores a type of event
    // on a channel. Section 04 has no Get, so neither reads back and each answer says the previous value is unknown. Nothing is deleted.
    // The gateway must outlive the tools. [RQ-MCP-047, ADR-MCP-005 (DEC-MCP-030)]
    [[nodiscard]] std::vector<Tool> makeMidiSetupTools(SamplerGateway& gateway);

    // The song files, the set lists and the scenelists: for each, a list tool (`list_song_files`, `list_set_lists`, `list_scenelists`) and a
    // rename tool; the song files and the scenelists, which have a current one, also a select tool. A set list is renamed by name. Nothing is
    // deleted here. The gateway must outlive the tools. [RQ-MCP-048, ADR-MCP-005 (DEC-MCP-031)]
    [[nodiscard]] std::vector<Tool> makeNamedListTools(SamplerGateway& gateway);

    /// What the launch arguments decide about the tools. [ADR-MCP-003 (DEC-MCP-015)]
    struct ToolOptions
    {
        bool allowDisk = false;  ///< `--allow-disk`: the disk tools are offered
        bool allowDiskRefresh = false;  ///< `--allow-disk-refresh`: `list_disks` may send the refresh of the disk list
    };

    /// Every tool the server offers: the editing tools, the structure tools, the sample and multi tools, and the disk tools
    /// when `options.allowDisk`. [ADR-MCP-002 (DEC-MCP-010), ADR-MCP-003 (DEC-MCP-015)]
    [[nodiscard]] std::vector<Tool> makeAllTools(SamplerGateway& gateway, const ParameterCatalogue& catalogue, ToolOptions options = {});

    /// The guidance the server gives the model with its first answer: what the server is for and how to use the tools.
    [[nodiscard]] std::string programEditingInstructions();

    /// What the server adds to its guidance when the disk tools are offered (`--allow-disk`). [ADR-MCP-003 (DEC-MCP-015)]
    [[nodiscard]] std::string diskInstructions(bool refreshOffered = false);
}
