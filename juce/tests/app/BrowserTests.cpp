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

// The file browser of the editor application, read side, over the gateway and the simulated sampler: the disks once the session opens, a
// disk selected with its format and free space, folders opened (the sampler's current folder and its path), what a file is, and the
// commands each selection offers. [TASK-APP-013, RQ-APP-017, RQ-APP-024 to RQ-APP-027, ADR-APP-001 (DEC-APP-004)]
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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
using app::BrowserCommand;
using app::BrowserItem;
using app::BrowserItemKind;
using app::ConnectionState;
using app::FileKind;
using app::test::CoreRig;
using app::test::setupA;

namespace
{
    constexpr int HARD_DISK = 1;
    constexpr int CD_ROM = 2;
    constexpr int MSDOS = 1;
    constexpr int ISO9660 = 3;
    constexpr std::uint64_t HD1_FREE_BYTES = 1000000;

    FileRecord file(const std::string& name, std::uint32_t size)
    {
        return FileRecord{name, size, std::nullopt, std::nullopt, {}};
    }

    /// HD1, a hard disk in MSDOS with 1 000 000 bytes free, whose root holds PAD.AKP, the folder DRUMS (KICK.WAV and the folder LOOPS) and
    /// the folder SYNTH; CD1, a CD-ROM in ISO9660 that reports 0 bytes free.
    std::vector<DiskRecord> twoDisks()
    {
        FolderRecord loops;
        loops.name = "LOOPS";
        FolderRecord drums;
        drums.name = "DRUMS";
        drums.subFolders = {loops};
        drums.files = {file("KICK.WAV", 150000)};
        FolderRecord synth;
        synth.name = "SYNTH";

        DiskRecord hard;
        hard.handle = 0;
        hard.name = "HD1";
        hard.type = HARD_DISK;
        hard.format = MSDOS;
        hard.writable = true;
        hard.freeBytes = HD1_FREE_BYTES;
        hard.rootFolder.subFolders = {drums, synth};
        hard.rootFolder.files = {file("PAD.AKP", 4096)};

        DiskRecord cd;
        cd.handle = 1;
        cd.name = "CD1";
        cd.type = CD_ROM;
        cd.format = ISO9660;
        cd.writable = false;
        cd.freeBytes = 0;
        return {hard, cd};
    }

    /// The rig, its sampler holding the two disks, the browser and a connection.
    struct BrowserRig
    {
        BrowserRig()
        {
            rig.sampler->setDisks(twoDisks());
            rig.controller.connect(setupA());
            REQUIRE(rig.settle());
            REQUIRE(rig.model.connection().state == ConnectionState::Connected);
        }

        CoreRig rig;
        app::FileBrowser browser{rig.model, rig.controller};
    };

    bool offers(const std::vector<BrowserCommand>& commands, BrowserCommand command)
    {
        return std::find(commands.begin(), commands.end(), command) != commands.end();
    }
}

TEST_CASE("Given the simulated sampler with two disks, When the session opens, Then the browser has two roots with their names and types "
          "[RQ-APP-024, RQ-APP-017]",
          "[app][browser]")
{
    CoreRig rig;
    rig.sampler->setDisks(twoDisks());
    app::FileBrowser browser(rig.model, rig.controller);

    rig.controller.connect(setupA());
    REQUIRE(rig.settle());

    const app::BrowserState& state = rig.model.browser();
    REQUIRE(state.disks.size() == 2);
    CHECK(state.disks[0].name == "HD1");
    CHECK(app::diskTypeName(state.disks[0].type) == "hard disk");
    CHECK(state.disks[1].name == "CD1");
    CHECK(app::diskTypeName(state.disks[1].type) == "CD-ROM");
}

