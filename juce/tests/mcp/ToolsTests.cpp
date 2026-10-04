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

// The six tools of the MCP server end to end: JSON-RPC lines in, JSON-RPC lines out, over a real session and the
// simulated sampler. [TASK-MCP-005, RQ-MCP-004, RQ-MCP-005, RQ-MCP-006, RQ-MCP-007, RQ-MCP-008, RQ-MCP-009,
// ADR-MCP-001 (DEC-MCP-006, DEC-MCP-007)]
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

    /// A server with the six tools, over a gateway on a simulated sampler holding PAD (1 keygroup), BASS (3, cutoffs 30,
    /// 60, 90) and LEAD (2); BASS is current.
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

        json call(const std::string& tool, json arguments = json::object())
        {
            const json request{{"jsonrpc", "2.0"},
                               {"id", ++lastId},
                               {"method", "tools/call"},
                               {"params",
                                {{"name", tool},
                                 {"arguments", std::move(arguments)},
                                 {"_meta",
                                  {{"io.modelcontextprotocol/protocolVersion", MODERN},
                                   {"io.modelcontextprotocol/clientCapabilities", json::object()}}}}}};
            const auto answer = server.handleLine(request.dump());
            REQUIRE(answer.has_value());
            return json::parse(*answer);
        }

        akm::RealScheduler scheduler;
        SimulatedMidiBackend backend{scheduler};
        SimulatedSampler* sampler = nullptr;
        mcp::SamplerGateway gateway{backend, configFor(backend)};
        mcp::McpServer server{mcp::ServerIdentity{"xs56k-mcp", "XS56K", "0.0.1", ""},
                              mcp::makeProgramEditingTools(gateway, mcp::ParameterCatalogue::standard())};
        int lastId = 0;
    };

    // Tools are called through the server, so the member order above matters: the server holds the tools, which hold
    // the gateway, which holds the backend. They are destroyed in reverse order.

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

    std::vector<akm::harness::AcceptedCommand> acceptedFor(const Rig& rig, akm::ItemId item)
    {
        std::vector<akm::harness::AcceptedCommand> found;
        const akm::ItemDescriptor& wanted = akm::descriptor(item);
        for (const auto& command : rig.sampler->acceptedCommands())
        {
            if (command.section == wanted.section && command.item == wanted.item)
                found.push_back(command);
        }
        return found;
    }
}

TEST_CASE("Given the server over a simulated sampler, When tools/list is called, Then exactly the six tools are listed and the three that only read carry readOnlyHint true, the others readOnlyHint false and destructiveHint false [RQ-MCP-008]",
          "[mcp][tools]")
{
    Rig rig;
    const json request{{"jsonrpc", "2.0"},
                       {"id", 1},
                       {"method", "tools/list"},
                       {"params",
                        {{"_meta",
                          {{"io.modelcontextprotocol/protocolVersion", MODERN},
                           {"io.modelcontextprotocol/clientCapabilities", json::object()}}}}}};

    const json answer = json::parse(*rig.server.handleLine(request.dump()));

    const json& tools = answer["result"]["tools"];
    std::vector<std::string> names;
    for (const json& tool : tools)
        names.push_back(tool["name"].get<std::string>());
    CHECK(names == std::vector<std::string>{"get_status", "list_programs", "select_program", "list_parameters",
                                            "get_parameters", "set_parameter"});
    for (const json& tool : tools)
    {
        CAPTURE(tool["name"]);
        const bool readsOnly = tool["name"] == "get_status" || tool["name"] == "list_programs" || tool["name"] == "list_parameters";
        CHECK(tool["annotations"]["readOnlyHint"].get<bool>() == readsOnly);
        if (!readsOnly)
            CHECK(tool["annotations"]["destructiveHint"] == false);
        CHECK(tool["annotations"]["openWorldHint"] == false);
        CHECK(tool["inputSchema"]["type"] == "object");
        CHECK_FALSE(tool["description"].get<std::string>().empty());
    }
    // The tools that act on the sampler's memory say it is not the disk.
    for (const char* writing : {"set_parameter", "select_program"})
    {
        for (const json& tool : tools)
        {
            if (tool["name"] == writing)
                CHECK(contains(tool["description"].get<std::string>(), "memory"));
        }
    }
}

TEST_CASE("Given a simulated sampler with BASS current, When get_status runs, Then the answer gives the DeviceID, three programs, BASS and its three keygroups [RQ-MCP-007]",
          "[mcp][tools]")
{
    Rig rig;

    const json answer = rig.call("get_status");

    REQUIRE_FALSE(isError(answer));
    const std::string text = textOf(answer);
    CHECK(contains(text, "DeviceID 0"));
    CHECK(contains(text, "3 programs"));
    CHECK(contains(text, "BASS"));
    CHECK(contains(text, "3 keygroups"));
}

