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

#include "akm/Command.hpp"
#include "akm/CommandResult.hpp"
#include "akm/Confirmation.hpp"
#include "akm/Scheduler.hpp"
#include "akm/SessionConfig.hpp"

namespace akm
{
    /// Which DeviceID a command carries, and which one may confirm it. [RQ-AKM-007, RQ-AKM-012]
    enum class Addressing
    {
        /// The session's bound target; only a confirmation carrying that DeviceID matches, since a
        /// sampler confirms with its own (observed under RQ-AKM-017).
        BoundTarget,
        /// DeviceID 0: every sampler on the chain executes the command and answers with its own
        /// DeviceID (spec p. 6), so any DeviceID matches. What discovery is sent with.
        Broadcast,
    };

    /// What the sampler is expected to answer, which decides whether a command may be sent while the
    /// port's checksum mode is unknown. [RQ-AKM-041, ADR-AKM-001 (DEC-AKM-009, DEC-AKM-011)]
    enum class ExpectedReply
    {
        /// A DONE, which carries no data, or a REPLY the codec delimits whatever the mode (the Echo of
        /// §00/&06): the command may be sent in any mode.
        Delimited,
        /// A REPLY whose data the codec cannot tell from a checksum without knowing the mode: the
        /// session refuses the command with `RefusalReason::ChecksumModeUnknown` until the mode is known.
        NeedsKnownChecksumMode,
    };

    /// Reported for each confirmation a command with a collection window collects, on the session's
    /// thread, in arrival order. [RQ-AKM-012]
    using ConfirmationObserver = std::function<void(const Confirmation&)>;

    /// What the session needs to know about a command beyond its bytes — what the protocol does not
    /// carry and the session cannot read back. The Items layer fills it per spec item; every field has a
    /// default that suits a plain Set addressed to the bound target.
    /// [RQ-AKM-010, RQ-AKM-011, RQ-AKM-012, RQ-AKM-013, RQ-AKM-041, ADR-AKM-001 (DEC-AKM-011)]
    struct CommandOptions
    {
        /// Replaces the session's command timeout for this command. [RQ-AKM-010]
        std::optional<Scheduler::Clock::duration> timeout{};
        /// Replaces the session's maximum total wait for this command. [RQ-AKM-010]
        std::optional<Scheduler::Clock::duration> maxTotalWait{};
        Addressing addressing = Addressing::BoundTarget;
        ExpectedReply expectedReply = ExpectedReply::Delimited;
        /// Set by the checksum-mode command (§00/&04): its frame carries a checksum whatever the mode in
        /// force, its own confirmations are decoded in mode Unknown — the sampler's OK follows the old
        /// mode and its DONE the new one — and the DONE switches the port to this value, on to `true` and
        /// off to `false`. A failure or a timeout leaves the mode unknown. [RQ-AKM-013, RQ-AKM-041]
        std::optional<bool> checksumModeAfterDone{};
        /// Set by the Still Alive command (§00/&07): the DONE starts or stops treating a received
        /// `F0 F7` as a reason to restart the pending command's timeout. [RQ-AKM-011, RQ-AKM-014]
        std::optional<bool> stillAliveAfterDone{};
        /// Set by the primitives of section 00 and by the opening: the session remembers that it tried to change
        /// this setting, so that a close puts it back to its documented default. A command that changes a setting
        /// without saying so is not remembered. [RQ-AKM-042]
        std::optional<SamplerSetting> changesSetting{};
        /// Discovery: instead of completing on the first DONE, the command collects every matching
        /// confirmation for this long, reports each to `onConfirmation`, and then completes as `Done`
        /// — empty window included, which is not an error. [RQ-AKM-012]
        std::optional<Scheduler::Clock::duration> collectionWindow{};
        ConfirmationObserver onConfirmation{};
    };

    /// One command and everything the session needs to run it. [ADR-AKM-001 (DEC-AKM-011, DEC-AKM-012)]
    struct CommandRequest
    {
        Command command{};
        CommandOptions options{};
        /// Set by whoever built the request when it cannot be sent — the catalogue found an argument out of
        /// range. The session then sends nothing and completes it as `Refused` like any other refusal:
        /// on its own thread, in its turn in the queue, and cancelling the rest of its sequence.
        /// [RQ-AKM-001, RQ-AKM-014, RQ-AKM-015, RQ-AKM-043]
        std::optional<RefusalReason> refusal{};
    };
}
