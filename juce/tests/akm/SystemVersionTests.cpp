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

// The operating-system version primitive (§02/&00 and &01) on a session and the simulated sampler: what an
// application reads to decide which items the connected sampler supports.
// [TASK-AKM-008, RQ-AKM-044, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <variant>

#include "SessionHarness.hpp"
#include "akm/SamplerError.hpp"
#include "akm/SystemVersion.hpp"

using akm::Error;
using akm::OsVersionResult;
using akm::Reply;
using akm::harness::ManualScenarioDriver;
using akm::harness::OsVersion;
using akm::harness::SamplerBehaviour;
using akm::harness::SamplerConfig;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;

namespace
{
    constexpr std::uint8_t SECTION_SYSTEM = 0x02;
    constexpr std::uint8_t ITEM_OS_VERSION = 0x00;
    constexpr std::uint8_t ITEM_OS_SUB_VERSION = 0x01;

    SamplerConfig runningOs(int major, int minor)
    {
        SamplerConfig config;
        config.osVersion = OsVersion{major, minor};
        return config;
    }

    void refuseItem(SessionHarness& harness, std::uint8_t section, std::uint8_t item, std::uint16_t number)
    {
        SamplerBehaviour behaviour;
        behaviour.itemErrors = {{section, item, number}};
        harness.sampler().setBehaviour(behaviour);
    }

    OsVersionResult requestVersion(SessionHarness& harness, Latched<OsVersionResult>& latched)
    {
        akm::queryOsVersion(harness.session(), [&latched](const OsVersionResult& result) { latched.set(result); });
        REQUIRE(harness.waitUntil([&latched] { return latched.isSet(); }));
        return *latched.value();
    }
}

TEST_CASE("Given a sampler running OS 2.10, When the version is requested, Then the result is major 2, minor 10, sub-version 0 [RQ-AKM-044]",
          "[akm][osversion]")
{
    ManualScenarioDriver driver;
    Latched<OsVersionResult> latched;
    SessionHarness harness{driver, {}, runningOs(2, 10)};

    const OsVersionResult result = requestVersion(harness, latched);

    REQUIRE(result.version.has_value());
    CHECK(result.version->major == 2);
    CHECK(result.version->minor == 10);
    REQUIRE(result.version->subVersion.has_value());
    CHECK(*result.version->subVersion == 0);
    CHECK(std::holds_alternative<Reply>(result.outcome));
}

TEST_CASE("Given the sampler of the first contact, running OS 2.14, When the version is requested, Then the result is 2.14 [RQ-AKM-017, RQ-AKM-044]",
          "[akm][osversion]")
{
    ManualScenarioDriver driver;
    Latched<OsVersionResult> latched;
    SessionHarness harness{driver, {}, runningOs(2, 14)};

    const OsVersionResult result = requestVersion(harness, latched);

    REQUIRE(result.version.has_value());
    CHECK(result.version->major == 2);
    CHECK(result.version->minor == 14);
}

TEST_CASE("Given the version request, When it runs, Then section 02 item 00 is sent first and item 01 second, one after the other [RQ-AKM-044]",
          "[akm][osversion]")
{
    ManualScenarioDriver driver;
    Latched<OsVersionResult> latched;
    SessionHarness harness{driver, {}, runningOs(2, 10)};

    static_cast<void>(requestVersion(harness, latched));

    const auto frames = harness.sentFrames();
    REQUIRE(frames.size() == 2);
    CHECK(frames[0].at(akm::test::SENT_SECTION_INDEX) == SECTION_SYSTEM);
    CHECK(frames[0].at(akm::test::SENT_ITEM_INDEX) == ITEM_OS_VERSION);
    CHECK(frames[1].at(akm::test::SENT_SECTION_INDEX) == SECTION_SYSTEM);
    CHECK(frames[1].at(akm::test::SENT_ITEM_INDEX) == ITEM_OS_SUB_VERSION);
    CHECK(harness.userRefOf(0) != harness.userRefOf(1));
}

TEST_CASE("Given the checksum mode unknown, on and off, When the version is requested, Then the same version is read in each [RQ-AKM-041, RQ-AKM-044]",
          "[akm][osversion]")
{
    // Unknown: the session has not established anything yet; the codec reads the fixed length of the reply
    // from the catalogue. On and off: the modes of the two checksum states of the sampler.
    for (const int mode : {-1, 0, 1})
    {
        INFO("checksum mode " << mode);
        ManualScenarioDriver driver;
        Latched<OsVersionResult> latched;
        SessionHarness harness{driver, {}, runningOs(2, 10)};
        if (mode >= 0)
            REQUIRE(harness.establishChecksumMode(mode == 1).has_value());

        const OsVersionResult result = requestVersion(harness, latched);

        REQUIRE(result.version.has_value());
        CHECK(result.version->major == 2);
        CHECK(result.version->minor == 10);
        REQUIRE(result.version->subVersion.has_value());
        CHECK(*result.version->subVersion == 0);
    }
}

TEST_CASE("Given a sampler that answers ERROR to the sub-version request, When the version is requested, Then major and minor are still returned and the sub-version is unavailable [RQ-AKM-044]",
          "[akm][osversion]")
{
    ManualScenarioDriver driver;
    Latched<OsVersionResult> latched;
    SessionHarness harness{driver, {}, runningOs(2, 10)};
    refuseItem(harness, SECTION_SYSTEM, ITEM_OS_SUB_VERSION, akm::error_number::NOT_SUPPORTED);

    const OsVersionResult result = requestVersion(harness, latched);

    REQUIRE(result.version.has_value());
    CHECK(result.version->major == 2);
    CHECK(result.version->minor == 10);
    CHECK_FALSE(result.version->subVersion.has_value());
}

TEST_CASE("Given a sampler that answers ERROR to the first request, When the version is requested, Then the failure is reported, nothing is assumed about the version and the second request is not sent [RQ-AKM-044]",
          "[akm][osversion]")
{
    ManualScenarioDriver driver;
    Latched<OsVersionResult> latched;
    SessionHarness harness{driver, {}, runningOs(2, 10)};
    refuseItem(harness, SECTION_SYSTEM, ITEM_OS_VERSION, akm::error_number::NOT_SUPPORTED);

    const OsVersionResult result = requestVersion(harness, latched);

    CHECK_FALSE(result.version.has_value());
    REQUIRE(std::holds_alternative<Error>(result.outcome));
    CHECK(std::get<Error>(result.outcome).number == akm::error_number::NOT_SUPPORTED);
    CHECK(harness.sentCount() == 1);
}

TEST_CASE("Given a REPLY to the version request that does not hold the two numbers, When the version is requested, Then nothing is assumed about the version [RQ-AKM-044]",
          "[akm][osversion]")
{
    ManualScenarioDriver driver;
    Latched<OsVersionResult> latched;
    SessionHarness harness{driver, {}, runningOs(2, 10)};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    SamplerBehaviour behaviour;
    behaviour.silent = true;
    harness.sampler().setBehaviour(behaviour);

    akm::queryOsVersion(harness.session(), [&latched](const OsVersionResult& result) { latched.set(result); });
    harness.settle();
    // One data byte where two are due: the mode being off, whatever follows the item is data.
    harness.inject(harness.confirmationFor(harness.sentCount() - 1, akm::ReplyId::Reply, akm::test::bytes({0x02})));
    REQUIRE(harness.waitUntil([&latched] { return latched.isSet(); }));

    CHECK_FALSE(latched.value()->version.has_value());
    CHECK(std::holds_alternative<Reply>(latched.value()->outcome));
}
