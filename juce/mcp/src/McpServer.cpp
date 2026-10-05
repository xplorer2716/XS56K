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
#include "mcp/McpServer.hpp"

#include <algorithm>
#include <exception>
#include <istream>
#include <ostream>
#include <utility>

using nlohmann::json;

namespace mcp
{
    namespace
    {
        // JSON-RPC 2.0 and MCP error codes. [ADR-MCP-001 (DEC-MCP-002)]
        constexpr int ERROR_PARSE = -32700;
        constexpr int ERROR_INVALID_REQUEST = -32600;
        constexpr int ERROR_METHOD_NOT_FOUND = -32601;
        constexpr int ERROR_INVALID_PARAMS = -32602;
        constexpr int ERROR_UNSUPPORTED_PROTOCOL_VERSION = -32022;

        constexpr const char* JSONRPC_VERSION = "2.0";
        constexpr const char* KEY_META = "_meta";
        constexpr const char* KEY_PROTOCOL_VERSION = "io.modelcontextprotocol/protocolVersion";
        constexpr const char* KEY_CLIENT_CAPABILITIES = "io.modelcontextprotocol/clientCapabilities";
        constexpr const char* KEY_SERVER_INFO = "io.modelcontextprotocol/serverInfo";
        constexpr const char* RESULT_TYPE_COMPLETE = "complete";
        constexpr const char* CACHE_SCOPE_PUBLIC = "public";

        // The tool list is fixed for a build, so a client may keep it for a minute. [ADR-MCP-001 (DEC-MCP-002)]
        constexpr long long LIST_TTL_MS = 60000;

        constexpr const char* METHOD_INITIALIZE = "initialize";
        constexpr const char* METHOD_PING = "ping";
        constexpr const char* METHOD_DISCOVER = "server/discover";
        constexpr const char* METHOD_TOOLS_LIST = "tools/list";
        constexpr const char* METHOD_TOOLS_CALL = "tools/call";

        json errorResponse(const json& id, int code, std::string message, json data = nullptr)
        {
            json error{{"code", code}, {"message", std::move(message)}};
            if (!data.is_null())
                error["data"] = std::move(data);
            return json{{"jsonrpc", JSONRPC_VERSION}, {"id", id}, {"error", std::move(error)}};
        }

        json resultResponse(const json& id, json result)
        {
            return json{{"jsonrpc", JSONRPC_VERSION}, {"id", id}, {"result", std::move(result)}};
        }

        bool isValidId(const json& id)
        {
            return id.is_string() || id.is_number_integer();
        }

        json toolErrorContent(std::string text)
        {
            return json{{"content", json::array({json{{"type", "text"}, {"text", std::move(text)}}})}, {"isError", true}};
        }

        bool isSupportedModernVersion(const std::string& version)
        {
            return version == MODERN_PROTOCOL_VERSION;
        }

        json supportedModernVersions()
        {
            return json::array({std::string(MODERN_PROTOCOL_VERSION)});
        }

        std::string legacyVersionFor(const std::string& requested)
        {
            const auto known = std::find(LEGACY_PROTOCOL_VERSIONS.begin(), LEGACY_PROTOCOL_VERSIONS.end(), requested);
            return std::string(known != LEGACY_PROTOCOL_VERSIONS.end() ? *known : LEGACY_PROTOCOL_VERSIONS.front());
        }

        // Compact (no newline inside) and tolerant of text that is not UTF-8: the line is written either way.
        std::string toLine(const json& message)
        {
            return message.dump(-1, ' ', false, json::error_handler_t::replace);
        }
    }

    McpServer::McpServer(ServerIdentity identity, std::vector<Tool> tools)
        : _identity(std::move(identity)), _tools(std::move(tools))
    {
    }

    std::optional<std::string> McpServer::handleLine(std::string_view line)
    {
        const json message = json::parse(line.begin(), line.end(), nullptr, false);
        if (message.is_discarded())
            return toLine(errorResponse(nullptr, ERROR_PARSE, "Parse error: the line is not valid JSON"));

        const auto answer = handleMessage(message);
        if (!answer)
            return std::nullopt;
        return toLine(*answer);
    }

