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
#include <string_view>

namespace akm
{
    /// The error numbers of spec Table 3 (pp. 6-7), for the callers that react to one of them.
    /// An error number is Data1 * 128 + Data2 of an ERROR confirmation. [RQ-AKM-005]
    namespace error_number
    {
        inline constexpr std::uint16_t NOT_SUPPORTED = 0x00;
        inline constexpr std::uint16_t INVALID_FORMAT = 0x01;
        inline constexpr std::uint16_t OUT_OF_RANGE = 0x02;
        inline constexpr std::uint16_t UNKNOWN_ERROR = 0x03;
        inline constexpr std::uint16_t NOT_FOUND = 0x04;
        inline constexpr std::uint16_t COULD_NOT_CREATE = 0x05;
        inline constexpr std::uint16_t DELETION_FAILED = 0x06;
        inline constexpr std::uint16_t CHECKSUM_INVALID = 0x81;
        inline constexpr std::uint16_t DISK_SELECTED_DISK_INVALID = 0x101;
        inline constexpr std::uint16_t DISK_ERROR_DURING_LOAD = 0x102;
        inline constexpr std::uint16_t DISK_ITEM_NOT_FOUND = 0x103;
        inline constexpr std::uint16_t DISK_UNABLE_TO_CREATE = 0x104;
        inline constexpr std::uint16_t DISK_FOLDER_NOT_EMPTY = 0x105;
        inline constexpr std::uint16_t DISK_UNABLE_TO_DELETE = 0x106;
        inline constexpr std::uint16_t DISK_UNKNOWN_ERROR = 0x107;
        inline constexpr std::uint16_t DISK_ERROR_DURING_SAVE = 0x108;
        inline constexpr std::uint16_t DISK_INSUFFICIENT_SPACE = 0x109;
        inline constexpr std::uint16_t DISK_WRITE_PROTECTED = 0x10A;
        inline constexpr std::uint16_t DISK_NAME_NOT_UNIQUE = 0x10B;
        inline constexpr std::uint16_t DISK_INVALID_HANDLE = 0x10C;
        inline constexpr std::uint16_t DISK_EMPTY = 0x10D;
        inline constexpr std::uint16_t DISK_OPERATION_ABORTED = 0x10E;
        inline constexpr std::uint16_t DISK_FAILED_ON_OPEN = 0x10F;
        inline constexpr std::uint16_t DISK_READ_ERROR = 0x110;
        inline constexpr std::uint16_t DISK_NOT_READY = 0x111;
        inline constexpr std::uint16_t DISK_SCSI_ERROR = 0x112;
        inline constexpr std::uint16_t KEYGROUP_NOT_IN_PROGRAM = 0x181;
    }

    /// An error number with its meaning. A number that is not in Table 3 is kept as it was received.
    struct ErrorInfo
    {
        std::uint16_t number;
        bool known;
        std::string_view meaning;
    };

    /// The meaning of an error number from Table 3, or `known == false` and the meaning "unknown error
    /// number" for any other number. [RQ-AKM-005]
    [[nodiscard]] ErrorInfo describeError(std::uint16_t number);
}
