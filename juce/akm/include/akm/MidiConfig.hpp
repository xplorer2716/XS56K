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

#include "akm/CommandResult.hpp"
#include "akm/Session.hpp"

namespace akm
{
    // The MIDI configuration primitives of section 04 (spec Table 8), on a session: thin typed wrappers over the
    // catalogue's records (ADR-AKM-001, DEC-AKM-003, DEC-AKM-012). Every §04 item is sampler-wide, with no current
    // item, no Get and no REPLY: each completes on DONE, and what it sets is the sampler's stored MIDI setup (the
    // MIDI SETUP page of UTILITIES), which cannot be read back. Each returns at once and reports on the session's
    // thread, like `Session::submit`. The session must outlive every call. [FTR-AKM-009]

    /// Multi Select (§04/&02): how multis are selected remotely, with the byte each travels as. [RQ-AKM-078]
    enum class MultiSelectMode : std::uint8_t
    {
        Off = 0,
        ProgramChange = 1,
        Bank = 2,
    };

    /// Aftertouch (§04/&05): which aftertouch the sampler responds to, system-wide. [RQ-AKM-078]
    enum class AftertouchType : std::uint8_t
    {
        Channel = 0,
        Polyphonic = 1,
    };

    /// The MIDI event types a filter acts on (§04/&06, &07), with the byte each travels as. [RQ-AKM-079]
    enum class MidiFilterEvent : std::uint8_t
    {
        NoteOn = 0,
        Aftertouch = 1,
        Wheels = 2,
        Volume = 3,
    };

    /// Program Change Enable (§04/&01): remote selection of programs within parts. [RQ-AKM-078]
    void setProgramChangeEnabled(Session& session, bool enabled, CommandCompletion completion);

    /// Multi Select (§04/&02). A mode that is not one of the three (an enumerator cast from another value) is
    /// refused without sending (`ArgumentOutOfRange`). [RQ-AKM-078]
    void setMultiSelect(Session& session, MultiSelectMode mode, CommandCompletion completion);

    /// Multi Select Channel (§04/&03): 0 to 31, 1A = 0 … 16B = 31; a channel outside that is refused without
    /// sending (`ArgumentOutOfRange`). It has no effect while Multi Select is off. [RQ-AKM-078]
    void setMultiSelectChannel(Session& session, int channel, CommandCompletion completion);

    /// External APM Controller (§04/&04): the MIDI controller used as a source in the APM matrix, 0 to 127; a
    /// value outside that is refused without sending (`ArgumentOutOfRange`). [RQ-AKM-078]
    void setExternalApmController(Session& session, int controller, CommandCompletion completion);

    /// Aftertouch (§04/&05); refused like `setMultiSelect` for a value that is neither type. [RQ-AKM-078]
    void setAftertouch(Session& session, AftertouchType type, CommandCompletion completion);

    /// Enables the MIDI filter of an event type on a channel (§04/&06): the sampler allows those messages. The
    /// channel is 0 to 31, 1A = 0 … 16B = 31, carrying the port; an event type that is not one of the four (an
    /// enumerator cast from another value) or a channel outside that is refused without sending
    /// (`ArgumentOutOfRange`). [RQ-AKM-079]
    void allowMidiEvents(Session& session, MidiFilterEvent event, int channel, CommandCompletion completion);

    /// Disables the MIDI filter of an event type on a channel (§04/&07): the sampler ignores those messages;
    /// refused like `allowMidiEvents`. [RQ-AKM-079]
    void ignoreMidiEvents(Session& session, MidiFilterEvent event, int channel, CommandCompletion completion);
}
