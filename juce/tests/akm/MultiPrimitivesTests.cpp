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

// The multi lifecycle primitives of section 0C on a session and the simulated sampler: create, select by name and
// by index, delete the current multi, set the number of parts of new multis, and the two Gets (current index,
// current name) this lot's own round trips need. Like a program, a multi is created by name and becomes current.
// [TASK-AKM-089, RQ-AKM-087, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <variant>

#include "SessionHarness.hpp"
#include "akm/MultiPrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::CommandResult;
using akm::Done;
using akm::Error;
using akm::MultiIndexResult;
using akm::MultiNameResult;
using akm::MultiPartCount;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::ManualScenarioDriver;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;
using akm::test::bytes;

namespace
{
    MultiNameResult getName(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<MultiNameResult>>();
        akm::getCurrentMultiName(harness.session(), [latched](const MultiNameResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    MultiIndexResult getIndex(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<MultiIndexResult>>();
        akm::getCurrentMultiIndex(harness.session(), [latched](const MultiIndexResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    void requireError(const CommandResult& result, std::uint16_t number)
    {
        REQUIRE(std::holds_alternative<Error>(result));
        CHECK(std::get<Error>(result).number == number);
    }
}

TEST_CASE("Given a simulated sampler, When a multi MIX1 is created, Then it is current, the frame carries the ASCII name null-terminated and the sampler holds one multi more [RQ-AKM-087]",
          "[akm][multi]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setMultiNames({"OLD"});

    akm::createMulti(harness.session(), "MIX1", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    const Bytes frame = harness.sentFrames().back();
    constexpr std::size_t dataStart = akm::test::SENT_ITEM_INDEX + 1;
    CHECK(Bytes(frame.begin() + dataStart, frame.begin() + dataStart + 5) == bytes({0x4D, 0x49, 0x58, 0x31, 0x00}));
    CHECK(getName(harness).name == "MIX1");
    CHECK(getIndex(harness).index == 1);
    CHECK(harness.sampler().multiCount() == 2);
}

TEST_CASE("Given the number of parts of new multis set to 64, When a multi is created, Then the frame carried 01 and the multi has 64 parts, and the default is 32 [RQ-AKM-087]",
          "[akm][multi]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    akm::createMulti(harness.session(), "SMALL", harness.recorder().completion());
    akm::setNewMultiPartCount(harness.session(), MultiPartCount::Parts64, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    const Bytes frame = harness.sentFrames().back();
    CHECK(frame[akm::test::SENT_ITEM_INDEX + 1] == 0x01);
    akm::createMulti(harness.session(), "BIG", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    for (const CommandResult& result : harness.recorder().results())
        CHECK(std::holds_alternative<Done>(result));
    CHECK(harness.sampler().multiPartCount(0) == 32);
    CHECK(harness.sampler().multiPartCount(1) == 64);

    akm::setNewMultiPartCount(harness.session(), MultiPartCount::Parts128, harness.recorder().completion());
    akm::createMulti(harness.session(), "HUGE", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(5));
    CHECK(harness.sampler().multiPartCount(2) == 128);
}

TEST_CASE("Given a name or an index that no multi has, When it is selected, Then the ERROR 04 is reported [RQ-AKM-087]",
          "[akm][multi]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    harness.sampler().setMultiNames({"ONLY"});

    akm::selectMultiByName(harness.session(), "NOPE", harness.recorder().completion());
    akm::selectMultiByIndex(harness.session(), 1, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    for (const CommandResult& result : harness.recorder().results())
        requireError(result, akm::error_number::NOT_FOUND);
}

TEST_CASE("Given a multi named like an existing one, When it is created, Then the sampler's ERROR 05 is reported and nothing is added [RQ-AKM-087]",
          "[akm][multi]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    harness.sampler().setMultiNames({"TWIN"});

    akm::createMulti(harness.session(), "TWIN", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    requireError(harness.recorder().results().back(), akm::error_number::COULD_NOT_CREATE);
    CHECK(harness.sampler().multiCount() == 1);
}

TEST_CASE("Given multis selected by name and by index, When the current index and name are read, Then they follow the selection [RQ-AKM-087]",
          "[akm][multi]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setMultiNames({"A", "B", "C"});

    akm::selectMultiByName(harness.session(), "B", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(getIndex(harness).index == 1);
    CHECK(getName(harness).name == "B");

    akm::selectMultiByIndex(harness.session(), 2, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    const Bytes frame = harness.sentFrames().back();
    constexpr std::size_t dataStart = akm::test::SENT_ITEM_INDEX + 1;
    CHECK(Bytes(frame.begin() + dataStart, frame.begin() + dataStart + 2) == bytes({0x00, 0x02}));
    CHECK(getIndex(harness).index == 2);
    CHECK(getName(harness).name == "C");
}

TEST_CASE("Given the current multi deleted, When the sampler is asked, Then it holds one multi less, none is current and the others keep their order [RQ-AKM-087]",
          "[akm][multi]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setMultiNames({"A", "B"});

    akm::selectMultiByIndex(harness.session(), 0, harness.recorder().completion());
    akm::deleteCurrentMulti(harness.session(), harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    for (const CommandResult& result : harness.recorder().results())
        CHECK(std::holds_alternative<Done>(result));
    CHECK(harness.sampler().multiNames() == std::vector<std::string>{"B"});
    requireError(getIndex(harness).outcome, akm::error_number::NOT_FOUND);
}

TEST_CASE("Given no multi is current, When the current one is deleted or its index or name is read, Then the ERROR 04 is reported [RQ-AKM-087]",
          "[akm][multi]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setMultiNames({"A"});

    akm::deleteCurrentMulti(harness.session(), harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    requireError(harness.recorder().results().back(), akm::error_number::NOT_FOUND);
    requireError(getIndex(harness).outcome, akm::error_number::NOT_FOUND);
    requireError(getName(harness).outcome, akm::error_number::NOT_FOUND);
    CHECK(harness.sampler().multiCount() == 1);
}

TEST_CASE("Given an index above 14 bits or a name that is not 7-bit ASCII, When a multi is selected or created, Then it is refused without sending [RQ-AKM-087, RQ-AKM-002]",
          "[akm][multi]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    akm::selectMultiByIndex(harness.session(), 16384, harness.recorder().completion());
    akm::createMulti(harness.session(), "caf\xC3\xA9", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    const auto& results = harness.recorder().results();
    REQUIRE(std::holds_alternative<Refused>(results[0]));
    CHECK(std::get<Refused>(results[0]).reason == RefusalReason::ArgumentOutOfRange);
    REQUIRE(std::holds_alternative<Refused>(results[1]));
    CHECK(std::get<Refused>(results[1]).reason == RefusalReason::NotEncodable);
    CHECK(harness.sentCount() == 0);
}

TEST_CASE("Given the checksum mode unknown, When the current multi's name is requested, Then it is refused as ChecksumModeUnknown without sending [RQ-AKM-087, RQ-AKM-041, ADR-AKM-001 (DEC-AKM-013)]",
          "[akm][multi]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    const MultiNameResult result = getName(harness);
    REQUIRE(std::holds_alternative<Refused>(result.outcome));
    CHECK(std::get<Refused>(result.outcome).reason == RefusalReason::ChecksumModeUnknown);
    CHECK_FALSE(result.name.has_value());
    CHECK(harness.sentCount() == 0);
}
