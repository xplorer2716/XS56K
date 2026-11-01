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
// The save controller over the gateway and the simulated sampler: the item proposed, saved alone or with its children into the browser's
// current folder, all the items of a kind, the whole memory, the questions of "check names" and the report checked on the disk; a disk the
// sampler cannot write is refused with nothing sent. [TASK-APP-056 to TASK-APP-059, RQ-APP-043 to RQ-APP-047, RQ-APP-013,
// ADR-APP-005 (DEC-APP-038 to DEC-APP-040), ADR-APP-001 (DEC-APP-003, DEC-APP-007)]
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
#include "app/SaveController.hpp"
#include "akm/harness/SimulatedSampler.hpp"

using akm::harness::DiskRecord;
using akm::harness::FileRecord;
using akm::harness::FolderRecord;
using app::ConnectionState;
using app::ExistingAnswer;
using app::MemoryItem;
using app::MemoryKind;
using app::SaveRequest;
using app::SaveScope;
using app::test::CoreRig;
using app::test::setupA;

namespace
{
    constexpr std::uint8_t SECTION_DISK = 0x10;
    constexpr std::uint8_t ITEM_SAVE_MEMORY_ITEM = 0x2C;
    constexpr std::uint8_t ITEM_SAVE_ALL_MEMORY_ITEMS = 0x2D;
    constexpr std::size_t TYPE_OF_ONE = 2;      ///< &2C: index (2 bytes), then the type
    constexpr std::size_t FLAG_OF_ONE = 3;      ///< then the overwrite flag
    constexpr std::size_t CHILDREN_OF_ONE = 4;  ///< then the children flag
    constexpr std::size_t TYPE_OF_ALL = 0;      ///< &2D: the type, the overwrite flag, the children flag
    constexpr std::size_t FLAG_OF_ALL = 1;
    constexpr std::uint8_t TYPE_PROGRAM = 2;
    constexpr std::uint8_t TYPE_SAMPLE = 3;
    constexpr std::uint8_t SKIP = 0;
    constexpr std::uint8_t OVERWRITE = 1;
    constexpr int FORMAT_MSDOS = 1;
    constexpr int FORMAT_FAT32 = 2;
    constexpr int FORMAT_S3000 = 5;
    constexpr std::size_t BASS = 1;
    constexpr const char* KIT = "KIT";
    constexpr const char* PAD_FILE = "PAD.AKP";

    /// HD1 (formatted `format`), whose root holds `rootFiles` and the empty folder KIT.
    std::vector<DiskRecord> oneDisk(int format, std::vector<FileRecord> rootFiles = {})
    {
        FolderRecord kit;
        kit.name = KIT;
        DiskRecord hard;
        hard.handle = 0;
        hard.name = "HD1";
        hard.type = 1;
        hard.format = format;
        hard.writable = true;
        hard.freeBytes = 1000000;
        hard.rootFolder.subFolders = {kit};
        hard.rootFolder.files = std::move(rootFiles);
        return {hard};
    }

    /// The programs PAD and BASS (current) and the samples KICK, SNARE and HAT in memory; HD1 selected, its root current.
    struct SaveRig
    {
        explicit SaveRig(int format = FORMAT_MSDOS, std::vector<FileRecord> rootFiles = {})
        {
            mcp::test::seedSimulatedPrograms(rig.backend, {{"PAD"}, {"BASS"}}, BASS);
            rig.sampler->setSampleNames({"KICK", "SNARE", "HAT"});
            rig.sampler->setDisks(oneDisk(format, std::move(rootFiles)));
            rig.controller.connect(setupA());
            REQUIRE(rig.settle());
            REQUIRE(rig.model.connection().state == ConnectionState::Connected);
            browser.selectDisk(0);
            REQUIRE(rig.settle());
        }

        [[nodiscard]] std::vector<std::vector<std::uint8_t>> sent(std::uint8_t item) const
        {
            std::vector<std::vector<std::uint8_t>> data;
            for (const auto& command : rig.sampler->acceptedCommands())
            {
                if (command.section == SECTION_DISK && command.item == item)
                    data.push_back(command.data);
            }
            return data;
        }