TEST_CASE("Given two disks, When the person selects one, Then it becomes the sampler's current disk with its format and free space, and a "
          "free space of 0 reads \"not reported\" [RQ-APP-024]",
          "[app][browser]")
{
    BrowserRig fixture;
    CoreRig& rig = fixture.rig;

    fixture.browser.selectDisk(0);
    REQUIRE(rig.settle());
    const app::BrowserState& onHard = rig.model.browser();
    REQUIRE(onHard.currentDisk == 0);
    REQUIRE(onHard.currentDiskEntry() != nullptr);
    CHECK(app::diskFormatName(onHard.currentDiskEntry()->format) == "MSDOS");
    REQUIRE(onHard.freeBytes.has_value());
    CHECK(app::freeSpaceText(*onHard.freeBytes) == "976.6 KB");  // in the unit of RQ-APP-081 since TASK-APP-020
    CHECK(onHard.currentFolder.empty());
    REQUIRE(onHard.listing({}) != nullptr);
    CHECK(onHard.listing({})->folders == std::vector<std::string>{"DRUMS", "SYNTH"});

    fixture.browser.selectDisk(1);
    REQUIRE(rig.settle());
    const app::BrowserState& onCd = rig.model.browser();
    CHECK(onCd.currentDisk == 1);
    CHECK(app::diskFormatName(onCd.currentDiskEntry()->format) == "ISO9660");
    REQUIRE(onCd.freeBytes.has_value());
    CHECK(app::freeSpaceText(*onCd.freeBytes) == "not reported");
    CHECK_FALSE(rig.model.problem().has_value());
}

TEST_CASE("Given a disk with a folder DRUMS holding KICK.WAV and a folder LOOPS, When the person opens DRUMS, Then LOOPS and KICK.WAV are "
          "under it, DRUMS is the sampler's current folder and the path says so [RQ-APP-025]",
          "[app][browser]")
{
    BrowserRig fixture;
    CoreRig& rig = fixture.rig;
    fixture.browser.selectDisk(0);
    REQUIRE(rig.settle());

    fixture.browser.openFolder({"DRUMS"});
    REQUIRE(rig.settle());

    const app::BrowserState& state = rig.model.browser();
    CHECK(state.currentFolder == std::vector<std::string>{"DRUMS"});
    CHECK(app::pathText(state.currentFolder) == "DRUMS");
    const app::FolderListing* drums = state.listing({"DRUMS"});
    REQUIRE(drums != nullptr);
    CHECK(drums->folders == std::vector<std::string>{"LOOPS"});
    REQUIRE(drums->files.size() == 1);
    CHECK(drums->files[0].name == "KICK.WAV");
    CHECK(state.listing({}) != nullptr);  // the root stays in the tree
}

TEST_CASE("Given DRUMS\\LOOPS open, When the person opens SYNTH at the root, Then the sampler goes up to the root and into SYNTH, which is "
          "the current folder [RQ-APP-025]",
          "[app][browser]")
{
    BrowserRig fixture;
    CoreRig& rig = fixture.rig;
    fixture.browser.selectDisk(0);
    fixture.browser.openFolder({"DRUMS", "LOOPS"});
    REQUIRE(rig.settle());
    REQUIRE(rig.model.browser().currentFolder == std::vector<std::string>{"DRUMS", "LOOPS"});
    CHECK(app::pathText(rig.model.browser().currentFolder) == "DRUMS\\LOOPS");

    fixture.browser.openFolder({"SYNTH"});
    REQUIRE(rig.settle());

    CHECK(rig.model.browser().currentFolder == std::vector<std::string>{"SYNTH"});
    CHECK(rig.model.browser().listing({"SYNTH"}) != nullptr);
    CHECK_FALSE(rig.model.problem().has_value());
}

TEST_CASE("Given a folder that is not there, When the person opens it, Then the problem is reported and the current folder stays [RQ-APP-025, "
          "RQ-APP-010]",
          "[app][browser]")
{
    BrowserRig fixture;
    CoreRig& rig = fixture.rig;
    fixture.browser.selectDisk(0);
    REQUIRE(rig.settle());

    fixture.browser.openFolder({"NOWHERE"});
    REQUIRE(rig.settle());

    CHECK(rig.model.problem().has_value());
    CHECK(rig.model.browser().currentFolder.empty());
}

