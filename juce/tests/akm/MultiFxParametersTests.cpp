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

// The parameter values of section 12 on a session and the simulated sampler: a value is passed as a signed
// compound word, a sign byte then a magnitude in two 7-bit bytes (magnitude = LSB + 128 x MSB), whatever the
// parameter. [TASK-AKM-103, RQ-AKM-101, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <variant>

#include "SessionHarness.hpp"
#include "akm/MultiFxPrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::CommandResult;
using akm::Done;
using akm::Error;
using akm::FxParameterResult;
using akm::RefusalReason;
using akm::Refused;
using akm::harness::ManualScenarioDriver;
using akm::harness::eb20Layout;
using akm::test::Bytes;
using akm::test::Latched;
using akm::test::SessionHarness;
using akm::test::bytes;

namespace
{
    constexpr std::size_t DATA_START = akm::test::SENT_ITEM_INDEX + 1;
    constexpr int CHANNEL = 0;
    constexpr int MODULE = 2;
    constexpr int PARAMETER = 1;

    // A sampler with an EB20 and one multi, which is current: the state in which a multi's effects can be reached.
    void seedEb20OnACurrentMulti(SessionHarness& harness)
    {
        harness.sampler().setMultiNames({"FX MULTI"});
        harness.sampler().setCurrentMulti(0);
        harness.sampler().setFxBoard(eb20Layout());
    }

    Bytes dataOfLastFrame(SessionHarness& harness, std::size_t length)
    {
        const Bytes frame = harness.sentFrames().back();
        return Bytes(frame.begin() + DATA_START, frame.begin() + DATA_START + static_cast<std::ptrdiff_t>(length));
    }

