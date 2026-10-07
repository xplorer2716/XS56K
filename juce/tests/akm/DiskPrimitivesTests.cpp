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

// Disk discovery of section 10: update the list of disks connected (&01), the number of disks (&04)
// and the list of all connected disks (&05). [TASK-AKM-057, RQ-AKM-060]
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string_view>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/DiskPrimitives.hpp"
#include "akm/ProgramPrimitives.hpp"
#include "akm/SamplePrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::CommandResult;
using akm::DiskCountResult;
using akm::DiskFileCountResult;
using akm::DiskFileIndexResult;
using akm::DiskFileNameResult;
using akm::DiskFileNamesResult;
using akm::DiskFileSizeResult;
using akm::DiskFolderCountResult;
using akm::DiskFolderNameResult;
using akm::DiskFolderNamesResult;
using akm::DiskFormatResult;
using akm::DiskFreeSpaceResult;
using akm::DiskHandleResult;
using akm::DiskInfo;
using akm::DiskListResult;
using akm::DiskNameResult;
using akm::DiskPathResult;
using akm::DiskTypeResult;
using akm::Done;
using akm::Error;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::DiskRecord;
using akm::harness::FileRecord;
using akm::harness::FolderRecord;
using akm::harness::ManualScenarioDriver;
using akm::test::Latched;
using akm::test::SessionHarness;

