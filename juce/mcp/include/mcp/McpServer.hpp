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

#include <array>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace mcp
{
    // The protocol unit of the MCP server: JSON-RPC 2.0 messages, one per line, the two eras of the protocol
    // (the stateless `2026-07-28` and the `initialize`-based earlier revisions), `ping`, `tools/list` and
    // `tools/call` over a registered set of tools. It knows nothing of the sampler: a tool is a name, a schema
    // and a function from JSON arguments to text. [RQ-MCP-001, RQ-MCP-009, ADR-MCP-001 (DEC-MCP-001,
    // DEC-MCP-002)]

    /// The one revision of the stateless era the server serves. [ADR-MCP-001 (DEC-MCP-002)]
    inline constexpr std::string_view MODERN_PROTOCOL_VERSION = "2026-07-28";

    /// The revisions that open with `initialize` and are served, latest first: the answer to an `initialize`
    /// that asks for another revision is the first of them. [ADR-MCP-001 (DEC-MCP-002)]
    inline constexpr std::array<std::string_view, 4> LEGACY_PROTOCOL_VERSIONS{"2025-11-25", "2025-06-18", "2025-03-26",
                                                                              "2024-11-05"};

    /// The hints of MCP's tool annotations. The defaults are the safe reading for a tool that must say so itself:
    /// it changes something, destroys nothing it says, and reaches only the closed world of one sampler.
    struct ToolAnnotations
    {
        bool readOnly = false;
        bool destructive = true;
        bool idempotent = false;
        bool openWorld = false;
    };

    struct ToolDefinition
    {
        std::string name;
        std::string title;
        std::string description;
        /// A JSON Schema object (2020-12, MCP's default dialect). The names in its `required` array are checked
        /// before the handler runs.
        nlohmann::json inputSchema = nlohmann::json::object();
        ToolAnnotations annotations;
    };

    /// What a tool answers: text for the model, and whether it is a failure the model can act on (a tool
    /// execution error, MCP's `isError`), as opposed to a protocol error. [RQ-MCP-009]
    struct ToolResult
    {
        std::string text;
        bool isError = false;
    };

    using ToolHandler = std::function<ToolResult(const nlohmann::json& arguments)>;

    struct Tool
    {
        ToolDefinition definition;
        ToolHandler handler;
    };

    struct ServerIdentity
    {
        std::string name;
        std::string title;
        std::string version;
        /// Natural-language guidance for the model, sent with the discovery and the `initialize` answer.
        std::string instructions;
    };

    /// Not thread-safe: requests are handled one at a time, in arrival order (ADR-MCP-001, DEC-MCP-003).
    class McpServer
    {
    public:
        McpServer(ServerIdentity identity, std::vector<Tool> tools);

        /// Handles one line (one JSON-RPC message, no trailing newline). Returns the one line to write back, or
        /// nothing for a notification. Never throws: a handler that throws becomes an `isError` result.
        [[nodiscard]] std::optional<std::string> handleLine(std::string_view line);

        /// Reads lines from `input` until its end and writes each answer, one line, to `output`, flushing after
        /// each. A blank line is skipped. Nothing but answers is written to `output`.
        void serve(std::istream& input, std::ostream& output);

    private:
        [[nodiscard]] std::optional<nlohmann::json> handleMessage(const nlohmann::json& message);
        [[nodiscard]] nlohmann::json handleRequest(const nlohmann::json& id, const std::string& method, const nlohmann::json& params);
        [[nodiscard]] nlohmann::json handleInitialize(const nlohmann::json& id, const nlohmann::json& params);
        [[nodiscard]] nlohmann::json dispatch(const nlohmann::json& id, const std::string& method,
                                              const nlohmann::json& params, bool modern);
        [[nodiscard]] nlohmann::json callTool(const nlohmann::json& id, const nlohmann::json& params, bool modern);
        [[nodiscard]] nlohmann::json toolDescription(const Tool& tool) const;
        [[nodiscard]] nlohmann::json completeResult(nlohmann::json result, bool modern) const;
        [[nodiscard]] nlohmann::json serverInfo() const;

        ServerIdentity _identity;
        std::vector<Tool> _tools;
        bool _legacyInitialized = false;
    };
}
