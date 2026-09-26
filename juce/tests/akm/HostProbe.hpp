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

// The host's side of a MIDI backend, for the tests of the simulated sampler: it opens the two ports,
// sends frames and records what arrives, with the thread it arrived on. [TASK-AKM-007, RQ-AKM-016]
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "TestBytes.hpp"
#include "akm/Confirmation.hpp"
#include "common/midi/MidiPorts.hpp"

namespace akm::test
{
    struct Arrival
    {
        Bytes bytes;
        std::thread::id thread;
        bool sendHadReturned = false;  ///< whether the host's send() had returned when this arrived
    };

    class HostProbe
    {
    public:
        HostProbe(common::midi::MidiBackend& backend, const std::string& inputName, const std::string& outputName)
            : _input(backend.openInput(inputName)), _output(backend.openOutput(outputName))
        {
            common::midi::MidiInputCallbacks callbacks;
            callbacks.onSysExMessage = [this](const common::midi::MidiMessage& message) { record(message); };
            _input->setCallbacks(std::move(callbacks));
            _input->start();
        }

        ~HostProbe() { _input->stop(); }

        HostProbe(const HostProbe&) = delete;
        HostProbe& operator=(const HostProbe&) = delete;

        /// Sends a complete SysEx frame.
        void send(const Bytes& frame)
        {
            _sendHasReturned = false;
            _output->send(common::midi::MidiMessage::sysEx(frame));
            _sendHasReturned = true;
        }

        [[nodiscard]] std::vector<Arrival> arrivals() const
        {
            const std::lock_guard lock(_mutex);
            return _arrivals;
        }

        [[nodiscard]] std::size_t arrivalCount() const
        {
            const std::lock_guard lock(_mutex);
            return _arrivals.size();
        }

        /// What has arrived so far, decoded in the given checksum mode.
        [[nodiscard]] std::vector<DecodedMessage> decoded(ChecksumMode mode) const
        {
            std::vector<DecodedMessage> result;
            for (const Arrival& arrival : arrivals())
                result.push_back(decodeMessage(arrival.bytes, mode));
            return result;
        }

        /// The Reply ID bytes of what has arrived (4F, 44, 52, 45), or 00 for anything that is not a
        /// confirmation of an S5000.
        [[nodiscard]] Bytes replyIds(ChecksumMode mode) const
        {
            Bytes ids;
            for (const DecodedMessage& message : decoded(mode))
            {
                const auto* confirmation = std::get_if<Confirmation>(&message);
                ids.push_back(confirmation ? static_cast<std::uint8_t>(confirmation->replyId) : std::uint8_t{0});
            }
            return ids;
        }

        void clear()
        {
            const std::lock_guard lock(_mutex);
            _arrivals.clear();
        }

    private:
        void record(const common::midi::MidiMessage& message)
        {
            const std::lock_guard lock(_mutex);
            _arrivals.push_back(Arrival{message.toBytes(), std::this_thread::get_id(), _sendHasReturned.load()});
        }

        std::unique_ptr<common::midi::MidiInputPort> _input;
        std::unique_ptr<common::midi::MidiOutputPort> _output;
        mutable std::mutex _mutex;
        std::vector<Arrival> _arrivals;
        std::atomic<bool> _sendHasReturned{true};
    };
}
