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
#include "akm/FrontPanel.hpp"

#include <memory>
#include <utility>

#include "akm/ItemRequest.hpp"

namespace akm
{
    namespace
    {
        // Table 31 lists every keycode of Table 30's range `&40`-`&6B` (64-107) but one.
        constexpr int KEYCODE_FIRST = 0x40;
        constexpr int KEYCODE_LAST = 0x6B;
        constexpr int KEYCODE_UNLISTED = 0x66;

        // The request for `id` with `key` as its keycode, or a request that carries the refusal when `key` is not a
        // key of Table 31 — the catalogue's own range check cannot see that `&66` is not one.
        CommandRequest keyRequest(ItemId id, FrontPanelKey key)
        {
            if (frontPanelKeyFromCode(static_cast<int>(key)))
                return makeRequest(id, {static_cast<std::int64_t>(key)});
            const ItemDescriptor& item = descriptor(id);
            CommandRequest request;
            request.command.section = item.section;
            request.command.item = item.item;
            request.refusal = RefusalReason::ArgumentOutOfRange;
            return request;
        }
    }

    std::optional<FrontPanelKey> frontPanelKeyFromCode(int code)
    {
        if (code < KEYCODE_FIRST || code > KEYCODE_LAST || code == KEYCODE_UNLISTED)
            return std::nullopt;
        return static_cast<FrontPanelKey>(code);
    }

    void holdKey(Session& session, FrontPanelKey key, CommandCompletion completion)
    {
        session.submit(keyRequest(ItemId::FrontPanelKeyHold, key), std::move(completion));
    }

    void releaseKey(Session& session, FrontPanelKey key, CommandCompletion completion)
    {
        session.submit(keyRequest(ItemId::FrontPanelKeyRelease, key), std::move(completion));
    }

    void moveDataWheel(Session& session, DataWheelDirection direction, int clicks, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::FrontPanelDataWheel, {static_cast<std::int64_t>(direction), clicks}),
                       std::move(completion));
    }

    void sendAsciiKey(Session& session, int ascii, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::FrontPanelAsciiKey, {ascii}), std::move(completion));
    }

    void pressKey(Session& session, FrontPanelKey key, KeyPressCompletion completion)
    {
        // Both completions run on the session's thread, the hold's before the release's, so the result needs no lock.
        auto result = std::make_shared<KeyPressResult>();
        holdKey(session, key, [result](const CommandResult& outcome) { result->hold = outcome; });
        releaseKey(session, key, [result, completion = std::move(completion)](const CommandResult& outcome) {
            result->release = outcome;
            if (completion)
                completion(*result);
        });
    }
}
