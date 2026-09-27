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
#include <span>

namespace akm
{
    /// Whether the sampler adds a checksum to what it sends and expects one in what it receives, for one
    /// port. §00 has no Get, so a session can be unable to know it. [RQ-AKM-003, RQ-AKM-041,
    /// ADR-AKM-001 (DEC-AKM-009)]
    ///  - On: a checksum is sent, and verified and stripped on reception.
    ///  - Off: none is sent, and every byte of a confirmation after the item is data.
    ///  - Unknown: a checksum is sent (the sampler ignores one it does not expect, spec p. 4), and a
    ///    confirmation is decoded by the expected length of its data.
    enum class ChecksumMode
    {
        On,
        Off,
        Unknown,
    };

    /// Checksum of the bytes from the first user-ref to the last data byte: their unsigned 8-bit
    /// wrapping sum with the high bit cleared (spec p. 4). [RQ-AKM-003]
    [[nodiscard]] std::uint8_t checksum(std::span<const std::uint8_t> bytes);
}
