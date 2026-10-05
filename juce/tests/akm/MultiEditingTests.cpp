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

// The items of section 0C that set general information about the current multi: rename (&30), program number
// (&31), a part's program by index (&32) or by name (&33), and the deletion of a part's program (&34). &31 and &33
// have shapes the generic encoder cannot build (a flag then a number; a part then a name), so they are built by
// hand. [TASK-AKM-093, RQ-AKM-092, ADR-AKM-001 (DEC-AKM-013)]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "SessionHarness.hpp"
#include "akm/MultiPrimitives.hpp"
#include "akm/ProgramPrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::CommandResult;
using akm::Done;
using akm::Error;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::ManualScenarioDriver;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;
using akm::test::bytes;

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

    // The bytes of the last frame sent, from the first data byte on, `count` of them.
    Bytes lastData(const SessionHarness& harness, std::size_t count)
    {
        const Bytes frame = harness.sentFrames().back();
        constexpr std::size_t dataStart = akm::test::SENT_ITEM_INDEX + 1;
        return Bytes(frame.begin() + dataStart, frame.begin() + dataStart + static_cast<std::ptrdiff_t>(count));
    }

    void requireError(const CommandResult& result, std::uint16_t number)
    {
        REQUIRE(std::holds_alternative<Error>(result));
        CHECK(std::get<Error>(result).number == number);
    }

    void requireRefusal(const CommandResult& result, RefusalReason reason)
    {
        REQUIRE(std::holds_alternative<Refused>(result));
        CHECK(std::get<Refused>(result).reason == reason);
    }

    // A sampler with a current multi `M` and the programs `LEAD` and `PAD`.
    void holdAMultiAndTwoPrograms(SessionHarness& harness)
    {
        REQUIRE(harness.establishChecksumMode(false).has_value());
        akm::createProgram(harness.session(), "LEAD", harness.recorder().completion());
        akm::createProgram(harness.session(), "PAD", harness.recorder().completion());
        akm::createMulti(harness.session(), "M", harness.recorder().completion());
        REQUIRE(harness.waitForCompletions(3));
    }

    std::optional<std::string> partName(SessionHarness& harness, int part)
    {
        return await<akm::MultiNameResult>(harness, [&](auto done) { akm::getMultiPartName(harness.session(), part, done); }).name;
    }
}

TEST_CASE("Given a current multi, When it is renamed MIX2, Then the frame carries the ASCII name null-terminated and Get Current Multi Name returns MIX2 [RQ-AKM-092]",
          "[akm][multi][multi-edit]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    holdAMultiAndTwoPrograms(harness);

    akm::renameCurrentMulti(harness.session(), "MIX2", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(4));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(lastData(harness, 5) == bytes({0x4D, 0x49, 0x58, 0x32, 0x00}));
    CHECK(await<akm::MultiNameResult>(harness, [&](auto done) { akm::getCurrentMultiName(harness.session(), done); }).name == "MIX2");
}

TEST_CASE("Given the program number set to 5 then cleared, When &41 is read, Then it reads on with 5 (4 on the wire), then off [RQ-AKM-092]",
          "[akm][multi][multi-edit]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    holdAMultiAndTwoPrograms(harness);

    akm::setMultiProgramNumber(harness.session(), 5, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(4));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(lastData(harness, 2) == bytes({0x01, 0x04}));
    CHECK(await<akm::MultiProgramNumberResult>(harness, [&](auto done) { akm::getMultiProgramNumber(harness.session(), done); })
              .frontPanelNumber == 5);

    akm::setMultiProgramNumber(harness.session(), std::nullopt, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(5));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    // Off sends the number byte too, as 0: the real S5000 answers ERROR 2 to the flag alone (TASK-AKM-094).
    CHECK(lastData(harness, 2) == bytes({0x00, 0x00}));
    CHECK_FALSE(await<akm::MultiProgramNumberResult>(harness, [&](auto done) { akm::getMultiProgramNumber(harness.session(), done); })
                    .frontPanelNumber.has_value());
}

