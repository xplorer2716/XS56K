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
// name (§02/&02 and &03). Real-sampler verification is TASK-AKM-053's.
// [TASK-AKM-048, RQ-AKM-052, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <variant>

#include "SessionHarness.hpp"
#include "akm/SamplerError.hpp"
#include "akm/SystemSetup.hpp"

using akm::CommandResult;
using akm::Done;
using akm::RefusalReason;
using akm::Refused;
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

    SamplerNameResult getName(SessionHarness& harness)
    {
        auto latched = std::make_shared<Latched<SamplerNameResult>>();
        akm::getSamplerName(harness.session(), [latched](const SamplerNameResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
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
