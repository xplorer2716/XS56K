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

// The Aux Envelope group of section 08 (Set &60-&64 / Get &68-&6C, spec Tables 11-12): a Set followed
// by a Get, verified by read-back, for every item. `&64`/`&6C` is catalogued as "Off Velocity->Rate
// (Aux Rate 4 only)" — a parameter distinct from `&61`/`&69`'s "Velocity->Rate", by analogy with the
// On/Off Velocity->Release pair the Filter and Amplitude Envelope groups already have for their release
// stage (RQ-AKM-032); TASK-AKM-033 confirms or corrects this reading on real hardware. The `&6C`
// numbered 107 (a duplicate of `&6B`'s number, instead of 108) is a documentation-only slip in the
// spec's own secondary numbering column, not used by this catalogue — recorded by TASK-AKM-034, nothing
// to prove here. [TASK-AKM-032, RQ-AKM-030, RQ-AKM-032]
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

TEST_CASE("Given each item of the Aux Envelope group, When a value inside its range is set then read back, Then it equals the value set [RQ-AKM-030]",
          "[akm][keygroup][aux-envelope]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const std::vector<ParameterCase> cases{{
        {akm::ItemId::KeygroupSetAuxEnvRate, akm::ItemId::KeygroupGetAuxEnvRate, {1, 30}},
        {akm::ItemId::KeygroupSetAuxEnvRate, akm::ItemId::KeygroupGetAuxEnvRate, {4, 70}},
        {akm::ItemId::KeygroupSetAuxEnvVelocityToRate, akm::ItemId::KeygroupGetAuxEnvVelocityToRate, {1, 0, 40}},
        {akm::ItemId::KeygroupSetAuxEnvVelocityToRate, akm::ItemId::KeygroupGetAuxEnvVelocityToRate, {4, 1, 20}},
        {akm::ItemId::KeygroupSetAuxEnvKeyboardToR2R4, akm::ItemId::KeygroupGetAuxEnvKeyboardToR2R4, {0, 55}},
        {akm::ItemId::KeygroupSetAuxEnvLevel, akm::ItemId::KeygroupGetAuxEnvLevel, {2, 65}},
        {akm::ItemId::KeygroupSetAuxEnvOffVelocityToRate, akm::ItemId::KeygroupGetAuxEnvOffVelocityToRate, {4, 1, 10}},
    }};
    akm::test::checkParameterRoundTrips(harness, cases);
}

TEST_CASE("Given &61 (Velocity->Rate) and &64 (Off Velocity->Rate) set to different values for Aux Rate 4, When both are read back, Then they hold independently [RQ-AKM-032]",
          "[akm][keygroup][aux-envelope]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const CommandResult velocityToRateSet =
        *harness.submitAndWait(akm::makeRequest(akm::ItemId::KeygroupSetAuxEnvVelocityToRate, {4, 0, 33}));
    REQUIRE(std::holds_alternative<akm::Done>(velocityToRateSet));
    const CommandResult offVelocityToRateSet =
        *harness.submitAndWait(akm::makeRequest(akm::ItemId::KeygroupSetAuxEnvOffVelocityToRate, {4, 1, 66}));
    REQUIRE(std::holds_alternative<akm::Done>(offVelocityToRateSet));

    const CommandResult velocityToRateGet = *harness.submitAndWait(akm::makeRequest(akm::ItemId::KeygroupGetAuxEnvVelocityToRate, {4}));
    REQUIRE(std::holds_alternative<akm::Reply>(velocityToRateGet));
    CHECK(*akm::decodeReply(akm::ItemId::KeygroupGetAuxEnvVelocityToRate, std::get<akm::Reply>(velocityToRateGet).data)
          == std::vector<std::int64_t>{0, 33});

    const CommandResult offVelocityToRateGet = *harness.submitAndWait(akm::makeRequest(akm::ItemId::KeygroupGetAuxEnvOffVelocityToRate, {4}));
    REQUIRE(std::holds_alternative<akm::Reply>(offVelocityToRateGet));
    CHECK(*akm::decodeReply(akm::ItemId::KeygroupGetAuxEnvOffVelocityToRate, std::get<akm::Reply>(offVelocityToRateGet).data)
          == std::vector<std::int64_t>{1, 66});
}

TEST_CASE("Given an Aux Rate outside 1-4, When Aux Env. Rate is set, Then it is refused without sending [RQ-AKM-030]",
          "[akm][keygroup][aux-envelope]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const CommandResult result = *harness.submitAndWait(akm::makeRequest(akm::ItemId::KeygroupSetAuxEnvRate, {5, 50}));
    REQUIRE(std::holds_alternative<Refused>(result));
    CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 1);  // only the Create Program
}
