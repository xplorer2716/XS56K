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

// A simulated sampler pre-loaded with programs, as the real one is before the real-sampler suite's
// program-lifecycle checks run against it (RQ-AKM-027's own scenario: "holding programs KEEP1 and
// KEEP2"). Sent as raw encoded frames through HostProbe, like AwkwardSampler.hpp does for section 00, so
// the seeding does not depend on a Session (the suite opens its own). [TASK-AKM-024, RQ-AKM-027]
#include <cstdint>
#include <string>
#include <vector>

#include "HostProbe.hpp"
#include "TestBytes.hpp"
#include "akm/Command.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"

namespace akm::test
{
    /// Creates one program per name, in order, then selects the one at `selectIndex` as current — each
    /// sent as a raw §0A frame, checksums off, so the sampler ends exactly as a real one holding these
    /// programs would, before any session is opened against it.
    inline void seedPrograms(akm::harness::SimulatedMidiBackend& backend, const std::vector<std::string>& names,
                             std::size_t selectIndex)
    {
        HostProbe host(backend, backend.inputName(), backend.outputName());
        std::uint8_t userRef = 0x01;
        const auto send = [&](const CommandRequest& request) {
            const EncodeResult frame = encodeCommand(0, Bytes{userRef++}, request.command, ChecksumMode::Off);
            host.send(frame.bytes);
        };
        for (const std::string& name : names)
            send(makeStringRequest(ItemId::ProgramCreate, name));
        if (!names.empty())
            send(makeRequest(ItemId::ProgramSelectByIndex, {static_cast<std::int64_t>(selectIndex) / 128,
                                                            static_cast<std::int64_t>(selectIndex) % 128}));
    }
}
