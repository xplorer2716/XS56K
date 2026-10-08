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

// The program structure tools of the MCP server end to end: create_program, rename_program and delete_program, with
// the delete guard, over a real session and the simulated sampler. [TASK-MCP-011, RQ-MCP-013, RQ-MCP-014,
// RQ-MCP-015, RQ-MCP-016, RQ-MCP-017, ADR-MCP-002 (DEC-MCP-010, DEC-MCP-011)]
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "SimulatedPrograms.hpp"
#include "TestBytes.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/RealScheduler.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"
#include "mcp/McpServer.hpp"
#include "mcp/SamplerGateway.hpp"
#include "mcp/Tools.hpp"

using json = nlohmann::json;
using akm::harness::SimulatedMidiBackend;
using akm::harness::SimulatedSampler;

namespace
{
    constexpr std::chrono::milliseconds COMMAND_TIMEOUT{300};
    constexpr const char* MODERN = "2026-07-28";
    constexpr int INVALID_PARAMS = -32602;

    /// A server with the six editing tools and the three structure tools, over a gateway on a simulated sampler holding
    /// PAD (1 keygroup), BASS (3, cutoffs 30, 60, 90) and LEAD (2); BASS is current. Without seeding, the sampler is empty.
    struct Rig
    {
        explicit Rig(bool seeded = true)
        {
            sampler = &backend.addSampler();
            if (seeded)
                mcp::test::seedThreePrograms(backend);
        }

        static mcp::GatewayConfig configFor(const SimulatedMidiBackend& backend)
        {
            mcp::GatewayConfig config;
            config.inputPort = backend.inputName();
            config.outputPort = backend.outputName();
            config.commandTimeout = COMMAND_TIMEOUT;
            return config;
        }

        static std::vector<mcp::Tool> allTools(mcp::SamplerGateway& gateway)
        {
            std::vector<mcp::Tool> tools = mcp::makeProgramEditingTools(gateway, mcp::ParameterCatalogue::standard());
            for (mcp::Tool& tool : mcp::makeProgramStructureTools(gateway))
                tools.push_back(std::move(tool));
            return tools;
        }

        json request(const std::string& method, json params)
        {
            params["_meta"] = {{"io.modelcontextprotocol/protocolVersion", MODERN},
                               {"io.modelcontextprotocol/clientCapabilities", json::object()}};
            const json message{{"jsonrpc", "2.0"}, {"id", ++lastId}, {"method", method}, {"params", std::move(params)}};
            const auto answer = server.handleLine(message.dump());
            REQUIRE(answer.has_value());
            return json::parse(*answer);
        }

        json call(const std::string& tool, json arguments = json::object())
        {
            return request("tools/call", json{{"name", tool}, {"arguments", std::move(arguments)}});
        }

        akm::RealScheduler scheduler;
        SimulatedMidiBackend backend{scheduler};
        SimulatedSampler* sampler = nullptr;
        mcp::SamplerGateway gateway{backend, configFor(backend)};
        mcp::McpServer server{mcp::ServerIdentity{"xs56k-mcp", "XS56K", "0.0.1", ""}, allTools(gateway)};
        int lastId = 0;
    };

    std::string textOf(const json& answer)
    {
        REQUIRE(answer.contains("result"));
        return answer["result"]["content"][0]["text"].get<std::string>();
    }

    bool isError(const json& answer)
    {
        REQUIRE(answer.contains("result"));
        return answer["result"]["isError"].get<bool>();
    }

    bool contains(const std::string& text, const char* part)
    {
        return text.find(part) != std::string::npos;
    }

    std::size_t acceptedCount(const Rig& rig, akm::ItemId item)
    {
        std::size_t count = 0;
        const akm::ItemDescriptor& wanted = akm::descriptor(item);
        for (const auto& command : rig.sampler->acceptedCommands())
        {
            if (command.section == wanted.section && command.item == wanted.item)
                ++count;
        }
        return count;
    }

    json toolNamed(const json& list, const std::string& name)
    {
        for (const json& tool : list["result"]["tools"])
        {
            if (tool["name"] == name)
                return tool;
        }
        return json();
    }
}

