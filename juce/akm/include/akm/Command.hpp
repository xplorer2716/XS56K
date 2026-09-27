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
#include <vector>

#include "akm/Checksum.hpp"

namespace akm
{
    /// What is sent to the sampler apart from the addressing: a section, an item and its data bytes
    /// (spec p. 5, "A complete message"). [RQ-AKM-001]
    struct Command
    {
        std::uint8_t section = 0;
        std::uint8_t item = 0;
        std::vector<std::uint8_t> data;
    };

    /// Why a command cannot be encoded. [RQ-AKM-001]
    enum class EncodeError
    {
        InvalidDeviceId,      ///< above 31
        InvalidUserRefCount,  ///< not 1 to 4
        InvalidDataByte,      ///< a user-ref, section, item or data byte above 7F
    };

    /// The frame, or the reason it was refused (`bytes` is then empty).
    struct EncodeResult
    {
        std::vector<std::uint8_t> bytes;
        std::optional<EncodeError> error;

        [[nodiscard]] bool ok() const { return !error.has_value(); }
    };

    /// Encodes `F0 47 5E <dev> <user-ref...> <section> <item> <data...> [<checksum>] F7`. `<dev>` holds the
    /// DeviceID in bits 0-4 and the number of user-refs minus one in bits 5-6. A checksum is appended in
    /// mode On and Unknown, not in mode Off (ADR-AKM-001, DEC-AKM-009). A command that cannot be a legal
    /// frame produces no bytes. [RQ-AKM-001, RQ-AKM-003, RQ-AKM-041, ADR-AKM-001 (DEC-AKM-002)]
    [[nodiscard]] EncodeResult encodeCommand(std::uint32_t deviceId, std::span<const std::uint8_t> userRefs,
                                             const Command& command, ChecksumMode mode);
}
