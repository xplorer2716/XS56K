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

// The zone parameters of the MCP server: the rows of section 06, their values, and the `zone` argument of the
// gateway and the tools, over a real session and the simulated sampler. [TASK-MCP-014, RQ-MCP-019,
// ADR-MCP-002 (DEC-MCP-012)]
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "SimulatedPrograms.hpp"
#include "TestBytes.hpp"
#include "akm/ItemCatalogue.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/RealScheduler.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"
#include "mcp/McpServer.hpp"
#include "mcp/ParameterCatalogue.hpp"
#include "mcp/SamplerGateway.hpp"
#include "mcp/Tools.hpp"

using json = nlohmann::json;
using akm::harness::SimulatedMidiBackend;
using akm::harness::SimulatedSampler;
using mcp::KeygroupSelection;
using mcp::ParameterCatalogue;
using mcp::ParameterDefinition;
using mcp::ParameterKind;
using mcp::ParameterScope;
using mcp::ZoneSelection;

namespace
{
    constexpr std::chrono::milliseconds COMMAND_TIMEOUT{300};
    constexpr const char* MODERN = "2026-07-28";
    constexpr int ZONES = 4;

    const ParameterDefinition& named(const char* name)
    {
        const auto resolution = ParameterCatalogue::standard().resolveName(name);
        INFO("parameter: " << name);
        REQUIRE(resolution.parameter != nullptr);
        return *resolution.parameter;
    }

    struct Rig
    {
        Rig()
        {
            sampler = &backend.addSampler();
            mcp::test::seedThreePrograms(backend);  // BASS (3 keygroups) is current
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
            const json message{{"jsonrpc", "2.0"},
                               {"id", ++lastId},
                               {"method", "tools/call"},
                               {"params",
                                {{"name", tool},
                                 {"arguments", std::move(arguments)},
                                 {"_meta",
                                  {{"io.modelcontextprotocol/protocolVersion", MODERN},
                                   {"io.modelcontextprotocol/clientCapabilities", json::object()}}}}}};
            const auto answer = server.handleLine(message.dump());
            REQUIRE(answer.has_value());
            return json::parse(*answer);
        }

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
        SimulatedMidiBackend backend{scheduler};
        SimulatedSampler* sampler = nullptr;
        mcp::SamplerGateway gateway{backend, configFor(backend)};
        mcp::McpServer server{mcp::ServerIdentity{"xs56k-mcp", "XS56K", "0.0.1", ""},
                              mcp::makeAllTools(gateway, ParameterCatalogue::standard())};
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

