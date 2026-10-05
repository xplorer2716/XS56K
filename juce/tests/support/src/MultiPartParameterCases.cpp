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
#include "akm/harness/MultiPartParameterCases.hpp"

namespace akm::harness
{
    const std::vector<MultiPartParameterCase>& allMultiPartParameterCases()
    {
        // Every case acts on part 3, independent of the mock test's own part 2 (MultiPartParametersTests.cpp).
        static const std::vector<MultiPartParameterCase> cases{{
            {ItemId::MultiSetMidiChannel, ItemId::MultiGetMidiChannel, {3, 5}},
            {ItemId::MultiSetMute, ItemId::MultiGetMute, {3, 1}},
            {ItemId::MultiSetSolo, ItemId::MultiGetSolo, {3, 1}},
            {ItemId::MultiSetLevel, ItemId::MultiGetLevel, {3, 80}},
            {ItemId::MultiSetOutput, ItemId::MultiGetOutput, {3, 9}},
            {ItemId::MultiSetPanBalance, ItemId::MultiGetPanBalance, {3, 100}},
            {ItemId::MultiSetEffectsChannel, ItemId::MultiGetEffectsChannel, {3, 3}},
            {ItemId::MultiSetFxSendLevel, ItemId::MultiGetFxSendLevel, {3, 40}},
            {ItemId::MultiSetFineTune, ItemId::MultiGetFineTune, {3, 60}},
            {ItemId::MultiSetTranspose, ItemId::MultiGetTranspose, {3, 48}},
            {ItemId::MultiSetLowNote, ItemId::MultiGetLowNote, {3, 36}},
            {ItemId::MultiSetHighNote, ItemId::MultiGetHighNote, {3, 96}},
        }};
        return cases;
    }
}
