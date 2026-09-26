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
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "akm/Checksum.hpp"

namespace akm
{
    /// The kind of a confirmation (spec Table 2, p. 5). The values are the Reply ID bytes on the wire.
    enum class ReplyId : std::uint8_t
    {
        Ok = 0x4F,     ///< 'O': valid SysEx received and being processed
        Done = 0x44,   ///< 'D': completed successfully
        Reply = 0x52,  ///< 'R': a variable number of data bytes is returned
        Error = 0x45,  ///< 'E': Data1 * 128 + Data2 is the error number
    };

    /// A decoded confirmation: `F0 47 5E <dev> <user-ref...> <reply ID> <section> <item> <data...>
    /// [<checksum>] F7`, the checksum stripped from `data`. [RQ-AKM-004]
    struct Confirmation
    {
        std::uint8_t deviceId = 0;               ///< bits 0-4 of <dev>
        std::vector<std::uint8_t> userRefs;      ///< 1 to 4, as sent in the command (bits 5-6 of <dev>)
        ReplyId replyId = ReplyId::Ok;
        std::uint8_t section = 0;
        std::uint8_t item = 0;
        std::vector<std::uint8_t> data;

        friend bool operator==(const Confirmation&, const Confirmation&) = default;
    };

    /// The null message F0 F7 that a sampler with the Still Alive monitor on sends about every second
    /// while a command is pending (spec Table 5, footnote b). It is not malformed. [RQ-AKM-006, RQ-AKM-011]
    struct StillAliveMessage
    {
        friend bool operator==(const StillAliveMessage&, const StillAliveMessage&) = default;
    };

    /// Why a received message was discarded. [RQ-AKM-006, RQ-AKM-041]
    enum class RejectReason
    {
        Foreign,            ///< not a message from an S5000/S6000 (not SysEx, or other manufacturer or model)
        Truncated,          ///< shorter than the minimum confirmation or than its own header announces
        InvalidDataByte,    ///< a byte above 7F between F0 and F7
        UnknownReplyId,     ///< a Reply ID other than 4F, 44, 52 and 45
        BadChecksum,        ///< checksum mode On and the checksum is missing or wrong; mode Unknown and the extra byte is not one
        UnknownDataLength,  ///< mode Unknown and a REPLY whose data length is not fixed
        BadDataLength,      ///< mode Unknown and a data length that is neither the expected one nor one more
    };

    struct Rejected
    {
        RejectReason reason;
    };

    using DecodedMessage = std::variant<Confirmation, StillAliveMessage, Rejected>;

    /// Decodes one complete received SysEx message, F0 and F7 included. It never throws: whatever is
    /// not a well-formed confirmation from an S5000/S6000 comes back as `Rejected` with the reason.
    ///
    /// The checksum mode decides where the data ends (DEC-AKM-009): On verifies and strips the last byte
    /// before F7, Off keeps every byte after the item as data, and Unknown decodes by the expected length
    /// of the data (OK and DONE none, ERROR two, the Echo REPLY four) and accepts one extra byte only if
    /// it is a valid checksum. The checksum covers the bytes from the first user-ref to the last data
    /// byte, the Reply ID included; that this is what the sampler sends has not been checked on hardware
    /// (RQ-AKM-017).
    /// [RQ-AKM-003, RQ-AKM-004, RQ-AKM-006, RQ-AKM-041, ADR-AKM-001 (DEC-AKM-002, DEC-AKM-009)]
    [[nodiscard]] DecodedMessage decodeMessage(std::span<const std::uint8_t> frame, ChecksumMode mode);

    /// A short text naming the reason, for the diagnostic that RQ-AKM-006 requires.
    [[nodiscard]] std::string_view describe(RejectReason reason);

    /// The error number of an ERROR confirmation, Data1 * 128 + Data2, or nothing when it is not an ERROR
    /// or carries fewer than two data bytes. A byte after the two (a checksum the mode did not
    /// anticipate) is ignored. [RQ-AKM-005]
    [[nodiscard]] std::optional<std::uint16_t> errorNumber(const Confirmation& confirmation);
}
