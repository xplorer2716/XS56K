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

// The destructive guard on "Clear Sampler Memory" (§02/&32), which deletes every program, multi and sample:
// sent only with its explicit confirmation. [TASK-AKM-052, RQ-AKM-056]
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <variant>

#include "SessionHarness.hpp"
#include "akm/ProgramPrimitives.hpp"
#include "akm/SamplePrimitives.hpp"
#include "akm/SamplerError.hpp"
#include "akm/SystemSetup.hpp"

using akm::CommandResult;
using akm::ConfirmClearSamplerMemory;
using akm::Done;
using akm::Error;
using akm::MemoryBytesResult;
using akm::MemoryPercentResult;
using akm::ProgramCountResult;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::ManualScenarioDriver;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;

namespace
{
    // What §02/&32 is on the wire: section 02, item 32 (hex), no data (spec Table 6).
    constexpr std::uint8_t SECTION_SYSTEM = 0x02;
    constexpr std::uint8_t ITEM_CLEAR_MEMORY = 0x32;

    // A sampler with 64 MiB of Wave memory of which 16 MiB are free and 40 % of its MPKS memory free: what a
    // sampler that holds something looks like, to be compared with the same sampler once cleared.
    constexpr std::uint32_t TOTAL_WAVE_BYTES = 64u * 1024 * 1024;
    constexpr std::uint32_t FREE_WAVE_BYTES = 16u * 1024 * 1024;
    constexpr std::uint8_t FREE_MPKS_PERCENT = 40;
    constexpr int ALL_FREE_PERCENT = 100;

    template <typename Result, typename Ask>
    Result await(SessionHarness& harness, Ask ask)
    {
        auto latched = std::make_shared<Latched<Result>>();
        ask([latched](const Result& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }
}

TEST_CASE("Given a request to clear the sampler's memory without its confirmation, When made, Then nothing is sent and NotConfirmed explains why [RQ-AKM-056]",
          "[akm][system][delete-all-guard]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    harness.sampler().setSampleNames({"A"});

    akm::clearSamplerMemory(harness.session(), std::nullopt, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));

    const CommandResult result = harness.recorder().results().back();
    REQUIRE(std::holds_alternative<Refused>(result));
    CHECK(std::get<Refused>(result).reason == RefusalReason::NotConfirmed);
    CHECK(harness.sentCount() == 0);
    // Nothing was touched.
    CHECK(harness.sampler().acceptedCommands().empty());
}

TEST_CASE("Given the explicit confirmation, When Clear Sampler Memory is requested, Then item 32 of section 02 is sent with no data and every program, multi and sample is gone [RQ-AKM-056]",
          "[akm][system][delete-all-guard]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    // With the checksum mode known to be off, the frame carries no byte after the item.
    REQUIRE(harness.establishChecksumMode(false).has_value());
    akm::createProgram(harness.session(), "A", harness.recorder().completion());
    akm::createProgram(harness.session(), "B", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    harness.sampler().setSampleNames({"KICK", "SNARE"});
    harness.sampler().setMultiNames({"LIVE"});
    harness.sampler().setMemory(TOTAL_WAVE_BYTES, FREE_WAVE_BYTES, FREE_MPKS_PERCENT);
    REQUIRE(harness.sampler().multiCount() == 1);

    akm::clearSamplerMemory(harness.session(), ConfirmClearSamplerMemory::IUnderstandThisDeletesEveryProgramMultiAndSampleInMemory,
                            harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    const auto accepted = harness.sampler().acceptedCommands();
    REQUIRE_FALSE(accepted.empty());
    CHECK(accepted.back().section == SECTION_SYSTEM);
    CHECK(accepted.back().item == ITEM_CLEAR_MEMORY);
    CHECK(accepted.back().data.empty());

    CHECK(await<ProgramCountResult>(harness, [&](auto done) { akm::getProgramCount(harness.session(), done); }).count == 0);
    akm::selectSampleByIndex(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(4));
    const CommandResult selectResult = harness.recorder().results().back();
    REQUIRE(std::holds_alternative<Error>(selectResult));
    CHECK(std::get<Error>(selectResult).number == akm::error_number::NOT_FOUND);
    CHECK(harness.sampler().multiCount() == 0);
}

TEST_CASE("Given a sampler that holds something, When its memory is cleared, Then all of its Wave memory and of its MPKS memory read free [RQ-AKM-056]",
          "[akm][system][delete-all-guard]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    harness.sampler().setMemory(TOTAL_WAVE_BYTES, FREE_WAVE_BYTES, FREE_MPKS_PERCENT);

    akm::clearSamplerMemory(harness.session(), ConfirmClearSamplerMemory::IUnderstandThisDeletesEveryProgramMultiAndSampleInMemory,
                            harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));

    CHECK(await<MemoryBytesResult>(harness, [&](auto done) { akm::getFreeWaveMemoryBytes(harness.session(), done); }).bytes
          == TOTAL_WAVE_BYTES);
    CHECK(await<MemoryPercentResult>(harness, [&](auto done) { akm::getFreeWaveMemoryPercent(harness.session(), done); })
              .percent == ALL_FREE_PERCENT);
    CHECK(await<MemoryPercentResult>(harness, [&](auto done) { akm::getFreeMpksMemoryPercent(harness.session(), done); })
              .percent == ALL_FREE_PERCENT);
}
