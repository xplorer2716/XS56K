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
#include "akm/MidiConfig.hpp"

#include <utility>

#include "akm/ItemRequest.hpp"

namespace akm
{
    // Every value below is checked against the catalogue's own range for its item (`items.json`), as `makeRequest`
    // does for every item: out of range, it is refused (`ArgumentOutOfRange`) and nothing is sent. [RQ-AKM-078]

    void setProgramChangeEnabled(Session& session, bool enabled, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::MidiProgramChangeEnable, {enabled ? 1 : 0}), std::move(completion));
    }

    void setMultiSelect(Session& session, MultiSelectMode mode, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::MidiMultiSelect, {static_cast<std::int64_t>(mode)}), std::move(completion));
    }

    void setMultiSelectChannel(Session& session, int channel, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::MidiMultiSelectChannel, {channel}), std::move(completion));
    }

    void setExternalApmController(Session& session, int controller, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::MidiExternalApmController, {controller}), std::move(completion));
    }

    void setAftertouch(Session& session, AftertouchType type, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::MidiAftertouch, {static_cast<std::int64_t>(type)}), std::move(completion));
    }
}
