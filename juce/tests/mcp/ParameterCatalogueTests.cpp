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

// The parameter catalogue of the MCP server (lot 1): the rows, their consistency with the AKM item catalogue,
// the tolerant resolution of names, labels and numbers, and the conversion of a signed value to the sign and
// magnitude the sampler takes. [TASK-MCP-003, RQ-MCP-004, RQ-MCP-005, RQ-MCP-006, RQ-MCP-011,
// ADR-MCP-001 (DEC-MCP-005)]
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <string>
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

    bool contains(const std::string& text, const char* part)
    {
        return text.find(part) != std::string::npos;
    }

    std::size_t valueArgumentCount(const ParameterDefinition& parameter)
    {
        return parameter.kind == ParameterKind::Signed ? 2 : 1;
    }
}

TEST_CASE("Given the lot 1 catalogue, When it is counted, Then it holds 24 parameters in five groups: filter 5, amplitude envelope 4, filter envelope 5, LFO 1 5 and LFO 2 5 [RQ-MCP-004]",
          "[mcp][catalogue]")
{
    const auto& catalogue = ParameterCatalogue::standard();

    CHECK(catalogue.parameters().size() == 24);
    REQUIRE(catalogue.groups().size() == 5);
    for (const auto& [group, expected] : std::vector<std::pair<const char*, std::size_t>>{
             {"filter", 5}, {"amplitude envelope", 4}, {"filter envelope", 5}, {"lfo 1", 5}, {"lfo 2", 5}})
    {
        CAPTURE(group);
        const auto groups = catalogue.findGroups(group);
        REQUIRE(groups.size() == 1);
        CHECK(catalogue.parametersInGroup(*groups.front()).size() == expected);
    }
}

TEST_CASE("Given every row of the catalogue, When it is compared with the AKM item catalogue, Then its Set and Get items exist, take the arguments the row gives and its range equals the item's [RQ-MCP-004, RQ-MCP-011]",
          "[mcp][catalogue]")
{
    for (const ParameterDefinition& parameter : ParameterCatalogue::standard().parameters())
    {
        CAPTURE(parameter.name);
        const akm::ItemDescriptor& set = akm::descriptor(parameter.setItem);
        const akm::ItemDescriptor& get = akm::descriptor(parameter.getItem);
        CHECK(set.kind == akm::ItemKind::Set);
        CHECK(get.kind == akm::ItemKind::Get);
        CHECK(set.section == get.section);

        const std::size_t leading = parameter.leadingArguments.size();
        const std::size_t values = valueArgumentCount(parameter);
        REQUIRE(set.args.size() == leading + values);
        REQUIRE(get.args.size() == leading);
        REQUIRE(get.reply.size() == values);

        for (std::size_t i = 0; i < leading; ++i)
        {
            CHECK(parameter.leadingArguments[i] >= set.args[i].min);
            CHECK(parameter.leadingArguments[i] <= set.args[i].max);
            CHECK(parameter.leadingArguments[i] >= get.args[i].min);
            CHECK(parameter.leadingArguments[i] <= get.args[i].max);
        }

        // The sampler's own range of the value, and the same one on the way back.
        const auto setValue = set.args.subspan(leading);
        const auto& wireMagnitude = setValue.back();
        CHECK(get.reply.back().min == wireMagnitude.min);
        CHECK(get.reply.back().max == wireMagnitude.max);
        switch (parameter.kind)
        {
            case ParameterKind::Number:
                CHECK(parameter.min == wireMagnitude.min * parameter.step);
                CHECK(parameter.max == wireMagnitude.max * parameter.step);
                break;
            case ParameterKind::Signed:
                CHECK(setValue[0].min == 0);
                CHECK(setValue[0].max == 1);
                CHECK(parameter.min == -wireMagnitude.max);
                CHECK(parameter.max == wireMagnitude.max);
                break;
            case ParameterKind::Choice:
                CHECK(wireMagnitude.min == 0);
                CHECK(static_cast<std::int64_t>(parameter.labels.size()) == wireMagnitude.max + 1);
                break;
            case ParameterKind::Switch:
                CHECK(wireMagnitude.min == 0);
                CHECK(wireMagnitude.max == 1);
                break;
        }
    }
}

