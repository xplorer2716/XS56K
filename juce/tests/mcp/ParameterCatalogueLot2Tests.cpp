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

// Lot 2 of the parameter catalogue: the rest of the filter, the amplitude envelope, the filter envelope and the two
// LFOs, as rows of the same table. [TASK-MCP-008, RQ-MCP-010, ADR-MCP-001 (DEC-MCP-005)]
#include <catch2/catch_test_macros.hpp>

#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "akm/ItemCatalogue.hpp"
#include "mcp/ParameterCatalogue.hpp"

using mcp::ParameterCatalogue;
using mcp::ParameterDefinition;
using mcp::ParameterKind;
using mcp::ParameterScope;

namespace
{
    const ParameterDefinition& named(const char* name)
    {
        const auto resolution = ParameterCatalogue::standard().resolveName(name);
        REQUIRE(resolution.parameter != nullptr);
        return *resolution.parameter;
    }

    // The spec's items of sections 08 and 0A that belong to the four groups, by the words of their names: the filter
    // (its type, cutoff, resonance, tracking, attenuation and its modulation inputs), the amplitude envelope, the filter
    // envelope and the LFOs. The amplitude modulation inputs ("Amp Mod"), the aux envelope and the other groups of the
    // two sections are not among them.
    bool belongsToTheFourGroups(const akm::ItemDescriptor& item)
    {
        constexpr std::uint8_t SECTION_KEYGROUP = 0x08;
        constexpr std::uint8_t SECTION_PROGRAM = 0x0A;
        if (item.section != SECTION_KEYGROUP && item.section != SECTION_PROGRAM)
            return false;
        const std::string name(item.name);
        for (const char* word : {"Filter", "Amplitude Envelope", "LFO"})
        {
            if (name.find(word) != std::string::npos)
                return true;
        }
        return false;
    }

    // Items of the four groups that the catalogue leaves out, each with its reason. None: lot 2 holds them all.
    const std::vector<std::pair<akm::ItemId, const char*>>& leftOut()
    {
        static const std::vector<std::pair<akm::ItemId, const char*>> items{};
        return items;
    }
}

TEST_CASE("Given the lot 2 catalogue, When it is compared with the AKM item catalogue, Then no filter, amplitude envelope, filter envelope or LFO item of sections 08 and 0A is unaccounted for, or each one left out is listed with its reason [RQ-MCP-010]",
          "[mcp][catalogue][lot2]")
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
        excused.insert(static_cast<std::size_t>(item));
    }

    std::size_t belonging = 0;
    for (std::size_t index = 0; index < std::size(akm::ITEM_TABLE); ++index)
    {
        const akm::ItemDescriptor& item = akm::ITEM_TABLE[index];
        if (!belongsToTheFourGroups(item))
            continue;
        ++belonging;
        INFO("unaccounted item: " << item.name << " (section " << int(item.section) << ", item " << int(item.item) << ")");
        CHECK((covered.count(index) == 1 || excused.count(index) == 1));
    }
    CHECK(belonging > 0);
}

TEST_CASE("Given the lot 2 catalogue, When it is counted, Then it holds 54 parameters: filter 11, amplitude envelope 8, filter envelope 9, LFO 1 13 and LFO 2 13, and every lot 1 name is still there [RQ-MCP-010]",
          "[mcp][catalogue][lot2]")
{
    const auto& catalogue = ParameterCatalogue::standard();

    CHECK(catalogue.parameters().size() >= 54);  // lot 3 adds to the catalogue, see ParameterCatalogueLot3Tests
    for (const auto& [group, expected] : std::vector<std::pair<const char*, std::size_t>>{
             {"filter", 11}, {"amplitude envelope", 8}, {"filter envelope", 9}, {"lfo 1", 13}, {"lfo 2", 13}})
    {
        CAPTURE(group);
        const auto groups = catalogue.findGroups(group);
        REQUIRE(groups.size() == 1);
        CHECK(catalogue.parametersInGroup(*groups.front()).size() == expected);
    }
    for (const char* lot1 : {"filter type", "filter cutoff", "filter resonance", "filter keyboard tracking", "filter attenuation",
                             "amplitude envelope attack", "amplitude envelope release", "filter envelope depth", "lfo 1 rate",
                             "lfo 1 sync", "lfo 2 waveform", "lfo 2 retrigger"})
        CHECK(catalogue.resolveName(lot1).parameter != nullptr);
}

TEST_CASE("Given the modulation sources of Table 15, When their labels are listed, Then there are 15 in code order from NO SOURCE to the second EXTERNAL, and a source is said by label, alias or code [RQ-MCP-010, RQ-MCP-006]",
          "[mcp][catalogue][lot2]")
{
    const ParameterDefinition& source = named("lfo 1 rate modulation source");

    REQUIRE(source.kind == ParameterKind::Choice);
    REQUIRE(source.labels.size() == 15);
    CHECK(source.labels[0] == "NO SOURCE");
    CHECK(source.labels[1] == "MODWHEEL");
    CHECK(source.labels[7] == "LFO1");
    CHECK(source.labels[8] == "LFO2");
    CHECK(source.labels[11] == "AUX ENV");
    CHECK(source.labels[14] == "EXTERNAL 2");
    CHECK(source.scope == ParameterScope::Program);

    CHECK(mcp::resolveValue(source, "velocity").value == 5);
    CHECK(mcp::resolveValue(source, "Mod Wheel").value == 1);
    CHECK(mcp::resolveValue(source, "pitch bend").value == 2);
    CHECK(mcp::resolveValue(source, "lfo 2").value == 8);
    CHECK(mcp::resolveValue(source, "filter envelope").value == 10);
    CHECK(mcp::resolveValue(source, "amp envelope").value == 9);
    CHECK(mcp::resolveValue(source, "none").value == 0);
    CHECK(mcp::resolveValue(source, "MODWHEEL 2").value == 12);
    CHECK(mcp::resolveValue(source, std::int64_t{14}).value == 14);
    CHECK_FALSE(mcp::resolveValue(source, std::int64_t{15}).value.has_value());
    CHECK(source.description.find("12 to 14") != std::string::npos);
}

