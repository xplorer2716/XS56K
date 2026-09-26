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
#include "akm/Checksum.hpp"

#include "akm/Protocol.hpp"

namespace akm
{
    std::uint8_t checksum(std::span<const std::uint8_t> bytes)
    {
        std::uint8_t sum = 0;
        for (const std::uint8_t value : bytes)
            sum = static_cast<std::uint8_t>(sum + value);
        return static_cast<std::uint8_t>(sum & DATA_BYTE_MAX);
    }
}