TEST_CASE("Given a program number of 0 or 129, When it is set, Then it is refused without sending [RQ-AKM-092]",
          "[akm][multi][multi-edit]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    akm::setMultiProgramNumber(harness.session(), 0, harness.recorder().completion());
    akm::setMultiProgramNumber(harness.session(), 129, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    for (const CommandResult& result : harness.recorder().results())
        requireRefusal(result, RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 0);
}

TEST_CASE("Given a program LEAD in memory, When it is assigned to part 2 by name, Then the part's name is LEAD, and after the part is deleted it is empty [RQ-AKM-092]",
          "[akm][multi][multi-edit]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    holdAMultiAndTwoPrograms(harness);

    akm::setMultiPartByName(harness.session(), 2, "LEAD", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(4));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(lastData(harness, 6) == bytes({0x02, 0x4C, 0x45, 0x41, 0x44, 0x00}));
    CHECK(partName(harness, 2) == "LEAD");

    akm::deleteMultiPart(harness.session(), 2, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(5));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(lastData(harness, 1) == bytes({0x02}));
    CHECK(partName(harness, 2) == "");
}

TEST_CASE("Given the programs LEAD and PAD, When PAD is assigned to part 5 by index 1, Then the frame carries the part and the two index bytes and the part's name is PAD [RQ-AKM-092]",
          "[akm][multi][multi-edit]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    holdAMultiAndTwoPrograms(harness);

    akm::setMultiPartByIndex(harness.session(), 5, 1, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(4));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(lastData(harness, 3) == bytes({0x05, 0x00, 0x01}));
    CHECK(partName(harness, 5) == "PAD");
}

TEST_CASE("Given a program no memory holds, When it is assigned to a part by name or by index, Then the ERROR 04 is reported and the part stays empty [RQ-AKM-092]",
          "[akm][multi][multi-edit]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    holdAMultiAndTwoPrograms(harness);

    akm::setMultiPartByName(harness.session(), 1, "NOPE", harness.recorder().completion());
    akm::setMultiPartByIndex(harness.session(), 1, 2, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(5));
    requireError(harness.recorder().results()[3], akm::error_number::NOT_FOUND);
    requireError(harness.recorder().results()[4], akm::error_number::NOT_FOUND);
    CHECK(partName(harness, 1) == "");
}

TEST_CASE("Given no multi is current, When it is renamed, given a program number or a part is assigned or deleted, Then the ERROR 04 is reported [RQ-AKM-092]",
          "[akm][multi][multi-edit]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    harness.sampler().setMultiNames({"A"});

    akm::renameCurrentMulti(harness.session(), "X", harness.recorder().completion());
    akm::setMultiProgramNumber(harness.session(), 1, harness.recorder().completion());
    akm::setMultiPartByIndex(harness.session(), 0, 0, harness.recorder().completion());
    akm::setMultiPartByName(harness.session(), 0, "X", harness.recorder().completion());
    akm::deleteMultiPart(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(5));
    for (const CommandResult& result : harness.recorder().results())
        requireError(result, akm::error_number::NOT_FOUND);
}

TEST_CASE("Given a part above 127, an index above 14 bits or a name that is not 7-bit ASCII, When a part is assigned or deleted, Then it is refused without sending [RQ-AKM-092, RQ-AKM-002]",
          "[akm][multi][multi-edit]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    akm::setMultiPartByName(harness.session(), 128, "X", harness.recorder().completion());
    akm::setMultiPartByName(harness.session(), 0, "caf\xC3\xA9", harness.recorder().completion());
    akm::setMultiPartByIndex(harness.session(), 0, 16384, harness.recorder().completion());
    akm::setMultiPartByIndex(harness.session(), 128, 0, harness.recorder().completion());
    akm::deleteMultiPart(harness.session(), 128, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(5));
    const auto& results = harness.recorder().results();
    requireRefusal(results[0], RefusalReason::ArgumentOutOfRange);
    requireRefusal(results[1], RefusalReason::NotEncodable);
    requireRefusal(results[2], RefusalReason::ArgumentOutOfRange);
    requireRefusal(results[3], RefusalReason::ArgumentOutOfRange);
    requireRefusal(results[4], RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == 0);
}