TEST_CASE("Given the filter's three modulation inputs, When their sources and amounts are looked up, Then the source is a program choice, the amount a signed keygroup value and the input number leads both items [RQ-MCP-010]",
          "[mcp][catalogue][lot2]")
{
    for (const std::int64_t input : {1, 2, 3})
    {
        const std::string which = std::to_string(input);
        const ParameterDefinition& source = named(("filter modulation " + which + " source").c_str());
        const ParameterDefinition& amount = named(("filter modulation " + which + " amount").c_str());

        CAPTURE(input);
        CHECK(source.scope == ParameterScope::Program);
        CHECK(source.leadingArguments == std::vector<std::int64_t>{input});
        CHECK(source.group == "filter");
        CHECK(amount.scope == ParameterScope::Keygroup);
        CHECK(amount.kind == ParameterKind::Signed);
        CHECK(amount.min == -100);
        CHECK(amount.leadingArguments == std::vector<std::int64_t>{input});
    }
    CHECK(&named("filter mod 2 amount") == &named("filter modulation 2 amount"));
}

TEST_CASE("Given the envelope extras, When looked up, Then each is a signed keygroup value of its envelope's group [RQ-MCP-010]",
          "[mcp][catalogue][lot2]")
{
    for (const char* envelope : {"amplitude envelope", "filter envelope"})
    {
        for (const char* extra : {"velocity to attack", "on-velocity to release", "off-velocity to release", "key scale"})
        {
            const std::string name = std::string(envelope) + " " + extra;
            CAPTURE(name);
            const ParameterDefinition& parameter = named(name.c_str());
            CHECK(parameter.kind == ParameterKind::Signed);
            CHECK(parameter.scope == ParameterScope::Keygroup);
            CHECK(parameter.group == envelope);
            CHECK(parameter.min == -100);
            CHECK(parameter.max == 100);
        }
    }
}

TEST_CASE("Given the LFO extras, When looked up, Then each LFO has a source and an amount for its rate, delay and depth, LFO 1 a modwheel and an aftertouch level, and LFO 2 a MIDI clock sync and its division [RQ-MCP-010]",
          "[mcp][catalogue][lot2]")
{
    for (const std::int64_t lfo : {1, 2})
    {
        for (const char* what : {"rate", "delay", "depth"})
        {
            const std::string prefix = "lfo " + std::to_string(lfo) + " " + what + " modulation ";
            CAPTURE(prefix);
            const ParameterDefinition& source = named((prefix + "source").c_str());
            const ParameterDefinition& amount = named((prefix + "amount").c_str());
            CHECK(source.kind == ParameterKind::Choice);
            CHECK(source.leadingArguments == std::vector<std::int64_t>{lfo});
            CHECK(amount.kind == ParameterKind::Signed);
            CHECK(amount.leadingArguments == std::vector<std::int64_t>{lfo});
        }
    }
    CHECK(named("lfo 1 modwheel").kind == ParameterKind::Number);
    CHECK(named("lfo 1 modwheel").max == 100);
    CHECK(named("lfo 1 aftertouch").leadingArguments == std::vector<std::int64_t>{1});
    CHECK(named("lfo 2 clock sync").kind == ParameterKind::Switch);
    CHECK(named("lfo 2 clock sync").leadingArguments == std::vector<std::int64_t>{2});
}

TEST_CASE("Given the LFO 2 clock division, When its labels are listed, Then there are 69 from 8 cycles per beat to 64 beats per cycle, and \"4 beats per cycle\" is code 8 [RQ-MCP-010]",
          "[mcp][catalogue][lot2]")
{
    const ParameterDefinition& division = named("lfo 2 clock division");

    REQUIRE(division.kind == ParameterKind::Choice);
    REQUIRE(division.labels.size() == 69);
    CHECK(division.labels[0] == "8 cycles per beat");
    CHECK(division.labels[3] == "3 cycles per beat");
    CHECK(division.labels[5] == "1 cycle per beat");
    CHECK(division.labels[6] == "2 beats per cycle");
    CHECK(division.labels[7] == "3 beats per cycle");
    CHECK(division.labels[68] == "64 beats per cycle");
    CHECK(mcp::resolveValue(division, "4 beats per cycle").value == 8);
    CHECK(mcp::resolveValue(division, "1 cycle per beat").value == 5);
    CHECK(mcp::resolveValue(division, "64 beats per cycle").value == 68);
    CHECK_FALSE(mcp::resolveValue(division, "5 beats per cycle plus").value.has_value());
}
