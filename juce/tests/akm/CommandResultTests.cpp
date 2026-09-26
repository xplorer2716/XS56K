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

// The small value types of the session's API: the six results, the reasons a command is refused, the
// diagnostics. Their functions had no test of their own when the session core was delivered.
// [TASK-AKM-006, TASK-AKM-008, RQ-AKM-009, RQ-AKM-041, RQ-AKM-043]
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <set>
#include <string>

#include "akm/CommandResult.hpp"
#include "akm/DiagnosticSink.hpp"
#include "akm/SamplerError.hpp"

using Catch::Matchers::ContainsSubstring;

namespace
{
    constexpr int UNDEFINED_ENUMERATOR = 99;

    std::string text(akm::RefusalReason reason)
    {
        return std::string(akm::describe(reason));
    }

    std::string text(akm::DiagnosticKind kind)
    {
        return std::string(akm::describe(kind));
    }
}

TEST_CASE("Given each kind of result, When asked whether the sampler carried the command out, Then only a Done and a Reply say yes [RQ-AKM-009]",
          "[akm][result]")
{
    CHECK(akm::succeeded(akm::Done{}));
    CHECK(akm::succeeded(akm::Reply{{0x01, 0x02}}));
    CHECK_FALSE(akm::succeeded(akm::Error{akm::error_number::OUT_OF_RANGE}));
    CHECK_FALSE(akm::succeeded(akm::Timeout{}));
    CHECK_FALSE(akm::succeeded(akm::Refused{akm::RefusalReason::NotEncodable}));
    CHECK_FALSE(akm::succeeded(akm::Cancelled{}));
}

TEST_CASE("Given a sequence outcome, When asked whether all its commands succeeded, Then it says so only without a failure index [RQ-AKM-043]",
          "[akm][result]")
{
    akm::SequenceResult clean;
    clean.results = {akm::Done{}, akm::Reply{{0x01}}};
    CHECK(clean.allSucceeded());

    akm::SequenceResult failed;
    failed.results = {akm::Done{}, akm::Timeout{}, akm::Cancelled{}};
    failed.failureIndex = 1;
    CHECK_FALSE(failed.allSucceeded());
}

TEST_CASE("Given each reason a command is refused, When described, Then each has its own text and an unknown one says so [RQ-AKM-001, RQ-AKM-014, RQ-AKM-041, RQ-AKM-042]",
          "[akm][result]")
{
    using akm::RefusalReason;
    const RefusalReason reasons[] = {RefusalReason::NotEncodable,       RefusalReason::WrongArgumentCount,
                                     RefusalReason::ArgumentOutOfRange, RefusalReason::ChecksumModeUnknown,
                                     RefusalReason::NoTargetBound,      RefusalReason::SessionClosed};

    std::set<std::string> distinct;
    for (const RefusalReason reason : reasons)
    {
        CHECK_FALSE(text(reason).empty());
        distinct.insert(text(reason));
    }
    CHECK(distinct.size() == std::size(reasons));

    CHECK_THAT(text(RefusalReason::ArgumentOutOfRange), ContainsSubstring("range"));
    CHECK_THAT(text(RefusalReason::WrongArgumentCount), ContainsSubstring("number of arguments"));
    CHECK_THAT(text(RefusalReason::ChecksumModeUnknown), ContainsSubstring("checksum mode unknown"));
    CHECK_THAT(text(static_cast<RefusalReason>(UNDEFINED_ENUMERATOR)), ContainsSubstring("unknown"));
}

TEST_CASE("Given each kind of diagnostic, When described, Then each has its own text and an unknown one says so [RQ-AKM-006, RQ-AKM-007, RQ-AKM-041]",
          "[akm][result]")
{
    using akm::DiagnosticKind;
    const DiagnosticKind kinds[] = {DiagnosticKind::RejectedMessage, DiagnosticKind::UnsolicitedConfirmation,
                                    DiagnosticKind::LateErrorAfterReply, DiagnosticKind::ChecksumModeChanged};

    std::set<std::string> distinct;
    for (const DiagnosticKind kind : kinds)
    {
        CHECK_FALSE(text(kind).empty());
        distinct.insert(text(kind));
    }
    CHECK(distinct.size() == std::size(kinds));

    CHECK_THAT(text(DiagnosticKind::LateErrorAfterReply), ContainsSubstring("REPLY"));
    CHECK_THAT(text(static_cast<DiagnosticKind>(UNDEFINED_ENUMERATOR)), ContainsSubstring("unknown"));
}

TEST_CASE("Given the null diagnostic sink, When a diagnostic is reported, Then nothing happens [RQ-AKM-006]",
          "[akm][result]")
{
    akm::NullDiagnosticSink sink;
    akm::Diagnostic diagnostic;
    diagnostic.kind = akm::DiagnosticKind::UnsolicitedConfirmation;

    CHECK_NOTHROW(sink.report(diagnostic));
}
