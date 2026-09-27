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

// The Pitch Bend parameter group of section 0A (Set &40-&47 / Get &48-&4F, spec Tables 13-14): a Set
// followed by a Get, verified by read-back, for every item. [TASK-AKM-020, RQ-AKM-024]
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "ProgramParameterRoundTrip.hpp"
#include "SessionHarness.hpp"
#include "akm/ProgramPrimitives.hpp"

using akm::harness::ManualScenarioDriver;
using akm::test::ParameterCase;
using akm::test::SessionHarness;

TEST_CASE("Given each item of the Pitch Bend group, When a value inside its range is set then read back, Then it equals the value set [RQ-AKM-024]",
          "[akm][program][pitchbend]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    akm::createProgram(harness.session(), "A", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));

    const std::vector<ParameterCase> cases{{
        {akm::ItemId::ProgramSetPitchBendUp, akm::ItemId::ProgramGetPitchBendUp, {12}},
        {akm::ItemId::ProgramSetPitchBendDown, akm::ItemId::ProgramGetPitchBendDown, {24}},
        {akm::ItemId::ProgramSetBendMode, akm::ItemId::ProgramGetBendMode, {1}},
        {akm::ItemId::ProgramSetAftertouchValue, akm::ItemId::ProgramGetAftertouchValue, {1, 8}},
        {akm::ItemId::ProgramSetLegato, akm::ItemId::ProgramGetLegato, {1}},
        {akm::ItemId::ProgramSetPortamentoEnable, akm::ItemId::ProgramGetPortamentoEnable, {1}},
        {akm::ItemId::ProgramSetPortamentoMode, akm::ItemId::ProgramGetPortamentoMode, {0}},
        {akm::ItemId::ProgramSetPortamentoTime, akm::ItemId::ProgramGetPortamentoTime, {60}},
    }};
    akm::test::checkParameterRoundTrips(harness, cases);
}
