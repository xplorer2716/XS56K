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

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/Browser.hpp"

namespace app
{
    /// The six kinds of item the sampler holds in memory and saves (section 10, `&2C`), in the order of the memory view's headings.
    /// [RQ-APP-035]
    enum class MemoryKind
    {
        Multi,
        Program,
        Sample,
        SongFile,
        SetList,
        SceneList,
    };

    inline constexpr std::array<MemoryKind, 6> MEMORY_KINDS{MemoryKind::Multi,    MemoryKind::Program, MemoryKind::Sample,
                                                            MemoryKind::SongFile, MemoryKind::SetList, MemoryKind::SceneList};

    /// "Multis", "Programs", "Samples", "Song files", "Set lists", "Scenelists". [RQ-APP-035]
    [[nodiscard]] std::string_view memoryHeading(MemoryKind kind);
    /// The kind of item a file of this kind loads, or nothing for an unknown file. [RQ-APP-028]
    [[nodiscard]] std::optional<MemoryKind> memoryKindOf(FileKind kind);

    /// One item of the memory: its kind and its name. [RQ-APP-035, RQ-APP-037]
    struct MemoryItem
    {
        MemoryKind kind = MemoryKind::Program;
        std::string name;

        [[nodiscard]] bool operator==(const MemoryItem&) const = default;
    };

    /// What the browser tells the memory view after an action that changed the memory: the item to select once the lists are read again,
    /// if any. `number` counts the changes. [RQ-APP-028, RQ-APP-036]
    struct MemoryChange
    {
        std::uint64_t number = 0;
        std::optional<MemoryItem> select;
    };

    /// The items of one kind in the sampler's order, and the current one, if the kind has one and one is current. [RQ-APP-035]
    struct MemoryList
    {
        std::vector<std::string> names;
        std::optional<std::size_t> current;
    };

    /// The sampler's wave memory and the free percentages it reports. [RQ-APP-042]
    struct FreeMemory
    {
        std::uint32_t freeWaveBytes = 0;
        std::uint32_t totalWaveBytes = 0;
        int freeWavePercent = 0;
        int freeMpksPercent = 0;  ///< multis, programs, keygroups and samples
    };

    /// The memory view's part of the model: the six lists, the free memory and the selection. [RQ-APP-035, RQ-APP-037, RQ-APP-042,
    /// ADR-APP-001 (DEC-APP-004)]
    struct MemoryState
    {
        std::array<MemoryList, 6> lists{};
        std::optional<FreeMemory> free;
        std::optional<MemoryItem> selection;

        [[nodiscard]] const MemoryList& list(MemoryKind kind) const { return lists[static_cast<std::size_t>(kind)]; }
        [[nodiscard]] MemoryList& list(MemoryKind kind) { return lists[static_cast<std::size_t>(kind)]; }
        /// The current item of a kind, by name.
        [[nodiscard]] std::optional<std::string> current(MemoryKind kind) const;
    };
}
