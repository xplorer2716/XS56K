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
    // The program lifecycle primitives of section 0A (spec Tables 13-14), on a session: thin typed
    // wrappers over the catalogue's records (ADR-AKM-001, DEC-AKM-003, DEC-AKM-012, DEC-AKM-013). Each
    // returns at once and reports on the session's thread, like `Session::submit`. The session must
    // outlive every call. [RQ-AKM-021]

    /// Creates a program named `name` (§0A/&02, one keygroup) and makes it current. [RQ-AKM-021]
    void createProgram(Session& session, std::string_view name, CommandCompletion completion);

    /// Creates a program named `name` with `keygroupCount` keygroups (§0A/&03, 1-99) and makes it
    /// current; an out-of-range count or an invalid name is refused without sending. [RQ-AKM-021]
    void createProgramWithKeygroups(Session& session, int keygroupCount, std::string_view name,
                                    CommandCompletion completion);

    /// Selects the program named `name` as current (§0A/&05); ERROR 04 when no program has that name.
    /// [RQ-AKM-021]
    void selectProgramByName(Session& session, std::string_view name, CommandCompletion completion);

    /// Selects the program at zero-based `index` as current (§0A/&06); an index outside 0-16383 (what
    /// two data bytes hold) is refused without sending, ERROR 04 when it names no program. [RQ-AKM-021]
    void selectProgramByIndex(Session& session, int index, CommandCompletion completion);

    /// Deletes the current program (§0A/&08). [RQ-AKM-021]
    void deleteCurrentProgram(Session& session, CommandCompletion completion);

    /// Renames the current program to `name` (§0A/&09). [RQ-AKM-021]
    void renameCurrentProgram(Session& session, std::string_view name, CommandCompletion completion);

    /// `count` is empty when the command did not complete on a REPLY of the length the catalogue gives
    /// it; `outcome` is the result of the command. [RQ-AKM-021, RQ-AKM-023]
    struct ProgramCountResult
    {
        std::optional<int> count{};
        CommandResult outcome{};
    };
    using ProgramCountCompletion = std::function<void(const ProgramCountResult&)>;

    /// Gets the number of programs in memory (§0A/&10). [RQ-AKM-021, RQ-AKM-023]
    void getProgramCount(Session& session, ProgramCountCompletion completion);

    /// `name` is empty when the command did not complete on a REPLY holding exactly one null-terminated
    /// name; `outcome` is the result of the command. [RQ-AKM-021, RQ-AKM-023]
    struct ProgramNameResult
    {
        std::optional<std::string> name{};
        CommandResult outcome{};
    };
    using ProgramNameCompletion = std::function<void(const ProgramNameResult&)>;

    /// Gets the current program's name (§0A/&13); refused as `ChecksumModeUnknown` while the port's
    /// checksum mode is unknown, since a String REPLY has no fixed length to delimit it by
    /// (ADR-AKM-001, DEC-AKM-013). [RQ-AKM-021, RQ-AKM-023, RQ-AKM-041]
    void getCurrentProgramName(Session& session, ProgramNameCompletion completion);

    // The structure and identity primitives of section 0A (§0A/&0A-&0D, &11, &14, &15). [RQ-AKM-022]

    /// Sets or clears the "Program Number" front-panel display (§0A/&0A). `frontPanelNumber` is the
    /// number as shown on the front panel (1-128); the wire carries it minus one (spec Table 13,
    /// footnote a) — resolving FTR-AKM-002's "program numbering" open point. `std::nullopt` switches it
    /// off. A number outside 1-128 is refused without sending. [RQ-AKM-022]
    void setProgramNumber(Session& session, std::optional<int> frontPanelNumber, CommandCompletion completion);

    /// `frontPanelNumber` is empty when the display is off (or the command did not complete on a REPLY
    /// of the length the catalogue gives it); `outcome` is the result of the command. [RQ-AKM-022,
    /// RQ-AKM-023]
    struct ProgramNumberResult
    {
        std::optional<int> frontPanelNumber{};
        CommandResult outcome{};
    };
    using ProgramNumberCompletion = std::function<void(const ProgramNumberResult&)>;

    /// Gets the current program's "Program Number" (§0A/&11), converted back to the front-panel number.
    /// [RQ-AKM-022, RQ-AKM-023]
    void getProgramNumber(Session& session, ProgramNumberCompletion completion);

    /// Adds `count` keygroups (1-98) to the current program (§0A/&0B); an out-of-range count is refused
    /// without sending. [RQ-AKM-022]
    void addKeygroupsToProgram(Session& session, int count, CommandCompletion completion);

    /// Deletes keygroup `keygroup` (zero-based, 0-98) from the current program (§0A/&0C); ERROR 385
    /// (`error_number::KEYGROUP_NOT_IN_PROGRAM`) when it does not exist. [RQ-AKM-022]
    void deleteKeygroupFromProgram(Session& session, int keygroup, CommandCompletion completion);

    /// Sets the current program's keygroup crossfade (§0A/&0D). [RQ-AKM-022]
    void setKeygroupCrossfade(Session& session, bool on, CommandCompletion completion);

    /// [RQ-AKM-022, RQ-AKM-023]
    struct ProgramKeygroupCountResult
    {
        std::optional<int> count{};
        CommandResult outcome{};
    };
    using ProgramKeygroupCountCompletion = std::function<void(const ProgramKeygroupCountResult&)>;

    /// Gets the number of keygroups in the current program (§0A/&14). [RQ-AKM-022, RQ-AKM-023]
    void getProgramKeygroupCount(Session& session, ProgramKeygroupCountCompletion completion);

    /// [RQ-AKM-022, RQ-AKM-023]
    struct ProgramCrossfadeResult
    {
        std::optional<bool> enabled{};
        CommandResult outcome{};
    };
    using ProgramCrossfadeCompletion = std::function<void(const ProgramCrossfadeResult&)>;

    /// Gets the current program's keygroup crossfade (§0A/&15). [RQ-AKM-022, RQ-AKM-023]
    void getKeygroupCrossfade(Session& session, ProgramCrossfadeCompletion completion);
}
