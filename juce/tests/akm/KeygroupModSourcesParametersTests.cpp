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

// The Keygroup Modulation Sources parameter group of section 0A (Set &70-&72 / Get &74-&76, spec
// Tables 13-15): a Set followed by a Get, verified by read-back, for every item. [TASK-AKM-022,
// RQ-AKM-024]
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "ProgramParameterRoundTrip.hpp"
#include "SessionHarness.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/ProgramPrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::CommandResult;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::ManualScenarioDriver;
using akm::test::ParameterCase;
using akm::test::SessionHarness;

TEST_CASE("Given each item of the Keygroup Modulation Sources group, When a value inside its range is set then read back, Then it equals the value set [RQ-AKM-024]",
          "[akm][program][keygroupmodsources]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    akm::createProgram(harness.session(), "A", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));

    const std::vector<ParameterCase> cases{{
        {akm::ItemId::ProgramSetPitchModSource, akm::ItemId::ProgramGetPitchModSource, {1, 6}},
        {akm::ItemId::ProgramSetPitchModSource, akm::ItemId::ProgramGetPitchModSource, {2, 11}},
        {akm::ItemId::ProgramSetKeygroupAmpModSource, akm::ItemId::ProgramGetKeygroupAmpModSource, {1, 4}},
        {akm::ItemId::ProgramSetFilterModInputSource, akm::ItemId::ProgramGetFilterModInputSource, {1, 0}},
        {akm::ItemId::ProgramSetFilterModInputSource, akm::ItemId::ProgramGetFilterModInputSource, {2, 8}},
        {akm::ItemId::ProgramSetFilterModInputSource, akm::ItemId::ProgramGetFilterModInputSource, {3, 14}},
    }};
    akm::test::checkParameterRoundTrips(harness, cases);
}

TEST_CASE("Given a modulation source above 14, When set, Then it is refused without sending [RQ-AKM-024]",
          "[akm][program][keygroupmodsources]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    akm::createProgram(harness.session(), "A", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));

    const CommandResult result =
        *harness.submitAndWait(akm::makeRequest(akm::ItemId::ProgramSetPitchModSource, {1, 15}));
    REQUIRE(std::holds_alternative<Refused>(result));
    CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 1);  // only the Create Program
}
