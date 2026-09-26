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

    WireLog::~WireLog()
    {
        flush();
    }

    void WireLog::record(Entry entry)
    {
        const std::lock_guard lock(_mutex);
        switch (entry.kind)
        {
            case Kind::Outgoing:
                ++_framesSent;
                break;
            case Kind::Incoming:
                ++_framesReceived;
                if (isStillAlive(entry.frame))
                    ++_stillAlive;
                break;
            case Kind::Note:
            case Kind::InputError:
                break;
        }
        _pending.push_back(std::move(entry));
    }

    void WireLog::note(std::string_view text)
    {
        record(Entry{Kind::Note, {}, {}, std::string(text)});
    }

    void WireLog::outgoing(std::span<const std::uint8_t> frame)
    {
        record(Entry{Kind::Outgoing, elapsed(), std::vector<std::uint8_t>(frame.begin(), frame.end()), {}});
    }

    void WireLog::incoming(std::span<const std::uint8_t> frame)
    {
        record(Entry{Kind::Incoming, elapsed(), std::vector<std::uint8_t>(frame.begin(), frame.end()), {}});
    }

    void WireLog::inputError(std::string_view description)
    {
        record(Entry{Kind::InputError, {}, {}, std::string(description)});
    }

    void WireLog::flush()
    {
        const std::lock_guard writing(_writeMutex);
        std::vector<Entry> entries;
        {
            const std::lock_guard lock(_mutex);
            entries.swap(_pending);
        }
        for (const Entry& entry : entries)
            write(entry);
        _out << std::flush;
    }

    void WireLog::write(const Entry& entry)
    {
        switch (entry.kind)
        {
            case Kind::Note:
                _out << "# " << entry.text << "\n";
                break;
            case Kind::Outgoing:
                _out << secondsText(entry.at) << "  OUT  " << hex(entry.frame) << "\n";
                break;
            case Kind::Incoming:
                _out << secondsText(entry.at) << "  IN   " << hex(entry.frame) << " | off: "
                     << reading(entry.frame, ChecksumMode::Off) << " | on: " << reading(entry.frame, ChecksumMode::On)
                     << "\n";
                break;
            case Kind::InputError:
                _out << "# input error: " << entry.text << "\n";
                break;
        }
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
