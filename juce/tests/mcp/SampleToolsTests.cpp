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

// The sample tools of the MCP server: the catalogue of the sample's parameters (section 0E), the gateway's sample calls
// and the tools list_samples, select_sample, get_sample_parameters and set_sample_parameter, over a real session and
// the simulated sampler. [TASK-MCP-015, RQ-MCP-020, ADR-MCP-002 (DEC-MCP-012, DEC-MCP-013)]
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

namespace
{
    constexpr std::chrono::milliseconds COMMAND_TIMEOUT{300};
    constexpr const char* MODERN = "2026-07-28";
    constexpr std::int64_t MAX_POSITION = 268435455;  // four 7-bit bytes

    const ParameterDefinition& named(const char* name)
    {
        const auto resolution = ParameterCatalogue::samples().resolveName(name);
        INFO("sample parameter: " << name);
        REQUIRE(resolution.parameter != nullptr);
        return *resolution.parameter;
    }

    /// A simulated sampler holding the samples KICK, SNARE and PAD, with the attributes of a real one for KICK. With
    /// `seeded` false the sampler holds none.
    struct Rig
    {
        explicit Rig(bool seeded = true)
        {
            sampler = &backend.addSampler();
            mcp::test::seedThreePrograms(backend);
            if (seeded)
            {
                sampler->setSampleNames({"KICK", "SNARE", "PAD"});
                sampler->setSampleAttributes(0, 0, 2, 143169, 22050);
            }
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

TEST_CASE("Given the sample catalogue, When it is read, Then it holds the 8 settable parameters of section 0E and the 4 read-only ones, in three groups [RQ-MCP-020]",
          "[mcp][sample][catalogue]")
{
    const ParameterCatalogue& catalogue = ParameterCatalogue::samples();
    CHECK(catalogue.parameters().size() == 12);
    CHECK(catalogue.groups().size() == 3);
    for (const ParameterDefinition& parameter : catalogue.parameters())
        CHECK(parameter.scope == ParameterScope::Sample);

    std::size_t readOnly = 0;
    for (const ParameterDefinition& parameter : catalogue.parameters())
        readOnly += parameter.readOnly ? 1 : 0;
    CHECK(readOnly == 4);
    for (const char* name : {"sample type", "sample channels", "sample length", "sample rate"})
        CHECK(named(name).readOnly);
    for (const char* name : {"sample start position", "sample end position", "sample original pitch", "sample semitone tune",
                             "sample fine tune", "sample playback mode", "sample loop start", "sample loop end"})
        CHECK_FALSE(named(name).readOnly);
}

TEST_CASE("Given the sample catalogue, When it is compared with the AKM item catalogue, Then every Set and Get item of section 0E that is a sample parameter is a row, and the rest are left out with their reason [RQ-MCP-020]",
          "[mcp][sample][catalogue]")
{
    // The selection, the lifecycle, the audition and the information about the samples in memory, and the two grouped
    // Gets: not parameters of the current sample.
    const std::set<std::size_t> leftOut{
        static_cast<std::size_t>(akm::ItemId::SampleSelectByName),   static_cast<std::size_t>(akm::ItemId::SampleSelectByIndex),
        static_cast<std::size_t>(akm::ItemId::SampleDeleteCurrent),  static_cast<std::size_t>(akm::ItemId::SampleRenameCurrent),
        static_cast<std::size_t>(akm::ItemId::SampleStartAudition),  static_cast<std::size_t>(akm::ItemId::SampleStopAudition),
        static_cast<std::size_t>(akm::ItemId::SampleGetCurrentIndex), static_cast<std::size_t>(akm::ItemId::SampleGetCurrentName),
        static_cast<std::size_t>(akm::ItemId::SampleDeleteAll),      static_cast<std::size_t>(akm::ItemId::SampleGetCount),
        static_cast<std::size_t>(akm::ItemId::SampleGetNameByIndex), static_cast<std::size_t>(akm::ItemId::SampleGetAllNames),
        static_cast<std::size_t>(akm::ItemId::SampleGetAllBasicParams), static_cast<std::size_t>(akm::ItemId::SampleGetAllSettableParams)};
    std::set<std::size_t> covered;
    for (const ParameterDefinition& parameter : ParameterCatalogue::samples().parameters())
    {
        covered.insert(static_cast<std::size_t>(parameter.getItem));
        if (!parameter.readOnly)
            covered.insert(static_cast<std::size_t>(parameter.setItem));
    }
    constexpr std::uint8_t SECTION_SAMPLE = 0x0E;
    std::size_t inSection = 0;
    for (std::size_t index = 0; index < std::size(akm::ITEM_TABLE); ++index)
    {
        const akm::ItemDescriptor& item = akm::ITEM_TABLE[index];
        if (item.section != SECTION_SAMPLE)
            continue;
        ++inSection;
        INFO("unaccounted item: " << item.name);
        CHECK((covered.count(index) == 1 || leftOut.count(index) == 1));
    }
    CHECK(inSection == 14 + 8 + 12);
}

TEST_CASE("Given a sample position, When it is converted, Then it is four 7-bit bytes, most significant first, up to 268435455 [RQ-MCP-020]",
          "[mcp][sample][catalogue]")
{
    // 143154 is the loop end of the real sample AMEN (OBSERVATIONS-RQ-AKM-051): 143154 = 8 * 16384 + 94 * 128 + 50.
    for (const char* name : {"sample start position", "sample end position", "sample loop start", "sample loop end"})
    {
        const ParameterDefinition& position = named(name);
        CHECK(position.kind == ParameterKind::Number);
        CHECK(mcp::toItemValues(position, 143154) == std::vector<std::int64_t>{0, 8, 94, 50});
        CHECK(mcp::toItemValues(position, 0) == std::vector<std::int64_t>{0, 0, 0, 0});
        CHECK(mcp::toItemValues(position, MAX_POSITION) == std::vector<std::int64_t>{127, 127, 127, 127});
        const std::vector<std::int64_t> reply{0, 8, 94, 50};
        CHECK(mcp::fromItemValues(position, reply) == 143154);
        const std::vector<std::int64_t> tooShort{0, 8, 94};
        CHECK_FALSE(mcp::fromItemValues(position, tooShort).has_value());
        CHECK(mcp::resolveValue(position, MAX_POSITION).value == MAX_POSITION);
        CHECK_FALSE(mcp::resolveValue(position, MAX_POSITION + 1).value.has_value());
        CHECK_FALSE(mcp::resolveValue(position, std::int64_t{-1}).value.has_value());
    }
}

TEST_CASE("Given the sample's pitch, tuning and playback mode, When their values are checked, Then the note is 21 to 127, the tunes are signed and the six playback modes are named [RQ-MCP-020]",
          "[mcp][sample][catalogue]")
{
    const ParameterDefinition& pitch = named("sample original pitch");
    CHECK(mcp::resolveValue(pitch, std::int64_t{21}).value == 21);
    CHECK_FALSE(mcp::resolveValue(pitch, std::int64_t{20}).value.has_value());
    CHECK(named("sample semitone tune").kind == ParameterKind::Signed);
    CHECK(named("sample fine tune").max == 50);

    const ParameterDefinition& mode = named("sample playback mode");
    REQUIRE(mode.labels.size() == 6);
    CHECK(mode.labels[0] == "NO LOOPING");
    CHECK(mode.labels[5] == "PLAY->RETRIG");
    CHECK(mcp::resolveValue(mode, "loop until rel").value == 3);
    CHECK_FALSE(mcp::resolveValue(mode, "as sample").value.has_value());  // the zone has it, the sample does not
}

TEST_CASE("Given an empty sampler, When the samples are listed, Then there is none and no sample is current [RQ-MCP-020]",
          "[mcp][sample][gateway]")
{
    Rig rig(false);
    const auto listing = rig.gateway.listSamples();
    REQUIRE(listing.ok());
    CHECK(listing.value->samples.empty());
    CHECK_FALSE(listing.value->current.has_value());
}

TEST_CASE("Given three samples, When they are listed and one is selected by name and another by index, Then the names come in order with positions and the selection is reported [RQ-MCP-020]",
          "[mcp][sample][gateway]")
{
    Rig rig;
    const auto listing = rig.gateway.listSamples();
    REQUIRE(listing.ok());
    REQUIRE(listing.value->samples.size() == 3);
    CHECK(listing.value->samples[0].name == "KICK");
    CHECK(listing.value->samples[2].index == 2);
    CHECK(listing.value->samples[2].name == "PAD");
    CHECK_FALSE(listing.value->current.has_value());

    const auto snare = rig.gateway.selectSampleByName("SNARE");
    REQUIRE(snare.ok());
    CHECK(snare.value->name == "SNARE");
    CHECK(snare.value->index == 1);
    const auto pad = rig.gateway.selectSampleByIndex(2);
    REQUIRE(pad.ok());
    CHECK(pad.value->name == "PAD");
    CHECK(rig.gateway.listSamples().value->current == 2);

    const auto missing = rig.gateway.selectSampleByName("NOPE");
    CHECK_FALSE(missing.ok());
    CHECK(contains(missing.problem, "No sample is named \"NOPE\""));
    const auto far = rig.gateway.selectSampleByIndex(9);
    CHECK_FALSE(far.ok());
    CHECK(contains(far.problem, "No sample is at index 9"));
}

TEST_CASE("Given a selected sample, When its parameters are set and read, Then each reads back, and the read-only ones read what the sampler holds [RQ-MCP-020]",
          "[mcp][sample][gateway]")
{
    Rig rig;
    REQUIRE(rig.gateway.selectSampleByName("KICK").ok());

    for (const auto& [name, value] : std::vector<std::pair<const char*, std::int64_t>>{{"sample start position", 50},
                                                                                         {"sample loop end", 1500},
                                                                                         {"sample loop start", 500},
                                                                                         {"sample end position", 2000},
                                                                                         {"sample original pitch", 60},
                                                                                         {"sample semitone tune", -5},
                                                                                         {"sample fine tune", 12},
                                                                                         {"sample playback mode", 3}})
    {
        INFO(name);
        const auto written = rig.gateway.writeSampleParameter(named(name), value);
        INFO(written.problem);
        REQUIRE(written.ok());
        CHECK(*written.value == value);
        CHECK(*rig.gateway.readSampleParameter(named(name)).value == value);
    }

    CHECK(*rig.gateway.readSampleParameter(named("sample type")).value == 0);
    CHECK(*rig.gateway.readSampleParameter(named("sample channels")).value == 2);
    CHECK(*rig.gateway.readSampleParameter(named("sample length")).value == 143169);
    CHECK(*rig.gateway.readSampleParameter(named("sample rate")).value == 22050);
}

TEST_CASE("Given no sample selected, or a read-only parameter, When a sample parameter is read or set, Then the problem says to select a sample, or that it is read-only, and nothing is set [RQ-MCP-020, RQ-MCP-009]",
          "[mcp][sample][gateway]")
{
    Rig rig;
    const auto read = rig.gateway.readSampleParameter(named("sample loop end"));
    CHECK_FALSE(read.ok());
    CHECK(contains(read.problem, "Is a sample selected? Use select_sample first."));

    REQUIRE(rig.gateway.selectSampleByName("KICK").ok());
    const auto length = rig.gateway.writeSampleParameter(named("sample length"), 5);
    CHECK_FALSE(length.ok());
    CHECK(contains(length.problem, "read-only"));
    CHECK(rig.accepted(akm::ItemId::SampleSetEndPosition) == 0);
}

TEST_CASE("Given three samples, When list_samples runs, Then the names are listed with their positions and the current one is marked; with none it says so [RQ-MCP-020]",
          "[mcp][sample][tools]")
{
    Rig rig;
    const std::string before = textOf(rig.call("list_samples"));
    CHECK(contains(before, "Samples in memory (3):"));
    CHECK(contains(before, "0: KICK"));
    CHECK(contains(before, "2: PAD"));
    CHECK(contains(before, "No sample is selected"));

    CHECK_FALSE(isError(rig.call("select_sample", {{"name", "SNARE"}})));
    const std::string after = textOf(rig.call("list_samples"));
    CHECK(contains(after, "1: SNARE (current)"));

    Rig empty(false);
    CHECK(contains(textOf(empty.call("list_samples")), "The sampler holds no sample."));
}

TEST_CASE("Given the sample tools, When select_sample is given a name, an index, neither, both or nothing found, Then the answer is the sample, or an error saying so [RQ-MCP-020, RQ-MCP-009]",
          "[mcp][sample][tools]")
{
    Rig rig;
    const json byName = rig.call("select_sample", {{"name", "KICK"}});
    CHECK_FALSE(isError(byName));
    CHECK(contains(textOf(byName), "Selected the sample \"KICK\""));
    const json byIndex = rig.call("select_sample", {{"index", 2}});
    CHECK_FALSE(isError(byIndex));
    CHECK(contains(textOf(byIndex), "PAD"));
    CHECK(isError(rig.call("select_sample", {{"name", "NOPE"}})));
    CHECK(isError(rig.call("select_sample", {{"index", 99}})));
    CHECK(isError(rig.call("select_sample")));
    CHECK(isError(rig.call("select_sample", {{"name", "KICK"}, {"index", 0}})));
}

TEST_CASE("Given a selected sample, When set_sample_parameter and get_sample_parameters run, Then the answers use the sample's vocabulary, a group reads all its parameters and a read-only one cannot be set [RQ-MCP-020]",
          "[mcp][sample][tools]")
{
    Rig rig;
    REQUIRE_FALSE(isError(rig.call("select_sample", {{"name", "KICK"}})));

    const json set = rig.call("set_sample_parameter", {{"parameter", "sample playback mode"}, {"value", "loop in rel"}});
    CHECK_FALSE(isError(set));
    CHECK(contains(textOf(set), "sample playback mode = LOOP IN REL (code 2)"));
    CHECK(contains(textOf(rig.call("set_sample_parameter", {{"parameter", "sample loop end"}, {"value", 1500}})),
                   "sample loop end = 1500"));
    CHECK(contains(textOf(rig.call("set_sample_parameter", {{"parameter", "sample semitone tune"}, {"value", -3}})),
                   "sample semitone tune = -3"));

    const std::string info = textOf(rig.call("get_sample_parameters", {{"group", "info"}}));
    CHECK(contains(info, "sample type = RAM (code 0)"));
    CHECK(contains(info, "sample channels = 2"));
    CHECK(contains(info, "sample length = 143169"));
    CHECK(contains(info, "sample rate = 22050 Hz"));

    const json refused = rig.call("set_sample_parameter", {{"parameter", "sample length"}, {"value", 10}});
    CHECK(isError(refused));
    CHECK(contains(textOf(refused), "read-only"));
    CHECK(isError(rig.call("set_sample_parameter", {{"parameter", "filter cutoff"}, {"value", 10}})));
    CHECK(isError(rig.call("set_sample_parameter", {{"parameter", "sample loop end"}, {"value", 268435456}})));
    CHECK(isError(rig.call("get_sample_parameters", {{"group", "nonsense"}})));
    CHECK(isError(rig.call("get_sample_parameters")));
}

TEST_CASE("Given no sample selected, When a sample parameter is read, Then the answer says to select a sample [RQ-MCP-020, RQ-MCP-009]",
          "[mcp][sample][tools]")
{
    Rig rig;
    const json answer = rig.call("get_sample_parameters", {{"parameters", json::array({"sample loop end"})}});
    CHECK(isError(answer));
    CHECK(contains(textOf(answer), "select_sample"));
}

TEST_CASE("Given list_parameters with a domain, When it is called, Then it lists the sample parameters, and an unknown domain is an error naming the domains [RQ-MCP-020]",
          "[mcp][sample][tools]")
{
    Rig rig;
    const std::string samples = textOf(rig.call("list_parameters", {{"domain", "sample"}}));
    CHECK(contains(samples, "sample loop end"));
    CHECK(contains(samples, "read-only"));
    CHECK_FALSE(contains(samples, "filter cutoff"));
    const std::string program = textOf(rig.call("list_parameters", {{"domain", "program"}}));
    CHECK(contains(program, "filter cutoff"));
    CHECK(contains(textOf(rig.call("list_parameters")), "filter cutoff"));
    const json unknown = rig.call("list_parameters", {{"domain", "nonsense"}});
    CHECK(isError(unknown));
    CHECK(contains(textOf(unknown), "sample"));
}

TEST_CASE("Given the sources of the sample tools, When the tools are listed, Then no tool creates or loads a sample from memory or deletes every sample, and the sample tools carry their tiers [RQ-MCP-020, RQ-MCP-013, RQ-MCP-036]",
          "[mcp][sample][tools]")
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
    for (const char* tool : {"list_samples", "select_sample", "get_sample_parameters", "set_sample_parameter"})
        CHECK(names.count(tool) == 1);
    for (const std::string& name : names)
    {
        // delete_sample and rename_sample (of the current sample) are offered since ADR-MCP-004 (DEC-MCP-024); deleting every sample is not.
        CHECK(name.find("delete_all_samples") == std::string::npos);
        CHECK(name.find("load_sample") == std::string::npos);
        CHECK(name.find("create_sample") == std::string::npos);
    }
    for (const json& tool : list["result"]["tools"])
    {
        const std::string name = tool["name"].get<std::string>();
        if (name == "list_samples")
            CHECK(tool["annotations"]["readOnlyHint"].get<bool>());
        if (name == "set_sample_parameter")
        {
            CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
            CHECK_FALSE(tool["annotations"]["destructiveHint"].get<bool>());
            CHECK(contains(tool["description"].get<std::string>(), "memory, not on disk"));
        }
    }
    CHECK(rig.accepted(akm::ItemId::SampleDeleteCurrent) == 0);
    CHECK(rig.accepted(akm::ItemId::SampleDeleteAll) == 0);
    CHECK(rig.accepted(akm::ItemId::SampleRenameCurrent) == 0);
}
