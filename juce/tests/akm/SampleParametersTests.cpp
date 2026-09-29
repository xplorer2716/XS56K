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

// The 8 settable §0E (Sample) items (Set &20-&24/&28-&2A, Get &40-&44/&48-&4A, spec Tables 18-19): a Set
// followed by a Get, verified by read-back, for every item, of the current sample — no selector, the
// same no-selector shape as Program's Output group (ProgramParameterRoundTrip.hpp is reused as-is).
// [TASK-AKM-043, RQ-AKM-048]
#include <catch2/catch_test_macros.hpp>

#include <variant>
#include <vector>

#include "ProgramParameterRoundTrip.hpp"
#include "SessionHarness.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/SamplePrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::CommandResult;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::ManualScenarioDriver;
using akm::test::ParameterCase;
using akm::test::SessionHarness;

namespace
{
    void seedAndSelectASample(SessionHarness& harness)
    {
        harness.sampler().setSampleNames({"A"});
        akm::selectSampleByName(harness.session(), "A", harness.recorder().completion());
        REQUIRE(harness.waitForCompletions(1));
    }
}

TEST_CASE("Given each of the 8 settable sample items, When a value inside its range is set on the current sample then read back, Then it equals the value set [RQ-AKM-048]",
          "[akm][sample]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    seedAndSelectASample(harness);

    const std::vector<ParameterCase> cases{{
        {akm::ItemId::SampleSetStartPosition, akm::ItemId::SampleGetStartPosition, {1, 2, 3, 4}},
        {akm::ItemId::SampleSetEndPosition, akm::ItemId::SampleGetEndPosition, {5, 6, 7, 8}},
        {akm::ItemId::SampleSetOriginalPitch, akm::ItemId::SampleGetOriginalPitch, {60}},
        {akm::ItemId::SampleSetSemitoneTune, akm::ItemId::SampleGetSemitoneTune, {0, 12}},
        {akm::ItemId::SampleSetFineTune, akm::ItemId::SampleGetFineTune, {1, 25}},
        {akm::ItemId::SampleSetPlaybackMode, akm::ItemId::SampleGetPlaybackMode, {3}},
        {akm::ItemId::SampleSetLoopStart, akm::ItemId::SampleGetLoopStart, {0, 10, 20, 30}},
        {akm::ItemId::SampleSetLoopEnd, akm::ItemId::SampleGetLoopEnd, {0, 11, 21, 31}},
    }};
    akm::test::checkParameterRoundTrips(harness, cases);
}

TEST_CASE("Given a playback mode of 6, When set, Then it is refused without sending [RQ-AKM-048]",
          "[akm][sample]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    seedAndSelectASample(harness);

    const akm::CommandRequest request = akm::makeRequest(akm::ItemId::SampleSetPlaybackMode, {6});
    REQUIRE(request.refusal.has_value());
    CHECK(*request.refusal == RefusalReason::ArgumentOutOfRange);
}

TEST_CASE("Given an original pitch of 20, below the spec's 21-127 range, When set, Then it is refused without sending [RQ-AKM-048]",
          "[akm][sample]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    seedAndSelectASample(harness);

    const akm::CommandRequest request = akm::makeRequest(akm::ItemId::SampleSetOriginalPitch, {20});
    REQUIRE(request.refusal.has_value());
    CHECK(*request.refusal == RefusalReason::ArgumentOutOfRange);
}

TEST_CASE("Given no sample is current, When a settable parameter is set, Then the ERROR 04 is reported [RQ-AKM-048]",
          "[akm][sample]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    harness.session().submit(akm::makeRequest(akm::ItemId::SampleSetPlaybackMode, {3}), harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    const CommandResult result = harness.recorder().results().back();
    REQUIRE(std::holds_alternative<akm::Error>(result));
    CHECK(std::get<akm::Error>(result).number == akm::error_number::NOT_FOUND);
}
