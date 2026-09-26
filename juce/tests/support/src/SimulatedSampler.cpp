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
#include "akm/testing/SimulatedSampler.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "akm/Checksum.hpp"
#include "akm/Protocol.hpp"
#include "akm/SamplerError.hpp"
#include "common/midi/MidiMessage.hpp"

namespace akm::testing
{
    namespace
    {
        using Bytes = std::vector<std::uint8_t>;

        // Reply IDs of spec Table 2.
        constexpr std::uint8_t REPLY_OK = 0x4F;
        constexpr std::uint8_t REPLY_DONE = 0x44;
        constexpr std::uint8_t REPLY_REPLY = 0x52;
        constexpr std::uint8_t REPLY_ERROR = 0x45;

        // Section §00 and its items, spec Table 5 (there is no item 02).
        constexpr std::uint8_t SECTION_SYSEX_CONFIG = 0x00;
        constexpr std::uint8_t ITEM_QUERY = 0x00;
        constexpr std::uint8_t ITEM_NOTIFICATION = 0x01;
        constexpr std::uint8_t ITEM_SYNC_LCD = 0x03;
        constexpr std::uint8_t ITEM_CHECKSUM = 0x04;
        constexpr std::uint8_t ITEM_AUTO_SCREEN_UPDATE = 0x05;
        constexpr std::uint8_t ITEM_ECHO = 0x06;
        constexpr std::uint8_t ITEM_STILL_ALIVE = 0x07;

        // OS versions that introduced an item (spec, modification history).
        constexpr OsVersion SYNC_LCD_SINCE{2, 0};
        constexpr OsVersion STILL_ALIVE_SINCE{2, 10};

        constexpr std::size_t ECHO_DATA_SIZE = 4;
        constexpr std::uint8_t TOGGLE_MAX = 1;
        constexpr std::size_t SECTION_AND_ITEM_SIZE = 2;
        constexpr std::size_t CHECKSUM_SIZE = 1;
        constexpr int MIN_REPLY_REPEAT = 1;
        constexpr std::uint8_t ERROR_NUMBER_MASK = 0x7F;

        const Bytes STILL_ALIVE_MESSAGE{common::midi::SYSEX_START, common::midi::SYSEX_END};

        // What executing a command answers: DONE, a REPLY with data, or an ERROR with its two bytes.
        struct Outcome
        {
            std::uint8_t replyId = REPLY_DONE;
            Bytes data;
        };

        Outcome done()
        {
            return Outcome{REPLY_DONE, {}};
        }

        Outcome reply(Bytes data)
        {
            return Outcome{REPLY_REPLY, std::move(data)};
        }

        Bytes errorData(std::uint16_t number)
        {
            return Bytes{static_cast<std::uint8_t>((number >> BITS_PER_DATA_BYTE) & ERROR_NUMBER_MASK),
                         static_cast<std::uint8_t>(number & ERROR_NUMBER_MASK)};
        }

        Outcome failure(std::uint16_t number)
        {
            return Outcome{REPLY_ERROR, errorData(number)};
        }

        // A toggle takes 0 or 1; nothing at all is an invalid message and another value is out of range.
        Outcome setToggle(const Bytes& data, bool& target)
        {
            if (data.empty())
                return failure(error_number::INVALID_FORMAT);
            if (data.front() > TOGGLE_MAX)
                return failure(error_number::OUT_OF_RANGE);
            target = data.front() == TOGGLE_MAX;
            return done();
        }

        // Only §00 is modelled. A byte after the data an item expects is ignored, as the spec says of a
        // checksum sent while checksums are off.
        Outcome execute(std::uint8_t section, std::uint8_t item, const Bytes& data, SamplerSettings& settings,
                        const OsVersion& osVersion)
        {
            if (section != SECTION_SYSEX_CONFIG)
                return failure(error_number::NOT_SUPPORTED);
            switch (item)
            {
                case ITEM_QUERY:
                    return done();
                case ITEM_NOTIFICATION:
                    return setToggle(data, settings.notification);
                case ITEM_SYNC_LCD:
                    if (osVersion < SYNC_LCD_SINCE)
                        return failure(error_number::NOT_SUPPORTED);
                    return setToggle(data, settings.syncLcd);
                case ITEM_CHECKSUM:
                    return setToggle(data, settings.checksum);
                case ITEM_AUTO_SCREEN_UPDATE:
                    return setToggle(data, settings.autoScreenUpdate);
                case ITEM_ECHO:
                    if (data.size() < ECHO_DATA_SIZE)
                        return failure(error_number::INVALID_FORMAT);
                    return reply(Bytes(data.begin(), data.begin() + ECHO_DATA_SIZE));
                case ITEM_STILL_ALIVE:
                    if (osVersion < STILL_ALIVE_SINCE)
                        return failure(error_number::NOT_SUPPORTED);
                    return setToggle(data, settings.stillAlive);
                default:
                    return failure(error_number::NOT_SUPPORTED);
            }
        }

