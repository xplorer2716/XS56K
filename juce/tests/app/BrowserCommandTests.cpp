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

// The commands of the file browser over the gateway and the simulated sampler: loading a file or a folder, creating, renaming and deleting,
// auditioning a sample from the disk and refreshing the list of disks, each refusal or cancel sending nothing. [TASK-APP-014, RQ-APP-011,
// RQ-APP-013, RQ-APP-028 to RQ-APP-034, ADR-APP-001 (DEC-APP-005, DEC-APP-007)]
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "AppTestSupport.hpp"
#include "app/Browser.hpp"
#include "app/FileBrowser.hpp"
#include "akm/harness/SimulatedSampler.hpp"

using akm::harness::DiskRecord;
using akm::harness::FileRecord;
using akm::harness::FolderRecord;
using app::BrowserItem;
using app::BrowserItemKind;
using app::ConnectionState;
using app::MemoryItem;
using app::MemoryKind;
using app::test::CoreRig;
using app::test::setupA;

namespace
{
    constexpr std::uint8_t SECTION_DISK = 0x10;
    constexpr std::uint8_t ITEM_LOAD_FOLDER = 0x15;
    constexpr std::uint8_t ITEM_CREATE_FOLDER = 0x16;
    constexpr std::uint8_t ITEM_DELETE_SUB_FOLDER = 0x17;
    constexpr std::uint8_t ITEM_LOAD_FILE = 0x2A;
    constexpr std::uint8_t ITEM_LOAD_FILE_WITH_DEPENDENTS = 0x2B;
    constexpr std::uint8_t ITEM_START_FILE_AUDITION = 0x30;
    constexpr std::uint8_t ITEM_STOP_FILE_AUDITION = 0x31;

    /// HD1, whose root holds PAD.AKP (a program PAD using KICK.WAV), KICK.WAV (a sample KICK), SNARE.WAV, the folder DRUMS (holding the
    /// folder LOOPS), the folder KIT (a program KITPROG and the sub-folder EXTRA with a program EXTRAPROG) and the empty folder OLD.
    std::vector<DiskRecord> oneDisk()
    {
        FolderRecord loops;
        loops.name = "LOOPS";
        FolderRecord drums;
        drums.name = "DRUMS";
        drums.subFolders = {loops};
        FolderRecord extra;
        extra.name = "EXTRA";
        extra.programFiles = {"EXTRAPROG"};
        FolderRecord kit;
        kit.name = "KIT";
        kit.programFiles = {"KITPROG"};
        kit.subFolders = {extra};
        FolderRecord old;
        old.name = "OLD";

        DiskRecord hard;
        hard.handle = 0;
        hard.name = "HD1";
        hard.type = 1;
        hard.format = 1;
        hard.writable = true;
        hard.freeBytes = 1000000;
        hard.rootFolder.subFolders = {drums, kit, old};
        hard.rootFolder.files = {FileRecord{"PAD.AKP", 4096, std::string("PAD"), std::nullopt, {"KICK.WAV"}},
                                 FileRecord{"KICK.WAV", 150000, std::nullopt, std::string("KICK"), {}},
                                 FileRecord{"SNARE.WAV", 120000, std::nullopt, std::string("SNARE"), {}}};
        return {hard};
    }

    /// Connected, with the browser on HD1's root.
    struct CommandRig
    {
        CommandRig()
        {
            rig.sampler->setDisks(oneDisk());
            rig.controller.connect(setupA());
            REQUIRE(rig.settle());
            REQUIRE(rig.model.connection().state == ConnectionState::Connected);
            browser.selectDisk(0);
            REQUIRE(rig.settle());
        }

        [[nodiscard]] std::size_t sent(std::uint8_t item) const
        {
            const auto commands = rig.sampler->acceptedCommands();
            return static_cast<std::size_t>(std::count_if(commands.begin(), commands.end(), [item](const akm::harness::AcceptedCommand& command) {
                return command.section == SECTION_DISK && command.item == item;
            }));
        }

