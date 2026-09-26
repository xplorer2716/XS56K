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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "akm/Scheduler.hpp"
#include "akm/harness/SimulatedSampler.hpp"
#include "common/midi/MidiPorts.hpp"

namespace akm::harness
{
    /// On which thread, and when, a sampler's messages reach the host's input port.
    enum class DeliveryMode
    {
        /// Inside the host's `send()`, on the sending thread, before it returns: what MockMidiBackend does.
        OnSendingThread,
        /// From a thread of the simulation, at some point: possibly before `send()` returns, possibly after.
        OnOtherThread,
        /// From a thread of the simulation, and always before `send()` returns (it waits for the
        /// messages its frame caused): the case ADR-AKM-001 (DEC-AKM-005) is written for. The host must
        /// not send from a callback of its own input port in this mode.
        OnOtherThreadBeforeSendReturns,
    };

    /// A MIDI backend with one output port (host to samplers) and one input port (samplers to host), and
    /// the simulated samplers attached to the bus: every frame the host sends reaches every sampler, and
    /// what they emit reaches the host's input. It is the `MidiBackend` a scenario runs on in CI, in the
    /// place of JuceMidiBackend and the real S5000. [RQ-AKM-016, RQ-AKM-019, ADR-AKM-001 (DEC-AKM-008)]
    ///
    /// The scheduler it is given times the delayed replies and the Still Alive messages; give it the one the
    /// session under test uses, so that a manual scheduler moves both. Only SysEx is carried.
    class SimulatedMidiBackend final : public common::midi::MidiBackend
    {
    public:
        static constexpr const char* DEFAULT_INPUT_NAME = "Simulated S5000 In";
        static constexpr const char* DEFAULT_OUTPUT_NAME = "Simulated S5000 Out";

        explicit SimulatedMidiBackend(Scheduler& scheduler, std::string inputName = DEFAULT_INPUT_NAME,
                                      std::string outputName = DEFAULT_OUTPUT_NAME);
        ~SimulatedMidiBackend() override;

        SimulatedMidiBackend(const SimulatedMidiBackend&) = delete;
        SimulatedMidiBackend& operator=(const SimulatedMidiBackend&) = delete;

        /// Attaches a sampler to the bus; the reference stays valid as long as the backend does.
        SimulatedSampler& addSampler(SamplerConfig config = {});

        void setDeliveryMode(DeliveryMode mode);

        /// Puts a raw message on the host's input, as if a sampler had sent it: an unsolicited or late
        /// confirmation, a foreign message.
        void injectToHost(std::vector<std::uint8_t> bytes);

        /// Every message the host sent, and every message the samplers put on the bus, in order.
        [[nodiscard]] std::vector<common::midi::MidiMessage> sentByHost() const;
        [[nodiscard]] std::vector<common::midi::MidiMessage> emittedBySamplers() const;

        [[nodiscard]] const std::string& inputName() const { return _inputName; }
        [[nodiscard]] const std::string& outputName() const { return _outputName; }

        [[nodiscard]] std::vector<std::string> inputDeviceNames() const override;
        [[nodiscard]] std::vector<std::string> outputDeviceNames() const override;
        [[nodiscard]] std::unique_ptr<common::midi::MidiInputPort> openInput(const std::string& deviceName) override;
        [[nodiscard]] std::unique_ptr<common::midi::MidiOutputPort> openOutput(const std::string& deviceName) override;

        /// The state shared by the backend and its ports; opaque, named here only so that the ports can hold it.
        struct Bus;

    private:
        Scheduler& _scheduler;
        const std::string _inputName;
        const std::string _outputName;
        std::shared_ptr<Bus> _bus;
    };
}
