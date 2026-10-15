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
#include "app/Browser.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace app
{
    // [RQ-APP-024 to RQ-APP-027, TASK-APP-013]
    namespace
    {
        struct KindByExtension
        {
            std::string_view extension;
            FileKind kind;
        };

        // The extensions a real S5000 (OS 2.14) gave the files it saved, 2026-10-10 (juce/mcp/README.md).
        constexpr std::array<KindByExtension, 6> KINDS{{
            {"AKP", FileKind::Program},
            {"WAV", FileKind::Sample},
            {"AKM", FileKind::Multi},
            {"MID", FileKind::SongFile},
            {"SET", FileKind::SetList},
            {"ASC", FileKind::SceneList},
        }};

        // Section 10, Tables 20 and 21 (documents/_index/sysex_spec.kb.md, "Common value codes").
        constexpr std::array<std::string_view, 4> DISK_TYPES{"floppy", "hard disk", "CD-ROM", "removable"};
        constexpr std::array<std::string_view, 8> DISK_FORMATS{"other", "MSDOS", "FAT32", "ISO9660", "S1000", "S3000", "EMU", "ROLAND"};
        constexpr std::string_view UNKNOWN = "unknown";
        constexpr std::string_view NOT_REPORTED = "not reported";
        constexpr std::string_view BYTES = " bytes";
        constexpr char EXTENSION_SEPARATOR = '.';
        constexpr char PATH_SEPARATOR = '\\';
        constexpr char THOUSANDS_SEPARATOR = ' ';
        constexpr std::size_t DIGITS_PER_GROUP = 3;
        constexpr std::uint64_t UNIT_STEP = 1024;
        constexpr std::uint64_t TENTHS = 10;
        constexpr std::array<std::string_view, 3> UNITS{" KB", " MB", " GB"};

        template <std::size_t N>
        std::string_view nameAt(const std::array<std::string_view, N>& names, int code)
        {
            if (code < 0 || static_cast<std::size_t>(code) >= names.size())
                return UNKNOWN;
            return names[static_cast<std::size_t>(code)];
        }

        bool sameIgnoringCase(std::string_view a, std::string_view b)
        {
            return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
                       return std::toupper(static_cast<unsigned char>(x)) == std::toupper(static_cast<unsigned char>(y));
                   });
        }
    }

    FileKind fileKindOf(std::string_view fileName)
    {
        const auto dot = fileName.rfind(EXTENSION_SEPARATOR);
        if (dot == std::string_view::npos)
            return FileKind::Unknown;
        const std::string_view extension = fileName.substr(dot + 1);
        for (const KindByExtension& candidate : KINDS)
        {
            if (sameIgnoringCase(candidate.extension, extension))
                return candidate.kind;
        }
        return FileKind::Unknown;
    }

    std::string_view fileKindName(FileKind kind)
    {
        switch (kind)
        {
            case FileKind::Program:
                return "program";
            case FileKind::Sample:
                return "sample";
            case FileKind::Multi:
                return "multi";
            case FileKind::SongFile:
                return "song file";
            case FileKind::SetList:
                return "set list";
            case FileKind::SceneList:
                return "scenelist";
            case FileKind::Unknown:
                break;
        }
        return UNKNOWN;
    }

    std::string_view diskTypeName(int type)
    {
        return nameAt(DISK_TYPES, type);
    }

    std::string_view diskFormatName(int format)
    {
        return nameAt(DISK_FORMATS, format);
    }

    std::string bytesText(std::uint64_t bytes)
    {
        const std::string digits = std::to_string(bytes);
        std::string text;
        for (std::size_t i = 0; i < digits.size(); ++i)
        {
            if (i > 0 && (digits.size() - i) % DIGITS_PER_GROUP == 0)
                text += THOUSANDS_SEPARATOR;
            text += digits[i];
        }
        return text + std::string(BYTES);
    }

    std::string sizeText(std::uint64_t bytes)
    {
        // [RQ-APP-081, TASK-APP-020]
        if (bytes < UNIT_STEP)
            return std::to_string(bytes) + std::string(BYTES);
        std::uint64_t unitBytes = UNIT_STEP;
        for (std::size_t unit = 0; unit < UNITS.size(); ++unit, unitBytes *= UNIT_STEP)
        {
            // Tenths of the unit, rounded to the nearest; a value that rounds to 1024.0 goes to the next unit, if there is one.
            const std::uint64_t tenths = (bytes * TENTHS + unitBytes / 2) / unitBytes;
            if (tenths < UNIT_STEP * TENTHS || unit + 1 == UNITS.size())
                return std::to_string(tenths / TENTHS) + "." + std::to_string(tenths % TENTHS) + std::string(UNITS[unit]);
        }
        return bytesText(bytes);
    }

    std::string freeSpaceText(std::uint64_t freeBytes)
    {
        if (freeBytes == 0)
            return std::string(NOT_REPORTED);
        return sizeText(freeBytes);
    }

    std::vector<std::string> splitPath(std::string_view samplerPath)
    {
        std::vector<std::string> folders;
        std::size_t start = 0;
        while (start < samplerPath.size())
        {
            const auto end = samplerPath.find(PATH_SEPARATOR, start);
            const std::string_view name = samplerPath.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
            if (!name.empty())
                folders.emplace_back(name);
            if (end == std::string_view::npos)
                break;
            start = end + 1;
        }
        return folders;
    }

    std::string pathText(const std::vector<std::string>& folders)
    {
        std::string text;
        for (const std::string& folder : folders)
            text += (text.empty() ? "" : std::string(1, PATH_SEPARATOR)) + folder;
        return text;
    }

    std::vector<BrowserCommand> commandsFor(const BrowserItem& item)
    {
        switch (item.kind)
        {
            case BrowserItemKind::Disk:
                return {BrowserCommand::CreateFolder, BrowserCommand::RefreshDisks};
            case BrowserItemKind::Folder:
                return {BrowserCommand::Open, BrowserCommand::LoadFolder, BrowserCommand::CreateFolder, BrowserCommand::Rename,
                        BrowserCommand::Delete};
            case BrowserItemKind::File:
                break;
        }
        std::vector<BrowserCommand> commands{BrowserCommand::Load, BrowserCommand::LoadWithDependents, BrowserCommand::Rename,
                                             BrowserCommand::Delete};
        if (fileKindOf(item.name) == FileKind::Sample)
            commands.push_back(BrowserCommand::Audition);
        return commands;
    }

    const mcp::DiskEntry* BrowserState::currentDiskEntry() const
    {
        if (!currentDisk)
            return nullptr;
        const auto found = std::find_if(disks.begin(), disks.end(), [this](const mcp::DiskEntry& disk) { return disk.handle == *currentDisk; });
        return found == disks.end() ? nullptr : &*found;
    }

    const FolderListing* BrowserState::listing(const std::vector<std::string>& folder) const
    {
        const auto found = listings.find(pathText(folder));
        return found == listings.end() ? nullptr : &found->second;
    }

    std::optional<FileDetails> BrowserState::selectedFile() const
    {
        if (!selection || selection->kind != BrowserItemKind::File)
            return std::nullopt;
        const FolderListing* folder = listing(selection->folder);
        if (folder == nullptr)
            return std::nullopt;
        const auto found = std::find_if(folder->files.begin(), folder->files.end(),
                                        [this](const mcp::DiskFileEntry& file) { return file.name == selection->name; });
        if (found == folder->files.end())
            return std::nullopt;
        return FileDetails{found->name, found->sizeBytes, fileKindOf(found->name)};
    }
}
