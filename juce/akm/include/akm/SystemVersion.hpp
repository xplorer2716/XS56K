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

#include <cstdint>
#include <functional>
#include <optional>

#include "akm/CommandResult.hpp"
#include "akm/Session.hpp"

namespace akm
{
    /// The version of the sampler's operating system. [RQ-AKM-044]
    struct OsVersionReport
    {
        std::uint8_t major = 0;
        std::uint8_t minor = 0;
        /// Unavailable when the sampler answered the sub-version request with an ERROR or not at all; the
        /// spec says it is always zero for now (Table 6, footnote a).
        std::optional<std::uint8_t> subVersion{};
    };

    /// `version` is empty when the request for the major and minor numbers failed or its REPLY could not
    /// be read, and then nothing is assumed about the version; `outcome` is the result of that first
    /// request. [RQ-AKM-044]
    struct OsVersionResult
    {
        std::optional<OsVersionReport> version{};
        CommandResult outcome{};
    };
    using OsVersionCompletion = std::function<void(const OsVersionResult&)>;

    /// Sends Get Operating System Software Version (§02/&00) and, when it succeeded, Get the Sub-Version
    /// (§02/&01), and reports the major and minor numbers (Data1 and Data2 of the first REPLY) and the
    /// sub-version (Data1 of the second), so that an application can decide which items the connected
    /// sampler supports instead of trying them. The two requests are independent reads: a sub-version that
    /// fails does not lose the numbers already read. It works whatever the checksum mode, the catalogue
    /// giving the length of both REPLYs. The session must outlive the call. [RQ-AKM-044, RQ-AKM-041,
    /// ADR-AKM-001 (DEC-AKM-012)]
    void queryOsVersion(Session& session, OsVersionCompletion completion);
}
