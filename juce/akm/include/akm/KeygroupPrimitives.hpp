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

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "akm/CommandResult.hpp"
#include "akm/ItemCatalogue.hpp"
#include "akm/Session.hpp"

namespace akm
{
    // The keygroup selection primitives of section 08 (spec Tables 11-12), on a session: thin typed
    // wrappers over the catalogue's records (ADR-AKM-001, DEC-AKM-003, DEC-AKM-012), like
    // `ProgramPrimitives`. Every other §08 item and every §06 item acts on the keygroup selected here.
    // [RQ-AKM-028]

    /// Selects keygroup `keygroup` (1-99) as current, or 0 for "all keygroups" (§08/&01); a keygroup
    /// number outside 0-99 is refused without sending. ERROR `KEYGROUP_NOT_IN_PROGRAM` (0x181) when the
    /// current program does not have that many keygroups. [RQ-AKM-028]
    void selectKeygroup(Session& session, int keygroup, CommandCompletion completion);

    /// `keygroup` is empty when the command did not complete on a REPLY of the length the catalogue
    /// gives it; `outcome` is the result of the command. [RQ-AKM-028]
    struct CurrentKeygroupResult
    {
        std::optional<int> keygroup{};
        CommandResult outcome{};
    };
    using CurrentKeygroupCompletion = std::function<void(const CurrentKeygroupResult&)>;

    /// Gets which keygroup is current (§08/&02); 0 means "all keygroups" are current. [RQ-AKM-028]
    void getCurrentKeygroup(Session& session, CurrentKeygroupCompletion completion);

    /// One decoded value set per keygroup of the current program, in keygroup order starting at 1
    /// (RQ-AKM-031); empty when the command did not complete on a decodable REPLY, or the number of sets
    /// decoded differs from `expectedKeygroupCount` (a mismatch is not distinguished from any other
    /// decode failure: both leave `values` empty, `outcome` still carrying the raw result).
    struct AllKeygroupsResult
    {
        std::optional<std::vector<std::vector<std::int64_t>>> values{};
        CommandResult outcome{};
    };
    using AllKeygroupsCompletion = std::function<void(const AllKeygroupsResult&)>;

    /// Issues Get `getId` while keygroup 0 ("all") is current, decoding its REPLY as one value set per
    /// keygroup (RQ-AKM-031, extending ADR-AKM-001's DEC-AKM-014 via `decodeRepeatedReply`) and checking
    /// the count against `expectedKeygroupCount` — normally the current program's keygroup count
    /// (`getProgramKeygroupCount`), read by the caller since the session does not track it itself.
    /// [RQ-AKM-031]
    void getForAllKeygroups(Session& session, ItemId getId, int expectedKeygroupCount, AllKeygroupsCompletion completion);
}
