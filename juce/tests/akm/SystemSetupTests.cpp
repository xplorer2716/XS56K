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

// The system setup primitives of section §02 other than the version (that is SystemVersionTests.cpp's): on a
// session and the simulated sampler. This file grows with each task of PLAN-AKM-006 — for now the sampler's
// name (§02/&02 and &03), its model and its available memory (&04, &30, &31, &33, &34), its clock and date
// (&05, &06). Real-sampler verification is TASK-AKM-053's.
// [TASK-AKM-048, TASK-AKM-049, TASK-AKM-050, RQ-AKM-052, RQ-AKM-053, RQ-AKM-054,
// ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)]
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

#include "SessionHarness.hpp"
#include "akm/SamplerError.hpp"
#include "akm/SystemSetup.hpp"

using akm::ClockDate;
using akm::ClockDateResult;
using akm::ClockField;
using akm::CommandResult;
using akm::Done;
using akm::MemoryBytesResult;
using akm::MemoryPercentResult;
using akm::RefusalReason;
using akm::Refused;
using akm::SamplerModel;
using akm::SamplerModelResult;
using akm::SamplerNameResult;
using akm::harness::ManualScenarioDriver;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;
using akm::test::bytes;

namespace
{
    // The name a sampler carries until its user changes it (spec Table 6, footnote b).
    const std::string FACTORY_NAME = "AKAI S5000";
    // More characters than any name field of the catalogue holds (the spec states no maximum; 20 is what
    // the S5000 was observed to keep of a program name, sysex_spec.kb.md "Common value codes").
    const std::string TOO_LONG_NAME(21, 'A');

    // The model bytes of spec Table 7 (&04), and one no model has.
    constexpr std::uint8_t MODEL_S5000 = 0;
    constexpr std::uint8_t MODEL_S6000 = 1;
    constexpr std::uint8_t MODEL_UNKNOWN = 2;
    // A sampler with 64 MiB of Wave memory, 16 MiB of it free, and 40 % of its MPKS memory free: the Wave
    // percentage is then 25, and the totals need all four data bytes of a compound double word (more
    // than the 16383 a compound word holds).
    constexpr std::uint32_t TOTAL_WAVE_BYTES = 64u * 1024 * 1024;
    constexpr std::uint32_t FREE_WAVE_BYTES = 16u * 1024 * 1024;
    constexpr int FREE_WAVE_PERCENT = 25;
    constexpr std::uint8_t FREE_MPKS_PERCENT = 40;
    // More than the 0-100 % the spec gives.
    constexpr std::uint8_t MPKS_PERCENT_BEYOND_RANGE = 101;

    // Thursday 1 October 2026, 14:30:15 (day of week 1 = Sunday, spec Table 6). The year 2026 is MSB 15, LSB 106
    // of a compound word (128 x 15 + 106).
    constexpr ClockDate THURSDAY{2026, 10, 1, 5, 14, 30, 15};
    // The ends of the year range the spec gives, 1980-2079; 2079 is MSB 16, the largest the spec's own "0-16
    // (MSB year)" column allows.
    constexpr ClockDate FIRST_DAY{1980, 1, 1, 3, 0, 0, 0};
    constexpr ClockDate LAST_DAY{2079, 12, 31, 1, 23, 59, 59};

    // A copy of `THURSDAY` with one field changed to a value outside its range, and the field that is.
    struct BadClock
    {
        ClockDate clock;
        ClockField field;
        std::string_view fieldName;
    };
    std::array<BadClock, 13> badClocks()
    {
        const auto with = [](auto change) {
            ClockDate clock = THURSDAY;
            change(clock);
            return clock;
        };
        return {{
            {with([](ClockDate& c) { c.year = 1979; }), ClockField::Year, "year"},
            {with([](ClockDate& c) { c.year = 2080; }), ClockField::Year, "year"},
            {with([](ClockDate& c) { c.month = 0; }), ClockField::Month, "month"},
            {with([](ClockDate& c) { c.month = 13; }), ClockField::Month, "month"},
            {with([](ClockDate& c) { c.day = 0; }), ClockField::DayOfMonth, "dayOfMonth"},
            {with([](ClockDate& c) { c.day = 32; }), ClockField::DayOfMonth, "dayOfMonth"},
            {with([](ClockDate& c) { c.dayOfWeek = 0; }), ClockField::DayOfWeek, "dayOfWeek"},
            {with([](ClockDate& c) { c.dayOfWeek = 8; }), ClockField::DayOfWeek, "dayOfWeek"},
            {with([](ClockDate& c) { c.hours = -1; }), ClockField::Hours, "hours"},
            {with([](ClockDate& c) { c.hours = 24; }), ClockField::Hours, "hours"},
            {with([](ClockDate& c) { c.minutes = 60; }), ClockField::Minutes, "minutes"},
            {with([](ClockDate& c) { c.seconds = 60; }), ClockField::Seconds, "seconds"},
            {with([](ClockDate& c) { c.seconds = -1; }), ClockField::Seconds, "seconds"},
        }};
    }

