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

// General program information of section 0A: the current program's index (&12), and the "Program
// Numbers" (&18) and names (&19) of every program in memory. [TASK-AKM-017, RQ-AKM-023]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/ProgramPrimitives.hpp"

using akm::AllProgramNamesResult;
using akm::AllProgramNumbersResult;
using akm::CommandResult;
using akm::Done;
using akm::ProgramIndexResult;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::ManualScenarioDriver;
using akm::test::Latched;
using akm::test::SessionHarness;

namespace
{
    void createProgram(SessionHarness& harness, const char* name)
    {
        akm::createProgram(harness.session(), name, harness.recorder().completion());
    }

    AllProgramNumbersResult getAllNumbers(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<AllProgramNumbersResult>>();
        akm::getAllProgramNumbers(harness.session(), [latched](const AllProgramNumbersResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    AllProgramNamesResult getAllNames(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<AllProgramNamesResult>>();
        akm::getAllProgramNames(harness.session(), [latched](const AllProgramNamesResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    ProgramIndexResult getIndex(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<ProgramIndexResult>>();
        akm::getProgramIndex(harness.session(), [latched](const ProgramIndexResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }
}

TEST_CASE("Given no programs in memory, When the names and numbers of all programs are read, Then both lists are empty [RQ-AKM-023]",
          "[akm][program][generalinfo]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    const AllProgramNamesResult names = getAllNames(harness);
    REQUIRE(names.names.has_value());
    CHECK(names.names->empty());

    const AllProgramNumbersResult numbers = getAllNumbers(harness);
    REQUIRE(numbers.numbers.has_value());
    CHECK(numbers.numbers->empty());
}

TEST_CASE("Given one program in memory, When the names and numbers of all programs are read, Then each list holds one entry [RQ-AKM-023]",
          "[akm][program][generalinfo]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    createProgram(harness, "A");
    REQUIRE(harness.waitForCompletions(1));

    const AllProgramNamesResult names = getAllNames(harness);
    REQUIRE(names.names.has_value());
    CHECK(*names.names == std::vector<std::string>{"A"});

    const AllProgramNumbersResult numbers = getAllNumbers(harness);
    REQUIRE(numbers.numbers.has_value());
    REQUIRE(numbers.numbers->size() == 1);
    CHECK_FALSE((*numbers.numbers)[0].has_value());
}

TEST_CASE("Given a simulated sampler holding programs A, B, C, When the names of all programs are read, Then the result is the ordered list A, B, C [RQ-AKM-023]",
          "[akm][program][generalinfo]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    createProgram(harness, "A");
    createProgram(harness, "B");
    createProgram(harness, "C");
    REQUIRE(harness.waitForCompletions(3));

    const AllProgramNamesResult names = getAllNames(harness);
    REQUIRE(names.names.has_value());
    CHECK(*names.names == std::vector<std::string>{"A", "B", "C"});
}

TEST_CASE("Given programs with front-panel numbers set on some of them, When all program numbers are read, Then each is reported in memory order [RQ-AKM-023]",
          "[akm][program][generalinfo]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    createProgram(harness, "A");
    createProgram(harness, "B");
    REQUIRE(harness.waitForCompletions(2));

    // "A" is current-most-recent is "B"; select "A" back to give it a number.
    akm::selectProgramByIndex(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    akm::setProgramNumber(harness.session(), 5, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(4));

    const AllProgramNumbersResult numbers = getAllNumbers(harness);
    REQUIRE(numbers.numbers.has_value());
    REQUIRE(numbers.numbers->size() == 2);
    REQUIRE((*numbers.numbers)[0].has_value());
    CHECK(*(*numbers.numbers)[0] == 5);
    CHECK_FALSE((*numbers.numbers)[1].has_value());
}

TEST_CASE("Given the current program, When its index is read, Then it matches its position in memory [RQ-AKM-023]",
          "[akm][program][generalinfo]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createProgram(harness, "A");
    createProgram(harness, "B");
    REQUIRE(harness.waitForCompletions(2));
    CHECK(getIndex(harness).index == 1);  // "B" was created (and made current) last

    akm::selectProgramByIndex(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    CHECK(getIndex(harness).index == 0);
}

TEST_CASE("Given the checksum mode unknown, When all program numbers or all program names are requested, Then each is refused as ChecksumModeUnknown without sending [RQ-AKM-023, RQ-AKM-041]",
          "[akm][program][generalinfo]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    const AllProgramNumbersResult numbers = getAllNumbers(harness);
    REQUIRE(std::holds_alternative<Refused>(numbers.outcome));
    CHECK(std::get<Refused>(numbers.outcome).reason == RefusalReason::ChecksumModeUnknown);

    const AllProgramNamesResult names = getAllNames(harness);
    REQUIRE(std::holds_alternative<Refused>(names.outcome));
    CHECK(std::get<Refused>(names.outcome).reason == RefusalReason::ChecksumModeUnknown);
    CHECK(harness.sentCount() == 0);
}