TEST_CASE("Given the catalogue, When every name and alias is normalised, Then no two parameters share one, and no two labels of a parameter do [RQ-MCP-004]",
          "[mcp][catalogue]")
{
    std::set<std::string> seen;
    for (const ParameterDefinition& parameter : ParameterCatalogue::standard().parameters())
    {
        CAPTURE(parameter.name);
        std::set<std::string> own;
        own.insert(mcp::normalizeText(parameter.name));
        for (const std::string& alias : parameter.aliases)
            own.insert(mcp::normalizeText(alias));
        CHECK(own.size() == parameter.aliases.size() + 1);
        for (const std::string& text : own)
            CHECK(seen.insert(text).second);

        std::set<std::string> labels;
        for (const std::string& label : parameter.labels)
            labels.insert(mcp::normalizeText(label));
        CHECK(labels.size() == parameter.labels.size());
    }
}

TEST_CASE("Given the spellings \"Filter-Cutoff\", \"filter cutoff\" and the alias \"cutoff\", When resolved, Then each is the same parameter, a keygroup parameter [RQ-MCP-006]",
          "[mcp][catalogue]")
{
    const ParameterDefinition& reference = named("filter cutoff");

    CHECK(&named("Filter-Cutoff") == &reference);
    CHECK(&named("  FILTER   CUTOFF ") == &reference);
    CHECK(&named("cutoff") == &reference);
    CHECK(&named("filter frequency") == &reference);
    CHECK(reference.scope == ParameterScope::Keygroup);
    CHECK(reference.kind == ParameterKind::Number);
    CHECK(reference.min == 0);
    CHECK(reference.max == 100);
    CHECK(&named("amp env attack") == &named("amplitude envelope attack"));
    CHECK(&named("lfo1 rate") == &named("LFO 1 rate"));
    CHECK(named("lfo 1 rate").scope == ParameterScope::Program);
}

TEST_CASE("Given the name \"filter cutof\", When resolved, Then it is refused and \"filter cutoff\" is proposed; a name far from every parameter gets no proposal [RQ-MCP-006]",
          "[mcp][catalogue]")
{
    const auto& catalogue = ParameterCatalogue::standard();

    const auto near = catalogue.resolveName("filter cutof");
    CHECK(near.parameter == nullptr);
    REQUIRE_FALSE(near.suggestions.empty());
    CHECK(near.suggestions.front() == "filter cutoff");
    CHECK(near.suggestions.size() <= 3);

    const auto far = catalogue.resolveName("zzzzzzzzzzzzzzzz");
    CHECK(far.parameter == nullptr);
    CHECK(far.suggestions.empty());
}

TEST_CASE("Given the filter type, When its choices are listed, Then there are 26 labels in code order beginning 2-POLE LP, 4-POLE LP, 2-POLE LP+ and ending VOWELISER [RQ-MCP-004]",
          "[mcp][catalogue]")
{
    const ParameterDefinition& type = named("filter type");

    REQUIRE(type.kind == ParameterKind::Choice);
    REQUIRE(type.labels.size() == 26);
    CHECK(type.labels[0] == "2-POLE LP");
    CHECK(type.labels[1] == "4-POLE LP");
    CHECK(type.labels[2] == "2-POLE LP+");
    CHECK(type.labels[25] == "VOWELISER");
    CHECK(&named("filter mode") == &type);
}

TEST_CASE("Given the label \"2-pole lp+\", \"2 POLE LP plus\" and the code 2, When resolved for the filter type, Then each gives code 2, while \"2-pole lp\" gives code 0 and the code 26 is refused [RQ-MCP-006]",
          "[mcp][catalogue]")
{
    const ParameterDefinition& type = named("filter type");

    CHECK(mcp::resolveValue(type, "2-pole lp+").value == 2);
    CHECK(mcp::resolveValue(type, "2 POLE LP plus").value == 2);
    CHECK(mcp::resolveValue(type, "2POLELP+").value == 2);
    CHECK(mcp::resolveValue(type, "2").value == 2);
    CHECK(mcp::resolveValue(type, std::int64_t{2}).value == 2);
    CHECK(mcp::resolveValue(type, "2-pole lp").value == 0);
    CHECK(mcp::resolveValue(type, "notch 2").value == 13);
    CHECK(mcp::resolveValue(type, "Wide Notch").value == 15);

    const auto outOfRange = mcp::resolveValue(type, std::int64_t{26});
    CHECK_FALSE(outOfRange.value.has_value());
    CHECK(contains(outOfRange.problem, "2-POLE LP+"));
    const auto unknown = mcp::resolveValue(type, "sawtooth");
    CHECK_FALSE(unknown.value.has_value());
    CHECK(contains(unknown.problem, "VOWELISER"));
}