TEST_CASE("Given three programs, When create_program is called with TEST and 3 keygroups, Then the program is listed, current and has 3 keygroups [RQ-MCP-015]",
          "[mcp][structure]")
{
    Rig rig;
    const json answer = rig.call("create_program", {{"name", "TEST"}, {"keygroups", 3}});
    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "TEST"));
    CHECK(contains(textOf(answer), "3 keygroups"));

    const std::string listing = textOf(rig.call("list_programs"));
    CHECK(contains(listing, "Programs in memory (4)"));
    CHECK(contains(listing, "TEST"));

    const std::string status = textOf(rig.call("get_status"));
    CHECK(contains(status, "Current program: TEST, 3 keygroups"));
    CHECK(acceptedCount(rig, akm::ItemId::ProgramCreateWithKeygroups) == 1);
}

TEST_CASE("Given an empty sampler, When create_program is called with one keygroup, Then the program is the only one and is current [RQ-MCP-015]",
          "[mcp][structure]")
{
    Rig rig(false);
    const json answer = rig.call("create_program", {{"name", "SCRATCH"}, {"keygroups", 1}});
    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "1 keygroup"));
    CHECK(contains(textOf(rig.call("list_programs")), "Programs in memory (1)"));
}

TEST_CASE("Given an invalid name or keygroup count, When create_program is called, Then nothing is sent and the answer says what is accepted [RQ-MCP-015, RQ-MCP-009]",
          "[mcp][structure]")
{
    Rig rig;
    const std::vector<json> invalid{
        json{{"name", "THIRTEENCHARS"}, {"keygroups", 2}},  // 13 characters
        json{{"name", ""}, {"keygroups", 2}},
        json{{"name", "CAF\xC3\x89"}, {"keygroups", 2}},    // not ASCII
        json{{"name", "TEST"}, {"keygroups", 0}},
        json{{"name", "TEST"}, {"keygroups", 100}},
        json{{"name", "TEST"}, {"keygroups", 2.5}},
        json{{"name", "TEST"}, {"keygroups", "three"}},
        json{{"name", 12}, {"keygroups", 2}},
        json{{"keygroups", 2}},
        json{{"name", "TEST"}},
        json{{"name", "TEST"}, {"keygroups", 2}, {"index", 0}},
    };
    for (const json& arguments : invalid)
    {
        INFO(arguments.dump());
        const json answer = rig.call("create_program", arguments);
        CHECK(isError(answer));
    }
    CHECK(acceptedCount(rig, akm::ItemId::ProgramCreateWithKeygroups) == 0);
    CHECK(contains(textOf(rig.call("create_program", {{"name", "THIRTEENCHARS"}, {"keygroups", 2}})), "12"));
    CHECK(contains(textOf(rig.call("create_program", {{"name", "TEST"}, {"keygroups", 100}})), "1 to 99"));
    CHECK(contains(textOf(rig.call("list_programs")), "Programs in memory (3)"));
}

TEST_CASE("Given a current program, When rename_program gives TEST2, Then the answer names the old and the new name and the list shows the new one [RQ-MCP-016]",
          "[mcp][structure]")
{
    Rig rig;
    const json answer = rig.call("rename_program", {{"name", "BASS2"}});
    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "BASS"));
    CHECK(contains(textOf(answer), "BASS2"));

    const std::string listing = textOf(rig.call("list_programs"));
    CHECK(contains(listing, "BASS2"));
    CHECK(contains(textOf(rig.call("get_status")), "Current program: BASS2, 3 keygroups"));
    CHECK(acceptedCount(rig, akm::ItemId::ProgramRenameCurrent) == 1);
}

TEST_CASE("Given an invalid name or no current program, When rename_program is called, Then nothing is renamed and the answer says why [RQ-MCP-016, RQ-MCP-009]",
          "[mcp][structure]")
{
    SECTION("an invalid name sends nothing")
    {
        Rig rig;
        CHECK(isError(rig.call("rename_program", {{"name", "THIRTEENCHARS"}})));
        CHECK(isError(rig.call("rename_program", {{"name", ""}})));
        CHECK(isError(rig.call("rename_program", json::object())));
        CHECK(acceptedCount(rig, akm::ItemId::ProgramRenameCurrent) == 0);
    }
    SECTION("an empty sampler has no current program")
    {
        Rig rig(false);
        const json answer = rig.call("rename_program", {{"name", "NEW"}});
        CHECK(isError(answer));
        CHECK(acceptedCount(rig, akm::ItemId::ProgramRenameCurrent) == 0);
    }
}