    std::vector<std::int64_t> valuesOf(const std::vector<mcp::ParameterValue>& values)
    {
        std::vector<std::int64_t> numbers;
        for (const mcp::ParameterValue& value : values)
            numbers.push_back(value.value);
        return numbers;
    }
}

TEST_CASE("Given the catalogue, When the zone group is read, Then it holds the 13 zone parameters of section 06 as zone-scope rows, and every item of the section is a row or the sample assignment, left out with its reason [RQ-MCP-019]",
          "[mcp][zone][catalogue]")
{
    const ParameterCatalogue& catalogue = ParameterCatalogue::standard();
    const auto groups = catalogue.findGroups("zone");
    REQUIRE(groups.size() == 1);
    const auto rows = catalogue.parametersInGroup(*groups.front());
    CHECK(rows.size() == 13);
    for (const ParameterDefinition* row : rows)
        CHECK(row->scope == ParameterScope::Zone);

    // The sample assignment is a text item (a mixed byte and string shape): not a numeric parameter of the table.
    const std::set<std::size_t> excused{static_cast<std::size_t>(akm::ItemId::ZoneSetSample),
                                        static_cast<std::size_t>(akm::ItemId::ZoneGetSample)};
    std::set<std::size_t> covered;
    for (const ParameterDefinition& parameter : catalogue.parameters())
    {
        covered.insert(static_cast<std::size_t>(parameter.setItem));
        covered.insert(static_cast<std::size_t>(parameter.getItem));
    }
    constexpr std::uint8_t SECTION_ZONE = 0x06;
    std::size_t inSection = 0;
    for (std::size_t index = 0; index < std::size(akm::ITEM_TABLE); ++index)
    {
        const akm::ItemDescriptor& item = akm::ITEM_TABLE[index];
        if (item.section != SECTION_ZONE)
            continue;
        ++inSection;
        INFO("unaccounted item: " << item.name);
        CHECK((covered.count(index) == 1 || excused.count(index) == 1));
    }
    CHECK(inSection == 28);
}

TEST_CASE("Given the zone pan, When values are converted, Then -50 to 50 is the sampler's 14 to 114 with 0 at 64 [RQ-MCP-019]",
          "[mcp][zone][catalogue]")
{
    const ParameterDefinition& pan = named("zone pan");
    CHECK(pan.kind == ParameterKind::Number);
    CHECK(mcp::resolveValue(pan, std::int64_t{-50}).value == -50);
    CHECK(mcp::resolveValue(pan, std::int64_t{50}).value == 50);
    CHECK_FALSE(mcp::resolveValue(pan, std::int64_t{51}).value.has_value());
    CHECK(mcp::toItemValues(pan, -50) == std::vector<std::int64_t>{14});
    CHECK(mcp::toItemValues(pan, 0) == std::vector<std::int64_t>{64});
    CHECK(mcp::toItemValues(pan, 50) == std::vector<std::int64_t>{114});
    const std::vector<std::int64_t> centre{64};
    CHECK(mcp::fromItemValues(pan, centre) == 0);
    CHECK(pan.description.find("left") != std::string::npos);
}

TEST_CASE("Given the zone velocity to start, When values are converted, Then +-9999 is a sign and a magnitude of two 7-bit bytes, most significant first [RQ-MCP-019]",
          "[mcp][zone][catalogue]")
{
    const ParameterDefinition& start = named("zone velocity to start");
    CHECK(start.kind == ParameterKind::Signed);
    CHECK(mcp::resolveValue(start, std::int64_t{9999}).value == 9999);
    CHECK(mcp::resolveValue(start, std::int64_t{-9999}).value == -9999);
    CHECK_FALSE(mcp::resolveValue(start, std::int64_t{10000}).value.has_value());
    CHECK(mcp::toItemValues(start, -9999) == std::vector<std::int64_t>{1, 78, 15});
    CHECK(mcp::toItemValues(start, 128) == std::vector<std::int64_t>{0, 1, 0});
    CHECK(mcp::toItemValues(start, 0) == std::vector<std::int64_t>{0, 0, 0});
    const std::vector<std::int64_t> reply{1, 78, 15};
    CHECK(mcp::fromItemValues(start, reply) == -9999);
    const std::vector<std::int64_t> tooShort{1, 78};
    CHECK_FALSE(mcp::fromItemValues(start, tooShort).has_value());
}

TEST_CASE("Given the zone output and playback, When their labels are read, Then they are the spec's in code order [RQ-MCP-019]",
          "[mcp][zone][catalogue]")
{
    const ParameterDefinition& output = named("zone output");
    REQUIRE(output.labels.size() == 25);
    CHECK(output.labels[0] == "MULTI");
    CHECK(output.labels[1] == "OP1/2");
    CHECK(output.labels[8] == "OP15/16");
    CHECK(output.labels[9] == "OP1");
    CHECK(output.labels[24] == "OP16");
    CHECK(mcp::resolveValue(output, "op15/16").value == 8);
    CHECK(mcp::resolveValue(output, "op1").value == 9);

    const ParameterDefinition& playback = named("zone playback");
    REQUIRE(playback.labels.size() == 7);
    CHECK(playback.labels[0] == "NO LOOPING");
    CHECK(playback.labels[6] == "AS SAMPLE");
    CHECK(mcp::resolveValue(playback, "one shot").value == 1);
}

TEST_CASE("Given BASS with three keygroups, When the zone level is set for keygroup 2, zone 3 only, Then only that zone changes and a reading of every zone of every keygroup gives 12 values numbered from 1 [RQ-MCP-019]",
          "[mcp][zone][gateway]")
{
    Rig rig;
    const ParameterDefinition& level = named("zone level");

    const auto written = rig.gateway.writeParameter(level, 25, KeygroupSelection::of(2), ZoneSelection::of(3));
    REQUIRE(written.ok());
    REQUIRE(written.value->size() == 1);
    CHECK(written.value->front().keygroup == 2);
    CHECK(written.value->front().zone == 3);
    CHECK(written.value->front().value == 25);

    const auto all = rig.gateway.readParameter(level, KeygroupSelection::all(), ZoneSelection::all());
    REQUIRE(all.ok());
    REQUIRE(all.value->size() == 3 * ZONES);
    for (std::size_t i = 0; i < all.value->size(); ++i)
    {
        const mcp::ParameterValue& value = all.value->at(i);
        CHECK(value.keygroup == static_cast<int>(i / ZONES) + 1);
        CHECK(value.zone == static_cast<int>(i % ZONES) + 1);
        CHECK(value.value == (value.keygroup == 2 && value.zone == 3 ? 25 : 0));
    }

    const auto zoneTwo = rig.gateway.readParameter(level, KeygroupSelection::all(), ZoneSelection::of(2));
    REQUIRE(zoneTwo.ok());
    CHECK(valuesOf(*zoneTwo.value) == std::vector<std::int64_t>{0, 0, 0});
    const auto keygroupTwo = rig.gateway.readParameter(level, KeygroupSelection::of(2), ZoneSelection::all());
    REQUIRE(keygroupTwo.ok());
    CHECK(valuesOf(*keygroupTwo.value) == std::vector<std::int64_t>{0, 0, 25, 0});
}

TEST_CASE("Given BASS, When the zone level is set for all keygroups and all zones, Then one Set with zone 0 is sent and all 12 zones read back the value [RQ-MCP-019]",
          "[mcp][zone][gateway]")
{
    Rig rig;
    const ParameterDefinition& level = named("zone level");

    const auto written = rig.gateway.writeParameter(level, -40, KeygroupSelection::all(), ZoneSelection::all());

    REQUIRE(written.ok());
    CHECK(written.value->size() == 3 * ZONES);
    for (const mcp::ParameterValue& value : *written.value)
        CHECK(value.value == -40);
    CHECK(rig.accepted(akm::ItemId::ZoneSetLevel) == 1);
}

TEST_CASE("Given a zone outside 1 to 4, When a zone parameter is read or set, Then nothing is sent and the problem says the zones are 1 to 4 [RQ-MCP-019, RQ-MCP-009]",
          "[mcp][zone][gateway]")
{
    Rig rig;
    const ParameterDefinition& level = named("zone level");
    for (const int zone : {0, 5, -1})
    {
        const auto read = rig.gateway.readParameter(level, KeygroupSelection::all(), ZoneSelection::of(zone));
        const auto written = rig.gateway.writeParameter(level, 5, KeygroupSelection::all(), ZoneSelection::of(zone));
        for (const auto& outcome : {read, written})
        {
            CHECK_FALSE(outcome.ok());
            CHECK(contains(outcome.problem, "zones 1 to 4"));
        }
    }
    CHECK(rig.accepted(akm::ItemId::ZoneSetLevel) == 0);
    CHECK(rig.accepted(akm::ItemId::ZoneGetLevel) == 0);
}

TEST_CASE("Given the tools, When set_parameter is called on a zone parameter with a keygroup and a zone, Then the answer names both and the value reads back for that zone [RQ-MCP-019]",
          "[mcp][zone][tools]")
{
    Rig rig;
    const json answer = rig.call("set_parameter", {{"parameter", "zone level"}, {"value", 10}, {"keygroup", 1}, {"zone", 2}});
    CHECK_FALSE(isError(answer));
    CHECK(contains(textOf(answer), "zone level = 10 (keygroup 1, zone 2)"));

    const std::string everything = textOf(rig.call("get_parameters", {{"parameters", json::array({"zone level"})}, {"keygroup", 1}}));
    CHECK(contains(everything, "zone level:"));
    CHECK(contains(everything, "zone 1 = 0"));
    CHECK(contains(everything, "zone 2 = 10"));
    CHECK(contains(everything, "zone 4 = 0"));

    const std::string uniform = textOf(rig.call("get_parameters", {{"parameters", json::array({"zone mute"})}}));
    CHECK(contains(uniform, "zone mute = off (all 3 keygroups, all 4 zones)"));
}

TEST_CASE("Given the tools, When a zone is given to a parameter that is not a zone parameter, or is not 1 to 4 or all, Then the answer is an error and nothing is sent [RQ-MCP-019, RQ-MCP-009]",
          "[mcp][zone][tools]")
{
    Rig rig;
    const std::size_t cutoffSetsBefore = rig.accepted(akm::ItemId::KeygroupSetFilterCutoff);  // the seeding sets some
    CHECK(isError(rig.call("set_parameter", {{"parameter", "filter cutoff"}, {"value", 50}, {"zone", 2}})));
    CHECK(isError(rig.call("set_parameter", {{"parameter", "lfo 1 rate"}, {"value", 50}, {"zone", 2}})));
    CHECK(isError(rig.call("get_parameters", {{"parameters", json::array({"filter cutoff"})}, {"zone", 2}})));
    for (const json& zone : {json(0), json(5), json(1.5), json("two"), json(-1)})
    {
        INFO(zone.dump());
        CHECK(isError(rig.call("set_parameter", {{"parameter", "zone level"}, {"value", 5}, {"zone", zone}})));
    }
    CHECK_FALSE(isError(rig.call("set_parameter", {{"parameter", "zone level"}, {"value", 5}, {"zone", "all"}})));
    CHECK(contains(textOf(rig.call("set_parameter", {{"parameter", "filter cutoff"}, {"value", 50}, {"zone", 2}})), "zone"));
    CHECK(rig.accepted(akm::ItemId::ZoneSetLevel) == 1);
    CHECK(rig.accepted(akm::ItemId::KeygroupSetFilterCutoff) == cutoffSetsBefore);
}
