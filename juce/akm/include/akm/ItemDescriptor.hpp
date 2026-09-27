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
#include <string_view>

#include "akm/Protocol.hpp"

namespace akm
{
    /// The value formats of the spec (pp. 8-9) that an item's arguments and reply can use. Strings and
    /// qwords are added when the first item that needs one is catalogued (ADR-AKM-001, DEC-AKM-003).
    /// [RQ-AKM-002]
    enum class ValueFormat
    {
        Byte,         ///< one data byte, 0 to 127
        Word,         ///< two bytes, most significant first, 0 to 16383
        Dword,        ///< four bytes, most significant first, 0 to 268435455
        SignedByte,   ///< a sign byte, then the magnitude in a byte
        SignedWord,   ///< a sign byte, then the magnitude in a word
        SignedDword,  ///< a sign byte, then the magnitude in a dword
    };

    /// Returned for a format this build does not know: an enumerator added without its width.
    inline constexpr std::size_t UNDEFINED_VALUE_WIDTH = 0;

    /// The number of data bytes a value of `format` occupies. [RQ-AKM-002]
    [[nodiscard]] constexpr std::size_t valueWidth(ValueFormat format)
    {
        switch (format)
        {
            case ValueFormat::Byte:
                return BYTE_WIDTH;
            case ValueFormat::Word:
                return WORD_WIDTH;
            case ValueFormat::Dword:
                return DWORD_WIDTH;
            case ValueFormat::SignedByte:
                return SIGN_BYTE_WIDTH + BYTE_WIDTH;
            case ValueFormat::SignedWord:
                return SIGN_BYTE_WIDTH + WORD_WIDTH;
            case ValueFormat::SignedDword:
                return SIGN_BYTE_WIDTH + DWORD_WIDTH;
        }
        return UNDEFINED_VALUE_WIDTH;
    }

    /// One argument or reply value of an item: its name, its format and the range the spec gives it.
    /// Ranges are enforced on what is sent, not on what is received. [RQ-AKM-001, RQ-AKM-014]
    struct ValueSpec
    {
        std::string_view name;
        ValueFormat format;
        std::int64_t min;
        std::int64_t max;
    };

    /// How the sampler answers an item: a Set with DONE, a Get with a REPLY described by `reply`.
    enum class ItemKind
    {
        Set,
        Get,
    };

    /// One record of the catalogue: a spec item, described as data. The table of them is generated from
    /// `juce/akm/data/items.json` (ADR-AKM-001, DEC-AKM-003, DEC-AKM-012).
    struct ItemDescriptor
    {
        std::string_view name;
        std::uint8_t section;
        std::uint8_t item;
        ItemKind kind;
        std::span<const ValueSpec> args;
        std::span<const ValueSpec> reply;

        /// The number of data bytes of the item's REPLY: the total width of its values, or nothing for an
        /// item that has no REPLY. A codec that does not know the checksum mode reads it to tell a checksum
        /// from data. [RQ-AKM-041]
        [[nodiscard]] constexpr std::optional<std::size_t> fixedReplyLength() const
        {
            if (kind != ItemKind::Get)
                return std::nullopt;
            std::size_t total = 0;
            for (const ValueSpec& value : reply)
                total += valueWidth(value.format);
            return total;
        }
    };
}
