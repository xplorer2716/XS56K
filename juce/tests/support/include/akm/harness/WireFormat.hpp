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
#include <string>

#include "akm/Checksum.hpp"
#include "akm/Scheduler.hpp"

namespace akm::harness
{
    // The text formats of the logs the owner sends back from the real sampler: the first-contact probe and the
    // session smoke test write the same lines, so that one reading habit serves both. [TASK-AKM-012,
    // TASK-AKM-013, RQ-AKM-017]

    /// Pairs of upper-case hexadecimal digits separated by spaces: `F0 47 5E 00 F7`.
    [[nodiscard]] std::string hex(std::span<const std::uint8_t> bytes);

    /// Like `hex`, or `-` for no bytes at all.
    [[nodiscard]] std::string hexOrDash(std::span<const std::uint8_t> bytes);

    /// A time as seconds with three decimals, right-aligned in a width of nine: `    1.500`.
    [[nodiscard]] std::string secondsText(Scheduler::Clock::duration elapsed);

    /// Whole milliseconds.
    [[nodiscard]] long long millisecondsOf(Scheduler::Clock::duration duration);

    /// How a received message reads under one checksum mode: its kind (OK, DONE, REPLY, ERROR with its number
    /// and meaning, or the Still Alive `F0 F7`), then its DeviceID, user-refs, section, item and data; or why the
    /// codec refused it. Reading the same frame under two modes shows which one the sampler is in.
    [[nodiscard]] std::string reading(std::span<const std::uint8_t> bytes, ChecksumMode mode);
}
