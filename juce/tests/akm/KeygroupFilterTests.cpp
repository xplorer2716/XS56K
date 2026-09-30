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

// The Filter group of section 08 (Set &20-&25 / Get &28-&2D, spec Tables 11-12): a Set followed by a
// Get, verified by read-back, for every item. [TASK-AKM-029, RQ-AKM-030]
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

TEST_CASE("Given each item of the Filter group, When a value inside its range is set then read back, Then it equals the value set [RQ-AKM-030]",
          "[akm][keygroup][filter]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const std::vector<ParameterCase> cases{{
        {akm::ItemId::KeygroupSetFilterMode, akm::ItemId::KeygroupGetFilterMode, {17}},
        {akm::ItemId::KeygroupSetFilterCutoff, akm::ItemId::KeygroupGetFilterCutoff, {80}},
        {akm::ItemId::KeygroupSetFilterResonance, akm::ItemId::KeygroupGetFilterResonance, {10}},
        {akm::ItemId::KeygroupSetFilterKeyboardTrack, akm::ItemId::KeygroupGetFilterKeyboardTrack, {0, 24}},
        {akm::ItemId::KeygroupSetFilterModInputValue, akm::ItemId::KeygroupGetFilterModInputValue, {1, 0, 50}},
        {akm::ItemId::KeygroupSetFilterModInputValue, akm::ItemId::KeygroupGetFilterModInputValue, {3, 1, 15}},
        {akm::ItemId::KeygroupSetFilterAttenuation, akm::ItemId::KeygroupGetFilterAttenuation, {3}},
    }};
    akm::test::checkParameterRoundTrips(harness, cases);
}

TEST_CASE("Given a filter mode outside 0-25, When set, Then it is refused without sending [RQ-AKM-030]",
          "[akm][keygroup][filter]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const CommandResult result = *harness.submitAndWait(akm::makeRequest(akm::ItemId::KeygroupSetFilterMode, {26}));
    REQUIRE(std::holds_alternative<Refused>(result));
    CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 1);  // only the Create Program
}
