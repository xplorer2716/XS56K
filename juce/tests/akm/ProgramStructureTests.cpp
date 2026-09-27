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

// The program structure and identity primitives of section 0A (&0A-&0D, &11, &14, &15): Program Number,
// keygroup count and crossfade. [TASK-AKM-016, RQ-AKM-022]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <variant>

#include "SessionHarness.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/ProgramPrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::CommandResult;
using akm::Done;
using akm::Error;
using akm::ProgramCrossfadeResult;
using akm::ProgramKeygroupCountResult;
using akm::ProgramNumberResult;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::ManualScenarioDriver;
using akm::test::Latched;
using akm::test::SessionHarness;

namespace
{
    void createAndSelectAProgram(SessionHarness& harness)
    {
        akm::createProgram(harness.session(), "A", harness.recorder().completion());
        REQUIRE(harness.waitForCompletions(1));
    }

    ProgramKeygroupCountResult getKeygroupCount(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<ProgramKeygroupCountResult>>();
        akm::getProgramKeygroupCount(harness.session(), [latched](const ProgramKeygroupCountResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    ProgramCrossfadeResult getCrossfade(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<ProgramCrossfadeResult>>();
        akm::getKeygroupCrossfade(harness.session(), [latched](const ProgramCrossfadeResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    ProgramNumberResult getNumber(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<ProgramNumberResult>>();
        akm::getProgramNumber(harness.session(), [latched](const ProgramNumberResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }
}

TEST_CASE("Given a program with 1 keygroup, When 3 are added, Then Get number of keygroups reports 4 [RQ-AKM-022]",
          "[akm][program][structure]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);
    REQUIRE(getKeygroupCount(harness).count == 1);

    akm::addKeygroupsToProgram(harness.session(), 3, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(getKeygroupCount(harness).count == 4);
}

TEST_CASE("Given the value 0 for keygroups to add, When requested, Then it is refused without sending [RQ-AKM-022]",
          "[akm][program][structure]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const CommandResult result = *harness.submitAndWait(akm::makeRequest(akm::ItemId::ProgramAddKeygroups, {0}));
    REQUIRE(std::holds_alternative<Refused>(result));
    CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 1);  // only the Create Program
}

TEST_CASE("Given a program with 4 keygroups, When keygroup 1 is deleted, Then the count is 3, and deleting an unknown keygroup fails [RQ-AKM-022]",
          "[akm][program][structure]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);
    akm::addKeygroupsToProgram(harness.session(), 3, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));

    akm::deleteKeygroupFromProgram(harness.session(), 1, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(getKeygroupCount(harness).count == 3);

    akm::deleteKeygroupFromProgram(harness.session(), 9, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(4));
    const CommandResult result = harness.recorder().results().back();
    REQUIRE(std::holds_alternative<Error>(result));
    CHECK(std::get<Error>(result).number == akm::error_number::KEYGROUP_NOT_IN_PROGRAM);
}

TEST_CASE("Given crossfade set to ON, When Get crossfade runs, Then it reports ON [RQ-AKM-022]",
          "[akm][program][structure]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);
    CHECK(getCrossfade(harness).enabled == false);

    akm::setKeygroupCrossfade(harness.session(), true, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(getCrossfade(harness).enabled == true);
}

TEST_CASE("Given a front-panel Program Number, When set then read back, Then the same front-panel number comes back, and switching it off clears it [RQ-AKM-022]",
          "[akm][program][structure]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);
    CHECK_FALSE(getNumber(harness).frontPanelNumber.has_value());

    akm::setProgramNumber(harness.session(), 1, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    // Table 13 footnote a: 1 on the front panel is 0 on the wire.
    CHECK(harness.sentFrames().back().at(akm::test::SENT_ITEM_INDEX + 2) == 0x00);
    REQUIRE(getNumber(harness).frontPanelNumber.has_value());
    CHECK(*getNumber(harness).frontPanelNumber == 1);

    akm::setProgramNumber(harness.session(), 128, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    CHECK(harness.sentFrames().back().at(akm::test::SENT_ITEM_INDEX + 2) == 127);
    CHECK(*getNumber(harness).frontPanelNumber == 128);

    akm::setProgramNumber(harness.session(), std::nullopt, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(4));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK_FALSE(getNumber(harness).frontPanelNumber.has_value());
}

TEST_CASE("Given a front-panel number of 0 or 129, When set, Then it is refused without sending [RQ-AKM-022]",
          "[akm][program][structure]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    for (const int number : {0, 129})
    {
        akm::setProgramNumber(harness.session(), number, harness.recorder().completion());
    }
    REQUIRE(harness.waitForCompletions(3));
    for (std::size_t i = 1; i < 3; ++i)
    {
        REQUIRE(std::holds_alternative<Refused>(harness.recorder().results()[i]));
        CHECK(std::get<Refused>(harness.recorder().results()[i]).reason == RefusalReason::ArgumentOutOfRange);
    }
    CHECK(harness.sentCount() == 1);  // only the Create Program
}
