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

#include <optional>
#include <string_view>

#include "akm/Checksum.hpp"
#include "akm/Confirmation.hpp"

namespace akm
{
    /// What a session reports besides the result of a command. [RQ-AKM-006, RQ-AKM-007, RQ-AKM-013,
    /// RQ-AKM-041]
    enum class DiagnosticKind
    {
        /// A received message the codec refused; `rejection` says why. [RQ-AKM-006]
        RejectedMessage,
        /// A well-formed confirmation that matches no pending command, so it completed nothing.
        /// [RQ-AKM-007]
        UnsolicitedConfirmation,
        /// An ERROR for a command a REPLY had already completed (spec p. 6), reported with the identity
        /// of that command, since cycling user-refs make it unattributable otherwise. [RQ-AKM-007]
        LateErrorAfterReply,
        /// The port's checksum mode changed; `checksumMode` is the new one. [RQ-AKM-013, RQ-AKM-041]
        ChecksumModeChanged,
    };

    /// One reported event. Which optional fields are set depends on the kind, as documented above.
    struct Diagnostic
    {
        DiagnosticKind kind = DiagnosticKind::RejectedMessage;
        std::optional<RejectReason> rejection{};
        std::optional<Confirmation> confirmation{};
        std::optional<ChecksumMode> checksumMode{};
    };

    /// Where a session reports what it cannot return through a completion, so that the library depends
    /// on no logger. [RQ-AKM-006, ADR-AKM-001 (DEC-AKM-004)]
    class DiagnosticSink
    {
    public:
        virtual ~DiagnosticSink() = default;

        /// Called on the session's thread, one at a time. It must neither throw nor block.
        virtual void report(const Diagnostic& diagnostic) = 0;
    };

    /// Drops every diagnostic, for a caller that wants none.
    class NullDiagnosticSink final : public DiagnosticSink
    {
    public:
        void report(const Diagnostic&) override {}
    };

    /// A short text naming the kind of event. [RQ-AKM-006]
    [[nodiscard]] std::string_view describe(DiagnosticKind kind);
}
