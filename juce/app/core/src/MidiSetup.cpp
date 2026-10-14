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
#include "app/MidiSetup.hpp"

#include <charconv>
#include <string_view>

namespace app
{
    // [RQ-APP-015, RQ-APP-016, ADR-APP-001 (DEC-APP-009), TASK-APP-012]
    namespace
    {
        constexpr std::string_view INPUT_KEY = "midi.input";
        constexpr std::string_view OUTPUT_KEY = "midi.output";
        constexpr std::string_view DEVICE_ID_KEY = "midi.deviceId";
    }

    MidiPortLists listMidiPorts(const common::midi::MidiBackend& backend)
    {
        return MidiPortLists{backend.inputDeviceNames(), backend.outputDeviceNames()};
    }

    std::optional<std::string> deviceIdProblem(int deviceId)
    {
        if (deviceId >= MIN_DEVICE_ID && deviceId <= MAX_DEVICE_ID)
            return std::nullopt;
        return "The DeviceID " + std::to_string(deviceId) + " is not valid: a sampler's DeviceID is " + std::to_string(MIN_DEVICE_ID) +
               " to " + std::to_string(MAX_DEVICE_ID) + ".";
    }

    std::optional<MidiSetup> loadMidiSetup(const AppSettings& settings)
    {
        const auto input = settings.read(INPUT_KEY);
        const auto output = settings.read(OUTPUT_KEY);
        const auto deviceIdText = settings.read(DEVICE_ID_KEY);
        if (!input || !output || !deviceIdText)
            return std::nullopt;
        int deviceId = 0;
        const char* const end = deviceIdText->data() + deviceIdText->size();
        const auto parsed = std::from_chars(deviceIdText->data(), end, deviceId);
        if (parsed.ec != std::errc{} || parsed.ptr != end || deviceIdProblem(deviceId))
            return std::nullopt;
        return MidiSetup{*input, *output, deviceId};
    }

    void storeMidiSetup(AppSettings& settings, const MidiSetup& setup)
    {
        settings.write(INPUT_KEY, setup.inputPort);
        settings.write(OUTPUT_KEY, setup.outputPort);
        settings.write(DEVICE_ID_KEY, std::to_string(setup.deviceId));
    }
}
