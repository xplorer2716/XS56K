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

// The core of the editor application over the gateway and the simulated sampler: the window's thread never waits for the sampler, a
// failure is reported with what failed and why, a destructive action sends nothing until it is confirmed, and a slow disk command waits
// for the disk timeout. [TASK-APP-011, RQ-APP-010, RQ-APP-011, RQ-APP-012, RQ-APP-013, RQ-APP-014, ADR-APP-001 (DEC-APP-003,
// DEC-APP-004, DEC-APP-007)]
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "AppTestSupport.hpp"
#include "app/DesignTokens.generated.hpp"
#include "akm/harness/SimulatedSampler.hpp"

using app::Confirmation;
using app::ModelPart;
using app::test::CoreRig;
using akm::harness::DiskRecord;
using akm::harness::FileRecord;
using akm::harness::SamplerBehaviour;

namespace
{
    constexpr std::uint8_t SECTION_PROGRAM = 0x0A;
    constexpr std::uint8_t ITEM_CREATE_PROGRAM_WITH_KEYGROUPS = 0x03;
    constexpr std::uint8_t SECTION_DISK = 0x10;
    constexpr std::uint8_t ITEM_LOAD_FILE = 0x2A;
    constexpr std::uint16_t ERROR_OUT_OF_RANGE = 2;
    constexpr std::chrono::milliseconds HELD_ANSWER{300};
    constexpr std::chrono::milliseconds LONG_COMMAND_TIMEOUT{3000};
    constexpr std::chrono::milliseconds AT_ONCE{100};
    constexpr std::chrono::milliseconds SLOWER_THAN_A_COMMAND{700};

    bool contains(const std::string& text, const std::string& part)
    {
        return text.find(part) != std::string::npos;
    }

    /// One hard disk HD1 whose root holds INIT.AKP, a program INIT.
    std::vector<DiskRecord> oneDiskWithAProgram()
    {
        DiskRecord disk;
        disk.handle = 0;
        disk.type = 1;
        disk.format = 1;
        disk.writable = true;
        disk.name = "HD1";
        disk.freeBytes = 1000000;
        disk.rootFolder.files = {FileRecord{"INIT.AKP", 4000, std::string("INIT"), std::nullopt, {}}};
        return {disk};
    }

    /// A model listener that counts what it is told.
    struct Counter final : app::ModelListener
    {
        void modelChanged(ModelPart part) override { parts.push_back(part); }
        std::vector<ModelPart> parts;
    };
}

TEST_CASE("Given the simulated sampler holding its answer, When the controller queues a job, Then the call returns at once, the model names "
          "the job, and the UI thread's work completes before the answer is released [RQ-APP-012]",
          "[app][core]")
{
    CoreRig rig(LONG_COMMAND_TIMEOUT);
    rig.sampler->setProgramNames({"BASS", "PAD"});
    REQUIRE(rig.gateway.status().ok());  // the session is open before the sampler starts holding its answers
    SamplerBehaviour behaviour;
    behaviour.replyDelay = HELD_ANSWER;
    rig.sampler->setBehaviour(behaviour);
    Counter counter;
    rig.model.addListener(counter);
    std::optional<int> programs;

    const auto started = std::chrono::steady_clock::now();
    rig.controller.run<mcp::SamplerStatus>("Reading the sampler's status", [&rig] { return rig.gateway.status(); },
                                           [&programs](const mcp::SamplerStatus& status) { programs = status.programCount; });
    const auto returnedAfter = std::chrono::steady_clock::now() - started;

    CHECK(returnedAfter < AT_ONCE);
    REQUIRE(rig.model.busy().has_value());
    CHECK(*rig.model.busy() == "Reading the sampler's status");
    CHECK(counter.parts == std::vector<ModelPart>{ModelPart::Busy});
    // The UI thread's work: nothing waits for it, and the answer has not come yet.
    CHECK(rig.dispatcher.drain() == 0);
    CHECK_FALSE(rig.controller.idle());
    CHECK_FALSE(programs.has_value());

    REQUIRE(rig.settle());
    CHECK(programs == 2);
    CHECK_FALSE(rig.model.busy().has_value());
    CHECK_FALSE(rig.model.problem().has_value());
    rig.model.removeListener(counter);
}

TEST_CASE("Given two jobs queued, When the first ends, Then the model names the second until it ends too [RQ-APP-012]", "[app][core]")
{
    CoreRig rig;
    rig.sampler->setProgramNames({"BASS", "PAD"});
    std::vector<std::string> busyLabels;
    rig.controller.run<mcp::SamplerStatus>("First job", [&rig] { return rig.gateway.status(); },
                                           [&rig, &busyLabels](const mcp::SamplerStatus&) { busyLabels.push_back(rig.model.busy().value_or("")); });
    rig.controller.run<mcp::SamplerStatus>("Second job", [&rig] { return rig.gateway.status(); },
                                           [&rig, &busyLabels](const mcp::SamplerStatus&) { busyLabels.push_back(rig.model.busy().value_or("")); });
    CHECK(rig.model.busy() == std::optional<std::string>("First job"));

    REQUIRE(rig.settle());
    CHECK(busyLabels == std::vector<std::string>{"Second job", ""});
    CHECK_FALSE(rig.model.busy().has_value());
}