        Bytes buildConfirmation(std::uint8_t deviceByte, const Bytes& userRefs, std::uint8_t replyId, std::uint8_t section,
                                std::uint8_t item, const Bytes& data, bool withChecksum)
        {
            Bytes frame{common::midi::SYSEX_START, AKAI_MANUFACTURER_ID, SAMPLER_MODEL_ID, deviceByte};
            frame.insert(frame.end(), userRefs.begin(), userRefs.end());
            frame.push_back(replyId);
            frame.push_back(section);
            frame.push_back(item);
            frame.insert(frame.end(), data.begin(), data.end());
            if (withChecksum)
                frame.push_back(checksum(std::span<const std::uint8_t>(frame).subspan(FIRST_USER_REF_INDEX)));
            frame.push_back(common::midi::SYSEX_END);
            return frame;
        }
    }

    SimulatedSampler::SimulatedSampler(SamplerConfig config, Scheduler& scheduler, Emit emit)
        : _config(config), _scheduler(scheduler), _emit(std::move(emit))
    {
    }

    void SimulatedSampler::setBehaviour(SamplerBehaviour behaviour)
    {
        const std::lock_guard lock(_mutex);
        _behaviour = std::move(behaviour);
    }

    SamplerBehaviour SimulatedSampler::behaviour() const
    {
        const std::lock_guard lock(_mutex);
        return _behaviour;
    }

    SamplerSettings SimulatedSampler::settings() const
    {
        const std::lock_guard lock(_mutex);
        return _settings;
    }

    void SimulatedSampler::powerCycle()
    {
        const std::lock_guard lock(_mutex);
        _settings = SamplerSettings{};
    }

    std::vector<std::vector<std::uint8_t>> SimulatedSampler::receivedFrames() const
    {
        const std::lock_guard lock(_mutex);
        return _received;
    }

    std::vector<AcceptedCommand> SimulatedSampler::acceptedCommands() const
    {
        const std::lock_guard lock(_mutex);
        return _accepted;
    }

    void SimulatedSampler::receive(std::span<const std::uint8_t> message)
    {
        std::vector<Bytes> confirmations;
        SamplerBehaviour behaviour;
        bool stillAlive = false;
        {
            const std::lock_guard lock(_mutex);
            _received.emplace_back(message.begin(), message.end());
            stillAlive = _settings.stillAlive;
            confirmations = process(message);
            behaviour = _behaviour;
        }
        if (confirmations.empty() || behaviour.silent)
            return;

        std::vector<Bytes> outgoing = behaviour.junkBeforeReply;
        for (const Bytes& confirmation : confirmations)
        {
            for (int repeat = 0; repeat < std::max(behaviour.timesEachReply, MIN_REPLY_REPEAT); ++repeat)
                outgoing.push_back(confirmation);
        }

        if (behaviour.replyDelay <= Scheduler::Clock::duration::zero())
        {
            for (Bytes& frame : outgoing)
                _emit(std::move(frame));
            return;
        }

        // A sampler with Still Alive on says so about every second while it works; the scheduled tasks
        // capture the emit function, not the sampler, so they are safe when the bus is gone.
        if (stillAlive && behaviour.stillAliveInterval > Scheduler::Clock::duration::zero())
        {
            for (auto at = behaviour.stillAliveInterval; at < behaviour.replyDelay; at += behaviour.stillAliveInterval)
                _scheduler.scheduleAfter(at, [emit = _emit] { emit(STILL_ALIVE_MESSAGE); });
        }
        _scheduler.scheduleAfter(behaviour.replyDelay, [emit = _emit, frames = std::move(outgoing)] {
            for (const Bytes& frame : frames)
                emit(frame);
        });
    }

