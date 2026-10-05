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

// Lot 3 of the parameter catalogue: the rest of the keygroup (general options, pitch and amplitude, the auxiliary
// envelope) and of the program (output, tuning, pitch bend and the keygroup modulation sources), as rows of the same
// table, and a read-back of every row on the simulated sampler. [TASK-MCP-013, RQ-MCP-018, ADR-MCP-002 (DEC-MCP-012)]
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "SimulatedPrograms.hpp"
#include "akm/ItemCatalogue.hpp"
#include "akm/RealScheduler.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"
#include "mcp/ParameterCatalogue.hpp"
#include "mcp/SamplerGateway.hpp"

using mcp::KeygroupSelection;
using mcp::ParameterCatalogue;
using mcp::ParameterDefinition;
using mcp::ParameterKind;
using mcp::ParameterScope;

namespace
{
    constexpr std::chrono::milliseconds COMMAND_TIMEOUT{300};

    const ParameterDefinition& named(const char* name)
    {
        const auto resolution = ParameterCatalogue::standard().resolveName(name);
        INFO("parameter: " << name);
        REQUIRE(resolution.parameter != nullptr);
        return *resolution.parameter;
    }

    // The items of sections 08 and 0A that are not a musical parameter of a program or a keygroup, each with its reason:
    // the selection and the lifecycle of programs and keygroups (the structure tools and the gateway own them), the
    // information about programs, and the two items whose value is not one parameter.
    const std::vector<std::pair<akm::ItemId, const char*>>& leftOut()
    {
        static const std::vector<std::pair<akm::ItemId, const char*>> items{
            {akm::ItemId::KeygroupSelect, "selects the keygroup an edit acts on: done by the gateway for every keygroup parameter"},
            {akm::ItemId::KeygroupGetCurrent, "reports the selected keygroup: not a parameter"},
            {akm::ItemId::ProgramCreate, "creates a program: the create_program tool"},
            {akm::ItemId::ProgramCreateWithKeygroups, "creates a program: the create_program tool"},
            {akm::ItemId::ProgramSelectByName, "selects a program: the select_program tool"},
            {akm::ItemId::ProgramSelectByIndex, "selects a program: the select_program tool"},
            {akm::ItemId::ProgramDeleteCurrent, "deletes a program: the delete_program tool"},
            {akm::ItemId::ProgramRenameCurrent, "renames a program: the rename_program tool"},
            {akm::ItemId::ProgramDeleteAll, "deletes every program: never offered (RQ-MCP-014)"},
            {akm::ItemId::ProgramAddKeygroups, "adds keygroups to a program: not offered, a keygroup is not a parameter"},
            {akm::ItemId::ProgramDeleteKeygroup, "deletes a keygroup: not offered"},
            {akm::ItemId::ProgramGetCount, "counts the programs: get_status"},
            {akm::ItemId::ProgramGetCurrentName, "names the current program: get_status"},
            {akm::ItemId::ProgramGetKeygroupCount, "counts the keygroups: get_status"},
            {akm::ItemId::ProgramGetIndex, "the position of the current program: not a parameter"},
            {akm::ItemId::ProgramGetAllNumbers, "the front-panel numbers of every program: not a parameter"},
            {akm::ItemId::ProgramGetAllNames, "the names of every program: list_programs"},
            {akm::ItemId::ProgramSetNumber, "the program number takes an enable flag and a number in one item: not one parameter"},
            {akm::ItemId::ProgramGetNumber, "the program number takes an enable flag and a number in one item: not one parameter"},
            {akm::ItemId::ProgramSetUserTuneTemplate, "sets all twelve notes at once: a per-note edit would overwrite the others"},
            {akm::ItemId::ProgramGetUserTuneTemplate, "reads all twelve notes at once: a per-note edit would overwrite the others"},
        };
        return items;
    }

    bool inSectionsKeygroupAndProgram(const akm::ItemDescriptor& item)
    {
        constexpr std::uint8_t SECTION_KEYGROUP = 0x08;
        constexpr std::uint8_t SECTION_PROGRAM = 0x0A;
        return item.section == SECTION_KEYGROUP || item.section == SECTION_PROGRAM;
    }

    struct Rig
    {
        Rig()
        {
            backend.addSampler();
            mcp::test::seedThreePrograms(backend);
        }

        mcp::GatewayConfig config()
        {
            mcp::GatewayConfig made;
            made.inputPort = backend.inputName();
            made.outputPort = backend.outputName();
            made.commandTimeout = COMMAND_TIMEOUT;
            return made;
        }

