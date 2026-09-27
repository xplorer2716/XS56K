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

// A simulated sampler left as a program that crashed might have left the real one, for the tests of the scenarios
// that must cope with it. [TASK-AKM-010, TASK-AKM-013, RQ-AKM-018]
#include <cstdint>
#include <utility>
#include <vector>

#include "HostProbe.hpp"
#include "TestBytes.hpp"
#include "akm/Command.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"

namespace akm::test
{
    /// Leaves the sampler with checksums on, notification off, Sync LCD off, Auto screen update on and Still Alive on,
    /// sent through the codec while checksums are off, the checksum-mode command last.
    inline void leaveInAwkwardState(akm::harness::SimulatedMidiBackend& backend)
    {
        HostProbe host(backend, backend.inputName(), backend.outputName());
        const std::vector<std::pair<std::uint8_t, std::uint8_t>> changes{{0x01, 0}, {0x03, 0}, {0x05, 1}, {0x07, 1}, {0x04, 1}};
        ChecksumMode mode = ChecksumMode::Off;
        std::uint8_t userRef = 0x30;
        for (const auto& [item, value] : changes)
        {
            const akm::EncodeResult frame = akm::encodeCommand(0, Bytes{userRef++}, Command{0x00, item, {value}}, mode);
            host.send(frame.bytes);
        }
    }
}