    void McpServer::serve(std::istream& input, std::ostream& output)
    {
        std::string line;
        while (std::getline(input, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (std::all_of(line.begin(), line.end(), [](unsigned char c) { return c == ' ' || c == '\t'; }))
                continue;
            if (const auto answer = handleLine(line))
            {
                output << *answer << '\n';
                output.flush();
            }
        }
    }

    std::optional<json> McpServer::handleMessage(const json& message)
    {
        const bool isObject = message.is_object();
        const json* id = isObject && message.contains("id") ? &message.at("id") : nullptr;
        const bool wellFormed = isObject && message.contains("jsonrpc") && message.at("jsonrpc") == JSONRPC_VERSION &&
                                message.contains("method") && message.at("method").is_string() &&
                                (id == nullptr || isValidId(*id));
        if (!wellFormed)
        {
            const json answerId = id != nullptr && isValidId(*id) ? *id : json(nullptr);
            return errorResponse(answerId, ERROR_INVALID_REQUEST, "Invalid request: not a JSON-RPC 2.0 request");
        }

        if (id == nullptr)
            return std::nullopt;  // a notification: nothing is answered, whatever its name

        const std::string method = message.at("method").get<std::string>();
        const auto params = message.find("params");
        if (params != message.end() && !params->is_object())
            return errorResponse(*id, ERROR_INVALID_PARAMS, "Invalid params: params must be an object");
        return handleRequest(*id, method, params != message.end() ? *params : json::object());
    }

    json McpServer::handleRequest(const json& id, const std::string& method, const json& params)
    {
        if (method == METHOD_INITIALIZE)
            return handleInitialize(id, params);

        const auto meta = params.find(KEY_META);
        const bool hasModernVersion =
            meta != params.end() && meta->is_object() && meta->contains(KEY_PROTOCOL_VERSION);
        if (!hasModernVersion)
        {
            if (_legacyInitialized)
                return dispatch(id, method, params, false);
            return errorResponse(id, ERROR_INVALID_PARAMS,
                                 "Invalid params: the request carries no _meta[\"io.modelcontextprotocol/protocolVersion\"] "
                                 "and no initialize request came before it");
        }

        const json& version = meta->at(KEY_PROTOCOL_VERSION);
        if (!version.is_string())
            return errorResponse(id, ERROR_INVALID_PARAMS, "Invalid params: the protocol version must be a string");
        if (!isSupportedModernVersion(version.get<std::string>()))
            return errorResponse(id, ERROR_UNSUPPORTED_PROTOCOL_VERSION, "Unsupported protocol version",
                                 json{{"supported", supportedModernVersions()}, {"requested", version}});
        if (!meta->contains(KEY_CLIENT_CAPABILITIES) || !meta->at(KEY_CLIENT_CAPABILITIES).is_object())
            return errorResponse(id, ERROR_INVALID_PARAMS,
                                 "Invalid params: _meta[\"io.modelcontextprotocol/clientCapabilities\"] is required");
        return dispatch(id, method, params, true);
    }

    json McpServer::handleInitialize(const json& id, const json& params)
    {
        const auto requested = params.find("protocolVersion");
        if (requested == params.end() || !requested->is_string())
            return errorResponse(id, ERROR_INVALID_PARAMS, "Invalid params: protocolVersion must be a string");

        _legacyInitialized = true;
        json result{{"protocolVersion", legacyVersionFor(requested->get<std::string>())},
                    {"capabilities", json{{"tools", json::object()}}},
                    {"serverInfo", serverInfo()}};
        if (!_identity.instructions.empty())
            result["instructions"] = _identity.instructions;
        return resultResponse(id, std::move(result));
    }

    json McpServer::dispatch(const json& id, const std::string& method, const json& params, bool modern)
    {
        if (method == METHOD_PING)
            return resultResponse(id, completeResult(json::object(), modern));

        if (method == METHOD_DISCOVER && modern)
        {
            json result{{"supportedVersions", supportedModernVersions()},
                        {"capabilities", json{{"tools", json::object()}}},
                        {"ttlMs", LIST_TTL_MS},
                        {"cacheScope", CACHE_SCOPE_PUBLIC}};
            if (!_identity.instructions.empty())
                result["instructions"] = _identity.instructions;
            return resultResponse(id, completeResult(std::move(result), modern));
        }

        if (method == METHOD_TOOLS_LIST)
        {
            json tools = json::array();
            for (const Tool& tool : _tools)
                tools.push_back(toolDescription(tool));
            json result{{"tools", std::move(tools)}};
            if (modern)
            {
                result["ttlMs"] = LIST_TTL_MS;
                result["cacheScope"] = CACHE_SCOPE_PUBLIC;
            }
            return resultResponse(id, completeResult(std::move(result), modern));
        }

        if (method == METHOD_TOOLS_CALL)
            return callTool(id, params, modern);

        return errorResponse(id, ERROR_METHOD_NOT_FOUND, "Method not found: " + method);
    }

    json McpServer::callTool(const json& id, const json& params, bool modern)
    {
        const auto name = params.find("name");
        if (name == params.end() || !name->is_string())
            return errorResponse(id, ERROR_INVALID_PARAMS, "Invalid params: name must be a string");
        const std::string toolName = name->get<std::string>();

        const auto tool = std::find_if(_tools.begin(), _tools.end(),
                                       [&toolName](const Tool& candidate) { return candidate.definition.name == toolName; });
        if (tool == _tools.end())
            return errorResponse(id, ERROR_INVALID_PARAMS, "Unknown tool: " + toolName);

        const auto given = params.find("arguments");
        if (given != params.end() && !given->is_object())
            return errorResponse(id, ERROR_INVALID_PARAMS, "Invalid params: arguments must be an object");
        const json arguments = given != params.end() ? *given : json::object();

        const auto required = tool->definition.inputSchema.find("required");
        if (required != tool->definition.inputSchema.end() && required->is_array())
        {
            for (const json& key : *required)
            {
                if (key.is_string() && !arguments.contains(key.get<std::string>()))
                    return resultResponse(id, completeResult(toolErrorContent("Missing required argument '" +
                                                                                key.get<std::string>() + "' of the tool '" +
                                                                                toolName + "'."),
                                                             modern));
            }
        }

        ToolResult outcome;
        try
        {
            outcome = tool->handler(arguments);
        }
        catch (const std::exception& problem)
        {
            outcome = ToolResult{"The tool '" + toolName + "' failed: " + problem.what(), true};
        }
        catch (...)
        {
            outcome = ToolResult{"The tool '" + toolName + "' failed for a reason it did not report.", true};
        }

        json result{{"content", json::array({json{{"type", "text"}, {"text", std::move(outcome.text)}}})},
                    {"isError", outcome.isError}};
        return resultResponse(id, completeResult(std::move(result), modern));
    }

    json McpServer::toolDescription(const Tool& tool) const
    {
        const ToolDefinition& definition = tool.definition;
        json description{{"name", definition.name},
                         {"description", definition.description},
                         {"inputSchema", definition.inputSchema},
                         {"annotations",
                          json{{"readOnlyHint", definition.annotations.readOnly},
                               {"destructiveHint", definition.annotations.destructive},
                               {"idempotentHint", definition.annotations.idempotent},
                               {"openWorldHint", definition.annotations.openWorld}}}};
        if (!definition.title.empty())
            description["title"] = definition.title;
        return description;
    }

    json McpServer::completeResult(json result, bool modern) const
    {
        if (modern)
        {
            result["resultType"] = RESULT_TYPE_COMPLETE;
            result["_meta"] = json{{KEY_SERVER_INFO, json{{"name", _identity.name}, {"version", _identity.version}}}};
        }
        return result;
    }

    json McpServer::serverInfo() const
    {
        json info{{"name", _identity.name}, {"version", _identity.version}};
        if (!_identity.title.empty())
            info["title"] = _identity.title;
        return info;
    }
}
