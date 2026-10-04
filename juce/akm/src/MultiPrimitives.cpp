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
#include "akm/MultiPrimitives.hpp"

#include <cstdint>
#include <span>
#include <utility>
#include <variant>

#include "akm/ItemRequest.hpp"

namespace akm
{
    namespace
    {
        constexpr std::span<const std::int64_t> NO_VALUES{};
        // A zero-based index is split into two 7-bit data bytes, most significant first (spec pp. 8-9's compound
        // word), the same convention SamplePrimitives.cpp and SongPrimitives.cpp use.
        constexpr std::int64_t DATA_BYTE_BASE = 128;
    }

    void setNewMultiPartCount(Session& session, MultiPartCount partCount, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::MultiSetPartCount, {static_cast<std::int64_t>(partCount)}), std::move(completion));
    }

    void createMulti(Session& session, std::string_view name, CommandCompletion completion)
    {
        session.submit(makeStringRequest(ItemId::MultiCreate, name), std::move(completion));
    }

    void selectMultiByName(Session& session, std::string_view name, CommandCompletion completion)
    {
        session.submit(makeStringRequest(ItemId::MultiSelectByName, name), std::move(completion));
    }

    void selectMultiByIndex(Session& session, int index, CommandCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(index) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(index) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::MultiSelectByIndex, {msb, lsb}), std::move(completion));
    }

    void deleteCurrentMulti(Session& session, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::MultiDeleteCurrent, NO_VALUES), std::move(completion));
    }

    void getCurrentMultiIndex(Session& session, MultiIndexCompletion completion)
    {
        session.submit(makeRequest(ItemId::MultiGetCurrentIndex, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           MultiIndexResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::MultiGetCurrentIndex, rep->data);
                               if (values && values->size() == 2)
                                   result.index = static_cast<int>((*values)[0] * DATA_BYTE_BASE + (*values)[1]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getCurrentMultiName(Session& session, MultiNameCompletion completion)
    {
        CommandOptions options;
        options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
        session.submit(makeRequest(ItemId::MultiGetCurrentName, NO_VALUES, std::move(options)),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           MultiNameResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                               result.name = decodeStringReply(ItemId::MultiGetCurrentName, rep->data);
                           if (completion)
                               completion(result);
                       });
    }
}
