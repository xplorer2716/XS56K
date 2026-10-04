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
#include "akm/SceneListPrimitives.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>

#include "akm/ItemRequest.hpp"

namespace akm
{
    namespace
    {
        constexpr std::span<const std::int64_t> NO_VALUES{};
        // A zero-based index is split into two 7-bit data bytes, most significant first (spec pp. 8-9's
        // compound word), the same convention SongPrimitives.cpp uses for &06/&11.
        constexpr std::int64_t DATA_BYTE_BASE = 128;

        CommandOptions nameReplyOptions()
        {
            CommandOptions options;
            options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
            return options;
        }

        // A REPLY of two data bytes read as one 14-bit number, or empty when the outcome is not such a REPLY.
        std::optional<int> decodeWordReply(ItemId id, const CommandResult& outcome)
        {
            if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
            {
                const auto values = decodeReply(id, rep->data);
                if (values && values->size() == 2)
                    return static_cast<int>((*values)[0] * DATA_BYTE_BASE + (*values)[1]);
            }
            return std::nullopt;
        }

        std::optional<std::string> decodeNameReply(ItemId id, const CommandResult& outcome)
        {
            if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                return decodeStringReply(id, rep->data);
            return std::nullopt;
        }
    }

    void selectSceneListByName(Session& session, std::string_view name, CommandCompletion completion)
    {
        session.submit(makeStringRequest(ItemId::SceneListSelectByName, name), std::move(completion));
    }

    void selectSceneListByIndex(Session& session, int index, CommandCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(index) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(index) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::SceneListSelectByIndex, {msb, lsb}), std::move(completion));
    }

    void deleteCurrentSceneList(Session& session, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::SceneListDeleteCurrent, NO_VALUES), std::move(completion));
    }

    void renameCurrentSceneList(Session& session, std::string_view name, CommandCompletion completion)
    {
        session.submit(makeStringRequest(ItemId::SceneListRenameCurrent, name), std::move(completion));
    }

    void getSceneListCount(Session& session, SceneListCountCompletion completion)
    {
        session.submit(makeRequest(ItemId::SceneListGetCount, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           if (completion)
                               completion({decodeWordReply(ItemId::SceneListGetCount, outcome), outcome});
                       });
    }

    void getCurrentSceneListIndex(Session& session, SceneListIndexCompletion completion)
    {
        session.submit(makeRequest(ItemId::SceneListGetCurrentIndex, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           if (completion)
                               completion({decodeWordReply(ItemId::SceneListGetCurrentIndex, outcome), outcome});
                       });
    }

    void getSceneListNameByIndex(Session& session, int index, SceneListNameCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(index) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(index) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::SceneListGetNameByIndex, {msb, lsb}, nameReplyOptions()),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           if (completion)
                               completion({decodeNameReply(ItemId::SceneListGetNameByIndex, outcome), outcome});
                       });
    }

    void getCurrentSceneListName(Session& session, SceneListNameCompletion completion)
    {
        session.submit(makeRequest(ItemId::SceneListGetCurrentName, NO_VALUES, nameReplyOptions()),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           if (completion)
                               completion({decodeNameReply(ItemId::SceneListGetCurrentName, outcome), outcome});
                       });
    }
}
