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

// The disk unit of the sampler gateway: browsing the sampler's disks (section 10) with the long timeout of the slow commands.
// It is the only unit of the library that may call the disk primitives of browsing, loading and saving; the delete, rename,
// create-folder, eject and format primitives are called nowhere (checked by `CheckNoDestructiveCalls.cmake`). [RQ-MCP-024,
// RQ-MCP-028, RQ-MCP-029, ADR-MCP-003 (DEC-MCP-015, DEC-MCP-016, DEC-MCP-017, DEC-MCP-019)]
#include <algorithm>
#include <functional>
#include <utility>
#include <variant>

#include "GatewayDetail.hpp"
#include "akm/CommandOptions.hpp"
#include "akm/DiskPrimitives.hpp"
#include "akm/SamplerError.hpp"
#include "mcp/ParameterCatalogue.hpp"
#include "mcp/SamplerGateway.hpp"

namespace mcp
{
    using detail::await;
    using detail::explain;
    using detail::numberText;

    namespace
    {
        constexpr const char* DISK = "disk";

        std::string diskList(const std::vector<DiskEntry>& disks)
        {
            std::string text;
            for (std::size_t i = 0; i < disks.size(); ++i)
                text += (i == 0 ? "" : ", ") + std::string("\"") + disks[i].name + "\"";
            return text;
        }
    }

    std::chrono::milliseconds SamplerGateway::waitForDisk() const
    {
        return _config.diskTimeout + detail::WAIT_MARGIN;
    }

    akm::CommandOptions SamplerGateway::diskOptions() const
    {
        akm::CommandOptions options;
        options.timeout = _config.diskTimeout;
        options.maxTotalWait = _config.diskTimeout;
        return options;
    }

    std::string SamplerGateway::explainDisk(const akm::CommandResult& outcome, const std::string& doing) const
    {
        if (std::holds_alternative<akm::Timeout>(outcome))
            return "The sampler did not answer while " + doing + " (no reply within " + numberText(_config.diskTimeout.count()) +
                   " ms). A slow disk command can leave the sampler answering nothing: it may have to be switched off and on. "
                   "The command was not retried.";
        return explain(outcome, doing, _config, true, DISK);
    }

    Outcome<std::vector<DiskEntry>> SamplerGateway::listDisks(bool refresh)
    {
        using Disks = std::vector<DiskEntry>;
        if (const auto problem = connect())
            return Outcome<Disks>::failure(*problem);

        if (refresh)
        {
            const auto refreshed = await<akm::CommandResult>(waitForDisk(), [&](std::function<void(const akm::CommandResult&)> done) {
                akm::updateDiskList(session(), std::move(done), diskOptions());
            });
            if (!refreshed)
                return Outcome<Disks>::failure("The sampler session did not complete the refresh of the disk list in time.");
            if (!akm::succeeded(*refreshed))
                return Outcome<Disks>::failure(explainDisk(*refreshed, "refreshing the list of disks"));
        }

        const auto disks = await<akm::DiskListResult>(waitFor(1), [&](std::function<void(const akm::DiskListResult&)> done) {
            akm::getConnectedDisks(session(), std::move(done));
        });
        if (!disks)
            return Outcome<Disks>::failure("The sampler session did not complete the command in time.");
        if (!disks->disks)
            return Outcome<Disks>::failure(explain(disks->outcome, "listing the disks", _config, false));

        Disks entries;
        for (const akm::DiskInfo& info : *disks->disks)
            entries.push_back(DiskEntry{info.handle, info.name, info.type, info.format, info.writable, false});

        // Which one is current: a disk not selected yet is not an error, only "none".
        const auto current = await<akm::DiskHandleResult>(waitFor(1), [&](std::function<void(const akm::DiskHandleResult&)> done) {
            akm::getCurrentDiskHandle(session(), std::move(done));
        });
        if (current && current->handle)
        {
            for (DiskEntry& entry : entries)
                entry.current = entry.handle == *current->handle;
        }
        return Outcome<Disks>::success(std::move(entries));
    }

