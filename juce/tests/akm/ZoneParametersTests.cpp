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

// The 13 non-sample §06 (Keygroup Zone) items (Set &02-&0E / Get &22-&2E, spec Tables 9-10): a Set
// followed by a Get, verified by read-back, for every item, and isolation between zones of the same
// keygroup. Sample assignment (&01/&21) is ZoneSampleTests.cpp's (TASK-AKM-036). [TASK-AKM-035,
// RQ-AKM-034]
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "ProgramParameterRoundTrip.hpp"
#include "SessionHarness.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/ProgramPrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::CommandResult;
using akm::Refused;
using akm::RefusalReason;
using akm::harness::ManualScenarioDriver;
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

TEST_CASE("Given each of the 13 non-sample zone items, When a value inside its range is set on zone 2 "
          "then read back, Then it equals the value set [RQ-AKM-034]",
          "[akm][zone]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const std::vector<ParameterCase> cases{{
        {akm::ItemId::ZoneSetLevel, akm::ItemId::ZoneGetLevel, {2, 0, 75}},
        {akm::ItemId::ZoneSetPanBalance, akm::ItemId::ZoneGetPanBalance, {2, 100}},
        {akm::ItemId::ZoneSetOutput, akm::ItemId::ZoneGetOutput, {2, 5}},
        {akm::ItemId::ZoneSetFilter, akm::ItemId::ZoneGetFilter, {2, 1, 60}},
        {akm::ItemId::ZoneSetFineTune, akm::ItemId::ZoneGetFineTune, {2, 0, 30}},
        {akm::ItemId::ZoneSetSemitoneTune, akm::ItemId::ZoneGetSemitoneTune, {2, 1, 20}},
        {akm::ItemId::ZoneSetKeyboardTrack, akm::ItemId::ZoneGetKeyboardTrack, {2, 1}},
        {akm::ItemId::ZoneSetPlayback, akm::ItemId::ZoneGetPlayback, {2, 4}},
        {akm::ItemId::ZoneSetVelocityToStart, akm::ItemId::ZoneGetVelocityToStart, {2, 1, 3, 10}},
        {akm::ItemId::ZoneSetHighVelocity, akm::ItemId::ZoneGetHighVelocity, {2, 100}},
        {akm::ItemId::ZoneSetLowVelocity, akm::ItemId::ZoneGetLowVelocity, {2, 20}},
        {akm::ItemId::ZoneSetMute, akm::ItemId::ZoneGetMute, {2, 1}},
        {akm::ItemId::ZoneSetSolo, akm::ItemId::ZoneGetSolo, {2, 1}},
    }};
    akm::test::checkParameterRoundTrips(harness, cases);
}

TEST_CASE("Given a value set on zone 2, When zones 1, 3 and 4 are read back, Then they are unchanged [RQ-AKM-034]",
          "[akm][zone]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const CommandResult setResult = *harness.submitAndWait(akm::makeRequest(akm::ItemId::ZoneSetLevel, {2, 0, 75}));
    REQUIRE(std::holds_alternative<akm::Done>(setResult));

    for (const std::int64_t zone : {1, 3, 4})
    {
        const CommandResult getResult = *harness.submitAndWait(akm::makeRequest(akm::ItemId::ZoneGetLevel, {zone}));
        REQUIRE(std::holds_alternative<akm::Reply>(getResult));
        const auto decoded = akm::decodeReply(akm::ItemId::ZoneGetLevel, std::get<akm::Reply>(getResult).data);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == std::vector<std::int64_t>{0, 0});
    }
}

TEST_CASE("Given a zone number outside 0-4, When set, Then it is refused without sending [RQ-AKM-034]",
          "[akm][zone]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const CommandResult result = *harness.submitAndWait(akm::makeRequest(akm::ItemId::ZoneSetLevel, {5, 0, 75}));
    REQUIRE(std::holds_alternative<Refused>(result));
    CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 1);  // only the Create Program
}