        akm::RealScheduler scheduler;
        akm::harness::SimulatedMidiBackend backend{scheduler};
        mcp::SamplerGateway gateway{backend, config()};
    };

    // A value of the parameter that is not the one it holds: the next step up, or down at the top.
    std::int64_t otherValue(const ParameterDefinition& parameter, std::int64_t current)
    {
        const std::int64_t step = parameter.kind == ParameterKind::Number ? parameter.step : 1;
        // The simulated sampler starts a new keygroup with 0 for a note, which is outside its own range: the lowest value then.
        if (current < parameter.min || current > parameter.max)
            return parameter.min;
        return current + step <= parameter.max ? current + step : current - step;
    }
}

TEST_CASE("Given the lot 3 catalogue, When it is counted, Then it holds 106 parameters and the six new groups have 7, 9, 12, 12, 4 and 8 [RQ-MCP-018]",
          "[mcp][catalogue][lot3]")
{
    const ParameterCatalogue& catalogue = ParameterCatalogue::standard();
    CHECK(catalogue.parameters().size() == 106);
    CHECK(catalogue.groups().size() == 11);
    const std::vector<std::pair<const char*, std::size_t>> expected{{"keygroup", 7},   {"pitch and amplitude", 9}, {"aux envelope", 12},
                                                                    {"output", 12},    {"tuning", 4},              {"pitch bend", 8},
                                                                    {"filter", 11},    {"amplitude envelope", 8},  {"filter envelope", 9}};
    for (const auto& [group, count] : expected)
    {
        INFO("group: " << group);
        const auto found = catalogue.findGroups(group);
        REQUIRE(found.size() == 1);
        CHECK(catalogue.parametersInGroup(*found.front()).size() == count);
    }
}

TEST_CASE("Given the lot 3 catalogue, When it is compared with the AKM item catalogue, Then every item of sections 08 and 0A is a row or is left out with its reason [RQ-MCP-018]",
          "[mcp][catalogue][lot3]")
{
    std::set<std::size_t> covered;
    for (const ParameterDefinition& parameter : ParameterCatalogue::standard().parameters())
    {
        covered.insert(static_cast<std::size_t>(parameter.setItem));
        covered.insert(static_cast<std::size_t>(parameter.getItem));
    }
    std::set<std::size_t> excused;
    for (const auto& [item, reason] : leftOut())
    {
        CHECK(std::string(reason).size() > 10);
        CHECK(covered.count(static_cast<std::size_t>(item)) == 0);
        excused.insert(static_cast<std::size_t>(item));
    }

    for (std::size_t index = 0; index < std::size(akm::ITEM_TABLE); ++index)
    {
        const akm::ItemDescriptor& item = akm::ITEM_TABLE[index];
        if (!inSectionsKeygroupAndProgram(item))
            continue;
        INFO("unaccounted item: " << item.name << " (section " << int(item.section) << ", item " << int(item.item) << ")");
        CHECK((covered.count(index) == 1 || excused.count(index) == 1));
    }
}

TEST_CASE("Given the keygroup level, When values are checked, Then it is -30 to 30 dB in steps of 6 and the sampler holds code 0 to 10 [RQ-MCP-018]",
          "[mcp][catalogue][lot3]")
{
    const ParameterDefinition& level = named("keygroup level");
    CHECK(level.kind == ParameterKind::Number);
    CHECK(level.scope == ParameterScope::Keygroup);
    CHECK(mcp::resolveValue(level, std::int64_t{-30}).value == -30);
    CHECK(mcp::resolveValue(level, std::int64_t{0}).value == 0);
    CHECK(mcp::resolveValue(level, std::int64_t{30}).value == 30);
    CHECK_FALSE(mcp::resolveValue(level, std::int64_t{4}).value.has_value());
    CHECK_FALSE(mcp::resolveValue(level, std::int64_t{36}).value.has_value());
    CHECK(mcp::resolveValue(level, "-12 dB").value == -12);
    CHECK(mcp::toItemValues(level, -30) == std::vector<std::int64_t>{0});
    CHECK(mcp::toItemValues(level, 0) == std::vector<std::int64_t>{5});
    CHECK(mcp::toItemValues(level, 30) == std::vector<std::int64_t>{10});
    const std::vector<std::int64_t> reply{3};
    CHECK(mcp::fromItemValues(level, reply) == -12);
    CHECK(mcp::describeRange(level).find("-30, -24, -18") == 0);
}

