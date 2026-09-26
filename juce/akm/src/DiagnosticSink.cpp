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
#include "akm/DiagnosticSink.hpp"

namespace akm
{
    namespace
    {
        constexpr std::string_view UNKNOWN_KIND_TEXT = "unknown diagnostic";
    }

    std::string_view describe(DiagnosticKind kind)
    {
        switch (kind)
        {
            case DiagnosticKind::RejectedMessage:
                return "rejected message";
            case DiagnosticKind::UnsolicitedConfirmation:
                return "unsolicited confirmation: it matches no pending command";
            case DiagnosticKind::LateErrorAfterReply:
                return "late ERROR: the command it names was already completed by a REPLY";
            case DiagnosticKind::ChecksumModeChanged:
                return "checksum mode changed";
        }
        return UNKNOWN_KIND_TEXT;
    }
}
