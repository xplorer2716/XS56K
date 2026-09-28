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
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "akm/Scheduler.hpp"
#include "akm/Session.hpp"
#include "akm/SystemVersion.hpp"
#include "akm/harness/EchoScenario.hpp"
#include "akm/harness/ScenarioDriver.hpp"
#include "common/midi/MidiPorts.hpp"

namespace akm::harness
{
    struct RealSuiteOptions
    {
        ScenarioTarget target;
        /// How long a session waits for the answer to each of its commands.
        Scheduler::Clock::duration commandTimeout = std::chrono::seconds(3);
        Scheduler::Clock::duration discoveryWindow = DEFAULT_DISCOVERY_WINDOW;
        /// Echo round trips timed by the latency check; none skips the check.
        int echoRepeats = ECHO_LATENCY_ROUND_TRIPS;
        /// Whether the sessions of the run switch Sync LCD and Auto screen update. They change what the LCD does,
        /// and the sampler cannot be asked what they were, so a run that must not touch them leaves them alone.
        bool touchLcdSettings = true;
        /// The optional check that sends one slow, harmless command (§10/&01, update the list of disks) with Still
        /// Alive on, to see whether `F0 F7` messages reach the host while the sampler works. The one command outside
        /// sections 00 and 02 that the suite sends, so it is asked for.
        bool slowOperation = false;
        /// The optional check that asks the owner to power-cycle the sampler while a session is open, to see what
        /// survives it. It needs `askOwner`.
        bool powerCycle = false;
        /// The optional checks that create, select, change and delete a program under a reserved test name
        /// (RQ-AKM-027): off by default, like `slowOperation` and `powerCycle`, since they are the only checks that
        /// touch a stored program at all (every other check changes section 00 settings only).
        bool programLifecycle = false;
        /// The sample name §06/&01 (Set Zone Sample) assigns during the zone check, part of
        /// `programLifecycle` (RQ-AKM-038): a real sample the operator confirms is already in the
        /// sampler's memory. Empty (the default) skips only the sample-assignment part of that check,
        /// reported as skipped rather than passed; every other §06 item still runs.
        std::optional<std::string> sampleName;
        /// Tells the owner what to do and returns once they have done it, or false when they decline. It runs on the
        /// scenario's thread, with nothing in flight and the log written up to that point.
        std::function<bool(const std::string& instruction)> askOwner;
        /// Written in the log header when not empty (the scenario itself reads no wall clock).
        std::string startedAt;
    };

    enum class CheckOutcome
    {
        Passed,
        Failed,
        Skipped,  ///< not run: not asked for, declined, or nothing to check it against
    };

    struct CheckReport
    {
        std::string title;
        CheckOutcome outcome = CheckOutcome::Passed;
        /// What the check found; why it failed or was skipped.
        std::string detail;
    };

    struct RealSuiteResult
    {
        bool portsOpened = false;
        /// The checks in the order they ran; none when the ports could not be opened.
        std::vector<CheckReport> checks;
        /// The DeviceIDs that answered the discovery of the first session that got one.
        std::vector<std::uint8_t> discoveredDeviceIds;
        std::optional<OsVersionReport> osVersion;
        /// Echo round trips that came back right, and the time each took from submit to completion.
        std::size_t echoRoundTrips = 0;
        std::vector<Scheduler::Clock::duration> echoLatencies;
        /// `F0 F7` messages that reached the host, all checks together.
        std::size_t stillAliveMessagesSeen = 0;
        std::size_t framesSent = 0;
        std::size_t framesReceived = 0;
        std::size_t rejectedMessages = 0;
        std::size_t unsolicitedConfirmations = 0;
        std::size_t lateErrors = 0;
        /// Every session of the run was closed and put back every setting it had changed: the sampler is in the
        /// state the closing documents. True when nothing was changed at all.
        bool knownStateRestored = true;

        [[nodiscard]] std::size_t count(CheckOutcome outcome) const;
        /// No check failed (one that was skipped did not).
        [[nodiscard]] bool passed() const { return count(CheckOutcome::Failed) == 0; }
    };

    /// The suite that runs against a real sampler, opt-in (`xs56k_akm_probe --suite`): checks, each on a session of its
    /// own opened with `Session::open` and closed with `Session::close`, so the opening and the closing run on the
    /// hardware. A check that fails, or throws, leaves its session closed all the same — the destructor of the
    /// guard around it closes it — so whatever a check did, the sampler ends as the closing leaves it (checksums
    /// off, Still Alive off, Notification on, Sync LCD on, Auto screen update off) and only §00 settings were changed
    /// (RQ-AKM-018). In order: open and close; Echo returns the bytes sent; the round trips of the latency
    /// measurement; the OS version; checksums on and off; the close puts back every setting, all of them switched
    /// first; a check that fails half way leaves the sampler in the known state; then, if asked for, the slow
    /// operation, the power cycle, and the program lifecycle checks — the only ones that touch a stored program, and
    /// only one of their own, created under a reserved name and always deleted again, even when a check fails half
    /// way (RQ-AKM-027) — the third of which also adds keygroups to it and round-trips every section 08 item on
    /// them, including the "all keygroups" shape of keygroup 0 (RQ-AKM-030, RQ-AKM-031, RQ-AKM-033), and a fourth
    /// that round-trips every non-sample section 06 item on a zone, then zone 0 ("all four") and keygroup 0 + zone
    /// 0 (RQ-AKM-034, RQ-AKM-036, TASK-AKM-037), assigning a sample by name too when `sampleName` is given
    /// (RQ-AKM-035, RQ-AKM-038). A check that finds no sampler at the target ends the suite: nothing else is
    /// sent. Everything is logged as by the session smoke test (the wire in the format of the first-contact probe,
    /// buffered so that the log cannot slow what it records), and the log ends with the observations of RQ-AKM-017.
    ///
    /// It is written against `MidiBackend&` and a ScenarioDriver only, so it runs on the simulated sampler in CI and on
    /// JuceMidiBackend against the real S5000. [TASK-AKM-010, TASK-AKM-024, TASK-AKM-033, TASK-AKM-038, RQ-AKM-010,
    /// RQ-AKM-011, RQ-AKM-012, RQ-AKM-013, RQ-AKM-015, RQ-AKM-017, RQ-AKM-018, RQ-AKM-019, RQ-AKM-025, RQ-AKM-027,
    /// RQ-AKM-030, RQ-AKM-031, RQ-AKM-033, RQ-AKM-034, RQ-AKM-035, RQ-AKM-036, RQ-AKM-038, RQ-AKM-039, RQ-AKM-040,
    /// RQ-AKM-041, RQ-AKM-042, RQ-AKM-044, ADR-AKM-001 (DEC-AKM-006, DEC-AKM-007, DEC-AKM-008)]
    RealSuiteResult runRealSamplerSuite(common::midi::MidiBackend& backend, ScenarioDriver& driver,
                                        const RealSuiteOptions& options, std::ostream& log);
}
