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
#include "akm/testing/EchoScenario.hpp"

#include <algorithm>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "akm/Command.hpp"
#include "akm/Confirmation.hpp"

namespace akm::testing
{
    namespace
    {
        // The Echo Message: §00, item 06, four data bytes (spec Table 5).
        constexpr std::uint8_t SECTION_SYSEX_CONFIG = 0x00;
        constexpr std::uint8_t ITEM_ECHO = 0x06;
        constexpr std::uint8_t USER_REF = 0x2A;

        EchoResult failed(std::string reason)
        {
            EchoResult result;
            result.failure = std::move(reason);
            return result;
        }

        // What the input port received, from whichever thread it calls back on.
        class Inbox
        {
        public:
            void add(std::vector<std::uint8_t> frame)
            {
                const std::lock_guard lock(_mutex);
                _frames.push_back(std::move(frame));
            }

            [[nodiscard]] std::vector<std::vector<std::uint8_t>> frames() const
            {
                const std::lock_guard lock(_mutex);
                return _frames;
            }

        private:
            mutable std::mutex _mutex;
            std::vector<std::vector<std::uint8_t>> _frames;
        };
    }

    EchoResult runEchoScenario(common::midi::MidiBackend& backend, ScenarioDriver& driver, const ScenarioTarget& target,
                               const std::array<std::uint8_t, 4>& payload, ChecksumMode mode,
                               Scheduler::Clock::duration timeout)
    {
        auto input = backend.openInput(target.inputPortName);
        if (!input)
            return failed("input port not found: " + target.inputPortName);
        auto output = backend.openOutput(target.outputPortName);
        if (!output)
            return failed("output port not found: " + target.outputPortName);

        const Command echo{SECTION_SYSEX_CONFIG, ITEM_ECHO, std::vector<std::uint8_t>(payload.begin(), payload.end())};
        const std::vector<std::uint8_t> userRefs{USER_REF};
        const EncodeResult frame = encodeCommand(target.deviceId, userRefs, echo, mode);
        if (!frame.ok())
            return failed("the Echo command cannot be encoded for DeviceID " + std::to_string(target.deviceId));

        Inbox inbox;
        common::midi::MidiInputCallbacks callbacks;
        callbacks.onSysExMessage = [&inbox](const common::midi::MidiMessage& message) { inbox.add(message.toBytes()); };
        input->setCallbacks(std::move(callbacks));
        input->start();

        output->send(common::midi::MidiMessage::sysEx(frame.bytes));

        // The first REPLY, or ERROR, that carries this command's user-ref ends the wait.
        std::optional<Confirmation> answer;
        const bool arrived = driver.waitUntil(
            [&] {
                for (const auto& bytes : inbox.frames())
                {
                    const DecodedMessage decoded = decodeMessage(bytes, mode);
                    const auto* confirmation = std::get_if<Confirmation>(&decoded);
                    if (confirmation && confirmation->userRefs == userRefs
                        && (confirmation->replyId == ReplyId::Reply || confirmation->replyId == ReplyId::Error))
                    {
                        answer = *confirmation;
                        return true;
                    }
                }
                return false;
            },
            timeout);
        // No callback may run once the inbox is gone.
        input->stop();

        if (!arrived || !answer)
            return failed("no answer to the Echo command within the timeout");
        if (answer->replyId == ReplyId::Error)
        {
            const std::optional<std::uint16_t> number = errorNumber(*answer);
            return failed("the Echo command was answered by an ERROR" + (number ? " " + std::to_string(*number) : std::string{}));
        }
        if (answer->data.size() != payload.size())
            return failed("the Echo REPLY carries " + std::to_string(answer->data.size()) + " data bytes instead of 4");

        EchoResult result;
        result.answered = true;
        std::copy(answer->data.begin(), answer->data.end(), result.echoed.begin());
        return result;
    }
}