TEST_CASE("Given the keygroup's low and high notes, When values are checked, Then they are MIDI notes 21 to 127 sent as they are [RQ-MCP-018]",
          "[mcp][catalogue][lot3]")
{
    for (const char* name : {"keygroup low note", "keygroup high note"})
    {
        const ParameterDefinition& note = named(name);
        CHECK(mcp::resolveValue(note, std::int64_t{21}).value == 21);
        CHECK(mcp::resolveValue(note, std::int64_t{127}).value == 127);
        CHECK_FALSE(mcp::resolveValue(note, std::int64_t{20}).value.has_value());
        CHECK_FALSE(mcp::resolveValue(note, std::int64_t{128}).value.has_value());
        CHECK(mcp::toItemValues(note, 60) == std::vector<std::int64_t>{60});
        CHECK(note.description.find("A-1") != std::string::npos);
    }
}

TEST_CASE("Given the choices of lot 3, When their labels are read, Then they are the spec's, in code order, and a name resolves to its code [RQ-MCP-018]",
          "[mcp][catalogue][lot3]")
{
    const auto check = [](const char* parameter, const std::vector<std::string>& labels) {
        const ParameterDefinition& row = named(parameter);
        INFO("parameter: " << parameter);
        CHECK(row.kind == ParameterKind::Choice);
        CHECK(row.labels == labels);
        for (std::size_t code = 0; code < labels.size(); ++code)
            CHECK(mcp::resolveValue(row, labels[code]).value == static_cast<std::int64_t>(code));
    };
    check("keygroup fx override", {"OFF", "FX1", "FX2", "RV3", "RV4"});
    check("tune template", {"USER", "EVEN-TEMPERED", "ORCHESTRAL", "WERKMEISTER", "1/5 MEANTONE", "1/4 MEANTONE", "JUST", "ARABIAN"});
    check("tune key", {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"});
    check("pitch bend mode", {"NORMAL", "HELD"});
    check("portamento mode", {"TIME", "RATE"});
    CHECK(mcp::resolveValue(named("tune template"), "even tempered").value == 1);
}

TEST_CASE("Given the modulation sources of lot 3, When their rows are read, Then they are program values with the source labels and the amounts are signed keygroup or program values [RQ-MCP-018]",
          "[mcp][catalogue][lot3]")
{
    for (const char* source : {"pitch modulation 1 source", "pitch modulation 2 source", "keygroup amp modulation source",
                               "program amp modulation 1 source", "program amp modulation 2 source", "pan modulation 1 source",
                               "pan modulation 2 source", "pan modulation 3 source"})
    {
        const ParameterDefinition& row = named(source);
        CHECK(row.scope == ParameterScope::Program);
        CHECK(row.labels.size() == 15);
    }
    CHECK(named("pitch modulation 1 amount").scope == ParameterScope::Keygroup);
    CHECK(named("keygroup amp modulation amount").scope == ParameterScope::Keygroup);
    CHECK(named("program amp modulation 2 amount").scope == ParameterScope::Program);
    CHECK(named("pan modulation 3 amount").scope == ParameterScope::Program);
    CHECK(named("pan modulation 3 amount").kind == ParameterKind::Signed);
}

TEST_CASE("Given every row of the catalogue, When it is set to another value on the simulated sampler, Then it reads back that value and putting the old one back restores it [RQ-MCP-018, RQ-MCP-006]",
          "[mcp][catalogue][lot3]")
{
    Rig rig;
    REQUIRE(rig.gateway.selectProgramByName("BASS").ok());
    for (const ParameterDefinition& parameter : ParameterCatalogue::standard().parameters())
    {
        INFO("parameter: " << parameter.name);
        const auto before = rig.gateway.readParameter(parameter, KeygroupSelection::all());
        REQUIRE(before.ok());
        const std::int64_t old = before.value->front().value;
        const std::int64_t other = otherValue(parameter, old);

        const auto set = rig.gateway.writeParameter(parameter, other, KeygroupSelection::all());
        INFO("set " << other << " (was " << old << "): " << set.problem);
        REQUIRE(set.ok());
        for (const mcp::ParameterValue& value : *set.value)
            CHECK(value.value == other);

        // A value the sampler could not be given (the simulated note of 0) cannot be put back.
        if (old < parameter.min || old > parameter.max)
            continue;
        const auto restored = rig.gateway.writeParameter(parameter, old, KeygroupSelection::all());
        REQUIRE(restored.ok());
        for (const mcp::ParameterValue& value : *restored.value)
            CHECK(value.value == old);
    }
}
