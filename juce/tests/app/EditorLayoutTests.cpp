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
// The editor layouts against the catalogues: every name exists and can be set, and the sample editor holds every settable parameter of a
// sample. [RQ-APP-086, RQ-APP-049, ADR-APP-001 (DEC-APP-006), ADR-APP-004 (DEC-APP-024), TASK-APP-035]
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <string>

#include "app/EditorLayouts.generated.hpp"
#include "mcp/ParameterCatalogue.hpp"

namespace
{
    const app::layouts::EditorLayout* layoutOf(std::string_view editor)
    {
        const auto& layouts = app::layouts::kEditorLayouts;
        const auto found = std::ranges::find_if(layouts, [editor](const auto& layout) { return layout.editor == editor; });
        return found == layouts.end() ? nullptr : &*found;
    }
}

TEST_CASE("Given the sample editor's layout, Then its tab Sample holds Trim, Loop and Pitch, each name is a settable parameter of the "
          "sample catalogue, and every settable parameter of a sample is in it once [RQ-APP-086, TASK-APP-035]",
          "[app][layout]")
{
    const app::layouts::EditorLayout* sample = layoutOf("sample");
    REQUIRE(sample != nullptr);
    REQUIRE(sample->tabs.size() == 1);
    CHECK(sample->tabs[0].title == "Sample");
    REQUIRE(sample->tabs[0].groups.size() == 3);
    CHECK(sample->tabs[0].groups[0].title == "Trim");
    CHECK(sample->tabs[0].groups[1].title == "Loop");
    CHECK(sample->tabs[0].groups[2].title == "Pitch");

    const auto& catalogue = mcp::ParameterCatalogue::samples();
    std::multiset<std::string> named;
    for (const auto& group : sample->tabs[0].groups)
    {
        for (const auto& parameter : group.parameters)
        {
            named.insert(std::string(parameter.name));
            const auto& rows = catalogue.parameters();
            const auto row = std::ranges::find_if(rows, [&parameter](const mcp::ParameterDefinition& definition) {
                return definition.name == parameter.name;
            });
            INFO(std::string(parameter.name));
            REQUIRE(row != rows.end());
            CHECK_FALSE(row->readOnly);
        }
    }
    for (const mcp::ParameterDefinition& definition : catalogue.parameters())
    {
        INFO(definition.name);
        CHECK(named.count(definition.name) == (definition.readOnly ? 0U : 1U));
    }
}

TEST_CASE("Given the sample editor's layout, Then its one order is start, loop start, loop end, end [RQ-APP-092, TASK-APP-042]", "[app][layout]")
{
    const app::layouts::EditorLayout* sample = layoutOf("sample");
    REQUIRE(sample != nullptr);
    REQUIRE(sample->orders.size() == 1);
    const auto& order = sample->orders[0];
    REQUIRE(order.size() == 4);
    CHECK(order[0] == "sample start position");
    CHECK(order[1] == "sample loop start");
    CHECK(order[2] == "sample loop end");
    CHECK(order[3] == "sample end position");
}

TEST_CASE("Given the program editor's layout, Then every name is a parameter of the standard catalogue and every group's parameters live where "
          "its scope, or its column's, says [RQ-APP-056, RQ-APP-093, RQ-APP-094, DEC-APP-031, DEC-APP-035, TASK-APP-044, TASK-APP-049]",
          "[app][layout]")
{
    const app::layouts::EditorLayout* program = layoutOf("program");
    REQUIRE(program != nullptr);
    const auto& rows = mcp::ParameterCatalogue::standard().parameters();
    for (const auto& tab : program->tabs)
    {
        for (const auto& group : tab.groups)
        {
            INFO(std::string(group.title));
            // A group of columns says the scope of each column: of its cells. [RQ-APP-056, DEC-APP-035]
            CHECK((group.scope != app::layouts::Scope::None) != !group.columns.empty());
            for (std::size_t i = 0; i < group.parameters.size(); ++i)
            {
                const auto& parameter = group.parameters[i];
                const app::layouts::Scope scope = group.columns.empty() ? group.scope : group.columns[i % group.columns.size()].scope;
                INFO(std::string(parameter.name));
                if (parameter.control == app::layouts::Control::Sample)
                {
                    CHECK(group.zones > 0);  // a zone's sample is no catalogue row: it is chosen by name in a zone grid [RQ-APP-060, DEC-APP-033]
                    continue;
                }
                const auto row = std::ranges::find_if(rows, [&parameter](const mcp::ParameterDefinition& definition) {
                    return definition.name == parameter.name;
                });
                REQUIRE(row != rows.end());
                const bool programScope = row->scope == mcp::ParameterScope::Program;
                CHECK(programScope == (scope == app::layouts::Scope::Program));
            }
        }
    }
}

