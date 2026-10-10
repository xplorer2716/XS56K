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
#include "mcp/FxCatalogue.hpp"

#include <algorithm>
#include <cctype>

#include "mcp/ParameterCatalogue.hpp"

namespace mcp
{
    namespace
    {
        // The module codes of Table 24.
        constexpr int CODE_NONE = 0x00;
        constexpr int CODE_RINGMOD_DISTORTION = 0x01;
        constexpr int CODE_CHORUS = 0x02;
        constexpr int CODE_FLANGE = 0x03;
        constexpr int CODE_PHASE = 0x04;
        constexpr int CODE_ROTARY_SPEAKER = 0x05;
        constexpr int CODE_FMOD_AUTOPAN = 0x06;
        constexpr int CODE_PITCH_SHIFT = 0x07;
        constexpr int CODE_PITCH_SHIFT_FEEDBACK = 0x08;
        constexpr int CODE_EQ = 0x09;
        constexpr int CODE_MONO_DELAY = 0x0A;
        constexpr int CODE_MONO_LEFT_RIGHT = 0x0B;
        constexpr int CODE_MONO_CROSSOVER = 0x0C;
        constexpr int CODE_STEREO_DELAY = 0x0D;
        constexpr int CODE_REVERB = 0x0E;
        constexpr int CODE_OUTPUT_MIX = 0x0F;
        constexpr int CODE_REVERB_INPUT = 0x10;

        // The EB20 layout of Figure 2: the channels and the modules whose type may change.
        constexpr int LAST_CHANGEABLE_CHANNEL = 1;
        constexpr int MODULATION_MODULE = 2;
        constexpr int DELAY_MODULE = 3;

        // The ranges Table 25 gives, named once.
        constexpr int PERCENT_MIN = 0;
        constexpr int PERCENT_MAX = 100;
        constexpr int RATE_MAX = 99;
        constexpr int SIGNED_PERCENT_MIN = -50;
        constexpr int SIGNED_PERCENT_MAX = 50;
        constexpr int FLAG_MAX = 1;
        constexpr int GAIN_MIN = -37;
        constexpr int GAIN_MAX = 12;
        constexpr int LOW_HIGH_FREQUENCY_MAX = 30;
        constexpr int MID_FREQUENCY_MAX = 44;
        constexpr int DELAY_FEEDBACK_MAX = 100;
        constexpr int HF_DAMPING_MAX = 46;
        constexpr int MONO_DELAY_MAX = 670;
        constexpr int STEREO_DELAY_MAX = 335;
        constexpr int PITCH_DELAY_MAX = 275;

        constexpr const char* TENTHS = "0 to 99 = 0.0 to 9.9";
        constexpr const char* NO_MEANING = "";