TEST_CASE("Given the LFO waveform labels SQUARE, SQUARE+ and SQUARE-, When \"square\", \"SQUARE +\" and \"square minus\" are resolved, Then they give 2, 3 and 4 [RQ-MCP-006]",
          "[mcp][catalogue]")
{
    const ParameterDefinition& waveform = named("lfo 1 waveform");

    REQUIRE(waveform.labels.size() == 9);
    CHECK(waveform.labels[0] == "SINE");
    CHECK(waveform.labels[1] == "TRIANGLE");
    CHECK(mcp::resolveValue(waveform, "square").value == 2);
    CHECK(mcp::resolveValue(waveform, "SQUARE +").value == 3);
    CHECK(mcp::resolveValue(waveform, "square minus").value == 4);
    CHECK(mcp::resolveValue(waveform, "saw bi").value == 5);
    CHECK(mcp::resolveValue(waveform, "random").value == 8);
    CHECK(named("lfo 2 waveform").leadingArguments == std::vector<std::int64_t>{2});
    CHECK(waveform.leadingArguments == std::vector<std::int64_t>{1});
}

TEST_CASE("Given a signed parameter, When -40, 25 and 0 are converted for the sampler and the replies sign 1 magnitude 40, sign 0 magnitude 25 and sign 1 magnitude 0 are converted back, Then the sign and magnitude and the numbers match [RQ-MCP-005, RQ-MCP-006]",
          "[mcp][catalogue]")
{
    const ParameterDefinition& depth = named("filter envelope depth");
    REQUIRE(depth.kind == ParameterKind::Signed);
    CHECK(depth.min == -100);
    CHECK(depth.max == 100);

    CHECK(mcp::toItemValues(depth, -40) == std::vector<std::int64_t>{1, 40});
    CHECK(mcp::toItemValues(depth, 25) == std::vector<std::int64_t>{0, 25});
    CHECK(mcp::toItemValues(depth, 0) == std::vector<std::int64_t>{0, 0});

    const std::vector<std::int64_t> negative{1, 40};
    const std::vector<std::int64_t> positive{0, 25};
    const std::vector<std::int64_t> negativeZero{1, 0};
    CHECK(mcp::fromItemValues(depth, negative) == -40);
    CHECK(mcp::fromItemValues(depth, positive) == 25);
    CHECK(mcp::fromItemValues(depth, negativeZero) == 0);
    CHECK_FALSE(mcp::fromItemValues(depth, std::vector<std::int64_t>{1}).has_value());

    const ParameterDefinition& keyboardTrack = named("filter keyboard tracking");
    CHECK(keyboardTrack.min == -36);
    CHECK(keyboardTrack.max == 36);
}

TEST_CASE("Given the filter cutoff, When 101, -1, 80.5 and \"eighty\" are given, Then each is refused naming the range 0 to 100, and 80, \"80\" and 0 are accepted [RQ-MCP-006]",
          "[mcp][catalogue]")
{
    const ParameterDefinition& cutoff = named("filter cutoff");

    const auto high = mcp::resolveValue(cutoff, std::int64_t{101});
    CHECK_FALSE(high.value.has_value());
    CHECK(contains(high.problem, "0 to 100"));
    CHECK(contains(high.problem, "filter cutoff"));
    CHECK(contains(high.problem, "101"));
    CHECK_FALSE(mcp::resolveValue(cutoff, std::int64_t{-1}).value.has_value());
    CHECK(contains(mcp::resolveValue(cutoff, "80.5").problem, "0 to 100"));
    CHECK(contains(mcp::resolveValue(cutoff, "eighty").problem, "0 to 100"));

    CHECK(mcp::resolveValue(cutoff, std::int64_t{80}).value == 80);
    CHECK(mcp::resolveValue(cutoff, " 80 ").value == 80);
    CHECK(mcp::resolveValue(cutoff, "+80").value == 80);
    CHECK(mcp::resolveValue(cutoff, std::int64_t{0}).value == 0);
    CHECK(mcp::toItemValues(cutoff, 80) == std::vector<std::int64_t>{80});
    CHECK(mcp::fromItemValues(cutoff, std::vector<std::int64_t>{80}) == 80);
}

