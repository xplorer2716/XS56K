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
// The multi editor against the simulated sampler: the multi binding of the edit controller (the part chosen or all parts, the part parameters),
// the program of a part, the multi's program number, a multi created from the memory view. [RQ-APP-100 to RQ-APP-104, ADR-APP-006 (DEC-APP-041
// to DEC-APP-043), TASK-APP-062 to TASK-APP-065]
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "AppTestSupport.hpp"
#include "SimulatedPrograms.hpp"
#include "akm/harness/SimulatedSampler.hpp"
#include "app/EditController.hpp"
#include "app/MemoryController.hpp"
#include "mcp/ParameterCatalogue.hpp"

using app::ConnectionState;
using app::MemoryItem;
using app::MemoryKind;
using app::test::CoreRig;
using app::test::setupA;

namespace
{
    constexpr std::uint8_t SECTION_MULTI = 0x0C;
    constexpr std::uint8_t ITEM_CREATE_MULTI = 0x02;
    constexpr std::uint8_t ITEM_SET_LOW_NOTE = 0x1A;
    constexpr std::uint8_t ITEM_SET_PART_BY_INDEX = 0x32;
    constexpr std::uint8_t ITEM_SET_PART_BY_NAME = 0x33;
    constexpr int LIVE_PARTS = 32;
    constexpr int SECOND_PART = 2;
    constexpr std::size_t MULTI_TAB_VALUES = 14;  // the program number, the part's program and its twelve parameters
    constexpr std::int64_t PART_1_LEVEL = 60;
    constexpr std::int64_t PART_2_LEVEL = 30;
    constexpr std::int64_t NEW_LEVEL = 70;
    constexpr std::int64_t C3 = 60;
    constexpr std::int64_t C4 = 72;
    constexpr std::int64_t ON = 1;
    constexpr int PROGRAM_NUMBER = 5;
    constexpr std::size_t LIVE_INDEX = 1;
    constexpr const char* LIVE = "LIVE";

    const mcp::ParameterDefinition& partParameter(const std::string& name)
    {
        const auto& rows = mcp::ParameterCatalogue::multis().parameters();
        return *std::ranges::find_if(rows, [&name](const mcp::ParameterDefinition& row) { return row.name == name; });
    }

    /// The programs PAD and BASS, the multis M1 and LIVE (32 parts each, part 1 at level 60, part 2 at 30, nothing played), connected.
    struct MultiRig
    {
        MultiRig()
        {
            mcp::test::seedSimulatedPrograms(rig.backend, {{"PAD", 1}, {"BASS", 1}}, 0);
            rig.sampler->setMultiNames({"M1", LIVE});
            setPart("part level", 1, PART_1_LEVEL);
            setPart("part level", SECOND_PART, PART_2_LEVEL);
            rig.controller.connect(setupA());
            REQUIRE(rig.settle());
            REQUIRE(rig.model.connection().state == ConnectionState::Connected);
        }

        /// A part parameter of LIVE put on the sampler from outside the application (the front panel).
        void setPart(const std::string& name, int part, std::int64_t value)
        {
            REQUIRE(rig.gateway.selectMultiByName(LIVE).ok());
            REQUIRE(rig.gateway.writeMultiParameter(partParameter(name), value, mcp::PartSelection::of(part)).ok());
        }

        /// A part parameter of LIVE as the sampler holds it.
        [[nodiscard]] std::int64_t onSampler(const std::string& name, int part)
        {
            REQUIRE(rig.gateway.selectMultiByName(LIVE).ok());
            const auto read = rig.gateway.readMultiParameter(partParameter(name), mcp::PartSelection::of(part));
            REQUIRE(read.ok());
            REQUIRE_FALSE(read.value->empty());
            return read.value->front().value;
        }

        void showLive()
        {
            memory.select(MemoryItem{MemoryKind::Multi, LIVE});
            REQUIRE(rig.settle());
        }

        [[nodiscard]] const app::EditValue& shown(const std::string& name) const
        {
            const auto& values = rig.model.edit().values;
            const auto found = std::ranges::find_if(values, [&name](const app::EditValue& value) { return value.definition->name == name; });
            REQUIRE(found != values.end());
            return *found;
        }

        [[nodiscard]] std::size_t index(const std::string& name) const
        {
            const auto& values = rig.model.edit().values;
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                if (values[i].definition->name == name)
                    return i;
            }
            FAIL_CHECK("no parameter " << name);
            return 0;
        }

