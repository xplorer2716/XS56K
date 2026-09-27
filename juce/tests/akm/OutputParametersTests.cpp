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

// The Output parameter group of section 0A (Set &20-&25 / Get &28-&2D, spec Tables 13-14): a Set
// followed by a Get, verified by read-back, for every item. [TASK-AKM-018, RQ-AKM-024]
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

TEST_CASE("Given each item of the Output group, When a value inside its range is set then read back, Then it equals the value set [RQ-AKM-024]",
          "[akm][program][output]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const std::vector<ParameterCase> cases{{
        {akm::ItemId::ProgramSetLoudness, akm::ItemId::ProgramGetLoudness, {50}},
        {akm::ItemId::ProgramSetVelocitySensitivity, akm::ItemId::ProgramGetVelocitySensitivity, {1, 30}},
        {akm::ItemId::ProgramSetAmpModSource, akm::ItemId::ProgramGetAmpModSource, {1, 7}},
        {akm::ItemId::ProgramSetAmpModSource, akm::ItemId::ProgramGetAmpModSource, {2, 3}},
        {akm::ItemId::ProgramSetAmpModValue, akm::ItemId::ProgramGetAmpModValue, {2, 0, 45}},
        {akm::ItemId::ProgramSetPanModSource, akm::ItemId::ProgramGetPanModSource, {3, 2}},
        {akm::ItemId::ProgramSetPanModValue, akm::ItemId::ProgramGetPanModValue, {1, 1, 10}},
    }};
    akm::test::checkParameterRoundTrips(harness, cases);
}

TEST_CASE("Given a value outside its range, When Amp Mod Source or Pan Mod Source is set, Then it is refused without sending [RQ-AKM-024]",
          "[akm][program][output]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const CommandResult tooManyAmpMods = *harness.submitAndWait(
        akm::makeRequest(akm::ItemId::ProgramSetAmpModSource, {3, 0}));
    REQUIRE(std::holds_alternative<Refused>(tooManyAmpMods));
    CHECK(std::get<Refused>(tooManyAmpMods).reason == RefusalReason::ArgumentOutOfRange);

    const CommandResult tooManyPanMods = *harness.submitAndWait(
        akm::makeRequest(akm::ItemId::ProgramSetPanModSource, {4, 0}));
    REQUIRE(std::holds_alternative<Refused>(tooManyPanMods));
    CHECK(std::get<Refused>(tooManyPanMods).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 1);  // only the Create Program of createAndSelectAProgram
}
