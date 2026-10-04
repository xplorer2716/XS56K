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

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "akm/CommandResult.hpp"
#include "akm/Session.hpp"

namespace akm
{
    // The multi primitives of section 0C (spec Tables 16-17), on a session: thin typed wrappers over the
    // catalogue's records (ADR-AKM-001, DEC-AKM-003, DEC-AKM-012, DEC-AKM-013). §0C has a sampler-wide "current
    // multi" selection state, set by creating a multi or selecting one, the same pattern as §0A's current
    // program: every primitive that acts on "the current multi" acts on it. Each returns at once and reports on
    // the session's thread, like `Session::submit`. The session must outlive every call.
    // [RQ-AKM-087]

    /// The number of parts of the multis created from now on (§0C/&01); the wire carries 0, 1 or 2.
    /// A stored setting no item reads back. [RQ-AKM-087]
    enum class MultiPartCount
    {
        Parts32 = 0,
        Parts64 = 1,
        Parts128 = 2,
    };

    /// Sets the number of parts of new multis (§0C/&01). [RQ-AKM-087]
    void setNewMultiPartCount(Session& session, MultiPartCount partCount, CommandCompletion completion);

    /// Creates a multi named `name` and makes it current (§0C/&02); ERROR 05 when a multi of that name already
    /// exists. [RQ-AKM-087]
    void createMulti(Session& session, std::string_view name, CommandCompletion completion);

    /// Selects the multi named `name` as current (§0C/&05); ERROR 04 when none has that name. [RQ-AKM-087]
    void selectMultiByName(Session& session, std::string_view name, CommandCompletion completion);

    /// Selects the multi at zero-based `index` as current (§0C/&06), the wire carrying it as two 7-bit data
    /// bytes; ERROR 04 when it names no multi. [RQ-AKM-087]
    void selectMultiByIndex(Session& session, int index, CommandCompletion completion);

    /// Deletes the current multi from memory (§0C/&08); ERROR 04 when none is current. [RQ-AKM-087]
    void deleteCurrentMulti(Session& session, CommandCompletion completion);

    /// `index` is empty when no multi is current (or the command did not complete on a REPLY of the length the
    /// catalogue gives it); `outcome` is the result of the command. [RQ-AKM-087, RQ-AKM-091]
    struct MultiIndexResult
    {
        std::optional<int> index{};
        CommandResult outcome{};
    };
    using MultiIndexCompletion = std::function<void(const MultiIndexResult&)>;

    /// Gets the current multi's index, its position in memory (§0C/&42). [RQ-AKM-087, RQ-AKM-091]
    void getCurrentMultiIndex(Session& session, MultiIndexCompletion completion);

    /// `name` is empty when the command did not complete on a REPLY holding exactly one null-terminated name;
    /// `outcome` is the result of the command. [RQ-AKM-087, RQ-AKM-091]
    struct MultiNameResult
    {
        std::optional<std::string> name{};
        CommandResult outcome{};
    };
    using MultiNameCompletion = std::function<void(const MultiNameResult&)>;

    /// Gets the current multi's name (§0C/&43); refused as `ChecksumModeUnknown` while the port's checksum mode
    /// is unknown, since a String REPLY has no fixed length to delimit it by (ADR-AKM-001, DEC-AKM-013).
    /// [RQ-AKM-087, RQ-AKM-091, RQ-AKM-041]
    void getCurrentMultiName(Session& session, MultiNameCompletion completion);
}
