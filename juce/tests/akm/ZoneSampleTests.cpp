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

// Zone sample assignment (§06/&01, &21, spec Tables 9-10): a mixed byte+String shape, so `setZoneSample`
// builds the frame by hand rather than through `makeStringRequest` (ADR-AKM-001, DEC-AKM-013), the same
// way `createProgramWithKeygroups` does for its own count+name shape. [TASK-AKM-036, RQ-AKM-035]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "TestBytes.hpp"
#include "akm/ProgramPrimitives.hpp"
#include "akm/SamplerError.hpp"
#include "akm/ZonePrimitives.hpp"

using akm::CommandResult;
using akm::Done;
using akm::Error;
using akm::RefusalReason;
using akm::Refused;
using akm::ZoneSampleResult;
using akm::harness::ManualScenarioDriver;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;
using akm::test::bytes;

namespace
{
    void createAndSelectAProgram(SessionHarness& harness)
    {
        akm::createProgram(harness.session(), "A", harness.recorder().completion());
        REQUIRE(harness.waitForCompletions(1));
    }

    ZoneSampleResult getSample(SessionHarness& harness, int zone)
    {
        auto latched = std::make_shared<Latched<ZoneSampleResult>>();
        akm::getZoneSample(harness.session(), zone, [latched](const ZoneSampleResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }
}

TEST_CASE("Given a simulated sampler holding a sample KICK, When it is assigned to zone 1, Then the frame "
          "carries section 06, item 01, data 01 then KICK null-terminated, the command completes on DONE "
          "and Get Zone Sample returns KICK [RQ-AKM-035]",
          "[akm][zone]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setSampleNames({"KICK"});
    createAndSelectAProgram(harness);

    akm::setZoneSample(harness.session(), 1, "KICK", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    const Bytes frame = harness.sentFrames().back();
    constexpr std::size_t dataStart = akm::test::SENT_ITEM_INDEX + 1;
    CHECK(Bytes(frame.begin() + dataStart, frame.begin() + dataStart + 6)
          == bytes({0x01, 0x4B, 0x49, 0x43, 0x4B, 0x00}));

    const ZoneSampleResult result = getSample(harness, 1);
    REQUIRE(result.name.has_value());
    CHECK(*result.name == "KICK");
}

TEST_CASE("Given a zone with no sample assigned, When Get Zone Sample is read, Then an empty name is "
          "returned (the REPLY's single byte 00) [RQ-AKM-035]",
          "[akm][zone]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    createAndSelectAProgram(harness);

    const ZoneSampleResult result = getSample(harness, 2);
    REQUIRE(result.name.has_value());
    CHECK(result.name->empty());
}

TEST_CASE("Given a name that does not exist, When it is assigned to a zone, Then ERROR 04 is reported [RQ-AKM-035]",
          "[akm][zone]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    akm::setZoneSample(harness.session(), 1, "NOPE", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    const CommandResult result = harness.recorder().results().back();
    REQUIRE(std::holds_alternative<Error>(result));
    CHECK(std::get<Error>(result).number == akm::error_number::NOT_FOUND);
}

TEST_CASE("Given a name containing a byte above 7F, When it is assigned to a zone, Then it is refused "
          "without sending [RQ-AKM-035]",
          "[akm][zone]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    const std::string invalid = "K\x80" "CK";  // "\x" consumes every following hex digit, hence the split
    akm::setZoneSample(harness.session(), 1, invalid, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    const CommandResult result = harness.recorder().results().back();
    REQUIRE(std::holds_alternative<Refused>(result));
    CHECK(std::get<Refused>(result).reason == RefusalReason::NotEncodable);
}

TEST_CASE("Given a zone number outside 0-4, When a sample is assigned, Then it is refused without sending [RQ-AKM-035]",
          "[akm][zone]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    createAndSelectAProgram(harness);

    akm::setZoneSample(harness.session(), 5, "KICK", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    const CommandResult result = harness.recorder().results().back();
    REQUIRE(std::holds_alternative<Refused>(result));
    CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
}
