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

// The equipment of the tests of the tools of the memory (zone samples, keygroups, samples, multis, information, audition):
// a simulated sampler holding the programs PAD (1 keygroup), BASS (3, current) and LEAD (2), the samples KICK, SNARE and PAD, the
// multis LIVE and STUDIO, a server over it and the calls a test makes. [PLAN-MCP-004]

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

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
    inline constexpr std::chrono::milliseconds TOOL_RIG_COMMAND_TIMEOUT{300};
    inline constexpr int TOOL_RIG_INVALID_PARAMS = -32602;

    struct ToolRig
    {
        explicit ToolRig(bool withDisk = false, bool withFrontPanel = false)
        {
            sampler = &backend.addSampler();
            seedThreePrograms(backend);
            sampler->setSampleNames({"KICK", "SNARE", "PAD"});
            sampler->setMultiNames({"LIVE", "STUDIO"});
            ToolOptions options;
            options.allowDisk = withDisk;
            options.allowFrontPanel = withFrontPanel;
            server = std::make_unique<McpServer>(ServerIdentity{"xs56k-mcp", "XS56K", "0.0.1", ""},
                                                 makeAllTools(gateway, ParameterCatalogue::standard(), options));
        }

        static GatewayConfig configFor(const akm::harness::SimulatedMidiBackend& backend)
        {
            GatewayConfig config;
            config.inputPort = backend.inputName();
            config.outputPort = backend.outputName();
            config.commandTimeout = TOOL_RIG_COMMAND_TIMEOUT;
            return config;
        }

        nlohmann::json call(const std::string& tool, nlohmann::json arguments = nlohmann::json::object())
        {
            return request("tools/call", nlohmann::json{{"name", tool}, {"arguments", std::move(arguments)}});
        }

        nlohmann::json request(const std::string& method, nlohmann::json params)
        {
            params["_meta"] = {{"io.modelcontextprotocol/protocolVersion", "2026-07-28"},
                               {"io.modelcontextprotocol/clientCapabilities", nlohmann::json::object()}};
            const nlohmann::json message{{"jsonrpc", "2.0"}, {"id", ++lastId}, {"method", method}, {"params", std::move(params)}};
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

        /// The data bytes of every command of `item` the simulated sampler has accepted, in order.
        std::vector<std::vector<std::uint8_t>> sentData(akm::ItemId item) const
        {
            std::vector<std::vector<std::uint8_t>> sent;
            const akm::ItemDescriptor& wanted = akm::descriptor(item);
            for (const auto& command : sampler->acceptedCommands())
            {
                if (command.section == wanted.section && command.item == wanted.item)
                    sent.push_back(command.data);
            }
            return sent;
        }

        /// The tool named `name` as `tools/list` gives it, or null.
        nlohmann::json tool(const std::string& name)
        {
            const nlohmann::json list = request("tools/list", nlohmann::json::object());
            for (const nlohmann::json& candidate : list["result"]["tools"])
            {
                if (candidate["name"] == name)
                    return candidate;
            }
            return nullptr;
        }

        akm::RealScheduler scheduler;
        akm::harness::SimulatedMidiBackend backend{scheduler};
        akm::harness::SimulatedSampler* sampler = nullptr;
        SamplerGateway gateway{backend, configFor(backend)};
        std::unique_ptr<McpServer> server;
        int lastId = 0;
    };

    inline std::string toolText(const nlohmann::json& answer)
    {
        REQUIRE(answer.contains("result"));
        return answer["result"]["content"][0]["text"].get<std::string>();
    }

    inline bool toolFailed(const nlohmann::json& answer)
    {
        REQUIRE(answer.contains("result"));
        return answer["result"]["isError"].get<bool>();
    }

    inline bool hasText(const std::string& text, const char* part)
    {
        return text.find(part) != std::string::npos;
    }
}