TEST_CASE("Given the filter attenuation in steps of 6 dB, When 12 is given, Then the sampler is sent code 2 and a reply of 2 reads 12; 7 is refused naming the steps [RQ-MCP-006]",
          "[mcp][catalogue]")
{
    const ParameterDefinition& attenuation = named("filter attenuation");
    REQUIRE(attenuation.kind == ParameterKind::Number);
    CHECK(attenuation.step == 6);
    CHECK(attenuation.unit == "dB");
    CHECK(attenuation.max == 30);

    CHECK(mcp::resolveValue(attenuation, std::int64_t{12}).value == 12);
    CHECK(mcp::toItemValues(attenuation, 12) == std::vector<std::int64_t>{2});
    CHECK(mcp::fromItemValues(attenuation, std::vector<std::int64_t>{2}) == 12);
    CHECK(mcp::resolveValue(attenuation, "12 dB").value == 12);
    const auto odd = mcp::resolveValue(attenuation, std::int64_t{7});
    CHECK_FALSE(odd.value.has_value());
    CHECK(contains(odd.problem, "0, 6, 12, 18, 24 or 30"));
    CHECK(mcp::describeValue(attenuation, 12) == "12 dB");
}

TEST_CASE("Given the switches LFO 1 sync and LFO 2 re-trigger, When on, off, true, no and 1 are given, Then they resolve to 1 or 0, and the first argument is the LFO number [RQ-MCP-006]",
          "[mcp][catalogue]")
{
    const ParameterDefinition& sync = named("lfo 1 sync");
    const ParameterDefinition& retrigger = named("lfo 2 retrigger");

    REQUIRE(sync.kind == ParameterKind::Switch);
    CHECK(sync.leadingArguments == std::vector<std::int64_t>{1});
    CHECK(retrigger.leadingArguments == std::vector<std::int64_t>{2});
    CHECK(&named("lfo 2 re-trigger") == &retrigger);
    CHECK(mcp::resolveValue(sync, "on").value == 1);
    CHECK(mcp::resolveValue(sync, "OFF").value == 0);
    CHECK(mcp::resolveValue(sync, "true").value == 1);
    CHECK(mcp::resolveValue(sync, "no").value == 0);
    CHECK(mcp::resolveValue(sync, "1").value == 1);
    CHECK(mcp::resolveValue(sync, std::int64_t{0}).value == 0);
    CHECK_FALSE(mcp::resolveValue(sync, "maybe").value.has_value());
    CHECK_FALSE(mcp::resolveValue(sync, std::int64_t{2}).value.has_value());
    CHECK(mcp::describeValue(sync, 1) == "on");
}

TEST_CASE("Given values of each kind, When they are described, Then a choice gives its label and code, a signed number its sign, a plain number itself [RQ-MCP-005]",
          "[mcp][catalogue]")
{
    CHECK(mcp::describeValue(named("filter type"), 2) == "2-POLE LP+ (code 2)");
    CHECK(mcp::describeValue(named("filter envelope depth"), -40) == "-40");
    CHECK(mcp::describeValue(named("filter envelope depth"), 40) == "40");
    CHECK(mcp::describeValue(named("filter cutoff"), 80) == "80");
    CHECK(mcp::describeRange(named("filter cutoff")) == "0 to 100");
    CHECK(mcp::describeRange(named("filter envelope depth")) == "-100 to 100");
    CHECK(mcp::describeRange(named("filter attenuation")) == "0, 6, 12, 18, 24 or 30 dB");
}

TEST_CASE("Given the group names, When \"Amp Envelope\", \"filter env\", \"lfo1\" and \"lfo\" are looked up, Then the first three give one group each and \"lfo\" gives both LFOs; an unknown group gives none [RQ-MCP-004]",
          "[mcp][catalogue]")
{
    const auto& catalogue = ParameterCatalogue::standard();

    REQUIRE(catalogue.findGroups("Amp Envelope").size() == 1);
    CHECK(catalogue.findGroups("Amp Envelope").front()->name == "amplitude envelope");
    REQUIRE(catalogue.findGroups("filter env").size() == 1);
    CHECK(catalogue.findGroups("filter env").front()->name == "filter envelope");
    REQUIRE(catalogue.findGroups("lfo1").size() == 1);
    CHECK(catalogue.findGroups("lfo1").front()->name == "lfo 1");
    CHECK(catalogue.findGroups("lfo").size() == 2);
    CHECK(catalogue.findGroups("reverb").empty());
}
