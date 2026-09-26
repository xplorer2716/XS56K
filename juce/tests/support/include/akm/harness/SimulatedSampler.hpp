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

#include <chrono>
#include <compare>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

#include "akm/Scheduler.hpp"

namespace akm::harness
{
    /// An OS version, for the items that exist only since one. [RQ-AKM-013]
    struct OsVersion
    {
        int major = 2;
        int minor = 10;

        friend auto operator<=>(const OsVersion&, const OsVersion&) = default;
    };

    /// What the sampler was configured with at power-on.
    struct SamplerConfig
    {
        std::uint8_t deviceId = 0;
        OsVersion osVersion{2, 10};
    };

    /// The §00 settings of one port, which the protocol cannot read back (§00 has no Get item) and this
    /// model exposes so that the tests can. The defaults are the spec's (notification and synchronisation
    /// on, checksums off) and, where it is silent, an assumption.
    struct SamplerSettings
    {
        bool notification = true;
        bool checksum = false;
        bool syncLcd = true;
        bool autoScreenUpdate = false;
        bool stillAlive = false;

        friend bool operator==(const SamplerSettings&, const SamplerSettings&) = default;
    };

    /// Which DeviceID a confirmation carries: the spec says both that it is the sampler's own (so that a
    /// Query to DeviceID 0 reveals each sampler's) and that the first bytes are those of the command.
    /// Which one the S5000 does is one of RQ-AKM-017's questions, so both are modelled.
    enum class ConfirmationDeviceId
    {
        Own,
        Echoed,
    };

    /// What a real bus does badly, switchable per sampler.
    struct SamplerBehaviour
    {
        /// Executes the commands and answers none.
        bool silent = false;
        /// Time between the command and its confirmations, on the scheduler the simulation is given.
        Scheduler::Clock::duration replyDelay = Scheduler::Clock::duration::zero();
        /// Each confirmation is sent this many times, consecutively.
        int timesEachReply = 1;
        /// Raw messages sent before the confirmations of every command: foreign SysEx, malformed frames,
        /// `F0 F7`.
        std::vector<std::vector<std::uint8_t>> junkBeforeReply{};
        /// After a REPLY, an ERROR with this number (spec p. 6: possible but unlikely).
        std::optional<std::uint16_t> errorAfterReply{};
        /// While Still Alive is on and a reply is delayed, an `F0 F7` this often (spec: about every second).
        Scheduler::Clock::duration stillAliveInterval = std::chrono::seconds(1);
        ConfirmationDeviceId confirmationDeviceId = ConfirmationDeviceId::Own;
        /// Whether the DONE of the command that switches checksums on or off already follows the new
        /// mode (the spec does not say); the OK never does, being sent before the command runs.
        bool checksumChangeAppliesToOwnConfirmation = false;
    };

    /// A command the sampler accepted: addressed to it, well framed, with a valid checksum when checksums
    /// are on. Whether the item was supported is another matter (it answers ERROR 0 when it is not).
    struct AcceptedCommand
    {
        std::uint8_t deviceId = 0;  ///< as written in the message, not the sampler's
        std::vector<std::uint8_t> userRefs;
        std::uint8_t section = 0;
        std::uint8_t item = 0;
        std::vector<std::uint8_t> data;  ///< without its checksum
    };

    /// One port of a sampler, modelled on the spec: it decodes the frames it is sent, answers those that are
    /// addressed to it (DeviceID 0 on either side matches everything), keeps its §00 state across sessions,
    /// applies or refuses checksums, and answers OK / DONE / REPLY / ERROR. Only §00 is modelled; any other
    /// section answers ERROR 0 (not supported) until the lots that need it (FTR-AKM-002 to 004) add theirs.
    /// The frames it builds are written out from the spec, not with the codec under test.
    /// [RQ-AKM-016, ADR-AKM-001 (DEC-AKM-008)]
    ///
    /// Thread-safe: the knobs and the state may be read and changed while frames arrive.
    class SimulatedSampler
    {
    public:
        /// Sends one raw message to the host (the bus decides on which thread it arrives).
        using Emit = std::function<void(std::vector<std::uint8_t>)>;

        SimulatedSampler(SamplerConfig config, Scheduler& scheduler, Emit emit);

        void setBehaviour(SamplerBehaviour behaviour);
        [[nodiscard]] SamplerBehaviour behaviour() const;

        [[nodiscard]] SamplerSettings settings() const;
        /// Power-off and on: the §00 settings go back to their defaults.
        void powerCycle();

        /// Every message that reached the sampler, addressed to it or not, in order.
        [[nodiscard]] std::vector<std::vector<std::uint8_t>> receivedFrames() const;
        [[nodiscard]] std::vector<AcceptedCommand> acceptedCommands() const;

        /// The sampler's MIDI input: called by the bus with each SysEx message the host sent.
        void receive(std::span<const std::uint8_t> message);

    private:
        /// Decodes and executes one message, and returns the confirmations it causes; empty when the sampler
        /// ignores the message. Called with the lock held.
        [[nodiscard]] std::vector<std::vector<std::uint8_t>> process(std::span<const std::uint8_t> message);

        const SamplerConfig _config;
        Scheduler& _scheduler;
        const Emit _emit;

        mutable std::mutex _mutex;
        SamplerBehaviour _behaviour;
        SamplerSettings _settings;
        std::vector<std::vector<std::uint8_t>> _received;
        std::vector<AcceptedCommand> _accepted;
    };
}