TEST_CASE("Given a current program BASS, When delete_program is called with another name, Then nothing is sent, the program stays and the answer names BASS [RQ-MCP-017]",
          "[mcp][structure]")
{
    Rig rig;
    const json answer = rig.call("delete_program", {{"confirm", "OTHER"}});
    CHECK(isError(answer));
    CHECK(contains(textOf(answer), "BASS"));
    CHECK(contains(textOf(answer), "nothing was deleted"));
    CHECK(acceptedCount(rig, akm::ItemId::ProgramDeleteCurrent) == 0);
    CHECK(contains(textOf(rig.call("list_programs")), "Programs in memory (3)"));
}

TEST_CASE("Given a current program BASS, When delete_program is called with its name, Then the program is gone and the answer says how many remain [RQ-MCP-017]",
          "[mcp][structure]")
{
    Rig rig;
    const json answer = rig.call("delete_program", {{"confirm", "BASS"}});
    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "Deleted the program \"BASS\""));
    CHECK(contains(textOf(answer), "2 programs"));
    CHECK(acceptedCount(rig, akm::ItemId::ProgramDeleteCurrent) == 1);

    const std::string listing = textOf(rig.call("list_programs"));
    CHECK(contains(listing, "Programs in memory (2)"));
    CHECK_FALSE(contains(listing, "BASS"));
}

TEST_CASE("Given a program created by the server, When it is created, renamed and deleted, Then the sampler ends as it began [RQ-MCP-015, RQ-MCP-016, RQ-MCP-017]",
          "[mcp][structure]")
{
    Rig rig;
    CHECK_FALSE(isError(rig.call("create_program", {{"name", "SCRATCH"}, {"keygroups", 2}})));
    CHECK_FALSE(isError(rig.call("rename_program", {{"name", "SCRATCH2"}})));
    CHECK_FALSE(isError(rig.call("delete_program", {{"confirm", "SCRATCH2"}})));
    const std::string listing = textOf(rig.call("list_programs"));
    CHECK(contains(listing, "Programs in memory (3)"));
    CHECK_FALSE(contains(listing, "SCRATCH"));
}

TEST_CASE("Given an empty sampler, When delete_program is called, Then nothing is sent and the answer says there is no current program [RQ-MCP-017, RQ-MCP-009]",
          "[mcp][structure]")
{
    Rig rig(false);
    const json answer = rig.call("delete_program", {{"confirm", "ANY"}});
    CHECK(isError(answer));
    CHECK(acceptedCount(rig, akm::ItemId::ProgramDeleteCurrent) == 0);
}

TEST_CASE("Given the nine tools, When tools/list is called, Then each tool's annotations match its tier and only delete_program is destructive [RQ-MCP-013]",
          "[mcp][structure]")
{
    Rig rig;
    const json list = rig.request("tools/list", json::object());
    CHECK(list["result"]["tools"].size() == 9);

    const json create = toolNamed(list, "create_program");
    const json rename = toolNamed(list, "rename_program");
    const json remove = toolNamed(list, "delete_program");
    for (const json* tool : {&create, &rename, &remove})
    {
        REQUIRE_FALSE(tool->is_null());
        CHECK_FALSE((*tool)["annotations"]["readOnlyHint"].get<bool>());
        CHECK(contains((*tool)["description"].get<std::string>(), "memory, not on disk"));
    }
    CHECK_FALSE(create["annotations"]["destructiveHint"].get<bool>());
    CHECK_FALSE(create["annotations"]["idempotentHint"].get<bool>());
    CHECK_FALSE(rename["annotations"]["destructiveHint"].get<bool>());
    CHECK(remove["annotations"]["destructiveHint"].get<bool>());
    CHECK(remove["inputSchema"]["required"][0] == "confirm");

    for (const char* reader : {"get_status", "list_programs", "list_parameters"})
        CHECK(toolNamed(list, reader)["annotations"]["readOnlyHint"].get<bool>());
    for (const json& tool : list["result"]["tools"])
    {
        const std::string name = tool["name"].get<std::string>();
        CHECK_FALSE(contains(name, "save"));
        CHECK_FALSE(contains(name, "load"));
        CHECK_FALSE(contains(name, "clear"));
        CHECK_FALSE(contains(name, "delete_all"));
        if (name != "delete_program")
            CHECK_FALSE(tool["annotations"]["destructiveHint"].get<bool>());
    }
}

