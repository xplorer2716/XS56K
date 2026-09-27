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
#include "akm/ItemRequest.hpp"

#include <utility>

#include "akm/ByteReader.hpp"
#include "akm/ByteWriter.hpp"

namespace akm
{
    namespace
    {
        // The value is inside the range the record gives it, which the data file keeps inside the
        // format's; the writer still refuses what does not fit, and its refusal is reported as out of range.
        bool appendValue(ByteWriter& writer, ValueFormat format, std::int64_t value)
        {
            switch (format)
            {
                case ValueFormat::Byte:
                    return writer.appendByte(static_cast<std::uint32_t>(value));
                case ValueFormat::Word:
                    return writer.appendWord(static_cast<std::uint32_t>(value));
                case ValueFormat::Dword:
                    return writer.appendDword(static_cast<std::uint32_t>(value));
                case ValueFormat::SignedByte:
                    return writer.appendSignedByte(static_cast<std::int32_t>(value));
                case ValueFormat::SignedWord:
                    return writer.appendSignedWord(static_cast<std::int32_t>(value));
                case ValueFormat::SignedDword:
                    return writer.appendSignedDword(static_cast<std::int32_t>(value));
                case ValueFormat::String:
                    // Not reachable through the int64_t path: a String argument goes through
                    // makeStringRequest instead (ADR-AKM-001, DEC-AKM-013).
                    return false;
            }
            return false;
        }

        template <typename Integer>
        std::optional<std::int64_t> widened(const std::optional<Integer>& value)
        {
            if (!value)
                return std::nullopt;
            return static_cast<std::int64_t>(*value);
        }

        std::optional<std::int64_t> readValue(ByteReader& reader, ValueFormat format)
        {
            switch (format)
            {
                case ValueFormat::Byte:
                    return widened(reader.readByte());
                case ValueFormat::Word:
                    return widened(reader.readWord());
                case ValueFormat::Dword:
                    return widened(reader.readDword());
                case ValueFormat::SignedByte:
                    return widened(reader.readSignedByte());
                case ValueFormat::SignedWord:
                    return widened(reader.readSignedWord());
                case ValueFormat::SignedDword:
                    return widened(reader.readSignedDword());
                case ValueFormat::String:
                    // Not reachable through the int64_t path: a String reply is read through
                    // decodeStringReply instead (ADR-AKM-001, DEC-AKM-013).
                    return std::nullopt;
            }
            return std::nullopt;
        }
    }

    CommandRequest makeRequest(const ItemDescriptor& item, std::span<const std::int64_t> values, CommandOptions options)
    {
        CommandRequest request;
        request.command.section = item.section;
        request.command.item = item.item;
        request.options = std::move(options);

        if (values.size() != item.args.size())
        {
            request.refusal = RefusalReason::WrongArgumentCount;
            return request;
        }

        ByteWriter writer;
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            const ValueSpec& spec = item.args[index];
            const std::int64_t value = values[index];
            if (value < spec.min || value > spec.max || !appendValue(writer, spec.format, value))
            {
                request.refusal = RefusalReason::ArgumentOutOfRange;
                return request;
            }
        }
        request.command.data = writer.bytes();
        return request;
    }

    CommandRequest makeRequest(ItemId id, std::span<const std::int64_t> values, CommandOptions options)
    {
        return makeRequest(descriptor(id), values, std::move(options));
    }

    CommandRequest makeRequest(ItemId id, std::initializer_list<std::int64_t> values, CommandOptions options)
    {
        return makeRequest(descriptor(id), std::span<const std::int64_t>(values.begin(), values.size()),
                           std::move(options));
    }

    std::optional<std::vector<std::int64_t>> decodeReply(const ItemDescriptor& item, std::span<const std::uint8_t> data)
    {
        ByteReader reader(data);
        std::vector<std::int64_t> values;
        values.reserve(item.reply.size());
        for (const ValueSpec& spec : item.reply)
        {
            const std::optional<std::int64_t> value = readValue(reader, spec.format);
            if (!value)
                return std::nullopt;
            values.push_back(*value);
        }
        if (reader.remaining() != 0)
            return std::nullopt;
        return values;
    }

    std::optional<std::vector<std::int64_t>> decodeReply(ItemId id, std::span<const std::uint8_t> data)
    {
        return decodeReply(descriptor(id), data);
    }

    namespace
    {
        // The one and only ValueSpec of a String item's single argument or reply, or null when the item
        // is not shaped that way.
        const ValueSpec* singleStringSpec(std::span<const ValueSpec> values)
        {
            if (values.size() != 1 || values.front().format != ValueFormat::String)
                return nullptr;
            return &values.front();
        }
    }

    CommandRequest makeStringRequest(const ItemDescriptor& item, std::string_view text, CommandOptions options)
    {
        CommandRequest request;
        request.command.section = item.section;
        request.command.item = item.item;
        request.options = std::move(options);

        const ValueSpec* spec = singleStringSpec(item.args);
        if (spec == nullptr)
        {
            request.refusal = RefusalReason::WrongArgumentCount;
            return request;
        }

        const auto length = static_cast<std::int64_t>(text.size());
        if (length < spec->min || length > spec->max)
        {
            request.refusal = RefusalReason::ArgumentOutOfRange;
            return request;
        }

        ByteWriter writer;
        if (!writer.appendString(text))
        {
            request.refusal = RefusalReason::NotEncodable;
            return request;
        }
        request.command.data = writer.bytes();
        return request;
    }

    CommandRequest makeStringRequest(ItemId id, std::string_view text, CommandOptions options)
    {
        return makeStringRequest(descriptor(id), text, std::move(options));
    }

    std::optional<std::string> decodeStringReply(const ItemDescriptor& item, std::span<const std::uint8_t> data)
    {
        if (singleStringSpec(item.reply) == nullptr)
            return std::nullopt;

        ByteReader reader(data);
        std::optional<std::string> text = reader.readString();
        if (!text || reader.remaining() != 0)
            return std::nullopt;
        return text;
    }

    std::optional<std::string> decodeStringReply(ItemId id, std::span<const std::uint8_t> data)
    {
        return decodeStringReply(descriptor(id), data);
    }
}
