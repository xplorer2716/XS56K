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

// The multi tools of the MCP server: the catalogue of a multi part's parameters (section 0C), the gateway's multi calls
// and the tools list_multis, select_multi, get_multi_parameters and set_multi_parameter, over a real session and the
// simulated sampler. [TASK-MCP-016, RQ-MCP-021, ADR-MCP-002 (DEC-MCP-012, DEC-MCP-013)]
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
using mcp::ParameterCatalogue;
using mcp::ParameterDefinition;
using mcp::ParameterKind;
using mcp::ParameterScope;
using mcp::PartSelection;

namespace
{
    constexpr std::chrono::milliseconds COMMAND_TIMEOUT{300};
    constexpr const char* MODERN = "2026-07-28";
    constexpr int PARTS = 32;  // what the simulated sampler gives a multi

    const ParameterDefinition& named(const char* name)
    {
        const auto resolution = ParameterCatalogue::multis().resolveName(name);
        INFO("multi parameter: " << name);
        REQUIRE(resolution.parameter != nullptr);
        return *resolution.parameter;
    }

    /// A simulated sampler holding the multis LIVE and STUDIO (32 parts each); with `seeded` false, none.
    struct Rig
    {
        explicit Rig(bool seeded = true)
        {
            sampler = &backend.addSampler();
            mcp::test::seedThreePrograms(backend);
            if (seeded)
                sampler->setMultiNames({"LIVE", "STUDIO"});
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
}

TEST_CASE("Given the multi catalogue, When it is read, Then it holds the 12 part parameters of section 0C in two groups, 7 and 5 [RQ-MCP-021]",
          "[mcp][multi][catalogue]")
{
    const ParameterCatalogue& catalogue = ParameterCatalogue::multis();
    CHECK(catalogue.parameters().size() == 12);
    CHECK(catalogue.groups().size() == 2);
    for (const ParameterDefinition& parameter : catalogue.parameters())
    {
        CHECK(parameter.scope == ParameterScope::MultiPart);
        CHECK_FALSE(parameter.readOnly);
    }
    const auto mix = catalogue.findGroups("mix");
    const auto setup = catalogue.findGroups("setup");
    REQUIRE(mix.size() == 1);
    REQUIRE(setup.size() == 1);
    CHECK(catalogue.parametersInGroup(*mix.front()).size() == 7);
    CHECK(catalogue.parametersInGroup(*setup.front()).size() == 5);
}

TEST_CASE("Given the multi catalogue, When it is compared with the AKM item catalogue, Then every part parameter item of section 0C is a row and the rest are left out for their reason [RQ-MCP-021]",
          "[mcp][multi][catalogue]")
{
    // Selection, creation, deletion and renaming of multis and parts, the assignment of programs to parts, the program
    // number, and the information Gets (counts, names, the grouped Gets): not part parameters.
    const std::set<std::size_t> leftOut{
        static_cast<std::size_t>(akm::ItemId::MultiSetPartCount),         static_cast<std::size_t>(akm::ItemId::MultiCreate),
        static_cast<std::size_t>(akm::ItemId::MultiSelectByName),         static_cast<std::size_t>(akm::ItemId::MultiSelectByIndex),
        static_cast<std::size_t>(akm::ItemId::MultiDeleteAll),            static_cast<std::size_t>(akm::ItemId::MultiDeleteCurrent),
        static_cast<std::size_t>(akm::ItemId::MultiGetCurrentIndex),      static_cast<std::size_t>(akm::ItemId::MultiGetCurrentName),
        static_cast<std::size_t>(akm::ItemId::MultiGetCount),             static_cast<std::size_t>(akm::ItemId::MultiGetProgramNumber),
        static_cast<std::size_t>(akm::ItemId::MultiGetPartCount),         static_cast<std::size_t>(akm::ItemId::MultiGetPartName),
        static_cast<std::size_t>(akm::ItemId::MultiGetAllPartNames),      static_cast<std::size_t>(akm::ItemId::MultiGetAllPartParameters),
        static_cast<std::size_t>(akm::ItemId::MultiGetMuteSolo),          static_cast<std::size_t>(akm::ItemId::MultiGetAllProgramNumbers),
        static_cast<std::size_t>(akm::ItemId::MultiGetAllNames),          static_cast<std::size_t>(akm::ItemId::MultiGetAllPartCounts),
        static_cast<std::size_t>(akm::ItemId::MultiRename),               static_cast<std::size_t>(akm::ItemId::MultiSetProgramNumber),
        static_cast<std::size_t>(akm::ItemId::MultiSetPartByIndex),       static_cast<std::size_t>(akm::ItemId::MultiSetPartByName),
        static_cast<std::size_t>(akm::ItemId::MultiDeletePart)};
    std::set<std::size_t> covered;
    for (const ParameterDefinition& parameter : ParameterCatalogue::multis().parameters())
    {
        covered.insert(static_cast<std::size_t>(parameter.setItem));
        covered.insert(static_cast<std::size_t>(parameter.getItem));
    }
    constexpr std::uint8_t SECTION_MULTI = 0x0C;
    std::size_t inSection = 0;
    for (std::size_t index = 0; index < std::size(akm::ITEM_TABLE); ++index)
    {
        const akm::ItemDescriptor& item = akm::ITEM_TABLE[index];
        if (item.section != SECTION_MULTI)
            continue;
        ++inSection;
        INFO("unaccounted item: " << item.name);
        CHECK((covered.count(index) == 1 || leftOut.count(index) == 1));
    }
    CHECK(inSection == 47);
}

TEST_CASE("Given the part parameters, When values are converted, Then the pan, fine tune and transpose are centred signed numbers over the sampler's codes [RQ-MCP-021]",
          "[mcp][multi][catalogue]")
{
    const ParameterDefinition& pan = named("part pan");
    CHECK(mcp::toItemValues(pan, -50) == std::vector<std::int64_t>{14});
    CHECK(mcp::toItemValues(pan, 0) == std::vector<std::int64_t>{64});
    CHECK(mcp::toItemValues(pan, 50) == std::vector<std::int64_t>{114});
    CHECK_FALSE(mcp::resolveValue(pan, std::int64_t{51}).value.has_value());

    const ParameterDefinition& fine = named("part fine tune");
    CHECK(mcp::resolveValue(fine, std::int64_t{-50}).value == -50);
    CHECK(mcp::toItemValues(fine, -50) == std::vector<std::int64_t>{0});
    CHECK(mcp::toItemValues(fine, 0) == std::vector<std::int64_t>{50});
    CHECK(mcp::toItemValues(fine, 50) == std::vector<std::int64_t>{100});

    const ParameterDefinition& transpose = named("part transpose");
    CHECK(mcp::toItemValues(transpose, -36) == std::vector<std::int64_t>{0});
    CHECK(mcp::toItemValues(transpose, 0) == std::vector<std::int64_t>{36});
    CHECK(mcp::toItemValues(transpose, 36) == std::vector<std::int64_t>{72});
    CHECK_FALSE(mcp::resolveValue(transpose, std::int64_t{37}).value.has_value());

    for (const char* note : {"part low note", "part high note"})
    {
        CHECK(mcp::resolveValue(named(note), std::int64_t{21}).value == 21);
        CHECK_FALSE(mcp::resolveValue(named(note), std::int64_t{20}).value.has_value());
        CHECK(mcp::toItemValues(named(note), 60) == std::vector<std::int64_t>{60});
    }
}

TEST_CASE("Given the part choices, When their labels are read, Then the MIDI channel runs 1A to 16B, the output has 24 labels and the effects channel five [RQ-MCP-021]",
          "[mcp][multi][catalogue]")
{
    const ParameterDefinition& channel = named("part midi channel");
    REQUIRE(channel.labels.size() == 32);
    CHECK(channel.labels[0] == "1A");
    CHECK(channel.labels[15] == "16A");
    CHECK(channel.labels[16] == "1B");
    CHECK(channel.labels[31] == "16B");
    CHECK(mcp::resolveValue(channel, "10b").value == 25);

    const ParameterDefinition& output = named("part output");
    REQUIRE(output.labels.size() == 24);
    CHECK(output.labels[0] == "OP1/2");
    CHECK(output.labels[7] == "OP15/16");
    CHECK(output.labels[8] == "OP1");
    CHECK(output.labels[23] == "OP16");

    const ParameterDefinition& effects = named("part effects channel");
    CHECK(effects.labels == std::vector<std::string>{"OFF", "FX1", "FX2", "RV3", "RV4"});
    CHECK(named("part mute").kind == ParameterKind::Switch);
    CHECK(named("part solo").kind == ParameterKind::Switch);
}

TEST_CASE("Given no multi in memory, When the multis are listed, Then there is none and no multi is current [RQ-MCP-021]",
          "[mcp][multi][gateway]")
{
    Rig rig(false);
    const auto listing = rig.gateway.listMultis();
    REQUIRE(listing.ok());
    CHECK(listing.value->multis.empty());
    CHECK_FALSE(listing.value->current.has_value());
}

TEST_CASE("Given two multis, When they are listed and one is selected by name and the other by index, Then the names come in order and the current one reports its part count [RQ-MCP-021]",
          "[mcp][multi][gateway]")
{
    Rig rig;
    const auto listing = rig.gateway.listMultis();
    REQUIRE(listing.ok());
    REQUIRE(listing.value->multis.size() == 2);
    CHECK(listing.value->multis[0].name == "LIVE");
    CHECK(listing.value->multis[1].index == 1);
    CHECK_FALSE(listing.value->current.has_value());

    const auto live = rig.gateway.selectMultiByName("LIVE");
    REQUIRE(live.ok());
    CHECK(live.value->name == "LIVE");
    CHECK(live.value->index == 0);
    const auto studio = rig.gateway.selectMultiByIndex(1);
    REQUIRE(studio.ok());
    CHECK(studio.value->name == "STUDIO");
    const auto after = rig.gateway.listMultis();
    CHECK(after.value->current == 1);
    CHECK(after.value->currentPartCount == PARTS);

    const auto missing = rig.gateway.selectMultiByName("NOPE");
    CHECK_FALSE(missing.ok());
    CHECK(contains(missing.problem, "No multi is named \"NOPE\""));
    const auto far = rig.gateway.selectMultiByIndex(9);
    CHECK_FALSE(far.ok());
    CHECK(contains(far.problem, "No multi is at index 9"));
}

TEST_CASE("Given a selected multi, When the level of part 3 is set, Then only part 3 changes, the 32 parts read back numbered from 1, and setting all parts sends one Set per part [RQ-MCP-021]",
          "[mcp][multi][gateway]")
{
    Rig rig;
    REQUIRE(rig.gateway.selectMultiByName("LIVE").ok());
    const ParameterDefinition& level = named("part level");

    const auto written = rig.gateway.writeMultiParameter(level, 77, PartSelection::of(3));
    REQUIRE(written.ok());
    REQUIRE(written.value->size() == 1);
    CHECK(written.value->front().part == 3);
    CHECK(written.value->front().value == 77);
    CHECK(rig.accepted(akm::ItemId::MultiSetLevel) == 1);

    const auto all = rig.gateway.readMultiParameter(level, PartSelection::all());
    REQUIRE(all.ok());
    REQUIRE(all.value->size() == PARTS);
    for (const mcp::PartValue& value : *all.value)
        CHECK(value.value == (value.part == 3 ? 77 : 0));
    CHECK(all.value->front().part == 1);
    CHECK(all.value->back().part == PARTS);

    const auto every = rig.gateway.writeMultiParameter(level, 40, PartSelection::all());
    REQUIRE(every.ok());
    REQUIRE(every.value->size() == PARTS);
    for (const mcp::PartValue& value : *every.value)
        CHECK(value.value == 40);
    CHECK(rig.accepted(akm::ItemId::MultiSetLevel) == 1 + PARTS);
}

TEST_CASE("Given the part wire number, When part 1 is set, Then the sampler is sent part 0, the first part [RQ-MCP-021]",
          "[mcp][multi][gateway]")
{
    // The tools number the parts from 1 as the front panel does; the wire's part number starts at 0 (spec Table 16,
    // 0 to 127). An assumption of the tools, to be observed on a real sampler with a multi (TASK-MCP-016).
    Rig rig;
    REQUIRE(rig.gateway.selectMultiByName("LIVE").ok());
    REQUIRE(rig.gateway.writeMultiParameter(named("part level"), 55, PartSelection::of(1)).ok());
    const auto sent = rig.sampler->acceptedCommands();
    bool found = false;
    const akm::ItemDescriptor& item = akm::descriptor(akm::ItemId::MultiSetLevel);
    for (const auto& command : sent)
    {
        if (command.section == item.section && command.item == item.item)
        {
            REQUIRE_FALSE(command.data.empty());
            CHECK(command.data.front() == 0);
            found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("Given a part outside the multi, no multi selected or a missing parameter value, When a part parameter is read or set, Then nothing is sent and the problem says why [RQ-MCP-021, RQ-MCP-009]",
          "[mcp][multi][gateway]")
{
    Rig rig;
    const ParameterDefinition& level = named("part level");
    const auto none = rig.gateway.readMultiParameter(level, PartSelection::of(1));
    CHECK_FALSE(none.ok());
    CHECK(contains(none.problem, "Is a multi selected? Use select_multi first."));

    REQUIRE(rig.gateway.selectMultiByName("LIVE").ok());
    for (const int part : {0, PARTS + 1, -3})
    {
        const auto read = rig.gateway.readMultiParameter(level, PartSelection::of(part));
        const auto written = rig.gateway.writeMultiParameter(level, 5, PartSelection::of(part));
        for (const auto& outcome : {read, written})
        {
            CHECK_FALSE(outcome.ok());
            CHECK(contains(outcome.problem, "parts 1 to 32"));
        }
    }
    CHECK(rig.accepted(akm::ItemId::MultiSetLevel) == 0);
}

TEST_CASE("Given every row of the multi catalogue, When it is set to another value on part 2 of the simulated sampler, Then it reads back and putting the old one back restores it [RQ-MCP-021, RQ-MCP-006]",
          "[mcp][multi][gateway]")
{
    Rig rig;
    REQUIRE(rig.gateway.selectMultiByName("STUDIO").ok());
    for (const ParameterDefinition& parameter : ParameterCatalogue::multis().parameters())
    {
        INFO("parameter: " << parameter.name);
        const auto before = rig.gateway.readMultiParameter(parameter, PartSelection::of(2));
        REQUIRE(before.ok());
        const std::int64_t old = before.value->front().value;
        const std::int64_t step = parameter.kind == ParameterKind::Number ? parameter.step : 1;
        // The simulated sampler starts a part with zeros, outside some ranges (a note of 0): the lowest value then.
        const std::int64_t other = (old < parameter.min || old > parameter.max) ? parameter.min
                                   : (old + step <= parameter.max ? old + step : old - step);
        const auto set = rig.gateway.writeMultiParameter(parameter, other, PartSelection::of(2));
        INFO("set " << other << " (was " << old << "): " << set.problem);
        REQUIRE(set.ok());
        CHECK(set.value->front().value == other);
        if (old >= parameter.min && old <= parameter.max)
        {
            const auto restored = rig.gateway.writeMultiParameter(parameter, old, PartSelection::of(2));
            REQUIRE(restored.ok());
            CHECK(restored.value->front().value == old);
        }
    }
}

TEST_CASE("Given two multis, When list_multis and select_multi run, Then the answers list the multis, mark the current one with its part count, and refuse a missing multi [RQ-MCP-021]",
          "[mcp][multi][tools]")
{
    Rig rig;
    const std::string before = textOf(rig.call("list_multis"));
    CHECK(contains(before, "Multis in memory (2):"));
    CHECK(contains(before, "0: LIVE"));
    CHECK(contains(before, "1: STUDIO"));
    CHECK(contains(before, "No multi is selected"));

    const json selected = rig.call("select_multi", {{"name", "STUDIO"}});
    CHECK_FALSE(isError(selected));
    CHECK(contains(textOf(selected), "Selected the multi \"STUDIO\" (position 1, 32 parts)"));
    const std::string after = textOf(rig.call("list_multis"));
    CHECK(contains(after, "1: STUDIO (current, 32 parts)"));

    CHECK(isError(rig.call("select_multi", {{"name", "NOPE"}})));
    CHECK(isError(rig.call("select_multi", {{"index", 7}})));
    CHECK(isError(rig.call("select_multi")));
    CHECK(isError(rig.call("select_multi", {{"name", "LIVE"}, {"index", 0}})));

    Rig empty(false);
    CHECK(contains(textOf(empty.call("list_multis")), "The sampler holds no multi."));
}

TEST_CASE("Given a selected multi, When set_multi_parameter and get_multi_parameters run, Then the answers name the part, a group reads its parameters and a part is required to set [RQ-MCP-021]",
          "[mcp][multi][tools]")
{
    Rig rig;
    REQUIRE_FALSE(isError(rig.call("select_multi", {{"name", "LIVE"}})));

    const json set = rig.call("set_multi_parameter", {{"parameter", "part level"}, {"value", 80}, {"part", 3}});
    CHECK_FALSE(isError(set));
    CHECK(contains(textOf(set), "part level = 80 (part 3)"));
    CHECK(contains(textOf(rig.call("set_multi_parameter", {{"parameter", "part pan"}, {"value", -20}, {"part", 3}})),
                   "part pan = -20 (part 3)"));
    CHECK(contains(textOf(rig.call("set_multi_parameter", {{"parameter", "part midi channel"}, {"value", "10B"}, {"part", 3}})),
                   "part midi channel = 10B (code 25) (part 3)"));
    CHECK(contains(textOf(rig.call("set_multi_parameter", {{"parameter", "part mute"}, {"value", true}, {"part", "all"}})),
                   "part mute = on (all 32 parts)"));

    const std::string mix = textOf(rig.call("get_multi_parameters", {{"group", "mix"}, {"part", 3}}));
    CHECK(contains(mix, "part level = 80 (part 3)"));
    CHECK(contains(mix, "part mute = on (part 3)"));
    CHECK(contains(mix, "part pan = -20 (part 3)"));

    const std::string levels = textOf(rig.call("get_multi_parameters", {{"parameters", json::array({"part level"})}}));
    CHECK(contains(levels, "part level:"));
    CHECK(contains(levels, "part 3 = 80"));
    CHECK(contains(levels, "part 1 = 0"));

    CHECK(isError(rig.call("set_multi_parameter", {{"parameter", "part level"}, {"value", 80}})));
    CHECK(isError(rig.call("set_multi_parameter", {{"parameter", "part level"}, {"value", 80}, {"part", 0}})));
    CHECK(isError(rig.call("set_multi_parameter", {{"parameter", "part level"}, {"value", 80}, {"part", 33}})));
    CHECK(isError(rig.call("set_multi_parameter", {{"parameter", "part level"}, {"value", 101}, {"part", 1}})));
    CHECK(isError(rig.call("set_multi_parameter", {{"parameter", "filter cutoff"}, {"value", 10}, {"part", 1}})));
    CHECK(isError(rig.call("get_multi_parameters", {{"group", "nonsense"}})));
    CHECK(isError(rig.call("get_multi_parameters")));
}

TEST_CASE("Given no multi selected, When a part parameter is read, Then the answer says to select a multi [RQ-MCP-021, RQ-MCP-009]",
          "[mcp][multi][tools]")
{
    Rig rig;
    const json answer = rig.call("get_multi_parameters", {{"parameters", json::array({"part level"})}, {"part", 1}});
    CHECK(isError(answer));
    CHECK(contains(textOf(answer), "select_multi"));
}

TEST_CASE("Given list_parameters with the domain multi, When it is called, Then it lists the part parameters [RQ-MCP-021]",
          "[mcp][multi][tools]")
{
    Rig rig;
    const std::string multis = textOf(rig.call("list_parameters", {{"domain", "multi"}}));
    CHECK(contains(multis, "part level"));
    CHECK(contains(multis, "part midi channel"));
    CHECK_FALSE(contains(multis, "filter cutoff"));
    const json unknown = rig.call("list_parameters", {{"domain", "nonsense"}});
    CHECK(isError(unknown));
    CHECK(contains(textOf(unknown), "multi"));
}

TEST_CASE("Given the multi tools, When the tools are listed, Then none creates, deletes, renames or assigns a multi, the multi tools carry their tiers and Delete ALL Multis is never sent [RQ-MCP-021, RQ-MCP-013, RQ-MCP-014]",
          "[mcp][multi][tools]")
{
    Rig rig;
    const json message{{"jsonrpc", "2.0"},
                       {"id", 1},
                       {"method", "tools/list"},
                       {"params",
                        {{"_meta",
                          {{"io.modelcontextprotocol/protocolVersion", MODERN},
                           {"io.modelcontextprotocol/clientCapabilities", json::object()}}}}}};
    const json list = json::parse(*rig.server.handleLine(message.dump()));
    std::set<std::string> names;
    for (const json& tool : list["result"]["tools"])
        names.insert(tool["name"].get<std::string>());
    for (const char* tool : {"list_multis", "select_multi", "get_multi_parameters", "set_multi_parameter"})
        CHECK(names.count(tool) == 1);
    for (const std::string& name : names)
    {
        CHECK(name.find("create_multi") == std::string::npos);
        CHECK(name.find("delete_multi") == std::string::npos);
        CHECK(name.find("rename_multi") == std::string::npos);
        CHECK(name.find("multi_part_program") == std::string::npos);
    }
    for (const json& tool : list["result"]["tools"])
    {
        const std::string name = tool["name"].get<std::string>();
        if (name == "list_multis")
            CHECK(tool["annotations"]["readOnlyHint"].get<bool>());
        if (name == "set_multi_parameter")
        {
            CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
            CHECK_FALSE(tool["annotations"]["destructiveHint"].get<bool>());
            CHECK(contains(tool["description"].get<std::string>(), "memory, not on disk"));
        }
    }
    CHECK(rig.accepted(akm::ItemId::MultiDeleteAll) == 0);
    CHECK(rig.accepted(akm::ItemId::MultiDeleteCurrent) == 0);
    CHECK(rig.accepted(akm::ItemId::MultiCreate) == 0);
}
