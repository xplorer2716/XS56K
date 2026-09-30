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

// The 4 read-only §0E items (Type, Channels, Length, Rate: &30-&33) and the two grouped-REPLY
// convenience items (&34 groups &30-&33, &4B groups &40-&4A, spec Tables 18-19): each grouped REPLY
// decodes to the same values as reading its members individually. [TASK-AKM-044, RQ-AKM-049]
#include <catch2/catch_test_macros.hpp>

#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/SamplePrimitives.hpp"
#include "akm/SamplerError.hpp"

using akm::CommandResult;
using akm::Done;
using akm::Reply;
using akm::harness::ManualScenarioDriver;
using akm::test::SessionHarness;

namespace
{
    struct SetCall
    {
        akm::ItemId id;
        std::vector<std::int64_t> values;
    };

    void seedAndSelectASample(SessionHarness& harness)
    {
        harness.sampler().setSampleNames({"A"});
        harness.sampler().setSampleAttributes(0, /*type=*/1, /*channels=*/2, /*length=*/123456, /*rate=*/44100);
        akm::selectSampleByName(harness.session(), "A", harness.recorder().completion());
        REQUIRE(harness.waitForCompletions(1));
    }

    // Appends `src` to `dest` with a plain loop rather than `dest.insert(dest.end(), src.begin(),
    // src.end())`: GCC 11's Release build (-O2, -Werror) false-positives -Wstringop-overread on that
    // range-insert (linux-x64-release-canary on PR #4; not reproduced in Debug or on MSVC/Clang) — a
    // known GCC inlining bug, not a real out-of-bounds read.
    void appendAll(std::vector<std::int64_t>& dest, const std::vector<std::int64_t>& src)
    {
        for (const std::int64_t value : src)
            dest.push_back(value);
    }

    std::vector<std::int64_t> getValues(SessionHarness& harness, akm::ItemId id)
    {
        auto latched = std::make_shared<akm::test::Latched<CommandResult>>();
        harness.session().submit(akm::makeRequest(id, {}), [latched](const CommandResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        const CommandResult result = *latched->value();
        REQUIRE(std::holds_alternative<Reply>(result));
        const auto decoded = akm::decodeReply(id, std::get<Reply>(result).data);
        REQUIRE(decoded.has_value());
        return *decoded;
    }
}

TEST_CASE("Given a simulated sample with a known length and rate, When &34 is read, Then it decodes to the same four values as reading &30-&33 individually [RQ-AKM-049]",
          "[akm][sample]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    seedAndSelectASample(harness);

    std::vector<std::int64_t> expected;
    for (const akm::ItemId id : {akm::ItemId::SampleGetType, akm::ItemId::SampleGetChannels,
                                 akm::ItemId::SampleGetLength, akm::ItemId::SampleGetRate})
    {
        const std::vector<std::int64_t> values = getValues(harness, id);
        appendAll(expected, values);
    }

    CHECK(getValues(harness, akm::ItemId::SampleGetAllBasicParams) == expected);
}

TEST_CASE("Given a sample with its settable parameters set, When &4B is read, Then it decodes to the same eight values as reading &40-&4A individually [RQ-AKM-049]",
          "[akm][sample]")
{
    ManualScenarioDriver driver;
    SessionHarness harness{driver};
    seedAndSelectASample(harness);

    const std::vector<SetCall> setCalls{
        {akm::ItemId::SampleSetStartPosition, {1, 2, 3, 4}},
        {akm::ItemId::SampleSetEndPosition, {5, 6, 7, 8}},
        {akm::ItemId::SampleSetOriginalPitch, {60}},
        {akm::ItemId::SampleSetSemitoneTune, {0, 12}},
        {akm::ItemId::SampleSetFineTune, {1, 25}},
        {akm::ItemId::SampleSetPlaybackMode, {3}},
        {akm::ItemId::SampleSetLoopStart, {0, 10, 20, 30}},
        {akm::ItemId::SampleSetLoopEnd, {0, 11, 21, 31}},
    };
    for (const SetCall& call : setCalls)
    {
        auto latched = std::make_shared<akm::test::Latched<CommandResult>>();
        harness.session().submit(akm::makeRequest(call.id, call.values), [latched](const CommandResult& r) { latched->set(r); });
        REQUIRE(harness.waitUntil([latched] { return latched->isSet(); }));
        REQUIRE(std::holds_alternative<Done>(*latched->value()));
    }

    std::vector<std::int64_t> expected;
    for (const akm::ItemId id : {akm::ItemId::SampleGetStartPosition, akm::ItemId::SampleGetEndPosition,
                                 akm::ItemId::SampleGetOriginalPitch, akm::ItemId::SampleGetSemitoneTune,
                                 akm::ItemId::SampleGetFineTune, akm::ItemId::SampleGetPlaybackMode,
                                 akm::ItemId::SampleGetLoopStart, akm::ItemId::SampleGetLoopEnd})
    {
        const std::vector<std::int64_t> values = getValues(harness, id);
        appendAll(expected, values);
    }

    CHECK(getValues(harness, akm::ItemId::SampleGetAllSettableParams) == expected);
}
