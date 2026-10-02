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
using akm::DiskInfo;
using akm::DiskListResult;
using akm::Done;
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
