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

// The protocol unit of the MCP server on strings and streams: JSON-RPC lines, the modern (stateless) and the
// legacy (`initialize`) era, `ping`, `tools/list` and `tools/call` over registered tools, and the errors.
// [TASK-MCP-002, RQ-MCP-001, RQ-MCP-009, ADR-MCP-001 (DEC-MCP-002)]
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "mcp/McpServer.hpp"

using json = nlohmann::json;

namespace
{
    constexpr const char* MODERN = "2026-07-28";
    constexpr const char* LEGACY_LATEST = "2025-11-25";

    mcp::Tool echoTool()
    {
        mcp::Tool tool;
        tool.definition.name = "echo";
        tool.definition.title = "Echo";
        tool.definition.description = "Returns its text argument.";
        tool.definition.inputSchema = json::parse(
            R"({"type":"object","properties":{"text":{"type":"string"}},"required":["text"]})");
        tool.definition.annotations.readOnly = true;
        tool.definition.annotations.destructive = false;
        tool.definition.annotations.idempotent = true;
        tool.handler = [](const json& arguments) { return mcp::ToolResult{arguments.at("text").get<std::string>(), false}; };
        return tool;
    }

    mcp::Tool boomTool()
    {
        mcp::Tool tool;
        tool.definition.name = "boom";
        tool.definition.description = "Always throws.";
        tool.definition.inputSchema = json::parse(R"({"type":"object","additionalProperties":false})");
        tool.handler = [](const json&) -> mcp::ToolResult { throw std::runtime_error("kaboom"); };
        return tool;
    }

    mcp::McpServer makeServer()
    {
        mcp::ServerIdentity identity;
        identity.name = "xs56k-mcp";
        identity.title = "XS56K";
        identity.version = "0.0.1";
        identity.instructions = "Edit a program.";
        return mcp::McpServer(identity, {echoTool(), boomTool()});
    }

    json meta(const char* version = MODERN)
    {
        return json{{"io.modelcontextprotocol/protocolVersion", version},
                    {"io.modelcontextprotocol/clientCapabilities", json::object()}};
    }

    json modernRequest(int id, const std::string& method, json params = json::object(), const char* version = MODERN)
    {
        params["_meta"] = meta(version);
        return json{{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}};
    }

    json legacyRequest(int id, const std::string& method, json params = json::object())
    {
        return json{{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}};
    }

    json ask(mcp::McpServer& server, const json& request)
    {
        const auto answer = server.handleLine(request.dump());
        REQUIRE(answer.has_value());
        return json::parse(*answer);
    }

    json initialize(mcp::McpServer& server, const char* version)
    {
        return ask(server, legacyRequest(1, "initialize",
                                         json{{"protocolVersion", version},
                                              {"capabilities", json::object()},
                                              {"clientInfo", {{"name", "test"}, {"version", "1"}}}}));
    }
}

TEST_CASE("Given a server on strings, When server/discover carries 2026-07-28, Then the answer lists the supported versions, the tools capability, the server info, resultType complete and the caching hints [RQ-MCP-001]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    const json answer = ask(server, modernRequest(7, "server/discover"));

    CHECK(answer["jsonrpc"] == "2.0");
    CHECK(answer["id"] == 7);
    const json& result = answer["result"];
    CHECK(result["resultType"] == "complete");
    CHECK(std::find(result["supportedVersions"].begin(), result["supportedVersions"].end(), MODERN) !=
          result["supportedVersions"].end());
    CHECK(result["capabilities"].contains("tools"));
    CHECK(result["_meta"]["io.modelcontextprotocol/serverInfo"]["name"] == "xs56k-mcp");
    CHECK(result["_meta"]["io.modelcontextprotocol/serverInfo"]["version"] == "0.0.1");
    CHECK(result["instructions"] == "Edit a program.");
    CHECK(result["ttlMs"].get<long long>() >= 0);
    CHECK(result["cacheScope"] == "public");
}