TEST_CASE("Given the simulated sampler answering ERROR \"parameter out of range\", When a job sends that command, Then the model says which "
          "action failed and why in the sampler's words, and keeps what it had read [RQ-APP-010]",
          "[app][core]")
{
    CoreRig rig;
    rig.sampler->setProgramNames({"BASS", "PAD"});
    SamplerBehaviour behaviour;
    behaviour.itemErrors.push_back(akm::harness::ItemError{SECTION_PROGRAM, ITEM_CREATE_PROGRAM_WITH_KEYGROUPS, ERROR_OUT_OF_RANGE});
    rig.sampler->setBehaviour(behaviour);
    bool applied = false;

    rig.controller.run<mcp::ProgramInfo>("Creating the program LEAD", [&rig] { return rig.gateway.createProgram("LEAD", 2); },
                                         [&applied](const mcp::ProgramInfo&) { applied = true; });
    REQUIRE(rig.settle());

    CHECK_FALSE(applied);
    REQUIRE(rig.model.problem().has_value());
    CHECK(rig.model.problem()->action == "Creating the program LEAD");
    CHECK(contains(rig.model.problem()->reason, "parameter out of range"));
    CHECK_FALSE(rig.model.busy().has_value());

    rig.model.clearProblem();
    CHECK_FALSE(rig.model.problem().has_value());
}

TEST_CASE("Given a destructive action, When the person cancels its confirmation, Then nothing is sent; When the person confirms, Then the "
          "job runs [RQ-APP-011]",
          "[app][core]")
{
    CoreRig rig;
    rig.sampler->setProgramNames({"BASS", "PAD"});
    REQUIRE(rig.gateway.selectProgramByName("PAD").ok());
    const Confirmation question{"Delete the program?", "PAD", "The program PAD is removed from the sampler's memory."};
    const auto deletePad = [&rig] { return rig.gateway.deleteCurrentProgram("PAD"); };
    std::optional<bool> deleted;
    const auto record = [&deleted](const mcp::ProgramDeletion& deletion) { deleted = deletion.deleted; };

    const std::size_t sentBefore = rig.sampler->acceptedCommands().size();
    rig.prompts.answers = {false};
    rig.controller.runConfirmed<mcp::ProgramDeletion>(question, "Deleting the program PAD", deletePad, record);
    REQUIRE(rig.settle());

    REQUIRE(rig.prompts.asked.size() == 1);
    CHECK(rig.prompts.asked.front().object == "PAD");
    CHECK(rig.sampler->acceptedCommands().size() == sentBefore);
    CHECK_FALSE(rig.model.busy().has_value());
    CHECK_FALSE(deleted.has_value());

    rig.prompts.answers = {true};
    rig.controller.runConfirmed<mcp::ProgramDeletion>(question, "Deleting the program PAD", deletePad, record);
    REQUIRE(rig.settle());

    CHECK(deleted == true);
    CHECK(rig.sampler->acceptedCommands().size() > sentBefore);
}

TEST_CASE("Given the sampler answering a load after the ordinary timeout and before the disk timeout, When a job loads a file, Then the "
          "load succeeds [RQ-APP-013]",
          "[app][core][disk]")
{
    CoreRig rig;
    rig.sampler->setDisks(oneDiskWithAProgram());
    REQUIRE(rig.gateway.selectDiskByName("HD1").ok());
    SamplerBehaviour behaviour;
    behaviour.slowItems.push_back(akm::harness::SlowItem{SECTION_DISK, ITEM_LOAD_FILE, SLOWER_THAN_A_COMMAND});
    rig.sampler->setBehaviour(behaviour);
    REQUIRE(SLOWER_THAN_A_COMMAND > app::test::TEST_COMMAND_TIMEOUT);
    REQUIRE(SLOWER_THAN_A_COMMAND < app::test::TEST_DISK_TIMEOUT);
    std::vector<std::string> programsAfter;

    rig.controller.run<mcp::LoadOutcome>("Loading INIT.AKP",
                                         [&rig] { return rig.gateway.loadFile("INIT.AKP", false, mcp::SampleLoadMode::Normal); },
                                         [&programsAfter](const mcp::LoadOutcome& outcome) { programsAfter = outcome.after.programs; });
    CHECK(rig.model.busy() == std::optional<std::string>("Loading INIT.AKP"));
    REQUIRE(rig.settle());

    CHECK_FALSE(rig.model.problem().has_value());
    CHECK(programsAfter == std::vector<std::string>{"INIT"});
}

TEST_CASE("Given a sampler that never answers a load, When a job loads a file, Then the model says the sampler may need to be switched off "
          "and on [RQ-APP-013]",
          "[app][core][disk]")
{
    CoreRig rig;
    rig.sampler->setDisks(oneDiskWithAProgram());
    REQUIRE(rig.gateway.selectDiskByName("HD1").ok());
    SamplerBehaviour behaviour;
    behaviour.silentItems.push_back(akm::harness::SilentItem{SECTION_DISK, ITEM_LOAD_FILE});
    rig.sampler->setBehaviour(behaviour);

    rig.controller.run<mcp::LoadOutcome>("Loading INIT.AKP",
                                         [&rig] { return rig.gateway.loadFile("INIT.AKP", false, mcp::SampleLoadMode::Normal); },
                                         [](const mcp::LoadOutcome&) {});
    REQUIRE(rig.settle());

    REQUIRE(rig.model.problem().has_value());
    CHECK(rig.model.problem()->action == "Loading INIT.AKP");
    CHECK(contains(rig.model.problem()->reason, "switched off and on"));
}

TEST_CASE("Given the token module, Then the type scale is two pixels larger than the first one in every role [RQ-APP-084, TASK-APP-030]", "[app][design]")
{
    CHECK(app::tokens::kFontCaptionSize == 13.0f);
    CHECK(app::tokens::kFontLabelSize == 13.0f);
    CHECK(app::tokens::kFontValueSize == 15.0f);
    CHECK(app::tokens::kFontTitleSize == 17.0f);
    CHECK(app::tokens::kFontHeadingSize == 20.0f);
}
