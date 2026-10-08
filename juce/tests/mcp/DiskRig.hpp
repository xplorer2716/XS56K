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

// A server with every tool, disk tools included, over a gateway on a simulated sampler holding the programs PAD, BASS and
// LEAD (BASS current) and the disks it is given: what the tests of the disk tools share. [TASK-MCP-020, TASK-MCP-021,
// RQ-MCP-025, RQ-MCP-026, ADR-MCP-003 (DEC-MCP-015)]
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "SimulatedPrograms.hpp"
#include "akm/ItemCatalogue.hpp"
#include "akm/RealScheduler.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"
#include "mcp/McpServer.hpp"
#include "mcp/ParameterCatalogue.hpp"
#include "mcp/SamplerGateway.hpp"
#include "mcp/Tools.hpp"

namespace mcp::test
{
    inline constexpr std::chrono::milliseconds DISK_RIG_COMMAND_TIMEOUT{300};
    inline constexpr std::chrono::milliseconds DISK_RIG_DISK_TIMEOUT{1500};

    struct DiskRig
    {
        explicit DiskRig(std::vector<akm::harness::DiskRecord> disks, bool allowDisk = true)
        {
            sampler = &backend.addSampler();
            seedThreePrograms(backend);
            sampler->setDisks(std::move(disks));
            ToolOptions options;
            options.allowDisk = allowDisk;
            server = std::make_unique<McpServer>(ServerIdentity{"xs56k-mcp", "XS56K", "0.0.1", ""},
                                                 makeAllTools(gateway, ParameterCatalogue::standard(), options));
        }

        static GatewayConfig configFor(const akm::harness::SimulatedMidiBackend& backend)
        {
            GatewayConfig config;
            config.inputPort = backend.inputName();
            config.outputPort = backend.outputName();
            config.commandTimeout = DISK_RIG_COMMAND_TIMEOUT;
            config.diskTimeout = DISK_RIG_DISK_TIMEOUT;
            return config;
        }

        nlohmann::json call(const std::string& tool, nlohmann::json arguments = nlohmann::json::object())
        {
            const nlohmann::json message{{"jsonrpc", "2.0"},
                                         {"id", ++lastId},
                                         {"method", "tools/call"},
                                         {"params",
                                          {{"name", tool},
                                           {"arguments", std::move(arguments)},
                                           {"_meta",
                                            {{"io.modelcontextprotocol/protocolVersion", "2026-07-28"},
                                             {"io.modelcontextprotocol/clientCapabilities", nlohmann::json::object()}}}}}};
            const auto answer = server->handleLine(message.dump());
            REQUIRE(answer.has_value());
            return nlohmann::json::parse(*answer);
        }

        /// How many commands of `item` the simulated sampler has accepted.
        std::size_t accepted(akm::ItemId item) const
        {
            std::size_t count = 0;
            const akm::ItemDescriptor& wanted = akm::descriptor(item);
            for (const auto& command : sampler->acceptedCommands())
            {
                if (command.section == wanted.section && command.item == wanted.item)
                    ++count;
            }
            return count;
        }

        akm::RealScheduler scheduler;
        akm::harness::SimulatedMidiBackend backend{scheduler};
        akm::harness::SimulatedSampler* sampler = nullptr;
        SamplerGateway gateway{backend, configFor(backend)};
        std::unique_ptr<McpServer> server;
        int lastId = 0;
    };

    inline std::string textOf(const nlohmann::json& answer)
    {
        REQUIRE(answer.contains("result"));
        return answer["result"]["content"][0]["text"].get<std::string>();
    }

    inline bool isError(const nlohmann::json& answer)
    {
        REQUIRE(answer.contains("result"));
        return answer["result"]["isError"].get<bool>();
    }

    inline bool contains(const std::string& text, const char* part)
    {
        return text.find(part) != std::string::npos;
    }

    /// A writable hard disk HD1 whose root holds the folders DRUMS and SYNTH and the files given, and a read-only CD-ROM CD1.
    inline std::vector<akm::harness::DiskRecord> standardDisks(std::vector<akm::harness::FileRecord> rootFiles = {},
                                                               std::vector<akm::harness::FolderRecord> folders = {})
    {
        akm::harness::DiskRecord hard;
        hard.handle = 0;
        hard.type = 1;
        hard.format = 1;
        hard.writable = true;
        hard.name = "HD1";
        hard.freeBytes = 1000000;
        hard.rootFolder.subFolders = std::move(folders);
        hard.rootFolder.files = std::move(rootFiles);
        akm::harness::DiskRecord cd;
        cd.handle = 1;
        cd.type = 2;
        cd.format = 3;
        cd.writable = false;
        cd.name = "CD1";
        return {hard, cd};
    }

    /// A file of a simulated disk: a name, a size, and what loading it materializes.
    inline akm::harness::FileRecord programFile(const std::string& name, const std::string& program, std::uint32_t size = 4000,
                                                std::vector<std::string> dependsOn = {})
    {
        return akm::harness::FileRecord{name, size, program, std::nullopt, std::move(dependsOn)};
    }

    inline akm::harness::FileRecord sampleFile(const std::string& name, const std::string& sample, std::uint32_t size = 150000)
    {
        return akm::harness::FileRecord{name, size, std::nullopt, sample, {}};
    }
}
