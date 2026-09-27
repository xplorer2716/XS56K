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

// The Keygroup Pitch/Amp group of section 08 (Set &10-&14 / Get &18-&1C, spec Tables 11-12): a Set
// followed by a Get, verified by read-back, for every item. [TASK-AKM-028, RQ-AKM-030]
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

TEST_CASE("Given each item of the Keygroup Pitch/Amp group, When a value inside its range is set then read back, Then it equals the value set [RQ-AKM-030]",
          "[akm][keygroup][pitch-amp]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const std::vector<ParameterCase> cases{{
        {akm::ItemId::KeygroupSetSemitoneTune, akm::ItemId::KeygroupGetSemitoneTune, {0, 12}},
        {akm::ItemId::KeygroupSetFineTune, akm::ItemId::KeygroupGetFineTune, {1, 25}},
        {akm::ItemId::KeygroupSetLevel, akm::ItemId::KeygroupGetLevel, {5}},
        {akm::ItemId::KeygroupSetPitchModValue, akm::ItemId::KeygroupGetPitchModValue, {1, 0, 40}},
        {akm::ItemId::KeygroupSetPitchModValue, akm::ItemId::KeygroupGetPitchModValue, {2, 1, 10}},
        {akm::ItemId::KeygroupSetAmpModValue, akm::ItemId::KeygroupGetAmpModValue, {1, 0, 60}},
    }};
    akm::test::checkParameterRoundTrips(harness, cases);
}

TEST_CASE("Given a Pitch Mod selector outside 1-2, When Pitch Mod Value is set, Then it is refused without sending [RQ-AKM-030]",
          "[akm][keygroup][pitch-amp]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const CommandResult result = *harness.submitAndWait(
        akm::makeRequest(akm::ItemId::KeygroupSetPitchModValue, {3, 0, 10}));
    REQUIRE(std::holds_alternative<Refused>(result));
    CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 1);  // only the Create Program
}