TEST_CASE("Given a request carrying a version the server does not support, When it is sent, Then the answer is error -32022 with the supported versions and the requested one [RQ-MCP-001]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    const json answer = ask(server, modernRequest(3, "tools/list", json::object(), "1900-01-01"));

    CHECK(answer["id"] == 3);
    CHECK(answer["error"]["code"] == -32022);
    CHECK(answer["error"]["data"]["requested"] == "1900-01-01");
    CHECK(answer["error"]["data"]["supported"].is_array());
    CHECK(answer["error"]["data"]["supported"].size() >= 1);
}

TEST_CASE("Given a request with no _meta and no earlier initialize, When it is sent, Then the answer is -32602 [RQ-MCP-001]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    const json answer = ask(server, legacyRequest(4, "tools/list"));

    CHECK(answer["id"] == 4);
    CHECK(answer["error"]["code"] == -32602);
}

TEST_CASE("Given a modern request without the client capabilities in its _meta, When it is sent, Then the answer is -32602 naming them [RQ-MCP-001]",
          "[mcp][protocol]")
{
    auto server = makeServer();
    json request = legacyRequest(5, "tools/list", json{{"_meta", {{"io.modelcontextprotocol/protocolVersion", MODERN}}}});

    const json answer = ask(server, request);

    CHECK(answer["error"]["code"] == -32602);
    CHECK(answer["error"]["message"].get<std::string>().find("clientCapabilities") != std::string::npos);
}

TEST_CASE("Given an initialize request with a supported legacy revision, When it is sent, Then the answer carries the same revision and the tools capability, and a later tools/list without _meta is served without resultType [RQ-MCP-001]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    const json answer = initialize(server, "2025-06-18");

    CHECK(answer["id"] == 1);
    CHECK(answer["result"]["protocolVersion"] == "2025-06-18");
    CHECK(answer["result"]["capabilities"].contains("tools"));
    CHECK(answer["result"]["serverInfo"]["name"] == "xs56k-mcp");
    CHECK(answer["result"]["instructions"] == "Edit a program.");
    CHECK_FALSE(server.handleLine(json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}.dump()).has_value());

    const json list = ask(server, legacyRequest(2, "tools/list"));
    REQUIRE(list.contains("result"));
    CHECK(list["result"]["tools"].size() == 2);
    CHECK_FALSE(list["result"].contains("resultType"));
}

TEST_CASE("Given an initialize request with a revision the server does not know, When it is sent, Then the answer carries the latest legacy revision [RQ-MCP-001]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    const json answer = initialize(server, "1900-01-01");

    CHECK(answer["result"]["protocolVersion"] == LEGACY_LATEST);
}

TEST_CASE("Given a line that is not JSON, When it is read, Then the answer is -32700 with a null id and the next line is still served [RQ-MCP-001, RQ-MCP-009]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    const auto bad = server.handleLine("{not json");
    REQUIRE(bad.has_value());
    const json badAnswer = json::parse(*bad);
    CHECK(badAnswer["error"]["code"] == -32700);
    CHECK(badAnswer["id"].is_null());

    const json good = ask(server, modernRequest(9, "ping"));
    CHECK(good["id"] == 9);
    CHECK(good["result"]["resultType"] == "complete");
}

TEST_CASE("Given valid JSON that is not a request, When it is read, Then the answer is -32600 [RQ-MCP-001]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    for (const char* line : {"[1,2]", "42", R"({"jsonrpc":"2.0","id":1})", R"({"jsonrpc":"1.0","id":1,"method":"ping"})",
                             R"({"jsonrpc":"2.0","id":null,"method":"ping"})", R"({"jsonrpc":"2.0","id":true,"method":"ping"})",
                             R"({"jsonrpc":"2.0","id":1,"method":7})"})
    {
        CAPTURE(line);
        const auto answer = server.handleLine(line);
        REQUIRE(answer.has_value());
        CHECK(json::parse(*answer)["error"]["code"] == -32600);
    }
}

