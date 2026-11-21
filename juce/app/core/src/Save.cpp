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
#include "app/Save.hpp"

#include <algorithm>
#include <cctype>

#include "app/Browser.hpp"

namespace app
{
    // [RQ-APP-045, RQ-APP-046, ADR-APP-005 (DEC-APP-039), TASK-APP-059]
    namespace
    {
        constexpr const char* SAVED_TO = "Saved to ";
        constexpr const char* NOTHING_SAVED = "Nothing was saved to ";
        constexpr const char* SKIPPED = "Skipped (already on the disk): ";
        constexpr const char* MISSING = "Expected and missing from the folder: ";
        constexpr const char* ROOT_NAME = "(root)";
        constexpr const char* LIST_SEPARATOR = ", ";
        constexpr const char* SENTENCE_END = ". ";
        constexpr char EXTENSION_DOT = '.';

        /// The extensions a real S5000 (OS 2.14) gave the files it saved (juce/mcp/README.md), by memory kind.
        std::string_view extensionOf(MemoryKind kind)
        {
            using enum app::MemoryKind;
            switch (kind)
            {
                case Multi:
                    return "AKM";
                case Program:
                    return "AKP";
                case Sample:
                    return "WAV";
                case SongFile:
                    return "MID";
                case SetList:
                    return "SET";
                case SceneList:
                    break;
            }
            return "ASC";
        }

        bool sameIgnoringCase(std::string_view a, std::string_view b)
        {
            return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
                       return std::toupper(static_cast<unsigned char>(x)) == std::toupper(static_cast<unsigned char>(y));
                   });
        }

        bool listed(const std::vector<mcp::DiskFileEntry>& files, const std::string& name)
        {
            return std::ranges::any_of(files, [&name](const mcp::DiskFileEntry& file) { return sameIgnoringCase(file.name, name); });
        }

        std::string joined(const std::vector<std::string>& names)
        {
            std::string text;
            for (const std::string& name : names)
                text += (text.empty() ? "" : LIST_SEPARATOR) + name;
            return text;
        }
    }

    std::string savedFileName(const MemoryItem& item)
    {
        return item.name + EXTENSION_DOT + std::string(extensionOf(item.kind));
    }

    bool isFileOf(const MemoryItem& item, const std::string& fileName)
    {
        return sameIgnoringCase(fileName, savedFileName(item)) && memoryKindOf(fileKindOf(fileName)) == item.kind;
    }

    SaveReport reportOf(const std::vector<MemoryItem>& items, const mcp::SaveOutcome& outcome)
    {
        SaveReport report;
        report.diskName = outcome.diskName;
        report.path = outcome.path;
        report.skipped = outcome.skipped;
        for (const MemoryItem& item : items)
        {
            const std::string file = savedFileName(item);
            if (std::ranges::any_of(report.skipped, [&file](const std::string& skipped) { return sameIgnoringCase(skipped, file); }))
                continue;
            if (listed(outcome.filesAfter, file))
                report.saved.push_back(file);
            else
                report.missing.push_back(file);
        }
        // The children: files the save added that bear no item's name. [RQ-APP-043]
        for (const mcp::DiskFileEntry& after : outcome.filesAfter)
        {
            const bool added = !listed(outcome.filesBefore, after.name);
            const bool ofAnItem = std::ranges::any_of(items, [&after](const MemoryItem& item) { return isFileOf(item, after.name); });
            if (added && !ofAnItem)
                report.saved.push_back(after.name);
        }
        return report;
    }

    std::string SaveReport::text() const
    {
        const std::string where = diskName + " (" + (path.empty() ? std::string(ROOT_NAME) : path) + ")";
        std::string text = saved.empty() ? NOTHING_SAVED + where + SENTENCE_END : SAVED_TO + where + ": " + joined(saved) + SENTENCE_END;
        if (!skipped.empty())
            text += SKIPPED + joined(skipped) + SENTENCE_END;
        if (!missing.empty())
            text += MISSING + joined(missing) + SENTENCE_END;
        text.pop_back();
        return text;
    }
}
