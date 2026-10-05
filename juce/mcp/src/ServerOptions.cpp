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
#include "mcp/ServerOptions.hpp"

#include <charconv>
#include <optional>
#include <utility>

namespace mcp
{
    namespace
    {
        constexpr const char* OPTION_IN = "--in";
        constexpr const char* OPTION_OUT = "--out";
        constexpr const char* OPTION_DEVICE_ID = "--device-id";
        constexpr const char* OPTION_TIMEOUT = "--timeout-ms";
        constexpr const char* OPTION_NO_LCD = "--no-lcd";
        constexpr const char* OPTION_ALLOW_DISK = "--allow-disk";
        constexpr const char* OPTION_DISK_TIMEOUT = "--disk-timeout-ms";
        constexpr const char* OPTION_LIST_PORTS = "--list-ports";
        constexpr const char* OPTION_HELP = "--help";
        constexpr const char* OPTION_HELP_SHORT = "-h";

        // A sampler's DeviceID (spec p. 2, as the session takes it) and the longest command timeout that is not longer
        // than the session's own maximum total wait.
        constexpr std::int64_t MAX_DEVICE_ID = 31;
        constexpr std::int64_t MAX_TIMEOUT_MS = 60000;
        // The slow disk commands wait much longer than the ordinary ones: up to half an hour. [ADR-MCP-003 (DEC-MCP-017)]
        constexpr std::int64_t MAX_DISK_TIMEOUT_MS = 1800000;

        std::optional<std::int64_t> wholeNumber(const std::string& text)
        {
            std::int64_t number = 0;
            const char* end = text.data() + text.size();
            const auto parsed = std::from_chars(text.data(), end, number);
            if (text.empty() || parsed.ec != std::errc() || parsed.ptr != end)
                return std::nullopt;
            return number;
        }

        ParsedArguments usageError(std::string message)
        {
            ParsedArguments result;
            result.error = std::move(message);
            return result;
        }
    }

    ParsedArguments parseArguments(const std::vector<std::string>& arguments)
    {
        ParsedArguments result;
        ServerOptions& options = result.options;

        for (std::size_t at = 0; at < arguments.size(); ++at)
        {
            std::string name = arguments[at];
            std::optional<std::string> inlineValue;
            const auto equals = name.find('=');
            if (name.rfind("--", 0) == 0 && equals != std::string::npos)
            {
                inlineValue = name.substr(equals + 1);
                name.resize(equals);
            }

            const auto valueOf = [&](std::string& value) {
                if (inlineValue)
                {
                    value = *inlineValue;
                    return true;
                }
                if (at + 1 >= arguments.size())
                    return false;
                value = arguments[++at];
                return true;
            };

            if (name == OPTION_NO_LCD || name == OPTION_ALLOW_DISK || name == OPTION_LIST_PORTS || name == OPTION_HELP ||
                name == OPTION_HELP_SHORT)
            {
                if (inlineValue)
                    return usageError(name + " takes no value.");
                if (name == OPTION_NO_LCD)
                    options.touchLcdSettings = false;
                else if (name == OPTION_ALLOW_DISK)
                    options.allowDisk = true;
                else if (name == OPTION_LIST_PORTS)
                    options.listPorts = true;
                else
                    options.help = true;
                continue;
            }

            if (name != OPTION_IN && name != OPTION_OUT && name != OPTION_DEVICE_ID && name != OPTION_TIMEOUT &&
                name != OPTION_DISK_TIMEOUT)
                return usageError("Unknown argument '" + arguments[at] + "'.");

            std::string value;
            if (!valueOf(value))
                return usageError(name + " needs a value.");

            if (name == OPTION_IN || name == OPTION_OUT)
            {
                if (value.empty())
                    return usageError(name + " needs the name of a MIDI port.");
                (name == OPTION_IN ? options.inputPort : options.outputPort) = value;
            }
            else if (name == OPTION_DEVICE_ID)
            {
                const auto number = wholeNumber(value);
                if (!number || *number < 0 || *number > MAX_DEVICE_ID)
                    return usageError(std::string(OPTION_DEVICE_ID) + " must be a whole number from 0 to " +
                                      std::to_string(MAX_DEVICE_ID) + " (got '" + value + "').");
                options.deviceId = static_cast<std::uint32_t>(*number);
            }
            else if (name == OPTION_DISK_TIMEOUT)
            {
                const auto number = wholeNumber(value);
                if (!number || *number < 1 || *number > MAX_DISK_TIMEOUT_MS)
                    return usageError(std::string(OPTION_DISK_TIMEOUT) + " must be a whole number of milliseconds from 1 to " +
                                      std::to_string(MAX_DISK_TIMEOUT_MS) + " (got '" + value + "').");
                options.diskTimeout = std::chrono::milliseconds(*number);
            }
            else
            {
                const auto number = wholeNumber(value);
                if (!number || *number < 1 || *number > MAX_TIMEOUT_MS)
                    return usageError(std::string(OPTION_TIMEOUT) + " must be a whole number of milliseconds from 1 to " +
                                      std::to_string(MAX_TIMEOUT_MS) + " (got '" + value + "').");
                options.commandTimeout = std::chrono::milliseconds(*number);
            }
        }

        if (options.help || options.listPorts)
            return result;
        if (options.inputPort.empty())
            return usageError(std::string(OPTION_IN) + " is required: the MIDI input port the sampler sends on (see " +
                              OPTION_LIST_PORTS + ").");
        if (options.outputPort.empty())
            return usageError(std::string(OPTION_OUT) + " is required: the MIDI output port the sampler receives on (see " +
                              OPTION_LIST_PORTS + ").");
        return result;
    }

    GatewayConfig gatewayConfigFrom(const ServerOptions& options)
    {
        GatewayConfig config;
        config.inputPort = options.inputPort;
        config.outputPort = options.outputPort;
        config.deviceId = options.deviceId;
        config.commandTimeout = options.commandTimeout;
        config.touchLcdSettings = options.touchLcdSettings;
        config.diskTimeout = options.diskTimeout;
        return config;
    }

    std::string usageText()
    {
        return "xs56k_mcp_server: an MCP server that edits a program of an AKAI S5000/S6000 sampler.\n"
               "\n"
               "It speaks MCP on its standard input and output; an MCP client launches it, and these arguments are\n"
               "its configuration.\n"
               "\n"
               "Usage: xs56k_mcp_server --in <port> --out <port> [--device-id <0-31>] [--timeout-ms <ms>] [--no-lcd]\n"
               "                        [--allow-disk [--disk-timeout-ms <ms>]]\n"
               "       xs56k_mcp_server --list-ports\n"
               "\n"
               "  --in <port>         the MIDI input port the sampler sends on (required)\n"
               "  --out <port>        the MIDI output port the sampler receives on (required)\n"
               "  --device-id <n>     the sampler's DeviceID, 0 to 31 (default 0)\n"
               "  --timeout-ms <n>    how long a command waits for the sampler's answer, 1 to 60000 (default 2000)\n"
               "  --no-lcd            leave the sampler's Sync LCD and Auto screen update settings alone\n"
               "  --allow-disk        offer the disk tools (browse, load, save). A slow disk command can leave the sampler\n"
               "                      answering nothing until it is switched off and on: off unless you ask for it\n"
               "  --disk-timeout-ms <n>  how long a slow disk command waits, 1 to 1800000 (default 120000)\n"
               "  --list-ports        print the MIDI ports and exit\n"
               "  --help              print this text and exit\n"
               "\n"
               "Diagnostics go to standard error; standard output carries nothing but the protocol.\n";
    }
}
