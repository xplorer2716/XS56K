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

// The General Options group of section 08 (Set &04-&09 / Get &0A-&0F, spec Tables 11-12): a Set
// followed by a Get, verified by read-back, for every item, and RQ-AKM-031's "all keygroups" shape
// proven end to end against a real per-keygroup item for the first time (TASK-AKM-026 only had a
// stand-in). [TASK-AKM-027, RQ-AKM-029, RQ-AKM-031]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <variant>
#include <vector>

#include "ProgramParameterRoundTrip.hpp"
#include "SessionHarness.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/KeygroupPrimitives.hpp"
#include "akm/ProgramPrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::AllKeygroupsResult;
using akm::CommandResult;
using akm::Refused;
using akm::RefusalReason;
using akm::harness::ManualScenarioDriver;
using akm::test::Latched;
using akm::test::ParameterCase;
using akm::test::SessionHarness;

namespace
{
    void createAndSelectAProgram(SessionHarness& harness)
    {
        akm::createProgram(harness.session(), "A", harness.recorder().completion());
        REQUIRE(harness.waitForCompletions(1));
    }
}

TEST_CASE("Given each item of the General Options group, When a value inside its range is set then read back, Then it equals the value set [RQ-AKM-029]",
          "[akm][keygroup][general-options]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const std::vector<ParameterCase> cases{{
        {akm::ItemId::KeygroupSetLowNote, akm::ItemId::KeygroupGetLowNote, {36}},
        {akm::ItemId::KeygroupSetHighNote, akm::ItemId::KeygroupGetHighNote, {60}},
        {akm::ItemId::KeygroupSetMuteGroup, akm::ItemId::KeygroupGetMuteGroup, {5}},
        {akm::ItemId::KeygroupSetFxOverride, akm::ItemId::KeygroupGetFxOverride, {2}},
        {akm::ItemId::KeygroupSetFxSendLevel, akm::ItemId::KeygroupGetFxSendLevel, {75}},
        {akm::ItemId::KeygroupSetZoneCrossfade, akm::ItemId::KeygroupGetZoneCrossfade, {1}},
    }};
    akm::test::checkParameterRoundTrips(harness, cases);
}

TEST_CASE("Given a low note outside 21-127, When set, Then it is refused without sending [RQ-AKM-029]",
          "[akm][keygroup][general-options]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const CommandResult result = *harness.submitAndWait(akm::makeRequest(akm::ItemId::KeygroupSetLowNote, {20}));
    REQUIRE(std::holds_alternative<Refused>(result));
    CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 1);  // only the Create Program
}

TEST_CASE("Given a 3-keygroup program with keygroup 0 current, When Low Note is set then read for all keygroups, Then every keygroup has the value set [RQ-AKM-028, RQ-AKM-031]",
          "[akm][keygroup][general-options]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    createAndSelectAProgram(harness);
    akm::addKeygroupsToProgram(harness.session(), 2, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));

    akm::selectKeygroup(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));

    const CommandResult set = *harness.submitAndWait(akm::makeRequest(akm::ItemId::KeygroupSetLowNote, {40}));
    REQUIRE(std::holds_alternative<akm::Done>(set));

    auto latched = std::make_shared<Latched<AllKeygroupsResult>>();
    akm::getForAllKeygroups(harness.session(), akm::ItemId::KeygroupGetLowNote, 3,
                            [latched](const AllKeygroupsResult& r) { latched->set(r); });
    REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
    const AllKeygroupsResult result = *latched->value();
    REQUIRE(result.values.has_value());
    REQUIRE(result.values->size() == 3);
    for (const std::vector<std::int64_t>& record : *result.values)
        CHECK(record == std::vector<std::int64_t>{40});
}

TEST_CASE("Given a REPLY holding fewer records than the program's keygroup count, When getForAllKeygroups runs, Then values is empty [RQ-AKM-031]",
          "[akm][keygroup][general-options]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    createAndSelectAProgram(harness);
    akm::addKeygroupsToProgram(harness.session(), 2, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    akm::selectKeygroup(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));

    auto latched = std::make_shared<Latched<AllKeygroupsResult>>();
    // The program has 3 keygroups; 4 is deliberately wrong.
    akm::getForAllKeygroups(harness.session(), akm::ItemId::KeygroupGetLowNote, 4,
                            [latched](const AllKeygroupsResult& r) { latched->set(r); });
    REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
    const AllKeygroupsResult result = *latched->value();
    CHECK_FALSE(result.values.has_value());
}
