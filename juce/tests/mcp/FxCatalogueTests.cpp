/*
 * XS56K - Editor for AKAI S5000/S6000 samplers
 * Copyright (C) 2026 xplorer2716
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// The catalogue of the effects modules and their parameters, as Tables 24 and 25 of the specification give them. [TASK-MCP-052, RQ-MCP-053,
// ADR-MCP-005 (DEC-MCP-033)]
#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

#include "mcp/FxCatalogue.hpp"

using namespace mcp;

namespace
{
    constexpr int KIND_COUNT = 17;  // the codes 0x00 to 0x10 of Table 24
    constexpr int CODE_CHORUS = 0x02;
    constexpr int CODE_EQ = 0x09;
    constexpr int CODE_ROTARY = 0x05;
}

TEST_CASE("Given Table 24, When the kinds are listed, Then there are 17 in code order from 0x00 to 0x10 with unique names [RQ-MCP-053]", "[mcp][fx][fxcatalogue]")
{
    const auto& kinds = fxModuleKinds();
    REQUIRE(kinds.size() == KIND_COUNT);
    std::set<std::string> names;
    for (int code = 0; code < KIND_COUNT; ++code)
    {
        CHECK(kinds[static_cast<std::size_t>(code)].code == code);
        names.insert(kinds[static_cast<std::size_t>(code)].name);
    }
    CHECK(names.size() == KIND_COUNT);
    CHECK(fxKindByCode(0)->parameters.empty());
}

TEST_CASE("Given Table 25, When the parameters of each kind are counted, Then each kind has the parameters the table gives, indexed from 0 with a range that is not empty [RQ-MCP-053]",
          "[mcp][fx][fxcatalogue]")
{
    const std::pair<int, std::size_t> expected[] = {{0x00, 0}, {0x01, 4}, {0x02, 3}, {0x03, 3}, {0x04, 3}, {0x05, 8}, {0x06, 6}, {0x07, 4}, {0x08, 8},
                                                    {0x09, 14}, {0x0A, 5}, {0x0B, 5}, {0x0C, 5}, {0x0D, 7}, {0x0E, 9}, {0x0F, 7}, {0x10, 1}};
    for (const auto& [code, count] : expected)
    {
        const FxModuleKind* kind = fxKindByCode(code);
        INFO(code);
        REQUIRE(kind != nullptr);
        CHECK(kind->parameters.size() == count);
        std::set<std::string> names;
        for (std::size_t i = 0; i < kind->parameters.size(); ++i)
        {
            CHECK(kind->parameters[i].index == static_cast<int>(i));
            CHECK(kind->parameters[i].minimum <= kind->parameters[i].maximum);
            names.insert(kind->parameters[i].name);
        }
        CHECK(names.size() == count);
    }
}

TEST_CASE("Given a code or a name, When a kind is looked for, Then it is found by its code or by its name in other letters or with a space for the underscore; an unknown one is null [RQ-MCP-053]",
          "[mcp][fx][fxcatalogue]")
{
    CHECK(fxKindByCode(CODE_CHORUS)->name == std::string("chorus"));
    CHECK(fxKindByCode(KIND_COUNT) == nullptr);
    CHECK(fxKindByCode(-1) == nullptr);
    CHECK(fxKindByName("Chorus")->code == CODE_CHORUS);
    CHECK(fxKindByName("rotary speaker")->code == CODE_ROTARY);
    CHECK(fxKindByName("rotary_speaker")->code == CODE_ROTARY);
    CHECK(fxKindByName("ROTARY-SPEAKER")->code == CODE_ROTARY);
    CHECK(fxKindByName("wobble") == nullptr);
    CHECK(fxKindByName("") == nullptr);
}

TEST_CASE("Given a kind, When a parameter is looked for by name or by index, Then it is found either way; an unknown name or index is null [RQ-MCP-053]",
          "[mcp][fx][fxcatalogue]")
{
    const FxModuleKind& eq = *fxKindByCode(CODE_EQ);
    CHECK(fxParameterOf(eq, "low_gain")->index == 1);
    CHECK(fxParameterOf(eq, "Low Gain")->index == 1);
    CHECK(fxParameterOf(eq, "13")->name == std::string("high_mid_sweep_depth"));
    CHECK(fxParameterOf(eq, "14") == nullptr);
    CHECK(fxParameterOf(eq, "nope") == nullptr);
    CHECK(fxParameterOf(eq, "-1") == nullptr);
}

TEST_CASE("Given the EB20 of Figure 2, When the kinds a module may take are asked for, Then only modules 2 and 3 of channels 0 and 1 have any [RQ-MCP-053]",
          "[mcp][fx][fxcatalogue]")
{
    CHECK(fxKindNames(eb20KindsFor(0, 2)) == "chorus, flange, phase, rotary_speaker, fmod_autopan, pitch_shift, pitch_shift_feedback");
    CHECK(fxKindNames(eb20KindsFor(1, 3)) == "mono_delay, mono_left_right, mono_crossover, stereo_delay");
    for (const int channel : {0, 1})
    {
        for (const int module : {0, 1, 4, 5, 6})
            CHECK(eb20KindsFor(channel, module).empty());
    }
    for (const int channel : {2, 3, -1})
    {
        for (const int module : {0, 1, 2, 3})
            CHECK(eb20KindsFor(channel, module).empty());
    }
}

TEST_CASE("Given a parameter, When its range is written, Then it reads as the table does: with its meaning when it has one [RQ-MCP-053]",
          "[mcp][fx][fxcatalogue]")
{
    const FxModuleKind& chorus = *fxKindByCode(CODE_CHORUS);
    CHECK(fxRangeText(chorus.parameters[0]) == "0 to 99 = 0.0 to 9.9");
    CHECK(fxRangeText(chorus.parameters[1]) == "0 to 100");
    CHECK(fxRangeText(chorus.parameters[2]) == "-50 to 50");
    CHECK(fxRangeText(fxKindByCode(0x01)->parameters[2]) == "1 to 5000 (hertz)");
    CHECK(fxParameterNames(chorus) == "rate, depth, feedback");
}
