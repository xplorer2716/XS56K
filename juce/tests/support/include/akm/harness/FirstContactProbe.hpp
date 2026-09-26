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

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <set>
#include <string>

#include "akm/Scheduler.hpp"
#include "akm/harness/EchoScenario.hpp"
#include "akm/harness/ScenarioDriver.hpp"
#include "common/midi/MidiPorts.hpp"

namespace akm::harness
{
    /// The operating-system version read from §02/&00 and §02/&01. [RQ-AKM-044]
    struct SamplerOsVersion
    {
        int major = 0;
        int minor = 0;
        std::optional<int> subVersion;  ///< empty when the sampler did not answer the sub-version request
    };

    struct ProbeOptions
    {
        ScenarioTarget target;
        /// A DeviceID other than the target's, for the step that shows how addressing works.
        std::uint32_t otherDeviceId = 5;
        /// How long each step waits for its DONE, REPLY or ERROR.
        Scheduler::Clock::duration answerTimeout = std::chrono::seconds(3);
        /// How long each step keeps listening after its confirmation, for what follows it (a second
        /// sampler, an ERROR after a REPLY).
        Scheduler::Clock::duration settleTime = std::chrono::milliseconds(500);
        /// Written in the log header when not empty (the probe itself reads no wall clock).
        std::string startedAt;
    };

    struct ProbeResult
    {
        bool portsOpened = false;
        bool anySamplerAnswered = false;
        std::size_t framesSent = 0;
        std::size_t framesReceived = 0;
        /// `F0 F7` messages that reached the host: the check that the backend delivers them.
        std::size_t stillAliveMessagesSeen = 0;
        std::optional<SamplerOsVersion> osVersion;
        /// The DeviceIDs (bits 0-4 of the device byte) that confirmations carried.
        std::set<std::uint8_t> confirmationDeviceIds;
        /// The longest time from a frame sent to its DONE, REPLY or ERROR.
        Scheduler::Clock::duration maxLatency = Scheduler::Clock::duration::zero();
    };

    /// The first contact with a sampler: fifteen frames sent one at a time through the codec, without the
    /// session's flow control, that fix what the session design rests on and that the specification leaves
    /// open — which DeviceID a confirmation carries, the shape of DONE and ERROR, whether confirmations
    /// carry a checksum while checksums are on, whether `F0 F7` is delivered, how long the sampler takes,
    /// which OS it runs. It also switches checksums and Still Alive on and off; it begins by putting
    /// checksums off on every sampler, with the spec's own frame, so as to start from a known state, and
    /// ends with both settings off.
    ///
    /// Each frame goes out with a user-ref of its own. The log gets one line per frame in each direction
    /// ("<seconds> OUT|IN <bytes>", the reading of a received frame after a bar, in each checksum mode),
    /// the latency of each step and a block of observations; lines starting with '#' are comments. It runs
    /// on the simulated sampler in CI and on JuceMidiBackend against the real one (`xs56k_akm_probe`).
    /// [RQ-AKM-017, RQ-AKM-018, RQ-AKM-044, ADR-AKM-001 (DEC-AKM-007, DEC-AKM-009)]
    ProbeResult runFirstContactProbe(common::midi::MidiBackend& backend, ScenarioDriver& driver,
                                     const ProbeOptions& options, std::ostream& log);
}
