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
#include "akm/ByteWriter.hpp"

#include "akm/Protocol.hpp"

namespace akm
{
    bool ByteWriter::appendByte(std::uint32_t value)
    {
        return appendUnsigned(value, BYTE_WIDTH);
    }

    bool ByteWriter::appendWord(std::uint32_t value)
    {
        return appendUnsigned(value, WORD_WIDTH);
    }

    bool ByteWriter::appendDword(std::uint32_t value)
    {
        return appendUnsigned(value, DWORD_WIDTH);
    }

    bool ByteWriter::appendQword(std::uint64_t value)
    {
        return appendUnsigned(value, QWORD_WIDTH);
    }

    bool ByteWriter::appendSignedByte(std::int32_t value)
    {
        return appendSigned(value, BYTE_WIDTH);
    }

    bool ByteWriter::appendSignedWord(std::int32_t value)
    {
        return appendSigned(value, WORD_WIDTH);
    }

    bool ByteWriter::appendSignedDword(std::int32_t value)
    {
        return appendSigned(value, DWORD_WIDTH);
    }

    bool ByteWriter::appendString(std::string_view text)
    {
        if (!_ok)
            return false;
        for (const char character : text)
        {
            const auto code = static_cast<std::uint8_t>(character);
            if (code == STRING_TERMINATOR || !isDataByte(code))
                return fail();
        }
        for (const char character : text)
            _bytes.push_back(static_cast<std::uint8_t>(character));
        _bytes.push_back(STRING_TERMINATOR);
        return true;
    }

    bool ByteWriter::appendUnsigned(std::uint64_t value, std::size_t width)
    {
        if (!_ok)
            return false;
        if (value > maxUnsignedValue(width))
            return fail();
        // Most significant byte first, seven bits per byte.
        for (std::size_t index = width; index > 0; --index)
        {
            const unsigned int shift = BITS_PER_DATA_BYTE * static_cast<unsigned int>(index - 1);
            _bytes.push_back(static_cast<std::uint8_t>((value >> shift) & DATA_BYTE_MAX));
        }
        return true;
    }

    bool ByteWriter::appendSigned(std::int64_t value, std::size_t width)
    {
        if (!_ok)
            return false;
        const std::uint64_t magnitude = value < 0 ? static_cast<std::uint64_t>(-value) : static_cast<std::uint64_t>(value);
        // Checked before anything is written, so a refused value leaves no lone sign byte behind.
        if (magnitude > maxUnsignedValue(width))
            return fail();
        _bytes.push_back(value < 0 ? SIGN_NEGATIVE : SIGN_POSITIVE);
        return appendUnsigned(magnitude, width);
    }

    bool ByteWriter::fail()
    {
        _ok = false;
        return false;
    }
}
