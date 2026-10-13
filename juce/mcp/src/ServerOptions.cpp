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
        constexpr const char* OPTION_SCREEN = "--screen";
        // The argument --screen replaced: refused with a message that points to its successor. [RQ-MCP-045]
        constexpr const char* OPTION_FORMER_NO_LCD = "--no-lcd";
        constexpr const char* SCREEN_INDEPENDENT = "independent";
        constexpr const char* SCREEN_FOLLOW = "follow";
        constexpr const char* SCREEN_AS_IS = "as-is";
        constexpr const char* OPTION_ALLOW_DISK = "--allow-disk";
        constexpr const char* OPTION_ALLOW_DISK_REFRESH = "--allow-disk-refresh";
        constexpr const char* OPTION_ALLOW_FRONT_PANEL = "--allow-front-panel";
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

        /// The name of an argument, and the value that came with it as `--name=value`, if any.
        struct Argument
        {
            std::string name;
            std::optional<std::string> inlineValue;
        };

        Argument splitArgument(const std::string& text)
        {
            Argument argument{text, std::nullopt};
            const auto equals = argument.name.find('=');
            if (argument.name.rfind("--", 0) == 0 && equals != std::string::npos)
            {
                argument.inlineValue = argument.name.substr(equals + 1);
                argument.name.resize(equals);
            }
            return argument;
        }

        /// The options that take no value.
        bool isFlag(const std::string& name)
        {
            return name == OPTION_ALLOW_DISK || name == OPTION_ALLOW_DISK_REFRESH || name == OPTION_ALLOW_FRONT_PANEL || name == OPTION_LIST_PORTS ||
                   name == OPTION_HELP || name == OPTION_HELP_SHORT;
        }

        void setFlag(ServerOptions& options, const std::string& name)
        {
            if (name == OPTION_ALLOW_DISK)
                options.allowDisk = true;
            else if (name == OPTION_ALLOW_DISK_REFRESH)
                options.allowDiskRefresh = true;
            else if (name == OPTION_ALLOW_FRONT_PANEL)
                options.allowFrontPanel = true;
            else if (name == OPTION_LIST_PORTS)
                options.listPorts = true;
            else
                options.help = true;
        }

        /// The options that take a value.
        bool takesValue(const std::string& name)
        {
            return name == OPTION_IN || name == OPTION_OUT || name == OPTION_DEVICE_ID || name == OPTION_TIMEOUT ||
                   name == OPTION_DISK_TIMEOUT || name == OPTION_SCREEN;
        }

        /// The value of an option: the one given after an `=`, else the next argument (`at` then points to it); nothing when there is none.
        std::optional<std::string> valueOf(const std::vector<std::string>& arguments, std::size_t& at, const std::optional<std::string>& inlineValue)
        {
            if (inlineValue)
                return inlineValue;
            if (at + 1 >= arguments.size())
                return std::nullopt;
            return arguments[++at];
        }

        // The functions that apply the value of an option say what is wrong with it, or nothing.

        std::optional<std::string> applyPort(ServerOptions& options, const std::string& name, const std::string& value)
        {
            if (value.empty())
                return name + " needs the name of a MIDI port.";
            (name == OPTION_IN ? options.inputPort : options.outputPort) = value;
            return std::nullopt;
        }

        std::optional<std::string> applyDeviceId(ServerOptions& options, const std::string& value)
        {
            const auto number = wholeNumber(value);
            if (!number || *number < 0 || *number > MAX_DEVICE_ID)
                return std::string(OPTION_DEVICE_ID) + " must be a whole number from 0 to " + std::to_string(MAX_DEVICE_ID) + " (got '" +
                       value + "').";
            options.deviceId = static_cast<std::uint32_t>(*number);
            return std::nullopt;
        }

        std::optional<std::string> applyScreen(ServerOptions& options, const std::string& value)
        {
            if (value == SCREEN_INDEPENDENT)
                options.screen = ScreenMode::Independent;
            else if (value == SCREEN_FOLLOW)
                options.screen = ScreenMode::Follow;
            else if (value == SCREEN_AS_IS)
                options.screen = ScreenMode::AsIs;
            else
                return std::string(OPTION_SCREEN) + " must be " + SCREEN_INDEPENDENT + ", " + SCREEN_FOLLOW + " or " + SCREEN_AS_IS + " (got '" +
                       value + "').";
            return std::nullopt;
        }

        /// A number of milliseconds from 1 to `maximum`, for the option `option`.
        std::optional<std::string> applyTimeout(const char* option, std::int64_t maximum, const std::string& value,
                                                std::chrono::milliseconds& target)
        {
            const auto number = wholeNumber(value);
            if (!number || *number < 1 || *number > maximum)
                return std::string(option) + " must be a whole number of milliseconds from 1 to " + std::to_string(maximum) + " (got '" + value +
                       "').";
            target = std::chrono::milliseconds(*number);
            return std::nullopt;
        }

        std::optional<std::string> applyValue(ServerOptions& options, const std::string& name, const std::string& value)
        {
            if (name == OPTION_IN || name == OPTION_OUT)
                return applyPort(options, name, value);
            if (name == OPTION_DEVICE_ID)
                return applyDeviceId(options, value);
            if (name == OPTION_SCREEN)
                return applyScreen(options, value);
            if (name == OPTION_DISK_TIMEOUT)
                return applyTimeout(OPTION_DISK_TIMEOUT, MAX_DISK_TIMEOUT_MS, value, options.diskTimeout);
            return applyTimeout(OPTION_TIMEOUT, MAX_TIMEOUT_MS, value, options.commandTimeout);
        }

        /// What the arguments, all read, must give together: the ports are required, except to list the ports or to get help.
        ParsedArguments checkTogether(ParsedArguments result)
        {
            const ServerOptions& options = result.options;
            if (options.help || options.listPorts)
                return result;
            if (options.allowDiskRefresh && !options.allowDisk)
                return usageError(std::string(OPTION_ALLOW_DISK_REFRESH) + " needs " + OPTION_ALLOW_DISK +
                                  ": the refresh is a part of the disk tools.");
            if (options.inputPort.empty())
                return usageError(std::string(OPTION_IN) + " is required: the MIDI input port the sampler sends on (see " +
                                  OPTION_LIST_PORTS + ").");
            if (options.outputPort.empty())
                return usageError(std::string(OPTION_OUT) + " is required: the MIDI output port the sampler receives on (see " +
                                  OPTION_LIST_PORTS + ").");
            return result;
        }
    }

    ParsedArguments parseArguments(const std::vector<std::string>& arguments)
    {
        ParsedArguments result;
        ServerOptions& options = result.options;

        for (std::size_t at = 0; at < arguments.size(); ++at)
        {
            const Argument argument = splitArgument(arguments[at]);
            const std::string& name = argument.name;

            if (name == OPTION_FORMER_NO_LCD)
                return usageError(std::string(OPTION_FORMER_NO_LCD) + " was replaced by " + OPTION_SCREEN + " " + SCREEN_AS_IS +
                                  " (leave the sampler's screen settings alone); see " + OPTION_HELP + ".");

            if (isFlag(name))
            {
                if (argument.inlineValue)
                    return usageError(name + " takes no value.");
                setFlag(options, name);
                continue;
            }

            if (!takesValue(name))
                return usageError("Unknown argument '" + arguments[at] + "'.");

            const auto value = valueOf(arguments, at, argument.inlineValue);
            if (!value)
                return usageError(name + " needs a value.");

            if (const auto problem = applyValue(options, name, *value))
                return usageError(*problem);
        }

        return checkTogether(std::move(result));
    }

    GatewayConfig gatewayConfigFrom(const ServerOptions& options)
    {
        GatewayConfig config;
        config.inputPort = options.inputPort;
        config.outputPort = options.outputPort;
        config.deviceId = options.deviceId;
        config.commandTimeout = options.commandTimeout;
        config.screen = options.screen;
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
               "Usage: xs56k_mcp_server --in <port> --out <port> [--device-id <0-31>] [--timeout-ms <ms>]\n"
               "                        [--screen independent|follow|as-is]\n"
               "                        [--allow-disk [--allow-disk-refresh] [--disk-timeout-ms <ms>]]\n"
               "                        [--allow-front-panel]\n"
               "       xs56k_mcp_server --list-ports\n"
               "\n"
               "  --in <port>         the MIDI input port the sampler sends on (required)\n"
               "  --out <port>        the MIDI output port the sampler receives on (required)\n"
               "  --device-id <n>     the sampler's DeviceID, 0 to 31 (default 0)\n"
               "  --timeout-ms <n>    how long a command waits for the sampler's answer, 1 to 60000 (default 2000)\n"
               "  --screen <mode>     how the sampler's screen behaves while the server runs:\n"
               "                        independent  (default) the assistant has its own selection, the screen does not follow it\n"
               "                                     and is redrawn after each edit\n"
               "                        follow       the screen follows the assistant's selection, and a selection made on the\n"
               "                                     sampler changes the assistant's\n"
               "                        as-is        the server does not touch the sampler's screen settings\n"
               "                      When the server stops it puts the settings it changed back to the sampler's standard values\n"
               "                      (Sync LCD on, Auto screen update off), not to what you had set.\n"
               "  --allow-disk        offer the disk tools (browse, load, save). A slow disk command can leave the sampler\n"
               "                      answering nothing until it is switched off and on: off unless you ask for it\n"
               "  --allow-disk-refresh  let list_disks send the sampler's refresh of its disk list (needs --allow-disk). Off\n"
               "                      unless you ask: it hung a real S5000 whose disk is a SCSI2SD, until it was switched off and on\n"
               "  --disk-timeout-ms <n>  how long a slow disk command waits, 1 to 1800000 (default 120000)\n"
               "  --allow-front-panel  offer the front-panel key tools (press, hold, release, data wheel, ASCII key). A key answers\n"
               "                      \"ENT\" to any delete or save screen with no confirm to stop it: off unless you ask for it\n"
               "  --list-ports        print the MIDI ports and exit\n"
               "  --help              print this text and exit\n"
               "\n"
               "Diagnostics go to standard error; standard output carries nothing but the protocol.\n";
    }
}
