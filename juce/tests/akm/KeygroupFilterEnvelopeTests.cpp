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

// The Filter Envelope group of section 08 (Set &30-&38 / Get &40-&48, spec Tables 11-12): a Set
// followed by a Get, verified by read-back, for every item. `&48`'s own mock round trip stands in for
// the real-sampler observation RQ-AKM-032 asks for: the catalogue already treats `&48` as `&38`'s own
// Get ("Off Velocity->Release", not the REPLY table's "Off Velocity->Rate"), and TASK-AKM-033 confirms
// it on real hardware. [TASK-AKM-030, RQ-AKM-030, RQ-AKM-032]
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

TEST_CASE("Given each item of the Filter Envelope group, When a value inside its range is set then read back, Then it equals the value set [RQ-AKM-030]",
          "[akm][keygroup][filter-envelope]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const std::vector<ParameterCase> cases{{
        {akm::ItemId::KeygroupSetFilterEnvAttack, akm::ItemId::KeygroupGetFilterEnvAttack, {30}},
        {akm::ItemId::KeygroupSetFilterEnvVelocityToAttack, akm::ItemId::KeygroupGetFilterEnvVelocityToAttack, {0, 45}},
        {akm::ItemId::KeygroupSetFilterEnvDecay, akm::ItemId::KeygroupGetFilterEnvDecay, {50}},
        {akm::ItemId::KeygroupSetFilterEnvSustain, akm::ItemId::KeygroupGetFilterEnvSustain, {60}},
        {akm::ItemId::KeygroupSetFilterEnvRelease, akm::ItemId::KeygroupGetFilterEnvRelease, {20}},
        {akm::ItemId::KeygroupSetFilterEnvOnVelocityToRelease, akm::ItemId::KeygroupGetFilterEnvOnVelocityToRelease, {1, 35}},
        {akm::ItemId::KeygroupSetFilterEnvKeyscale, akm::ItemId::KeygroupGetFilterEnvKeyscale, {0, 15}},
        {akm::ItemId::KeygroupSetFilterEnvDepth, akm::ItemId::KeygroupGetFilterEnvDepth, {1, 70}},
        // The erratum item: &38 (Set ... Off Velocity->Release) round-tripped through &48, which the
        // spec's own REPLY table mislabels "Off Velocity->Rate" (RQ-AKM-032, documents/_index/sysex_spec.kb.md).
        {akm::ItemId::KeygroupSetFilterEnvOffVelocityToRelease, akm::ItemId::KeygroupGetFilterEnvOffVelocityToRelease, {0, 55}},
    }};
    akm::test::checkParameterRoundTrips(harness, cases);
}

TEST_CASE("Given a Filter Envelope Attack value outside 0-100, When set, Then it is refused without sending [RQ-AKM-030]",
          "[akm][keygroup][filter-envelope]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const CommandResult result = *harness.submitAndWait(akm::makeRequest(akm::ItemId::KeygroupSetFilterEnvAttack, {101}));
    REQUIRE(std::holds_alternative<Refused>(result));
    CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 1);  // only the Create Program
}