    FxParameterResult getParameter(SessionHarness& harness, int channel, int module, int parameter)
    {
        auto latched = std::make_shared<Latched<FxParameterResult>>();
        akm::getFxParameter(harness.session(), channel, module, parameter, [latched](const FxParameterResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        return *latched->value();
    }

    void setParameter(SessionHarness& harness, int value)
    {
        const std::size_t before = harness.recorder().results().size();
        akm::setFxParameter(harness.session(), CHANNEL, MODULE, PARAMETER, value, harness.recorder().completion());
        REQUIRE(harness.waitForCompletions(before + 1));
        CHECK(std::holds_alternative<Done>(harness.recorder().results().back()));
    }

    void requireNotFound(const CommandResult& result)
    {
        REQUIRE(std::holds_alternative<Error>(result));
        CHECK(std::get<Error>(result).number == akm::error_number::NOT_FOUND);
    }

    void requireRefused(const CommandResult& result, RefusalReason reason)
    {
        REQUIRE(std::holds_alternative<Refused>(result));
        CHECK(std::get<Refused>(result).reason == reason);
    }
}

TEST_CASE("Given a simulated EB20, When parameter 1 of module 2 of channel 0 is set to -25, Then the frame carries 00 02 01 01 00 19 after the item and the value reads -25 [RQ-AKM-101]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    seedEb20OnACurrentMulti(harness);

    setParameter(harness, -25);
    CHECK(harness.sentFrames().back()[akm::test::SENT_SECTION_INDEX] == 0x12);
    CHECK(harness.sentFrames().back()[akm::test::SENT_ITEM_INDEX] == 0x50);
    CHECK(dataOfLastFrame(harness, 6) == bytes({0x00, 0x02, 0x01, 0x01, 0x00, 0x19}));

    const FxParameterResult result = getParameter(harness, CHANNEL, MODULE, PARAMETER);
    CHECK(result.value == -25);
    CHECK(harness.sentFrames().back()[akm::test::SENT_ITEM_INDEX] == 0x51);
    CHECK(dataOfLastFrame(harness, 3) == bytes({0x00, 0x02, 0x01}));
}

TEST_CASE("Given a parameter set to 4000, When it is read back, Then the frame carried 00 1F 20 for the value and it reads 4000 [RQ-AKM-101]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    seedEb20OnACurrentMulti(harness);

    setParameter(harness, 4000);
    // 4000 = 31 x 128 + 32: sign 00, MSB 1F, LSB 20, after the channel, the module and the parameter.
    CHECK(dataOfLastFrame(harness, 6) == bytes({0x00, 0x02, 0x01, 0x00, 0x1F, 0x20}));
    CHECK(getParameter(harness, CHANNEL, MODULE, PARAMETER).value == 4000);
}

TEST_CASE("Given the largest magnitudes the wire carries, When set and read, Then 16383 and -16383 round-trip with 7F 7F as the magnitude, and 0 is a positive zero [RQ-AKM-101]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    seedEb20OnACurrentMulti(harness);

    setParameter(harness, 16383);
    CHECK(dataOfLastFrame(harness, 6) == bytes({0x00, 0x02, 0x01, 0x00, 0x7F, 0x7F}));
    CHECK(getParameter(harness, CHANNEL, MODULE, PARAMETER).value == 16383);

    setParameter(harness, -16383);
    CHECK(dataOfLastFrame(harness, 6) == bytes({0x00, 0x02, 0x01, 0x01, 0x7F, 0x7F}));
    CHECK(getParameter(harness, CHANNEL, MODULE, PARAMETER).value == -16383);

    setParameter(harness, 0);
    CHECK(dataOfLastFrame(harness, 6) == bytes({0x00, 0x02, 0x01, 0x00, 0x00, 0x00}));
    CHECK(getParameter(harness, CHANNEL, MODULE, PARAMETER).value == 0);
}

TEST_CASE("Given two parameters of a module and the same parameter of another module, When one is set, Then only it changes [RQ-AKM-101]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    seedEb20OnACurrentMulti(harness);

    setParameter(harness, 77);

    CHECK(getParameter(harness, CHANNEL, MODULE, PARAMETER).value == 77);
    CHECK(getParameter(harness, CHANNEL, MODULE, PARAMETER + 1).value == 0);
    CHECK(getParameter(harness, CHANNEL, MODULE + 1, PARAMETER).value == 0);
    CHECK(getParameter(harness, CHANNEL + 1, MODULE, PARAMETER).value == 0);
}

TEST_CASE("Given a magnitude of 16384, or an index of 128, When a parameter is set or read, Then the request is refused as ArgumentOutOfRange without sending [RQ-AKM-101]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    seedEb20OnACurrentMulti(harness);
    const auto sentBefore = harness.sentCount();

    akm::setFxParameter(harness.session(), CHANNEL, MODULE, PARAMETER, 16384, harness.recorder().completion());
    akm::setFxParameter(harness.session(), CHANNEL, MODULE, PARAMETER, -16384, harness.recorder().completion());
    akm::setFxParameter(harness.session(), CHANNEL, MODULE, 128, 1, harness.recorder().completion());
    akm::setFxParameter(harness.session(), 128, MODULE, PARAMETER, 1, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(4));
    for (const CommandResult& result : harness.recorder().results())
        requireRefused(result, RefusalReason::ArgumentOutOfRange);
    requireRefused(getParameter(harness, CHANNEL, MODULE, 128).outcome, RefusalReason::ArgumentOutOfRange);
    requireRefused(getParameter(harness, CHANNEL, 128, PARAMETER).outcome, RefusalReason::ArgumentOutOfRange);
    CHECK(harness.sentCount() == sentBefore);
}

TEST_CASE("Given a channel or a module the board does not have, or no current multi, When a parameter is set or read, Then the sampler's ERROR 04 is reported and nothing changes [RQ-AKM-101]",
          "[akm][multifx]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    REQUIRE(harness.establishChecksumMode(false).has_value());
    seedEb20OnACurrentMulti(harness);

    akm::setFxParameter(harness.session(), 4, MODULE, PARAMETER, 5, harness.recorder().completion());
    akm::setFxParameter(harness.session(), 2, 2, PARAMETER, 5, harness.recorder().completion());
    REQUIRE(harness.waitForCompletions(2));
    for (const CommandResult& result : harness.recorder().results())
        requireNotFound(result);
    const FxParameterResult missing = getParameter(harness, 0, 6, PARAMETER);
    requireNotFound(missing.outcome);
    CHECK_FALSE(missing.value.has_value());
    CHECK(getParameter(harness, 3, 1, PARAMETER).value == 0);

    ManualScenarioDriver otherDriver;
    SessionHarness noMulti{otherDriver};
    REQUIRE(noMulti.establishChecksumMode(false).has_value());
    noMulti.sampler().setFxBoard(eb20Layout());
    requireNotFound(getParameter(noMulti, CHANNEL, MODULE, PARAMETER).outcome);
}