        [[nodiscard]] bool rootHas(const std::string& name) const
        {
            const app::FolderListing* root = rig.model.browser().listing({});
            if (root == nullptr)
                return false;
            const bool folder = std::find(root->folders.begin(), root->folders.end(), name) != root->folders.end();
            const bool file = std::any_of(root->files.begin(), root->files.end(), [&name](const mcp::DiskFileEntry& entry) { return entry.name == name; });
            return folder || file;
        }

        CoreRig rig;
        app::FileBrowser browser{rig.model, rig.controller};
    };

    BrowserItem rootFile(const std::string& name)
    {
        return BrowserItem{BrowserItemKind::File, {}, name};
    }

    BrowserItem rootFolder(const std::string& name)
    {
        return BrowserItem{BrowserItemKind::Folder, {}, name};
    }
}

TEST_CASE("Given PAD.AKP that uses KICK.WAV, When it is loaded with its dependents, Then the load with dependents is sent and the memory view "
          "is told to select the program PAD; When it is loaded alone, Then the plain load is sent [RQ-APP-028]",
          "[app][browser][load]")
{
    CommandRig fixture;

    fixture.browser.load(rootFile("PAD.AKP"), true, mcp::SampleLoadMode::Normal);
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.sent(ITEM_LOAD_FILE_WITH_DEPENDENTS) == 1);
    CHECK(fixture.sent(ITEM_LOAD_FILE) == 0);
    CHECK(fixture.rig.model.lastMemoryChange().number == 1);
    CHECK(fixture.rig.model.lastMemoryChange().select == std::optional<MemoryItem>(MemoryItem{MemoryKind::Program, "PAD"}));
    CHECK_FALSE(fixture.rig.model.problem().has_value());

    CommandRig alone;
    alone.browser.load(rootFile("PAD.AKP"), false, mcp::SampleLoadMode::Normal);
    REQUIRE(alone.rig.settle());
    CHECK(alone.sent(ITEM_LOAD_FILE) == 1);
    CHECK(alone.sent(ITEM_LOAD_FILE_WITH_DEPENDENTS) == 0);
    CHECK(alone.rig.model.lastMemoryChange().select == std::optional<MemoryItem>(MemoryItem{MemoryKind::Program, "PAD"}));
}

TEST_CASE("Given a file in a folder other than the current one, When it is loaded, Then the sampler goes to its folder first [RQ-APP-028, "
          "RQ-APP-025]",
          "[app][browser][load]")
{
    CommandRig fixture;
    fixture.browser.openFolder({"DRUMS", "LOOPS"});
    REQUIRE(fixture.rig.settle());

    fixture.browser.load(rootFile("KICK.WAV"), false, mcp::SampleLoadMode::Ram);
    REQUIRE(fixture.rig.settle());

    CHECK_FALSE(fixture.rig.model.problem().has_value());
    CHECK(fixture.rig.model.lastMemoryChange().select == std::optional<MemoryItem>(MemoryItem{MemoryKind::Sample, "KICK"}));
    CHECK(fixture.rig.model.browser().currentFolder.empty());
}