TEST_CASE("Given the server, When a tool that deletes everything is called, Then the answer is an invalid-params error and nothing is sent [RQ-MCP-014]",
          "[mcp][structure]")
{
    Rig rig;
    for (const char* name : {"delete_all_programs", "clear_sampler_memory", "save_program", "load_program"})
    {
        const json answer = rig.call(name);
        REQUIRE(answer.contains("error"));
        CHECK(answer["error"]["code"] == INVALID_PARAMS);
    }
    CHECK(acceptedCount(rig, akm::ItemId::ProgramDeleteAll) == 0);
    CHECK(contains(textOf(rig.call("list_programs")), "Programs in memory (3)"));
}

TEST_CASE("Given an empty sampler, When programs are created as mmm, aaa, zzz and BBB, Then the sampler lists them alphabetically without regard to case, and a renamed program takes its new place [RQ-MCP-015, RQ-MCP-016, RQ-MCP-022]",
          "[mcp][structure]")
{
    // Observed on a real S5000 (2026-10-05, OBSERVATIONS-RQ-MCP-012-real-sampler.md): the order is the alphabet's, not the
    // order of creation, and it follows a renaming.
    Rig rig(false);
    for (const char* name : {"mmm", "aaa", "zzz", "BBB"})
        CHECK_FALSE(isError(rig.call("create_program", {{"name", name}, {"keygroups", 1}})));
    CHECK(textOf(rig.call("list_programs")) == "Programs in memory (4):\n0: aaa\n1: BBB\n2: mmm\n3: zzz\n");
    CHECK(contains(textOf(rig.call("get_status")), "Current program: BBB"));

    CHECK_FALSE(isError(rig.call("select_program", {{"name", "zzz"}})));
    CHECK_FALSE(isError(rig.call("rename_program", {{"name", "b first"}})));
    CHECK(textOf(rig.call("list_programs")) == "Programs in memory (4):\n0: aaa\n1: b first\n2: BBB\n3: mmm\n");
    CHECK(contains(textOf(rig.call("get_status")), "Current program: b first"));
}

TEST_CASE("Given four programs, When the current one is deleted, Then the program before it becomes current, or the first when none is before it, and none once memory is empty [RQ-MCP-017, RQ-MCP-022]",
          "[mcp][structure]")
{
    // Observed on a real S5000 (2026-10-05): deleting BBB (second of aaa, BBB, mmm, zzz) left aaa current; deleting aaa
    // (first) left mmm current; deleting the last program left the one before it current.
    Rig rig(false);
    for (const char* name : {"aaa", "BBB", "mmm", "zzz"})
        CHECK_FALSE(isError(rig.call("create_program", {{"name", name}, {"keygroups", 1}})));

    CHECK_FALSE(isError(rig.call("select_program", {{"name", "BBB"}})));
    CHECK_FALSE(isError(rig.call("delete_program", {{"confirm", "BBB"}})));
    CHECK(contains(textOf(rig.call("get_status")), "Current program: aaa"));

    CHECK_FALSE(isError(rig.call("delete_program", {{"confirm", "aaa"}})));
    CHECK(contains(textOf(rig.call("get_status")), "Current program: mmm"));

    CHECK_FALSE(isError(rig.call("select_program", {{"name", "zzz"}})));
    CHECK_FALSE(isError(rig.call("delete_program", {{"confirm", "zzz"}})));
    CHECK(contains(textOf(rig.call("get_status")), "Current program: mmm"));

    CHECK_FALSE(isError(rig.call("delete_program", {{"confirm", "mmm"}})));
    CHECK(contains(textOf(rig.call("get_status")), "The sampler holds no program."));
}