        std::vector<FxModuleKind> buildKinds()
        {
            const FxParameter rate{0, "rate", PERCENT_MIN, RATE_MAX, TENTHS};
            const FxParameter depth{1, "depth", PERCENT_MIN, PERCENT_MAX, NO_MEANING};
            const FxParameter feedback{2, "feedback", SIGNED_PERCENT_MIN, SIGNED_PERCENT_MAX, NO_MEANING};
            const std::vector<FxParameter> modulation{rate, depth, feedback};
            const std::vector<FxParameter> delay{
                {0, "delay_time", 0, MONO_DELAY_MAX, "milliseconds"},
                {1, "feedback", 0, DELAY_FEEDBACK_MAX, "percent"},
                {2, "hf_damping", 0, HF_DAMPING_MAX, "0 to 46 = 100 Hz to 20 kHz"},
                {3, "ping_pong", SIGNED_PERCENT_MIN, SIGNED_PERCENT_MAX, NO_MEANING},
                {4, "feedback_monitor", 0, FLAG_MAX, "0 = pre, 1 = post"},
            };
            return {
                {CODE_NONE, "none", {}},
                {CODE_RINGMOD_DISTORTION,
                 "ringmod_distortion",
                 {{0, "distortion", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {1, "output_level", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {2, "rmod_frequency", 1, 5000, "hertz"},
                  {3, "rmod_depth", PERCENT_MIN, PERCENT_MAX, NO_MEANING}}},
                {CODE_CHORUS, "chorus", modulation},
                {CODE_FLANGE, "flange", modulation},
                {CODE_PHASE, "phase", modulation},
                {CODE_ROTARY_SPEAKER,
                 "rotary_speaker",
                 {{0, "speed_1", PERCENT_MIN, RATE_MAX, TENTHS},
                  {1, "speed_2", PERCENT_MIN, RATE_MAX, TENTHS},
                  {2, "acceleration", PERCENT_MIN, RATE_MAX, TENTHS},
                  {3, "depth_width", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {4, "init_speed", 0, FLAG_MAX, "0 = speed 1, 1 = speed 2"},
                  {5, "midi_control", 1, 127, "a MIDI controller number"},
                  {6, "midi_mode", 0, FLAG_MAX, "0 = level, 1 = toggle"},
                  {7, "midi_channel", 0, 31, "0 to 31 = 1A to 16B"}}},
                {CODE_FMOD_AUTOPAN,
                 "fmod_autopan",
                 {{0, "fmod_rate", PERCENT_MIN, RATE_MAX, TENTHS},
                  {1, "fmod_depth", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {2, "fmod_feedback", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {3, "amod_rate", PERCENT_MIN, RATE_MAX, TENTHS},
                  {4, "amod_depth", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {5, "amod_mode", 0, 3, "0 to 3 = pan to tremolo"}}},
                {CODE_PITCH_SHIFT,
                 "pitch_shift",
                 {{0, "left_semitone", SIGNED_PERCENT_MIN, SIGNED_PERCENT_MAX, NO_MEANING},
                  {1, "left_fine", SIGNED_PERCENT_MIN, SIGNED_PERCENT_MAX, NO_MEANING},
                  {2, "right_semitone", SIGNED_PERCENT_MIN, SIGNED_PERCENT_MAX, NO_MEANING},
                  {3, "right_fine", SIGNED_PERCENT_MIN, SIGNED_PERCENT_MAX, NO_MEANING}}},
                {CODE_PITCH_SHIFT_FEEDBACK,
                 "pitch_shift_feedback",
                 {{0, "left_semitone", SIGNED_PERCENT_MIN, SIGNED_PERCENT_MAX, NO_MEANING},
                  {1, "left_fine", SIGNED_PERCENT_MIN, SIGNED_PERCENT_MAX, NO_MEANING},
                  {2, "left_delay", 0, PITCH_DELAY_MAX, "milliseconds"},
                  {3, "left_feedback", 0, DELAY_FEEDBACK_MAX, NO_MEANING},
                  {4, "right_semitone", SIGNED_PERCENT_MIN, SIGNED_PERCENT_MAX, NO_MEANING},
                  {5, "right_fine", SIGNED_PERCENT_MIN, SIGNED_PERCENT_MAX, NO_MEANING},
                  {6, "right_delay", 0, PITCH_DELAY_MAX, "milliseconds"},
                  {7, "right_feedback", 0, DELAY_FEEDBACK_MAX, NO_MEANING}}},
                {CODE_EQ,
                 "eq",
                 {{0, "low_freq", 0, LOW_HIGH_FREQUENCY_MAX, "0 to 30 = 16 Hz to 500 Hz"},
                  {1, "low_gain", GAIN_MIN, GAIN_MAX, "decibels"},
                  {2, "low_mid_freq", 0, MID_FREQUENCY_MAX, "0 to 44 = 40 Hz to 6.3 kHz"},
                  {3, "low_mid_gain", GAIN_MIN, GAIN_MAX, "decibels"},
                  {4, "low_mid_width", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {5, "high_mid_freq", 0, MID_FREQUENCY_MAX, "0 to 44 = 40 Hz to 6.3 kHz"},
                  {6, "high_mid_gain", GAIN_MIN, GAIN_MAX, "decibels"},
                  {7, "high_mid_width", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {8, "high_freq", 0, LOW_HIGH_FREQUENCY_MAX, "0 to 30 = 500 Hz to 16 kHz"},
                  {9, "high_gain", GAIN_MIN, GAIN_MAX, "decibels"},
                  {10, "low_mid_sweep_rate", PERCENT_MIN, RATE_MAX, TENTHS},
                  {11, "low_mid_sweep_depth", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {12, "high_mid_sweep_rate", PERCENT_MIN, RATE_MAX, TENTHS},
                  {13, "high_mid_sweep_depth", PERCENT_MIN, PERCENT_MAX, NO_MEANING}}},
                {CODE_MONO_DELAY, "mono_delay", delay},
                {CODE_MONO_LEFT_RIGHT, "mono_left_right", delay},
                {CODE_MONO_CROSSOVER, "mono_crossover", delay},
                {CODE_STEREO_DELAY,
                 "stereo_delay",
                 {{0, "left_delay_time", 0, STEREO_DELAY_MAX, "milliseconds"},
                  {1, "left_feedback", 0, DELAY_FEEDBACK_MAX, "percent"},
                  {2, "left_hf_damping", 0, HF_DAMPING_MAX, "0 to 46 = 100 Hz to 20 kHz"},
                  {3, "right_delay_time", 0, STEREO_DELAY_MAX, "milliseconds"},
                  {4, "right_feedback", 0, DELAY_FEEDBACK_MAX, "percent"},
                  {5, "right_hf_damping", 0, HF_DAMPING_MAX, "0 to 46 = 100 Hz to 20 kHz"},
                  {6, "feedback_monitor", 0, FLAG_MAX, "0 = pre, 1 = post"}}},
                {CODE_REVERB,
                 "reverb",
                 {{0, "reverb_type", 0, 6, NO_MEANING},
                  {1, "pre_delay", 0, 90, NO_MEANING},
                  {2, "reverb_decay_time", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {3, "diffusion", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {4, "near", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {5, "reverb_level", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {6, "reverb_pan", SIGNED_PERCENT_MIN, SIGNED_PERCENT_MAX, "-50 to 50 = L50 to R50"},
                  {7, "lf_damping", 0, 40, "0 to 40 = 10 Hz to 1 kHz"},
                  {8, "hf_damping", 0, 25, "0 to 25 = 1 kHz to 20 kHz"}}},
                {CODE_OUTPUT_MIX,
                 "output_mix",
                 {{0, "mod_delay_level", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {1, "mod_delay_pan", SIGNED_PERCENT_MIN, SIGNED_PERCENT_MAX, "-50 to 50 = L50 to R50"},
                  {2, "mod_delay_width", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {3, "direct_signal", 0, FLAG_MAX, "0 = off, 1 = on"},
                  {4, "path_control", SIGNED_PERCENT_MIN, SIGNED_PERCENT_MAX, NO_MEANING},
                  {5, "dist_eq_level", PERCENT_MIN, PERCENT_MAX, NO_MEANING},
                  {6, "dist_eq_pan", SIGNED_PERCENT_MIN, SIGNED_PERCENT_MAX, "-50 to 50 = L50 to R50"}}},
                {CODE_REVERB_INPUT, "reverb_input", {{0, "rv_input", 0, 4, NO_MEANING}}},
            };
        }

        /// A name as a client may write it, reduced to compare: other letters, a space or a hyphen for the underscore.
        std::string keyOf(std::string_view text)
        {
            std::string spaced(text);
            std::replace(spaced.begin(), spaced.end(), '_', ' ');
            return normalizeText(spaced);
        }

        std::string joinedNames(const std::vector<std::string>& names)
        {
            std::string text;
            for (std::size_t i = 0; i < names.size(); ++i)
                text += (i == 0 ? "" : ", ") + names[i];
            return text;
        }
    }

    const std::vector<FxModuleKind>& fxModuleKinds()
    {
        static const std::vector<FxModuleKind> KINDS = buildKinds();
        return KINDS;
    }

    const FxModuleKind* fxKindByCode(int code)
    {
        for (const FxModuleKind& kind : fxModuleKinds())
        {
            if (kind.code == code)
                return &kind;
        }
        return nullptr;
    }

    const FxModuleKind* fxKindByName(std::string_view name)
    {
        const std::string wanted = keyOf(name);
        for (const FxModuleKind& kind : fxModuleKinds())
        {
            if (keyOf(kind.name) == wanted)
                return &kind;
        }
        return nullptr;
    }

    const FxParameter* fxParameterOf(const FxModuleKind& kind, std::string_view nameOrIndex)
    {
        const std::string text(nameOrIndex);
        const bool digits = !text.empty() && std::all_of(text.begin(), text.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
        const std::string wanted = keyOf(nameOrIndex);
        for (const FxParameter& parameter : kind.parameters)
        {
            if (digits ? std::to_string(parameter.index) == text : keyOf(parameter.name) == wanted)
                return &parameter;
        }
        return nullptr;
    }

    std::vector<int> eb20KindsFor(int channel, int module)
    {
        if (channel < 0 || channel > LAST_CHANGEABLE_CHANNEL)
            return {};
        if (module == MODULATION_MODULE)
            return {CODE_CHORUS, CODE_FLANGE, CODE_PHASE, CODE_ROTARY_SPEAKER, CODE_FMOD_AUTOPAN, CODE_PITCH_SHIFT, CODE_PITCH_SHIFT_FEEDBACK};
        if (module == DELAY_MODULE)
            return {CODE_MONO_DELAY, CODE_MONO_LEFT_RIGHT, CODE_MONO_CROSSOVER, CODE_STEREO_DELAY};
        return {};
    }

    std::string fxKindNames(const std::vector<int>& codes)
    {
        std::vector<std::string> names;
        for (const int code : codes)
        {
            if (const FxModuleKind* kind = fxKindByCode(code))
                names.emplace_back(kind->name);
        }
        return joinedNames(names);
    }

    std::string fxParameterNames(const FxModuleKind& kind)
    {
        std::vector<std::string> names;
        for (const FxParameter& parameter : kind.parameters)
            names.emplace_back(parameter.name);
        return joinedNames(names);
    }

    std::string fxRangeText(const FxParameter& parameter)
    {
        const std::string range = std::to_string(parameter.minimum) + " to " + std::to_string(parameter.maximum);
        const std::string meaning = parameter.meaning;
        if (meaning.empty())
            return range;
        // A meaning that already starts with the range (such as "0 to 99 = 0.0 to 9.9") is the whole sentence.
        if (meaning.compare(0, range.size(), range) == 0)
            return meaning;
        return range + " (" + meaning + ")";
    }
}
