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

// The Gets of general information of section 0C on a session and the simulated sampler: the number of multis
// (&40), the current multi's program number (&41), number of parts (&44), the name of a part (&45), the names of
// all its parts (&46), all the parameters of a part (&47), its mute and solo status (&48), and the three "all the
// multis" Gets (&50, &51, &52). [TASK-AKM-092, RQ-AKM-090, RQ-AKM-091, ADR-AKM-001 (DEC-AKM-013, DEC-AKM-014,
// DEC-AKM-015)]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/MultiPrimitives.hpp"
#include "akm/SamplerError.hpp"
#include "akm/harness/MultiPartParameterCases.hpp"

using akm::CommandResult;
using akm::Error;
using akm::MultiPartCount;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::ManualScenarioDriver;
using akm::test::Latched;
using akm::test::SessionHarness;

namespace
{
    template <typename Result, typename Launch>
    Result await(SessionHarness& harness, Launch launch)
    {
        auto latched = std::make_shared<Latched<Result>>();
        launch([latched](const Result& result) { latched->set(result); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    void requireNotFound(const CommandResult& result)
    {
        REQUIRE(std::holds_alternative<Error>(result));
        CHECK(std::get<Error>(result).number == akm::error_number::NOT_FOUND);
    }

    void requireChecksumModeRefusal(const CommandResult& result)
    {
        REQUIRE(std::holds_alternative<Refused>(result));
        CHECK(std::get<Refused>(result).reason == RefusalReason::ChecksumModeUnknown);
    }

    // A sampler holding `A` (32 parts) and `B` (64 parts), `B` current.
    void holdTwoMultis(SessionHarness& harness)
    {
        REQUIRE(harness.establishChecksumMode(false).has_value());
        harness.sampler().setMultiNames({"A"});
        akm::setNewMultiPartCount(harness.session(), MultiPartCount::Parts64, harness.recorder().completion());
        akm::createMulti(harness.session(), "B", harness.recorder().completion());
        REQUIRE(harness.waitForCompletions(2));
    }
}

TEST_CASE("Given a simulated sampler holding A (32 parts) and B (64 parts), When the count, the names and the numbers of parts of all multis are read, Then they are 2, A and B, and 32 and 64 in memory order [RQ-AKM-091]",
          "[akm][multi][multi-info]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    holdTwoMultis(harness);

    CHECK(await<akm::MultiCountResult>(harness, [&](auto done) { akm::getMultiCount(harness.session(), done); }).count == 2);
    const auto names = await<akm::MultiNameListResult>(harness, [&](auto done) { akm::getAllMultiNames(harness.session(), done); });
    REQUIRE(names.names.has_value());
    CHECK(*names.names == std::vector<std::string>{"A", "B"});
    const auto counts = await<akm::MultiValueListResult>(harness, [&](auto done) { akm::getAllMultiPartCounts(harness.session(), done); });
    REQUIRE(counts.values.has_value());
    CHECK(*counts.values == std::vector<int>{32, 64});
    const auto current = await<akm::MultiPartCountResult>(harness, [&](auto done) { akm::getCurrentMultiPartCount(harness.session(), done); });
    CHECK(current.partCount == 64);
}

TEST_CASE("Given a multi with a program number set and one without, When &41 and &50 are read, Then the flag and the front-panel number agree [RQ-AKM-091]",
          "[akm][multi][multi-info]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    holdTwoMultis(harness);
    harness.sampler().setMultiProgramNumber(1, 4);

    const auto current = await<akm::MultiProgramNumberResult>(harness, [&](auto done) { akm::getMultiProgramNumber(harness.session(), done); });
    CHECK(current.frontPanelNumber == 5);
    const auto all = await<akm::MultiProgramNumbersResult>(harness, [&](auto done) { akm::getAllMultiProgramNumbers(harness.session(), done); });
    REQUIRE(all.numbers.has_value());
    REQUIRE(all.numbers->size() == 2);
    CHECK_FALSE((*all.numbers)[0].has_value());
    CHECK((*all.numbers)[1] == 5);

    harness.sampler().setMultiProgramNumber(1, std::nullopt);
    CHECK_FALSE(await<akm::MultiProgramNumberResult>(harness, [&](auto done) { akm::getMultiProgramNumber(harness.session(), done); })
                    .frontPanelNumber.has_value());
}

TEST_CASE("Given a part with no program assigned and a part with one, When their names are read, Then the first is empty, not a failure, and all 64 names come in part order [RQ-AKM-091]",
          "[akm][multi][multi-info]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    holdTwoMultis(harness);
    harness.sampler().setMultiPartProgram(1, 2, "LEAD");

    const auto none = await<akm::MultiNameResult>(harness, [&](auto done) { akm::getMultiPartName(harness.session(), 0, done); });
    REQUIRE(none.name.has_value());
    CHECK(none.name->empty());
    CHECK(await<akm::MultiNameResult>(harness, [&](auto done) { akm::getMultiPartName(harness.session(), 2, done); }).name == "LEAD");

    const auto all = await<akm::MultiNameListResult>(harness, [&](auto done) { akm::getAllMultiPartNames(harness.session(), done); });
    REQUIRE(all.names.has_value());
    REQUIRE(all.names->size() == 64);
    CHECK((*all.names)[2] == "LEAD");
    CHECK((*all.names)[0].empty());
    CHECK((*all.names)[63].empty());
}

TEST_CASE("Given a part whose twelve parameters were set, When &47 is read, Then the twelve values equal the twelve single Gets in order [RQ-AKM-090]",
          "[akm][multi][multi-info]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    holdTwoMultis(harness);

    std::vector<int> expected;
    for (const akm::harness::MultiPartParameterCase& testCase : akm::harness::allMultiPartParameterCases())
    {
        const CommandResult set = *harness.submitAndWait(akm::makeRequest(testCase.setId, testCase.values));
        REQUIRE(std::holds_alternative<akm::Done>(set));
        expected.push_back(static_cast<int>(testCase.values[1]));
    }

    const auto all = await<akm::MultiValueListResult>(harness, [&](auto done) { akm::getAllMultiPartParameters(harness.session(), 3, done); });
    REQUIRE(all.values.has_value());
    CHECK(*all.values == expected);
    CHECK(expected.size() == 12);
}

TEST_CASE("Given parts 1 muted and 2 soloed in a 32-part multi, When &48 is read, Then 32 values come back with 1 at index 1, 2 at index 2 and 0 elsewhere [RQ-AKM-090]",
          "[akm][multi][multi-info]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    akm::createMulti(harness.session(), "M", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    REQUIRE(std::holds_alternative<akm::Done>(*harness.submitAndWait(akm::makeRequest(akm::ItemId::MultiSetMute, {1, 1}))));
    REQUIRE(std::holds_alternative<akm::Done>(*harness.submitAndWait(akm::makeRequest(akm::ItemId::MultiSetSolo, {2, 1}))));

    const auto status = await<akm::MultiValueListResult>(harness, [&](auto done) { akm::getMultiMuteSoloStatus(harness.session(), done); });
    REQUIRE(status.values.has_value());
    REQUIRE(status.values->size() == 32);
    for (std::size_t part = 0; part < status.values->size(); ++part)
        CHECK((*status.values)[part] == (part == 1 ? 1 : (part == 2 ? 2 : 0)));
}

TEST_CASE("Given no multi is current, When a Get of the current multi is requested, Then the ERROR 04 is reported [RQ-AKM-091, RQ-AKM-090]",
          "[akm][multi][multi-info]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setMultiNames({"A"});

    requireNotFound(await<akm::MultiProgramNumberResult>(harness, [&](auto done) { akm::getMultiProgramNumber(harness.session(), done); }).outcome);
    requireNotFound(await<akm::MultiPartCountResult>(harness, [&](auto done) { akm::getCurrentMultiPartCount(harness.session(), done); }).outcome);
    requireNotFound(await<akm::MultiNameResult>(harness, [&](auto done) { akm::getMultiPartName(harness.session(), 0, done); }).outcome);
    requireNotFound(await<akm::MultiNameListResult>(harness, [&](auto done) { akm::getAllMultiPartNames(harness.session(), done); }).outcome);
    requireNotFound(await<akm::MultiValueListResult>(harness, [&](auto done) { akm::getAllMultiPartParameters(harness.session(), 0, done); }).outcome);
    requireNotFound(await<akm::MultiValueListResult>(harness, [&](auto done) { akm::getMultiMuteSoloStatus(harness.session(), done); }).outcome);
}

TEST_CASE("Given no multi in memory, When the count and the three all-multis Gets are read, Then the count is 0 and each list is empty, not a failure [RQ-AKM-091]",
          "[akm][multi][multi-info]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    CHECK(await<akm::MultiCountResult>(harness, [&](auto done) { akm::getMultiCount(harness.session(), done); }).count == 0);
    const auto names = await<akm::MultiNameListResult>(harness, [&](auto done) { akm::getAllMultiNames(harness.session(), done); });
    REQUIRE(names.names.has_value());
    CHECK(names.names->empty());
    const auto counts = await<akm::MultiValueListResult>(harness, [&](auto done) { akm::getAllMultiPartCounts(harness.session(), done); });
    REQUIRE(counts.values.has_value());
    CHECK(counts.values->empty());
    const auto numbers = await<akm::MultiProgramNumbersResult>(harness, [&](auto done) { akm::getAllMultiProgramNumbers(harness.session(), done); });
    REQUIRE(numbers.numbers.has_value());
    CHECK(numbers.numbers->empty());
}

TEST_CASE("Given the checksum mode unknown, When a name or a variable-length Get of section 0C is requested, Then it is refused as ChecksumModeUnknown without sending [RQ-AKM-090, RQ-AKM-091, RQ-AKM-041]",
          "[akm][multi][multi-info]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    requireChecksumModeRefusal(await<akm::MultiNameResult>(harness, [&](auto done) { akm::getMultiPartName(harness.session(), 0, done); }).outcome);
    requireChecksumModeRefusal(await<akm::MultiNameListResult>(harness, [&](auto done) { akm::getAllMultiPartNames(harness.session(), done); }).outcome);
    requireChecksumModeRefusal(await<akm::MultiNameListResult>(harness, [&](auto done) { akm::getAllMultiNames(harness.session(), done); }).outcome);
    requireChecksumModeRefusal(await<akm::MultiValueListResult>(harness, [&](auto done) { akm::getMultiMuteSoloStatus(harness.session(), done); }).outcome);
    requireChecksumModeRefusal(await<akm::MultiValueListResult>(harness, [&](auto done) { akm::getAllMultiPartCounts(harness.session(), done); }).outcome);
    requireChecksumModeRefusal(await<akm::MultiProgramNumbersResult>(harness, [&](auto done) { akm::getAllMultiProgramNumbers(harness.session(), done); }).outcome);
    CHECK(harness.sentCount() == 0);
}

TEST_CASE("Given a part number above 127, When a part's name or parameters are requested, Then it is refused without sending [RQ-AKM-090, RQ-AKM-091]",
          "[akm][multi][multi-info]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const std::size_t sentBefore = harness.sentCount();

    const auto name = await<akm::MultiNameResult>(harness, [&](auto done) { akm::getMultiPartName(harness.session(), 128, done); });
    REQUIRE(std::holds_alternative<Refused>(name.outcome));
    CHECK(std::get<Refused>(name.outcome).reason == RefusalReason::ArgumentOutOfRange);
    const auto parameters = await<akm::MultiValueListResult>(harness, [&](auto done) { akm::getAllMultiPartParameters(harness.session(), 128, done); });
    REQUIRE(std::holds_alternative<Refused>(parameters.outcome));
    CHECK(std::get<Refused>(parameters.outcome).reason == RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == sentBefore);
}
