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
        //
        // The 4-byte items (Start/End Position, Loop Start/Loop End) are each ONE 28-bit compound word
        // on the wire (Msb, Sb2, Sb1, Lsb — spec pp. 8-9, `items.json`'s own arg names, the same shape
        // `appendCompoundWord` in SimulatedSampler.cpp encodes), not 4 independent bytes: `values` here
        // is that word's 4 wire bytes, most significant first, not 4 unrelated small numbers. A prior
        // version used {10,20,30,40}/{50,60,70,80}/{1,1,1,1}/{2,2,2,2} as if each byte were independent
        // (to catch byte-order mistakes, the way the 1- and 2-byte items below do) — decoded as compound
        // words those are 21303080/105849680/2113665/4227458, far beyond any real sample's length. Run
        // against the real S5000 (2026-09-30, samples "AMEN" and "AllTime2", ~143k and ~238k samples
        // long), Set Start Position with such a value still answered DONE but the sampler silently
        // clamped it to End Position − 128 rather than rejecting it with an ERROR — not a firmware bug,
        // just proof the value was nonsense. Replaced with small, ordered, realistic values (well inside
        // even a short one-shot sample): Start < Loop Start < Loop End < End.
        //
        // Loop End set *after* Loop Start on the real S5000 (same 2026-09-30 session, samples "AMEN" and
        // "Honesty"): Loop Start's own value drifted away from what was just set, reproducibly, by the
        // time it was read back — the exact mechanism isn't pinned down (it does not match a simple
        // "preserve the current loop length" formula once checked against both the mid-test and the
        // restore readings), but the correlation is consistent both times: Set Loop End after Set Loop
        // Start leaves Loop Start wrong. Loop End is listed before Loop Start here so nothing sent after
        // Loop Start's own Set can disturb it again; `GuardedTestSample::restoreParameters` mirrors this.
        static const std::vector<SampleParameterCase> cases{{
            {ItemId::SampleSetStartPosition, ItemId::SampleGetStartPosition, {0, 0, 0, 50}},      // 50
            {ItemId::SampleSetEndPosition, ItemId::SampleGetEndPosition, {0, 0, 15, 80}},          // 2000
            {ItemId::SampleSetOriginalPitch, ItemId::SampleGetOriginalPitch, {72}},
            {ItemId::SampleSetSemitoneTune, ItemId::SampleGetSemitoneTune, {1, 5}},
            {ItemId::SampleSetFineTune, ItemId::SampleGetFineTune, {0, 40}},
            {ItemId::SampleSetPlaybackMode, ItemId::SampleGetPlaybackMode, {1}},
            {ItemId::SampleSetLoopEnd, ItemId::SampleGetLoopEnd, {0, 0, 11, 92}},                  // 1500
            {ItemId::SampleSetLoopStart, ItemId::SampleGetLoopStart, {0, 0, 3, 116}},              // 500
        }};
        return cases;
    }
}
