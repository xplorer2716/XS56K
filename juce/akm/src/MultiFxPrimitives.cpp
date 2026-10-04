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
#include "akm/MultiFxPrimitives.hpp"

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

        // A REPLY of one data byte, or empty when the outcome is not such a REPLY.
        std::optional<std::int64_t> decodeByteReply(ItemId id, const CommandResult& outcome)
        {
            if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
            {
                const auto values = decodeReply(id, rep->data);
                if (values && values->size() == 1)
                    return values->front();
            }
            return std::nullopt;
        }

        std::optional<FxCard> cardFromCode(std::optional<std::int64_t> code)
        {
            if (code == static_cast<std::int64_t>(FxCard::None))
                return FxCard::None;
            if (code == static_cast<std::int64_t>(FxCard::Eb20))
                return FxCard::Eb20;
            return std::nullopt;
        }

        std::optional<int> countFromReply(std::optional<std::int64_t> value)
        {
            if (!value)
                return std::nullopt;
            return static_cast<int>(*value);
        }
    }

    void getFxCard(Session& session, FxCardCompletion completion)
    {
        session.submit(makeRequest(ItemId::FxGetCard, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           if (completion)
                               completion({cardFromCode(decodeByteReply(ItemId::FxGetCard, outcome)), outcome});
                       });
    }

    void getFxChannelCount(Session& session, FxCountCompletion completion)
    {
        session.submit(makeRequest(ItemId::FxGetChannelCount, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           if (completion)
                               completion({countFromReply(decodeByteReply(ItemId::FxGetChannelCount, outcome)), outcome});
                       });
    }

    void getFxModuleCount(Session& session, int channel, FxCountCompletion completion)
    {
        session.submit(makeRequest(ItemId::FxGetModuleCount, {channel}),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           if (completion)
                               completion({countFromReply(decodeByteReply(ItemId::FxGetModuleCount, outcome)), outcome});
                       });
    }
}
