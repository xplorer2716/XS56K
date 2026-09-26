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
#include "akm/harness/WireLog.hpp"

#include <functional>
#include <utility>

#include "akm/harness/WireFormat.hpp"
#include "common/midi/MidiMessage.hpp"

namespace akm::harness
{
    namespace
    {
        constexpr std::size_t STILL_ALIVE_SIZE = 2;

        bool isStillAlive(std::span<const std::uint8_t> frame)
        {
            return frame.size() == STILL_ALIVE_SIZE && frame.front() == common::midi::SYSEX_START
                   && frame.back() == common::midi::SYSEX_END;
        }
    }

    WireLog::WireLog(std::ostream& out, Scheduler& scheduler)
        : _out(out), _scheduler(scheduler), _start(scheduler.now())
    {
    }

    void WireLog::note(std::string_view text)
    {
        const std::lock_guard lock(_mutex);
        _out << "# " << text << "\n";
    }

    void WireLog::outgoing(std::span<const std::uint8_t> frame)
    {
        const std::lock_guard lock(_mutex);
        ++_framesSent;
        _out << secondsText(_scheduler.now() - _start) << "  OUT  " << hex(frame) << "\n";
    }

    void WireLog::incoming(std::span<const std::uint8_t> frame)
    {
        const std::lock_guard lock(_mutex);
        ++_framesReceived;
        if (isStillAlive(frame))
            ++_stillAlive;
        _out << secondsText(_scheduler.now() - _start) << "  IN   " << hex(frame)
             << " | off: " << reading(frame, ChecksumMode::Off) << " | on: " << reading(frame, ChecksumMode::On) << "\n";
    }

    void WireLog::inputError(std::string_view description)
    {
        const std::lock_guard lock(_mutex);
        _out << "# input error: " << description << "\n";
    }

    std::size_t WireLog::framesSent() const
    {
        const std::lock_guard lock(_mutex);
        return _framesSent;
    }

    std::size_t WireLog::framesReceived() const
    {
        const std::lock_guard lock(_mutex);
        return _framesReceived;
    }

    std::size_t WireLog::stillAliveMessages() const
    {
        const std::lock_guard lock(_mutex);
        return _stillAlive;
    }

    Scheduler::Clock::duration WireLog::elapsed() const
    {
        return _scheduler.now() - _start;
    }

    void LoggingOutputPort::send(const common::midi::MidiMessage& message)
    {
        _log.outgoing(message.bytes());
        _inner.send(message);
    }

    void LoggingInputPort::setCallbacks(common::midi::MidiInputCallbacks callbacks)
    {
        common::midi::MidiInputCallbacks logged = std::move(callbacks);
        WireLog* const log = &_log;
        logged.onSysExMessage = [log, next = std::move(logged.onSysExMessage)](const common::midi::MidiMessage& message) {
            log->incoming(message.bytes());
            if (next)
                next(message);
        };
        logged.onError = [log, next = std::move(logged.onError)](const std::string& description) {
            log->inputError(description);
            if (next)
                next(description);
        };
        _inner.setCallbacks(std::move(logged));
    }
}
