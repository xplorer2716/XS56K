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

    /// Passed to `deleteAllMultis` to prove the caller means it. A default `bool` could be satisfied by
    /// accident (`true`, `1`, a stray flag); this enumerator cannot — it must be named. [RQ-AKM-088]
    enum class ConfirmDeleteAllMultis
    {
        IUnderstandThisDeletesEveryMultiInMemory,
    };

    /// Deletes every multi in memory (§0C/&07) — irreversible without a saved backup. `confirmation` has no
    /// default: sent only when it is the enumerator; `std::nullopt` refuses the command as `NotConfirmed`
    /// without sending anything. No real-sampler test of any feature calls this. [RQ-AKM-088]
    void deleteAllMultis(Session& session, std::optional<ConfirmDeleteAllMultis> confirmation, CommandCompletion completion);

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

    // The Sets of general information about the current multi (§0C/&30-&34). [RQ-AKM-092]

    /// Renames the current multi to `name` (§0C/&30); ERROR 04 when none is current. [RQ-AKM-092]
    void renameCurrentMulti(Session& session, std::string_view name, CommandCompletion completion);

    /// Sets or clears the current multi's program number (§0C/&31). `frontPanelNumber` is the number as shown on
    /// the front panel (1-128); the wire carries it minus one (spec Table 16, footnote a). `std::nullopt`
    /// switches it off. A number outside 1-128 is refused without sending. ERROR 04 when no multi is current.
    /// [RQ-AKM-092]
    void setMultiProgramNumber(Session& session, std::optional<int> frontPanelNumber, CommandCompletion completion);

    /// Assigns the program at zero-based `programIndex` of the sampler's memory to part `part` (0-127) of the
    /// current multi (§0C/&32); ERROR 04 when no program has that index or no multi is current. A part outside
    /// 0-127 or an index outside 0-16383 is refused without sending. [RQ-AKM-092]
    void setMultiPartByIndex(Session& session, int part, int programIndex, CommandCompletion completion);

    /// Assigns the program named `name` to part `part` (0-127) of the current multi (§0C/&33); ERROR 04 when no
    /// program has that name or no multi is current. A part outside 0-127 is refused as `ArgumentOutOfRange` and
    /// a name that is not 7-bit ASCII as `NotEncodable`, without sending. [RQ-AKM-092]
    void setMultiPartByName(Session& session, int part, std::string_view name, CommandCompletion completion);

    /// Deletes the program of part `part` (0-127) of the current multi (§0C/&34); ERROR 04 when no multi is
    /// current. [RQ-AKM-092]
    void deleteMultiPart(Session& session, int part, CommandCompletion completion);

    // General information about the multis (§0C/&40-&48, &50-&52, Table 17). The Gets that return a name, a
    // list or a count that depends on the multi are refused as `ChecksumModeUnknown` while the port's checksum
    // mode is unknown: their REPLY has no fixed length to delimit it by (ADR-AKM-001, DEC-AKM-013,
    // DEC-AKM-014, DEC-AKM-015). The three "all the multis" Gets read ERROR 04 as an empty list, as §0A's
    // "all programs" Gets do (observed on the real S5000 for programs). [RQ-AKM-090, RQ-AKM-091, RQ-AKM-041]

    /// `count` is empty when the command did not complete on a REPLY of the length the catalogue gives it.
    /// [RQ-AKM-091]
    struct MultiCountResult
    {
        std::optional<int> count{};
        CommandResult outcome{};
    };
    using MultiCountCompletion = std::function<void(const MultiCountResult&)>;

    /// Gets the number of multis in memory (§0C/&40). [RQ-AKM-091]
    void getMultiCount(Session& session, MultiCountCompletion completion);

    /// `frontPanelNumber` is empty when the multi's program number is off (or the command did not complete on a
    /// REPLY of the length the catalogue gives it): 0-127 on the wire is 1-128 on the front panel.
    /// [RQ-AKM-091]
    struct MultiProgramNumberResult
    {
        std::optional<int> frontPanelNumber{};
        CommandResult outcome{};
    };
    using MultiProgramNumberCompletion = std::function<void(const MultiProgramNumberResult&)>;

    /// Gets the current multi's program number (§0C/&41), converted to the front-panel number. [RQ-AKM-091]
    void getMultiProgramNumber(Session& session, MultiProgramNumberCompletion completion);

    /// `partCount` is 32, 64 or 128 (the REPLY byte plus one); empty when the REPLY does not decode.
    /// [RQ-AKM-091]
    struct MultiPartCountResult
    {
        std::optional<int> partCount{};
        CommandResult outcome{};
    };
    using MultiPartCountCompletion = std::function<void(const MultiPartCountResult&)>;

    /// Gets the number of parts of the current multi (§0C/&44). [RQ-AKM-091]
    void getCurrentMultiPartCount(Session& session, MultiPartCountCompletion completion);

    /// Gets the name of part `part` (0-127) of the current multi (§0C/&45); the name is empty (not absent) when
    /// no program is assigned to the part, the REPLY being the single byte `00`. A part outside 0-127 is refused
    /// without sending. [RQ-AKM-091]
    void getMultiPartName(Session& session, int part, MultiNameCompletion completion);

    /// A list of names in the order the sampler sent them; empty (`nullopt`) when the command did not
    /// complete on a decodable REPLY. [RQ-AKM-091]
    struct MultiNameListResult
    {
        std::optional<std::vector<std::string>> names{};
        CommandResult outcome{};
    };
    using MultiNameListCompletion = std::function<void(const MultiNameListResult&)>;

    /// Gets the names of all the parts of the current multi (§0C/&46), 32, 64 or 128 of them, an empty name for a
    /// part with no program. [RQ-AKM-091]
    void getAllMultiPartNames(Session& session, MultiNameListCompletion completion);

    /// Gets the names of all the multis in memory (§0C/&51), in memory order. [RQ-AKM-091]
    void getAllMultiNames(Session& session, MultiNameListCompletion completion);

    /// A list of numbers; empty (`nullopt`) when the command did not complete on a decodable REPLY.
    /// [RQ-AKM-090, RQ-AKM-091]
    struct MultiValueListResult
    {
        std::optional<std::vector<int>> values{};
        CommandResult outcome{};
    };
    using MultiValueListCompletion = std::function<void(const MultiValueListResult&)>;

    /// Gets all the parameters of part `part` (0-127) of the current multi in one message (§0C/&47): twelve
    /// values, in the order of items &20-&2B (MIDI channel, mute, solo, level, output, pan/balance, effects
    /// channel, FX send level, fine tune, transpose, low note, high note). [RQ-AKM-090]
    void getAllMultiPartParameters(Session& session, int part, MultiValueListCompletion completion);

    /// Gets the mute and solo status of every part of the current multi (§0C/&48): one value per part, 0 = mute
    /// and solo off, 1 = mute on, 2 = solo on. [RQ-AKM-090]
    void getMultiMuteSoloStatus(Session& session, MultiValueListCompletion completion);

    /// Gets the number of parts of every multi in memory (§0C/&52), 32, 64 or 128 each, in memory order.
    /// [RQ-AKM-091]
    void getAllMultiPartCounts(Session& session, MultiValueListCompletion completion);

    /// One entry per multi, in memory order; an empty entry means that multi's program number is off.
    /// [RQ-AKM-091]
    struct MultiProgramNumbersResult
    {
        std::optional<std::vector<std::optional<int>>> numbers{};
        CommandResult outcome{};
    };
    using MultiProgramNumbersCompletion = std::function<void(const MultiProgramNumbersResult&)>;

    /// Gets the program numbers of every multi in memory (§0C/&50), converted to front-panel numbers like
    /// `getMultiProgramNumber`. [RQ-AKM-091]
    void getAllMultiProgramNumbers(Session& session, MultiProgramNumbersCompletion completion);
}
