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
#include "akm/harness/ProgramParameterCases.hpp"

namespace akm::harness
{
    const std::vector<ProgramParameterCase>& allProgramParameterCases()
    {
        static const std::vector<ProgramParameterCase> cases{{
            // Output (6 pairs).
            {ItemId::ProgramSetLoudness, ItemId::ProgramGetLoudness, {50}},
            {ItemId::ProgramSetVelocitySensitivity, ItemId::ProgramGetVelocitySensitivity, {1, 30}},
            {ItemId::ProgramSetAmpModSource, ItemId::ProgramGetAmpModSource, {1, 7}},
            {ItemId::ProgramSetAmpModValue, ItemId::ProgramGetAmpModValue, {2, 0, 45}},
            {ItemId::ProgramSetPanModSource, ItemId::ProgramGetPanModSource, {3, 2}},
            {ItemId::ProgramSetPanModValue, ItemId::ProgramGetPanModValue, {1, 1, 10}},

            // MIDI/Tune (5 pairs).
            {ItemId::ProgramSetSemitoneTune, ItemId::ProgramGetSemitoneTune, {1, 12}},
            {ItemId::ProgramSetFineTune, ItemId::ProgramGetFineTune, {0, 25}},
            {ItemId::ProgramSetTuneTemplate, ItemId::ProgramGetTuneTemplate, {5}},
            {ItemId::ProgramSetUserTuneTemplate,
             ItemId::ProgramGetUserTuneTemplate,
             {0, 1, 1, 2, 0, 3, 1, 4, 0, 5, 1, 6, 0, 7, 1, 8, 0, 9, 1, 10, 0, 11, 1, 12}},
            {ItemId::ProgramSetKey, ItemId::ProgramGetKey, {9}},

            // Pitch Bend (8 pairs).
            {ItemId::ProgramSetPitchBendUp, ItemId::ProgramGetPitchBendUp, {12}},
            {ItemId::ProgramSetPitchBendDown, ItemId::ProgramGetPitchBendDown, {24}},
            {ItemId::ProgramSetBendMode, ItemId::ProgramGetBendMode, {1}},
            {ItemId::ProgramSetAftertouchValue, ItemId::ProgramGetAftertouchValue, {1, 8}},
            {ItemId::ProgramSetLegato, ItemId::ProgramGetLegato, {1}},
            {ItemId::ProgramSetPortamentoEnable, ItemId::ProgramGetPortamentoEnable, {1}},
            {ItemId::ProgramSetPortamentoMode, ItemId::ProgramGetPortamentoMode, {0}},
            {ItemId::ProgramSetPortamentoTime, ItemId::ProgramGetPortamentoTime, {60}},

            // LFOs (16 pairs); &54/&55/&5C/&5D/&5E/&5F are for one LFO only, their selector fixed accordingly.
            {ItemId::ProgramSetLfoRate, ItemId::ProgramGetLfoRate, {1, 40}},
            {ItemId::ProgramSetLfoDelay, ItemId::ProgramGetLfoDelay, {1, 10}},
            {ItemId::ProgramSetLfoDepth, ItemId::ProgramGetLfoDepth, {1, 30}},
            {ItemId::ProgramSetLfoWaveform, ItemId::ProgramGetLfoWaveform, {1, 5}},
            {ItemId::ProgramSetLfoSync, ItemId::ProgramGetLfoSync, {1, 1}},
            {ItemId::ProgramSetLfoRetrigger, ItemId::ProgramGetLfoRetrigger, {2, 1}},
            {ItemId::ProgramSetLfoRateModSource, ItemId::ProgramGetLfoRateModSource, {1, 7}},
            {ItemId::ProgramSetLfoRateModValue, ItemId::ProgramGetLfoRateModValue, {1, 0, 50}},
            {ItemId::ProgramSetLfoDelayModSource, ItemId::ProgramGetLfoDelayModSource, {1, 2}},
            {ItemId::ProgramSetLfoDelayModValue, ItemId::ProgramGetLfoDelayModValue, {1, 1, 15}},
            {ItemId::ProgramSetLfoDepthModSource, ItemId::ProgramGetLfoDepthModSource, {1, 4}},
            {ItemId::ProgramSetLfoDepthModValue, ItemId::ProgramGetLfoDepthModValue, {1, 0, 33}},
            {ItemId::ProgramSetLfoModwheel, ItemId::ProgramGetLfoModwheel, {1, 80}},
            {ItemId::ProgramSetLfoAftertouch, ItemId::ProgramGetLfoAftertouch, {1, 90}},
            {ItemId::ProgramSetLfoMidiClockSyncEnable, ItemId::ProgramGetLfoMidiClockSyncEnable, {2, 1}},
            {ItemId::ProgramSetLfoMidiClockSyncDivision, ItemId::ProgramGetLfoMidiClockSyncDivision, {2, 42}},

            // Keygroup Modulation Sources (3 pairs).
            {ItemId::ProgramSetPitchModSource, ItemId::ProgramGetPitchModSource, {1, 6}},
            {ItemId::ProgramSetKeygroupAmpModSource, ItemId::ProgramGetKeygroupAmpModSource, {1, 4}},
            {ItemId::ProgramSetFilterModInputSource, ItemId::ProgramGetFilterModInputSource, {2, 8}},
        }};
        return cases;
    }
}
