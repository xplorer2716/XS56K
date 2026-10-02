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

#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/DiskPrimitives.hpp"

using akm::CommandResult;
using akm::DiskCountResult;
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
