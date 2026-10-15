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
// The memory view of the editor application, commands, over the gateway and the simulated sampler: creating a program, renaming and
// deleting an item, and auditioning a sample of the memory, each refusal or cancel sending nothing. [TASK-APP-016, RQ-APP-038, RQ-APP-039,
// RQ-APP-040, RQ-APP-041, RQ-APP-011, ADR-APP-001 (DEC-APP-005, DEC-APP-007)]
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "AppTestSupport.hpp"
#include "SimulatedPrograms.hpp"
#include "app/MemoryController.hpp"
#include "akm/harness/SimulatedSampler.hpp"

using app::ConnectionState;
using app::MemoryItem;
using app::MemoryKind;
using app::test::CoreRig;
using app::test::setupA;

namespace
{
    constexpr std::uint8_t SECTION_PROGRAM = 0x0A;
    constexpr std::uint8_t SECTION_SAMPLE = 0x0E;
    constexpr std::uint8_t ITEM_CREATE_PROGRAM = 0x03;
    constexpr std::uint8_t ITEM_DELETE_PROGRAM = 0x08;
    constexpr std::uint8_t ITEM_RENAME_SAMPLE = 0x09;
    constexpr std::uint8_t ITEM_START_AUDITION = 0x0A;
    constexpr std::uint8_t ITEM_STOP_AUDITION = 0x0B;
    constexpr int TWO_KEYGROUPS = 2;

    /// The sampler holds the programs PAD and BASS and the samples KICK and SNARE; the memory view is connected to it.
    struct CommandRig
    {
        CommandRig()
        {
            mcp::test::seedSimulatedPrograms(rig.backend, {{"PAD"}, {"BASS"}}, 1);
            rig.sampler->setSampleNames({"KICK", "SNARE"});
            rig.controller.connect(setupA());
            REQUIRE(rig.settle());
            REQUIRE(rig.model.connection().state == ConnectionState::Connected);
        }

        [[nodiscard]] const app::MemoryList& list(MemoryKind kind) const { return rig.model.memory().list(kind); }

        [[nodiscard]] bool holds(MemoryKind kind, const std::string& name) const
        {
            const auto& names = list(kind).names;
            return std::find(names.begin(), names.end(), name) != names.end();
        }

        [[nodiscard]] std::size_t sent(std::uint8_t section, std::uint8_t item) const
        {
            const auto commands = rig.sampler->acceptedCommands();
            return static_cast<std::size_t>(std::count_if(commands.begin(), commands.end(), [section, item](const akm::harness::AcceptedCommand& command) {
                return command.section == section && command.item == item;
            }));
        }

        CoreRig rig;
        app::MemoryController memory{rig.model, rig.controller};
    };
}

TEST_CASE("Given no program LEAD, When the person creates LEAD with 2 keygroups, Then it is listed, current and selected; When the person "
          "creates PAD, which exists, Then nothing is sent and the reason is shown [RQ-APP-038]",
          "[app][memory]")
{
    CommandRig fixture;

    fixture.memory.createProgram("LEAD", TWO_KEYGROUPS);
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.sent(SECTION_PROGRAM, ITEM_CREATE_PROGRAM) == 1);
    CHECK(fixture.holds(MemoryKind::Program, "LEAD"));
    CHECK(fixture.rig.model.memory().current(MemoryKind::Program) == std::optional<std::string>("LEAD"));
    CHECK(fixture.rig.model.memory().selection == std::optional<MemoryItem>(MemoryItem{MemoryKind::Program, "LEAD"}));
    CHECK_FALSE(fixture.rig.model.problem().has_value());

    const std::size_t before = fixture.rig.sampler->acceptedCommands().size();
    fixture.memory.createProgram("pad", TWO_KEYGROUPS);
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
    REQUIRE(fixture.rig.model.problem().has_value());
    CHECK(fixture.rig.model.problem()->reason.find("PAD") != std::string::npos);
}

TEST_CASE("Given the samples KICK and SNARE, When the person renames KICK as KICK 2, Then the list shows KICK 2 and SNARE; When the person "
          "renames KICK 2 as SNARE, Then nothing is sent [RQ-APP-039]",
          "[app][memory]")
{
    CommandRig fixture;

    fixture.memory.rename(MemoryItem{MemoryKind::Sample, "KICK"}, "KICK 2");
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.sent(SECTION_SAMPLE, ITEM_RENAME_SAMPLE) == 1);
    CHECK(fixture.list(MemoryKind::Sample).names == std::vector<std::string>{"KICK 2", "SNARE"});
    CHECK(fixture.rig.model.memory().selection == std::optional<MemoryItem>(MemoryItem{MemoryKind::Sample, "KICK 2"}));
    CHECK_FALSE(fixture.rig.model.problem().has_value());

    const std::size_t before = fixture.rig.sampler->acceptedCommands().size();
    fixture.memory.rename(MemoryItem{MemoryKind::Sample, "KICK 2"}, "SNARE");
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
    CHECK(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given the program PAD, When the person deletes it and cancels, Then nothing is sent; When the person confirms, Then PAD is no "
          "longer listed [RQ-APP-040, RQ-APP-011]",
          "[app][memory]")
{
    CommandRig fixture;
    const MemoryItem pad{MemoryKind::Program, "PAD"};
    const std::size_t before = fixture.rig.sampler->acceptedCommands().size();

    fixture.rig.prompts.answers = {false};
    fixture.memory.remove(pad);
    REQUIRE(fixture.rig.settle());

    REQUIRE(fixture.rig.prompts.asked.size() == 1);
    CHECK(fixture.rig.prompts.asked[0].object == "PAD");
    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
    CHECK(fixture.holds(MemoryKind::Program, "PAD"));

    fixture.rig.prompts.answers = {true};
    fixture.memory.remove(pad);
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.sent(SECTION_PROGRAM, ITEM_DELETE_PROGRAM) == 1);
    CHECK_FALSE(fixture.holds(MemoryKind::Program, "PAD"));
    CHECK(fixture.holds(MemoryKind::Program, "BASS"));
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given KICK selected, When the person auditions it, Then the audition starts; When the person selects SNARE, Then the audition of "
          "KICK stops [RQ-APP-041]",
          "[app][memory]")
{
    CommandRig fixture;
    fixture.memory.select(MemoryItem{MemoryKind::Sample, "KICK"});
    REQUIRE(fixture.rig.settle());

    fixture.memory.audition(MemoryItem{MemoryKind::Sample, "KICK"});
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(SECTION_SAMPLE, ITEM_START_AUDITION) == 1);
    CHECK(fixture.rig.model.memory().current(MemoryKind::Sample) == std::optional<std::string>("KICK"));

    fixture.memory.select(MemoryItem{MemoryKind::Sample, "SNARE"});
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(SECTION_SAMPLE, ITEM_STOP_AUDITION) == 1);

    fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(SECTION_SAMPLE, ITEM_STOP_AUDITION) == 1);  // nothing was playing any more
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}
