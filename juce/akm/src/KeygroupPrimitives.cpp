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
#include "akm/KeygroupPrimitives.hpp"

#include <span>
#include <utility>

#include "akm/ItemRequest.hpp"

namespace akm
{
    namespace
    {
        constexpr std::span<const std::int64_t> NO_VALUES{};
    }

    void selectKeygroup(Session& session, int keygroup, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::KeygroupSelect, {static_cast<std::int64_t>(keygroup)}), std::move(completion));
    }

    void getCurrentKeygroup(Session& session, CurrentKeygroupCompletion completion)
    {
        session.submit(makeRequest(ItemId::KeygroupGetCurrent, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           CurrentKeygroupResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::KeygroupGetCurrent, rep->data);
                               if (values && values->size() == 1)
                                   result.keygroup = static_cast<int>((*values)[0]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getForAllKeygroups(Session& session, ItemId getId, int expectedKeygroupCount, AllKeygroupsCompletion completion)
    {
        // The number of records is unknown until decoded, like the repeated-record REPLYs of &18/&19
        // (ADR-AKM-001, DEC-AKM-014): refused while the port's checksum mode is unknown, for every item
        // consumed this way, not just the specific ones DEC-AKM-014 first named.
        CommandOptions options;
        options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
        session.submit(makeRequest(getId, NO_VALUES, std::move(options)),
                       [getId, expectedKeygroupCount, completion = std::move(completion)](const CommandResult& outcome) {
                           AllKeygroupsResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto records = decodeRepeatedReply(getId, rep->data);
                               if (records && records->size() == static_cast<std::size_t>(expectedKeygroupCount))
                                   result.values = records;
                           }
                           if (completion)
                               completion(result);
                       });
    }
}