    Outcome<DiskEntry> SamplerGateway::selectDiskByHandle(int handle)
    {
        const auto disks = listDisks(false);
        if (!disks.ok())
            return Outcome<DiskEntry>::failure(disks.problem);
        if (disks.value->empty())
            return Outcome<DiskEntry>::failure("No disk is connected to the sampler.");
        const auto found = std::find_if(disks.value->begin(), disks.value->end(), [handle](const DiskEntry& disk) { return disk.handle == handle; });
        if (found == disks.value->end())
            return Outcome<DiskEntry>::failure("No disk has the handle " + numberText(handle) + ". The disks are: " + diskList(*disks.value) + ".");

        const auto selected = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::selectDisk(session(), handle, std::move(done));
        });
        if (!selected)
            return Outcome<DiskEntry>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*selected))
            return Outcome<DiskEntry>::failure(explain(*selected, "selecting the disk \"" + found->name + "\"", _config, false));
        DiskEntry entry = *found;
        entry.current = true;
        return Outcome<DiskEntry>::success(std::move(entry));
    }

    Outcome<DiskEntry> SamplerGateway::selectDiskByName(std::string_view name)
    {
        const auto disks = listDisks(false);
        if (!disks.ok())
            return Outcome<DiskEntry>::failure(disks.problem);
        if (disks.value->empty())
            return Outcome<DiskEntry>::failure("No disk is connected to the sampler.");
        const std::string wanted = normalizeText(name);
        const auto found = std::find_if(disks.value->begin(), disks.value->end(),
                                        [&wanted](const DiskEntry& disk) { return normalizeText(disk.name) == wanted; });
        if (found == disks.value->end())
            return Outcome<DiskEntry>::failure("No disk is named \"" + std::string(name) + "\". The disks are: " + diskList(*disks.value) + ".");
        return selectDiskByHandle(found->handle);
    }

    Outcome<std::vector<std::string>> SamplerGateway::folderNames()
    {
        using Names = std::vector<std::string>;
        const auto count = await<akm::DiskFolderCountResult>(waitFor(1), [&](std::function<void(const akm::DiskFolderCountResult&)> done) {
            akm::getFolderCount(session(), std::move(done));
        });
        if (!count)
            return Outcome<Names>::failure("The sampler session did not complete the command in time.");
        if (!count->count)
            return Outcome<Names>::failure(explain(count->outcome, "counting the folders", _config, true, DISK));
        if (*count->count == 0)
            return Outcome<Names>::success({});
        const auto names = await<akm::DiskFolderNamesResult>(waitFor(1), [&](std::function<void(const akm::DiskFolderNamesResult&)> done) {
            akm::getAllFolderNames(session(), std::move(done));
        });
        if (!names)
            return Outcome<Names>::failure("The sampler session did not complete the command in time.");
        if (!names->names)
            return Outcome<Names>::failure(explain(names->outcome, "reading the folder names", _config, true, DISK));
        return Outcome<Names>::success(*names->names);
    }

    Outcome<DiskContents> SamplerGateway::listDiskContents()
    {
        if (const auto problem = connect())
            return Outcome<DiskContents>::failure(*problem);

        const auto handle = await<akm::DiskHandleResult>(waitFor(1), [&](std::function<void(const akm::DiskHandleResult&)> done) {
            akm::getCurrentDiskHandle(session(), std::move(done));
        });
        if (!handle)
            return Outcome<DiskContents>::failure("The sampler session did not complete the command in time.");
        if (!handle->handle)
            return Outcome<DiskContents>::failure(explain(handle->outcome, "reading the current disk", _config, true, DISK));

        DiskContents contents;
        const auto name = await<akm::DiskNameResult>(waitFor(1), [&](std::function<void(const akm::DiskNameResult&)> done) {
            akm::getDiskName(session(), *handle->handle, std::move(done));
        });
        if (!name)
            return Outcome<DiskContents>::failure("The sampler session did not complete the command in time.");
        if (!name->name)
            return Outcome<DiskContents>::failure(explain(name->outcome, "reading the name of the current disk", _config, true, DISK));
        contents.diskName = *name->name;

        const auto path = await<akm::DiskPathResult>(waitFor(1), [&](std::function<void(const akm::DiskPathResult&)> done) {
            akm::getCurrentDiskPath(session(), std::move(done));
        });
        if (!path)
            return Outcome<DiskContents>::failure("The sampler session did not complete the command in time.");
        if (!path->path)
            return Outcome<DiskContents>::failure(explain(path->outcome, "reading the current folder", _config, true, DISK));
        contents.path = *path->path;

        const auto folders = folderNames();
        if (!folders.ok())
            return Outcome<DiskContents>::failure(folders.problem);
        contents.folders = *folders.value;

        const auto fileCount = await<akm::DiskFileCountResult>(waitFor(1), [&](std::function<void(const akm::DiskFileCountResult&)> done) {
            akm::getFileCount(session(), std::move(done));
        });
        if (!fileCount)
            return Outcome<DiskContents>::failure("The sampler session did not complete the command in time.");
        if (!fileCount->count)
            return Outcome<DiskContents>::failure(explain(fileCount->outcome, "counting the files", _config, true, DISK));
        if (*fileCount->count > 0)
        {
            const auto names = await<akm::DiskFileNamesResult>(waitFor(1), [&](std::function<void(const akm::DiskFileNamesResult&)> done) {
                akm::getAllFileNames(session(), std::move(done));
            });
            if (!names)
                return Outcome<DiskContents>::failure("The sampler session did not complete the command in time.");
            if (!names->names)
                return Outcome<DiskContents>::failure(explain(names->outcome, "reading the file names", _config, true, DISK));
            for (std::size_t i = 0; i < names->names->size(); ++i)
            {
                const auto size = await<akm::DiskFileSizeResult>(waitFor(1), [&](std::function<void(const akm::DiskFileSizeResult&)> done) {
                    akm::getFileSize(session(), static_cast<int>(i), std::move(done));
                });
                if (!size)
                    return Outcome<DiskContents>::failure("The sampler session did not complete the command in time.");
                if (!size->sizeBytes)
                    return Outcome<DiskContents>::failure(explain(size->outcome, "reading the size of \"" + (*names->names)[i] + "\"", _config, true, DISK));
                contents.files.push_back(DiskFileEntry{(*names->names)[i], *size->sizeBytes});
            }
        }
        return Outcome<DiskContents>::success(std::move(contents));
    }

    Outcome<DiskSpace> SamplerGateway::readDiskSpace()
    {
        const auto here = listDiskContents();
        if (!here.ok())
            return Outcome<DiskSpace>::failure(here.problem);
        const auto space = await<akm::DiskFreeSpaceResult>(waitFor(1), [&](std::function<void(const akm::DiskFreeSpaceResult&)> done) {
            akm::getCurrentDiskFreeSpace(session(), std::move(done));
        });
        if (!space)
            return Outcome<DiskSpace>::failure("The sampler session did not complete the command in time.");
        if (!space->freeBytes)
            return Outcome<DiskSpace>::failure(explain(space->outcome, "reading the free space of the disk \"" + here.value->diskName + "\"", _config, true, DISK));
        return Outcome<DiskSpace>::success(DiskSpace{here.value->diskName, *space->freeBytes});
    }

    Outcome<DiskContents> SamplerGateway::openFolder(std::string_view name)
    {
        if (const auto problem = connect())
            return Outcome<DiskContents>::failure(*problem);

        const auto folders = folderNames();
        if (!folders.ok())
            return Outcome<DiskContents>::failure(folders.problem);
        const std::string wanted = normalizeText(name);
        const auto found = std::find_if(folders.value->begin(), folders.value->end(),
                                        [&wanted](const std::string& folder) { return normalizeText(folder) == wanted; });
        if (found == folders.value->end())
        {
            std::string text = "There is no folder named \"" + std::string(name) + "\" here.";
            if (folders.value->empty())
                return Outcome<DiskContents>::failure(text + " The current folder has no sub-folder.");
            text += " Its folders are: ";
            for (std::size_t i = 0; i < folders.value->size(); ++i)
                text += (i == 0 ? "" : ", ") + std::string("\"") + (*folders.value)[i] + "\"";
            return Outcome<DiskContents>::failure(text + ".");
        }

        const auto opened = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::openFolder(session(), *found, std::move(done));
        });
        if (!opened)
            return Outcome<DiskContents>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*opened))
            return Outcome<DiskContents>::failure(explain(*opened, "opening the folder \"" + *found + "\"", _config, true, DISK));
        return listDiskContents();
    }

    Outcome<DiskContents> SamplerGateway::closeFolder()
    {
        if (const auto problem = connect())
            return Outcome<DiskContents>::failure(*problem);

        const auto path = await<akm::DiskPathResult>(waitFor(1), [&](std::function<void(const akm::DiskPathResult&)> done) {
            akm::getCurrentDiskPath(session(), std::move(done));
        });
        if (!path)
            return Outcome<DiskContents>::failure("The sampler session did not complete the command in time.");
        if (!path->path)
            return Outcome<DiskContents>::failure(explain(path->outcome, "reading the current folder", _config, true, DISK));
        if (path->path->empty())
            return Outcome<DiskContents>::failure("The current folder is already the root of the disk: there is no folder above it.");

        const auto closed = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::closeFolder(session(), std::move(done));
        });
        if (!closed)
            return Outcome<DiskContents>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*closed))
            return Outcome<DiskContents>::failure(explain(*closed, "closing the current folder", _config, true, DISK));
        return listDiskContents();
    }

    Outcome<MemoryNames> SamplerGateway::memoryNames()
    {
        MemoryNames names;
        const auto programs = listPrograms();
        if (!programs.ok())
            return Outcome<MemoryNames>::failure(programs.problem);
        for (const ProgramEntry& program : *programs.value)
            names.programs.push_back(program.name);
        const auto samples = listSamples();
        if (!samples.ok())
            return Outcome<MemoryNames>::failure(samples.problem);
        for (const SampleEntry& sample : samples.value->samples)
            names.samples.push_back(sample.name);
        const auto multis = listMultis();
        if (!multis.ok())
            return Outcome<MemoryNames>::failure(multis.problem);
        for (const MultiEntry& multi : multis.value->multis)
            names.multis.push_back(multi.name);
        return Outcome<MemoryNames>::success(std::move(names));
    }

    Outcome<LoadOutcome> SamplerGateway::loadFile(std::string_view name, bool withDependents, SampleLoadMode mode)
    {
        const auto contents = listDiskContents();
        if (!contents.ok())
            return Outcome<LoadOutcome>::failure(contents.problem);
        const std::string wanted = normalizeText(name);
        const auto found = std::find_if(contents.value->files.begin(), contents.value->files.end(),
                                        [&wanted](const DiskFileEntry& file) { return normalizeText(file.name) == wanted; });
        if (found == contents.value->files.end())
        {
            std::string text = "There is no file named \"" + std::string(name) + "\" in the current folder of the disk \"" +
                               contents.value->diskName + "\".";
            if (contents.value->files.empty())
                return Outcome<LoadOutcome>::failure(text + " The folder holds no file.");
            text += " Its files are: ";
            for (std::size_t i = 0; i < contents.value->files.size(); ++i)
                text += (i == 0 ? "" : ", ") + std::string("\"") + contents.value->files[i].name + "\"";
            return Outcome<LoadOutcome>::failure(text + ".");
        }

        const auto before = memoryNames();
        if (!before.ok())
            return Outcome<LoadOutcome>::failure(before.problem);

        const akm::SampleLoadOption option = mode == SampleLoadMode::Ram       ? akm::SampleLoadOption::Ram
                                             : mode == SampleLoadMode::Virtual ? akm::SampleLoadOption::Virtual
                                                                               : akm::SampleLoadOption::Normal;
        const auto loaded = await<akm::CommandResult>(waitForDisk(), [&](std::function<void(const akm::CommandResult&)> done) {
            if (withDependents)
                akm::loadFileWithDependents(session(), found->name, std::move(done), diskOptions());
            else
                akm::loadFile(session(), found->name, option, std::move(done), diskOptions());
        });
        if (!loaded)
            return Outcome<LoadOutcome>::failure("The sampler session did not complete the load in time.");
        if (!akm::succeeded(*loaded))
            return Outcome<LoadOutcome>::failure(explainDisk(*loaded, "loading the file \"" + found->name + "\""));

        const auto after = memoryNames();
        if (!after.ok())
            return Outcome<LoadOutcome>::failure("The file was loaded but the memory could not be read afterwards: " + after.problem);
        return Outcome<LoadOutcome>::success(LoadOutcome{contents.value->diskName, contents.value->path, *before.value, *after.value});
    }

    Outcome<LoadOutcome> SamplerGateway::loadFolder(std::string_view name)
    {
        const auto contents = listDiskContents();
        if (!contents.ok())
            return Outcome<LoadOutcome>::failure(contents.problem);
        const std::string wanted = normalizeText(name);
        const auto found = std::find_if(contents.value->folders.begin(), contents.value->folders.end(),
                                        [&wanted](const std::string& folder) { return normalizeText(folder) == wanted; });
        if (found == contents.value->folders.end())
        {
            std::string text = "There is no folder named \"" + std::string(name) + "\" in the current folder of the disk \"" +
                               contents.value->diskName + "\".";
            if (contents.value->folders.empty())
                return Outcome<LoadOutcome>::failure(text + " The current folder has no sub-folder.");
            text += " Its folders are: ";
            for (std::size_t i = 0; i < contents.value->folders.size(); ++i)
                text += (i == 0 ? "" : ", ") + std::string("\"") + contents.value->folders[i] + "\"";
            return Outcome<LoadOutcome>::failure(text + ".");
        }

        const auto before = memoryNames();
        if (!before.ok())
            return Outcome<LoadOutcome>::failure(before.problem);
        const auto loaded = await<akm::CommandResult>(waitForDisk(), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::loadFolder(session(), *found, std::move(done), diskOptions());
        });
        if (!loaded)
            return Outcome<LoadOutcome>::failure("The sampler session did not complete the load in time.");
        if (!akm::succeeded(*loaded))
            return Outcome<LoadOutcome>::failure(explainDisk(*loaded, "loading the folder \"" + *found + "\""));

        const auto after = memoryNames();
        if (!after.ok())
            return Outcome<LoadOutcome>::failure("The folder was loaded but the memory could not be read afterwards: " + after.problem);
        return Outcome<LoadOutcome>::success(LoadOutcome{contents.value->diskName, contents.value->path, *before.value, *after.value});
    }

    namespace
    {
        const char* kindName(SaveKind kind)
        {
            switch (kind)
            {
                case SaveKind::Program:
                    return "program";
                case SaveKind::Sample:
                    return "sample";
                case SaveKind::Multi:
                    return "multi";
            }
            return "item";
        }

        akm::SaveableMemoryType saveType(SaveKind kind)
        {
            switch (kind)
            {
                case SaveKind::Program:
                    return akm::SaveableMemoryType::Program;
                case SaveKind::Sample:
                    return akm::SaveableMemoryType::Sample;
                case SaveKind::Multi:
                    return akm::SaveableMemoryType::Multi;
            }
            return akm::SaveableMemoryType::Program;
        }

        const std::vector<std::string>& namesOfKind(const MemoryNames& memory, SaveKind kind)
        {
            switch (kind)
            {
                case SaveKind::Program:
                    return memory.programs;
                case SaveKind::Sample:
                    return memory.samples;
                case SaveKind::Multi:
                    return memory.multis;
            }
            return memory.programs;
        }

        /// A file's name without its extension.
        std::string baseName(const std::string& fileName)
        {
            const auto dot = fileName.rfind('.');
            return dot == std::string::npos ? fileName : fileName.substr(0, dot);
        }

        const DiskFileEntry* fileBearing(const std::vector<DiskFileEntry>& files, const std::string& itemName)
        {
            const std::string wanted = normalizeText(itemName);
            for (const DiskFileEntry& file : files)
            {
                if (normalizeText(baseName(file.name)) == wanted)
                    return &file;
            }
            return nullptr;
        }

        std::string quotedList(const std::vector<std::string>& names)
        {
            std::string text;
            for (std::size_t i = 0; i < names.size(); ++i)
                text += (i == 0 ? "" : ", ") + std::string("\"") + names[i] + "\"";
            return text;
        }
    }

    Outcome<DiskContents> SamplerGateway::reopenCurrentFolder()
    {
        const auto path = await<akm::DiskPathResult>(waitFor(1), [&](std::function<void(const akm::DiskPathResult&)> done) {
            akm::getCurrentDiskPath(session(), std::move(done));
        });
        if (!path)
            return Outcome<DiskContents>::failure("The sampler session did not complete the command in time.");
        if (!path->path)
            return Outcome<DiskContents>::failure(explain(path->outcome, "reading the current folder", _config, true, DISK));

        // The folder to open again is the last name of the path (the sampler writes it with '\'; '/' is accepted too).
        std::string last;
        const bool atRoot = path->path->empty();
        if (!atRoot)
        {
            const std::size_t cut = path->path->find_last_of("/\\");
            last = cut == std::string::npos ? *path->path : path->path->substr(cut + 1);
            const auto closed = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
                akm::closeFolder(session(), std::move(done));
            });
            if (!closed)
                return Outcome<DiskContents>::failure("The sampler session did not complete the command in time.");
            if (!akm::succeeded(*closed))
                return Outcome<DiskContents>::failure(explain(*closed, "closing the current folder", _config, true, DISK));
        }
        const auto opened = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::openFolder(session(), last, std::move(done));
        });
        if (!opened)
            return Outcome<DiskContents>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*opened))
            return Outcome<DiskContents>::failure(explain(*opened, "opening the folder again", _config, true, DISK));
        return listDiskContents();
    }

    namespace
    {
        /// A file of the folder by its whole name, the extension included, compared without regard to case, spaces or hyphens.
        const DiskFileEntry* fileNamed(const std::vector<DiskFileEntry>& files, std::string_view name)
        {
            const std::string wanted = normalizeText(name);
            for (const DiskFileEntry& file : files)
            {
                if (normalizeText(file.name) == wanted)
                    return &file;
            }
            return nullptr;
        }

        const std::string* folderNamed(const std::vector<std::string>& folders, std::string_view name)
        {
            const std::string wanted = normalizeText(name);
            for (const std::string& folder : folders)
            {
                if (normalizeText(folder) == wanted)
                    return &folder;
            }
            return nullptr;
        }

        bool endsWithIgnoringCase(const std::string& text, const std::string& tail)
        {
            if (tail.empty() || text.size() < tail.size())
                return false;
            return std::equal(tail.begin(), tail.end(), text.end() - static_cast<std::ptrdiff_t>(tail.size()),
                              [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
        }

        std::string noSuchFile(const DiskContents& here, std::string_view name)
        {
            std::string text = "There is no file named \"" + std::string(name) + "\" in the current folder of the disk \"" + here.diskName + "\".";
            if (here.files.empty())
                return text + " The folder holds no file.";
            std::vector<std::string> names;
            for (const DiskFileEntry& file : here.files)
                names.push_back(file.name);
            return text + " Its files are: " + quotedList(names) + ".";
        }

        std::string noSuchFolder(const DiskContents& here, std::string_view name)
        {
            std::string text = "There is no folder named \"" + std::string(name) + "\" in the current folder of the disk \"" + here.diskName + "\".";
            if (here.folders.empty())
                return text + " The folder holds no sub-folder.";
            return text + " Its folders are: " + quotedList(here.folders) + ".";
        }

        bool listingShows(ListingCheck check, const DiskContents& contents, std::string_view name)
        {
            switch (check)
            {
                case ListingCheck::FileThere:
                    return fileNamed(contents.files, name) != nullptr;
                case ListingCheck::FileGone:
                    return fileNamed(contents.files, name) == nullptr;
                case ListingCheck::FolderThere:
                    return folderNamed(contents.folders, name) != nullptr;
                case ListingCheck::FolderGone:
                    return folderNamed(contents.folders, name) == nullptr;
            }
            return false;
        }
    }

    Outcome<DiskContents> SamplerGateway::listAfterChange(ListingCheck check, std::string_view name)
    {
        const auto listed = listDiskContents();
        if (!listed.ok() || listingShows(check, *listed.value, name))
            return listed;
        return reopenCurrentFolder();
    }

    Outcome<DiskChange> SamplerGateway::renameFile(std::string_view name, std::string_view newName)
    {
        const auto here = writableFolder();
        if (!here.ok())
            return Outcome<DiskChange>::failure(here.problem);
        const DiskFileEntry* file = fileNamed(here.value->files, name);
        if (file == nullptr)
            return Outcome<DiskChange>::failure(noSuchFile(*here.value, name));

        const std::string extension = file->name.substr(baseName(file->name).size());
        const std::string wantedNew(newName);
        if (endsWithIgnoringCase(wantedNew, extension))
            return Outcome<DiskChange>::failure("The sampler keeps the file's own extension (\"" + extension + "\") and adds it to the new name: give \"" +
                                                wantedNew.substr(0, wantedNew.size() - extension.size()) + "\" and not \"" + wantedNew + "\".");
        const std::string expected = wantedNew + extension;
        for (const DiskFileEntry& other : here.value->files)
        {
            if (&other != file && normalizeText(other.name) == normalizeText(expected))
                return Outcome<DiskChange>::failure("The current folder already holds a file named \"" + other.name + "\": nothing was renamed. Choose another name.");
        }
        if (const std::string* folder = folderNamed(here.value->folders, expected))
            return Outcome<DiskChange>::failure("The current folder already holds a folder named \"" + *folder + "\": nothing was renamed. Choose another name.");

        const auto renamed = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::renameFile(session(), file->name, newName, std::move(done));
        });
        if (!renamed)
            return Outcome<DiskChange>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*renamed))
            return Outcome<DiskChange>::failure(explain(*renamed, "renaming the file \"" + file->name + "\"", _config, true, DISK));

        const auto after = listAfterChange(ListingCheck::FileThere, expected);
        if (!after.ok())
            return Outcome<DiskChange>::failure("The sampler accepted the rename but the folder could not be read afterwards: " + after.problem);
        const DiskFileEntry* now = fileNamed(after.value->files, expected);
        if (now == nullptr)
            return Outcome<DiskChange>::failure("The sampler accepted the rename but no file named \"" + expected + "\" is in the folder afterwards: check the disk.");
        DiskChange change;
        change.diskName = here.value->diskName;
        change.path = here.value->path;
        change.name = file->name;
        change.newName = now->name;
        return Outcome<DiskChange>::success(std::move(change));
    }

    Outcome<DiskChange> SamplerGateway::renameFolder(std::string_view name, std::string_view newName)
    {
        const auto here = writableFolder();
        if (!here.ok())
            return Outcome<DiskChange>::failure(here.problem);
        const std::string* folder = folderNamed(here.value->folders, name);
        if (folder == nullptr)
            return Outcome<DiskChange>::failure(noSuchFolder(*here.value, name));

        for (const std::string& other : here.value->folders)
        {
            if (&other != folder && normalizeText(other) == normalizeText(newName))
                return Outcome<DiskChange>::failure("The current folder already holds a folder named \"" + other + "\": nothing was renamed. Choose another name.");
        }
        if (const DiskFileEntry* file = fileNamed(here.value->files, newName))
            return Outcome<DiskChange>::failure("The current folder already holds a file named \"" + file->name + "\": nothing was renamed. Choose another name.");

        const auto renamed = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::renameFolder(session(), *folder, newName, std::move(done));
        });
        if (!renamed)
            return Outcome<DiskChange>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*renamed))
            return Outcome<DiskChange>::failure(explain(*renamed, "renaming the folder \"" + *folder + "\"", _config, true, DISK));

        const auto after = listAfterChange(ListingCheck::FolderThere, newName);
        if (!after.ok())
            return Outcome<DiskChange>::failure("The sampler accepted the rename but the folder could not be read afterwards: " + after.problem);
        const std::string* now = folderNamed(after.value->folders, newName);
        if (now == nullptr)
            return Outcome<DiskChange>::failure("The sampler accepted the rename but no folder named \"" + std::string(newName) + "\" is in the folder afterwards: check the disk.");
        DiskChange change;
        change.diskName = here.value->diskName;
        change.path = here.value->path;
        change.name = *folder;
        change.newName = *now;
        return Outcome<DiskChange>::success(std::move(change));
    }

    Outcome<DiskChange> SamplerGateway::deleteFile(std::string_view name, std::string_view confirm)
    {
        const auto here = writableFolder();
        if (!here.ok())
            return Outcome<DiskChange>::failure(here.problem);
        const DiskFileEntry* file = fileNamed(here.value->files, name);
        if (file == nullptr)
            return Outcome<DiskChange>::failure(noSuchFile(*here.value, name));
        DiskChange change;
        change.diskName = here.value->diskName;
        change.path = here.value->path;
        change.name = file->name;
        if (file->name != confirm)
        {
            change.done = false;
            return Outcome<DiskChange>::success(std::move(change));
        }

        const auto deleted = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::deleteFile(session(), change.name, akm::ConfirmDeleteFile::IUnderstandThisDeletesTheFile, std::move(done));
        });
        if (!deleted)
            return Outcome<DiskChange>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*deleted))
            return Outcome<DiskChange>::failure(explain(*deleted, "deleting the file \"" + change.name + "\"", _config, true, DISK));

        const auto after = listAfterChange(ListingCheck::FileGone, change.name);
        if (!after.ok())
            return Outcome<DiskChange>::failure("The sampler accepted the deletion but the folder could not be read afterwards: " + after.problem);
        if (fileNamed(after.value->files, change.name) != nullptr)
            return Outcome<DiskChange>::failure("The sampler accepted the deletion but the file \"" + change.name + "\" is still in the folder afterwards: check the disk.");
        return Outcome<DiskChange>::success(std::move(change));
    }

    Outcome<DiskChange> SamplerGateway::deleteFolder(std::string_view name, std::string_view confirm, bool deleteContents)
    {
        const auto here = writableFolder();
        if (!here.ok())
            return Outcome<DiskChange>::failure(here.problem);
        const std::string* folder = folderNamed(here.value->folders, name);
        if (folder == nullptr)
            return Outcome<DiskChange>::failure(noSuchFolder(*here.value, name));
        DiskChange change;
        change.diskName = here.value->diskName;
        change.path = here.value->path;
        change.name = *folder;
        if (*folder != confirm)
        {
            change.done = false;
            return Outcome<DiskChange>::success(std::move(change));
        }

        // What the folder holds: it is opened to count and closed again, so that the current folder is the same afterwards.
        const auto inside = openFolder(change.name);
        if (!inside.ok())
            return Outcome<DiskChange>::failure("The folder \"" + change.name + "\" could not be looked at before its deletion: " + inside.problem);
        change.files = static_cast<int>(inside.value->files.size());
        change.folders = static_cast<int>(inside.value->folders.size());
        const auto back = closeFolder();
        if (!back.ok())
            return Outcome<DiskChange>::failure("The folder \"" + change.name + "\" was looked at but the sampler could not go back up: " + back.problem);
        if ((change.files > 0 || change.folders > 0) && !deleteContents)
        {
            change.done = false;
            change.notEmpty = true;
            return Outcome<DiskChange>::success(std::move(change));
        }

        const auto deleted = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::deleteSubFolder(session(), change.name, akm::ConfirmDeleteSubFolder::IUnderstandThisDeletesTheFolderAndEverythingInIt, std::move(done));
        });
        if (!deleted)
            return Outcome<DiskChange>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*deleted))
            return Outcome<DiskChange>::failure(explain(*deleted, "deleting the folder \"" + change.name + "\"", _config, true, DISK));

        const auto after = listAfterChange(ListingCheck::FolderGone, change.name);
        if (!after.ok())
            return Outcome<DiskChange>::failure("The sampler accepted the deletion but the folder could not be read afterwards: " + after.problem);
        if (folderNamed(after.value->folders, change.name) != nullptr)
            return Outcome<DiskChange>::failure("The sampler accepted the deletion but the folder \"" + change.name + "\" is still listed afterwards: check the disk.");
        return Outcome<DiskChange>::success(std::move(change));
    }

    Outcome<std::string> SamplerGateway::startFileAudition(std::string_view name)
    {
        const auto here = listDiskContents();
        if (!here.ok())
            return Outcome<std::string>::failure(here.problem);
        const DiskFileEntry* found = fileNamed(here.value->files, name);
        if (found == nullptr)
            return Outcome<std::string>::failure(noSuchFile(*here.value, name));
        const int index = static_cast<int>(found - here.value->files.data());
        const auto started = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::startFileAudition(session(), index, std::move(done));
        });
        if (!started)
            return Outcome<std::string>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*started))
            return Outcome<std::string>::failure(explain(*started, "starting the audition of the file \"" + found->name + "\"", _config, true, DISK));
        return Outcome<std::string>::success(found->name);
    }

    Outcome<bool> SamplerGateway::stopFileAudition()
    {
        if (const auto problem = connect())
            return Outcome<bool>::failure(*problem);
        const auto stopped = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::stopFileAudition(session(), std::move(done));
        });
        if (!stopped)
            return Outcome<bool>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*stopped))
            return Outcome<bool>::failure(explain(*stopped, "stopping the audition of the file", _config, true, DISK));
        return Outcome<bool>::success(true);
    }

    Outcome<DiskContents> SamplerGateway::createFolder(std::string_view name)
    {
        const auto here = writableFolder();
        if (!here.ok())
            return Outcome<DiskContents>::failure(here.problem);

        const std::string wanted = normalizeText(name);
        for (const std::string& folder : here.value->folders)
        {
            if (normalizeText(folder) == wanted)
                return Outcome<DiskContents>::failure("The current folder of the disk \"" + here.value->diskName + "\" already holds a folder named \"" +
                                                      folder + "\": nothing was created.");
        }
        for (const DiskFileEntry& file : here.value->files)
        {
            if (normalizeText(file.name) == wanted)
                return Outcome<DiskContents>::failure("The current folder of the disk \"" + here.value->diskName + "\" already holds a file named \"" +
                                                      file.name + "\": nothing was created. Choose another name.");
        }

        const auto created = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::createFolder(session(), name, std::move(done));
        });
        if (!created)
            return Outcome<DiskContents>::failure("The sampler session did not complete the creation of the folder in time.");
        if (!akm::succeeded(*created))
            return Outcome<DiskContents>::failure(explain(*created, "creating the folder \"" + std::string(name) + "\"", _config, true, DISK));

        const auto after = listDiskContents();
        if (!after.ok())
            return Outcome<DiskContents>::failure("The sampler accepted the creation of the folder \"" + std::string(name) +
                                                  "\" but the folder could not be listed afterwards: " + after.problem);
        const bool present = std::any_of(after.value->folders.begin(), after.value->folders.end(),
                                         [&wanted](const std::string& folder) { return normalizeText(folder) == wanted; });
        if (!present)
            return Outcome<DiskContents>::failure("The sampler accepted the creation of the folder \"" + std::string(name) +
                                                  "\" but no such folder is in the listing afterwards: check the disk.");
        return after;
    }

    Outcome<SaveOutcome> SamplerGateway::saveMemoryItem(SaveKind kind, std::string_view name, bool overwrite, bool saveChildren)
    {
        const auto target = writableFolder();
        if (!target.ok())
            return Outcome<SaveOutcome>::failure(target.problem);

        const auto memory = memoryNames();
        if (!memory.ok())
            return Outcome<SaveOutcome>::failure(memory.problem);
        const std::vector<std::string>& names = namesOfKind(*memory.value, kind);
        const std::string wanted = normalizeText(name);
        const auto found = std::find_if(names.begin(), names.end(), [&wanted](const std::string& item) { return normalizeText(item) == wanted; });
        if (found == names.end())
        {
            std::string text = "No " + std::string(kindName(kind)) + " is named \"" + std::string(name) + "\" in the sampler's memory.";
            if (names.empty())
                return Outcome<SaveOutcome>::failure(text + " The memory holds none.");
            return Outcome<SaveOutcome>::failure(text + " It holds: " + quotedList(names) + ".");
        }
        const int index = static_cast<int>(found - names.begin());

        if (const DiskFileEntry* existing = fileBearing(target.value->files, *found); existing != nullptr && !overwrite)
            return Outcome<SaveOutcome>::failure("The current folder of the disk \"" + target.value->diskName + "\" already holds the file \"" +
                                                 existing->name + "\": nothing was saved. Pass overwrite true to replace it.");

        const auto saved = await<akm::CommandResult>(waitForDisk(), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::saveMemoryItem(session(), index, saveType(kind), overwrite, saveChildren, std::move(done), diskOptions());
        });
        if (!saved)
            return Outcome<SaveOutcome>::failure("The sampler session did not complete the save in time.");
        if (!akm::succeeded(*saved))
            return Outcome<SaveOutcome>::failure(explainDisk(*saved, "saving the " + std::string(kindName(kind)) + " \"" + *found + "\""));

        auto after = listDiskContents();
        if (!after.ok())
            return Outcome<SaveOutcome>::failure("The save was sent but the folder could not be read afterwards: " + after.problem);
        // The S5000 may keep the folder's old file list: when the file is not in it, the folder is opened again and read once more.
        if (fileBearing(after.value->files, *found) == nullptr)
        {
            const auto reopened = reopenCurrentFolder();
            if (!reopened.ok())
                return Outcome<SaveOutcome>::failure("The save was sent but the folder could not be read again afterwards: " + reopened.problem);
            after = reopened;
        }
        SaveOutcome outcome;
        outcome.diskName = target.value->diskName;
        outcome.path = target.value->path;
        outcome.itemName = *found;
        outcome.filesBefore = target.value->files;
        outcome.filesAfter = after.value->files;
        if (const DiskFileEntry* file = fileBearing(after.value->files, *found))
            outcome.savedFile = *file;
        return Outcome<SaveOutcome>::success(std::move(outcome));
    }

    Outcome<SaveOutcome> SamplerGateway::saveAllMemoryItems(SaveKind kind, int confirm, bool overwrite, bool saveChildren)
    {
        const auto target = writableFolder();
        if (!target.ok())
            return Outcome<SaveOutcome>::failure(target.problem);

        const auto memory = memoryNames();
        if (!memory.ok())
            return Outcome<SaveOutcome>::failure(memory.problem);
        const std::vector<std::string>& names = namesOfKind(*memory.value, kind);
        if (static_cast<int>(names.size()) != confirm)
            return Outcome<SaveOutcome>::failure("There are " + numberText(static_cast<std::int64_t>(names.size())) + " " + kindName(kind) +
                                                 (names.size() == 1 ? "" : "s") + " in the sampler's memory, not " + numberText(confirm) +
                                                 ": nothing was saved. Give the number there are as confirm.");
        if (names.empty())
            return Outcome<SaveOutcome>::failure("The sampler's memory holds no " + std::string(kindName(kind)) + ": nothing to save.");

        if (!overwrite)
        {
            std::vector<std::string> conflicts;
            for (const std::string& item : names)
            {
                if (const DiskFileEntry* existing = fileBearing(target.value->files, item))
                    conflicts.push_back(existing->name);
            }
            if (!conflicts.empty())
                return Outcome<SaveOutcome>::failure("The current folder of the disk \"" + target.value->diskName + "\" already holds " +
                                                     quotedList(conflicts) + ": nothing was saved. Pass overwrite true to replace them, or save "
                                                     "the items one by one.");
        }

        const auto saved = await<akm::CommandResult>(waitForDisk(), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::saveAllMemoryItems(session(), saveType(kind), overwrite, saveChildren, std::move(done), diskOptions());
        });
        if (!saved)
            return Outcome<SaveOutcome>::failure("The sampler session did not complete the save in time.");
        if (!akm::succeeded(*saved))
            return Outcome<SaveOutcome>::failure(explainDisk(*saved, "saving every " + std::string(kindName(kind))));

        auto after = listDiskContents();
        if (!after.ok())
            return Outcome<SaveOutcome>::failure("The save was sent but the folder could not be read afterwards: " + after.problem);
        // Same as for one item: when a file of an item is missing from the listing, the folder is opened again and read once more.
        const auto anyMissing = [&names](const DiskContents& contents) {
            return std::any_of(names.begin(), names.end(), [&contents](const std::string& item) { return fileBearing(contents.files, item) == nullptr; });
        };
        if (anyMissing(*after.value))
        {
            const auto reopened = reopenCurrentFolder();
            if (!reopened.ok())
                return Outcome<SaveOutcome>::failure("The save was sent but the folder could not be read again afterwards: " + reopened.problem);
            after = reopened;
        }
        SaveOutcome outcome;
        outcome.diskName = target.value->diskName;
        outcome.path = target.value->path;
        outcome.itemCount = confirm;
        outcome.filesBefore = target.value->files;
        outcome.filesAfter = after.value->files;
        return Outcome<SaveOutcome>::success(std::move(outcome));
    }

    Outcome<DiskContents> SamplerGateway::writableFolder()
    {
        const auto contents = listDiskContents();
        if (!contents.ok())
            return Outcome<DiskContents>::failure(contents.problem);
        const auto disks = listDisks(false);
        if (!disks.ok())
            return Outcome<DiskContents>::failure(disks.problem);
        const auto current = std::find_if(disks.value->begin(), disks.value->end(), [](const DiskEntry& disk) { return disk.current; });
        if (current == disks.value->end())
            return Outcome<DiskContents>::failure("No disk is selected: use select_disk first.");
        if (!current->writable)
            return Outcome<DiskContents>::failure("The disk \"" + current->name + "\" is read-only: nothing can be saved to it. Select a writable disk.");
        return contents;
    }
}
