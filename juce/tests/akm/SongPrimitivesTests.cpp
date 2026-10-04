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

// The song file primitives of section 16 on a session and the simulated sampler: select by name and by
// index, delete, rename, and the four Gets (count, name by index, current index, current name). Like §0E,
// §16 has a sampler-wide current selection and no "create": a song file only exists once `setSongNames`
// seeds it. [TASK-AKM-084, RQ-AKM-082, RQ-AKM-083, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <variant>

#include "SessionHarness.hpp"
#include "akm/SamplerError.hpp"
#include "akm/SongPrimitives.hpp"

using akm::CommandResult;
using akm::Done;
using akm::Error;
using akm::RefusalReason;
using akm::Refused;
using akm::SongCountResult;
using akm::SongIndexResult;
using akm::SongNameResult;
using akm::harness::ManualScenarioDriver;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;
using akm::test::bytes;

namespace
{
    SongNameResult getCurrentName(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<SongNameResult>>();
        akm::getCurrentSongName(harness.session(), [latched](const SongNameResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    SongIndexResult getCurrentIndex(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<SongIndexResult>>();
        akm::getCurrentSongIndex(harness.session(), [latched](const SongIndexResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    SongCountResult getCount(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<SongCountResult>>();
        akm::getSongCount(harness.session(), [latched](const SongCountResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    SongNameResult getNameByIndex(SessionHarness& harness, int index)
    {
        auto latched = std::make_shared<Latched<SongNameResult>>();
        akm::getSongNameByIndex(harness.session(), index, [latched](const SongNameResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    void requireNotFound(const CommandResult& result)
    {
        REQUIRE(std::holds_alternative<Error>(result));
        CHECK(std::get<Error>(result).number == akm::error_number::NOT_FOUND);
    }
}

TEST_CASE("Given a simulated sampler holding the song file SONG1, When it is selected by name then renamed SONG2, Then the frame carries the ASCII name null-terminated, both complete on DONE and Get Current Songfile's Name returns SONG2 [RQ-AKM-082]",
          "[akm][song]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setSongNames({"SONG1"});

    akm::selectSongByName(harness.session(), "SONG1", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    const Bytes selectFrame = harness.sentFrames().back();
    constexpr std::size_t dataStart = akm::test::SENT_ITEM_INDEX + 1;
    CHECK(Bytes(selectFrame.begin() + dataStart, selectFrame.begin() + dataStart + 6)
          == bytes({0x53, 0x4F, 0x4E, 0x47, 0x31, 0x00}));

    akm::renameCurrentSong(harness.session(), "SONG2", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(getCurrentName(harness).name == "SONG2");
}

TEST_CASE("Given a name or an index that no song file has, When it is selected, Then the ERROR 04 is reported [RQ-AKM-082]",
          "[akm][song]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    harness.sampler().setSongNames({"ONLY"});

    akm::selectSongByName(harness.session(), "NOPE", harness.recorder().completion());
    akm::selectSongByIndex(harness.session(), 1, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    for (const CommandResult& result : harness.recorder().results())
        requireNotFound(result);
}

TEST_CASE("Given song files seeded in order, When selected by index and the current one deleted, Then the selection, the count and the index shifting follow [RQ-AKM-082, RQ-AKM-083]",
          "[akm][song]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setSongNames({"A", "B"});

    akm::selectSongByIndex(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(1));
    const Bytes selectFrame = harness.sentFrames().back();
    constexpr std::size_t dataStart = akm::test::SENT_ITEM_INDEX + 1;
    CHECK(Bytes(selectFrame.begin() + dataStart, selectFrame.begin() + dataStart + 2) == bytes({0x00, 0x00}));
    CHECK(getCurrentIndex(harness).index == 0);
    CHECK(getCurrentName(harness).name == "A");

    akm::deleteCurrentSong(harness.session(), harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    CHECK(getCount(harness).count == 1);
    CHECK_FALSE(getCurrentIndex(harness).index.has_value());
    CHECK(getNameByIndex(harness, 0).name == "B");
}

TEST_CASE("Given no song file is current, When the current one is deleted or renamed, or its index or name is read, Then the ERROR 04 is reported [RQ-AKM-082, RQ-AKM-083]",
          "[akm][song]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setSongNames({"A"});

    akm::deleteCurrentSong(harness.session(), harness.recorder().completion());
    akm::renameCurrentSong(harness.session(), "X", harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    for (const CommandResult& result : harness.recorder().results())
        requireNotFound(result);

    requireNotFound(getCurrentIndex(harness).outcome);
    requireNotFound(getCurrentName(harness).outcome);
    CHECK(getCount(harness).count == 1);
}

TEST_CASE("Given a simulated sampler holding A, B, C, When the count and the name at each index are read, Then the count is 3 and the names come in order, and reading by index leaves the selection alone [RQ-AKM-083]",
          "[akm][song]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    harness.sampler().setSongNames({"A", "B", "C"});

    CHECK(getCount(harness).count == 3);
    CHECK(getNameByIndex(harness, 0).name == "A");
    CHECK(getNameByIndex(harness, 1).name == "B");
    CHECK(getNameByIndex(harness, 2).name == "C");
    requireNotFound(getNameByIndex(harness, 3).outcome);
    CHECK_FALSE(getCurrentIndex(harness).index.has_value());
}

TEST_CASE("Given no song file in memory, When the count is read, Then it is zero, not a failure [RQ-AKM-083]",
          "[akm][song]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());

    const SongCountResult result = getCount(harness);
    REQUIRE(std::holds_alternative<akm::Reply>(result.outcome));
    CHECK(result.count == 0);
}

TEST_CASE("Given the checksum mode unknown, When a song file's name is requested by index or as the current one, Then both are refused as ChecksumModeUnknown without sending [RQ-AKM-083, RQ-AKM-041, ADR-AKM-001 (DEC-AKM-013)]",
          "[akm][song]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};

    for (const SongNameResult& result : {getCurrentName(harness), getNameByIndex(harness, 0)})
    {
        REQUIRE(std::holds_alternative<Refused>(result.outcome));
        CHECK(std::get<Refused>(result.outcome).reason == RefusalReason::ChecksumModeUnknown);
        CHECK_FALSE(result.name.has_value());
    }
    CHECK(harness.sentCount() == 0);
}
