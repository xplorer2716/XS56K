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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace akm
{
    /// Why the session did not send a command at all. [RQ-AKM-001, RQ-AKM-041, RQ-AKM-042, RQ-AKM-043,
    /// ADR-AKM-001 (DEC-AKM-004)]
    enum class RefusalReason
    {
        NotEncodable,         ///< the codec could not build a legal frame from it (RQ-AKM-001)
        ChecksumModeUnknown,  ///< its REPLY cannot be delimited while the mode is unknown (RQ-AKM-041)
        NoTargetBound,        ///< it is addressed to the bound target and none is bound yet (RQ-AKM-039)
        SessionClosed,        ///< it was submitted after close() (RQ-AKM-042)
    };

    /// The sampler carried the command out and returned no data: it answered DONE. [RQ-AKM-009]
    struct Done
    {
        friend bool operator==(const Done&, const Done&) = default;
    };

    /// The sampler carried the command out and returned data: it answered REPLY. [RQ-AKM-009]
    struct Reply
    {
        std::vector<std::uint8_t> data;

        friend bool operator==(const Reply&, const Reply&) = default;
    };

    /// The sampler refused the command: it answered ERROR, `number` being Data1 * 128 + Data2
    /// (`describeError` gives its meaning). An ERROR carrying fewer than two data bytes — possible only
    /// while the checksum mode is Off, which lets any trailing byte count as data — is reported as
    /// `error_number::UNKNOWN_ERROR`, so that the port is released all the same. [RQ-AKM-005, RQ-AKM-009]
    struct Error
    {
        std::uint16_t number = 0;

        friend bool operator==(const Error&, const Error&) = default;
    };

    /// Neither DONE, REPLY nor ERROR arrived within the command's timeout, or the command waited longer
    /// than its maximum total wait. [RQ-AKM-010, RQ-AKM-011]
    struct Timeout
    {
        friend bool operator==(const Timeout&, const Timeout&) = default;
    };

    /// The session refused the command without putting a frame on the wire. [RQ-AKM-041, RQ-AKM-043]
    struct Refused
    {
        RefusalReason reason = RefusalReason::NotEncodable;

        friend bool operator==(const Refused&, const Refused&) = default;
    };

    /// The command was dropped without being sent: an earlier command of its sequence failed
    /// (RQ-AKM-043), or the session was closed while it waited (RQ-AKM-042).
    struct Cancelled
    {
        friend bool operator==(const Cancelled&, const Cancelled&) = default;
    };

    /// What a completion receives, exactly one of the six outcomes of ADR-AKM-001 (DEC-AKM-004).
    using CommandResult = std::variant<Done, Reply, Error, Timeout, Refused, Cancelled>;

    /// Whether the sampler carried the command out: a Done or a Reply. [RQ-AKM-009]
    [[nodiscard]] bool succeeded(const CommandResult& result);

    /// The outcome of a sequence: one result per submitted command, in submission order, and the index
    /// of the first command that did not succeed — the one that failed and cancelled the rest, or the
    /// first one cancelled by a close. [RQ-AKM-042, RQ-AKM-043, ADR-AKM-001 (DEC-AKM-010)]
    struct SequenceResult
    {
        std::vector<CommandResult> results;
        std::optional<std::size_t> failureIndex;

        [[nodiscard]] bool allSucceeded() const { return !failureIndex.has_value(); }

        friend bool operator==(const SequenceResult&, const SequenceResult&) = default;
    };

    /// A short text naming the reason a command was refused. [RQ-AKM-041, RQ-AKM-043]
    [[nodiscard]] std::string_view describe(RefusalReason reason);
}
