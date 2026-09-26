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
#include "akm/CommandResult.hpp"

namespace akm
{
    namespace
    {
        constexpr std::string_view UNKNOWN_REFUSAL_TEXT = "unknown refusal reason";
    }

    bool succeeded(const CommandResult& result)
    {
        return std::holds_alternative<Done>(result) || std::holds_alternative<Reply>(result);
    }

    std::string_view describe(RefusalReason reason)
    {
        switch (reason)
        {
            case RefusalReason::NotEncodable:
                return "not encodable: the command cannot form a legal frame";
            case RefusalReason::ChecksumModeUnknown:
                return "checksum mode unknown: this command's reply could not be delimited";
            case RefusalReason::NoTargetBound:
                return "no target bound: no sampler DeviceID has been bound to this session";
            case RefusalReason::SessionClosed:
                return "session closed";
        }
        return UNKNOWN_REFUSAL_TEXT;
    }
}
