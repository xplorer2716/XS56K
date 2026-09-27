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

// Constants of the AKAI S5000/S6000 SysEx frame and value formats (spec pp. 3-9), shared by the codec.
// [RQ-AKM-001, RQ-AKM-002, RQ-AKM-004, ADR-AKM-001 (DEC-AKM-002)]
#include <cstddef>
#include <cstdint>
#include <span>

namespace akm
{
    // --- Frame: F0 47 5E <dev> <user-ref...> [<reply ID>] <section> <item> <data...> [<checksum>] F7 ---

    inline constexpr std::uint8_t AKAI_MANUFACTURER_ID = 0x47;
    inline constexpr std::uint8_t SAMPLER_MODEL_ID = 0x5E;

    /// F0 opens the message and F7 closes it, each one byte.
    inline constexpr std::size_t START_BYTE_SIZE = 1;
    inline constexpr std::size_t END_BYTE_SIZE = 1;
    inline constexpr std::size_t MANUFACTURER_ID_INDEX = 1;
    inline constexpr std::size_t MODEL_ID_INDEX = 2;
    /// Index of the <dev> byte, after F0, the manufacturer ID and the model ID.
    inline constexpr std::size_t DEVICE_BYTE_INDEX = 3;
    /// Index of the first user-ref, also where the checksum calculation starts (spec p. 4).
    inline constexpr std::size_t FIRST_USER_REF_INDEX = 4;

    /// <dev> holds the DeviceID in bits 0-4 and the number of user-refs minus one in bits 5-6 (Table 1).
    inline constexpr std::uint8_t DEVICE_ID_MASK = 0x1F;
    inline constexpr std::uint32_t DEVICE_ID_MAX = 31;
    inline constexpr unsigned int USER_REF_COUNT_SHIFT = 5;
    inline constexpr std::uint8_t USER_REF_COUNT_MASK = 0x03;
    inline constexpr std::size_t USER_REF_COUNT_MIN = 1;
    inline constexpr std::size_t USER_REF_COUNT_MAX = 4;

    // --- Data bytes and value formats (pp. 8-9) ---

    /// Every byte between the model ID and F7 is a MIDI data byte.
    inline constexpr std::uint8_t DATA_BYTE_MAX = 0x7F;
    inline constexpr unsigned int BITS_PER_DATA_BYTE = 7;

    /// Number of data bytes of each unsigned width; the value is base 128, most significant byte first.
    inline constexpr std::size_t BYTE_WIDTH = 1;
    inline constexpr std::size_t WORD_WIDTH = 2;
    inline constexpr std::size_t DWORD_WIDTH = 4;
    inline constexpr std::size_t QWORD_WIDTH = 8;

    /// A signed value is a sign byte followed by its magnitude.
    inline constexpr std::size_t SIGN_BYTE_WIDTH = 1;
    inline constexpr std::uint8_t SIGN_POSITIVE = 0;
    inline constexpr std::uint8_t SIGN_NEGATIVE = 1;

    inline constexpr std::uint8_t STRING_TERMINATOR = 0;

    /// Largest value that fits in `width` data bytes: 128^width - 1.
    constexpr std::uint64_t maxUnsignedValue(std::size_t width)
    {
        return (std::uint64_t{1} << (BITS_PER_DATA_BYTE * width)) - 1;
    }

    constexpr bool isDataByte(std::uint8_t value)
    {
        return value <= DATA_BYTE_MAX;
    }

    constexpr bool allDataBytes(std::span<const std::uint8_t> values)
    {
        for (const std::uint8_t value : values)
        {
            if (!isDataByte(value))
                return false;
        }
        return true;
    }
}
