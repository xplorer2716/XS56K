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

#include <string>
#include <string_view>
#include <vector>

#include "akm/FrontPanel.hpp"

namespace akm::harness
{
    // The mapping of the PC keyboard to the sampler's front panel for the owner-driven check of section 20
    // (`xs56k_akm_probe --suite --front-panel`). It is the owner's own choice (session AKM, 2026-10-03), kept here, in one
    // place, with the function that applies it, so that it is tested and printed from the same source. The reading of
    // the console itself is the probe's. [FTR-AKM-008, RQ-AKM-076, TASK-AKM-073]

    /// The PC keys, as one `int` each: a key that has a character is that character's code (a letter, a digit, Tab,
    /// Enter, Escape, Backspace, Space), any other key is one of the codes from `EXTENDED_BASE` up. The probe's console
    /// reader turns what the console gives it into these.
    namespace pc_key
    {
        inline constexpr int BACKSPACE = 8;
        inline constexpr int TAB = 9;
        inline constexpr int ENTER = 13;
        inline constexpr int ESCAPE = 27;
        inline constexpr int SPACE = 32;
        /// Keys with no character; `F1 + n - 1` is function key n, 1 to 12.
        inline constexpr int EXTENDED_BASE = 256;
        inline constexpr int F1 = EXTENDED_BASE + 1;
        inline constexpr int UP = EXTENDED_BASE + 20;
        inline constexpr int DOWN = EXTENDED_BASE + 21;
        inline constexpr int LEFT = EXTENDED_BASE + 22;
        inline constexpr int RIGHT = EXTENDED_BASE + 23;
        inline constexpr int PAGE_UP = EXTENDED_BASE + 24;
        inline constexpr int PAGE_DOWN = EXTENDED_BASE + 25;
    }

    enum class RemoteActionKind
    {
        None,           ///< the PC key stands for nothing: nothing is sent
        Press,          ///< a short press of `key`: a Hold then a Release
        ToggleHold,     ///< holds `key` if it is not held, releases it if it is (to hear a sample, as the spec's example)
        Wheel,          ///< moves the data wheel `clicks` clicks in `direction`
        Ascii,          ///< sends `ascii` as ASCII keyboard data
        ToggleTextMode, ///< goes into the text mode
        LeaveTextMode,  ///< goes back to the normal mode
        End,            ///< ends the check
    };

    struct RemoteAction
    {
        RemoteActionKind kind = RemoteActionKind::None;
        FrontPanelKey key = FrontPanelKey::Exit;
        DataWheelDirection direction = DataWheelDirection::Forwards;
        int clicks = 0;
        int ascii = 0;
    };

    /// What the PC key `pcKey` stands for, in the normal mode (`textMode` false) or in the text mode, where every
    /// printable character is ASCII, Backspace and Enter are ASCII 8 and 13, and Tab or Escape leaves the mode.
    /// Letters of the normal mode are read in either case. [RQ-AKM-076]
    [[nodiscard]] RemoteAction remoteAction(bool textMode, int pcKey);

    /// The name the sampler's front panel gives the key ("F1", "EDIT SAMPLE", "ENT/PLAY"…), for what is said to the owner
    /// and written in the log. Empty for a value that is not a key of Table 31. [RQ-AKM-076]
    [[nodiscard]] std::string_view remoteKeyName(FrontPanelKey key);

    /// The mapping as text, one line each, for the owner to read before the check starts. [RQ-AKM-076]
    [[nodiscard]] const std::vector<std::string>& remoteMappingLines();
}
