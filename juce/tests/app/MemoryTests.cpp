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

// The memory view of the editor application, read side, over the gateway and the simulated sampler: the six lists with their current items
// once the session opens, read again after an action that changes the memory and on request, an item selected on the sampler, and the
// free memory. [TASK-APP-015, RQ-APP-035, RQ-APP-036, RQ-APP-037, RQ-APP-042, RQ-APP-010, ADR-APP-001 (DEC-APP-004)]
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "AppTestSupport.hpp"
#include "SimulatedPrograms.hpp"
#include "app/FileBrowser.hpp"
#include "app/MemoryController.hpp"
#include "akm/harness/SimulatedSampler.hpp"

using akm::harness::DiskRecord;
using akm::harness::FileRecord;
using app::BrowserItem;
using app::BrowserItemKind;
using app::ConnectionState;
using app::MemoryItem;
using app::MemoryKind;
using app::test::CoreRig;
using app::test::setupA;

namespace
{
    constexpr std::uint8_t SECTION_PROGRAM = 0x0A;
    constexpr std::uint8_t ITEM_SELECT_BY_NAME = 0x05;
    constexpr std::uint16_t ERROR_NOT_FOUND = 4;
    constexpr std::uint32_t MEGABYTE = 1024u * 1024u;
    constexpr std::uint32_t WAVE_TOTAL = 8 * MEGABYTE;
    constexpr std::uint32_t WAVE_FREE = 2 * MEGABYTE;
    constexpr std::uint8_t MPKS_FREE_PERCENT = 40;

    /// HD1, whose root holds LEAD.AKP, a program LEAD that uses SAW.WAV, a sample SAW.
    std::vector<DiskRecord> diskWithAProgram()
    {
        DiskRecord hard;
        hard.handle = 0;
        hard.name = "HD1";
        hard.type = 1;
        hard.format = 1;
        hard.writable = true;
        hard.freeBytes = 1000000;
        hard.rootFolder.files = {FileRecord{"LEAD.AKP", 4096, std::string("LEAD"), std::nullopt, {"SAW.WAV"}},
                                 FileRecord{"SAW.WAV", 150000, std::nullopt, std::string("SAW"), {}}};
        return {hard};
    }

    /// The sampler holds the programs PAD and BASS, BASS current, the sample KICK, 2 MB free of 8 MB of wave memory, and one disk; the
    /// memory view and the browser are connected to it.
    struct MemoryRig
    {
        MemoryRig()
        {
            mcp::test::seedSimulatedPrograms(rig.backend, {{"PAD"}, {"BASS"}}, 1);
            rig.sampler->setSampleNames({"KICK"});
            rig.sampler->setMemory(WAVE_TOTAL, WAVE_FREE, MPKS_FREE_PERCENT);
            rig.sampler->setDisks(diskWithAProgram());
            rig.controller.connect(setupA());
            REQUIRE(rig.settle());
            REQUIRE(rig.model.connection().state == ConnectionState::Connected);
        }

        [[nodiscard]] const app::MemoryList& list(MemoryKind kind) const { return rig.model.memory().list(kind); }

        [[nodiscard]] std::size_t selectsSent() const
        {
            const auto commands = rig.sampler->acceptedCommands();
            return static_cast<std::size_t>(std::count_if(commands.begin(), commands.end(), [](const akm::harness::AcceptedCommand& command) {
                return command.section == SECTION_PROGRAM && command.item == ITEM_SELECT_BY_NAME;
            }));
        }

        CoreRig rig;
        app::MemoryController memory{rig.model, rig.controller};
        app::FileBrowser browser{rig.model, rig.controller};
    };

    bool holds(const app::MemoryList& list, const std::string& name)
    {
        return std::ranges::find(list.names, name) != list.names.end();
    }
}

TEST_CASE("Given the simulated sampler holding the programs PAD and BASS with BASS current and the sample KICK, When the session opens, Then "
          "the programs list both with BASS current, the samples list KICK and the four other kinds are empty [RQ-APP-035]",
          "[app][memory]")
{
    MemoryRig fixture;

    const app::MemoryList& programs = fixture.list(MemoryKind::Program);
    CHECK(programs.names.size() == 2);
    CHECK(holds(programs, "PAD"));
    CHECK(holds(programs, "BASS"));
    CHECK(fixture.rig.model.memory().current(MemoryKind::Program) == std::optional<std::string>("BASS"));
    CHECK(fixture.list(MemoryKind::Sample).names == std::vector<std::string>{"KICK"});
    for (const MemoryKind kind : {MemoryKind::Multi, MemoryKind::SongFile, MemoryKind::SetList, MemoryKind::SceneList})
        CHECK(fixture.list(kind).names.empty());
    CHECK(app::memoryHeading(MemoryKind::Program) == "Programs");
    CHECK(app::memoryHeading(MemoryKind::SceneList) == "Scenelists");
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given a program loaded with its samples from the browser, When the load is done, Then the programs and the samples are read again "
          "and the loaded program is selected and current [RQ-APP-036, RQ-APP-028, RQ-APP-037]",
          "[app][memory]")
{
    MemoryRig fixture;
    fixture.browser.selectDisk(0);
    REQUIRE(fixture.rig.settle());

    fixture.browser.load(BrowserItem{BrowserItemKind::File, {}, "LEAD.AKP"}, true, mcp::SampleLoadMode::Normal);
    REQUIRE(fixture.rig.settle());

    CHECK(holds(fixture.list(MemoryKind::Program), "LEAD"));
    CHECK(holds(fixture.list(MemoryKind::Sample), "SAW"));
    CHECK(fixture.rig.model.memory().selection == std::optional<MemoryItem>(MemoryItem{MemoryKind::Program, "LEAD"}));
    CHECK(fixture.rig.model.memory().current(MemoryKind::Program) == std::optional<std::string>("LEAD"));
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given a sample deleted on the sampler's front panel, When the person refreshes the memory view, Then it is no longer listed "
          "[RQ-APP-036]",
          "[app][memory]")
{
    MemoryRig fixture;
    REQUIRE(holds(fixture.list(MemoryKind::Sample), "KICK"));

    fixture.rig.sampler->setSampleNames({});
    fixture.memory.refresh();
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.list(MemoryKind::Sample).names.empty());
}

