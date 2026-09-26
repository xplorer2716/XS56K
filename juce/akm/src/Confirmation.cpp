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
#include "akm/Confirmation.hpp"

#include <cstddef>

#include "akm/Protocol.hpp"
#include "common/midi/MidiMessage.hpp"

namespace akm
{
    namespace
    {
        // F0 47 5E <dev>, then the user-refs; after them <reply ID> <section> <item>; F7 last.
        constexpr std::size_t PREFIX_SIZE = FIRST_USER_REF_INDEX;
        constexpr std::size_t FIELDS_AFTER_USER_REFS = 3;
        constexpr std::size_t TRAILER_SIZE = END_BYTE_SIZE;
        constexpr std::size_t MIN_FRAME_SIZE = PREFIX_SIZE + USER_REF_COUNT_MIN + FIELDS_AFTER_USER_REFS + TRAILER_SIZE;
        constexpr std::size_t STILL_ALIVE_SIZE = START_BYTE_SIZE + END_BYTE_SIZE;
        constexpr std::size_t CHECKSUM_SIZE = 1;

        // Bytes a checksum-mode-Unknown decoder expects after the item, by kind of confirmation (DEC-AKM-009).
        constexpr std::size_t NO_DATA_LENGTH = 0;
        constexpr std::size_t ERROR_DATA_LENGTH = 2;
        constexpr std::size_t ECHO_DATA_LENGTH = 4;
        // The Echo Message, §00 item 06, is the only REPLY of fixed length that a session in mode Unknown sends.
        constexpr std::uint8_t ECHO_SECTION = 0x00;
        constexpr std::uint8_t ECHO_ITEM = 0x06;

        constexpr std::string_view UNKNOWN_REASON_TEXT = "unknown rejection reason";

        bool isStillAlive(std::span<const std::uint8_t> frame)
        {
            return frame.size() == STILL_ALIVE_SIZE && frame.front() == common::midi::SYSEX_START
                   && frame.back() == common::midi::SYSEX_END;
        }

        std::optional<ReplyId> replyIdOf(std::uint8_t byte)
        {
            switch (static_cast<ReplyId>(byte))
            {
                case ReplyId::Ok:
                case ReplyId::Done:
                case ReplyId::Reply:
                case ReplyId::Error:
                    return static_cast<ReplyId>(byte);
            }
            return std::nullopt;
        }

        std::optional<std::size_t> expectedDataLength(ReplyId replyId, std::uint8_t section, std::uint8_t item)
        {
            switch (replyId)
            {
                case ReplyId::Ok:
                case ReplyId::Done:
                    return NO_DATA_LENGTH;
                case ReplyId::Error:
                    return ERROR_DATA_LENGTH;
                case ReplyId::Reply:
                    if (section == ECHO_SECTION && item == ECHO_ITEM)
                        return ECHO_DATA_LENGTH;
                    return std::nullopt;
            }
            return std::nullopt;
        }

        // Where the data ends in `payload` (the bytes between the item and F7), by checksum mode; or why
        // the message is refused. `checksumSpan` is what a checksum would cover.
        struct DataExtent
        {
            std::size_t length = 0;
            std::optional<RejectReason> rejection;
        };

        DataExtent dataExtent(ChecksumMode mode, ReplyId replyId, std::uint8_t section, std::uint8_t item,
                              std::span<const std::uint8_t> payload, std::span<const std::uint8_t> checksumSpan)
        {
            const bool hasChecksumByte = !payload.empty();
            const bool checksumIsValid = hasChecksumByte && payload.back() == checksum(checksumSpan);

            switch (mode)
            {
                case ChecksumMode::Off:
                    return {payload.size(), std::nullopt};
                case ChecksumMode::On:
                    if (!checksumIsValid)
                        return {0, RejectReason::BadChecksum};
                    return {payload.size() - CHECKSUM_SIZE, std::nullopt};
                case ChecksumMode::Unknown:
                {
                    const auto expected = expectedDataLength(replyId, section, item);
                    if (!expected)
                        return {0, RejectReason::UnknownDataLength};
                    if (payload.size() == *expected)
                        return {*expected, std::nullopt};
                    if (payload.size() == *expected + CHECKSUM_SIZE)
                    {
                        if (!checksumIsValid)
                            return {0, RejectReason::BadChecksum};
                        return {*expected, std::nullopt};
                    }
                    return {0, RejectReason::BadDataLength};
                }
            }
            return {0, RejectReason::BadChecksum};
        }
    }

