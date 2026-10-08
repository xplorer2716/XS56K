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

// xs56k_mcp_server_simulated: the MCP server of xs56k_mcp_server wired to the simulated sampler instead of the JUCE MIDI
// backend. The sampler holds three programs (PAD with 1 keygroup, BASS with 3, LEAD with 2; BASS is current), so a
// scripted conversation piped into it is a ctest entry, and a person can try the server from an MCP client with no
// sampler. Test tooling, built only with BUILD_TESTS and never shipped: the shipped server does not link the simulated
// sampler. [TASK-MCP-007, RQ-MCP-012, ADR-MCP-001 (DEC-MCP-009)]
#include <iostream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#endif

#include "SimulatedPrograms.hpp"
#include "akm/RealScheduler.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"
#include "mcp/McpServer.hpp"
#include "mcp/SamplerGateway.hpp"
#include "mcp/Tools.hpp"

namespace
{
    constexpr const char* SERVER_NAME = "xs56k-mcp";
    constexpr const char* SERVER_TITLE = "XS56K sampler editor (simulated sampler)";
    // A fixed version, so that the expected output of the scripted conversation does not change with the build.
    constexpr const char* SERVER_VERSION = "simulated";
    constexpr std::chrono::milliseconds COMMAND_TIMEOUT{2000};
}

int main(int argc, char** argv)
{
    // --allow-disk offers the disk tools, as the shipped server does (ADR-MCP-003 DEC-MCP-015). The simulated sampler always
    // holds its two disks; only the tools are opt-in.
    mcp::ToolOptions toolOptions;
    for (int i = 1; i < argc; ++i)
    {
        toolOptions.allowDisk = toolOptions.allowDisk || std::string(argv[i]) == "--allow-disk";
        toolOptions.allowDiskRefresh = toolOptions.allowDiskRefresh || std::string(argv[i]) == "--allow-disk-refresh";
    }

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    akm::RealScheduler scheduler;
    akm::harness::SimulatedMidiBackend backend(scheduler);
    akm::harness::SimulatedSampler& sampler = backend.addSampler();
    mcp::test::seedThreePrograms(backend);
    sampler.setSampleNames({"KICK", "SNARE", "PAD"});
    sampler.setSampleAttributes(0, 0, 2, 143169, 22050);
    sampler.setMultiNames({"LIVE", "STUDIO"});
    sampler.setSongNames({"INTRO", "VERSE", "OUTRO"});
    sampler.setSetListNames({"TOUR", "HOME"});
    sampler.setSceneListNames({"LIVE SET", "STUDIO"});
    sampler.setFxBoard(akm::harness::eb20Layout());

    // Two disks: HD1, a writable hard disk (MSDOS) whose root holds the program file INIT.AKP, the sample file KICK2.WAV, a
    // program BIG.AKP that depends on a sample file BIGSAMPLE.WAV, and the folders SYNTH (a program LEAD2 and a sample WAVE)
    // and DRUMS; and CD1, a read-only CD-ROM (ISO9660).
    akm::harness::FolderRecord synth;
    synth.name = "SYNTH";
    synth.programFiles = {"LEAD2"};
    synth.sampleFiles = {"WAVE"};
    akm::harness::FolderRecord drums;
    drums.name = "DRUMS";
    akm::harness::DiskRecord hardDisk;
    hardDisk.handle = 0;
    hardDisk.type = 1;
    hardDisk.format = 1;
    hardDisk.writable = true;
    hardDisk.name = "HD1";
    hardDisk.freeBytes = 1000000;
    hardDisk.rootFolder.subFolders = {drums, synth};
    hardDisk.rootFolder.files = {akm::harness::FileRecord{"INIT.AKP", 3000, std::string("INIT"), std::nullopt, {}},
                                 akm::harness::FileRecord{"KICK2.WAV", 143000, std::nullopt, std::string("KICK2"), {}},
                                 akm::harness::FileRecord{"BIG.AKP", 9000, std::string("BIGPROG"), std::nullopt, {"BIGSAMPLE.WAV"}},
                                 akm::harness::FileRecord{"BIGSAMPLE.WAV", 90000, std::nullopt, std::string("BIGSAMPLE"), {}}};
    akm::harness::DiskRecord cdRom;
    cdRom.handle = 1;
    cdRom.type = 2;
    cdRom.format = 3;
    cdRom.writable = false;
    cdRom.name = "CD1";
    sampler.setDisks({hardDisk, cdRom});

    mcp::GatewayConfig config;
    config.inputPort = backend.inputName();
    config.outputPort = backend.outputName();
    config.commandTimeout = COMMAND_TIMEOUT;
    mcp::SamplerGateway gateway(backend, config);

    mcp::ServerIdentity identity;
    identity.name = SERVER_NAME;
    identity.title = SERVER_TITLE;
    identity.version = SERVER_VERSION;
    identity.instructions = mcp::programEditingInstructions();
    if (toolOptions.allowDisk)
        identity.instructions += mcp::diskInstructions(toolOptions.allowDiskRefresh);
    mcp::McpServer server(identity, mcp::makeAllTools(gateway, mcp::ParameterCatalogue::standard(), toolOptions));

    std::cerr << "xs56k_mcp_server_simulated: a simulated sampler holding the programs PAD, BASS and LEAD (BASS current), the samples KICK, SNARE and PAD and the multis LIVE and STUDIO\n";
    server.serve(std::cin, std::cout);
    gateway.close();
    return 0;
}