TEST_CASE("Given a sampler with nothing in memory, When get_status and list_programs run, Then they say so and are not errors [RQ-MCP-007]",
          "[mcp][tools]")
{
    Rig rig(false);

    const json status = rig.call("get_status");
    const json programs = rig.call("list_programs");

    CHECK_FALSE(isError(status));
    CHECK(contains(textOf(status), "no program"));
    CHECK_FALSE(isError(programs));
    CHECK(contains(textOf(programs), "no program"));
}

TEST_CASE("Given three programs, When list_programs runs, Then the names are listed in memory order with their positions [RQ-MCP-007]",
          "[mcp][tools]")
{
    Rig rig;

    const std::string text = textOf(rig.call("list_programs"));

    const auto pad = text.find("0: PAD");
    const auto bass = text.find("1: BASS");
    const auto lead = text.find("2: LEAD");
    REQUIRE(pad != std::string::npos);
    REQUIRE(bass != std::string::npos);
    REQUIRE(lead != std::string::npos);
    CHECK(pad < bass);
    CHECK(bass < lead);
}

TEST_CASE("Given select_program with the name LEAD, When get_status runs, Then the current program is LEAD; a name or an index that does not exist is an error saying so; giving both or neither is refused [RQ-MCP-007]",
          "[mcp][tools]")
{
    Rig rig;

    const json selected = rig.call("select_program", {{"name", "LEAD"}});
    CHECK_FALSE(isError(selected));
    CHECK(contains(textOf(selected), "LEAD"));
    CHECK(contains(textOf(selected), "2 keygroups"));
    CHECK(contains(textOf(rig.call("get_status")), "LEAD"));

    const json byIndex = rig.call("select_program", {{"index", 0}});
    CHECK_FALSE(isError(byIndex));
    CHECK(contains(textOf(byIndex), "PAD"));

    const json missing = rig.call("select_program", {{"name", "NOPE"}});
    CHECK(isError(missing));
    CHECK(contains(textOf(missing), "NOPE"));
    CHECK(isError(rig.call("select_program", {{"index", 9}})));
    CHECK(isError(rig.call("select_program")));
    const json both = rig.call("select_program", {{"name", "PAD"}, {"index", 0}});
    CHECK(isError(both));
    CHECK(contains(textOf(both), "either"));
    CHECK(isError(rig.call("select_program", {{"index", -1}})));
    CHECK(isError(rig.call("select_program", {{"name", 7}})));
}

TEST_CASE("Given the lot 1 catalogue, When list_parameters runs, Then it lists 24 parameters and, for filter type, its 26 labels; a group narrows it and an unknown group is an error naming the groups [RQ-MCP-004]",
          "[mcp][tools]")
{
    Rig rig;

    const std::string all = textOf(rig.call("list_parameters"));
    std::size_t parameterLines = 0;
    for (std::size_t at = all.find("\n- "); at != std::string::npos; at = all.find("\n- ", at + 1))
        ++parameterLines;
    CHECK(parameterLines == 24);
    CHECK(contains(all, "filter cutoff"));
    CHECK(contains(all, "0 to 100"));
    CHECK(contains(all, "2-POLE LP+ (2)"));
    CHECK(contains(all, "VOWELISER (25)"));
    CHECK(contains(all, "TRIANGLE (1)"));
    CHECK(contains(all, "cutoff"));

    const std::string envelope = textOf(rig.call("list_parameters", {{"group", "Amp Envelope"}}));
    CHECK(contains(envelope, "amplitude envelope attack"));
    CHECK_FALSE(contains(envelope, "filter cutoff"));
    const std::string lfos = textOf(rig.call("list_parameters", {{"group", "lfo"}}));
    CHECK(contains(lfos, "lfo 1 rate"));
    CHECK(contains(lfos, "lfo 2 retrigger"));

    const json unknown = rig.call("list_parameters", {{"group", "reverb"}});
    CHECK(isError(unknown));
    CHECK(contains(textOf(unknown), "filter envelope"));
}

TEST_CASE("Given set_parameter filter cutoff 80, When it runs, Then every keygroup reads 80 and the answer says 80 [RQ-MCP-006]",
          "[mcp][tools]")
{
    Rig rig;

    const json answer = rig.call("set_parameter", {{"parameter", "filter cutoff"}, {"value", 80}});

    REQUIRE_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "80"));
    CHECK(contains(textOf(answer), "all 3 keygroups"));
    const std::string read = textOf(rig.call("get_parameters", {{"parameters", {"filter cutoff"}}}));
    CHECK(contains(read, "filter cutoff = 80 (all 3 keygroups)"));
}

