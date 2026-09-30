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

// Keygroup selection (§08/&01, &02) and the generic repeated-record REPLY decoder that RQ-AKM-031's
// "all keygroups" Get shape will use once a per-keygroup item exists (TASK-AKM-027 on). [TASK-AKM-026,
// RQ-AKM-028, RQ-AKM-031, ADR-AKM-001 (DEC-AKM-014)]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/KeygroupPrimitives.hpp"
#include "akm/ProgramPrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::AllKeygroupsResult;
using akm::CommandResult;
using akm::CurrentKeygroupResult;
using akm::Done;
using akm::Error;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::ManualScenarioDriver;
using akm::test::Latched;
using akm::test::SessionHarness;

namespace
{
    void createAndSelectAProgram(SessionHarness& harness)
    {
        akm::createProgram(harness.session(), "A", harness.recorder().completion());
        REQUIRE(harness.waitForCompletions(1));
    }

    CurrentKeygroupResult getCurrent(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<CurrentKeygroupResult>>();
        akm::getCurrentKeygroup(harness.session(), [latched](const CurrentKeygroupResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }
}

TEST_CASE("Given a program with 3 keygroups, When keygroup 2 is selected then Get current runs, Then it reports 2 [RQ-AKM-028]",
          "[akm][keygroup]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);
    akm::addKeygroupsToProgram(harness.session(), 2, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));

    akm::selectKeygroup(harness.session(), 2, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    const CurrentKeygroupResult current = getCurrent(harness);
    REQUIRE(current.keygroup.has_value());
    CHECK(*current.keygroup == 2);
}

TEST_CASE("Given a program with 3 keygroups, When keygroup 9 is selected, Then it fails as keygroup not found [RQ-AKM-028]",
          "[akm][keygroup]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);
    akm::addKeygroupsToProgram(harness.session(), 2, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));

    akm::selectKeygroup(harness.session(), 9, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    const CommandResult result = harness.recorder().results().back();
    REQUIRE(std::holds_alternative<Error>(result));
    CHECK(std::get<Error>(result).number == akm::error_number::KEYGROUP_NOT_IN_PROGRAM);
}

TEST_CASE("Given a keygroup number outside 0-99, When selected, Then it is refused without sending [RQ-AKM-028]",
          "[akm][keygroup]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const CommandResult result = *harness.submitAndWait(akm::makeRequest(akm::ItemId::KeygroupSelect, {100}));
    REQUIRE(std::holds_alternative<Refused>(result));
    CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 1);  // only the Create Program
}

TEST_CASE("Given keygroup 0, When selected then a Get current runs, Then it reports 0 (all keygroups) [RQ-AKM-028]",
          "[akm][keygroup]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    akm::selectKeygroup(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    const CurrentKeygroupResult current = getCurrent(harness);
    REQUIRE(current.keygroup.has_value());
    CHECK(*current.keygroup == 0);
}

TEST_CASE("Given a newly created program, When Get current keygroup runs without selecting one first, Then it reports keygroup 1 [RQ-AKM-028]",
          "[akm][keygroup]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const CurrentKeygroupResult current = getCurrent(harness);
    REQUIRE(current.keygroup.has_value());
    CHECK(*current.keygroup == 1);
}

TEST_CASE("Given a three-byte REPLY of a one-byte-reply item, When decodeRepeatedReply runs, Then it decodes three one-value records [ADR-AKM-001 (DEC-AKM-014)]",
          "[akm][keygroup][codec]")
{
    // KeygroupGetCurrent's own REPLY shape (one byte) stands in for a real per-keygroup item's, exercising
    // the generic mechanism ahead of TASK-AKM-027, which proves RQ-AKM-031 end to end with a real one.
    const std::vector<std::uint8_t> data{5, 3, 7};
    const auto records = akm::decodeRepeatedReply(akm::ItemId::KeygroupGetCurrent, data);
    REQUIRE(records.has_value());
    REQUIRE(records->size() == 3);
    CHECK((*records)[0] == std::vector<std::int64_t>{5});
    CHECK((*records)[1] == std::vector<std::int64_t>{3});
    CHECK((*records)[2] == std::vector<std::int64_t>{7});
}

TEST_CASE("Given a REPLY whose length is not a whole multiple of the item's reply width, When decodeRepeatedReply runs, Then it fails [ADR-AKM-001 (DEC-AKM-014)]",
          "[akm][keygroup][codec]")
{
    const std::vector<std::uint8_t> data{5, 3, 7, 9, 2};  // 5 bytes, not a multiple of 1... use a 2-byte item instead
    // ProgramGetProgramNumber has a two-value (2-byte) REPLY; 5 bytes is not a whole multiple of that.
    const auto records = akm::decodeRepeatedReply(akm::ItemId::ProgramGetNumber, data);
    CHECK_FALSE(records.has_value());
}

TEST_CASE("Given a program with 2 keygroups current and keygroup 0 selected, When getForAllKeygroups runs on a repeatable item, Then it decodes one record per keygroup [RQ-AKM-031]",
          "[akm][keygroup]")
{
    // Borrows &0A's own all-programs Get (ProgramGetAllNumbers) purely as a stand-in repeatable-REPLY
    // item, to prove getForAllKeygroups' generic mechanism ahead of a real per-keygroup item
    // (TASK-AKM-027). Two programs stand in for "two keygroups".
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    akm::createProgram(harness.session(), "A", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    akm::createProgram(harness.session(), "B", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));

    auto latched = std::make_shared<Latched<AllKeygroupsResult>>();
    akm::getForAllKeygroups(harness.session(), akm::ItemId::ProgramGetAllNumbers, 2,
                            [latched](const AllKeygroupsResult& r) { latched->set(r); });
    REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
    const AllKeygroupsResult result = *latched->value();
    REQUIRE(result.values.has_value());
    CHECK(result.values->size() == 2);
}

TEST_CASE("Given the number of records decoded differs from the expected keygroup count, When getForAllKeygroups runs, Then values is empty [RQ-AKM-031]",
          "[akm][keygroup]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    akm::createProgram(harness.session(), "A", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));

    auto latched = std::make_shared<Latched<AllKeygroupsResult>>();
    akm::getForAllKeygroups(harness.session(), akm::ItemId::ProgramGetAllNumbers, 3,
                            [latched](const AllKeygroupsResult& r) { latched->set(r); });
    REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
    const AllKeygroupsResult result = *latched->value();
    CHECK_FALSE(result.values.has_value());
}