TEST_CASE("Given the folder KIT with a sub-folder EXTRA, When the person loads KIT and cancels, Then nothing is sent; When the person "
          "confirms, Then the folder load is sent and the memory view is told to read the memory again [RQ-APP-029, RQ-APP-011]",
          "[app][browser][load]")
{
    CommandRig fixture;
    const std::size_t before = fixture.rig.sampler->acceptedCommands().size();

    fixture.rig.prompts.answers = {false};
    fixture.browser.loadFolder(rootFolder("KIT"));
    REQUIRE(fixture.rig.settle());

    REQUIRE(fixture.rig.prompts.asked.size() == 1);
    CHECK(fixture.rig.prompts.asked[0].object == "KIT");
    CHECK(fixture.rig.prompts.asked[0].consequence.find("sub-folders") != std::string::npos);
    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
    CHECK(fixture.rig.model.lastMemoryChange().number == 0);

    fixture.rig.prompts.answers = {true};
    fixture.browser.loadFolder(rootFolder("KIT"));
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.sent(ITEM_LOAD_FOLDER) == 1);
    CHECK(fixture.rig.model.lastMemoryChange().number == 1);
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given DRUMS holding LOOPS, When the person creates NEW KIT in DRUMS, Then it appears under DRUMS; When the person creates LOOPS, "
          "which exists, Then nothing is sent and the reason is shown [RQ-APP-030]",
          "[app][browser]")
{
    CommandRig fixture;
    fixture.browser.openFolder({"DRUMS"});
    REQUIRE(fixture.rig.settle());

    fixture.browser.createFolder({"DRUMS"}, "NEW KIT");
    REQUIRE(fixture.rig.settle());
    const app::FolderListing* drums = fixture.rig.model.browser().listing({"DRUMS"});
    REQUIRE(drums != nullptr);
    CHECK(std::find(drums->folders.begin(), drums->folders.end(), "NEW KIT") != drums->folders.end());
    CHECK(fixture.sent(ITEM_CREATE_FOLDER) == 1);

    fixture.browser.createFolder({"DRUMS"}, "LOOPS");
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_CREATE_FOLDER) == 1);
    REQUIRE(fixture.rig.model.problem().has_value());
    CHECK(fixture.rig.model.problem()->reason.find("LOOPS") != std::string::npos);
}

TEST_CASE("Given KICK.WAV, When the person renames it KICK 2, Then the tree shows KICK 2.WAV; When the person renames it SNARE, which a "
          "file of the folder bears, Then nothing is sent [RQ-APP-031]",
          "[app][browser]")
{
    CommandRig fixture;

    fixture.browser.rename(rootFile("KICK.WAV"), "KICK 2");
    REQUIRE(fixture.rig.settle());
    CHECK_FALSE(fixture.rig.model.problem().has_value());
    CHECK(fixture.rootHas("KICK 2.WAV"));
    CHECK_FALSE(fixture.rootHas("KICK.WAV"));

    const std::size_t before = fixture.rig.sampler->acceptedCommands().size();
    fixture.browser.rename(rootFile("KICK 2.WAV"), "SNARE");
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.rig.model.problem().has_value());
    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
}

TEST_CASE("Given the folder OLD, When the person deletes it and cancels, Then nothing is sent; When the person confirms, Then OLD is no "
          "longer in the tree [RQ-APP-032, RQ-APP-011]",
          "[app][browser]")
{
    CommandRig fixture;
    const std::size_t before = fixture.rig.sampler->acceptedCommands().size();

    fixture.rig.prompts.answers = {false};
    fixture.browser.remove(rootFolder("OLD"));
    REQUIRE(fixture.rig.settle());
    REQUIRE(fixture.rig.prompts.asked.size() == 1);
    CHECK(fixture.rig.prompts.asked[0].object == "OLD");
    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
    CHECK(fixture.rootHas("OLD"));

    fixture.rig.prompts.answers = {true};
    fixture.browser.remove(rootFolder("OLD"));
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_DELETE_SUB_FOLDER) == 1);
    CHECK_FALSE(fixture.rootHas("OLD"));
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given KICK.WAV selected, When the person auditions it, Then the audition from the disk starts; When the person selects another "
          "file, Then the audition stops [RQ-APP-033]",
          "[app][browser]")
{
    CommandRig fixture;
    fixture.browser.select(rootFile("KICK.WAV"));

    fixture.browser.audition(rootFile("KICK.WAV"));
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_START_FILE_AUDITION) == 1);
    CHECK(fixture.sent(ITEM_LOAD_FILE) == 0);

    fixture.browser.select(rootFile("SNARE.WAV"));
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_STOP_FILE_AUDITION) == 1);

    fixture.browser.select(rootFile("PAD.AKP"));
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_STOP_FILE_AUDITION) == 1);  // nothing was playing any more
}
