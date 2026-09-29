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

// The destructive guard on "Delete ALL samples from memory" (§0E/&07): sent only with its explicit
// confirmation. [TASK-AKM-041, RQ-AKM-046]
#include <catch2/catch_test_macros.hpp>

#include <variant>

#include "SessionHarness.hpp"
#include "akm/SamplePrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::CommandResult;
using akm::ConfirmDeleteAllSamples;
using akm::Done;
using akm::Error;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::ManualScenarioDriver;
using akm::test::SessionHarness;

TEST_CASE("Given a request to delete ALL samples without its confirmation, When made, Then nothing is sent and NotConfirmed explains why [RQ-AKM-046]",
          "[akm][sample][delete-all-guard]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    akm::deleteAllSamples(harness.session(), std::nullopt, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));

    const CommandResult result = harness.recorder().results().back();
    REQUIRE(std::holds_alternative<Refused>(result));
    CHECK(std::get<Refused>(result).reason == RefusalReason::NotConfirmed);
    CHECK(harness.sentCount() == 0);
}

TEST_CASE("Given the explicit confirmation, When Delete ALL samples is requested, Then it is sent and every sample is gone [RQ-AKM-046]",
          "[akm][sample][delete-all-guard]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    harness.sampler().setSampleNames({"A", "B"});

    akm::deleteAllSamples(harness.session(), ConfirmDeleteAllSamples::IUnderstandThisDeletesEverySampleInMemory,
                          harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(harness.sentCount() == 1);

    akm::selectSampleByIndex(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    const CommandResult selectResult = harness.recorder().results().back();
    REQUIRE(std::holds_alternative<Error>(selectResult));
    CHECK(std::get<Error>(selectResult).number == akm::error_number::NOT_FOUND);
}