        [[nodiscard]] bool listed(const std::vector<std::string>& folder, const std::string& file) const
        {
            const app::FolderListing* listing = rig.model.browser().listing(folder);
            return listing != nullptr
                   && std::ranges::any_of(listing->files, [&file](const mcp::DiskFileEntry& entry) { return entry.name == file; });
        }

        CoreRig rig;
        app::FileBrowser browser{rig.model, rig.controller};
        app::MemoryController memory{rig.model, rig.controller};
        app::SaveController save{rig.model, rig.controller, rig.settings};
    };

    bool has(const std::vector<std::string>& names, const std::string& name)
    {
        return std::ranges::find(names, name) != names.end();
    }
}

// --- TASK-APP-056: the item proposed, alone or with its children, the format refused [RQ-APP-043, RQ-APP-047]

TEST_CASE("Given the program PAD selected and the folder KIT current, When it is saved with its children, Then PAD is saved into KIT with "
          "the save children flag [RQ-APP-043, TASK-APP-056]",
          "[app][save]")
{
    SaveRig fixture;
    fixture.browser.openFolder({KIT});
    REQUIRE(fixture.rig.settle());
    fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
    REQUIRE(fixture.rig.settle());
    REQUIRE(fixture.save.proposedItem() == std::optional<MemoryItem>(MemoryItem{MemoryKind::Program, "PAD"}));

    fixture.save.save(SaveRequest{MemoryItem{MemoryKind::Program, "PAD"}, SaveScope::WithChildren});
    REQUIRE(fixture.rig.settle());

    const auto data = fixture.sent(ITEM_SAVE_MEMORY_ITEM);
    REQUIRE(data.size() == 1);
    CHECK(data[0].at(TYPE_OF_ONE) == TYPE_PROGRAM);
    CHECK(data[0].at(CHILDREN_OF_ONE) == 1);
    CHECK(fixture.listed({KIT}, PAD_FILE));
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given nothing selected and BASS the current program, When the person asks to save, Then BASS is proposed; saved alone, the "
          "children flag is off [RQ-APP-043, TASK-APP-056]",
          "[app][save]")
{
    SaveRig fixture;
    REQUIRE_FALSE(fixture.rig.model.memory().selection.has_value());
    REQUIRE(fixture.save.proposedItem() == std::optional<MemoryItem>(MemoryItem{MemoryKind::Program, "BASS"}));

    fixture.save.save(SaveRequest{*fixture.save.proposedItem(), SaveScope::Alone});
    REQUIRE(fixture.rig.settle());

    const auto data = fixture.sent(ITEM_SAVE_MEMORY_ITEM);
    REQUIRE(data.size() == 1);
    CHECK(data[0].at(CHILDREN_OF_ONE) == 0);
    CHECK(fixture.listed({}, "BASS.AKP"));
}

TEST_CASE("Given the current disk formatted S3000, When the person asks to save, Then nothing is sent and the reason says the sampler saves "
          "only to an MS-DOS formatted disk; Given FAT32, Then the save proceeds [RQ-APP-047, TASK-APP-056]",
          "[app][save]")
{
    SaveRig akai(FORMAT_S3000);
    REQUIRE(akai.save.refusal().has_value());
    CHECK(akai.save.refusal()->find("MS-DOS") != std::string::npos);
    akai.save.save(SaveRequest{MemoryItem{MemoryKind::Program, "PAD"}, SaveScope::Alone});
    REQUIRE(akai.rig.settle());
    CHECK(akai.sent(ITEM_SAVE_MEMORY_ITEM).empty());
    REQUIRE(akai.rig.model.problem().has_value());
    CHECK(akai.rig.model.problem()->reason.find("MS-DOS") != std::string::npos);

    SaveRig fat(FORMAT_FAT32);
    CHECK_FALSE(fat.save.refusal().has_value());
    fat.save.save(SaveRequest{MemoryItem{MemoryKind::Program, "PAD"}, SaveScope::Alone});
    REQUIRE(fat.rig.settle());
    CHECK(fat.sent(ITEM_SAVE_MEMORY_ITEM).size() == 1);
}

