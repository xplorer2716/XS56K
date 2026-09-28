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

// "Several zones" REPLY shapes of section 06 (RQ-AKM-036): a Get with zone 0 answers one value set per
// zone of the current keygroup (getForAllZones); a Get with zone 0 while keygroup 0 is also current
// answers one value set per zone of every keygroup (getForAllZonesAllKeygroups), reusing
// `decodeRepeatedReply` (ADR-AKM-001, DEC-AKM-015) the same way §08's own getForAllKeygroups does.
// [TASK-AKM-037]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/KeygroupPrimitives.hpp"
#include "akm/ProgramPrimitives.hpp"
#include "akm/SamplerError.hpp"
#include "akm/ZonePrimitives.hpp"

using akm::AllZonesAllKeygroupsResult;
using akm::AllZonesResult;
using akm::CommandResult;
using akm::harness::ManualScenarioDriver;
using akm::test::Latched;
using akm::test::SessionHarness;

namespace
{
    void createAndSelectAProgram(SessionHarness& harness)
    {
        akm::createProgram(harness.session(), "A", harness.recorder().completion());
        REQUIRE(harness.waitForCompletions(1));
    }

    void setZoneLevel(SessionHarness& harness, std::int64_t zone, std::int64_t sign, std::int64_t magnitude)
    {
        const CommandResult result = *harness.submitAndWait(akm::makeRequest(akm::ItemId::ZoneSetLevel, {zone, sign, magnitude}));
        REQUIRE(std::holds_alternative<akm::Done>(result));
    }
}

TEST_CASE("Given a keygroup and zone 0, When Get Zone Level runs, Then four values are returned in zone order [RQ-AKM-036]",
          "[akm][zone]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    createAndSelectAProgram(harness);

    setZoneLevel(harness, 1, 0, 10);
    setZoneLevel(harness, 2, 0, 20);
    setZoneLevel(harness, 3, 0, 30);
    setZoneLevel(harness, 4, 0, 40);

    auto latched = std::make_shared<Latched<AllZonesResult>>();
    akm::getForAllZones(harness.session(), akm::ItemId::ZoneGetLevel, 4,
                        [latched](const AllZonesResult& r) { latched->set(r); });
    REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
    const AllZonesResult result = *latched->value();
    REQUIRE(result.values.has_value());
    const std::vector<std::vector<std::int64_t>> expected{{0, 10}, {0, 20}, {0, 30}, {0, 40}};
    CHECK(*result.values == expected);
}

TEST_CASE("Given the number of records decoded differs from the expected zone count, When getForAllZones runs, Then values is empty [RQ-AKM-036]",
          "[akm][zone]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    createAndSelectAProgram(harness);

    auto latched = std::make_shared<Latched<AllZonesResult>>();
    // The sampler always answers zone 0 with exactly 4 records; 5 is deliberately wrong.
    akm::getForAllZones(harness.session(), akm::ItemId::ZoneGetLevel, 5,
                        [latched](const AllZonesResult& r) { latched->set(r); });
    REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
    const AllZonesResult result = *latched->value();
    CHECK_FALSE(result.values.has_value());
}

TEST_CASE("Given a 2-keygroup program, keygroup 0 current and zone 0, When Get Zone Level runs, Then eight values are returned, indexed by keygroup and zone [RQ-AKM-036]",
          "[akm][zone]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    createAndSelectAProgram(harness);
    akm::addKeygroupsToProgram(harness.session(), 1, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));

    akm::selectKeygroup(harness.session(), 1, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));
    setZoneLevel(harness, 1, 0, 11);
    setZoneLevel(harness, 2, 0, 12);
    setZoneLevel(harness, 3, 0, 13);
    setZoneLevel(harness, 4, 0, 14);

    akm::selectKeygroup(harness.session(), 2, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(4));
    setZoneLevel(harness, 1, 0, 21);
    setZoneLevel(harness, 2, 0, 22);
    setZoneLevel(harness, 3, 0, 23);
    setZoneLevel(harness, 4, 0, 24);

    akm::selectKeygroup(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(5));

    auto latched = std::make_shared<Latched<AllZonesAllKeygroupsResult>>();
    akm::getForAllZonesAllKeygroups(harness.session(), akm::ItemId::ZoneGetLevel, 2, 4,
                                    [latched](const AllZonesAllKeygroupsResult& r) { latched->set(r); });
    REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
    const AllZonesAllKeygroupsResult result = *latched->value();
    REQUIRE(result.values.has_value());
    REQUIRE(result.values->size() == 2);
    const std::vector<std::vector<std::int64_t>> keygroup1{{0, 11}, {0, 12}, {0, 13}, {0, 14}};
    const std::vector<std::vector<std::int64_t>> keygroup2{{0, 21}, {0, 22}, {0, 23}, {0, 24}};
    CHECK((*result.values)[0] == keygroup1);
    CHECK((*result.values)[1] == keygroup2);
}

TEST_CASE("Given the total number of records decoded differs from keygroup count times zone count, When getForAllZonesAllKeygroups runs, Then values is empty [RQ-AKM-036]",
          "[akm][zone]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    createAndSelectAProgram(harness);
    akm::addKeygroupsToProgram(harness.session(), 1, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    akm::selectKeygroup(harness.session(), 0, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(3));

    auto latched = std::make_shared<Latched<AllZonesAllKeygroupsResult>>();
    // The program has 2 keygroups (8 records implied); 3 keygroups is deliberately wrong.
    akm::getForAllZonesAllKeygroups(harness.session(), akm::ItemId::ZoneGetLevel, 3, 4,
                                    [latched](const AllZonesAllKeygroupsResult& r) { latched->set(r); });
    REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
    const AllZonesAllKeygroupsResult result = *latched->value();
    CHECK_FALSE(result.values.has_value());
}

TEST_CASE("Given zone 0, When Zone Level is set, Then all four zones of the current keygroup hold the value set [RQ-AKM-036]",
          "[akm][zone]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    createAndSelectAProgram(harness);

    setZoneLevel(harness, 0, 1, 50);

    auto latched = std::make_shared<Latched<AllZonesResult>>();
    akm::getForAllZones(harness.session(), akm::ItemId::ZoneGetLevel, 4,
                        [latched](const AllZonesResult& r) { latched->set(r); });
    REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
    const AllZonesResult result = *latched->value();
    REQUIRE(result.values.has_value());
    for (const std::vector<std::int64_t>& record : *result.values)
        CHECK(record == std::vector<std::int64_t>{1, 50});
}