TEST_CASE("Given PAD.AKP of 4 096 bytes, When it is selected, Then the browser gives \"PAD.AKP\", 4 096 bytes and \"program\"; Given "
          "README.TXT, Then its kind is \"unknown\" [RQ-APP-026]",
          "[app][browser]")
{
    BrowserRig fixture;
    CoreRig& rig = fixture.rig;
    fixture.browser.selectDisk(0);
    REQUIRE(rig.settle());

    fixture.browser.select(BrowserItem{BrowserItemKind::File, {}, "PAD.AKP"});

    const app::BrowserState& state = rig.model.browser();
    REQUIRE(state.selection.has_value());
    const auto details = state.selectedFile();
    REQUIRE(details.has_value());
    CHECK(details->name == "PAD.AKP");
    CHECK(app::bytesText(details->sizeBytes) == "4 096 bytes");
    CHECK(app::fileKindName(details->kind) == "program");

    CHECK(app::fileKindOf("README.TXT") == FileKind::Unknown);
    CHECK(app::fileKindOf("KICK.WAV") == FileKind::Sample);
    CHECK(app::fileKindOf("LIVE.AKM") == FileKind::Multi);
    CHECK(app::fileKindOf("SONG.MID") == FileKind::SongFile);
    CHECK(app::fileKindOf("GIG.SET") == FileKind::SetList);
    CHECK(app::fileKindOf("SCENES.ASC") == FileKind::SceneList);
    CHECK(app::fileKindOf("pad.akp") == FileKind::Program);
    CHECK(app::fileKindOf("NOEXTENSION") == FileKind::Unknown);
}

TEST_CASE("Given a selection, When its context menu opens, Then only the commands that apply are offered: audition for a sample file only, "
          "the folder commands without Open for a folder, only create a folder for a disk [RQ-APP-027, RQ-APP-034]",
          "[app][browser]")
{
    const auto program = app::commandsFor(BrowserItem{BrowserItemKind::File, {}, "PAD.AKP"});
    CHECK(offers(program, BrowserCommand::Load));
    CHECK(offers(program, BrowserCommand::LoadWithDependents));
    CHECK(offers(program, BrowserCommand::Rename));
    CHECK(offers(program, BrowserCommand::Delete));
    CHECK_FALSE(offers(program, BrowserCommand::Audition));

    const auto sample = app::commandsFor(BrowserItem{BrowserItemKind::File, {"DRUMS"}, "KICK.WAV"});
    CHECK(offers(sample, BrowserCommand::Audition));

    const auto folder = app::commandsFor(BrowserItem{BrowserItemKind::Folder, {}, "DRUMS"});
    // No "Open" (the double click opens) and no refresh of the disks (withdrawn). [RQ-APP-027, RQ-APP-034, TASK-APP-027]
    CHECK(folder == std::vector<BrowserCommand>{BrowserCommand::LoadFolder, BrowserCommand::CreateFolder, BrowserCommand::Rename,
                                                BrowserCommand::Delete});

    const auto disk = app::commandsFor(BrowserItem{BrowserItemKind::Disk, {}, "HD1"});
    CHECK(disk == std::vector<BrowserCommand>{BrowserCommand::CreateFolder});
}

TEST_CASE("Given the browser filled, When the person disconnects, Then the browser is emptied [RQ-APP-019, RQ-APP-024]", "[app][browser]")
{
    BrowserRig fixture;
    CoreRig& rig = fixture.rig;
    REQUIRE_FALSE(rig.model.browser().disks.empty());

    rig.controller.disconnect();
    REQUIRE(rig.settle());

    CHECK(rig.model.browser().disks.empty());
    CHECK_FALSE(rig.model.browser().currentDisk.has_value());
}

