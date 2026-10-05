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

// The set list primitives of section 16 on a session and the simulated sampler: the count and the name by
// index, and the deletion and the renaming of a set list by index. A set list has no "current" selection:
// every item takes its index. The rename frame is an index (two 7-bit bytes) then a name, built by hand.
// [TASK-AKM-085, RQ-AKM-084, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <variant>

#include "SessionHarness.hpp"
#include "akm/SamplerError.hpp"
#include "akm/SongPrimitives.hpp"

using akm::CommandResult;
using akm::Done;
using akm::Error;
using akm::RefusalReason;
using akm::Refused;
using akm::SetListCountResult;
using akm::SetListNameResult;
using akm::harness::ManualScenarioDriver;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;
using akm::test::bytes;

namespace
{
    SetListCountResult getCount(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<SetListCountResult>>();
        akm::getSetListCount(harness.session(), [latched](const SetListCountResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    SetListNameResult getName(SessionHarness& harness, int index)
    {
        auto latched = std::make_shared<Latched<SetListNameResult>>();
        akm::getSetListNameByIndex(harness.session(), index, [latched](const SetListNameResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    void requireNotFound(const CommandResult& result)
    {
        REQUIRE(std::holds_alternative<Error>(result));
        CHECK(std::get<Error>(result).number == akm::error_number::NOT_FOUND);
    }
}

TEST_CASE("Given a simulated sampler holding the set lists SET1 and SET2, When the count and each name are read, Then the count is 2 and the names are SET1 and SET2 [RQ-AKM-084]",
          "[akm][setlist]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setSetListNames({"SET1", "SET2"});

    CHECK(getCount(harness).count == 2);
    CHECK(getName(harness, 0).name == "SET1");
    CHECK(getName(harness, 1).name == "SET2");
    requireNotFound(getName(harness, 2).outcome);
}

TEST_CASE("Given the set list at index 1, When it is renamed SET3, Then the frame carries the two index bytes then the name null-terminated, it completes on DONE and the name read back is SET3 [RQ-AKM-084]",
          "[akm][setlist]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setSetListNames({"SET1", "SET2"});

    akm::renameSetList(harness.session(), 1, "SET3", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    const Bytes frame = harness.sentFrames().back();
    constexpr std::size_t dataStart = akm::test::SENT_ITEM_INDEX + 1;
    CHECK(Bytes(frame.begin() + dataStart, frame.begin() + dataStart + 7)
          == bytes({0x00, 0x01, 0x53, 0x45, 0x54, 0x33, 0x00}));
    CHECK(getName(harness, 1).name == "SET3");
    CHECK(getName(harness, 0).name == "SET1");
}

TEST_CASE("Given the set list at index 0, When it is deleted, Then the count is 1 and index 0 names what was at index 1 [RQ-AKM-084]",
          "[akm][setlist]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setSetListNames({"SET1", "SET2"});

    akm::deleteSetList(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    const Bytes frame = harness.sentFrames().back();
    constexpr std::size_t dataStart = akm::test::SENT_ITEM_INDEX + 1;
    CHECK(Bytes(frame.begin() + dataStart, frame.begin() + dataStart + 2) == bytes({0x00, 0x00}));
    CHECK(getCount(harness).count == 1);
    CHECK(getName(harness, 0).name == "SET2");
}

TEST_CASE("Given an index with no set list, When it is deleted or renamed, Then the ERROR 04 is reported and nothing changes [RQ-AKM-084]",
          "[akm][setlist]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setSetListNames({"ONLY"});

    akm::deleteSetList(harness.session(), 1, harness.recorder().completion());
    akm::renameSetList(harness.session(), 5, "X", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    for (const CommandResult& result : harness.recorder().results())
        requireNotFound(result);
    CHECK(getCount(harness).count == 1);
    CHECK(getName(harness, 0).name == "ONLY");
}

TEST_CASE("Given an index above 14 bits or a name that is not 7-bit ASCII, When a set list is renamed, Then it is refused without sending [RQ-AKM-084, RQ-AKM-002]",
          "[akm][setlist]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    akm::renameSetList(harness.session(), 16384, "X", harness.recorder().completion());
    akm::renameSetList(harness.session(), -1, "X", harness.recorder().completion());
    akm::renameSetList(harness.session(), 0, "caf\xC3\xA9", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    const auto& results = harness.recorder().results();
    for (const CommandResult& result : results)
        REQUIRE(std::holds_alternative<Refused>(result));
    CHECK(std::get<Refused>(results[0]).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(std::get<Refused>(results[1]).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(std::get<Refused>(results[2]).reason == RefusalReason::NotEncodable);
    CHECK(harness.sentCount() == 0);
}

TEST_CASE("Given no set list in memory, When the count is read, Then it is zero on a REPLY [RQ-AKM-084]",
          "[akm][setlist]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    const SetListCountResult result = getCount(harness);
    REQUIRE(std::holds_alternative<akm::Reply>(result.outcome));
    CHECK(result.count == 0);
}

TEST_CASE("Given the checksum mode unknown, When a set list's name is requested, Then it is refused as ChecksumModeUnknown without sending [RQ-AKM-084, RQ-AKM-041, ADR-AKM-001 (DEC-AKM-013)]",
          "[akm][setlist]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    const SetListNameResult result = getName(harness, 0);
    REQUIRE(std::holds_alternative<Refused>(result.outcome));
    CHECK(std::get<Refused>(result.outcome).reason == RefusalReason::ChecksumModeUnknown);
    CHECK_FALSE(result.name.has_value());
    CHECK(harness.sentCount() == 0);
}
