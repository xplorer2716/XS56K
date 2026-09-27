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
#include <mutex>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "akm/Scheduler.hpp"
#include "common/midi/MidiPorts.hpp"

namespace akm::harness
{
    /// The log of a run against a sampler, in the format of the first-contact probe: a line per frame in each
    /// direction — `<seconds>  OUT  <hex>` and `<seconds>  IN   <hex> | off: <reading> | on: <reading>` — and
    /// `#` lines for what the scenario says. Times are read from the scheduler the scenario runs on, from the
    /// moment the log was opened.
    ///
    /// Recording is cheap and never waits for the stream: a frame or a note is kept in memory, stamped with its
    /// time, and only `flush()` formats and writes what was kept, in the order it was recorded. That is the whole
    /// point: the first run against the real S5000 wrote each line to the console from the input callback, which
    /// took about 12 ms a line and delayed the very exchanges the log was recording. A scenario flushes between
    /// its steps, when nothing is in flight, and the destructor writes what is left.
    ///
    /// Any thread may record. `flush()` is meant for one thread at a time — the scenario's — and concurrent
    /// flushes are serialised. [TASK-AKM-013, RQ-AKM-017, ADR-AKM-001 (DEC-AKM-008)]
    class WireLog
    {
    public:
        WireLog(std::ostream& out, Scheduler& scheduler);
        ~WireLog();

        WireLog(const WireLog&) = delete;
        WireLog& operator=(const WireLog&) = delete;

        /// A comment line: `# <text>`.
        void note(std::string_view text);
        void outgoing(std::span<const std::uint8_t> frame);
        void incoming(std::span<const std::uint8_t> frame);
        void inputError(std::string_view description);

        /// Writes what has been recorded since the last flush, then flushes the stream.
        void flush();

        /// Frames sent; messages received, the null `F0 F7` messages included; and those `F0 F7` alone. They
        /// count at the moment of recording, not of writing.
        [[nodiscard]] std::size_t framesSent() const;
        [[nodiscard]] std::size_t framesReceived() const;
        [[nodiscard]] std::size_t stillAliveMessages() const;

        /// The time since the log was opened.
        [[nodiscard]] Scheduler::Clock::duration elapsed() const;

    private:
        enum class Kind
        {
            Note,
            Outgoing,
            Incoming,
            InputError,
        };

        struct Entry
        {
            Kind kind = Kind::Note;
            Scheduler::Clock::duration at{};
            std::vector<std::uint8_t> frame;  ///< the bytes of a frame
            std::string text;                 ///< the text of a note or of an input error
        };

        void record(Entry entry);
        void write(const Entry& entry);

        std::ostream& _out;
        Scheduler& _scheduler;
        const Scheduler::Clock::time_point _start;
        // Guards what is pending and the counters; held for a moment, never while writing.
        mutable std::mutex _mutex;
        // Serialises the writers, so that two flushes cannot interleave their lines.
        std::mutex _writeMutex;
        std::vector<Entry> _pending;
        std::size_t _framesSent = 0;
        std::size_t _framesReceived = 0;
        std::size_t _stillAlive = 0;
    };

    /// An output port that logs each message before passing it on to the port it wraps. The wrapped port
    /// outlives it. [TASK-AKM-013]
    class LoggingOutputPort final : public common::midi::MidiOutputPort
    {
    public:
        LoggingOutputPort(common::midi::MidiOutputPort& inner, WireLog& log) : _inner(inner), _log(log) {}

        [[nodiscard]] std::string deviceName() const override { return _inner.deviceName(); }
        void send(const common::midi::MidiMessage& message) override;

    private:
        common::midi::MidiOutputPort& _inner;
        WireLog& _log;
    };

    /// An input port that logs each SysEx message and each error the port it wraps reports, then passes them
    /// on to the callbacks registered through it; every other kind of message goes straight through. The
    /// wrapped port outlives it. [TASK-AKM-013]
    class LoggingInputPort final : public common::midi::MidiInputPort
    {
    public:
        LoggingInputPort(common::midi::MidiInputPort& inner, WireLog& log) : _inner(inner), _log(log) {}

        [[nodiscard]] std::string deviceName() const override { return _inner.deviceName(); }
        void setCallbacks(common::midi::MidiInputCallbacks callbacks) override;
        void start() override { _inner.start(); }
        void stop() override { _inner.stop(); }
        [[nodiscard]] bool isStarted() const override { return _inner.isStarted(); }

    private:
        common::midi::MidiInputPort& _inner;
        WireLog& _log;
    };
}
