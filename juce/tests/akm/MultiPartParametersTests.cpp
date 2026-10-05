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

// The twelve multi part parameters of section 0C (Set &10-&1B / Get &20-&2B, spec Tables 16-17): a Set followed
// by a Get, verified by read-back, for every item, isolation between parts of the same multi, and the refusals of
// a part or a value out of range. [TASK-AKM-091, RQ-AKM-089]
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "ProgramParameterRoundTrip.hpp"
#include "SessionHarness.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/MultiPrimitives.hpp"
#include "akm/SamplerError.hpp"
#include "akm/harness/MultiPartParameterCases.hpp"

using akm::CommandResult;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::ManualScenarioDriver;
using akm::test::ParameterCase;
using akm::test::SessionHarness;

namespace
{
    void createAndSelectAMulti(SessionHarness& harness)
    {
        akm::createMulti(harness.session(), "A", harness.recorder().completion());
        REQUIRE(harness.waitForCompletions(1));
    }
}

TEST_CASE("Given each of the twelve multi part parameters, When a value inside its range is set on part 2 then read back, Then it equals the value set [RQ-AKM-089]",
          "[akm][multi][multi-part]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAMulti(harness);

    const std::vector<ParameterCase> cases{{
        {akm::ItemId::MultiSetMidiChannel, akm::ItemId::MultiGetMidiChannel, {2, 9}},
        {akm::ItemId::MultiSetMute, akm::ItemId::MultiGetMute, {2, 0}},
        {akm::ItemId::MultiSetSolo, akm::ItemId::MultiGetSolo, {2, 1}},
        {akm::ItemId::MultiSetLevel, akm::ItemId::MultiGetLevel, {2, 75}},
        {akm::ItemId::MultiSetOutput, akm::ItemId::MultiGetOutput, {2, 20}},
        {akm::ItemId::MultiSetPanBalance, akm::ItemId::MultiGetPanBalance, {2, 30}},
        {akm::ItemId::MultiSetEffectsChannel, akm::ItemId::MultiGetEffectsChannel, {2, 2}},
        {akm::ItemId::MultiSetFxSendLevel, akm::ItemId::MultiGetFxSendLevel, {2, 60}},
        {akm::ItemId::MultiSetFineTune, akm::ItemId::MultiGetFineTune, {2, 25}},
        {akm::ItemId::MultiSetTranspose, akm::ItemId::MultiGetTranspose, {2, 30}},
        {akm::ItemId::MultiSetLowNote, akm::ItemId::MultiGetLowNote, {2, 40}},
        {akm::ItemId::MultiSetHighNote, akm::ItemId::MultiGetHighNote, {2, 100}},
    }};
    akm::test::checkParameterRoundTrips(harness, cases);
}

TEST_CASE("Given the shared table of cases, When each is set on part 3 then read back, Then it equals the value set [RQ-AKM-089, RQ-AKM-093]",
          "[akm][multi][multi-part]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAMulti(harness);

    std::vector<ParameterCase> cases;
    for (const akm::harness::MultiPartParameterCase& shared : akm::harness::allMultiPartParameterCases())
        cases.push_back({shared.setId, shared.getId, shared.values});
    REQUIRE(cases.size() == 12);
    akm::test::checkParameterRoundTrips(harness, cases);
}

TEST_CASE("Given a value set on part 2, When parts 1, 3 and 128 minus one are read back, Then they are unchanged [RQ-AKM-089]",
          "[akm][multi][multi-part]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAMulti(harness);

    const CommandResult setResult = *harness.submitAndWait(akm::makeRequest(akm::ItemId::MultiSetLevel, {2, 75}));
    REQUIRE(std::holds_alternative<akm::Done>(setResult));

    for (const std::int64_t part : {1, 3, 127})
    {
        const CommandResult getResult = *harness.submitAndWait(akm::makeRequest(akm::ItemId::MultiGetLevel, {part}));
        REQUIRE(std::holds_alternative<akm::Reply>(getResult));
        const auto decoded = akm::decodeReply(akm::ItemId::MultiGetLevel, std::get<akm::Reply>(getResult).data);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == std::vector<std::int64_t>{0});
    }
}

TEST_CASE("Given no multi is current, When a part parameter is set or read, Then the ERROR 04 is reported [RQ-AKM-089]",
          "[akm][multi][multi-part]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    const CommandResult setResult = *harness.submitAndWait(akm::makeRequest(akm::ItemId::MultiSetLevel, {0, 50}));
    REQUIRE(std::holds_alternative<akm::Error>(setResult));
    CHECK(std::get<akm::Error>(setResult).number == akm::error_number::NOT_FOUND);
    const CommandResult getResult = *harness.submitAndWait(akm::makeRequest(akm::ItemId::MultiGetLevel, {0}));
    REQUIRE(std::holds_alternative<akm::Error>(getResult));
    CHECK(std::get<akm::Error>(getResult).number == akm::error_number::NOT_FOUND);
}

TEST_CASE("Given a part number above 127 or a value out of its range, When set, Then it is refused without sending [RQ-AKM-089]",
          "[akm][multi][multi-part]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAMulti(harness);

    for (const std::vector<std::int64_t>& values : std::vector<std::vector<std::int64_t>>{{128, 50}, {0, 101}})
    {
        const CommandResult result = *harness.submitAndWait(akm::makeRequest(akm::ItemId::MultiSetLevel, values));
        REQUIRE(std::holds_alternative<Refused>(result));
        CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
    }
    const CommandResult pan = *harness.submitAndWait(akm::makeRequest(akm::ItemId::MultiSetPanBalance, {0, 13}));
    REQUIRE(std::holds_alternative<Refused>(pan));
    CHECK(std::get<Refused>(pan).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 1);  // only the Create Multi
}
