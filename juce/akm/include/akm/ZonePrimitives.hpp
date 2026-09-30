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
#include <vector>

#include "akm/CommandResult.hpp"
#include "akm/ItemCatalogue.hpp"
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

    // "Several zones" REPLY shapes (RQ-AKM-036): a Get issued with zone 0 answers one value set per zone
    // of the current keygroup; issued with zone 0 while keygroup 0 is also current, one value set per
    // zone of every keygroup. Both reuse `decodeRepeatedReply` (ADR-AKM-001, DEC-AKM-015, already
    // generalised "for any item"), the same way `getForAllKeygroups` does for keygroups — no new decode
    // mechanism, only composing the existing one and, for the second shape, reshaping its flat result.

    /// One decoded value set per zone of the current keygroup, in zone order starting at 1 (RQ-AKM-036);
    /// empty when the command did not complete on a decodable REPLY, or the number of sets decoded
    /// differs from `expectedZoneCount` (normally 4) — a mismatch is not distinguished from any other
    /// decode failure: both leave `values` empty, `outcome` still carrying the raw result.
    struct AllZonesResult
    {
        std::optional<std::vector<std::vector<std::int64_t>>> values{};
        CommandResult outcome{};
    };
    using AllZonesCompletion = std::function<void(const AllZonesResult&)>;

    /// Issues Get `getId` with zone 0 ("all four"), decoding its REPLY as one value set per zone
    /// (RQ-AKM-036). [RQ-AKM-036]
    void getForAllZones(Session& session, ItemId getId, int expectedZoneCount, AllZonesCompletion completion);

    /// One decoded value set per zone of every keygroup of the current program — `values[keygroup][zone]`,
    /// keygroup order starting at 1, zone order starting at 1 within each keygroup (RQ-AKM-036); empty
    /// when the command did not complete on a decodable REPLY, or the total number of sets decoded
    /// differs from `expectedKeygroupCount * expectedZoneCount`.
    struct AllZonesAllKeygroupsResult
    {
        std::optional<std::vector<std::vector<std::vector<std::int64_t>>>> values{};
        CommandResult outcome{};
    };
    using AllZonesAllKeygroupsCompletion = std::function<void(const AllZonesAllKeygroupsResult&)>;

    /// Issues Get `getId` with zone 0 while keygroup 0 ("all") is current, decoding its REPLY as one
    /// value set per zone of every keygroup and reshaping the flat, keygroup-major decode into
    /// `values[keygroup][zone]`. [RQ-AKM-036]
    void getForAllZonesAllKeygroups(Session& session, ItemId getId, int expectedKeygroupCount, int expectedZoneCount,
                                    AllZonesAllKeygroupsCompletion completion);
}
