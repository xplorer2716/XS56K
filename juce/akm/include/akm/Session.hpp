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
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "akm/Checksum.hpp"
#include "akm/CommandOptions.hpp"
#include "akm/CommandResult.hpp"
#include "akm/DiagnosticSink.hpp"
#include "akm/Executor.hpp"
#include "akm/Scheduler.hpp"
#include "akm/Task.hpp"
#include "common/midi/MidiPorts.hpp"

namespace akm
{
    // Provisional timing values, measured on an S5000 running OS 2.14 where they could be (the OK arrives
    // 6 to 8 ms after the command, the DONE or REPLY 9 to 12 ms), to be revisited with the slow operations
    // of TASK-AKM-010. [RQ-AKM-010, RQ-AKM-012, RQ-AKM-017, ADR-AKM-001 (DEC-AKM-006)]
    inline constexpr std::chrono::milliseconds DEFAULT_COMMAND_TIMEOUT{2000};
    inline constexpr std::chrono::milliseconds DEFAULT_MAX_TOTAL_WAIT{60000};
    inline constexpr std::chrono::milliseconds DEFAULT_DISCOVERY_WINDOW{500};
    /// How many confirmations in a row may fail checksum verification before the port's mode is taken to
    /// be unknown — what a sampler reboot looks like from here. [RQ-AKM-041]
    inline constexpr int DEFAULT_CHECKSUM_FAILURES_BEFORE_UNKNOWN = 3;

    /// The named, configurable durations of one session. [RQ-AKM-010, RQ-AKM-041]
    struct SessionTiming
    {
        Scheduler::Clock::duration commandTimeout = DEFAULT_COMMAND_TIMEOUT;
        Scheduler::Clock::duration maxTotalWait = DEFAULT_MAX_TOTAL_WAIT;
        int checksumFailuresBeforeUnknown = DEFAULT_CHECKSUM_FAILURES_BEFORE_UNKNOWN;
    };

    using CommandCompletion = std::function<void(const CommandResult&)>;
    using SequenceCompletion = std::function<void(const SequenceResult&)>;

    /// The state machine of one MIDI port pair: user-ref allocation and matching, one command in flight
    /// with the others queued in order, completion on DONE, REPLY or ERROR, timeout, Still Alive,
    /// collection windows, the port's tri-state checksum mode, and sequences that stop at the first
    /// failure.
    ///
    /// Everything that reads or changes its state, sends a frame or invokes a completion runs as a task on
    /// the injected executor, one at a time, so the session needs no lock of its own: the backend's input
    /// callback, the scheduler's timer thread and the callers of `submit()` only post tasks. No completion
    /// ever runs on the caller's thread or on the backend's callback thread, and none blocks it.
    /// [RQ-AKM-007 to RQ-AKM-013, RQ-AKM-020, RQ-AKM-041, RQ-AKM-043,
    /// ADR-AKM-001 (DEC-AKM-002, DEC-AKM-004, DEC-AKM-005, DEC-AKM-009, DEC-AKM-010, DEC-AKM-011)]
    ///
    /// The ports outlive the session. The constructor registers the input callback and starts the input,
    /// as DEC-AKM-005 requires before the first send; `close()` stops it. A session SHALL be closed, and
    /// its executor left idle, before it is destroyed.
    class Session
    {
    public:
        Session(SessionTiming timing, Executor& executor, Scheduler& scheduler,
                common::midi::MidiInputPort& input, common::midi::MidiOutputPort& output,
                DiagnosticSink& diagnostics);
        ~Session();

        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        /// Binds the DeviceID that every following `Addressing::BoundTarget` command is addressed to, and
        /// that its confirmations must carry. Callable from any thread; it takes effect on the session
        /// thread, so it is ordered with the commands submitted around it. TASK-AKM-009's `open()` calls
        /// it once discovery has verified the target. [RQ-AKM-007, RQ-AKM-039]
        void bindTarget(std::uint8_t deviceId);

        /// A snapshot, readable from any thread: the bound target, the checksum mode the session assumes
        /// for this port (`Unknown` until a checksum-mode command succeeds), and whether a received
        /// `F0 F7` restarts the pending command's timeout.
        [[nodiscard]] std::optional<std::uint8_t> boundTarget() const;
        [[nodiscard]] ChecksumMode checksumMode() const;
        [[nodiscard]] bool stillAliveMonitoring() const;

        /// Queues one command and returns at once, from any thread. `completion` runs on the session
        /// thread, in completion order, exactly once — an immediate refusal included. [RQ-AKM-008,
        /// RQ-AKM-020]
        void submit(CommandRequest request, CommandCompletion completion);

        /// Queues commands that run in order with nothing else interleaved; when one fails, the rest
        /// complete as `Cancelled` and the index of the failure is reported. An empty sequence completes
        /// at once with no result. [RQ-AKM-043, ADR-AKM-001 (DEC-AKM-010)]
        void submitSequence(std::vector<CommandRequest> requests, SequenceCompletion completion);

        /// Stops the input port, then completes the command in flight and every queued command as
        /// `Cancelled` and runs `onClosed`, both on the session thread. Returns false, changing nothing,
        /// when it is called from the session thread — closing from one of the session's own completions
        /// is forbidden — or when the session is already closing. Commands submitted afterwards are
        /// refused with `RefusalReason::SessionClosed`. Restoring the §00 settings is TASK-AKM-011.
        /// [RQ-AKM-042, ADR-AKM-001 (DEC-AKM-004)]
        [[nodiscard]] bool close(Task onClosed);

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
    };
}