TEST_CASE("Given a method the server does not have, When a request names it in either era, Then the answer is -32601 [RQ-MCP-001]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    CHECK(ask(server, modernRequest(1, "resources/list"))["error"]["code"] == -32601);

    initialize(server, LEGACY_LATEST);
    CHECK(ask(server, legacyRequest(2, "prompts/list"))["error"]["code"] == -32601);
}

TEST_CASE("Given ping in each era, When it is sent, Then the modern answer is an empty complete result and the legacy one is an empty object [RQ-MCP-001]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    const json modern = ask(server, modernRequest(1, "ping"));
    CHECK(modern["result"]["resultType"] == "complete");

    initialize(server, LEGACY_LATEST);
    const json legacy = ask(server, legacyRequest(2, "ping"));
    CHECK(legacy["result"] == json::object());
}

TEST_CASE("Given two registered tools, When tools/list runs, Then both are listed in registration order with their schema and annotations, and the modern answer carries the caching hints [RQ-MCP-001, RQ-MCP-008]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    const json answer = ask(server, modernRequest(1, "tools/list"));

    const json& result = answer["result"];
    CHECK(result["resultType"] == "complete");
    CHECK(result["ttlMs"].get<long long>() >= 0);
    CHECK(result["cacheScope"] == "public");
    REQUIRE(result["tools"].size() == 2);
    const json& echo = result["tools"][0];
    CHECK(echo["name"] == "echo");
    CHECK(echo["title"] == "Echo");
    CHECK(echo["description"] == "Returns its text argument.");
    CHECK(echo["inputSchema"]["required"][0] == "text");
    CHECK(echo["annotations"]["readOnlyHint"] == true);
    CHECK(echo["annotations"]["destructiveHint"] == false);
    CHECK(echo["annotations"]["idempotentHint"] == true);
    CHECK(echo["annotations"]["openWorldHint"] == false);
    CHECK(result["tools"][1]["name"] == "boom");
}

TEST_CASE("Given a registered tool, When tools/call names it with its argument, Then the result carries its text and isError false in either era [RQ-MCP-001]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    const json modern = ask(server, modernRequest(1, "tools/call", json{{"name", "echo"}, {"arguments", {{"text", "hello"}}}}));
    CHECK(modern["result"]["resultType"] == "complete");
    CHECK(modern["result"]["content"][0]["type"] == "text");
    CHECK(modern["result"]["content"][0]["text"] == "hello");
    CHECK(modern["result"]["isError"] == false);

    initialize(server, LEGACY_LATEST);
    const json legacy = ask(server, legacyRequest(2, "tools/call", json{{"name", "echo"}, {"arguments", {{"text", "again"}}}}));
    CHECK(legacy["result"]["content"][0]["text"] == "again");
    CHECK(legacy["result"]["isError"] == false);
    CHECK_FALSE(legacy["result"].contains("resultType"));
}

TEST_CASE("Given a call to a tool that is not registered, When it is sent, Then the answer is -32602 naming it [RQ-MCP-001, RQ-MCP-008]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    const json answer = ask(server, modernRequest(1, "tools/call", json{{"name", "delete_everything"}}));

    CHECK(answer["error"]["code"] == -32602);
    CHECK(answer["error"]["message"].get<std::string>().find("delete_everything") != std::string::npos);
}

TEST_CASE("Given a tools/call without the required argument, When it is sent, Then the result is an error naming it and the next call succeeds [RQ-MCP-009]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    const json missing = ask(server, modernRequest(1, "tools/call", json{{"name", "echo"}, {"arguments", json::object()}}));
    CHECK(missing["result"]["isError"] == true);
    CHECK(missing["result"]["content"][0]["text"].get<std::string>().find("text") != std::string::npos);

    const json absent = ask(server, modernRequest(2, "tools/call", json{{"name", "echo"}}));
    CHECK(absent["result"]["isError"] == true);

    const json ok = ask(server, modernRequest(3, "tools/call", json{{"name", "echo"}, {"arguments", {{"text", "fine"}}}}));
    CHECK(ok["result"]["isError"] == false);
}

