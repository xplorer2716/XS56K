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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace akm
{
    /// Reads the data bytes of a confirmation in the value formats of the spec (pp. 8-9), the reverse of
    /// ByteWriter. [RQ-AKM-002, ADR-AKM-001 (DEC-AKM-002)]
    ///
    /// Every read either returns a value and consumes its bytes, or returns nothing and consumes none:
    /// too few bytes, a byte above 7F, a sign byte other than 00 or 01, a string with no terminator. The
    /// reader views the bytes it is given, which must outlive it. Nothing is thrown.
    class ByteReader
    {
    public:
        explicit ByteReader(std::span<const std::uint8_t> bytes) : _remaining(bytes) {}

        [[nodiscard]] std::optional<std::uint8_t> readByte();
        [[nodiscard]] std::optional<std::uint16_t> readWord();
        [[nodiscard]] std::optional<std::uint32_t> readDword();
        [[nodiscard]] std::optional<std::uint64_t> readQword();

        /// Signed values: a sign byte, then the magnitude. A negative zero (sign 01, magnitude 0) reads as 0.
        [[nodiscard]] std::optional<std::int32_t> readSignedByte();
        [[nodiscard]] std::optional<std::int32_t> readSignedWord();
        [[nodiscard]] std::optional<std::int32_t> readSignedDword();

        /// ASCII text up to its terminating 00, which is consumed.
        [[nodiscard]] std::optional<std::string> readString();

        /// Every remaining byte as concatenated null-terminated strings; an empty remainder is an empty
        /// list, and a last string without its terminator refuses the whole list.
        [[nodiscard]] std::optional<std::vector<std::string>> readStringList();

        [[nodiscard]] std::size_t remaining() const { return _remaining.size(); }

    private:
        std::optional<std::uint64_t> readUnsigned(std::size_t width);
        std::optional<std::int32_t> readSigned(std::size_t width);

        std::span<const std::uint8_t> _remaining;
    };
}
