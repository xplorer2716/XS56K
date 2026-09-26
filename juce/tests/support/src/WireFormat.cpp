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
#include "akm/harness/WireFormat.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <variant>
#include <vector>

#include "akm/Confirmation.hpp"
#include "akm/SamplerError.hpp"

namespace akm::harness
{
    namespace
    {
        constexpr int SECONDS_DECIMALS = 3;
        constexpr int SECONDS_WIDTH = 9;
    }

    std::string hex(std::span<const std::uint8_t> bytes)
    {
        std::ostringstream text;
        text << std::hex << std::uppercase << std::setfill('0');
        for (std::size_t index = 0; index < bytes.size(); ++index)
            text << (index == 0 ? "" : " ") << std::setw(2) << static_cast<unsigned int>(bytes[index]);
        return text.str();
    }

    std::string hexOrDash(std::span<const std::uint8_t> bytes)
    {
        return bytes.empty() ? "-" : hex(bytes);
    }

    std::string secondsText(Scheduler::Clock::duration elapsed)
    {
        std::ostringstream text;
        text << std::fixed << std::setprecision(SECONDS_DECIMALS) << std::setw(SECONDS_WIDTH)
             << std::chrono::duration<double>(elapsed).count();
        return text.str();
    }

    long long millisecondsOf(Scheduler::Clock::duration duration)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

    std::string reading(std::span<const std::uint8_t> bytes, ChecksumMode mode)
    {
        const DecodedMessage decoded = decodeMessage(bytes, mode);
        if (std::holds_alternative<StillAliveMessage>(decoded))
            return "still alive (F0 F7)";
        if (const auto* rejected = std::get_if<Rejected>(&decoded))
            return "rejected: " + std::string(describe(rejected->reason));
        const Confirmation& confirmation = std::get<Confirmation>(decoded);
        std::ostringstream text;
        switch (confirmation.replyId)
        {
            case ReplyId::Ok:
                text << "OK";
                break;
            case ReplyId::Done:
                text << "DONE";
                break;
            case ReplyId::Reply:
                text << "REPLY";
                break;
            case ReplyId::Error:
            {
                const auto number = errorNumber(confirmation);
                if (number)
                    text << "ERROR " << *number << " (" << describeError(*number).meaning << ")";
                else
                    text << "ERROR (no error number)";
                break;
            }
        }
        text << " dev " << static_cast<unsigned int>(confirmation.deviceId) << " ref " << hex(confirmation.userRefs)
             << " sec " << hex(std::vector<std::uint8_t>{confirmation.section}) << " item "
             << hex(std::vector<std::uint8_t>{confirmation.item}) << " data " << hexOrDash(confirmation.data);
        return text.str();
    }
}
