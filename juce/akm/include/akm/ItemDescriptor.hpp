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
    /// The value formats of the spec (pp. 8-9) that an item's arguments and reply can use. `Qword` was
    /// added by DEC-AKM-017 for the first item whose REPLY is a Compound Quad Word (§10/&0B, Get Free
    /// Space, TASK-AKM-059): the codec already encoded and decoded it (`ByteWriter::appendQword`,
    /// `ByteReader::readQword`) before any item declared one, and it fits the generic `std::int64_t`
    /// path (`makeRequest`/`decodeReply`) without a dedicated pair of functions the way `String` needed,
    /// its 56-bit range being well inside `std::int64_t`. `String` was added by DEC-AKM-013 for the
    /// first items that carry an ASCII name (FTR-AKM-002): it has no fixed width and is encoded and
    /// decoded through `ByteWriter::appendString` / `ByteReader::readString` directly
    /// (`makeStringRequest`, `decodeStringReply`), not through the generic per-value `makeRequest` /
    /// `decodeReply`. [RQ-AKM-002]
    enum class ValueFormat
    {
        Byte,         ///< one data byte, 0 to 127
        Word,         ///< two bytes, most significant first, 0 to 16383
        Dword,        ///< four bytes, most significant first, 0 to 268435455
        Qword,        ///< eight bytes, most significant first, 0 to 72057594037927935
        SignedByte,   ///< a sign byte, then the magnitude in a byte
        SignedWord,   ///< a sign byte, then the magnitude in a word
        SignedDword,  ///< a sign byte, then the magnitude in a dword
        String,       ///< ASCII text terminated by 00; variable width, no numeric range
    };

    /// Returned for a format with no fixed width: either an enumerator added without implementing it,
    /// or (`String`) a value whose width is inherently variable.
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
            case ValueFormat::Qword:
                return QWORD_WIDTH;
            case ValueFormat::SignedByte:
                return SIGN_BYTE_WIDTH + BYTE_WIDTH;
            case ValueFormat::SignedWord:
                return SIGN_BYTE_WIDTH + WORD_WIDTH;
            case ValueFormat::SignedDword:
                return SIGN_BYTE_WIDTH + DWORD_WIDTH;
            case ValueFormat::String:
                return UNDEFINED_VALUE_WIDTH;
        }
        return UNDEFINED_VALUE_WIDTH;
    }

    /// One argument or reply value of an item: its name, its format and the range the spec gives it.
    /// Ranges are enforced on what is sent, not on what is received. For `String`, `min` and `max` are
    /// the allowed character count instead of a numeric range: the spec itself gives no bound, but real
    /// hardware may (Program names: 0-20, observed by the owner on an S5000, 2026-09-27 — see
    /// `documents/_index/sysex_spec.kb.md`, "Common value codes"). [RQ-AKM-001, RQ-AKM-014, ADR-AKM-001
    /// (DEC-AKM-013)]
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
        /// The section byte the item's REPLY carries when it is not the command's own: observed on the S5000
        /// (OS 2.14) for Get Clock Time & Date, whose REPLY says 0B where its OK says 02. Empty for every other item.
        /// A REPLY is accepted under either section, so a sampler that follows the spec is still understood.
        /// [RQ-AKM-059, ADR-AKM-001 (DEC-AKM-016)]
        std::optional<std::uint8_t> replySection{};

        /// The number of data bytes of the item's REPLY: the total width of its values, or nothing for an
        /// item that has no REPLY, or whose REPLY carries a `String` (its width is not fixed: a codec that
        /// does not know the checksum mode cannot tell where it ends, RQ-AKM-041, ADR-AKM-001 DEC-AKM-013).
        /// A codec that does not know the checksum mode reads this to tell a checksum from data. [RQ-AKM-041]
        [[nodiscard]] constexpr std::optional<std::size_t> fixedReplyLength() const
        {
            if (kind != ItemKind::Get)
                return std::nullopt;
            std::size_t total = 0;
            for (const ValueSpec& value : reply)
            {
                if (value.format == ValueFormat::String)
                    return std::nullopt;
                total += valueWidth(value.format);
            }
            return total;
        }
    };
}
