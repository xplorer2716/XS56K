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
#include "akm/Command.hpp"

#include <utility>

#include "akm/Protocol.hpp"
#include "common/midi/MidiMessage.hpp"

namespace akm
{
    namespace
    {
        EncodeResult refused(EncodeError error)
        {
            return EncodeResult{{}, error};
        }
    }

    EncodeResult encodeCommand(std::uint32_t deviceId, std::span<const std::uint8_t> userRefs,
                               const Command& command, ChecksumMode mode)
    {
        if (deviceId > DEVICE_ID_MAX)
            return refused(EncodeError::InvalidDeviceId);
        if (userRefs.size() < USER_REF_COUNT_MIN || userRefs.size() > USER_REF_COUNT_MAX)
            return refused(EncodeError::InvalidUserRefCount);
        if (!allDataBytes(userRefs) || !isDataByte(command.section) || !isDataByte(command.item)
            || !allDataBytes(command.data))
            return refused(EncodeError::InvalidDataByte);

        const auto userRefCountBits = static_cast<std::uint32_t>(userRefs.size() - USER_REF_COUNT_MIN);

        std::vector<std::uint8_t> frame;
        frame.push_back(common::midi::SYSEX_START);
        frame.push_back(AKAI_MANUFACTURER_ID);
        frame.push_back(SAMPLER_MODEL_ID);
        frame.push_back(static_cast<std::uint8_t>(deviceId | (userRefCountBits << USER_REF_COUNT_SHIFT)));
        frame.insert(frame.end(), userRefs.begin(), userRefs.end());
        frame.push_back(command.section);
        frame.push_back(command.item);
        frame.insert(frame.end(), command.data.begin(), command.data.end());
        if (mode != ChecksumMode::Off)
            frame.push_back(checksum(std::span<const std::uint8_t>(frame).subspan(FIRST_USER_REF_INDEX)));
        frame.push_back(common::midi::SYSEX_END);
        return EncodeResult{std::move(frame), std::nullopt};
    }
}
