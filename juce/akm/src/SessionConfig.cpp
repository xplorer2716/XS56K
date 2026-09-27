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
#include "akm/SessionConfig.hpp"

namespace akm
{
    namespace
    {
        constexpr std::string_view UNKNOWN_SETTING_TEXT = "unknown setting";
        constexpr std::string_view UNKNOWN_STATUS_TEXT = "unknown open status";
    }

    std::string_view describe(SamplerSetting setting)
    {
        switch (setting)
        {
            case SamplerSetting::Checksums:
                return "checksum mode";
            case SamplerSetting::Notification:
                return "notification";
            case SamplerSetting::SyncLcd:
                return "Sync LCD";
            case SamplerSetting::AutoScreenUpdate:
                return "Auto screen update";
            case SamplerSetting::StillAlive:
                return "Still Alive";
        }
        return UNKNOWN_SETTING_TEXT;
    }

    std::string_view describe(OpenStatus status)
    {
        switch (status)
        {
            case OpenStatus::Ready:
                return "ready";
            case OpenStatus::ReadyDegraded:
                return "ready, degraded: some optional settings are not supported by the sampler";
            case OpenStatus::NoSamplerAtTarget:
                return "no sampler at the target DeviceID";
            case OpenStatus::AmbiguousSamplers:
                return "ambiguous: several samplers answered and the target would reach more than one";
            case OpenStatus::InvalidDeviceId:
                return "invalid DeviceID: it is at most 31";
            case OpenStatus::DiscoveryFailed:
                return "the discovery did not complete";
            case OpenStatus::SettingFailed:
                return "a setting failed or timed out";
            case OpenStatus::AlreadyOpen:
                return "the session is already opening or open";
            case OpenStatus::Cancelled:
                return "cancelled: the session was closed while it was opening";
        }
        return UNKNOWN_STATUS_TEXT;
    }
}