    // Runs `ask`, which starts a primitive with its completion, and returns the result it reports.
    template <typename Result, typename Ask>
    Result await(SessionHarness& harness, Ask ask)
    {
        auto latched = std::make_shared<Latched<Result>>();
        ask([latched](const Result& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    SamplerNameResult getName(SessionHarness& harness)
    {
        return await<SamplerNameResult>(harness, [&](auto done) { akm::getSamplerName(harness.session(), done); });
    }

    SamplerModelResult getModel(SessionHarness& harness)
    {
        return await<SamplerModelResult>(harness, [&](auto done) { akm::getSamplerModel(harness.session(), done); });
    }

    ClockDateResult getClock(SessionHarness& harness)
    {
        return await<ClockDateResult>(harness, [&](auto done) { akm::getClockDate(harness.session(), done); });
    }

    MemoryPercentResult getWavePercent(SessionHarness& harness)
    {
        return await<MemoryPercentResult>(harness,
                                          [&](auto done) { akm::getFreeWaveMemoryPercent(harness.session(), done); });
    }

    MemoryPercentResult getMpksPercent(SessionHarness& harness)
    {
        return await<MemoryPercentResult>(harness,
                                          [&](auto done) { akm::getFreeMpksMemoryPercent(harness.session(), done); });
    }

    MemoryBytesResult getTotalWaveBytes(SessionHarness& harness)
    {
        return await<MemoryBytesResult>(harness,
                                        [&](auto done) { akm::getTotalWaveMemoryBytes(harness.session(), done); });
    }

    MemoryBytesResult getFreeWaveBytes(SessionHarness& harness)
    {
        return await<MemoryBytesResult>(harness,
                                        [&](auto done) { akm::getFreeWaveMemoryBytes(harness.session(), done); });
    }
}

TEST_CASE("Given a simulated sampler named AKAI S5000, When its name is set to STUDIO then read, Then the frame carries 53 54 55 44 49 4F 00 and the Get returns STUDIO [RQ-AKM-052]",
          "[akm][system]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    CHECK(getName(harness).name == FACTORY_NAME);

    akm::setSamplerName(harness.session(), "STUDIO", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    const Bytes setFrame = harness.sentFrames().back();
    constexpr std::size_t dataStart = akm::test::SENT_ITEM_INDEX + 1;
    CHECK(Bytes(setFrame.begin() + dataStart, setFrame.begin() + dataStart + 7)
          == bytes({0x53, 0x54, 0x55, 0x44, 0x49, 0x4F, 0x00}));

    const SamplerNameResult result = getName(harness);
    CHECK(result.name == "STUDIO");
    CHECK(std::holds_alternative<akm::Reply>(result.outcome));
}

TEST_CASE("Given a name of 21 characters or one that is not 7-bit ASCII, When it is set, Then it is refused without sending [RQ-AKM-052]",
          "[akm][system]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const std::size_t sentBefore = harness.sentCount();

    akm::setSamplerName(harness.session(), TOO_LONG_NAME, harness.recorder().completion());
    akm::setSamplerName(harness.session(), "caf\xC3\xA9", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));

    const auto& results = harness.recorder().results();
    REQUIRE(std::holds_alternative<Refused>(results[0]));
    CHECK(std::get<Refused>(results[0]).reason == RefusalReason::ArgumentOutOfRange);
    REQUIRE(std::holds_alternative<Refused>(results[1]));
    CHECK(std::get<Refused>(results[1]).reason == RefusalReason::NotEncodable);
    CHECK(harness.sentCount() == sentBefore);
}

TEST_CASE("Given the checksum mode unknown, When the sampler's name is requested, Then it is refused as ChecksumModeUnknown without sending, since its REPLY has no fixed length [RQ-AKM-052, RQ-AKM-041, ADR-AKM-001 (DEC-AKM-013)]",
          "[akm][system]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    const SamplerNameResult result = getName(harness);
    REQUIRE(std::holds_alternative<Refused>(result.outcome));
    CHECK(std::get<Refused>(result.outcome).reason == RefusalReason::ChecksumModeUnknown);
    CHECK_FALSE(result.name.has_value());
    CHECK(harness.sentCount() == 0);
}

TEST_CASE("Given a simulated sampler of model S6000 with 64 MiB of Wave memory of which 16 MiB are free and 40 % of its MPKS memory free, When the model and the four memory values are read, Then they decode to S6000, 25 %, 40 %, 64 MiB and 16 MiB [RQ-AKM-053]",
          "[akm][system]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setModel(MODEL_S6000);
    harness.sampler().setMemory(TOTAL_WAVE_BYTES, FREE_WAVE_BYTES, FREE_MPKS_PERCENT);

    const SamplerModelResult model = getModel(harness);
    CHECK(model.model == SamplerModel::S6000);
    CHECK(std::holds_alternative<akm::Reply>(model.outcome));
    CHECK(getWavePercent(harness).percent == FREE_WAVE_PERCENT);
    CHECK(getMpksPercent(harness).percent == FREE_MPKS_PERCENT);
    CHECK(getTotalWaveBytes(harness).bytes == TOTAL_WAVE_BYTES);
    CHECK(getFreeWaveBytes(harness).bytes == FREE_WAVE_BYTES);
}

TEST_CASE("Given a sampler left as it comes from the factory, When its model is read, Then it is an S5000 [RQ-AKM-053]",
          "[akm][system]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    CHECK(getModel(harness).model == SamplerModel::S5000);
}

TEST_CASE("Given a REPLY whose model byte is neither 0 nor 1 or whose percentage is above 100, When it is decoded, Then no value is reported and the outcome is still the REPLY, not a guess [RQ-AKM-053]",
          "[akm][system]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setModel(MODEL_UNKNOWN);
    harness.sampler().setMemory(TOTAL_WAVE_BYTES, FREE_WAVE_BYTES, MPKS_PERCENT_BEYOND_RANGE);

    const SamplerModelResult model = getModel(harness);
    CHECK_FALSE(model.model.has_value());
    CHECK(std::holds_alternative<akm::Reply>(model.outcome));
    const MemoryPercentResult mpks = getMpksPercent(harness);
    CHECK_FALSE(mpks.percent.has_value());
    CHECK(std::holds_alternative<akm::Reply>(mpks.outcome));
}

TEST_CASE("Given a sampler holding no Wave memory at all, When the total and the free percentage are read, Then both read 0 [RQ-AKM-053]",
          "[akm][system]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setMemory(0, 0, FREE_MPKS_PERCENT);

    CHECK(getTotalWaveBytes(harness).bytes == 0);
    CHECK(getWavePercent(harness).percent == 0);
}

TEST_CASE("Given the checksum mode unknown, When the model and the memory values are requested, Then they are answered, since each REPLY has a fixed length the catalogue gives [RQ-AKM-053, RQ-AKM-041]",
          "[akm][system]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    harness.sampler().setMemory(TOTAL_WAVE_BYTES, FREE_WAVE_BYTES, FREE_MPKS_PERCENT);

    CHECK(getModel(harness).model == SamplerModel::S5000);
    CHECK(getTotalWaveBytes(harness).bytes == TOTAL_WAVE_BYTES);
}

TEST_CASE("Given a simulated sampler, When the clock is set to Thursday 2026-10-01 14:30:15 then read, Then the frame carries 0F 6A 0A 01 05 0E 1E 0F and the value read equals the value set [RQ-AKM-054]",
          "[akm][system]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    akm::setClockDate(harness.session(), THURSDAY, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    const Bytes setFrame = harness.sentFrames().back();
    constexpr std::size_t dataStart = akm::test::SENT_ITEM_INDEX + 1;
    constexpr std::size_t dataSize = 8;
    CHECK(Bytes(setFrame.begin() + dataStart, setFrame.begin() + dataStart + dataSize)
          == bytes({0x0F, 0x6A, 0x0A, 0x01, 0x05, 0x0E, 0x1E, 0x0F}));

    const ClockDateResult result = getClock(harness);
    CHECK(result.clock == THURSDAY);
    CHECK(std::holds_alternative<akm::Reply>(result.outcome));
}

TEST_CASE("Given the first and the last day the spec allows, When each is set then read, Then the value read equals the value set [RQ-AKM-054]",
          "[akm][system]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    akm::setClockDate(harness.session(), FIRST_DAY, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(getClock(harness).clock == FIRST_DAY);

    akm::setClockDate(harness.session(), LAST_DAY, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(getClock(harness).clock == LAST_DAY);
}

TEST_CASE("Given a field outside its range, When the clock is set, Then it is refused without sending and the field is named [RQ-AKM-054]",
          "[akm][system]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    const std::size_t sentBefore = harness.sentCount();
    const auto cases = badClocks();

    std::size_t completed = 0;
    for (const BadClock& bad : cases)
    {
        const auto invalid = akm::invalidClockField(bad.clock);
        REQUIRE(invalid.has_value());
        CHECK(*invalid == bad.field);
        CHECK(akm::clockFieldName(*invalid) == bad.fieldName);

        akm::setClockDate(harness.session(), bad.clock, harness.recorder().completion());
        REQUIRE(harness.waitForCompletions(++completed));
        const CommandResult result = harness.recorder().results().back();
        INFO("field " << bad.fieldName);
        REQUIRE(std::holds_alternative<Refused>(result));
        CHECK(std::get<Refused>(result).reason == RefusalReason::ArgumentOutOfRange);
    }
    CHECK(harness.sentCount() == sentBefore);

    CHECK_FALSE(akm::invalidClockField(THURSDAY).has_value());
    CHECK_FALSE(akm::invalidClockField(FIRST_DAY).has_value());
    CHECK_FALSE(akm::invalidClockField(LAST_DAY).has_value());
}

TEST_CASE("Given the checksum mode unknown, When the clock is requested, Then it is answered, since its REPLY has a fixed length the catalogue gives [RQ-AKM-054, RQ-AKM-041]",
          "[akm][system]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    const ClockDateResult result = getClock(harness);
    CHECK(result.clock.has_value());
    CHECK(std::holds_alternative<akm::Reply>(result.outcome));
}