        [[nodiscard]] std::size_t sent(std::uint8_t item) const
        {
            const auto commands = rig.sampler->acceptedCommands();
            return static_cast<std::size_t>(std::count_if(commands.begin(), commands.end(), [item](const akm::harness::AcceptedCommand& command) {
                return command.section == SECTION_MULTI && command.item == item;
            }));
        }

        CoreRig rig;
        app::MemoryController memory{rig.model, rig.controller};
        app::EditController editor{rig.model, rig.controller};
    };
}

TEST_CASE("Given LIVE with no program number, When it is read alone, Then none comes back; Given 5, Then 5 comes back [RQ-APP-103, "
          "DEC-APP-043, TASK-APP-062]",
          "[app][multi]")
{
    MultiRig fixture;
    REQUIRE(fixture.rig.gateway.selectMultiByName(LIVE).ok());
    const auto off = fixture.rig.gateway.readMultiProgramNumber();
    REQUIRE(off.ok());
    CHECK(off.value->multi == LIVE);
    CHECK_FALSE(off.value->number.has_value());

    REQUIRE(fixture.rig.gateway.setMultiProgramNumber(PROGRAM_NUMBER).ok());
    const auto five = fixture.rig.gateway.readMultiProgramNumber();
    REQUIRE(five.ok());
    CHECK(five.value->number == std::optional<int>(PROGRAM_NUMBER));
}

TEST_CASE("Given LIVE of 32 parts, When it is selected, Then the multi editor shows part 1's values read from the sampler and its 32 parts "
          "[RQ-APP-100, RQ-APP-101, DEC-APP-041, TASK-APP-063]",
          "[app][multi]")
{
    MultiRig fixture;
    fixture.showLive();

    const app::EditState& edit = fixture.rig.model.edit();
    REQUIRE(edit.layout != nullptr);
    CHECK(edit.layout->editor == "multi");
    CHECK(edit.keygroups == std::optional<int>(LIVE_PARTS));
    CHECK(edit.keygroup == 1);
    CHECK(edit.values.size() == MULTI_TAB_VALUES);
    CHECK(fixture.shown("part level").value == PART_1_LEVEL);
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given LIVE shown, When part 2 is chosen, Then its values are read; When the person sets its level to 70, Then it is sent, read "
          "back and shown [RQ-APP-100, RQ-APP-101, DEC-APP-041, TASK-APP-063]",
          "[app][multi]")
{
    MultiRig fixture;
    fixture.showLive();
    fixture.editor.chooseKeygroup(SECOND_PART);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.rig.model.edit().keygroup == SECOND_PART);
    CHECK(fixture.shown("part level").value == PART_2_LEVEL);

    fixture.editor.set(fixture.index("part level"), NEW_LEVEL);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shown("part level").value == NEW_LEVEL);
    CHECK_FALSE(fixture.shown("part level").pending);
    CHECK(fixture.onSampler("part level", SECOND_PART) == NEW_LEVEL);
    CHECK(fixture.onSampler("part level", 1) == PART_1_LEVEL);
}

TEST_CASE("Given LIVE's parts at levels 60 and 30, When all parts are chosen, Then the level says the parts differ; When the person mutes, "
          "Then every part is muted [RQ-APP-101, RQ-APP-059, DEC-APP-041, TASK-APP-063]",
          "[app][multi]")
{
    MultiRig fixture;
    fixture.showLive();
    fixture.editor.chooseKeygroup(app::ALL_KEYGROUPS);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shown("part level").differs);

    fixture.editor.set(fixture.index("part mute"), ON);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.onSampler("part mute", 1) == ON);
    CHECK(fixture.onSampler("part mute", LIVE_PARTS) == ON);
    CHECK_FALSE(fixture.shown("part mute").differs);
}

TEST_CASE("Given part 1's high note C3, When the person sets its low note to C4, Then nothing is sent and the range is shown [RQ-APP-100, "
          "RQ-APP-092, TASK-APP-063]",
          "[app][multi]")
{
    MultiRig fixture;
    fixture.setPart("part high note", 1, C3);
    fixture.showLive();
    const std::size_t before = fixture.sent(ITEM_SET_LOW_NOTE);

    fixture.editor.set(fixture.index("part low note"), C4);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_SET_LOW_NOTE) == before);
    REQUIRE(fixture.rig.model.problem().has_value());
    CHECK(fixture.rig.model.problem()->reason.find("60") != std::string::npos);
}

