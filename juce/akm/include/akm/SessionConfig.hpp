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
#include <optional>
#include <string_view>
#include <vector>

#include "akm/CommandResult.hpp"

namespace akm
{
    /// What opening a session does with one section 00 setting: switch it on, off, or leave it alone. A setting
    /// left alone is never sent. [RQ-AKM-040, ADR-AKM-001 (DEC-AKM-007)]
    enum class SettingChoice
    {
        Unchanged,
        On,
        Off,
    };

    /// What a session is opened with. The layer neither stores nor edits it: the application's settings do, and
    /// hand the values over. The defaults are the provisional ones of ADR-AKM-001 (DEC-AKM-007), each with its
    /// reason there. [RQ-AKM-039, RQ-AKM-040]
    struct SessionConfig
    {
        /// The sampler's own DeviceID, 0 to 31, as set on the sampler (it cannot be read or changed by SysEx);
        /// 0 is its default. Every command of the session is addressed to it, and only its confirmations match.
        std::uint32_t targetDeviceId = 0;
        /// The port's checksum mode: always established, first, because the session must know it (§00 has no
        /// Get) to read a REPLY whose length varies.
        bool checksums = false;
        SettingChoice notification = SettingChoice::Unchanged;
        SettingChoice syncLcd = SettingChoice::Off;
        SettingChoice autoScreenUpdate = SettingChoice::Unchanged;
        SettingChoice stillAlive = SettingChoice::On;
    };

    /// The section 00 settings of a port, as the opening and the closing name them. [RQ-AKM-040, RQ-AKM-042]
    enum class SamplerSetting
    {
        Checksums,
        Notification,
        SyncLcd,
        AutoScreenUpdate,
        StillAlive,
    };

    /// How an open ended. The first two are a session ready for use; the others are failures, and a session that
    /// failed to open refuses the application's commands. [RQ-AKM-039, RQ-AKM-040]
    enum class OpenStatus
    {
        Ready,              ///< every configured setting was accepted
        ReadyDegraded,      ///< ready, but the sampler answered ERROR 00 to some optional settings: `unsupported`
        NoSamplerAtTarget,  ///< the target's DeviceID is not among those that answered the discovery
        AmbiguousSamplers,  ///< several samplers answered, and the target is 0 or one of them has DeviceID 0
        InvalidDeviceId,    ///< the target is above 31: nothing was sent
        DiscoveryFailed,    ///< the discovery command itself did not complete
        SettingFailed,      ///< a setting failed or timed out: `failedSetting` and `failedResult`
        AlreadyOpen,        ///< the session is opening or open already: nothing was changed
        Cancelled,          ///< the session was closed while it was opening
    };

    struct OpenResult
    {
        OpenStatus status = OpenStatus::Ready;
        std::uint32_t targetDeviceId = 0;
        /// The DeviceIDs that answered the discovery, ascending; what "no sampler at DeviceID N" is reported with.
        std::vector<std::uint8_t> responders;
        /// The optional settings the sampler does not have, when the session opened degraded.
        std::vector<SamplerSetting> unsupported;
        /// The setting that failed and how, when the status is `SettingFailed`.
        std::optional<SamplerSetting> failedSetting;
        CommandResult failedResult{};

        /// A session that can be used: ready, or ready with some optional settings missing.
        [[nodiscard]] bool ready() const { return status == OpenStatus::Ready || status == OpenStatus::ReadyDegraded; }
    };

    /// Where a session is in its life. A session never opened runs the application's commands, which is how the
    /// tests of the session core and the probes drive one; once `open()` is called, only an open session does.
    enum class SessionState
    {
        Unopened,
        Opening,
        Open,
        OpenFailed,
        Closing,
        Closed,
    };

    /// The value a setting has on a sampler that nobody has touched, which a closing puts back: checksums off and
    /// Sync LCD on (the spec, p. 4 and the introductions of §0A and §0E), Notification on and Still Alive off (found
    /// at the first contact with the S5000, TASK-AKM-012), Auto screen update off (assumed: the spec states no default,
    /// and the sampler's own is not observable, §00 having no Get). [RQ-AKM-042, RQ-AKM-018, RQ-AKM-017]
    [[nodiscard]] constexpr bool samplerDefault(SamplerSetting setting)
    {
        switch (setting)
        {
            case SamplerSetting::Checksums:
                return false;
            case SamplerSetting::Notification:
                return true;
            case SamplerSetting::SyncLcd:
                return true;
            case SamplerSetting::AutoScreenUpdate:
                return false;
            case SamplerSetting::StillAlive:
                return false;
        }
        return false;
    }

    /// How a close went: which settings the session put back, and which it could not, for the application to
    /// report. A setting that was refused, or that timed out, or that was not tried because the sampler had stopped
    /// answering, is in `notRestored`; a close always finishes. [RQ-AKM-042]
    struct CloseResult
    {
        std::vector<SamplerSetting> restored;
        std::vector<SamplerSetting> notRestored;

        [[nodiscard]] bool restoredAll() const { return notRestored.empty(); }
    };

    /// Short names, for the diagnostics and the logs. [RQ-AKM-039, RQ-AKM-040]
    [[nodiscard]] std::string_view describe(SamplerSetting setting);
    [[nodiscard]] std::string_view describe(OpenStatus status);
}
