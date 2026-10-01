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

    /// The two models the protocol covers (spec Table 7, &04). [RQ-AKM-053]
    enum class SamplerModel
    {
        S5000,
        S6000,
    };

    /// `model` is empty when the command did not complete on a REPLY or its byte is neither `0` (S5000)
    /// nor `1` (S6000): what the sampler said is not guessed at, and `outcome` still holds the REPLY.
    /// [RQ-AKM-053]
    struct SamplerModelResult
    {
        std::optional<SamplerModel> model{};
        CommandResult outcome{};
    };
    using SamplerModelCompletion = std::function<void(const SamplerModelResult&)>;

    /// Gets the sampler's model (§02/&04). Answered whatever the checksum mode: the catalogue gives its
    /// REPLY a fixed length. [RQ-AKM-053]
    void getSamplerModel(Session& session, SamplerModelCompletion completion);

    /// `percent` (0-100) is empty when the command did not complete on a REPLY, or its value is above 100;
    /// `outcome` is the result of the command. [RQ-AKM-053]
    struct MemoryPercentResult
    {
        std::optional<int> percent{};
        CommandResult outcome{};
    };
    using MemoryPercentCompletion = std::function<void(const MemoryPercentResult&)>;

    /// Gets the percentage of free Wave memory (§02/&30). [RQ-AKM-053]
    void getFreeWaveMemoryPercent(Session& session, MemoryPercentCompletion completion);

    /// Gets the percentage of free MPKS (multis, programs, keygroups and samples) memory (§02/&31).
    /// [RQ-AKM-053]
    void getFreeMpksMemoryPercent(Session& session, MemoryPercentCompletion completion);

    /// `bytes` is empty when the command did not complete on a REPLY of the length the catalogue gives it;
    /// `outcome` is the result of the command. [RQ-AKM-053]
    struct MemoryBytesResult
    {
        std::optional<std::uint32_t> bytes{};
        CommandResult outcome{};
    };
    using MemoryBytesCompletion = std::function<void(const MemoryBytesResult&)>;

    /// Gets the total number of bytes of Wave memory installed (§02/&33), a compound double word
    /// (spec p. 9). [RQ-AKM-053]
    void getTotalWaveMemoryBytes(Session& session, MemoryBytesCompletion completion);

    /// Gets the number of bytes of free Wave memory (§02/&34), a compound double word. [RQ-AKM-053]
    void getFreeWaveMemoryBytes(Session& session, MemoryBytesCompletion completion);

    /// The sampler's clock and date (spec Table 6, &05 and &06). `dayOfWeek` is 1-7 with 1 = Sunday, and is
    /// the caller's to give: the sampler takes it as a field of its own. The year is the whole year, 1980-2079.
    /// [RQ-AKM-054]
    struct ClockDate
    {
        int year = 0;
        int month = 0;
        int day = 0;
        int dayOfWeek = 0;
        int hours = 0;
        int minutes = 0;
        int seconds = 0;

        friend bool operator==(const ClockDate&, const ClockDate&) = default;
    };

    /// The fields of a `ClockDate`, in the order of the item's arguments. [RQ-AKM-054]
    enum class ClockField
    {
        Year,
        Month,
        DayOfMonth,
        DayOfWeek,
        Hours,
        Minutes,
        Seconds,
    };

    /// The first field of `clock` outside the range the catalogue gives it, or nothing when all are inside:
    /// what `setClockDate` refuses a clock for, so that a caller can name the field. [RQ-AKM-054]
    [[nodiscard]] std::optional<ClockField> invalidClockField(const ClockDate& clock);

    /// The catalogue's name of a field ("year", "month", "dayOfMonth", "dayOfWeek", "hours", "minutes",
    /// "seconds"). [RQ-AKM-054]
    [[nodiscard]] std::string_view clockFieldName(ClockField field);

    /// Sets the sampler's clock and date (§02/&06); a field outside its range is refused without sending
    /// (`ArgumentOutOfRange`, the field being named by `invalidClockField`). [RQ-AKM-054]
    void setClockDate(Session& session, const ClockDate& clock, CommandCompletion completion);

    /// `clock` is empty when the command did not complete on a REPLY of the length the catalogue gives it;
    /// the sampler's values are reported as they are, ranges not enforced on a REPLY. `outcome` is the
    /// result of the command. [RQ-AKM-054]
    struct ClockDateResult
    {
        std::optional<ClockDate> clock{};
        CommandResult outcome{};
    };
    using ClockDateCompletion = std::function<void(const ClockDateResult&)>;

    /// Gets the sampler's clock and date (§02/&05). Answered whatever the checksum mode: the catalogue gives
    /// its REPLY a fixed length. [RQ-AKM-054]
    void getClockDate(Session& session, ClockDateCompletion completion);
}