TEST_CASE("Given the Filter tab's type, Then it names the filter shapes, each a label of the catalogue's filter types; 2-POLE LP falls to 0 at "
          "the right and VOWELISER has no shape [RQ-APP-096, DEC-APP-032, TASK-APP-051]",
          "[app][layout]")
{
    constexpr std::size_t MANUAL_SHAPES = 19;
    const app::layouts::EditorLayout* program = layoutOf("program");
    REQUIRE(program != nullptr);
    const app::layouts::LayoutParameter* type = nullptr;
    for (const auto& tab : program->tabs)
    {
        for (const auto& group : tab.groups)
        {
            for (const auto& parameter : group.parameters)
            {
                if (parameter.name == "filter type")
                    type = &parameter;
            }
        }
    }
    REQUIRE(type != nullptr);
    CHECK(type->shapes.size() == MANUAL_SHAPES);
    const auto& rows = mcp::ParameterCatalogue::standard().parameters();
    const auto row = std::ranges::find_if(rows, [](const mcp::ParameterDefinition& definition) { return definition.name == "filter type"; });
    REQUIRE(row != rows.end());
    for (const app::layouts::ChoiceShape& shape : type->shapes)
    {
        INFO(std::string(shape.label));
        CHECK(std::ranges::find(row->labels, std::string(shape.label)) != row->labels.end());
    }

    const auto lowPass = app::layouts::shapeOf(*type, "2-POLE LP");
    REQUIRE(lowPass.size() >= 2);
    CHECK(lowPass.front().y > 0.0F);
    CHECK(lowPass.back().x == 1.0F);
    CHECK(lowPass.back().y == 0.0F);
    CHECK(app::layouts::shapeOf(*type, "VOWELISER").empty());
}

TEST_CASE("Given the Filter tab's type, Then 2-POLE LP+, 2-POLE BP+ and 1-POLE HP+ carry a plus above their pass level and 2-POLE LP none "
          "[RQ-APP-099, DEC-APP-037, TASK-APP-054]",
          "[app][layout]")
{
    const app::layouts::EditorLayout* program = layoutOf("program");
    REQUIRE(program != nullptr);
    const app::layouts::LayoutParameter* type = nullptr;
    for (const auto& tab : program->tabs)
    {
        for (const auto& group : tab.groups)
        {
            for (const auto& parameter : group.parameters)
            {
                if (parameter.name == "filter type")
                    type = &parameter;
            }
        }
    }
    REQUIRE(type != nullptr);
    for (const std::string_view enhanced : {"2-POLE LP+", "2-POLE BP+", "1-POLE HP+"})
    {
        INFO(std::string(enhanced));
        const auto mark = app::layouts::markOf(*type, enhanced);
        REQUIRE(mark.has_value());
        CHECK(mark->text == "+");
        const auto shape = app::layouts::shapeOf(*type, enhanced);
        const auto top = std::ranges::max(shape, {}, &app::layouts::ShapePoint::y);
        CHECK(mark->at.y > top.y);
    }
    CHECK_FALSE(app::layouts::markOf(*type, "2-POLE LP").has_value());
    CHECK_FALSE(app::layouts::markOf(*type, "VOWELISER").has_value());
}

TEST_CASE("Given the LFO tab, Then LFO 2's clock division, whose labels are long, is a choice three knob cells wide "
          "[RQ-APP-057, DEC-APP-027, TASK-APP-048]",
          "[app][layout]")
{
    constexpr int WIDE = 3;
    const app::layouts::EditorLayout* program = layoutOf("program");
    REQUIRE(program != nullptr);
    const app::layouts::LayoutParameter* division = nullptr;
    for (const auto& tab : program->tabs)
    {
        for (const auto& group : tab.groups)
        {
            for (const auto& parameter : group.parameters)
            {
                if (parameter.name == "lfo 2 clock division")
                    division = &parameter;
            }
        }
    }
    REQUIRE(division != nullptr);
    CHECK(division->control == app::layouts::Control::Choice);
    CHECK(division->cells == WIDE);
}
