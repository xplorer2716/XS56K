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

// xs56k_mcp_server: an MCP server that lets an MCP client edit a program of an AKAI S5000/S6000 sampler. It speaks MCP
// on its standard input and output (one JSON message per line), takes the MIDI ports and the other settings from its
// launch arguments, and sends its diagnostics to standard error: standard output carries nothing but the protocol.
// It is the only place that knows the JUCE MIDI backend. The logic is the library xs56k_mcp, which CI tests against the
// simulated sampler. [TASK-MCP-006, RQ-MCP-001, RQ-MCP-002, RQ-MCP-003, RQ-MCP-009, ADR-MCP-001 (DEC-MCP-001,
// DEC-MCP-004, DEC-MCP-008)]
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#endif

#include "akm/SessionConfig.hpp"
#include "common/midi/JuceMidiBackend.hpp"
#include "mcp/McpServer.hpp"
#include "mcp/ServerOptions.hpp"
#include "mcp/Tools.hpp"

#ifndef XS56K_MCP_VERSION
#define XS56K_MCP_VERSION "0.0.0.0-local"
#endif

namespace
{
    constexpr int EXIT_OK = 0;
    constexpr int EXIT_USAGE = 2;
    constexpr int EXIT_FAILURE_UNEXPECTED = 3;

    constexpr const char* SERVER_NAME = "xs56k-mcp";
    constexpr const char* SERVER_TITLE = "XS56K sampler editor";
    constexpr const char* LOG_PREFIX = "xs56k_mcp_server: ";

    // The protocol is one message per line with a bare newline: a Windows text-mode stream would write CR LF.
    void useBinaryStreams()
    {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif
    }

    void listPorts(common::midi::MidiBackend& backend)
    {
        std::cout << "MIDI input ports (what the sampler sends to this computer, for --in):\n";
        for (const std::string& name : backend.inputDeviceNames())
            std::cout << "  " << name << "\n";
        std::cout << "MIDI output ports (what this computer sends to the sampler, for --out):\n";
        for (const std::string& name : backend.outputDeviceNames())
            std::cout << "  " << name << "\n";
    }

    std::string settingsText(const std::vector<akm::SamplerSetting>& settings)
    {
        if (settings.empty())
            return "none";
        std::string text;
        for (const akm::SamplerSetting setting : settings)
            text += (text.empty() ? "" : ", ") + std::string(akm::describe(setting));
        return text;
    }
}

int main(int argc, char** argv)
{
    try
    {
        const std::vector<std::string> arguments(argv + 1, argv + argc);
        const mcp::ParsedArguments parsed = mcp::parseArguments(arguments);
        if (!parsed.ok())
        {
            std::cerr << LOG_PREFIX << parsed.error << "\n\n" << mcp::usageText();
            return EXIT_USAGE;
        }
        if (parsed.options.help)
        {
            std::cout << mcp::usageText();
            return EXIT_OK;
        }

        common::midi::JuceMidiBackend backend;
        if (parsed.options.listPorts)
        {
            listPorts(backend);
            return EXIT_OK;
        }

        useBinaryStreams();
        mcp::SamplerGateway gateway(backend, mcp::gatewayConfigFrom(parsed.options));
        mcp::ServerIdentity identity;
        identity.name = SERVER_NAME;
        identity.title = SERVER_TITLE;
        identity.version = XS56K_MCP_VERSION;
        identity.instructions = mcp::programEditingInstructions();
        mcp::McpServer server(identity, mcp::makeAllTools(gateway, mcp::ParameterCatalogue::standard()));

        std::cerr << LOG_PREFIX << "version " << XS56K_MCP_VERSION << ", sampler input \"" << parsed.options.inputPort
                  << "\", output \"" << parsed.options.outputPort << "\", DeviceID " << parsed.options.deviceId << "\n";
        server.serve(std::cin, std::cout);

        // The client closed standard input: whatever the session changed on the sampler is put back before the exit.
        std::cerr << LOG_PREFIX << "input closed, closing the sampler session\n";
        if (const auto closed = gateway.close())
            std::cerr << LOG_PREFIX << "settings put back: " << settingsText(closed->restored)
                      << "; not put back: " << settingsText(closed->notRestored) << "\n";
        return EXIT_OK;
    }
    catch (const std::exception& problem)
    {
        std::cerr << LOG_PREFIX << "stopped by an unexpected error: " << problem.what() << "\n";
        return EXIT_FAILURE_UNEXPECTED;
    }
}