    std::vector<std::vector<std::uint8_t>> SimulatedSampler::process(std::span<const std::uint8_t> message)
    {
        // A sampler ignores what is not F0 47 5E ... F7 made of data bytes and long enough to hold a section and an item.
        constexpr std::size_t minimumSize = FIRST_USER_REF_INDEX + USER_REF_COUNT_MIN + SECTION_AND_ITEM_SIZE + END_BYTE_SIZE;
        if (message.size() < minimumSize || message.front() != common::midi::SYSEX_START
            || message.back() != common::midi::SYSEX_END || message[MANUFACTURER_ID_INDEX] != AKAI_MANUFACTURER_ID
            || message[MODEL_ID_INDEX] != SAMPLER_MODEL_ID
            || !allDataBytes(message.subspan(START_BYTE_SIZE, message.size() - START_BYTE_SIZE - END_BYTE_SIZE)))
            return {};

        const std::uint8_t deviceByte = message[DEVICE_BYTE_INDEX];
        const std::uint8_t messageDeviceId = deviceByte & DEVICE_ID_MASK;
        const std::size_t userRefCount = ((deviceByte >> USER_REF_COUNT_SHIFT) & USER_REF_COUNT_MASK) + USER_REF_COUNT_MIN;
        const std::size_t sectionIndex = FIRST_USER_REF_INDEX + userRefCount;
        if (message.size() < sectionIndex + SECTION_AND_ITEM_SIZE + END_BYTE_SIZE)
            return {};

        // Spec p. 4: DeviceID 0 on either side matches; otherwise the two must be equal.
        if (_config.deviceId != 0 && messageDeviceId != 0 && messageDeviceId != _config.deviceId)
            return {};

        const Bytes userRefs(message.begin() + FIRST_USER_REF_INDEX, message.begin() + static_cast<std::ptrdiff_t>(sectionIndex));
        const std::uint8_t section = message[sectionIndex];
        const std::uint8_t item = message[sectionIndex + 1];
        const std::size_t endIndex = message.size() - END_BYTE_SIZE;
        Bytes data(message.begin() + static_cast<std::ptrdiff_t>(sectionIndex + SECTION_AND_ITEM_SIZE),
                   message.begin() + static_cast<std::ptrdiff_t>(endIndex));

        // Confirmations echo the count of user-refs, and carry the sampler's own DeviceID or the message's.
        const std::uint8_t replyDeviceId =
            _behaviour.confirmationDeviceId == ConfirmationDeviceId::Own ? _config.deviceId : messageDeviceId;
        const auto replyDeviceByte = static_cast<std::uint8_t>((deviceByte & ~DEVICE_ID_MASK) | replyDeviceId);
        const auto confirmation = [&](std::uint8_t replyId, const Bytes& replyData, bool withChecksum) {
            return buildConfirmation(replyDeviceByte, userRefs, replyId, section, item, replyData, withChecksum);
        };

        const SamplerSettings before = _settings;
        std::vector<Bytes> confirmations;

        if (before.checksum)
        {
            // The last byte before F7 is the checksum of what precedes it, from the first user-ref on.
            const bool valid =
                !data.empty()
                && data.back() == checksum(message.subspan(FIRST_USER_REF_INDEX, endIndex - CHECKSUM_SIZE - FIRST_USER_REF_INDEX));
            if (!valid)
            {
                confirmations.push_back(confirmation(REPLY_ERROR, errorData(error_number::CHECKSUM_INVALID), true));
                return confirmations;
            }
            data.pop_back();
        }

        _accepted.push_back(AcceptedCommand{messageDeviceId, userRefs, section, item, data});

        // OK goes out as soon as the message is accepted, before it runs, so it follows the previous settings.
        if (before.notification)
            confirmations.push_back(confirmation(REPLY_OK, {}, before.checksum));

        const Outcome outcome = execute(section, item, data, _settings, _config.osVersion);
        const bool resultChecksum = _behaviour.checksumChangeAppliesToOwnConfirmation ? _settings.checksum : before.checksum;
        confirmations.push_back(confirmation(outcome.replyId, outcome.data, resultChecksum));
        if (outcome.replyId == REPLY_REPLY && _behaviour.errorAfterReply)
            confirmations.push_back(confirmation(REPLY_ERROR, errorData(*_behaviour.errorAfterReply), resultChecksum));
        return confirmations;
    }
}