namespace
{
    DiskCountResult getDiskCount(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<DiskCountResult>>();
        akm::getDiskCount(harness.session(), [latched](const DiskCountResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    DiskListResult getConnectedDisks(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<DiskListResult>>();
        akm::getConnectedDisks(harness.session(), [latched](const DiskListResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    void selectDisk(SessionHarness& harness, int handle)
    {
        akm::selectDisk(harness.session(), handle, harness.recorder().completion());
    }

    DiskTypeResult getCurrentDiskType(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<DiskTypeResult>>();
        akm::getCurrentDiskType(harness.session(), [latched](const DiskTypeResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    DiskTypeResult getDiskType(SessionHarness& harness, int handle)
    {
        auto latched = std::make_shared<Latched<DiskTypeResult>>();
        akm::getDiskType(harness.session(), handle, [latched](const DiskTypeResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    DiskHandleResult getCurrentDiskHandle(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<DiskHandleResult>>();
        akm::getCurrentDiskHandle(harness.session(), [latched](const DiskHandleResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    DiskPathResult getCurrentDiskPath(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<DiskPathResult>>();
        akm::getCurrentDiskPath(harness.session(), [latched](const DiskPathResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    DiskFormatResult getCurrentDiskFormat(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<DiskFormatResult>>();
        akm::getCurrentDiskFormat(harness.session(), [latched](const DiskFormatResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    DiskFreeSpaceResult getCurrentDiskFreeSpace(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<DiskFreeSpaceResult>>();
        akm::getCurrentDiskFreeSpace(harness.session(), [latched](const DiskFreeSpaceResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    DiskNameResult getDiskName(SessionHarness& harness, int handle)
    {
        auto latched = std::make_shared<Latched<DiskNameResult>>();
        akm::getDiskName(harness.session(), handle, [latched](const DiskNameResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    DiskFolderCountResult getFolderCount(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<DiskFolderCountResult>>();
        akm::getFolderCount(harness.session(), [latched](const DiskFolderCountResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    DiskFolderNameResult getFolderName(SessionHarness& harness, int index)
    {
        auto latched = std::make_shared<Latched<DiskFolderNameResult>>();
        akm::getFolderName(harness.session(), index, [latched](const DiskFolderNameResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    DiskFolderNamesResult getAllFolderNames(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<DiskFolderNamesResult>>();
        akm::getAllFolderNames(harness.session(), [latched](const DiskFolderNamesResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    void openFolder(SessionHarness& harness, std::string_view name)
    {
        akm::openFolder(harness.session(), name, harness.recorder().completion());
    }

    void closeFolder(SessionHarness& harness)
    {
        akm::closeFolder(harness.session(), harness.recorder().completion());
    }

    void createFolder(SessionHarness& harness, std::string_view name)
    {
        akm::createFolder(harness.session(), name, harness.recorder().completion());
    }

    void renameFolder(SessionHarness& harness, std::string_view oldName, std::string_view newName)
    {
        akm::renameFolder(harness.session(), oldName, newName, harness.recorder().completion());
    }

    DiskFileCountResult getFileCount(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<DiskFileCountResult>>();
        akm::getFileCount(harness.session(), [latched](const DiskFileCountResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    DiskFileNameResult getFileName(SessionHarness& harness, int index)
    {
        auto latched = std::make_shared<Latched<DiskFileNameResult>>();
        akm::getFileName(harness.session(), index, [latched](const DiskFileNameResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    DiskFileNamesResult getAllFileNames(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<DiskFileNamesResult>>();
        akm::getAllFileNames(harness.session(), [latched](const DiskFileNamesResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    DiskFileSizeResult getFileSize(SessionHarness& harness, int index)
    {
        auto latched = std::make_shared<Latched<DiskFileSizeResult>>();
        akm::getFileSize(harness.session(), index, [latched](const DiskFileSizeResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    DiskFileIndexResult getFileIndexByName(SessionHarness& harness, std::string_view name)
    {
        auto latched = std::make_shared<Latched<DiskFileIndexResult>>();
        akm::getFileIndexByName(harness.session(), name, [latched](const DiskFileIndexResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    void renameFile(SessionHarness& harness, std::string_view oldName, std::string_view newName)
    {
        akm::renameFile(harness.session(), oldName, newName, harness.recorder().completion());
    }
}

TEST_CASE("Given no disks connected, When the count and the list are read, Then the count is 0 and the list is empty [RQ-AKM-060]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    const DiskCountResult count = getDiskCount(harness);
    REQUIRE(count.count.has_value());
    CHECK(*count.count == 0);

    const DiskListResult disks = getConnectedDisks(harness);
    REQUIRE(disks.disks.has_value());
    CHECK(disks.disks->empty());
}

TEST_CASE("Given two simulated disks, When the count and the list are read, Then the count is 2 and each entry decodes [RQ-AKM-060]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({
        DiskRecord{.handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "INTERNAL"},
        DiskRecord{.handle = 3, .type = 2, .format = 3, .scsiId = 1, .writable = false, .name = "CDROM"},
    });

    const DiskCountResult count = getDiskCount(harness);
    REQUIRE(count.count.has_value());
    CHECK(*count.count == 2);

    const DiskListResult disks = getConnectedDisks(harness);
    REQUIRE(disks.disks.has_value());
    const std::vector<DiskInfo> expected{
        DiskInfo{.handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "INTERNAL"},
        DiskInfo{.handle = 3, .type = 2, .format = 3, .scsiId = 1, .writable = false, .name = "CDROM"},
    };
    CHECK(*disks.disks == expected);
}

TEST_CASE("Given a disk handle above 127, When the list is read, Then the handle decodes from its two data bytes [RQ-AKM-060]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 200, .type = 0, .format = 1, .scsiId = 2, .writable = true, .name = "USB"}});

    const DiskListResult disks = getConnectedDisks(harness);
    REQUIRE(disks.disks.has_value());
    REQUIRE(disks.disks->size() == 1);
    CHECK((*disks.disks)[0].handle == 200);
}

TEST_CASE("Given a session that has not sent Update List of Disks, When the count or the list is read, Then each is still sent and decoded [RQ-AKM-060]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 0, .type = 0, .format = 0, .scsiId = 0, .writable = true, .name = "A"}});

    const DiskCountResult count = getDiskCount(harness);
    REQUIRE(count.count.has_value());
    CHECK(*count.count == 1);
}

TEST_CASE("Given a session, When Update List of Disks is sent, Then it completes DONE [RQ-AKM-060]", "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    akm::updateDiskList(harness.session(), harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    REQUIRE(std::holds_alternative<Done>(harness.recorder().results().back()));
}

TEST_CASE("Given the checksum mode unknown, When the list of connected disks is requested, Then it is refused as ChecksumModeUnknown without sending [RQ-AKM-060, RQ-AKM-041]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    const DiskListResult disks = getConnectedDisks(harness);
    REQUIRE(std::holds_alternative<Refused>(disks.outcome));
    CHECK(std::get<Refused>(disks.outcome).reason == RefusalReason::ChecksumModeUnknown);
    CHECK(harness.sentCount() == 0);
}

TEST_CASE("Given a disk, When it is selected then tested, Then selection succeeds and the test completes DONE [RQ-AKM-061]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 3, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA"}});

    selectDisk(harness, 3);
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    akm::testDiskValid(harness.session(), 3, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
}

TEST_CASE("Given a handle that names no disk, When it is selected or tested, Then each completes ERROR, not a REPLY [RQ-AKM-061]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    selectDisk(harness, 9);
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Error>(harness.recorder().results().back()));

    akm::testDiskValid(harness.session(), 9, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Error>(harness.recorder().results().back()));
}

TEST_CASE("Given the current disk's type, the specified disk's type, its handle and its path, When each is read, Then they decode to the simulated sampler's own values, the path being empty at the root folder [RQ-AKM-061]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 5, .type = 2, .format = 3, .scsiId = 1, .writable = false, .name = "CDROM"}});
    selectDisk(harness, 5);
    REQUIRE(harness.waitForCompletions(1));

    const DiskTypeResult currentType = getCurrentDiskType(harness);
    REQUIRE(currentType.type.has_value());
    CHECK(*currentType.type == 2);

    const DiskTypeResult specifiedType = getDiskType(harness, 5);
    REQUIRE(specifiedType.type.has_value());
    CHECK(*specifiedType.type == 2);

    const DiskHandleResult handle = getCurrentDiskHandle(harness);
    REQUIRE(handle.handle.has_value());
    CHECK(*handle.handle == 5);

    const DiskPathResult path = getCurrentDiskPath(harness);
    REQUIRE(path.path.has_value());
    CHECK(path.path->empty());
}

TEST_CASE("Given no disk selected, When the current type, handle or path is read, Then each completes ERROR [RQ-AKM-061]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 0, .type = 0, .format = 0, .scsiId = 0, .writable = true, .name = "A"}});

    CHECK_FALSE(getCurrentDiskType(harness).type.has_value());
    CHECK_FALSE(getCurrentDiskHandle(harness).handle.has_value());
    CHECK_FALSE(getCurrentDiskPath(harness).path.has_value());
}

// The real S5000 answers error 257 ("selected disk is invalid"), not 4, to a command that needs a selected disk when none is
// (observed 2026-10-05, OBSERVATIONS-RQ-MCP-012-real-sampler.md: the first command of a listing is &06, the current handle).
// [TASK-MCP-041, RQ-MCP-044]
TEST_CASE("Given no disk selected, When the current handle or path is read, Then the error is 257, selected disk invalid, as the real sampler answered [RQ-MCP-044]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 0, .type = 0, .format = 0, .scsiId = 0, .writable = true, .name = "A"}});

    const DiskHandleResult handle = getCurrentDiskHandle(harness);
    REQUIRE(std::holds_alternative<Error>(handle.outcome));
    CHECK(std::get<Error>(handle.outcome).number == akm::error_number::DISK_SELECTED_DISK_INVALID);

    const DiskPathResult path = getCurrentDiskPath(harness);
    REQUIRE(std::holds_alternative<Error>(path.outcome));
    CHECK(std::get<Error>(path.outcome).number == akm::error_number::DISK_SELECTED_DISK_INVALID);
}

// The real S5000 writes the current path below the root with a backslash: `AKWF\AKWF_theremin` (observed 2026-10-05).
// [TASK-MCP-041, RQ-MCP-044]
TEST_CASE("Given two folders opened one in the other, When the current path is read, Then the names are joined with a backslash as the real sampler did [RQ-MCP-044]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const FolderRecord inner{"INNER", {}, {}, {}, {}};
    harness.sampler().setDisks({DiskRecord{.handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA",
                                           .rootFolder = FolderRecord{"", {FolderRecord{"OUTER", {inner}, {}, {}, {}}}}}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));
    openFolder(harness, "OUTER");
    REQUIRE(harness.waitForCompletions(2));
    openFolder(harness, "INNER");
    REQUIRE(harness.waitForCompletions(3));

    const DiskPathResult path = getCurrentDiskPath(harness);
    REQUIRE(path.path.has_value());
    CHECK(*path.path == "OUTER\\INNER");
}

TEST_CASE("Given a disk formatted FAT32 with free space, When its format and free space are read, Then they decode to FAT32 and the byte count [RQ-AKM-062]",
          "[akm][disk]")
{
    constexpr std::uint64_t FOUR_GIB = 4ull * 1024 * 1024 * 1024;
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks(
        {DiskRecord{.handle = 1, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA", .freeBytes = FOUR_GIB}});
    selectDisk(harness, 1);
    REQUIRE(harness.waitForCompletions(1));

    const DiskFormatResult format = getCurrentDiskFormat(harness);
    REQUIRE(format.format.has_value());
    CHECK(*format.format == 2);

    const DiskFreeSpaceResult freeSpace = getCurrentDiskFreeSpace(harness);
    REQUIRE(freeSpace.freeBytes.has_value());
    CHECK(*freeSpace.freeBytes == FOUR_GIB);
}

TEST_CASE("Given a specified disk's name, When read, Then it decodes the same way a sampler or program name does [RQ-AKM-062]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 2, .type = 1, .format = 1, .scsiId = 0, .writable = true, .name = "VOLUME ONE"}});

    const DiskNameResult name = getDiskName(harness, 2);
    REQUIRE(name.name.has_value());
    CHECK(*name.name == "VOLUME ONE");
}

TEST_CASE("Given no disk selected, When the current format or free space is read, Then each completes ERROR [RQ-AKM-062]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 0, .type = 0, .format = 0, .scsiId = 0, .writable = true, .name = "A"}});

    CHECK_FALSE(getCurrentDiskFormat(harness).format.has_value());
    CHECK_FALSE(getCurrentDiskFreeSpace(harness).freeBytes.has_value());
}

TEST_CASE("Given a simulated folder with two sub-folders, When their count, one name and all names are read, Then they decode to 2, the requested name and both names in order [RQ-AKM-063]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{
        .handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA",
        .rootFolder = FolderRecord{"", {FolderRecord{"ALPHA", {}, {}, {}, {}}, FolderRecord{"BETA", {}, {}, {}, {}}}}}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    const DiskFolderCountResult count = getFolderCount(harness);
    REQUIRE(count.count.has_value());
    CHECK(*count.count == 2);

    const DiskFolderNameResult name = getFolderName(harness, 1);
    REQUIRE(name.name.has_value());
    CHECK(*name.name == "BETA");

    const DiskFolderNamesResult names = getAllFolderNames(harness);
    REQUIRE(names.names.has_value());
    CHECK(*names.names == std::vector<std::string>{"ALPHA", "BETA"});
}

TEST_CASE("Given the root folder, When closed, Then it completes ERROR [RQ-AKM-063]", "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA"}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    closeFolder(harness);
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Error>(harness.recorder().results().back()));
}

TEST_CASE("Given a sub-folder opened then a new one created and renamed, When its name is read back, Then it is the new name [RQ-AKM-063]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{
        .handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA",
        .rootFolder = FolderRecord{"", {FolderRecord{"SONGS", {}, {}, {}, {}}}}}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    openFolder(harness, "SONGS");
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    createFolder(harness, "DRAFT");
    REQUIRE(harness.waitForCompletions(3));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    renameFolder(harness, "DRAFT", "FINAL");
    REQUIRE(harness.waitForCompletions(4));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    const DiskFolderNameResult name = getFolderName(harness, 0);
    REQUIRE(name.name.has_value());
    CHECK(*name.name == "FINAL");

    closeFolder(harness);
    REQUIRE(harness.waitForCompletions(5));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
}

TEST_CASE("Given a folder name that does not exist, When opened, Then it completes ERROR [RQ-AKM-063]", "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA"}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    openFolder(harness, "GHOST");
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Error>(harness.recorder().results().back()));
}

TEST_CASE("Given no disk selected, When a folder is opened, closed, listed or created, Then each completes ERROR [RQ-AKM-063]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    CHECK_FALSE(getFolderCount(harness).count.has_value());
    openFolder(harness, "ANY");
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Error>(harness.recorder().results().back()));
}

TEST_CASE("Given a simulated folder containing a program and a sample in a sub-folder, When loaded, Then the simulated sampler's memory gains both and the command completes DONE [RQ-AKM-064]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    FolderRecord child;
    child.name = "CHILD";
    child.sampleFiles = {"KICK"};
    FolderRecord songs;
    songs.name = "SONGS";
    songs.programFiles = {"LEAD"};
    songs.subFolders = {child};
    harness.sampler().setDisks({DiskRecord{.handle = 0,
                                           .type = 1,
                                           .format = 2,
                                           .scsiId = 0,
                                           .writable = true,
                                           .name = "DATA",
                                           .rootFolder = FolderRecord{"", {songs}}}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    akm::loadFolder(harness.session(), "SONGS", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    auto latchedPrograms = std::make_shared<Latched<akm::AllProgramNamesResult>>();
    akm::getAllProgramNames(harness.session(), [latchedPrograms](const akm::AllProgramNamesResult& r) { latchedPrograms->set(r); });
    REQUIRE(harness.waitUntil([latchedPrograms] { return latchedPrograms->isSet(); }));
    REQUIRE(latchedPrograms->value()->names.has_value());
    CHECK(*latchedPrograms->value()->names == std::vector<std::string>{"LEAD"});

    auto latchedSamples = std::make_shared<Latched<akm::AllSampleNamesResult>>();
    akm::getAllSampleNames(harness.session(), [latchedSamples](const akm::AllSampleNamesResult& r) { latchedSamples->set(r); });
    REQUIRE(harness.waitUntil([latchedSamples] { return latchedSamples->isSet(); }));
    REQUIRE(latchedSamples->value()->names.has_value());
    CHECK(*latchedSamples->value()->names == std::vector<std::string>{"KICK"});
}

TEST_CASE("Given a folder name that does not exist, When loaded, Then it completes ERROR [RQ-AKM-064]", "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA"}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    akm::loadFolder(harness.session(), "GHOST", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Error>(harness.recorder().results().back()));
}

TEST_CASE("Given a simulated folder with two files, When their count, one name, all names, one size and one index-by-name are read, Then they decode correctly, including a size of 0 for an empty file [RQ-AKM-065]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{
        .handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA",
        .rootFolder = FolderRecord{"", {}, {}, {}, {FileRecord{"KICK.AKP", 2048}, FileRecord{"EMPTY.AKP", 0}}}}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    const DiskFileCountResult count = getFileCount(harness);
    REQUIRE(count.count.has_value());
    CHECK(*count.count == 2);

    const DiskFileNameResult name = getFileName(harness, 0);
    REQUIRE(name.name.has_value());
    CHECK(*name.name == "KICK.AKP");

    const DiskFileNamesResult names = getAllFileNames(harness);
    REQUIRE(names.names.has_value());
    CHECK(*names.names == std::vector<std::string>{"KICK.AKP", "EMPTY.AKP"});

    const DiskFileSizeResult size = getFileSize(harness, 0);
    REQUIRE(size.sizeBytes.has_value());
    CHECK(*size.sizeBytes == 2048u);

    const DiskFileSizeResult emptySize = getFileSize(harness, 1);
    REQUIRE(emptySize.sizeBytes.has_value());
    CHECK(*emptySize.sizeBytes == 0u);

    const DiskFileIndexResult index = getFileIndexByName(harness, "EMPTY.AKP");
    REQUIRE(index.index.has_value());
    CHECK(*index.index == 1);
}

TEST_CASE("Given a file renamed with an extension accidentally included in the new name, When sent, Then the primitive still sends exactly what it was given [RQ-AKM-065]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 0,
                                           .type = 1,
                                           .format = 2,
                                           .scsiId = 0,
                                           .writable = true,
                                           .name = "DATA",
                                           .rootFolder = FolderRecord{"", {}, {}, {}, {FileRecord{"OLD.AKP", 10}}}}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    // The name is given without its extension: the sampler appends the renamed file's own extension.
    renameFile(harness, "OLD.AKP", "NEW");
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    const DiskFileNameResult name = getFileName(harness, 0);
    REQUIRE(name.name.has_value());
    CHECK(*name.name == "NEW.AKP");
}

TEST_CASE("Given a file name that does not exist, When its index is requested or it is renamed, Then each completes ERROR [RQ-AKM-065]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA"}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    CHECK_FALSE(getFileIndexByName(harness, "GHOST.AKP").index.has_value());

    renameFile(harness, "GHOST.AKP", "ANY.AKP");
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Error>(harness.recorder().results().back()));
}

TEST_CASE("Given no disk selected, When the file count or a file index by name is requested, Then each completes ERROR [RQ-AKM-065]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    CHECK_FALSE(getFileCount(harness).count.has_value());
    CHECK_FALSE(getFileIndexByName(harness, "ANY.AKP").index.has_value());
}

TEST_CASE("Given a simulated program file whose sample is a separate file, When loaded with &2A, Then only the program appears in memory [RQ-AKM-066]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    FileRecord programFile{"LEAD.AKP", 100, std::string{"LEAD"}, std::nullopt, {"LEAD.WAV"}};
    FileRecord sampleFile{"LEAD.WAV", 200, std::nullopt, std::string{"LEAD"}, {}};
    harness.sampler().setDisks({DiskRecord{.handle = 0,
                                           .type = 1,
                                           .format = 2,
                                           .scsiId = 0,
                                           .writable = true,
                                           .name = "DATA",
                                           .rootFolder = FolderRecord{"", {}, {}, {}, {programFile, sampleFile}}}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    akm::loadFile(harness.session(), "LEAD.AKP", akm::SampleLoadOption::Normal, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    auto latchedPrograms = std::make_shared<Latched<akm::AllProgramNamesResult>>();
    akm::getAllProgramNames(harness.session(), [latchedPrograms](const akm::AllProgramNamesResult& r) { latchedPrograms->set(r); });
    REQUIRE(harness.waitUntil([latchedPrograms] { return latchedPrograms->isSet(); }));
    REQUIRE(latchedPrograms->value()->names.has_value());
    CHECK(*latchedPrograms->value()->names == std::vector<std::string>{"LEAD"});

    auto latchedSamples = std::make_shared<Latched<akm::AllSampleNamesResult>>();
    akm::getAllSampleNames(harness.session(), [latchedSamples](const akm::AllSampleNamesResult& r) { latchedSamples->set(r); });
    REQUIRE(harness.waitUntil([latchedSamples] { return latchedSamples->isSet(); }));
    // No sample is in memory: the sampler answers ERROR 3 to the names of all samples (observed on a real S5000, 2026-10-05).
    CHECK_FALSE(latchedSamples->value()->names.has_value());
    REQUIRE(std::holds_alternative<akm::Error>(latchedSamples->value()->outcome));
    CHECK(std::get<akm::Error>(latchedSamples->value()->outcome).number == akm::error_number::UNKNOWN_ERROR);
}

TEST_CASE("Given the same file loaded with &2B, Then both the program and its sample appear [RQ-AKM-066]", "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    FileRecord programFile{"LEAD.AKP", 100, std::string{"LEAD"}, std::nullopt, {"LEAD.WAV"}};
    FileRecord sampleFile{"LEAD.WAV", 200, std::nullopt, std::string{"LEAD"}, {}};
    harness.sampler().setDisks({DiskRecord{.handle = 0,
                                           .type = 1,
                                           .format = 2,
                                           .scsiId = 0,
                                           .writable = true,
                                           .name = "DATA",
                                           .rootFolder = FolderRecord{"", {}, {}, {}, {programFile, sampleFile}}}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    akm::loadFileWithDependents(harness.session(), "LEAD.AKP", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    auto latchedPrograms = std::make_shared<Latched<akm::AllProgramNamesResult>>();
    akm::getAllProgramNames(harness.session(), [latchedPrograms](const akm::AllProgramNamesResult& r) { latchedPrograms->set(r); });
    REQUIRE(harness.waitUntil([latchedPrograms] { return latchedPrograms->isSet(); }));
    REQUIRE(latchedPrograms->value()->names.has_value());
    CHECK(*latchedPrograms->value()->names == std::vector<std::string>{"LEAD"});

    auto latchedSamples = std::make_shared<Latched<akm::AllSampleNamesResult>>();
    akm::getAllSampleNames(harness.session(), [latchedSamples](const akm::AllSampleNamesResult& r) { latchedSamples->set(r); });
    REQUIRE(harness.waitUntil([latchedSamples] { return latchedSamples->isSet(); }));
    REQUIRE(latchedSamples->value()->names.has_value());
    CHECK(*latchedSamples->value()->names == std::vector<std::string>{"LEAD"});
}

TEST_CASE("Given a sample load option of VIRTUAL, When sent, Then the option byte is 2 [RQ-AKM-066]", "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{
        .handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA",
        .rootFolder = FolderRecord{"", {}, {}, {}, {FileRecord{"KICK.WAV", 10, std::nullopt, std::string{"KICK"}, {}}}}}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    akm::loadFile(harness.session(), "KICK.WAV", akm::SampleLoadOption::Virtual, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    const auto commands = harness.sampler().acceptedCommands();
    REQUIRE_FALSE(commands.empty());
    REQUIRE_FALSE(commands.back().data.empty());
    CHECK(commands.back().data.back() == 2);
}

TEST_CASE("Given a file name that does not exist, When loaded with or without dependents, Then each completes ERROR [RQ-AKM-066]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA"}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    akm::loadFile(harness.session(), "GHOST.AKP", akm::SampleLoadOption::Normal, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Error>(harness.recorder().results().back()));

    akm::loadFileWithDependents(harness.session(), "GHOST.AKP", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    CHECK(std::holds_alternative<Error>(harness.recorder().results().back()));
}

TEST_CASE("Given a simulated disk with an existing file at the target name, When saved with overwrite 0, Then the file is unchanged and the command completes ERROR [RQ-AKM-067]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{
        .handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA",
        .rootFolder = FolderRecord{"", {}, {}, {}, {FileRecord{"LEAD.AKP", 10}}}}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));
    akm::createProgram(harness.session(), "LEAD", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));

    akm::saveMemoryItem(harness.session(), 0, akm::SaveableMemoryType::Program, false, false, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    CHECK(std::holds_alternative<Error>(harness.recorder().results().back()));

    const DiskFileSizeResult size = getFileSize(harness, 0);
    REQUIRE(size.sizeBytes.has_value());
    CHECK(*size.sizeBytes == 10u);
}

TEST_CASE("Given a simulated disk with an existing file at the target name, When saved with overwrite 1, Then the file is replaced and it completes DONE [RQ-AKM-067]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{
        .handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA",
        .rootFolder = FolderRecord{"", {}, {}, {}, {FileRecord{"LEAD.AKP", 10}}}}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));
    akm::createProgram(harness.session(), "LEAD", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));

    akm::saveMemoryItem(harness.session(), 0, akm::SaveableMemoryType::Program, true, false, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    const DiskFileCountResult count = getFileCount(harness);
    REQUIRE(count.count.has_value());
    CHECK(*count.count == 1);
    const DiskFileSizeResult size = getFileSize(harness, 0);
    REQUIRE(size.sizeBytes.has_value());
    // 516 bytes: the size of a saved program of one keygroup on the real S5000 (2026-10-05, TASK-MCP-041).
    CHECK(*size.sizeBytes == 516u);
}

// What the real S5000 wrote on 2026-10-06 (OBSERVATIONS-RQ-MCP-012-real-sampler.md): a mono sample of 616 points is a 1376-byte `.WAV`
// (144 bytes before the data, 2 bytes per point and channel; a stereo sample of 91985 points is 368084 bytes),
// a multi of 32 parts is a 2354-byte `.AKM`, a program is 164 bytes plus 352 per keygroup (516, 1220 and 3684 for 1, 3 and 10).
// Measured again on 2026-10-07 (OBSERVATIONS-RQ-MCP-012-real-sampler.md). [TASK-MCP-041, TASK-MCP-043, RQ-MCP-044]
TEST_CASE("Given a mono sample of 616 points and a stereo one of 100 points, When they are saved, Then the files are 1376 and 544 bytes [RQ-MCP-044]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA"}});
    harness.sampler().setSampleNames({"MONO", "STEREO"});
    harness.sampler().setSampleAttributes(0, 0, 1, 616, 44100);
    harness.sampler().setSampleAttributes(1, 0, 2, 100, 44100);
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    akm::saveMemoryItem(harness.session(), 0, akm::SaveableMemoryType::Sample, false, false, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    akm::saveMemoryItem(harness.session(), 1, akm::SaveableMemoryType::Sample, false, false, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));

    const DiskFileSizeResult mono = getFileSize(harness, 0);
    REQUIRE(mono.sizeBytes.has_value());
    CHECK(*mono.sizeBytes == 1376u);
    const DiskFileSizeResult stereo = getFileSize(harness, 1);
    REQUIRE(stereo.sizeBytes.has_value());
    CHECK(*stereo.sizeBytes == 544u);
}

TEST_CASE("Given programs of 1, 3 and 10 keygroups, When they are saved, Then the files are 516, 1220 and 3684 bytes [RQ-MCP-044]", "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA"}});
    harness.sampler().setProgramNames({"P01", "P03", "P10"});
    harness.sampler().setKeygroupCount(1, 3);
    harness.sampler().setKeygroupCount(2, 10);
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    for (int index = 0; index < 3; ++index)
    {
        akm::saveMemoryItem(harness.session(), index, akm::SaveableMemoryType::Program, false, false, harness.recorder().completion());
        REQUIRE(harness.waitForCompletions(2 + static_cast<std::size_t>(index)));
    }

    const std::uint32_t expected[] = {516u, 1220u, 3684u};
    for (int index = 0; index < 3; ++index)
    {
        const DiskFileSizeResult size = getFileSize(harness, index);
        REQUIRE(size.sizeBytes.has_value());
        CHECK(*size.sizeBytes == expected[index]);
    }
}

TEST_CASE("Given a multi of 32 parts, When it is saved, Then the file is 2354 bytes [RQ-MCP-044]", "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA"}});
    harness.sampler().setMultiNames({"LIVE"});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    akm::saveMemoryItem(harness.session(), 0, akm::SaveableMemoryType::Multi, false, false, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));

    const DiskFileSizeResult size = getFileSize(harness, 0);
    REQUIRE(size.sizeBytes.has_value());
    CHECK(*size.sizeBytes == 2354u);
}

TEST_CASE("Given two programs in memory, When all are saved, Then two files appear in the current folder [RQ-AKM-067]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA"}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));
    akm::createProgram(harness.session(), "A", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    akm::createProgram(harness.session(), "B", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));

    akm::saveAllMemoryItems(harness.session(), akm::SaveableMemoryType::Program, false, false, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(4));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    const DiskFileNamesResult names = getAllFileNames(harness);
    REQUIRE(names.names.has_value());
    CHECK(*names.names == std::vector<std::string>{"A.AKP", "B.AKP"});
}

TEST_CASE("Given a simulated file at index 0, When audition is started then stopped, Then both complete DONE [RQ-AKM-068]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{
        .handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA",
        .rootFolder = FolderRecord{"", {}, {}, {}, {FileRecord{"KICK.WAV", 100}}}}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    akm::startFileAudition(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    akm::stopFileAudition(harness.session(), harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
}

TEST_CASE("Given an index that names no file, When audition is started, Then it completes ERROR [RQ-AKM-068]", "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA"}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    akm::startFileAudition(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Error>(harness.recorder().results().back()));
}

TEST_CASE("Given a request for eject-with-discard, delete sub-folder or delete file without the confirmation argument, When made, Then nothing is sent and an error explains why [RQ-AKM-069]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    akm::ejectDiskDiscardingVirtualSamples(harness.session(), 0, std::nullopt, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    REQUIRE(std::holds_alternative<Refused>(harness.recorder().results().back()));
    CHECK(std::get<Refused>(harness.recorder().results().back()).reason == RefusalReason::NotConfirmed);

    akm::deleteSubFolder(harness.session(), "ANY", std::nullopt, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    REQUIRE(std::holds_alternative<Refused>(harness.recorder().results().back()));
    CHECK(std::get<Refused>(harness.recorder().results().back()).reason == RefusalReason::NotConfirmed);

    akm::deleteFile(harness.session(), "ANY", std::nullopt, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    REQUIRE(std::holds_alternative<Refused>(harness.recorder().results().back()));
    CHECK(std::get<Refused>(harness.recorder().results().back()).reason == RefusalReason::NotConfirmed);

    CHECK(harness.sentCount() == 0);
}

TEST_CASE("Given eject without discard, When requested, Then it is sent without needing confirmation [RQ-AKM-069]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{.handle = 3, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA"}});

    akm::ejectDisk(harness.session(), 3, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    const DiskListResult disks = getConnectedDisks(harness);
    REQUIRE(disks.disks.has_value());
    CHECK(disks.disks->empty());
}

// The real S5000 took longer than the 2 s of an ordinary command to delete a folder of 7 files (2026-10-06, TASK-MCP-042): the
// deletion succeeded but the command had already been given up. A caller can pass a longer timeout, as it does for a load or a
// save. [RQ-MCP-039, RQ-MCP-044]
TEST_CASE("Given a sampler that answers after 5 s, When a folder and a file are deleted with a 10 s timeout, Then each completes DONE; with the default options the first one times out [RQ-MCP-044]",
          "[akm][disk]")
{
    using namespace std::chrono_literals;
    constexpr auto SLOW_REPLY = 5s;
    constexpr auto LONG_TIMEOUT = 10s;
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{
        .handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA",
        .rootFolder = FolderRecord{"", {FolderRecord{"OLD", {}, {}, {}, {}}, FolderRecord{"OLDER", {}, {}, {}, {}}}, {}, {},
                                   {FileRecord{"JUNK.AKP", 1}}}}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));
    akm::harness::SamplerBehaviour slow;
    slow.replyDelay = SLOW_REPLY;
    harness.sampler().setBehaviour(slow);

    akm::deleteSubFolder(harness.session(), "OLD", akm::ConfirmDeleteSubFolder::IUnderstandThisDeletesTheFolderAndEverythingInIt,
                         harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2, 60s));
    CHECK(std::holds_alternative<akm::Timeout>(harness.recorder().results().back()));

    akm::CommandOptions longWait;
    longWait.timeout = LONG_TIMEOUT;
    longWait.maxTotalWait = LONG_TIMEOUT;
    akm::deleteSubFolder(harness.session(), "OLDER", akm::ConfirmDeleteSubFolder::IUnderstandThisDeletesTheFolderAndEverythingInIt,
                         harness.recorder().completion(), longWait);
    REQUIRE(harness.waitForCompletions(3, 60s));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    akm::deleteFile(harness.session(), "JUNK.AKP", akm::ConfirmDeleteFile::IUnderstandThisDeletesTheFile,
                    harness.recorder().completion(), longWait);
    REQUIRE(harness.waitForCompletions(4, 60s));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
}

TEST_CASE("Given confirmation, When eject-with-discard, delete sub-folder and delete file are requested, Then each removes its target and completes DONE [RQ-AKM-069]",
          "[akm][disk]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setDisks({DiskRecord{
        .handle = 0, .type = 1, .format = 2, .scsiId = 0, .writable = true, .name = "DATA",
        .rootFolder = FolderRecord{"", {FolderRecord{"OLD", {}, {}, {}, {}}}, {}, {}, {FileRecord{"JUNK.AKP", 1}}}}});
    selectDisk(harness, 0);
    REQUIRE(harness.waitForCompletions(1));

    akm::deleteSubFolder(harness.session(), "OLD", akm::ConfirmDeleteSubFolder::IUnderstandThisDeletesTheFolderAndEverythingInIt,
                         harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(getFolderCount(harness).count == 0);

    akm::deleteFile(harness.session(), "JUNK.AKP", akm::ConfirmDeleteFile::IUnderstandThisDeletesTheFile,
                    harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(getFileCount(harness).count == 0);

    akm::ejectDiskDiscardingVirtualSamples(harness.session(), 0,
                                           akm::ConfirmEjectDiscardingVirtualSamples::IUnderstandThisDiscardsEveryVirtualSampleOnThisDisk,
                                           harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(4));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
}
