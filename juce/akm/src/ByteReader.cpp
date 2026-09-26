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
#include "akm/ByteReader.hpp"

#include <utility>

#include "akm/Protocol.hpp"

namespace akm
{
    std::optional<std::uint8_t> ByteReader::readByte()
    {
        const auto value = readUnsigned(BYTE_WIDTH);
        if (!value)
            return std::nullopt;
        return static_cast<std::uint8_t>(*value);
    }

    std::optional<std::uint16_t> ByteReader::readWord()
    {
        const auto value = readUnsigned(WORD_WIDTH);
        if (!value)
            return std::nullopt;
        return static_cast<std::uint16_t>(*value);
    }

    std::optional<std::uint32_t> ByteReader::readDword()
    {
        const auto value = readUnsigned(DWORD_WIDTH);
        if (!value)
            return std::nullopt;
        return static_cast<std::uint32_t>(*value);
    }

    std::optional<std::uint64_t> ByteReader::readQword()
    {
        return readUnsigned(QWORD_WIDTH);
    }

    std::optional<std::int32_t> ByteReader::readSignedByte()
    {
        return readSigned(BYTE_WIDTH);
    }

    std::optional<std::int32_t> ByteReader::readSignedWord()
    {
        return readSigned(WORD_WIDTH);
    }

    std::optional<std::int32_t> ByteReader::readSignedDword()
    {
        return readSigned(DWORD_WIDTH);
    }

    std::optional<std::string> ByteReader::readString()
    {
        std::string text;
        for (std::size_t index = 0; index < _remaining.size(); ++index)
        {
            const std::uint8_t value = _remaining[index];
            if (!isDataByte(value))
                return std::nullopt;
            if (value == STRING_TERMINATOR)
            {
                _remaining = _remaining.subspan(index + 1);
                return text;
            }
            text.push_back(static_cast<char>(value));
        }
        return std::nullopt;
    }

    std::optional<std::vector<std::string>> ByteReader::readStringList()
    {
        const auto start = _remaining;
        std::vector<std::string> strings;
        while (!_remaining.empty())
        {
            auto text = readString();
            if (!text)
            {
                _remaining = start;
                return std::nullopt;
            }
            strings.push_back(std::move(*text));
        }
        return strings;
    }

    std::optional<std::uint64_t> ByteReader::readUnsigned(std::size_t width)
    {
        if (_remaining.size() < width)
            return std::nullopt;
        const auto field = _remaining.first(width);
        if (!allDataBytes(field))
            return std::nullopt;
        std::uint64_t value = 0;
        for (const std::uint8_t byte : field)
            value = (value << BITS_PER_DATA_BYTE) | byte;
        _remaining = _remaining.subspan(width);
        return value;
    }

    std::optional<std::int32_t> ByteReader::readSigned(std::size_t width)
    {
        if (_remaining.size() < SIGN_BYTE_WIDTH + width)
            return std::nullopt;
        const std::uint8_t sign = _remaining.front();
        if (sign != SIGN_POSITIVE && sign != SIGN_NEGATIVE)
            return std::nullopt;
        const auto start = _remaining;
        _remaining = _remaining.subspan(SIGN_BYTE_WIDTH);
        const auto magnitude = readUnsigned(width);
        if (!magnitude)
        {
            _remaining = start;
            return std::nullopt;
        }
        // At most 128^4 - 1 = 268435455 for a dword: it fits an int32.
        const auto signedMagnitude = static_cast<std::int32_t>(*magnitude);
        return sign == SIGN_NEGATIVE ? -signedMagnitude : signedMagnitude;
    }
}
