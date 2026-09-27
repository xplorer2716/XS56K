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
#include "akm/harness/KeygroupParameterCases.hpp"

namespace akm::harness
{
    const std::vector<KeygroupParameterCase>& allKeygroupParameterCases()
    {
        static const std::vector<KeygroupParameterCase> cases{{
            // General Options (6 pairs).
            {ItemId::KeygroupSetLowNote, ItemId::KeygroupGetLowNote, {40}},
            {ItemId::KeygroupSetHighNote, ItemId::KeygroupGetHighNote, {65}},
            {ItemId::KeygroupSetMuteGroup, ItemId::KeygroupGetMuteGroup, {10}},
            {ItemId::KeygroupSetFxOverride, ItemId::KeygroupGetFxOverride, {3}},
            {ItemId::KeygroupSetFxSendLevel, ItemId::KeygroupGetFxSendLevel, {55}},
            {ItemId::KeygroupSetZoneCrossfade, ItemId::KeygroupGetZoneCrossfade, {1}},

            // Pitch/Amp (5 pairs).
            {ItemId::KeygroupSetSemitoneTune, ItemId::KeygroupGetSemitoneTune, {1, 20}},
            {ItemId::KeygroupSetFineTune, ItemId::KeygroupGetFineTune, {0, 15}},
            {ItemId::KeygroupSetLevel, ItemId::KeygroupGetLevel, {7}},
            {ItemId::KeygroupSetPitchModValue, ItemId::KeygroupGetPitchModValue, {2, 1, 25}},
            {ItemId::KeygroupSetAmpModValue, ItemId::KeygroupGetAmpModValue, {1, 0, 50}},

            // Filter (6 pairs).
            {ItemId::KeygroupSetFilterMode, ItemId::KeygroupGetFilterMode, {9}},
            {ItemId::KeygroupSetFilterCutoff, ItemId::KeygroupGetFilterCutoff, {60}},
            {ItemId::KeygroupSetFilterResonance, ItemId::KeygroupGetFilterResonance, {8}},
            {ItemId::KeygroupSetFilterKeyboardTrack, ItemId::KeygroupGetFilterKeyboardTrack, {1, 18}},
            {ItemId::KeygroupSetFilterModInputValue, ItemId::KeygroupGetFilterModInputValue, {2, 0, 70}},
            {ItemId::KeygroupSetFilterAttenuation, ItemId::KeygroupGetFilterAttenuation, {2}},

            // Filter Envelope (9 pairs).
            {ItemId::KeygroupSetFilterEnvAttack, ItemId::KeygroupGetFilterEnvAttack, {45}},
            {ItemId::KeygroupSetFilterEnvVelocityToAttack, ItemId::KeygroupGetFilterEnvVelocityToAttack, {0, 20}},
            {ItemId::KeygroupSetFilterEnvDecay, ItemId::KeygroupGetFilterEnvDecay, {55}},
            {ItemId::KeygroupSetFilterEnvSustain, ItemId::KeygroupGetFilterEnvSustain, {40}},
            {ItemId::KeygroupSetFilterEnvRelease, ItemId::KeygroupGetFilterEnvRelease, {35}},
            {ItemId::KeygroupSetFilterEnvOnVelocityToRelease, ItemId::KeygroupGetFilterEnvOnVelocityToRelease, {1, 25}},
            {ItemId::KeygroupSetFilterEnvKeyscale, ItemId::KeygroupGetFilterEnvKeyscale, {0, 10}},
            {ItemId::KeygroupSetFilterEnvDepth, ItemId::KeygroupGetFilterEnvDepth, {1, 60}},
            {ItemId::KeygroupSetFilterEnvOffVelocityToRelease, ItemId::KeygroupGetFilterEnvOffVelocityToRelease, {0, 45}},

            // Amplitude Envelope (8 pairs).
            {ItemId::KeygroupSetAmpEnvAttack, ItemId::KeygroupGetAmpEnvAttack, {20}},
            {ItemId::KeygroupSetAmpEnvVelocityToAttack, ItemId::KeygroupGetAmpEnvVelocityToAttack, {1, 35}},
            {ItemId::KeygroupSetAmpEnvDecay, ItemId::KeygroupGetAmpEnvDecay, {30}},
            {ItemId::KeygroupSetAmpEnvSustain, ItemId::KeygroupGetAmpEnvSustain, {80}},
            {ItemId::KeygroupSetAmpEnvRelease, ItemId::KeygroupGetAmpEnvRelease, {25}},
            {ItemId::KeygroupSetAmpEnvOnVelocityToRelease, ItemId::KeygroupGetAmpEnvOnVelocityToRelease, {0, 15}},
            {ItemId::KeygroupSetAmpEnvKeyscale, ItemId::KeygroupGetAmpEnvKeyscale, {1, 5}},
            {ItemId::KeygroupSetAmpEnvOffVelocityToRelease, ItemId::KeygroupGetAmpEnvOffVelocityToRelease, {1, 50}},

            // Aux Envelope (5 pairs).
            {ItemId::KeygroupSetAuxEnvRate, ItemId::KeygroupGetAuxEnvRate, {2, 45}},
            {ItemId::KeygroupSetAuxEnvVelocityToRate, ItemId::KeygroupGetAuxEnvVelocityToRate, {1, 0, 30}},
            {ItemId::KeygroupSetAuxEnvKeyboardToR2R4, ItemId::KeygroupGetAuxEnvKeyboardToR2R4, {1, 40}},
            {ItemId::KeygroupSetAuxEnvLevel, ItemId::KeygroupGetAuxEnvLevel, {3, 60}},
            {ItemId::KeygroupSetAuxEnvOffVelocityToRate, ItemId::KeygroupGetAuxEnvOffVelocityToRate, {4, 0, 20}},
        }};
        return cases;
    }
}