TEST_CASE("Given the filter type with the label 2-pole LP+ in any case, spacing or hyphenation, When set, Then the sampler holds code 2 [RQ-MCP-006]",
          "[mcp][tools]")
{
    Rig rig;

    const json answer = rig.call("set_parameter", {{"parameter", "Filter Type"}, {"value", "2 POLE lp+"}});

    REQUIRE_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "2-POLE LP+"));
    CHECK(acceptedFor(rig, akm::ItemId::KeygroupSetFilterMode).front().data == akm::test::bytes({2}));
}

TEST_CASE("Given filter envelope depth -40, When set for keygroup 2, Then the sampler holds sign 1 magnitude 40 and the answer says -40 [RQ-MCP-006]",
          "[mcp][tools]")
{
    Rig rig;

    const json answer = rig.call("set_parameter", {{"parameter", "filter envelope depth"}, {"value", -40}, {"keygroup", 2}});

    REQUIRE_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "-40"));
    CHECK(contains(textOf(answer), "keygroup 2"));
    CHECK(acceptedFor(rig, akm::ItemId::KeygroupSetFilterEnvDepth).front().data == akm::test::bytes({1, 40}));
}

TEST_CASE("Given values that do not fit, When set_parameter runs, Then nothing is sent and the error names the range or proposes the name [RQ-MCP-006, RQ-MCP-009]",
          "[mcp][tools]")
{
    Rig rig;
    const auto setsBefore = acceptedFor(rig, akm::ItemId::KeygroupSetFilterCutoff).size();

    const json high = rig.call("set_parameter", {{"parameter", "filter cutoff"}, {"value", 101}});
    CHECK(isError(high));
    CHECK(contains(textOf(high), "0 to 100"));
    const json typo = rig.call("set_parameter", {{"parameter", "filter cutof"}, {"value", 50}});
    CHECK(isError(typo));
    CHECK(contains(textOf(typo), "filter cutoff"));
    const json label = rig.call("set_parameter", {{"parameter", "filter type"}, {"value", "sawtooth"}});
    CHECK(isError(label));
    CHECK(contains(textOf(label), "VOWELISER"));
    const json fraction = rig.call("set_parameter", {{"parameter", "filter cutoff"}, {"value", 80.5}});
    CHECK(isError(fraction));
    const json program = rig.call("set_parameter", {{"parameter", "lfo 1 rate"}, {"value", 40}, {"keygroup", 2}});
    CHECK(isError(program));
    CHECK(contains(textOf(program), "program"));
    const json beyond = rig.call("set_parameter", {{"parameter", "filter cutoff"}, {"value", 40}, {"keygroup", 4}});
    CHECK(isError(beyond));
    CHECK(contains(textOf(beyond), "has 3 keygroups"));
    CHECK(isError(rig.call("set_parameter", {{"parameter", "filter cutoff"}, {"value", 40}, {"keygroup", "some"}})));
    CHECK(isError(rig.call("set_parameter", {{"parameter", "filter cutoff"}, {"value", 40}, {"keygroup", 0}})));
    CHECK(isError(rig.call("set_parameter", {{"parameter", "filter cutoff"}, {"value", json::array()}})));

    CHECK(acceptedFor(rig, akm::ItemId::KeygroupSetFilterCutoff).size() == setsBefore);
}

TEST_CASE("Given the LFO parameters, When LFO 1 sync is set on with a boolean and LFO 2 waveform with a label, Then the sampler holds them, and \"all\" is accepted for a program parameter [RQ-MCP-006]",
          "[mcp][tools]")
{
    Rig rig;

    CHECK_FALSE(isError(rig.call("set_parameter", {{"parameter", "lfo 1 sync"}, {"value", true}})));
    CHECK_FALSE(isError(rig.call("set_parameter", {{"parameter", "lfo 2 waveform"}, {"value", "saw up"}, {"keygroup", "all"}})));

    CHECK(acceptedFor(rig, akm::ItemId::ProgramSetLfoSync).front().data == akm::test::bytes({1, 1}));
    CHECK(acceptedFor(rig, akm::ItemId::ProgramSetLfoWaveform).front().data == akm::test::bytes({2, 6}));
}

TEST_CASE("Given BASS with cutoffs 30, 60 and 90, When get_parameters reads the cutoff for all, for keygroup 2, and the filter group, Then the values are listed per keygroup, as one, and as the group's five parameters [RQ-MCP-005]",
          "[mcp][tools]")
{
    Rig rig;

    const std::string all = textOf(rig.call("get_parameters", {{"parameters", {"filter cutoff"}}}));
    CHECK(contains(all, "keygroup 1 = 30"));
    CHECK(contains(all, "keygroup 2 = 60"));
    CHECK(contains(all, "keygroup 3 = 90"));
    const std::string second = textOf(rig.call("get_parameters", {{"parameters", {"cutoff"}}, {"keygroup", 2}}));
    CHECK(contains(second, "filter cutoff = 60 (keygroup 2)"));

    const std::string filter = textOf(rig.call("get_parameters", {{"group", "filter"}}));
    for (const char* name : {"filter type", "filter cutoff", "filter resonance", "filter keyboard tracking", "filter attenuation"})
        CHECK(contains(filter, name));
}

