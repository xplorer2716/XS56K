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
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "akm/CommandOptions.hpp"
#include "akm/ItemCatalogue.hpp"

namespace akm
{
    /// The generic encoder and range validator of the catalogue: one function serves every record, and a
    /// new item is a new record, not new code. It checks the number of values and each value against the
    /// range the record gives it, and encodes them in the record's formats. A value that is not acceptable
    /// yields a request that carries the reason in `refusal`, with no data: the session sends nothing and
    /// completes it as `Refused`. `options` are kept as given. [RQ-AKM-001, RQ-AKM-014, RQ-AKM-015,
    /// ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]
    [[nodiscard]] CommandRequest makeRequest(const ItemDescriptor& item, std::span<const std::int64_t> values,
                                             CommandOptions options = {});
    [[nodiscard]] CommandRequest makeRequest(ItemId id, std::span<const std::int64_t> values,
                                             CommandOptions options = {});
    [[nodiscard]] CommandRequest makeRequest(ItemId id, std::initializer_list<std::int64_t> values,
                                             CommandOptions options = {});

    /// The generic REPLY decoder: reads the data of a REPLY in the record's reply formats, one value each,
    /// or returns nothing when the data is too short, too long or not made of data bytes. Ranges are not
    /// enforced on a reply: the sampler says what it says. [RQ-AKM-002, RQ-AKM-044,
    /// ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]
    [[nodiscard]] std::optional<std::vector<std::int64_t>> decodeReply(const ItemDescriptor& item,
                                                                        std::span<const std::uint8_t> data);
    [[nodiscard]] std::optional<std::vector<std::int64_t>> decodeReply(ItemId id, std::span<const std::uint8_t> data);

    /// Encodes the one `String` argument of an item: refused as `WrongArgumentCount` when the item does
    /// not take exactly one `String` argument, or as `NotEncodable` when `text` is not 7-bit ASCII or
    /// contains a `00` byte. `options` are kept as given. [RQ-AKM-002, ADR-AKM-001 (DEC-AKM-013)]
    [[nodiscard]] CommandRequest makeStringRequest(const ItemDescriptor& item, std::string_view text,
                                                   CommandOptions options = {});
    [[nodiscard]] CommandRequest makeStringRequest(ItemId id, std::string_view text, CommandOptions options = {});

    /// Decodes the one `String` REPLY value of an item, or returns nothing when the item's REPLY is not
    /// exactly one `String`, or `data` does not hold exactly one null-terminated ASCII string.
    /// [RQ-AKM-002, ADR-AKM-001 (DEC-AKM-013)]
    [[nodiscard]] std::optional<std::string> decodeStringReply(const ItemDescriptor& item,
                                                                std::span<const std::uint8_t> data);
    [[nodiscard]] std::optional<std::string> decodeStringReply(ItemId id, std::span<const std::uint8_t> data);
}
