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

#include <array>
#include <chrono>
#include <cstdint>
#include <string>

#include "akm/Checksum.hpp"
#include "akm/harness/ScenarioDriver.hpp"
#include "common/midi/MidiPorts.hpp"

namespace akm::harness
{
    /// Where a scenario talks to: the two port names of the sampler and the DeviceID to address.
    struct ScenarioTarget
    {
        std::string inputPortName;
        std::string outputPortName;
        std::uint32_t deviceId = 0;
    };

    struct EchoResult
    {
        bool answered = false;
        std::array<std::uint8_t, 4> echoed{};
        std::string failure;  ///< empty when answered
    };

    /// The plan's first hardware test: the Echo Message (§00 `&06`) sends four data bytes and the sampler
    /// returns them as a REPLY. The scenario opens the ports, sends the frame built by the codec and waits
    /// for the REPLY that carries its user-ref. It is written against `MidiBackend&` and a ScenarioDriver
    /// only, so it runs unchanged on the simulated sampler and on JuceMidiBackend (`compile_checks` builds
    /// it against the latter). With the mode Unknown, its default, it needs to know nothing of the
    /// sampler's checksum setting. [RQ-AKM-015, RQ-AKM-016, RQ-AKM-019, ADR-AKM-001 (DEC-AKM-008)]
    [[nodiscard]] EchoResult runEchoScenario(common::midi::MidiBackend& backend, ScenarioDriver& driver,
                                             const ScenarioTarget& target, const std::array<std::uint8_t, 4>& payload,
                                             ChecksumMode mode = ChecksumMode::Unknown,
                                             Scheduler::Clock::duration timeout = std::chrono::seconds(2));
}
