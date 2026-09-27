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

// The LFO parameter groups of section 0A (Set &50-&5F / Get &60-&6F, spec Tables 13-14, LFO 1 and 2
// selected by <Data1>): a Set followed by a Get, verified by read-back, for every item — the largest
// single group of this lot (32 items). [TASK-AKM-021, RQ-AKM-024]
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

namespace
{
    void createAndSelectAProgram(SessionHarness& harness)
    {
        akm::createProgram(harness.session(), "A", harness.recorder().completion());
        REQUIRE(harness.waitForCompletions(1));
    }
}

TEST_CASE("Given each item of the LFO groups, When a value inside its range is set then read back for both LFOs where both apply, Then it equals the value set [RQ-AKM-024]",
          "[akm][program][lfo]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const std::vector<ParameterCase> cases{{
        {akm::ItemId::ProgramSetLfoRate, akm::ItemId::ProgramGetLfoRate, {1, 40}},
        {akm::ItemId::ProgramSetLfoRate, akm::ItemId::ProgramGetLfoRate, {2, 60}},
        {akm::ItemId::ProgramSetLfoDelay, akm::ItemId::ProgramGetLfoDelay, {1, 10}},
        {akm::ItemId::ProgramSetLfoDelay, akm::ItemId::ProgramGetLfoDelay, {2, 20}},
        {akm::ItemId::ProgramSetLfoDepth, akm::ItemId::ProgramGetLfoDepth, {1, 30}},
        {akm::ItemId::ProgramSetLfoDepth, akm::ItemId::ProgramGetLfoDepth, {2, 45}},
        {akm::ItemId::ProgramSetLfoWaveform, akm::ItemId::ProgramGetLfoWaveform, {1, 5}},
        {akm::ItemId::ProgramSetLfoWaveform, akm::ItemId::ProgramGetLfoWaveform, {2, 8}},
        {akm::ItemId::ProgramSetLfoSync, akm::ItemId::ProgramGetLfoSync, {1, 1}},
        {akm::ItemId::ProgramSetLfoRetrigger, akm::ItemId::ProgramGetLfoRetrigger, {2, 1}},
        {akm::ItemId::ProgramSetLfoRateModSource, akm::ItemId::ProgramGetLfoRateModSource, {1, 7}},
        {akm::ItemId::ProgramSetLfoRateModSource, akm::ItemId::ProgramGetLfoRateModSource, {2, 3}},
        {akm::ItemId::ProgramSetLfoRateModValue, akm::ItemId::ProgramGetLfoRateModValue, {1, 0, 50}},
        {akm::ItemId::ProgramSetLfoRateModValue, akm::ItemId::ProgramGetLfoRateModValue, {2, 1, 20}},
        {akm::ItemId::ProgramSetLfoDelayModSource, akm::ItemId::ProgramGetLfoDelayModSource, {1, 2}},
        {akm::ItemId::ProgramSetLfoDelayModSource, akm::ItemId::ProgramGetLfoDelayModSource, {2, 9}},
        {akm::ItemId::ProgramSetLfoDelayModValue, akm::ItemId::ProgramGetLfoDelayModValue, {1, 1, 15}},
        {akm::ItemId::ProgramSetLfoDelayModValue, akm::ItemId::ProgramGetLfoDelayModValue, {2, 0, 70}},
        {akm::ItemId::ProgramSetLfoDepthModSource, akm::ItemId::ProgramGetLfoDepthModSource, {1, 4}},
        {akm::ItemId::ProgramSetLfoDepthModSource, akm::ItemId::ProgramGetLfoDepthModSource, {2, 14}},
        {akm::ItemId::ProgramSetLfoDepthModValue, akm::ItemId::ProgramGetLfoDepthModValue, {1, 0, 33}},
        {akm::ItemId::ProgramSetLfoDepthModValue, akm::ItemId::ProgramGetLfoDepthModValue, {2, 1, 66}},
        {akm::ItemId::ProgramSetLfoModwheel, akm::ItemId::ProgramGetLfoModwheel, {1, 80}},
        {akm::ItemId::ProgramSetLfoAftertouch, akm::ItemId::ProgramGetLfoAftertouch, {1, 90}},
        {akm::ItemId::ProgramSetLfoMidiClockSyncEnable, akm::ItemId::ProgramGetLfoMidiClockSyncEnable, {2, 1}},
        {akm::ItemId::ProgramSetLfoMidiClockSyncDivision, akm::ItemId::ProgramGetLfoMidiClockSyncDivision, {2, 42}},
    }};
    akm::test::checkParameterRoundTrips(harness, cases);
}

TEST_CASE("Given LFO Sync requested for LFO 2 or Re-trigger for LFO 1, When set, Then it is refused without sending, since each is for its own LFO only [RQ-AKM-021]",
          "[akm][program][lfo]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const CommandResult syncForLfo2 = *harness.submitAndWait(akm::makeRequest(akm::ItemId::ProgramSetLfoSync, {2, 1}));
    REQUIRE(std::holds_alternative<Refused>(syncForLfo2));
    CHECK(std::get<Refused>(syncForLfo2).reason == RefusalReason::ArgumentOutOfRange);

    const CommandResult retriggerForLfo1 =
        *harness.submitAndWait(akm::makeRequest(akm::ItemId::ProgramSetLfoRetrigger, {1, 1}));
    REQUIRE(std::holds_alternative<Refused>(retriggerForLfo1));
    CHECK(std::get<Refused>(retriggerForLfo1).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 1);  // only the Create Program
}
