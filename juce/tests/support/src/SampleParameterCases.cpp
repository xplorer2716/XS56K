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
#include "akm/harness/SampleParameterCases.hpp"

namespace akm::harness
{
    const std::vector<SampleParameterCase>& allSampleParameterCases()
    {
        // Independent of SampleParametersTests.cpp's own mock-test values.
        static const std::vector<SampleParameterCase> cases{{
            {ItemId::SampleSetStartPosition, ItemId::SampleGetStartPosition, {10, 20, 30, 40}},
            {ItemId::SampleSetEndPosition, ItemId::SampleGetEndPosition, {50, 60, 70, 80}},
            {ItemId::SampleSetOriginalPitch, ItemId::SampleGetOriginalPitch, {72}},
            {ItemId::SampleSetSemitoneTune, ItemId::SampleGetSemitoneTune, {1, 5}},
            {ItemId::SampleSetFineTune, ItemId::SampleGetFineTune, {0, 40}},
            {ItemId::SampleSetPlaybackMode, ItemId::SampleGetPlaybackMode, {1}},
            {ItemId::SampleSetLoopStart, ItemId::SampleGetLoopStart, {1, 1, 1, 1}},
            {ItemId::SampleSetLoopEnd, ItemId::SampleGetLoopEnd, {2, 2, 2, 2}},
        }};
        return cases;
    }
}
