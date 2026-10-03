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
#include "akm/ItemCatalogue.hpp"

#include <algorithm>

namespace akm
{
    const ItemDescriptor* findItem(std::uint8_t section, std::uint8_t item)
    {
        // A linear scan: the table is short, and a lookup happens only while a REPLY is decoded in mode
        // Unknown, at the speed of MIDI.
        const auto found = std::find_if(ITEM_TABLE.begin(), ITEM_TABLE.end(), [section, item](const ItemDescriptor& record) {
            return record.section == section && record.item == item;
        });
        return found == ITEM_TABLE.end() ? nullptr : &*found;
    }

    const ItemDescriptor* findReplyItem(std::uint8_t section, std::uint8_t item)
    {
        const auto found = std::find_if(ITEM_TABLE.begin(), ITEM_TABLE.end(), [section, item](const ItemDescriptor& record) {
            return record.item == item && (record.section == section || record.replySection == section);
        });
        return found == ITEM_TABLE.end() ? nullptr : &*found;
    }
}
