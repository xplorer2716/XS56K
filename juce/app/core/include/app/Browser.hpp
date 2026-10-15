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

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "mcp/SamplerGateway.hpp"

namespace app
{
    /// What a file is, from the extension the sampler gives it: `.AKP` program, `.WAV` sample, `.AKM` multi, `.MID` song file, `.SET` set
    /// list, `.ASC` scenelist, as a real S5000 (OS 2.14) saved them; any other extension is unknown. [RQ-APP-026]
    enum class FileKind
    {
        Program,
        Sample,
        Multi,
        SongFile,
        SetList,
        SceneList,
        Unknown,
    };

    [[nodiscard]] FileKind fileKindOf(std::string_view fileName);
    /// "program", "sample", "multi", "song file", "set list", "scenelist" or "unknown". [RQ-APP-026]
    [[nodiscard]] std::string_view fileKindName(FileKind kind);

    /// A disk's type and format as the specification names them (section 10, Tables 20 and 21). [RQ-APP-024]
    [[nodiscard]] std::string_view diskTypeName(int type);
    [[nodiscard]] std::string_view diskFormatName(int format);

    /// A count of bytes with its thousands apart: "4 096 bytes". [RQ-APP-024, RQ-APP-026]
    [[nodiscard]] std::string bytesText(std::uint64_t bytes);
    /// A size in the unit that reads best: bytes under 1 KB ("512 bytes"), otherwise KB or MB with one decimal ("114.6 KB", "151.2 MB"), 1 KB being
    /// 1 024 bytes; a value that rounds to 1024.0 in its unit is written in the next one. [RQ-APP-081, TASK-APP-020]
    [[nodiscard]] std::string sizeText(std::uint64_t bytes);
    /// The free space of a disk, as `sizeText` writes it: "not reported" for 0, which the S5000 answered for a FAT32 disk that was not full.
    /// [RQ-APP-024, RQ-APP-081]
    [[nodiscard]] std::string freeSpaceText(std::uint64_t freeBytes);

    /// A folder's path as the folder names from the root, and as the sampler writes it: the names joined by a backslash, the root empty.
    /// [RQ-APP-025]
    [[nodiscard]] std::vector<std::string> splitPath(std::string_view samplerPath);
    [[nodiscard]] std::string pathText(const std::vector<std::string>& folders);

    /// What one folder of the current disk holds, as last listed.
    struct FolderListing
    {
        std::vector<std::string> folders;
        std::vector<mcp::DiskFileEntry> files;
    };

    /// What can be selected in the browser: a disk, or a folder or a file of the current disk, with the folder that holds it.
    enum class BrowserItemKind
    {
        Disk,
        Folder,
        File,
    };

    struct BrowserItem
    {
        BrowserItemKind kind = BrowserItemKind::File;
        std::vector<std::string> folder;  ///< the folder holding the item, from the root; empty for a disk and for the root's items
        std::string name;
    };

    /// The commands of the browser's context menu. [RQ-APP-027]
    enum class BrowserCommand
    {
        Open,
        Load,
        LoadWithDependents,
        LoadFolder,
        CreateFolder,
        Rename,
        Delete,
        Audition,
        RefreshDisks,
    };

    /// Only the commands that apply to `item`, in the order of the menu: for a file, load, load with its dependents, rename, delete, and
    /// audition for a sample; for a folder, open, load the folder, create a folder in it, rename, delete; for a disk, create a folder at its
    /// root and refresh the list of disks. [RQ-APP-027]
    [[nodiscard]] std::vector<BrowserCommand> commandsFor(const BrowserItem& item);

    /// A file of the browser: its name, its size and its kind. [RQ-APP-026]
    struct FileDetails
    {
        std::string name;
        std::uint32_t sizeBytes = 0;
        FileKind kind = FileKind::Unknown;
    };

    /// The browser's part of the model: the disks, the sampler's current disk with its free space, the sampler's current folder, the
    /// folders of the current disk listed so far (the tree), and the selection. [RQ-APP-024 to RQ-APP-026, ADR-APP-001 (DEC-APP-004)]
    struct BrowserState
    {
        std::vector<mcp::DiskEntry> disks;
        std::optional<int> currentDisk;           ///< the handle of the sampler's current disk
        std::optional<std::uint64_t> freeBytes;   ///< of the current disk, as read
        std::vector<std::string> currentFolder;   ///< the sampler's current folder, from the root
        std::map<std::string, FolderListing> listings;  ///< by `pathText` of the folder
        std::optional<BrowserItem> selection;

        [[nodiscard]] const mcp::DiskEntry* currentDiskEntry() const;
        [[nodiscard]] const FolderListing* listing(const std::vector<std::string>& folder) const;
        /// The selected file, when the selection is a file that its folder's listing holds.
        [[nodiscard]] std::optional<FileDetails> selectedFile() const;
    };
}
