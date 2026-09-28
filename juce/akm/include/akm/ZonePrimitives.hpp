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
    // Zone sample assignment (§06/&01, &21), on a session: a thin typed wrapper over the catalogue's
    // record, like `ProgramPrimitives::createProgramWithKeygroups` — a mixed byte+String shape that
    // `makeStringRequest` cannot encode (it takes exactly one String argument), so the frame is built by
    // hand the same way. The other 13 §06 items (RQ-AKM-034) need no typed wrapper: they go through the
    // generic `makeRequest`/`decodeReply` path directly, like every §08 parameter (ADR-AKM-001,
    // DEC-AKM-003). [RQ-AKM-035, ADR-AKM-001 (DEC-AKM-013)]

    /// Assigns the sample named `name` to `zone` (1-4) of the current keygroup (§06/&01); an out-of-range
    /// zone or an invalid name is refused without sending. ERROR 04 ("requested item not found") when no
    /// sample of that name exists in the sampler's memory. [RQ-AKM-035]
    void setZoneSample(Session& session, int zone, std::string_view name, CommandCompletion completion);

    /// `name` is empty (`std::string{}`, not `std::nullopt`) when the REPLY is the single byte `00` the
    /// spec defines for "no sample assigned"; `std::nullopt` when the command did not complete on a
    /// decodable REPLY at all. `outcome` is the result of the command. [RQ-AKM-035]
    struct ZoneSampleResult
    {
        std::optional<std::string> name{};
        CommandResult outcome{};
    };
    using ZoneSampleCompletion = std::function<void(const ZoneSampleResult&)>;

    /// Gets the name of the sample assigned to `zone` (1-4) of the current keygroup (§06/&21); refused as
    /// `ChecksumModeUnknown` while the port's checksum mode is unknown, since a String REPLY has no fixed
    /// length to delimit it by (ADR-AKM-001, DEC-AKM-013). [RQ-AKM-035]
    void getZoneSample(Session& session, int zone, ZoneSampleCompletion completion);
}
