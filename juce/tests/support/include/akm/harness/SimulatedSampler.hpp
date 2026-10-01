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

#include <algorithm>
#include <array>
#include <chrono>
#include <compare>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>
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

    /// An item the sampler refuses: it answers OK, then an ERROR with this number, and does not execute the
    /// command. What an older OS does with an item it does not have (ERROR 0), or a busy one with another.
    /// [RQ-AKM-016, RQ-AKM-044]
    struct ItemError
    {
        std::uint8_t section = 0;
        std::uint8_t item = 0;
        std::uint16_t number = 0;
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
        /// Items answered with an ERROR instead of being executed.
        std::vector<ItemError> itemErrors{};
        /// While Still Alive is on and a reply is delayed, an `F0 F7` this often (spec: about every second).
        Scheduler::Clock::duration stillAliveInterval = std::chrono::seconds(1);
        ConfirmationDeviceId confirmationDeviceId = ConfirmationDeviceId::Own;
        /// Whether the DONE of the command that switches checksums on or off already follows the new
        /// mode. The S5000 (OS 2.14) does, and that is the default: its OK, sent before the command runs,
        /// follows the previous mode (first-contact probe, TASK-AKM-012). Set to false to model a sampler that
        /// confirms in the previous mode.
        bool checksumChangeAppliesToOwnConfirmation = true;
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

    /// Orders the (Set item code, selector bytes) keys of `ProgramRecord::parameters`,
    /// `KeygroupRecord::parameters`/`zoneParameters` and `SampleRecord::parameters`, in place of the
    /// default `std::less<std::pair<std::uint8_t, std::vector<std::uint8_t>>>`: GCC 11's Release build
    /// (`-O2`/`-O3`) synthesizes that pair's `<=>` through `std::lexicographical_compare_three_way` on
    /// the vector member and false-positives `-Wstringop-overread` on it (`-Werror`; never seen on
    /// Debug or on MSVC/Clang) — observed on the real CI, not reproduced locally. A plain boolean `<`
    /// comparator sidesteps that code path entirely while sorting identically.
    struct ParameterKeyLess
    {
        [[nodiscard]] bool operator()(const std::pair<std::uint8_t, std::vector<std::uint8_t>>& a,
                                      const std::pair<std::uint8_t, std::vector<std::uint8_t>>& b) const
        {
            if (a.first != b.first)
                return a.first < b.first;
            return std::lexicographical_compare(a.second.begin(), a.second.end(), b.second.begin(), b.second.end());
        }
    };

    /// One keygroup of a program (§08, spec Tables 11-12): its General Options, Pitch/Amp, Filter, and
    /// three envelope groups (RQ-AKM-030), stored the same generic way as `ProgramRecord::parameters` —
    /// keyed by (the group's Set item code, the selector bytes a multi-instance item carries, e.g. which
    /// Aux Rate), holding the value bytes; read back by the paired Get item. A newly added keygroup
    /// starts with none set (a Get reads back width-many zero bytes, as `ProgramRecord::parameters`
    /// already does for a program's own groups). [RQ-AKM-030]
    struct KeygroupRecord
    {
        std::map<std::pair<std::uint8_t, std::vector<std::uint8_t>>, std::vector<std::uint8_t>, ParameterKeyLess> parameters;
        /// The §06 zone parameters (RQ-AKM-034), stored the same generic way but kept in a map of its
        /// own: §06 and §08 item codes overlap (both have a &04, for instance), so a shared map would
        /// collide. Keyed by (the group's Set item code, the zone number byte 0-4), holding the value
        /// bytes; read back by the paired Get item. [TASK-AKM-035]
        std::map<std::pair<std::uint8_t, std::vector<std::uint8_t>>, std::vector<std::uint8_t>, ParameterKeyLess> zoneParameters;
    };

    /// One program in the sampler's memory (§0A, spec Tables 13-14): only what TASK-AKM-015's lifecycle
    /// primitives set or read. Where the spec is silent, the model chooses: a plain Create (`&02`) gives one
    /// keygroup; creating a name that already exists among the sampler's programs fails as `COULD_NOT_CREATE`
    /// (`&05`), the spec's own text for that error not saying when it applies. [RQ-AKM-021, RQ-AKM-022]
    struct ProgramRecord
    {
        std::string name;
        int keygroupCount = 1;
        bool crossfade = false;
        /// The front-panel "Program Number" (§0A/&0A, 1-128; the wire carries it minus one, Table 13
        /// footnote a), or empty when it is off. [RQ-AKM-022]
        std::optional<int> frontPanelNumber{};
        /// The Output, MIDI/Tune, Pitch Bend, LFO and Keygroup Modulation Sources parameters
        /// (RQ-AKM-024): keyed by (the group's Set item code, the selector bytes a multi-instance item
        /// carries, e.g. which LFO), holding the value bytes; read back by the paired Get item. Not
        /// compared by `ProgramRecord`'s own `==` (only the fields the earlier lifecycle tests need are).
        std::map<std::pair<std::uint8_t, std::vector<std::uint8_t>>, std::vector<std::uint8_t>, ParameterKeyLess> parameters;
        /// One entry per keygroup, `keygroupCount` long, in keygroup order starting at 1 (§08, RQ-AKM-030).
        /// Kept in sync with `keygroupCount` by `&0B`/`&0C` (add/delete keygroups); not compared by
        /// `ProgramRecord`'s own `==`.
        std::vector<KeygroupRecord> keygroups{KeygroupRecord{}};
    };

    inline bool operator==(const ProgramRecord& a, const ProgramRecord& b)
    {
        return a.name == b.name && a.keygroupCount == b.keygroupCount && a.crossfade == b.crossfade;
    }

    /// The §02 system setup this model holds beyond the OS version (spec Tables 6-7), one field per item
    /// pair as PLAN-AKM-006's tasks add them. Unlike the §00 settings it survives `powerCycle()`: the
    /// real sampler keeps its name across a power cycle. [RQ-AKM-052]
    struct SystemSetupState
    {
        /// What a sampler carries until its user changes it (Table 6, footnote b).
        std::string name = "AKAI S5000";
        /// &04's byte: 0 = S5000, 1 = S6000 (any other value is for a test that wants a malformed REPLY).
        std::uint8_t model = 0;
        /// The Wave memory (&30, &33, &34) and the MPKS memory (&31). The Wave percentage is derived from
        /// the two byte counts, as it is on the real sampler; the defaults (64 MiB all free) are arbitrary,
        /// the spec giving none. [RQ-AKM-053]
        std::uint32_t waveTotalBytes = 64u * 1024 * 1024;
        std::uint32_t waveFreeBytes = 64u * 1024 * 1024;
        std::uint8_t mpksFreePercent = 100;
        /// The clock (&05/&06, RQ-AKM-054) as the eight data bytes of the wire — year MSB and LSB, month, day of
        /// month, day of week, hours, minutes, seconds — set to Saturday 1 January 2000, 00:00:00 (the spec
        /// gives no default; 2000 is MSB 15, LSB 80).
        std::array<std::uint8_t, 8> clock{15, 80, 1, 1, 7, 0, 0, 0};
        /// The Play Mode (&10/&20, RQ-AKM-055): 0 = Multi, 1 = Program, 2 = Sample, 3 = Muted; Program until a
        /// Set (the spec gives no default). Any other byte is for a test that wants a malformed REPLY.
        std::uint8_t playMode = 1;
        /// The front-panel lock (&11/&21): 0 = normal, 1 = locked.
        std::uint8_t frontPanelLock = 0;
    };

    /// One sample in the sampler's memory (§0E, spec Tables 18-19): only what TASK-AKM-040's lifecycle
    /// primitives set or read. Unlike a program, this model has no "create" for a sample: §0E has no
    /// such item (a sample only exists once `setSampleNames` — or a later item of this lot — puts it
    /// there), matching the spec, which only lets samples be loaded or recorded, not created blank.
    /// [RQ-AKM-045]
    struct SampleRecord
    {
        std::string name;
        /// The settable parameters of RQ-AKM-048 (start/end position, original pitch, semitone/fine
        /// tune, playback mode, loop start/end): keyed by (the group's Set item code, an always-empty
        /// selector — these items take none), holding the value bytes; read back by the paired Get
        /// item. Stored the same generic way as `ProgramRecord::parameters`. [TASK-AKM-043]
        std::map<std::pair<std::uint8_t, std::vector<std::uint8_t>>, std::vector<std::uint8_t>, ParameterKeyLess> parameters;
        /// The read-only parameters of RQ-AKM-049 (§0E/&30-&33): no Set item exists for them, a real
        /// sample's audio data determines them, so `setSampleAttributes` is the only way to give them a
        /// value in this model. Defaults (a mono RAM sample, zero length and rate) are arbitrary, the
        /// spec giving none. [TASK-AKM-044]
        std::uint8_t type = 0;      ///< 0 = RAM, 1 = VIRTUAL
        std::uint8_t channels = 1;  ///< 1 = mono, 2 = stereo
        std::uint32_t length = 0;
        std::uint32_t rate = 0;
    };

    /// One port of a sampler, modelled on the spec: it decodes the frames it is sent, answers those that are
    /// addressed to it (DeviceID 0 on either side matches everything), keeps its §00 state across sessions,
    /// applies or refuses checksums, and answers OK / DONE / REPLY / ERROR. §00, the two version items of §02
    /// (RQ-AKM-044) and the program lifecycle and general-information items of §0A (RQ-AKM-021, RQ-AKM-023)
    /// are modelled; any other section or item answers ERROR 0 (not supported) until the lots that need it
    /// (the rest of FTR-AKM-002, FTR-AKM-003, FTR-AKM-004) add theirs.
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

        /// Seeds the sampler's sample memory (§0E) by name, one `SampleRecord` each, current-sample
        /// selection reset. The same list §06/&01 (Set Zone Sample) checks a name against — assigning a
        /// name not here fails with ERROR 04, the spec's "requested item not found" (RQ-AKM-035); §0E's
        /// own lifecycle primitives (RQ-AKM-045) act on this same list, so a test can seed a sample once
        /// and use it for both zone assignment and the sample lifecycle. Empty by default: a test that
        /// wants a successful assignment or selection must call this first, like `setBehaviour`.
        void setSampleNames(std::vector<std::string> names);

        /// Seeds the read-only attributes of the sample at `index` (§0E/&30-&33, RQ-AKM-049) — no Set
        /// item exists for them, so a test sets them directly, like `setSampleNames` itself. A no-op
        /// when `index` names no sample.
        void setSampleAttributes(std::size_t index, std::uint8_t type, std::uint8_t channels,
                                 std::uint32_t length, std::uint32_t rate);

        /// Sets what &04 reports (RQ-AKM-053): 0 = S5000, 1 = S6000, any other byte a REPLY no model has.
        void setModel(std::uint8_t model);

        /// Sets the memory &30, &31, &33 and &34 report (RQ-AKM-053): the Wave memory's total and free
        /// bytes — its free percentage follows from them — and the free percentage of the MPKS memory, which
        /// is given as is, whatever its value.
        void setMemory(std::uint32_t waveTotalBytes, std::uint32_t waveFreeBytes, std::uint8_t mpksFreePercent);

        /// Sets what &20 reports (RQ-AKM-055): 0-3 are the four Play Modes, any other byte a REPLY no mode has.
        void setPlayMode(std::uint8_t playMode);

        /// Sets what &21 reports (RQ-AKM-055): 0 = normal, 1 = locked, any other byte a REPLY no state has.
        void setFrontPanelLock(std::uint8_t lock);

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
        // §02 system setup beyond the OS version (RQ-AKM-052): not touched by powerCycle().
        SystemSetupState _system;
        std::vector<std::vector<std::uint8_t>> _received;
        std::vector<AcceptedCommand> _accepted;

        // §0A program memory: not touched by powerCycle() (real sampler memory, unlike the §00 settings).
        std::vector<ProgramRecord> _programs;
        std::optional<std::size_t> _currentProgram;
        // §08 current keygroup of the current program (RQ-AKM-028): the wire value, 1-99 or 0 for "all";
        // reset whenever the current program changes, since a keygroup number is only meaningful within
        // one program. A newly current program defaults to keygroup 1 (the spec does not say; observed on
        // the real sampler when TASK-AKM-026's real-sampler test runs).
        std::optional<int> _currentKeygroup;
        // §0E sample memory (RQ-AKM-045), seeded by `setSampleNames`: not touched by powerCycle(). Unlike
        // §0A's current program, §0E's current sample has no dependent selection to reset alongside it.
        std::vector<SampleRecord> _samples;
        std::optional<std::size_t> _currentSample;
    };
}
