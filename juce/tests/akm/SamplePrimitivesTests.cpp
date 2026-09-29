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

// The sample lifecycle primitives of section 0E on a session and the simulated sampler: select by name
// and by index, delete, rename, start/stop audition, and the two Gets (current index, current name)
// this lot's own round-trip verification needs. Unlike Program, §0E has no "create": a sample only
// exists once `setSampleNames` seeds it (the same list §06/&01's real-sampler tests already use, RQ-
// AKM-038). Real-sampler verification is TASK-AKM-045's (RQ-AKM-051 needs its dedicated test sample
// first). [TASK-AKM-040, RQ-AKM-045, RQ-AKM-047, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/SamplePrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::CommandResult;
using akm::Done;
using akm::Error;
using akm::RefusalReason;
using akm::Refused;
using akm::SampleIndexResult;
using akm::SampleNameResult;
using akm::harness::ManualScenarioDriver;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;
using akm::test::bytes;

namespace
{
    SampleNameResult getName(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<SampleNameResult>>();
        akm::getCurrentSampleName(harness.session(), [latched](const SampleNameResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    SampleIndexResult getIndex(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<SampleIndexResult>>();
        akm::getCurrentSampleIndex(harness.session(), [latched](const SampleIndexResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }
}

TEST_CASE("Given a simulated sampler holding a sample SNARE, When it is selected by name then renamed to SNARE2, Then the frame carries the ASCII name null-terminated, the commands complete on DONE and Get Current Sample's Name returns SNARE2 [RQ-AKM-045]",
          "[akm][sample]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setSampleNames({"SNARE"});

    akm::selectSampleByName(harness.session(), "SNARE", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    const Bytes selectFrame = harness.sentFrames().back();
    constexpr std::size_t dataStart = akm::test::SENT_ITEM_INDEX + 1;
    CHECK(Bytes(selectFrame.begin() + dataStart, selectFrame.begin() + dataStart + 6)
          == bytes({0x53, 0x4E, 0x41, 0x52, 0x45, 0x00}));

    akm::renameCurrentSample(harness.session(), "SNARE2", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(getName(harness).name == "SNARE2");
}

TEST_CASE("Given a name that does not exist, When it is selected, Then the ERROR 04 is reported [RQ-AKM-045]",
          "[akm][sample]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    akm::selectSampleByName(harness.session(), "NOPE", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    const CommandResult result = harness.recorder().results().back();
    REQUIRE(std::holds_alternative<Error>(result));
    CHECK(std::get<Error>(result).number == akm::error_number::NOT_FOUND);
}

TEST_CASE("Given samples seeded in order, When selected by index and the current one deleted, Then the index, the name and the deletion follow [RQ-AKM-045]",
          "[akm][sample]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setSampleNames({"A", "B"});

    akm::selectSampleByIndex(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(getIndex(harness).index == 0);
    CHECK(getName(harness).name == "A");

    akm::deleteCurrentSample(harness.session(), harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK_FALSE(getIndex(harness).index.has_value());

    // "A" is gone, so index 0 now names what was "B".
    akm::selectSampleByIndex(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(getName(harness).name == "B");
}

TEST_CASE("Given an index that names no sample, When selected, Then the ERROR 04 is reported [RQ-AKM-045]",
          "[akm][sample]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    akm::selectSampleByIndex(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    const CommandResult result = harness.recorder().results().back();
    REQUIRE(std::holds_alternative<Error>(result));
    CHECK(std::get<Error>(result).number == akm::error_number::NOT_FOUND);
}

TEST_CASE("Given no sample is current, When the current sample is renamed, deleted or its audition started or stopped, Then the ERROR 04 is reported [RQ-AKM-045]",
          "[akm][sample]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    akm::renameCurrentSample(harness.session(), "X", harness.recorder().completion());
    akm::deleteCurrentSample(harness.session(), harness.recorder().completion());
    akm::startSampleAudition(harness.session(), harness.recorder().completion());
    akm::stopSampleAudition(harness.session(), harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(4));
    for (const CommandResult& result : harness.recorder().results())
    {
        REQUIRE(std::holds_alternative<Error>(result));
        CHECK(std::get<Error>(result).number == akm::error_number::NOT_FOUND);
    }
}

TEST_CASE("Given the current sample, When auditioning is started then stopped, Then both commands complete on DONE [RQ-AKM-045]",
          "[akm][sample]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    harness.sampler().setSampleNames({"KICK"});

    akm::selectSampleByName(harness.session(), "KICK", harness.recorder().completion());
    akm::startSampleAudition(harness.session(), harness.recorder().completion());
    akm::stopSampleAudition(harness.session(), harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    for (const CommandResult& result : harness.recorder().results())
        CHECK(std::holds_alternative<Done>(result));
}

TEST_CASE("Given the checksum mode unknown, When Get Current Sample's Name is requested, Then it is refused as ChecksumModeUnknown without sending, since its REPLY has no fixed length [RQ-AKM-045, RQ-AKM-041, ADR-AKM-001 (DEC-AKM-013)]",
          "[akm][sample]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    const SampleNameResult result = getName(harness);
    REQUIRE(std::holds_alternative<Refused>(result.outcome));
    CHECK(std::get<Refused>(result.outcome).reason == RefusalReason::ChecksumModeUnknown);
    CHECK_FALSE(result.name.has_value());
    CHECK(harness.sentCount() == 0);
}
