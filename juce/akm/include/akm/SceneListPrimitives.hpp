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

#include <string_view>

#include "akm/CommandResult.hpp"
#include "akm/Session.hpp"
#include "akm/SongPrimitives.hpp"

namespace akm
{
    // The scenelist primitives of section 14 (spec Tables 26-27), on a session: thin typed wrappers over
    // the catalogue's records (ADR-AKM-001, DEC-AKM-003, DEC-AKM-012, DEC-AKM-013). Section 14 has the shape
    // of the song file half of §16: a sampler-wide "current scenelist" selection state set by
    // `selectSceneListByName`/`selectSceneListByIndex`, and every primitive that acts on "the current"
    // scenelist acts on it. The protocol cannot build or edit a scenelist (spec p. 39). Each primitive
    // returns at once and reports on the session's thread, like `Session::submit`. The session must outlive
    // every call. The results are the song file ones, a count, an index and a name being the same thing.
    // [RQ-AKM-095, RQ-AKM-096]
    using SceneListCountResult = SongCountResult;
    using SceneListCountCompletion = SongCountCompletion;
    using SceneListIndexResult = SongIndexResult;
    using SceneListIndexCompletion = SongIndexCompletion;
    using SceneListNameResult = SongNameResult;
    using SceneListNameCompletion = SongNameCompletion;

    /// Selects the scenelist named `name` as current (§14/&05); ERROR 04 when none has that name.
    /// [RQ-AKM-095]
    void selectSceneListByName(Session& session, std::string_view name, CommandCompletion completion);

    /// Selects the scenelist at zero-based `index` as current (§14/&06), the wire carrying it as two 7-bit
    /// data bytes; ERROR 04 when it names no scenelist. [RQ-AKM-095]
    void selectSceneListByIndex(Session& session, int index, CommandCompletion completion);

    /// Deletes the current scenelist from memory (§14/&08); ERROR 04 when none is current. [RQ-AKM-095]
    void deleteCurrentSceneList(Session& session, CommandCompletion completion);

    /// Renames the current scenelist to `name` (§14/&09); ERROR 04 when none is current. [RQ-AKM-095]
    void renameCurrentSceneList(Session& session, std::string_view name, CommandCompletion completion);

    /// Gets the number of scenelists in memory (§14/&10); `count` is empty when the command did not
    /// complete on a REPLY of the length the catalogue gives it. [RQ-AKM-096]
    void getSceneListCount(Session& session, SceneListCountCompletion completion);

    /// Gets the current scenelist's index, its position in memory (§14/&13); `index` is empty when no
    /// scenelist is current. [RQ-AKM-096]
    void getCurrentSceneListIndex(Session& session, SceneListIndexCompletion completion);

    /// Gets the name of the scenelist at zero-based `index` (§14/&11), without making it current; refused
    /// as `ChecksumModeUnknown` while the port's checksum mode is unknown, since a String REPLY has no fixed
    /// length to delimit it by (ADR-AKM-001, DEC-AKM-013). [RQ-AKM-096, RQ-AKM-041]
    void getSceneListNameByIndex(Session& session, int index, SceneListNameCompletion completion);

    /// Gets the current scenelist's name (§14/&14); refused as `ChecksumModeUnknown` like
    /// `getSceneListNameByIndex`. [RQ-AKM-096, RQ-AKM-041]
    void getCurrentSceneListName(Session& session, SceneListNameCompletion completion);
}