TEST_CASE("Given tools/call arguments that are not an object or a tool name that is not a string, When it is sent, Then the answer is -32602 [RQ-MCP-009]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    CHECK(ask(server, modernRequest(1, "tools/call", json{{"name", "echo"}, {"arguments", "text"}}))["error"]["code"] == -32602);
    CHECK(ask(server, modernRequest(2, "tools/call", json{{"name", 7}}))["error"]["code"] == -32602);
    CHECK(ask(server, modernRequest(3, "tools/call", json::object()))["error"]["code"] == -32602);
}

TEST_CASE("Given a tool whose handler throws, When it is called, Then the result is an error in plain words and the server answers the next request [RQ-MCP-009]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    const json crashed = ask(server, modernRequest(1, "tools/call", json{{"name", "boom"}}));
    CHECK(crashed["result"]["isError"] == true);
    CHECK(crashed["result"]["content"][0]["text"].get<std::string>().find("kaboom") != std::string::npos);

    CHECK(ask(server, modernRequest(2, "ping"))["result"]["resultType"] == "complete");
}

TEST_CASE("Given a notification, When it is read, Then nothing is answered, whatever its name [RQ-MCP-001]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    CHECK_FALSE(server.handleLine(R"({"jsonrpc":"2.0","method":"notifications/initialized"})").has_value());
    CHECK_FALSE(server.handleLine(R"({"jsonrpc":"2.0","method":"notifications/cancelled","params":{"requestId":1}})").has_value());
    CHECK_FALSE(server.handleLine(R"({"jsonrpc":"2.0","method":"something/unknown"})").has_value());
}

TEST_CASE("Given input streams with requests, a blank line and a bad line, When the server serves them, Then it writes exactly one JSON line per request, nothing else, and stops at the end of the input [RQ-MCP-001]",
          "[mcp][protocol]")
{
    auto server = makeServer();
    std::stringstream input;
    input << modernRequest(1, "ping").dump() << "\n"
          << "\n"
          << "{broken\n"
          << R"({"jsonrpc":"2.0","method":"notifications/initialized"})" << "\n"
          << modernRequest(2, "tools/list").dump();  // no final newline
    std::stringstream output;

    server.serve(input, output);

    std::vector<json> answers;
    std::string line;
    while (std::getline(output, line))
    {
        REQUIRE_FALSE(line.empty());
        REQUIRE(line.find('\r') == std::string::npos);
        answers.push_back(json::parse(line));
    }
    REQUIRE(answers.size() == 3);
    CHECK(answers[0]["id"] == 1);
    CHECK(answers[1]["error"]["code"] == -32700);
    CHECK(answers[2]["id"] == 2);
    CHECK(answers[2]["result"]["tools"].size() == 2);
}

TEST_CASE("Given text that is not valid UTF-8 inside a tool result, When the answer is written, Then it is still one JSON line [RQ-MCP-001, RQ-MCP-009]",
          "[mcp][protocol]")
{
    auto server = makeServer();

    const auto answer = server.handleLine(modernRequest(1, "tools/call", json{{"name", "echo"}, {"arguments", {{"text", "ok"}}}}).dump());
    REQUIRE(answer.has_value());
    CHECK(answer->find('\n') == std::string::npos);

    mcp::Tool invalid;
    invalid.definition.name = "invalid";
    invalid.definition.inputSchema = json::parse(R"({"type":"object"})");
    invalid.handler = [](const json&) { return mcp::ToolResult{std::string("caf\xE9"), false}; };
    mcp::ServerIdentity identity;
    identity.name = "x";
    identity.version = "1";
    mcp::McpServer other(identity, {invalid});

    const auto line = other.handleLine(modernRequest(1, "tools/call", json{{"name", "invalid"}}).dump());
    REQUIRE(line.has_value());
    CHECK(line->find('\n') == std::string::npos);
    CHECK_NOTHROW(json::parse(*line));
}
