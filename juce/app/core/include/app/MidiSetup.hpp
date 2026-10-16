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

#include <optional>
#include <string>
#include <vector>

#include "app/AppSettings.hpp"
#include "common/midi/MidiPorts.hpp"

namespace app
{
    /// The sampler's DeviceID is 0 to 31 (spec p. 3). [RQ-APP-015]
    inline constexpr int MIN_DEVICE_ID = 0;
    inline constexpr int MAX_DEVICE_ID = 31;

    /// The ports and the DeviceID the application talks to the sampler with. [RQ-APP-015, RQ-APP-016]
    struct MidiSetup
    {
        std::string inputPort;   ///< what the sampler sends on
        std::string outputPort;  ///< what the sampler receives on
        int deviceId = 0;
    };

    /// The computer's MIDI ports by name. [RQ-APP-015, RQ-MID-001]
    struct MidiPortLists
    {
        std::vector<std::string> inputs;
        std::vector<std::string> outputs;
    };

    /// Lists the computer's MIDI ports: nothing is opened and nothing is sent to the sampler. [RQ-APP-015, ADR-APP-001 (DEC-APP-009)]
    [[nodiscard]] MidiPortLists listMidiPorts(const common::midi::MidiBackend& backend);

    /// Why a DeviceID is refused, or nothing when it is one of 0 to 31. [RQ-APP-015]
    [[nodiscard]] std::optional<std::string> deviceIdProblem(int deviceId);

    /// The setup stored by the last validation, or nothing when none is complete. [RQ-APP-016, ADR-APP-001 (DEC-APP-009)]
    [[nodiscard]] std::optional<MidiSetup> loadMidiSetup(const AppSettings& settings);
    void storeMidiSetup(AppSettings& settings, const MidiSetup& setup);
}