TEST_CASE("Given sizes from 0 bytes to 151 MB, When they are written, Then bytes under 1 KB, otherwise KB or MB with one decimal, 1 KB being 1 024 "
          "bytes, a value that rounds to 1024.0 going to the next unit [RQ-APP-081, RQ-APP-026, RQ-APP-042, TASK-APP-020]",
          "[app][browser]")
{
    CHECK(app::sizeText(0) == "0 bytes");
    CHECK(app::sizeText(512) == "512 bytes");
    CHECK(app::sizeText(1023) == "1023 bytes");
    CHECK(app::sizeText(1024) == "1.0 KB");
    CHECK(app::sizeText(4096) == "4.0 KB");
    CHECK(app::sizeText(117398) == "114.6 KB");
    CHECK(app::sizeText(1048575) == "1.0 MB");
    CHECK(app::sizeText(1048576) == "1.0 MB");
    CHECK(app::sizeText(158548694) == "151.2 MB");
    CHECK(app::sizeText(158281898) == "150.9 MB");  // 150.95 MB
    CHECK(app::freeSpaceText(0) == "not reported");
}

TEST_CASE("Given DRUMS opened, When the person folds it, Then the model says it is folded and nothing is sent; When the person opens it "
          "again, Then it is opened on the sampler, listed and no longer folded [RQ-APP-079, TASK-APP-021]",
          "[app][browser]")
{
    BrowserRig fixture;
    CoreRig& rig = fixture.rig;
    fixture.browser.selectDisk(0);
    fixture.browser.openFolder({"DRUMS"});
    REQUIRE(rig.settle());
    REQUIRE_FALSE(rig.model.browser().isFolded({"DRUMS"}));
    const std::size_t before = rig.sampler->acceptedCommands().size();

    fixture.browser.fold({"DRUMS"});
    REQUIRE(rig.settle());

    CHECK(rig.model.browser().isFolded({"DRUMS"}));
    CHECK(rig.sampler->acceptedCommands().size() == before);
    CHECK(rig.model.browser().listing({"DRUMS"}) != nullptr);  // what it holds is kept, only not shown

    fixture.browser.openFolder({"DRUMS"});
    REQUIRE(rig.settle());

    CHECK_FALSE(rig.model.browser().isFolded({"DRUMS"}));
    CHECK(rig.sampler->acceptedCommands().size() > before);
    CHECK(rig.model.browser().currentFolder == std::vector<std::string>{"DRUMS"});
    CHECK_FALSE(rig.model.problem().has_value());
}

TEST_CASE("Given DRUMS folded, When a folder under it becomes the sampler's current folder, Then DRUMS is unfolded so that the current "
          "folder is shown [RQ-APP-079, RQ-APP-025, TASK-APP-021]",
          "[app][browser]")
{
    BrowserRig fixture;
    CoreRig& rig = fixture.rig;
    fixture.browser.selectDisk(0);
    fixture.browser.openFolder({"DRUMS"});
    REQUIRE(rig.settle());
    fixture.browser.fold({"DRUMS"});
    REQUIRE(rig.model.browser().isFolded({"DRUMS"}));

    fixture.browser.openFolder({"DRUMS", "LOOPS"});
    REQUIRE(rig.settle());

    CHECK_FALSE(rig.model.browser().isFolded({"DRUMS"}));
    CHECK(rig.model.browser().currentFolder == std::vector<std::string>{"DRUMS", "LOOPS"});
}

TEST_CASE("Given a session with no disk selected, When the root of the current disk is opened, Then the problem tells the person to select a disk "
          "in the application's words, with no MCP tool name [RQ-APP-082, TASK-APP-023]",
          "[app][browser]")
{
    BrowserRig fixture;
    CoreRig& rig = fixture.rig;
    REQUIRE_FALSE(rig.model.browser().currentDisk.has_value());

    fixture.browser.openFolder({});
    REQUIRE(rig.settle());

    REQUIRE(rig.model.problem().has_value());
    CHECK(rig.model.problem()->reason.find("Select a disk first.") != std::string::npos);
    CHECK(rig.model.problem()->reason.find("select_") == std::string::npos);
}