TEST_CASE("Given LIVE's part 2 playing nothing, When the person chooses PAD for it, Then PAD is assigned and part 2 reads PAD; When the person "
          "chooses none, Then part 2 plays nothing and PAD stays in memory [RQ-APP-102, DEC-APP-042, TASK-APP-064]",
          "[app][multi]")
{
    MultiRig fixture;
    fixture.showLive();
    fixture.editor.chooseKeygroup(SECOND_PART);
    REQUIRE(fixture.rig.settle());
    const app::EditState& before = fixture.rig.model.edit();
    REQUIRE(before.partPrograms.size() == static_cast<std::size_t>(LIVE_PARTS));
    CHECK(before.partPrograms[1].empty());
    CHECK(fixture.shown("part program").value == 0);
    const auto pad = std::ranges::find(before.programs, std::string("PAD"));
    REQUIRE(pad != before.programs.end());
    const auto padValue = static_cast<std::int64_t>(pad - before.programs.begin()) + 1;

    fixture.editor.set(fixture.index("part program"), padValue);
    REQUIRE(fixture.rig.settle());
    CHECK_FALSE(fixture.rig.model.problem().has_value());
    CHECK(fixture.shown("part program").value == padValue);
    CHECK(fixture.rig.model.edit().partPrograms[1] == "PAD");
    CHECK(fixture.rig.model.edit().partPrograms[0].empty());

    fixture.editor.set(fixture.index("part program"), 0);
    REQUIRE(fixture.rig.settle());
    CHECK_FALSE(fixture.rig.model.problem().has_value());
    CHECK(fixture.shown("part program").value == 0);
    CHECK(fixture.rig.model.edit().partPrograms[1].empty());
    CHECK(std::ranges::find(fixture.rig.model.edit().programs, std::string("PAD")) != fixture.rig.model.edit().programs.end());
}

TEST_CASE("Given all parts chosen, When the person chooses a program, Then nothing is sent and the problem says to choose one part "
          "[RQ-APP-102, DEC-APP-042, TASK-APP-064]",
          "[app][multi]")
{
    MultiRig fixture;
    fixture.showLive();
    fixture.editor.chooseKeygroup(app::ALL_KEYGROUPS);
    REQUIRE(fixture.rig.settle());
    const std::size_t before = fixture.sent(ITEM_SET_PART_BY_NAME) + fixture.sent(ITEM_SET_PART_BY_INDEX);

    fixture.editor.set(fixture.index("part program"), 1);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_SET_PART_BY_NAME) + fixture.sent(ITEM_SET_PART_BY_INDEX) == before);
    REQUIRE(fixture.rig.model.problem().has_value());
    CHECK(fixture.rig.model.problem()->reason.find("one part") != std::string::npos);
}

TEST_CASE("Given LIVE with no program number, When it is shown, Then it reads OFF; When the person sets 5, Then it is sent and reads 5 "
          "[RQ-APP-103, DEC-APP-043, TASK-APP-064]",
          "[app][multi]")
{
    MultiRig fixture;
    fixture.showLive();
    CHECK(fixture.shown("multi program number").value == 0);
    CHECK(app::valueText(fixture.shown("multi program number")) == "OFF");

    fixture.editor.set(fixture.index("multi program number"), PROGRAM_NUMBER);
    REQUIRE(fixture.rig.settle());
    CHECK_FALSE(fixture.rig.model.problem().has_value());
    CHECK(fixture.shown("multi program number").value == PROGRAM_NUMBER);
    CHECK(app::valueText(fixture.shown("multi program number")) == "5");
    REQUIRE(fixture.rig.gateway.selectMultiByName(LIVE).ok());
    CHECK(fixture.rig.gateway.readMultiProgramNumber().value->number == std::optional<int>(PROGRAM_NUMBER));
    CHECK(fixture.rig.sampler->currentMulti() == std::optional<std::size_t>(LIVE_INDEX));
}

TEST_CASE("Given the multi M1, When the person creates SET 1, Then it is created, listed and selected, its editor shown; When the person "
          "creates m-1, Then nothing is sent [RQ-APP-104, DEC-APP-043, TASK-APP-065]",
          "[app][multi]")
{
    MultiRig fixture;
    fixture.memory.createMulti("SET 1");
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_CREATE_MULTI) == 1);
    CHECK(fixture.rig.model.memory().selection == std::optional<MemoryItem>(MemoryItem{MemoryKind::Multi, "SET 1"}));
    CHECK(fixture.rig.model.edit().layout != nullptr);
    CHECK_FALSE(fixture.rig.model.problem().has_value());

    fixture.memory.createMulti("m-1");
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_CREATE_MULTI) == 1);
    REQUIRE(fixture.rig.model.problem().has_value());
    CHECK(fixture.rig.model.problem()->reason.find("M1") != std::string::npos);
}
