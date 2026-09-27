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

// The MIDI/Tune parameter group of section 0A (Set &30-&34 / Get &38-&3C, spec Tables 13-14): a Set
// followed by a Get, verified by read-back, for every item, including the 12-note User Tune Template.
// [TASK-AKM-019, RQ-AKM-024]
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "ProgramParameterRoundTrip.hpp"
#include "SessionHarness.hpp"
#include "akm/ProgramPrimitives.hpp"

using akm::harness::ManualScenarioDriver;
using akm::test::ParameterCase;
using akm::test::SessionHarness;

TEST_CASE("Given each item of the MIDI/Tune group, When a value inside its range is set then read back, Then it equals the value set [RQ-AKM-024]",
          "[akm][program][miditune]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    akm::createProgram(harness.session(), "A", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));

    const std::vector<ParameterCase> cases{{
        {akm::ItemId::ProgramSetSemitoneTune, akm::ItemId::ProgramGetSemitoneTune, {1, 12}},
        {akm::ItemId::ProgramSetFineTune, akm::ItemId::ProgramGetFineTune, {0, 25}},
        {akm::ItemId::ProgramSetTuneTemplate, akm::ItemId::ProgramGetTuneTemplate, {5}},
        {akm::ItemId::ProgramSetKey, akm::ItemId::ProgramGetKey, {9}},
        // 12 notes starting at C, each (sign, magnitude): the format User Tune Template shares with
        // Fine Tune (spec, "same format as Item &31{49}").
        {akm::ItemId::ProgramSetUserTuneTemplate, akm::ItemId::ProgramGetUserTuneTemplate,
         {0, 1, 1, 2, 0, 3, 1, 4, 0, 5, 1, 6, 0, 7, 1, 8, 0, 9, 1, 10, 0, 11, 1, 12}},
    }};
    akm::test::checkParameterRoundTrips(harness, cases);
}
