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
#include "app/Memory.hpp"

namespace app
{
    // [RQ-APP-028, RQ-APP-035, TASK-APP-014, TASK-APP-015]
    std::string_view memoryHeading(MemoryKind kind)
    {
        using enum app::MemoryKind;
        switch (kind)
        {
            case Multi:
                return "Multis";
            case Program:
                return "Programs";
            case Sample:
                return "Samples";
            case SongFile:
                return "Song files";
            case SetList:
                return "Set lists";
            case SceneList:
                break;
        }
        return "Scenelists";
    }

    std::optional<MemoryKind> memoryKindOf(FileKind kind)
    {
        switch (kind)
        {
            case FileKind::Program:
                return MemoryKind::Program;
            case FileKind::Sample:
                return MemoryKind::Sample;
            case FileKind::Multi:
                return MemoryKind::Multi;
            case FileKind::SongFile:
                return MemoryKind::SongFile;
            case FileKind::SetList:
                return MemoryKind::SetList;
            case FileKind::SceneList:
                return MemoryKind::SceneList;
            case FileKind::Unknown:
                break;
        }
        return std::nullopt;
    }

    std::optional<std::string> MemoryState::current(MemoryKind kind) const
    {
        const MemoryList& items = list(kind);
        if (!items.current.has_value() || *items.current >= items.names.size())
            return std::nullopt;
        return items.names[*items.current];
    }
}
