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

#include <functional>
#include <optional>

#include "app/MidiSetup.hpp"

namespace app::ui
{
    class LookAndFeel;

    /// Opens the MIDI setup dialog without blocking the message thread: the computer's input and output ports by name, the ones of the
    /// stored setup marked "in use", and the DeviceID, refused outside 0 to 31 with the range said. `onValidated` gets the setup when the
    /// person presses OK on a valid one; a cancel calls nothing. Listing the ports sends nothing to the sampler. [RQ-APP-015, RQ-APP-016,
    /// ADR-APP-001 (DEC-APP-009), TASK-APP-017]
    void showMidiSetupDialog(LookAndFeel& lookAndFeel, const common::midi::MidiBackend& backend, const std::optional<MidiSetup>& current,
                             std::function<void(const MidiSetup&)> onValidated);
}
