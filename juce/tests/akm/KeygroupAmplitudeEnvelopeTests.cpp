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

// The Amplitude Envelope group of section 08 (Set &50-&57 / Get &58-&5F, spec Tables 11-12): a Set
// followed by a Get, verified by read-back, for every item. `&5F`'s round trip stands in for the
// real-sampler observation RQ-AKM-032 asks for, the same way `&48` did in TASK-AKM-030 — the catalogue
// already treats it as `&57`'s own Get ("Off Velocity->Release", not the REPLY table's "Off
// Velocity->Rate"), confirmed on real hardware by TASK-AKM-033. [TASK-AKM-031, RQ-AKM-030, RQ-AKM-032]
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

TEST_CASE("Given each item of the Amplitude Envelope group, When a value inside its range is set then read back, Then it equals the value set [RQ-AKM-030]",
          "[akm][keygroup][amplitude-envelope]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const std::vector<ParameterCase> cases{{
        {akm::ItemId::KeygroupSetAmpEnvAttack, akm::ItemId::KeygroupGetAmpEnvAttack, {25}},
        {akm::ItemId::KeygroupSetAmpEnvVelocityToAttack, akm::ItemId::KeygroupGetAmpEnvVelocityToAttack, {1, 40}},
        {akm::ItemId::KeygroupSetAmpEnvDecay, akm::ItemId::KeygroupGetAmpEnvDecay, {45}},
        {akm::ItemId::KeygroupSetAmpEnvSustain, akm::ItemId::KeygroupGetAmpEnvSustain, {70}},
        {akm::ItemId::KeygroupSetAmpEnvRelease, akm::ItemId::KeygroupGetAmpEnvRelease, {15}},
        {akm::ItemId::KeygroupSetAmpEnvOnVelocityToRelease, akm::ItemId::KeygroupGetAmpEnvOnVelocityToRelease, {0, 30}},
        {akm::ItemId::KeygroupSetAmpEnvKeyscale, akm::ItemId::KeygroupGetAmpEnvKeyscale, {1, 20}},
        // The erratum item: &57 (Set ... Off Velocity->Release) round-tripped through &5F, which the
        // spec's own REPLY table mislabels "Off Velocity->Rate" (RQ-AKM-032, documents/_index/sysex_spec.kb.md).
        {akm::ItemId::KeygroupSetAmpEnvOffVelocityToRelease, akm::ItemId::KeygroupGetAmpEnvOffVelocityToRelease, {0, 65}},
    }};
    akm::test::checkParameterRoundTrips(harness, cases);
}

TEST_CASE("Given an Amplitude Envelope Sustain value outside 0-100, When set, Then it is refused without sending [RQ-AKM-030]",
          "[akm][keygroup][amplitude-envelope]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const CommandResult result = *harness.submitAndWait(akm::makeRequest(akm::ItemId::KeygroupSetAmpEnvSustain, {101}));
    REQUIRE(std::holds_alternative<Refused>(result));
    CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 1);  // only the Create Program
}
