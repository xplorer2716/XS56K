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
    // The sample lifecycle primitives of section 0E (spec Tables 18-19), on a session: thin typed
    // wrappers over the catalogue's records (ADR-AKM-001, DEC-AKM-003, DEC-AKM-012, DEC-AKM-013). Unlike
    // §06 (Keygroup Zone), §0E has its own sampler-wide "current sample" selection state, the same
    // pattern as §0A's current program (documents/_index/sysex_spec.kb.md line 91): every primitive here
    // acts on that current sample, set by `selectSampleByName`/`selectSampleByIndex`. Each returns at
    // once and reports on the session's thread, like `Session::submit`. The session must outlive every
    // call. [RQ-AKM-045]

    /// Selects the sample named `name` as current (§0E/&05); ERROR 04 when no sample has that name.
    /// [RQ-AKM-045]
    void selectSampleByName(Session& session, std::string_view name, CommandCompletion completion);

    /// Selects the sample at zero-based `index` as current (§0E/&06), the wire carrying it as two 7-bit
    /// data bytes; ERROR 04 when it names no sample. [RQ-AKM-045]
    void selectSampleByIndex(Session& session, int index, CommandCompletion completion);

    /// Deletes the current sample (§0E/&08); ERROR 04 when no sample is current. [RQ-AKM-045]
    void deleteCurrentSample(Session& session, CommandCompletion completion);

    /// Renames the current sample to `name` (§0E/&09); ERROR 04 when no sample is current. [RQ-AKM-045]
    void renameCurrentSample(Session& session, std::string_view name, CommandCompletion completion);

    /// Starts auditioning the current sample (§0E/&0A); ERROR 04 when no sample is current. [RQ-AKM-045]
    void startSampleAudition(Session& session, CommandCompletion completion);

    /// Stops auditioning the current sample (§0E/&0B); ERROR 04 when no sample is current. [RQ-AKM-045]
    void stopSampleAudition(Session& session, CommandCompletion completion);

    /// `index` is empty when no sample is current (or the command did not complete on a REPLY of the
    /// length the catalogue gives it); `outcome` is the result of the command. [RQ-AKM-045, RQ-AKM-047]
    struct SampleIndexResult
    {
        std::optional<int> index{};
        CommandResult outcome{};
    };
    using SampleIndexCompletion = std::function<void(const SampleIndexResult&)>;

    /// Gets the current sample's index, its position in memory (§0E/&13). [RQ-AKM-045, RQ-AKM-047]
    void getCurrentSampleIndex(Session& session, SampleIndexCompletion completion);

    /// `name` is empty when the command did not complete on a REPLY holding exactly one null-terminated
    /// name; `outcome` is the result of the command. [RQ-AKM-045, RQ-AKM-047]
    struct SampleNameResult
    {
        std::optional<std::string> name{};
        CommandResult outcome{};
    };
    using SampleNameCompletion = std::function<void(const SampleNameResult&)>;

    /// Gets the current sample's name (§0E/&14); refused as `ChecksumModeUnknown` while the port's
    /// checksum mode is unknown, since a String REPLY has no fixed length to delimit it by
    /// (ADR-AKM-001, DEC-AKM-013). [RQ-AKM-045, RQ-AKM-047, RQ-AKM-041]
    void getCurrentSampleName(Session& session, SampleNameCompletion completion);

    /// Passed to `deleteAllSamples` to prove the caller means it. A default `bool` could be satisfied by
    /// accident (`true`, `1`, a stray flag); this enumerator cannot — it must be named. [RQ-AKM-046]
    enum class ConfirmDeleteAllSamples
    {
        IUnderstandThisDeletesEverySampleInMemory,
    };

    /// Deletes every sample in memory (§0E/&07) — irreversible without a saved backup. `confirmation`
    /// has no default: sent only when it is the enumerator; `std::nullopt` refuses the command as
    /// `NotConfirmed` without sending anything. No real-sampler test of any feature calls this.
    /// [RQ-AKM-046]
    void deleteAllSamples(Session& session, std::optional<ConfirmDeleteAllSamples> confirmation,
                          CommandCompletion completion);

    // General information about the samples in memory (§0E/&10-&12). [RQ-AKM-047]

    /// `count` is empty when the command did not complete on a REPLY of the length the catalogue gives
    /// it; `outcome` is the result of the command. [RQ-AKM-047]
    struct SampleCountResult
    {
        std::optional<int> count{};
        CommandResult outcome{};
    };
    using SampleCountCompletion = std::function<void(const SampleCountResult&)>;

    /// Gets the number of samples in memory (§0E/&10). [RQ-AKM-047]
    void getSampleCount(Session& session, SampleCountCompletion completion);

    /// Gets the name of the sample at zero-based `index` (§0E/&11), without making it current; refused
    /// as `ChecksumModeUnknown` while the port's checksum mode is unknown, since a String REPLY has no
    /// fixed length to delimit it by (ADR-AKM-001, DEC-AKM-013). [RQ-AKM-047, RQ-AKM-041]
    void getSampleNameByIndex(Session& session, int index, SampleNameCompletion completion);

    /// One entry per sample in memory, in memory order. Empty when the command did not complete on a
    /// decodable REPLY. [RQ-AKM-047]
    struct AllSampleNamesResult
    {
        std::optional<std::vector<std::string>> names{};
        CommandResult outcome{};
    };
    using AllSampleNamesCompletion = std::function<void(const AllSampleNamesResult&)>;

    /// Gets the names of every sample in memory (§0E/&12). Refused as `ChecksumModeUnknown` while the
    /// port's checksum mode is unknown, the REPLY repeating a record (a name) once per sample with no
    /// fixed length either (ADR-AKM-001, DEC-AKM-014). [RQ-AKM-047, RQ-AKM-041]
    void getAllSampleNames(Session& session, AllSampleNamesCompletion completion);
}
