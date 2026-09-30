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
#pragma once

// Shared by the five parameter-group test files (Output, MIDI/Tune, Pitch Bend, LFOs, Keygroup
// Modulation Sources): one table-driven round trip proves RQ-AKM-024 ("a Get immediately after a Set
// SHALL return the value set") for every item, on the generic `makeRequest`/`decodeReply` path — no
// per-item typed helper exists for these groups (ADR-AKM-001, DEC-AKM-003: "a new item is a new record,
// not new code"). [TASK-AKM-018 to 022]
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

#include "SessionHarness.hpp"
#include "akm/ItemRequest.hpp"

namespace akm::test
{
    /// `values` is the full Set argument list (selector bytes, if any, followed by the value bytes); the
    /// selector prefix is however many arguments the paired Get item itself takes.
    struct ParameterCase
    {
        ItemId setId;
        ItemId getId;
        std::vector<std::int64_t> values;
    };

    inline void checkParameterRoundTrip(SessionHarness& harness, const ParameterCase& testCase)
    {
        INFO("set item 0x" << std::hex << static_cast<int>(descriptor(testCase.setId).item)
                           << ", get item 0x" << static_cast<int>(descriptor(testCase.getId).item));
        const std::size_t selectorCount = descriptor(testCase.getId).args.size();
        REQUIRE(testCase.values.size() >= selectorCount);
        const std::vector<std::int64_t> selector(testCase.values.begin(), testCase.values.begin() + static_cast<std::ptrdiff_t>(selectorCount));
        const std::vector<std::int64_t> expected(testCase.values.begin() + static_cast<std::ptrdiff_t>(selectorCount), testCase.values.end());

        auto setLatched = std::make_shared<Latched<CommandResult>>();
        harness.session().submit(makeRequest(testCase.setId, testCase.values),
                                 [setLatched](const CommandResult& r) { setLatched->set(r); });
        REQUIRE(harness.waitUntil([setLatched] { return setLatched->isSet(); }));
        REQUIRE(std::holds_alternative<Done>(*setLatched->value()));

        auto getLatched = std::make_shared<Latched<CommandResult>>();
        harness.session().submit(makeRequest(testCase.getId, selector),
                                 [getLatched](const CommandResult& r) { getLatched->set(r); });
        REQUIRE(harness.waitUntil([getLatched] { return getLatched->isSet(); }));
        const CommandResult getResult = *getLatched->value();
        REQUIRE(std::holds_alternative<Reply>(getResult));
        const auto decoded = decodeReply(testCase.getId, std::get<Reply>(getResult).data);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == expected);
    }

    inline void checkParameterRoundTrips(SessionHarness& harness, const std::vector<ParameterCase>& cases)
    {
        for (const ParameterCase& testCase : cases)
            checkParameterRoundTrip(harness, testCase);
    }
}
