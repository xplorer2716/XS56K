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

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <variant>
#include <vector>

#include "akm/CommandResult.hpp"
#include "akm/Session.hpp"

namespace akm
{
    // The primitives of section 00, SysEx Configuration (spec Table 5), on a session: thin typed
    // wrappers over the catalogue's records (ADR-AKM-001, DEC-AKM-003, DEC-AKM-012). Each returns at once
    // and reports on the session's thread, like `Session::submit`. Section 00 has no Get item: what these
    // set cannot be read back, so DONE is their proof, and the Echo is the exchange that proves the link.
    // The session must outlive every call. [RQ-AKM-012 to RQ-AKM-015]

    /// Data bytes of the Echo Message (§00/&06). [RQ-AKM-015]
    inline constexpr std::size_t ECHO_DATA_SIZE = 4;

    /// What discovery found: the distinct DeviceIDs (bits 0-4) that answered, in ascending order, and how
    /// the command ended — `Done` when its window ended, whether or not anyone answered. [RQ-AKM-012]
    struct DiscoveryResult
    {
        std::vector<std::uint8_t> deviceIds;
        CommandResult outcome{};
    };
    using DiscoveryCompletion = std::function<void(const DiscoveryResult&)>;

    /// Sends the Query (§00/&00) addressed to every sampler, collects the OK, DONE and ERROR confirmations
    /// that arrive during `window`, and reports the DeviceIDs that answered — an ERROR is still a sampler
    /// present. It is the only command that ends with its window rather than with its first DONE, and it
    /// needs no bound target. The window is a named, configurable value. [RQ-AKM-012,
    /// ADR-AKM-001 (DEC-AKM-007)]
    void discover(Session& session, DiscoveryCompletion completion,
                  Scheduler::Clock::duration window = DEFAULT_DISCOVERY_WINDOW);

    /// Sets the checksum mode of the sampler's port (§00/&04). The command carries a checksum whatever the
    /// mode the session assumes, and on DONE the session switches its own mode, so that every following
    /// command is framed accordingly; a failure leaves the mode unknown. [RQ-AKM-013,
    /// ADR-AKM-001 (DEC-AKM-009, DEC-AKM-011)]
    void setChecksumMode(Session& session, bool on, CommandCompletion completion);

    /// Notification (§00/&01), Sync LCD (§00/&03), Auto screen update (§00/&05) and Still Alive (§00/&07),
    /// each completing on DONE. Still Alive also tells the session whether to read `F0 F7` as proof that
    /// the sampler is busy. A value other than on or off cannot be expressed here; the catalogue refuses
    /// one sent through `makeRequest`. [RQ-AKM-011, RQ-AKM-014]
    void setNotification(Session& session, bool on, CommandCompletion completion);
    void setSyncLcd(Session& session, bool on, CommandCompletion completion);
    void setAutoScreenUpdate(Session& session, bool on, CommandCompletion completion);
    void setStillAlive(Session& session, bool on, CommandCompletion completion);

    /// The Echo REPLY when it is not the four bytes that were sent. [RQ-AKM-015]
    struct EchoMismatch
    {
        std::vector<std::uint8_t> sent;
        std::vector<std::uint8_t> received;

        friend bool operator==(const EchoMismatch&, const EchoMismatch&) = default;
    };

    /// How an Echo ended: the result of the command itself and, when the sampler answered with a REPLY that
    /// differs from what was sent, the mismatch that names both byte sequences. [RQ-AKM-015]
    struct EchoResult
    {
        CommandResult outcome{};
        std::optional<EchoMismatch> mismatch{};

        /// The sampler answered with a REPLY holding exactly the bytes that were sent.
        [[nodiscard]] bool succeeded() const { return !mismatch && std::holds_alternative<Reply>(outcome); }
    };
    using EchoCompletion = std::function<void(const EchoResult&)>;

    /// Sends the Echo Message (§00/&06) with four data bytes, each between 00 and 7F, and completes on
    /// its REPLY. A byte above 7F is refused without sending. [RQ-AKM-015]
    void echo(Session& session, const std::array<std::uint8_t, ECHO_DATA_SIZE>& data, EchoCompletion completion);
}
