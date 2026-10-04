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
#include "akm/SongPrimitives.hpp"

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
        // A zero-based index is split into two 7-bit data bytes, most significant first (spec pp. 8-9's
        // compound word), the same convention SamplePrimitives.cpp uses for &06/&11.
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

    void selectSongByName(Session& session, std::string_view name, CommandCompletion completion)
    {
        session.submit(makeStringRequest(ItemId::SongSelectByName, name), std::move(completion));
    }

    void selectSongByIndex(Session& session, int index, CommandCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(index) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(index) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::SongSelectByIndex, {msb, lsb}), std::move(completion));
    }

    void deleteCurrentSong(Session& session, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::SongDeleteCurrent, NO_VALUES), std::move(completion));
    }

    void renameCurrentSong(Session& session, std::string_view name, CommandCompletion completion)
    {
        session.submit(makeStringRequest(ItemId::SongRenameCurrent, name), std::move(completion));
    }

    void getSongCount(Session& session, SongCountCompletion completion)
    {
        session.submit(makeRequest(ItemId::SongGetCount, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           if (completion)
                               completion({decodeWordReply(ItemId::SongGetCount, outcome), outcome});
                       });
    }

    void getCurrentSongIndex(Session& session, SongIndexCompletion completion)
    {
        session.submit(makeRequest(ItemId::SongGetCurrentIndex, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           if (completion)
                               completion({decodeWordReply(ItemId::SongGetCurrentIndex, outcome), outcome});
                       });
    }

    void getSongNameByIndex(Session& session, int index, SongNameCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(index) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(index) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::SongGetNameByIndex, {msb, lsb}, nameReplyOptions()),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           if (completion)
                               completion({decodeNameReply(ItemId::SongGetNameByIndex, outcome), outcome});
                       });
    }

    void getCurrentSongName(Session& session, SongNameCompletion completion)
    {
        session.submit(makeRequest(ItemId::SongGetCurrentName, NO_VALUES, nameReplyOptions()),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           if (completion)
                               completion({decodeNameReply(ItemId::SongGetCurrentName, outcome), outcome});
                       });
    }
}
