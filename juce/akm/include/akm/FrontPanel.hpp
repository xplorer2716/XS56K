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
#include <functional>
#include <optional>

#include "akm/CommandResult.hpp"
#include "akm/Session.hpp"

namespace akm
{
    // The front panel primitives of section 20 (spec Tables 30-31), on a session: thin typed wrappers over the
    // catalogue's records (ADR-AKM-001, DEC-AKM-003, DEC-AKM-012). Every §20 item is sampler-wide, with no current
    // item, no Get and no REPLY: each completes on DONE, which only means the sampler QUEUED the data, not that it
    // acted on it (Table 30, note a). Each returns at once and reports on the session's thread, like
    // `Session::submit`. The session must outlive every call. [FTR-AKM-008]

    /// The keys of spec Table 31, with the keycode each travels as. The spec lists these 43 codes among the 44
    /// values of `&40`-`&6B`: `&66` is listed by no row, and "use of other values may lead to undefined
    /// behaviour" (Table 30, note b), so only these are keys. [RQ-AKM-073]
    enum class FrontPanelKey : std::uint8_t
    {
        // Mode keys.
        Multi = 0x44,
        Fx = 0x40,
        EditSample = 0x42,
        EditProgram = 0x43,
        Record = 0x41,
        Utilities = 0x45,
        Save = 0x46,
        Load = 0x47,
        // Function keys.
        F1 = 0x48,
        F2 = 0x49,
        F3 = 0x4A,
        F4 = 0x4B,
        F5 = 0x4C,
        F6 = 0x4D,
        F7 = 0x4E,
        F8 = 0x4F,
        F9 = 0x50,
        F10 = 0x51,
        F11 = 0x52,
        F12 = 0x53,
        F13 = 0x54,
        F14 = 0x55,
        F15 = 0x56,
        F16 = 0x57,
        // Numeric keys.
        Digit0 = 0x58,
        Digit1 = 0x59,
        Digit2 = 0x5A,
        Digit3 = 0x5B,
        Digit4 = 0x5C,
        Digit5 = 0x5D,
        Digit6 = 0x5E,
        Digit7 = 0x5F,
        Digit8 = 0x60,
        Digit9 = 0x61,
        // Other keys.
        Minus = 0x62,
        Plus = 0x63,
        CursorLeft = 0x64,
        CursorRight = 0x65,
        Window = 0x67,
        Mark = 0x68,
        Jump = 0x69,
        Exit = 0x6A,
        EntPlay = 0x6B,
    };

    /// The key whose keycode is `code`, or nothing when no row of Table 31 lists it (any value outside `&40`-`&6B`,
    /// and `&66`). [RQ-AKM-073]
    [[nodiscard]] std::optional<FrontPanelKey> frontPanelKeyFromCode(int code);

    /// Holds a key down (§20/&01). A value that is not a key of Table 31 — an enumerator cast from an unlisted code —
    /// is refused without sending (`ArgumentOutOfRange`). The key stays down on the sampler until `releaseKey`:
    /// the caller owes it a release (spec p. 41), and a session that is closed first releases it itself
    /// (`Session::close`). [RQ-AKM-073, RQ-AKM-075, ADR-AKM-001 (DEC-AKM-019)]
    void holdKey(Session& session, FrontPanelKey key, CommandCompletion completion);

    /// Releases a key (§20/&02); refused like `holdKey` for a value that is not a key of Table 31. The sampler is
    /// not told which key was held: releasing one that is not down is the sampler's to answer, not refused here.
    /// [RQ-AKM-073]
    void releaseKey(Session& session, FrontPanelKey key, CommandCompletion completion);

    /// What a press reports: the outcome of each of its two commands. [RQ-AKM-073]
    struct KeyPressResult
    {
        CommandResult hold{};
        CommandResult release{};
    };
    using KeyPressCompletion = std::function<void(const KeyPressResult&)>;

    /// Holds a key then releases it. The release is queued behind the hold at once and is sent whatever the hold's
    /// outcome — an ERROR or a timeout included, since the sampler may have registered a key it did not confirm —
    /// so a press never leaves the key down by its own doing. `completion` runs once, after both. A value that is not
    /// a key of Table 31 has both refused and nothing sent. [RQ-AKM-073]
    void pressKey(Session& session, FrontPanelKey key, KeyPressCompletion completion);

    /// The two directions of the data wheel (Table 30, &03), with the byte each travels as. [RQ-AKM-074]
    enum class DataWheelDirection : std::uint8_t
    {
        Forwards = 0,
        Backwards = 1,
    };

    /// Moves the data wheel (§20/&03) `clicks` clicks, 1 to 8; a direction that is neither, or a number of clicks
    /// outside 1-8, is refused without sending (`ArgumentOutOfRange`). DONE means the movement was queued, not
    /// that the sampler made it. [RQ-AKM-074]
    void moveDataWheel(Session& session, DataWheelDirection direction, int clicks, CommandCompletion completion);

    /// Sends one character of ASCII keyboard data (§20/&04), 0 to 127; a value outside that is refused without
    /// sending (`ArgumentOutOfRange`). DONE means the character was queued, not typed. [RQ-AKM-074]
    void sendAsciiKey(Session& session, int ascii, CommandCompletion completion);
}
