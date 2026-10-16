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
          "its scope says [RQ-APP-056, RQ-APP-093, RQ-APP-094, DEC-APP-031, TASK-APP-044]",
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
            CHECK(group.scope != app::layouts::Scope::None);
            for (const auto& parameter : group.parameters)
            {
                INFO(std::string(parameter.name));
                const auto row = std::ranges::find_if(rows, [&parameter](const mcp::ParameterDefinition& definition) {
                    return definition.name == parameter.name;
                });
                REQUIRE(row != rows.end());
                const bool programScope = row->scope == mcp::ParameterScope::Program;
                CHECK(programScope == (group.scope == app::layouts::Scope::Program));
            }
        }
    }
}