    DecodedMessage decodeMessage(std::span<const std::uint8_t> frame, ChecksumMode mode)
    {
        if (isStillAlive(frame))
            return StillAliveMessage{};
        if (frame.empty())
            return Rejected{RejectReason::Truncated};

        // Foreign first: a message that is not ours is not "truncated", whatever its length.
        if (frame.front() != common::midi::SYSEX_START)
            return Rejected{RejectReason::Foreign};
        if (frame.size() > MANUFACTURER_ID_INDEX && frame[MANUFACTURER_ID_INDEX] != AKAI_MANUFACTURER_ID)
            return Rejected{RejectReason::Foreign};
        if (frame.size() > MODEL_ID_INDEX && frame[MODEL_ID_INDEX] != SAMPLER_MODEL_ID)
            return Rejected{RejectReason::Foreign};

        if (frame.size() < MIN_FRAME_SIZE || frame.back() != common::midi::SYSEX_END)
            return Rejected{RejectReason::Truncated};
        if (!allDataBytes(frame.subspan(START_BYTE_SIZE, frame.size() - START_BYTE_SIZE - END_BYTE_SIZE)))
            return Rejected{RejectReason::InvalidDataByte};

        const std::uint8_t deviceByte = frame[DEVICE_BYTE_INDEX];
        const std::size_t userRefCount = ((deviceByte >> USER_REF_COUNT_SHIFT) & USER_REF_COUNT_MASK) + USER_REF_COUNT_MIN;
        if (frame.size() < PREFIX_SIZE + userRefCount + FIELDS_AFTER_USER_REFS + TRAILER_SIZE)
            return Rejected{RejectReason::Truncated};

        const std::size_t replyIndex = FIRST_USER_REF_INDEX + userRefCount;
        const auto replyId = replyIdOf(frame[replyIndex]);
        if (!replyId)
            return Rejected{RejectReason::UnknownReplyId};

        const std::uint8_t section = frame[replyIndex + 1];
        const std::uint8_t item = frame[replyIndex + 2];
        const std::size_t payloadStart = replyIndex + FIELDS_AFTER_USER_REFS;
        const std::size_t endIndex = frame.size() - TRAILER_SIZE;
        const auto payload = frame.subspan(payloadStart, endIndex - payloadStart);
        // A checksum, if there is one, is the last payload byte: it covers what precedes it.
        const auto checksumSpan = frame.subspan(FIRST_USER_REF_INDEX, endIndex - CHECKSUM_SIZE - FIRST_USER_REF_INDEX);

        const DataExtent extent = dataExtent(mode, *replyId, section, item, payload, checksumSpan);
        if (extent.rejection)
            return Rejected{*extent.rejection};

        Confirmation confirmation;
        confirmation.deviceId = deviceByte & DEVICE_ID_MASK;
        confirmation.userRefs.assign(frame.begin() + FIRST_USER_REF_INDEX, frame.begin() + replyIndex);
        confirmation.replyId = *replyId;
        confirmation.section = section;
        confirmation.item = item;
        confirmation.data.assign(payload.begin(), payload.begin() + extent.length);
        return confirmation;
    }

    std::string_view describe(RejectReason reason)
    {
        switch (reason)
        {
            case RejectReason::Foreign:
                return "foreign message: not from an S5000/S6000";
            case RejectReason::Truncated:
                return "truncated message: shorter than a confirmation or than its own header announces";
            case RejectReason::InvalidDataByte:
                return "invalid data byte: a byte above 7F between F0 and F7";
            case RejectReason::UnknownReplyId:
                return "unknown reply ID: not OK, DONE, REPLY or ERROR";
            case RejectReason::BadChecksum:
                return "checksum missing or wrong";
            case RejectReason::UnknownDataLength:
                return "checksum mode unknown: the data length of this reply is not fixed";
            case RejectReason::BadDataLength:
                return "unexpected data length while the checksum mode is unknown";
        }
        return UNKNOWN_REASON_TEXT;
    }

    std::optional<std::uint16_t> errorNumber(const Confirmation& confirmation)
    {
        constexpr std::size_t ERROR_NUMBER_BYTES = 2;
        constexpr std::size_t DATA1_INDEX = 0;
        constexpr std::size_t DATA2_INDEX = 1;
        if (confirmation.replyId != ReplyId::Error || confirmation.data.size() < ERROR_NUMBER_BYTES)
            return std::nullopt;
        // Data1 * 128 + Data2, both 7-bit: at most 16383.
        return static_cast<std::uint16_t>((confirmation.data[DATA1_INDEX] << BITS_PER_DATA_BYTE)
                                          | confirmation.data[DATA2_INDEX]);
    }
}