// --- TASK-APP-057: all of a kind and the whole memory [RQ-APP-044]

TEST_CASE("Given three samples in memory, When all samples are saved, Then one save all of kind sample is sent [RQ-APP-044, TASK-APP-057]",
          "[app][save]")
{
    SaveRig fixture;
    fixture.save.save(SaveRequest{MemoryItem{MemoryKind::Sample, "KICK"}, SaveScope::AllOfKind});
    REQUIRE(fixture.rig.settle());

    const auto data = fixture.sent(ITEM_SAVE_ALL_MEMORY_ITEMS);
    REQUIRE(data.size() == 1);
    CHECK(data[0].at(TYPE_OF_ALL) == TYPE_SAMPLE);
    CHECK(fixture.sent(ITEM_SAVE_MEMORY_ITEM).empty());
    CHECK(fixture.listed({}, "HAT.WAV"));
}

TEST_CASE("Given programs and samples only, When the whole memory is saved, Then one save all is sent for each kind holding items, programs "
          "then samples [RQ-APP-044, DEC-APP-040, TASK-APP-057]",
          "[app][save]")
{
    SaveRig fixture;
    fixture.save.save(SaveRequest{MemoryItem{MemoryKind::Program, "PAD"}, SaveScope::WholeMemory});
    REQUIRE(fixture.rig.settle());

    const auto data = fixture.sent(ITEM_SAVE_ALL_MEMORY_ITEMS);
    REQUIRE(data.size() == 2);
    CHECK(data[0].at(TYPE_OF_ALL) == TYPE_PROGRAM);
    CHECK(data[1].at(TYPE_OF_ALL) == TYPE_SAMPLE);
    REQUIRE(fixture.rig.model.lastSave().has_value());
    CHECK(fixture.rig.model.lastSave()->kindsSaved == std::vector<MemoryKind>{MemoryKind::Program, MemoryKind::Sample});
}

TEST_CASE("Given the whole memory saved and the disk gone, When the save of the programs fails, Then no save of the samples is sent, the "
          "problem is shown and the report names no kind saved [RQ-APP-044, DEC-APP-040, TASK-APP-057]",
          "[app][save]")
{
    SaveRig fixture;
    fixture.rig.sampler->setDisks({});  // the disk taken away behind the browser's back: the first save fails
    fixture.save.save(SaveRequest{MemoryItem{MemoryKind::Program, "PAD"}, SaveScope::WholeMemory});
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.sent(ITEM_SAVE_ALL_MEMORY_ITEMS).empty());
    REQUIRE(fixture.rig.model.problem().has_value());
    REQUIRE(fixture.rig.model.lastSave().has_value());
    CHECK(fixture.rig.model.lastSave()->kindsSaved.empty());
}

// --- TASK-APP-058: check names [RQ-APP-045]

TEST_CASE("Given PAD.AKP in the folder and check names on, When PAD is saved alone and the answer is skip, Then nothing replaces it; When "
          "the answer is replace, Then the save is sent with overwrite; When the answer is cancel, Then nothing is sent [RQ-APP-045, "
          "TASK-APP-058]",
          "[app][save]")
{
    SaveRig fixture(FORMAT_MSDOS, {FileRecord{PAD_FILE, 10, std::string("PAD"), std::nullopt, {}}});
    REQUIRE(fixture.save.checkNames());
    const SaveRequest pad{MemoryItem{MemoryKind::Program, "PAD"}, SaveScope::Alone};

    fixture.rig.prompts.existingAnswers = {ExistingAnswer::Skip};
    fixture.save.save(pad);
    REQUIRE(fixture.rig.settle());
    REQUIRE(fixture.rig.prompts.existingAsked.size() == 1);
    CHECK(has(fixture.rig.prompts.existingAsked[0].files, PAD_FILE));
    CHECK(fixture.sent(ITEM_SAVE_MEMORY_ITEM).empty());
    REQUIRE(fixture.rig.model.lastSave().has_value());
    CHECK(has(fixture.rig.model.lastSave()->skipped, PAD_FILE));

    fixture.rig.prompts.existingAnswers = {ExistingAnswer::Cancel};
    fixture.save.save(pad);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_SAVE_MEMORY_ITEM).empty());

    fixture.rig.prompts.existingAnswers = {ExistingAnswer::Replace};
    fixture.save.save(pad);
    REQUIRE(fixture.rig.settle());
    const auto data = fixture.sent(ITEM_SAVE_MEMORY_ITEM);
    REQUIRE(data.size() == 1);
    CHECK(data[0].at(FLAG_OF_ONE) == OVERWRITE);
}

