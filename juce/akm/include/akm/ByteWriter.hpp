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
#include <string_view>
#include <vector>

namespace akm
{
    /// Builds the data bytes of a command in the value formats of the spec (pp. 8-9): byte, word, dword,
    /// qword, signed values and null-terminated strings, all on 7-bit data bytes. [RQ-AKM-002,
    /// ADR-AKM-001 (DEC-AKM-002)]
    ///
    /// A value that does not fit its format is refused: nothing is appended, the call returns false and
    /// the writer stays failed (`ok()` is false, later calls append nothing), so a sequence of appends
    /// can be checked once at the end.
    class ByteWriter
    {
    public:
        bool appendByte(std::uint32_t value);
        bool appendWord(std::uint32_t value);
        bool appendDword(std::uint32_t value);
        bool appendQword(std::uint64_t value);

        /// Signed values: a sign byte (00 positive, 01 negative), then the magnitude in a byte, a word or
        /// a dword. Zero is always written positive.
        bool appendSignedByte(std::int32_t value);
        bool appendSignedWord(std::int32_t value);
        bool appendSignedDword(std::int32_t value);

        /// ASCII text followed by the terminating 00. Refused when a character is not 7-bit ASCII or is 00.
        bool appendString(std::string_view text);

        [[nodiscard]] bool ok() const { return _ok; }
        [[nodiscard]] const std::vector<std::uint8_t>& bytes() const& { return _bytes; }

    private:
        bool appendUnsigned(std::uint64_t value, std::size_t width);
        bool appendSigned(std::int64_t value, std::size_t width);
        bool fail();

        std::vector<std::uint8_t> _bytes;
        bool _ok = true;
    };
}
