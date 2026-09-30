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
#include "akm/harness/ZoneParameterCases.hpp"

namespace akm::harness
{
    const std::vector<ZoneParameterCase>& allZoneParameterCases()
    {
        // Every case acts on zone 3, independent of the mock test's own zone 2 (ZoneParametersTests.cpp).
        static const std::vector<ZoneParameterCase> cases{{
            {ItemId::ZoneSetLevel, ItemId::ZoneGetLevel, {3, 0, 60}},
            {ItemId::ZoneSetPanBalance, ItemId::ZoneGetPanBalance, {3, 90}},
            {ItemId::ZoneSetOutput, ItemId::ZoneGetOutput, {3, 3}},
            {ItemId::ZoneSetFilter, ItemId::ZoneGetFilter, {3, 1, 45}},
            {ItemId::ZoneSetFineTune, ItemId::ZoneGetFineTune, {3, 0, 15}},
            {ItemId::ZoneSetSemitoneTune, ItemId::ZoneGetSemitoneTune, {3, 1, 12}},
            {ItemId::ZoneSetKeyboardTrack, ItemId::ZoneGetKeyboardTrack, {3, 1}},
            {ItemId::ZoneSetPlayback, ItemId::ZoneGetPlayback, {3, 2}},
            {ItemId::ZoneSetVelocityToStart, ItemId::ZoneGetVelocityToStart, {3, 1, 2, 50}},
            {ItemId::ZoneSetHighVelocity, ItemId::ZoneGetHighVelocity, {3, 110}},
            {ItemId::ZoneSetLowVelocity, ItemId::ZoneGetLowVelocity, {3, 10}},
            {ItemId::ZoneSetMute, ItemId::ZoneGetMute, {3, 1}},
            {ItemId::ZoneSetSolo, ItemId::ZoneGetSolo, {3, 0}},
        }};
        return cases;
    }
}