TEST_CASE("Given a signed value and a choice stored on the sampler, When they are read, Then the depth is a signed number and the type its label with its code [RQ-MCP-005]",
          "[mcp][tools]")
{
    Rig rig;
    REQUIRE_FALSE(isError(rig.call("set_parameter", {{"parameter", "filter envelope depth"}, {"value", -40}})));
    REQUIRE_FALSE(isError(rig.call("set_parameter", {{"parameter", "filter type"}, {"value", 2}})));

    const std::string text =
        textOf(rig.call("get_parameters", {{"parameters", {"filter envelope depth", "filter type", "lfo 1 rate"}}}));

    CHECK(contains(text, "filter envelope depth = -40 (all 3 keygroups)"));
    CHECK(contains(text, "filter type = 2-POLE LP+ (code 2) (all 3 keygroups)"));
    CHECK(contains(text, "lfo 1 rate = 0 (program)"));
}

TEST_CASE("Given bad arguments, When get_parameters, set_parameter and select_program are called, Then each answer is an error naming what is wrong and the next call succeeds [RQ-MCP-009]",
          "[mcp][tools]")
{
    Rig rig;

    CHECK(isError(rig.call("get_parameters")));
    const json both = rig.call("get_parameters", {{"group", "filter"}, {"parameters", {"filter cutoff"}}});
    CHECK(isError(both));
    CHECK(contains(textOf(both), "either"));
    const json unknownName = rig.call("get_parameters", {{"parameters", {"filter cutoff", "zzzzzzzzzzzzzzzz"}}});
    CHECK(isError(unknownName));
    CHECK(contains(textOf(unknownName), "zzzzzzzzzzzzzzzz"));
    CHECK(isError(rig.call("get_parameters", {{"parameters", "filter cutoff"}})));
    const json unknownArgument = rig.call("set_parameter", {{"parameter", "filter cutoff"}, {"value", 1}, {"loudly", true}});
    CHECK(isError(unknownArgument));
    CHECK(contains(textOf(unknownArgument), "loudly"));

    CHECK_FALSE(isError(rig.call("get_status")));
}

TEST_CASE("Given a simulated sampler that does not answer, When set_parameter is called, Then the answer arrives within the command timeout and is an error saying the sampler did not answer [RQ-MCP-009]",
          "[mcp][tools]")
{
    Rig rig;
    REQUIRE_FALSE(isError(rig.call("get_status")));
    akm::harness::SamplerBehaviour silent;
    silent.silent = true;
    rig.sampler->setBehaviour(silent);

    const auto started = std::chrono::steady_clock::now();
    const json answer = rig.call("set_parameter", {{"parameter", "filter cutoff"}, {"value", 80}});
    const auto elapsed = std::chrono::steady_clock::now() - started;

    CHECK(isError(answer));
    CHECK(contains(textOf(answer), "did not answer"));
    CHECK(elapsed < std::chrono::seconds(3));
}

TEST_CASE("Given the sampler with no connection possible, When tools/list is asked and then get_status, Then the list works and the status is an error with the reason [RQ-MCP-003]",
          "[mcp][tools]")
{
    akm::RealScheduler scheduler;
    SimulatedMidiBackend backend(scheduler);  // no sampler attached
    mcp::GatewayConfig config = Rig::configFor(backend);
    mcp::SamplerGateway gateway(backend, config);
    mcp::McpServer server(mcp::ServerIdentity{"xs56k-mcp", "XS56K", "0.0.1", ""},
                          mcp::makeProgramEditingTools(gateway, mcp::ParameterCatalogue::standard()));
    const json meta{{"io.modelcontextprotocol/protocolVersion", MODERN},
                    {"io.modelcontextprotocol/clientCapabilities", json::object()}};

    const json list = json::parse(*server.handleLine(
        json{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "tools/list"}, {"params", {{"_meta", meta}}}}.dump()));
    const json status = json::parse(*server.handleLine(
        json{{"jsonrpc", "2.0"},
             {"id", 2},
             {"method", "tools/call"},
             {"params", {{"name", "get_status"}, {"_meta", meta}}}}
            .dump()));

    CHECK(list["result"]["tools"].size() == 6);
    CHECK(status["result"]["isError"] == true);
    CHECK(contains(status["result"]["content"][0]["text"].get<std::string>(), "answer"));
}
