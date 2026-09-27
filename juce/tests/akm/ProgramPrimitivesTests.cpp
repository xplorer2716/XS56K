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

// The program lifecycle primitives of section 0A on a session and the simulated sampler: create (plain
// and with keygroups), select by name and by index, delete, rename, and the two Gets (count, current
// name) their own round-trip verification needs. Real-sampler verification is TASK-AKM-024's (RQ-AKM-027
// needs its dedicated test program first). [TASK-AKM-015, RQ-AKM-021, RQ-AKM-023, ADR-AKM-001
// (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/ProgramPrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::CommandResult;
using akm::Done;
using akm::Error;
using akm::ProgramCountResult;
using akm::ProgramNameResult;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::ManualScenarioDriver;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;
using akm::test::bytes;

namespace
{
    ProgramNameResult getName(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<ProgramNameResult>>();
        akm::getCurrentProgramName(harness.session(), [latched](const ProgramNameResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    ProgramCountResult getCount(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<ProgramCountResult>>();
        akm::getProgramCount(harness.session(), [latched](const ProgramCountResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }
}

TEST_CASE("Given a simulated sampler, When a program named TESTPRG is created then selected by name, Then the frame carries the ASCII name null-terminated and the commands complete on DONE [RQ-AKM-021]",
          "[akm][program]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    akm::createProgram(harness.session(), "TESTPRG", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    // A fixed-length slice, not "to end() - 1": the mode is unknown here, so the frame also carries a
    // trailing checksum (Command.cpp appends one whenever the mode is not known to be Off).
    const Bytes createFrame = harness.sentFrames().back();
    constexpr std::size_t dataStart = akm::test::SENT_ITEM_INDEX + 1;
    CHECK(Bytes(createFrame.begin() + dataStart, createFrame.begin() + dataStart + 8)
          == bytes({0x54, 0x45, 0x53, 0x54, 0x50, 0x52, 0x47, 0x00}));

    akm::selectProgramByName(harness.session(), "TESTPRG", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    const ProgramNameResult name = getName(harness);
    REQUIRE(name.name.has_value());
    CHECK(*name.name == "TESTPRG");
}

TEST_CASE("Given a name that does not exist, When it is selected, Then ERROR 04 is reported [RQ-AKM-021]",
          "[akm][program]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    akm::selectProgramByName(harness.session(), "NOPE", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    const CommandResult result = harness.recorder().results().back();
    REQUIRE(std::holds_alternative<Error>(result));
    CHECK(std::get<Error>(result).number == akm::error_number::NOT_FOUND);
}

TEST_CASE("Given a name that already exists, When created again, Then ERROR 05 is reported [RQ-AKM-021]",
          "[akm][program]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    akm::createProgram(harness.session(), "DUP", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));

    akm::createProgram(harness.session(), "DUP", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    const CommandResult result = harness.recorder().results().back();
    REQUIRE(std::holds_alternative<Error>(result));
    CHECK(std::get<Error>(result).number == akm::error_number::COULD_NOT_CREATE);
}

TEST_CASE("Given 4 keygroups and the name AB, When a program is created with keygroups, Then the frame carries the count byte then the name null-terminated, and completes on DONE [RQ-AKM-021]",
          "[akm][program]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    akm::createProgramWithKeygroups(harness.session(), 4, "AB", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    // A fixed-length slice: the mode is unknown here, so the frame also carries a trailing checksum.
    const Bytes frame = harness.sentFrames().back();
    constexpr std::size_t dataStart = akm::test::SENT_ITEM_INDEX + 1;
    CHECK(Bytes(frame.begin() + dataStart, frame.begin() + dataStart + 4) == bytes({4, 0x41, 0x42, 0x00}));
}

TEST_CASE("Given a keygroup count of 0 or 100, When a program is created with keygroups, Then it is refused as out of range without sending [RQ-AKM-021, RQ-AKM-022]",
          "[akm][program]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    for (const int count : {0, 100})
        akm::createProgramWithKeygroups(harness.session(), count, "AB", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    for (const CommandResult& result : harness.recorder().results())
    {
        REQUIRE(std::holds_alternative<Refused>(result));
        CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
    }
    CHECK(harness.sentCount() == 0);
}

TEST_CASE("Given programs created in order, When selected by index and the current one deleted, Then the name and the count follow [RQ-AKM-021]",
          "[akm][program]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    akm::createProgram(harness.session(), "A", harness.recorder().completion());
    akm::createProgram(harness.session(), "B", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(getCount(harness).count == 2);

    akm::selectProgramByIndex(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(getName(harness).name == "A");

    akm::deleteCurrentProgram(harness.session(), harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(4));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(getCount(harness).count == 1);
}

TEST_CASE("Given an index that names no program, When selected, Then ERROR 04 is reported [RQ-AKM-021]",
          "[akm][program]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    akm::selectProgramByIndex(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    const CommandResult result = harness.recorder().results().back();
    REQUIRE(std::holds_alternative<Error>(result));
    CHECK(std::get<Error>(result).number == akm::error_number::NOT_FOUND);
}

TEST_CASE("Given the current program, When renamed, Then Get Current Program Name reports the new name [RQ-AKM-021]",
          "[akm][program]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    akm::createProgram(harness.session(), "OLD", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));

    akm::renameCurrentProgram(harness.session(), "NEW", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(getName(harness).name == "NEW");
}

TEST_CASE("Given the checksum mode unknown, When Get Current Program Name is requested, Then it is refused as ChecksumModeUnknown without sending, since its REPLY has no fixed length [RQ-AKM-021, RQ-AKM-041, ADR-AKM-001 (DEC-AKM-013)]",
          "[akm][program]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    const ProgramNameResult result = getName(harness);
    REQUIRE(std::holds_alternative<Refused>(result.outcome));
    CHECK(std::get<Refused>(result.outcome).reason == RefusalReason::ChecksumModeUnknown);
    CHECK_FALSE(result.name.has_value());
    CHECK(harness.sentCount() == 0);
}

TEST_CASE("Given the default session settings, When a program is created and selected, Then Sync LCD is left untouched: no primitive of this lot toggles it [RQ-AKM-021, FTR-AKM-002 open point]",
          "[akm][program]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    const bool syncLcdBefore = harness.sampler().settings().syncLcd;

    akm::createProgram(harness.session(), "A", harness.recorder().completion());
    akm::selectProgramByName(harness.session(), "A", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));

    CHECK(harness.sampler().settings().syncLcd == syncLcdBefore);
    for (const Bytes& frame : harness.sentFrames())
        CHECK(frame.at(akm::test::SENT_SECTION_INDEX) == 0x0A);
}
