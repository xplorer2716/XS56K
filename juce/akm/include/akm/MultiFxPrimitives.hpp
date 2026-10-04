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

#include <functional>
#include <optional>

#include "akm/CommandResult.hpp"
#include "akm/Session.hpp"

namespace akm
{
    // The Multi FX primitives of section 12 (spec Tables 22-25, Figure 2), on a session: thin typed wrappers over
    // the catalogue's records (ADR-AKM-001, DEC-AKM-003, DEC-AKM-012). The effects belong to the current multi
    // (§0C): the items act on it. The hardware is presented as numbered effects channels, each with numbered
    // modules, all zero-based; the layout has to be read (`getFxCard`, `getFxChannelCount`, `getFxModuleCount`)
    // before anything is changed. Each primitive returns at once and reports on the session's thread, like
    // `Session::submit`. The session must outlive every call. [RQ-AKM-099]

    /// The FX board the sampler reports (§12/&01, Table 23): 0 none, 1 the EB20.
    enum class FxCard
    {
        None = 0,
        Eb20 = 1,
    };

    /// `card` is empty when the command did not complete on a REPLY of the length the catalogue gives it, or when the
    /// code is none of the two the spec names; `outcome` is the result of the command. [RQ-AKM-099]
    struct FxCardResult
    {
        std::optional<FxCard> card{};
        CommandResult outcome{};
    };
    using FxCardCompletion = std::function<void(const FxCardResult&)>;

    /// Gets whether an FX card is installed (§12/&01). [RQ-AKM-099]
    void getFxCard(Session& session, FxCardCompletion completion);

    /// `count` is empty when the command did not complete on a REPLY of the length the catalogue gives it;
    /// `outcome` is the result of the command. [RQ-AKM-099]
    struct FxCountResult
    {
        std::optional<int> count{};
        CommandResult outcome{};
    };
    using FxCountCompletion = std::function<void(const FxCountResult&)>;

    /// Gets the number of FX channels (§12/&10). [RQ-AKM-099]
    void getFxChannelCount(Session& session, FxCountCompletion completion);

    /// Gets the number of FX modules of the zero-based `channel` (§12/&11); a channel outside 0-127 is refused as
    /// `ArgumentOutOfRange` without sending, and the sampler's ERROR is reported unchanged for a channel it does not
    /// have. [RQ-AKM-099]
    void getFxModuleCount(Session& session, int channel, FxCountCompletion completion);
}
