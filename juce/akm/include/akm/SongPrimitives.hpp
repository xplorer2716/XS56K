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
    // The song file primitives of section 16 (spec Tables 28-29), on a session: thin typed wrappers over
    // the catalogue's records (ADR-AKM-001, DEC-AKM-003, DEC-AKM-012, DEC-AKM-013). Like §0E, §16 has a
    // sampler-wide "current song file" selection state set by `selectSongByName`/`selectSongByIndex`,
    // and every primitive that acts on "the current" song file acts on it. Each returns at once and
    // reports on the session's thread, like `Session::submit`. The session must outlive every call.
    // Playing a song file is done by standard MIDI messages, not by SysEx (spec p. 40).
    // [RQ-AKM-082, RQ-AKM-083]

    /// Selects the song file named `name` as current (§16/&05); ERROR 04 when none has that name.
    /// [RQ-AKM-082]
    void selectSongByName(Session& session, std::string_view name, CommandCompletion completion);

    /// Selects the song file at zero-based `index` as current (§16/&06), the wire carrying it as two
    /// 7-bit data bytes; ERROR 04 when it names no song file. [RQ-AKM-082]
    void selectSongByIndex(Session& session, int index, CommandCompletion completion);

    /// Deletes the current song file from memory (§16/&08); ERROR 04 when none is current. [RQ-AKM-082]
    void deleteCurrentSong(Session& session, CommandCompletion completion);

    /// Renames the current song file to `name` (§16/&09); ERROR 04 when none is current. [RQ-AKM-082]
    void renameCurrentSong(Session& session, std::string_view name, CommandCompletion completion);

    /// `count` is empty when the command did not complete on a REPLY of the length the catalogue gives
    /// it; `outcome` is the result of the command. [RQ-AKM-083]
    struct SongCountResult
    {
        std::optional<int> count{};
        CommandResult outcome{};
    };
    using SongCountCompletion = std::function<void(const SongCountResult&)>;

    /// Gets the number of song files in memory (§16/&10). [RQ-AKM-083]
    void getSongCount(Session& session, SongCountCompletion completion);

    /// `index` is empty when no song file is current (or the command did not complete on a REPLY of the
    /// length the catalogue gives it); `outcome` is the result of the command. [RQ-AKM-083]
    struct SongIndexResult
    {
        std::optional<int> index{};
        CommandResult outcome{};
    };
    using SongIndexCompletion = std::function<void(const SongIndexResult&)>;

    /// Gets the current song file's index, its position in memory (§16/&13). [RQ-AKM-083]
    void getCurrentSongIndex(Session& session, SongIndexCompletion completion);

    /// `name` is empty when the command did not complete on a REPLY holding exactly one null-terminated
    /// name; `outcome` is the result of the command. [RQ-AKM-083]
    struct SongNameResult
    {
        std::optional<std::string> name{};
        CommandResult outcome{};
    };
    using SongNameCompletion = std::function<void(const SongNameResult&)>;

    /// Gets the name of the song file at zero-based `index` (§16/&11), without making it current;
    /// refused as `ChecksumModeUnknown` while the port's checksum mode is unknown, since a String REPLY
    /// has no fixed length to delimit it by (ADR-AKM-001, DEC-AKM-013). [RQ-AKM-083, RQ-AKM-041]
    void getSongNameByIndex(Session& session, int index, SongNameCompletion completion);

    /// Gets the current song file's name (§16/&14); refused as `ChecksumModeUnknown` like
    /// `getSongNameByIndex`. [RQ-AKM-083, RQ-AKM-041]
    void getCurrentSongName(Session& session, SongNameCompletion completion);
}
