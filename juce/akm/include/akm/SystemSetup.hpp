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
    // The system setup primitives of section 02 (spec Tables 6-7) other than the operating-system version
    // (`SystemVersion.hpp`), on a session: thin typed wrappers over the catalogue's records (ADR-AKM-001,
    // DEC-AKM-003, DEC-AKM-012, DEC-AKM-013). Every §02 item is sampler-wide, with no current item. Each
    // returns at once and reports on the session's thread, like `Session::submit`. The session must
    // outlive every call. [FTR-AKM-006]

    /// Sets the sampler's name (§02/&02); a name longer than the catalogue's 20 characters or not 7-bit
    /// ASCII is refused without sending. [RQ-AKM-052]
    void setSamplerName(Session& session, std::string_view name, CommandCompletion completion);

    /// `name` is empty when the command did not complete on a REPLY holding exactly one null-terminated
    /// name; `outcome` is the result of the command. [RQ-AKM-052]
    struct SamplerNameResult
    {
        std::optional<std::string> name{};
        CommandResult outcome{};
    };
    using SamplerNameCompletion = std::function<void(const SamplerNameResult&)>;

    /// Gets the sampler's name (§02/&03); refused as `ChecksumModeUnknown` while the port's checksum mode
    /// is unknown, since a String REPLY has no fixed length to delimit it by (ADR-AKM-001, DEC-AKM-013).
    /// [RQ-AKM-052, RQ-AKM-041]
    void getSamplerName(Session& session, SamplerNameCompletion completion);
}
