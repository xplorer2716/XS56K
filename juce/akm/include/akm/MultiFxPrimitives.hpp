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

    // The configuration of the channels and the modules (§12/&20-&41, Tables 22-24). A channel and a module are
    // zero-based; one outside 0-127 is refused as `ArgumentOutOfRange` without sending, and the sampler's ERROR is
    // reported unchanged for one the board does not have or when no multi is current. [RQ-AKM-100]

    /// Mutes or unmutes the effects channel `channel` (§12/&20; the wire carries 0 for ON and 1 for MUTE).
    /// [RQ-AKM-100]
    void setFxChannelMute(Session& session, int channel, bool muted, CommandCompletion completion);

    /// `muted` is empty when the command did not complete on a REPLY of the length the catalogue gives it.
    /// [RQ-AKM-100]
    struct FxMuteResult
    {
        std::optional<bool> muted{};
        CommandResult outcome{};
    };
    using FxMuteCompletion = std::function<void(const FxMuteResult&)>;

    /// Gets the mute status of the effects channel `channel` (§12/&21). [RQ-AKM-100]
    void getFxChannelMute(Session& session, int channel, FxMuteCompletion completion);

    /// The module types the spec names (Table 24, §12/&30 and &31). The enumeration is a convenience, not a limit: the
    /// protocol is "as flexible and extensible as possible" (spec p. 35), so a code Table 24 does not name is read and
    /// written as it is, through a cast, and only a code outside 0-127 is refused. [RQ-AKM-100]
    enum class FxModuleType
    {
        None = 0x00,
        RingModDistortion = 0x01,
        Chorus = 0x02,
        Flange = 0x03,
        Phase = 0x04,
        RotarySpeaker = 0x05,
        FModAutopan = 0x06,
        PitchShift = 0x07,
        PitchShiftFeedback = 0x08,
        Eq = 0x09,
        MonoDelay = 0x0A,
        MonoLeftRight = 0x0B,
        MonoCrossover = 0x0C,
        StereoDelay = 0x0D,
        Reverb = 0x0E,
        OutputMix = 0x0F,
        ReverbInput = 0x10,
    };

    /// Sets the type of module `module` of channel `channel` (§12/&30). With the EB20 only modules 2 and 3 of channels 0
    /// and 1 may be changed (spec p. 35). [RQ-AKM-100]
    void setFxModuleType(Session& session, int channel, int module, FxModuleType type, CommandCompletion completion);

    /// `type` is empty when the command did not complete on a REPLY of the length the catalogue gives it; a code Table 24
    /// does not name is given unchanged. [RQ-AKM-100]
    struct FxModuleTypeResult
    {
        std::optional<FxModuleType> type{};
        CommandResult outcome{};
    };
    using FxModuleTypeCompletion = std::function<void(const FxModuleTypeResult&)>;

    /// Gets the type of module `module` of channel `channel` (§12/&31). [RQ-AKM-100]
    void getFxModuleType(Session& session, int channel, int module, FxModuleTypeCompletion completion);

    /// Enables or disables (bypasses) module `module` of channel `channel` (§12/&40). [RQ-AKM-100]
    void setFxModuleEnabled(Session& session, int channel, int module, bool enabled, CommandCompletion completion);

    /// `enabled` is empty when the command did not complete on a REPLY of the length the catalogue gives it.
    /// [RQ-AKM-100]
    struct FxEnabledResult
    {
        std::optional<bool> enabled{};
        CommandResult outcome{};
    };
    using FxEnabledCompletion = std::function<void(const FxEnabledResult&)>;

    /// Gets whether module `module` of channel `channel` is enabled (§12/&41). [RQ-AKM-100]
    void getFxModuleEnabled(Session& session, int channel, int module, FxEnabledCompletion completion);

    // The parameter values (§12/&50, &51, Table 25). Whatever the parameter, a value travels as a signed compound word:
    // a sign byte (0 positive, 1 negative) then the magnitude as a most- and a least-significant 7-bit byte, magnitude =
    // LSB + 128 x MSB, so -16383 to 16383. The primitive takes and gives the signed `int`. [RQ-AKM-101]

    /// The largest magnitude the wire carries: two 7-bit bytes. [RQ-AKM-101]
    inline constexpr int FX_PARAMETER_MAX_MAGNITUDE = 128 * 128 - 1;

    /// Sets parameter `parameter` (zero-based, 0-127) of module `module` of channel `channel` to `value` (§12/&50); the
    /// parameter's own range is for the sampler to judge (Table 25). A channel, module or parameter outside 0-127 and
    /// a value of a magnitude above 16383 are refused as `ArgumentOutOfRange` without sending. [RQ-AKM-101]
    void setFxParameter(Session& session, int channel, int module, int parameter, int value, CommandCompletion completion);

    /// `value` is empty when the command did not complete on a REPLY of the length the catalogue gives it.
    /// [RQ-AKM-101]
    struct FxParameterResult
    {
        std::optional<int> value{};
        CommandResult outcome{};
    };
    using FxParameterCompletion = std::function<void(const FxParameterResult&)>;

    /// Gets the value of parameter `parameter` of module `module` of channel `channel` (§12/&51). [RQ-AKM-101]
    void getFxParameter(Session& session, int channel, int module, int parameter, FxParameterCompletion completion);
}
