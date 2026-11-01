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

// What a save to the disk is and what it did, as plain C++: the scope the person chose, the request, and the report checked on the folder's
// listing. No JUCE. [RQ-APP-043 to RQ-APP-046, ADR-APP-005 (DEC-APP-039, DEC-APP-040)]

#include <string>
#include <vector>

#include "app/Memory.hpp"
#include "mcp/SamplerGateway.hpp"

namespace app
{
    /// What is saved: the item alone or with its children, all the items of its kind with or without their children, or the whole memory.
    /// [RQ-APP-043, RQ-APP-044]
    enum class SaveScope
    {
        Alone,
        WithChildren,
        AllOfKind,
        AllOfKindWithChildren,
        WholeMemory,
    };

    /// A save the person validated: the item proposed (its kind for a save of all) and the scope. [RQ-APP-043, RQ-APP-044]
    struct SaveRequest
    {
        MemoryItem item;
        SaveScope scope = SaveScope::Alone;
    };

    /// What a save did, as the folder's listing shows it afterwards: the files saved, those skipped (there before and kept) and those
    /// expected and missing; for the whole memory, the kinds saved before a failure. [RQ-APP-046, DEC-APP-040]
    struct SaveReport
    {
        std::string diskName;
        std::string path;
        std::vector<std::string> saved;
        std::vector<std::string> skipped;
        std::vector<std::string> missing;
        std::vector<MemoryKind> kindsSaved;

        /// One sentence per list that is not empty: "Saved to HD1 (KIT): PAD.AKP. Skipped (already on the disk): ... Missing: ...".
        [[nodiscard]] std::string text() const;
    };

    /// The file name the sampler gives an item when it saves it: its name and the extension of its kind ("PAD.AKP"). [RQ-APP-045]
    [[nodiscard]] std::string savedFileName(const MemoryItem& item);
    /// True when `fileName` is the file the sampler writes for `item`: the same name, case ignored, and the extension of its kind. [RQ-APP-045]
    [[nodiscard]] bool isFileOf(const MemoryItem& item, const std::string& fileName);

    /// The report of a save of `items` from the gateway's outcome: an item's file in the listing afterwards and not skipped is saved, an
    /// item's file not in it is missing; the files added that bear no item's name (children) are saved too. [RQ-APP-046]
    [[nodiscard]] SaveReport reportOf(const std::vector<MemoryItem>& items, const mcp::SaveOutcome& outcome);
}
