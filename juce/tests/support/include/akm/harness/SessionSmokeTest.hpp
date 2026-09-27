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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "akm/Checksum.hpp"
#include "akm/Scheduler.hpp"
#include "akm/Session.hpp"
#include "akm/SystemVersion.hpp"
#include "akm/harness/EchoScenario.hpp"
#include "akm/harness/ScenarioDriver.hpp"
#include "common/midi/MidiPorts.hpp"

namespace akm::harness
{
    /// Round trips of the Echo timed in a run: what RQ-AKM-017 asks of the latency measurement.
    inline constexpr int DEFAULT_SMOKE_ECHO_ROUND_TRIPS = ECHO_LATENCY_ROUND_TRIPS;

    struct SessionSmokeOptions
    {
        ScenarioTarget target;
        /// How long the session waits for the answer to each command.
        Scheduler::Clock::duration stepTimeout = std::chrono::seconds(3);
        Scheduler::Clock::duration discoveryWindow = DEFAULT_DISCOVERY_WINDOW;
        /// Echo round trips timed one after the other; none skips them.
        int echoRepeats = DEFAULT_SMOKE_ECHO_ROUND_TRIPS;
        /// Whether Sync LCD and Auto screen update are switched too. They change what the LCD does, and the
        /// sampler cannot be asked what they were, so a run that must not touch them leaves them alone.
        bool touchLcdSettings = true;
        /// Written in the log header when not empty (the scenario itself reads no wall clock).
        std::string startedAt;
    };

    struct SessionSmokeResult
    {
        bool portsOpened = false;
        /// The DeviceIDs that answered the discovery, and whether the target's was among them.
        std::vector<std::uint8_t> discoveredDeviceIds;
        bool samplerFound = false;
        std::optional<OsVersionReport> osVersion;
        /// Echo round trips that came back right, and the time each took from submit to completion.
        std::size_t echoRoundTrips = 0;
        std::vector<Scheduler::Clock::duration> echoLatencies;
        /// Titles of the steps that did not complete as they had to; empty when the run went as expected.
        std::vector<std::string> failedSteps;
        std::size_t framesSent = 0;
        std::size_t framesReceived = 0;
        /// `F0 F7` messages that reached the host: the check that the backend delivers them.
        std::size_t stillAliveMessagesSeen = 0;
        std::size_t rejectedMessages = 0;
        std::size_t unsolicitedConfirmations = 0;
        std::size_t lateErrors = 0;
        ChecksumMode finalChecksumMode = ChecksumMode::Unknown;
        /// Every closing command was accepted by the sampler: checksums off, Still Alive off, Notification on and,
        /// when they were touched, Sync LCD on and Auto screen update off.
        bool knownStateRestored = false;
    };

    /// A run of the session and the section 00 primitives against a sampler, the opening done by hand so that the
    /// primitives are exercised one by one (`runRealSamplerSuite` runs `Session::open` itself): it opens the two ports
    /// of `options.target`, puts a logging decorator on each so that every frame on the wire is in the log, and drives a
    /// `Session` on the driver's scheduler and executor through discovery with the target's DeviceID verified (nothing
    /// more is sent if it is not among the answers), the checksum mode command, the OS version, timed Echo round trips,
    /// the checksum mode on and off again with an Echo and the OS version in each, Notification, Sync LCD, Auto screen
    /// update and Still Alive on and off, and then `Session::close`, which puts back what the run changed (checksums
    /// off, Still Alive off, Notification on, Sync LCD on, Auto screen update off — the first three found at the first
    /// contact, the others the spec's or assumed defaults) whatever happened before. A step that must succeed and does
    /// not is recorded in `failedSteps`; an item an older OS lacks may answer ERROR, which is an observation. The log
    /// ends with an observations block.
    ///
    /// It is written against `MidiBackend&` and a ScenarioDriver only, so it runs on the simulated sampler in CI and
    /// on JuceMidiBackend against the real S5000 (`xs56k_akm_probe --session`).
    /// [TASK-AKM-013, RQ-AKM-011 to RQ-AKM-015, RQ-AKM-017, RQ-AKM-018, RQ-AKM-019, RQ-AKM-044,
    /// ADR-AKM-001 (DEC-AKM-007, DEC-AKM-008, DEC-AKM-011, DEC-AKM-012)]
    SessionSmokeResult runSessionSmokeTest(common::midi::MidiBackend& backend, ScenarioDriver& driver,
                                           const SessionSmokeOptions& options, std::ostream& log);
}