TEST_CASE("Given PAD.AKP in the folder and check names on, When all programs are saved, Then one question is asked for the whole save and "
          "a skip sends the skip flag [RQ-APP-045, DEC-APP-040, TASK-APP-058]",
          "[app][save]")
{
    SaveRig fixture(FORMAT_MSDOS, {FileRecord{PAD_FILE, 10, std::string("PAD"), std::nullopt, {}}});
    fixture.rig.prompts.existingAnswers = {ExistingAnswer::Skip};
    fixture.save.save(SaveRequest{MemoryItem{MemoryKind::Program, "PAD"}, SaveScope::AllOfKind});
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.rig.prompts.existingAsked.size() == 1);
    const auto data = fixture.sent(ITEM_SAVE_ALL_MEMORY_ITEMS);
    REQUIRE(data.size() == 1);
    CHECK(data[0].at(FLAG_OF_ALL) == SKIP);
}

TEST_CASE("Given PAD.AKP in the folder and check names off, When PAD is saved, Then the save is sent with overwrite and no question is "
          "asked; the setting is remembered [RQ-APP-045, TASK-APP-058]",
          "[app][save]")
{
    SaveRig fixture(FORMAT_MSDOS, {FileRecord{PAD_FILE, 10, std::string("PAD"), std::nullopt, {}}});
    fixture.save.setCheckNames(false);
    fixture.save.save(SaveRequest{MemoryItem{MemoryKind::Program, "PAD"}, SaveScope::Alone});
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.rig.prompts.existingAsked.empty());
    const auto data = fixture.sent(ITEM_SAVE_MEMORY_ITEM);
    REQUIRE(data.size() == 1);
    CHECK(data[0].at(FLAG_OF_ONE) == OVERWRITE);

    const app::SaveController again{fixture.rig.model, fixture.rig.controller, fixture.rig.settings};
    CHECK_FALSE(again.checkNames());
}

// --- TASK-APP-059: the save checked on the disk [RQ-APP-046]

TEST_CASE("Given PAD saved alone into KIT, When the save is done, Then the browser lists PAD.AKP in KIT and the report says PAD.AKP was saved "
          "[RQ-APP-046, TASK-APP-059]",
          "[app][save]")
{
    SaveRig fixture;
    fixture.browser.openFolder({KIT});
    REQUIRE(fixture.rig.settle());
    fixture.save.save(SaveRequest{MemoryItem{MemoryKind::Program, "PAD"}, SaveScope::Alone});
    REQUIRE(fixture.rig.settle());

    CHECK(fixture.listed({KIT}, PAD_FILE));
    REQUIRE(fixture.rig.model.lastSave().has_value());
    const app::SaveReport& report = *fixture.rig.model.lastSave();
    CHECK(has(report.saved, PAD_FILE));
    CHECK(report.skipped.empty());
    CHECK(report.missing.empty());
    CHECK(report.text().find(PAD_FILE) != std::string::npos);
}

TEST_CASE("Given a file expected and absent after the save, Then the report says it is missing [RQ-APP-046, TASK-APP-059]", "[app][save]")
{
    const app::SaveReport report = app::reportOf({MemoryItem{MemoryKind::Program, "PAD"}}, mcp::SaveOutcome{});
    CHECK(has(report.missing, PAD_FILE));
    CHECK(report.saved.empty());
    CHECK(report.text().find("missing") != std::string::npos);
}