TEST_CASE("Given PAD and BASS with BASS current, When the person selects PAD, Then PAD is selected by name on the sampler and is current; "
          "When the select is refused, Then the selection goes back to the sampler's current item and the error is reported [RQ-APP-037, "
          "RQ-APP-010]",
          "[app][memory]")
{
    MemoryRig fixture;
    const std::size_t before = fixture.selectsSent();

    fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.selectsSent() == before + 1);
    CHECK(fixture.rig.model.memory().selection == std::optional<MemoryItem>(MemoryItem{MemoryKind::Program, "PAD"}));
    CHECK(fixture.rig.model.memory().current(MemoryKind::Program) == std::optional<std::string>("PAD"));
    CHECK_FALSE(fixture.rig.model.problem().has_value());

    akm::harness::SamplerBehaviour behaviour;
    behaviour.itemErrors.push_back(akm::harness::ItemError{SECTION_PROGRAM, ITEM_SELECT_BY_NAME, ERROR_NOT_FOUND});
    fixture.rig.sampler->setBehaviour(behaviour);
    fixture.memory.select(MemoryItem{MemoryKind::Program, "BASS"});
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.rig.model.memory().selection == std::optional<MemoryItem>(MemoryItem{MemoryKind::Program, "PAD"}));
    CHECK(fixture.rig.model.memory().current(MemoryKind::Program) == std::optional<std::string>("PAD"));
    REQUIRE(fixture.rig.model.problem().has_value());
    CHECK(fixture.rig.model.problem()->action.find("BASS") != std::string::npos);
}

TEST_CASE("Given a set list, When the person selects it, Then it is selected in the view and nothing is sent, the sampler having no current "
          "set list [RQ-APP-037]",
          "[app][memory]")
{
    MemoryRig fixture;
    fixture.rig.sampler->setSetListNames({"GIG"});
    fixture.memory.refresh();
    REQUIRE(fixture.rig.settle());
    REQUIRE(fixture.list(MemoryKind::SetList).names == std::vector<std::string>{"GIG"});
    const std::size_t before = fixture.rig.sampler->acceptedCommands().size();

    fixture.memory.select(MemoryItem{MemoryKind::SetList, "GIG"});
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.rig.model.memory().selection == std::optional<MemoryItem>(MemoryItem{MemoryKind::SetList, "GIG"}));
    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
    CHECK_FALSE(fixture.list(MemoryKind::SetList).current.has_value());
}

TEST_CASE("Given 8 MB of wave memory with 2 MB free, When the session opens, Then the view gives 2 MB free of 8 MB and the free percentages; "
          "When a file is loaded, Then the figures are read again [RQ-APP-042]",
          "[app][memory]")
{
    MemoryRig fixture;

    const auto& free = fixture.rig.model.memory().free;
    REQUIRE(free.has_value());
    CHECK(free->freeWaveBytes == WAVE_FREE);
    CHECK(free->totalWaveBytes == WAVE_TOTAL);
    CHECK(free->freeWavePercent == 25);
    CHECK(free->freeMpksPercent == MPKS_FREE_PERCENT);

    fixture.rig.sampler->setMemory(WAVE_TOTAL, MEGABYTE, MPKS_FREE_PERCENT);
    fixture.browser.selectDisk(0);
    fixture.browser.load(BrowserItem{BrowserItemKind::File, {}, "SAW.WAV"}, false, mcp::SampleLoadMode::Normal);
    REQUIRE(fixture.rig.settle());

    REQUIRE(fixture.rig.model.memory().free.has_value());
    CHECK(fixture.rig.model.memory().free->freeWaveBytes == MEGABYTE);
}

TEST_CASE("Given the memory view filled, When the person disconnects, Then it is emptied [RQ-APP-019, RQ-APP-035]", "[app][memory]")
{
    MemoryRig fixture;

    fixture.rig.controller.disconnect();
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.list(MemoryKind::Program).names.empty());
    CHECK_FALSE(fixture.rig.model.memory().free.has_value());
}
