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
#include "app/FileBrowser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <utility>

namespace app
{
    // [RQ-APP-017, RQ-APP-024 to RQ-APP-027, ADR-APP-001 (DEC-APP-004), TASK-APP-013]
    namespace
    {
        constexpr const char* LISTING_LABEL = "Listing the disks";
        constexpr const char* SELECTING_LABEL = "Selecting a disk";
        constexpr const char* OPENING_LABEL = "Opening the folder ";
        constexpr const char* ROOT_NAME = "(root)";

        /// What the browser shows of the disks: the list, and for the current disk its free space and its current folder.
        struct DiskView
        {
            std::vector<mcp::DiskEntry> disks;
            std::optional<int> currentDisk;
            std::optional<std::uint64_t> freeBytes;
            std::optional<mcp::DiskContents> contents;
        };

        mcp::Outcome<DiskView> readDisks(mcp::SamplerGateway& gateway)
        {
            auto disks = gateway.listDisks(false);
            if (!disks.ok())
                return mcp::Outcome<DiskView>::failure(disks.problem);
            DiskView view;
            view.disks = std::move(*disks.value);
            const auto current = std::find_if(view.disks.begin(), view.disks.end(), [](const mcp::DiskEntry& disk) { return disk.current; });
            if (current == view.disks.end())
                return mcp::Outcome<DiskView>::success(std::move(view));
            view.currentDisk = current->handle;
            auto contents = gateway.listDiskContents();
            if (!contents.ok())
                return mcp::Outcome<DiskView>::failure(contents.problem);
            const auto space = gateway.readDiskSpace();
            if (!space.ok())
                return mcp::Outcome<DiskView>::failure(space.problem);
            view.contents = std::move(*contents.value);
            view.freeBytes = space.value->freeBytes;
            return mcp::Outcome<DiskView>::success(std::move(view));
        }

        /// From the sampler's current folder up to the folder both paths share, then down to `target`; the last listing is the target's.
        mcp::Outcome<mcp::DiskContents> moveTo(mcp::SamplerGateway& gateway, const std::vector<std::string>& target)
        {
            auto contents = gateway.listDiskContents();
            if (!contents.ok())
                return contents;
            const std::vector<std::string> here = splitPath(contents.value->path);
            const auto shared = static_cast<std::size_t>(std::mismatch(here.begin(), here.end(), target.begin(), target.end()).first - here.begin());
            for (std::size_t level = here.size(); level > shared; --level)
            {
                contents = gateway.closeFolder();
                if (!contents.ok())
                    return contents;
            }
            for (std::size_t level = shared; level < target.size(); ++level)
            {
                contents = gateway.openFolder(target[level]);
                if (!contents.ok())
                    return contents;
            }
            return contents;
        }

        void record(BrowserState& state, const mcp::DiskContents& contents)
        {
            state.currentFolder = splitPath(contents.path);
            state.listings[pathText(state.currentFolder)] = FolderListing{contents.folders, contents.files};
            // The current folder and the folders on its path are shown: none of them stays folded. [RQ-APP-079, TASK-APP-021]
            for (std::size_t depth = 1; depth <= state.currentFolder.size(); ++depth)
                state.folded.erase(pathText({state.currentFolder.begin(), state.currentFolder.begin() + static_cast<std::ptrdiff_t>(depth)}));
        }

        constexpr const char* LOADING_LABEL = "Loading ";
        constexpr const char* CREATING_LABEL = "Creating the folder ";
        constexpr const char* RENAMING_LABEL = "Renaming ";
        constexpr const char* DELETING_LABEL = "Deleting ";
        constexpr const char* AUDITIONING_LABEL = "Auditioning ";
        constexpr const char* STOPPING_LABEL = "Stopping the audition";
        constexpr const char* REFRESHING_LABEL = "Refreshing the list of disks";
        constexpr const char* THE_ROOT = "the root";
        constexpr char EXTENSION_DOT = '.';

        /// A load and the folder it was made from, as listed after it.
        struct LoadDone
        {
            mcp::DiskContents where;
            mcp::LoadOutcome outcome;
        };

        bool sameName(const std::string& a, const std::string& b)
        {
            return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
                       return std::toupper(static_cast<unsigned char>(x)) == std::toupper(static_cast<unsigned char>(y));
                   });
        }

        std::string folderName(const std::vector<std::string>& folder)
        {
            return folder.empty() ? std::string(THE_ROOT) : "\"" + pathText(folder) + "\"";
        }

        /// Goes to `folder`, runs the load there, and answers the folder and the load.
        template <typename Load>
        mcp::Outcome<LoadDone> loadAt(mcp::SamplerGateway& gateway, const std::vector<std::string>& folder, Load load)
        {
            auto there = moveTo(gateway, folder);
            if (!there.ok())
                return mcp::Outcome<LoadDone>::failure(there.problem);
            auto loaded = load(gateway);
            if (!loaded.ok())
                return mcp::Outcome<LoadDone>::failure(loaded.problem);
            return mcp::Outcome<LoadDone>::success(LoadDone{std::move(*there.value), std::move(*loaded.value)});
        }

        /// Goes to `folder`, runs the change there, and answers the folder as listed afterwards. A change the gateway did not make (a
        /// confirmation that did not match) is a problem.
        template <typename Change>
        mcp::Outcome<mcp::DiskContents> changeAt(mcp::SamplerGateway& gateway, const std::vector<std::string>& folder, Change change)
        {
            const auto there = moveTo(gateway, folder);
            if (!there.ok())
                return there;
            const auto changed = change(gateway);
            if (!changed.ok())
                return mcp::Outcome<mcp::DiskContents>::failure(changed.problem);
            if (!changed.value->done)
                return mcp::Outcome<mcp::DiskContents>::failure("The sampler was sent nothing: \"" + changed.value->name + "\" was not changed.");
            return gateway.listDiskContents();
        }

        const std::vector<std::string>& namesOf(const mcp::MemoryNames& names, MemoryKind kind)
        {
            switch (kind)
            {
                case MemoryKind::Multi:
                    return names.multis;
                case MemoryKind::Program:
                    return names.programs;
                case MemoryKind::Sample:
                    return names.samples;
                case MemoryKind::SongFile:
                    return names.songFiles;
                case MemoryKind::SetList:
                    return names.setLists;
                case MemoryKind::SceneList:
                    break;
            }
            return names.sceneLists;
        }

        /// The item of `kind` the load added, or, when it replaced one of the same name, the first of that kind it found again.
        std::optional<MemoryItem> addedItem(const mcp::LoadOutcome& outcome, MemoryKind kind)
        {
            const auto& before = namesOf(outcome.before, kind);
            const auto& after = namesOf(outcome.after, kind);
            const auto added = std::find_if(after.begin(), after.end(), [&before](const std::string& name) {
                return std::find(before.begin(), before.end(), name) == before.end();
            });
            if (added != after.end())
                return MemoryItem{kind, *added};
            return std::nullopt;
        }

        /// A browser of the disks just read: the tree starts again from the current folder, the selection is empty.
        BrowserState stateOf(const DiskView& view)
        {
            BrowserState state;
            state.disks = view.disks;
            state.currentDisk = view.currentDisk;
            state.freeBytes = view.freeBytes;
            if (view.contents)
                record(state, *view.contents);
            return state;
        }
    }

    FileBrowser::FileBrowser(Model& model, Controller& controller) : _model(model), _controller(controller)
    {
        _model.addListener(*this);
    }

    FileBrowser::~FileBrowser()
    {
        _model.removeListener(*this);
    }

    void FileBrowser::modelChanged(ModelPart part)
    {
        if (part != ModelPart::Connection)
            return;
        const ConnectionState state = _model.connection().state;
        const ConnectionState last = std::exchange(_lastState, state);
        if (state == ConnectionState::Connected && last != ConnectionState::Connected)
            refresh();
        else if (state != ConnectionState::Connected && last == ConnectionState::Connected)
            _model.setBrowser(BrowserState{});
    }

    void FileBrowser::refresh()
    {
        _controller.runOnSampler<DiskView>(LISTING_LABEL, readDisks, [this, alive = std::weak_ptr<int>(_alive)](const DiskView& view) {
            if (!alive.expired())
                _model.setBrowser(stateOf(view));
        });
    }

    void FileBrowser::selectDisk(int handle)
    {
        _controller.runOnSampler<DiskView>(
            SELECTING_LABEL,
            [handle](mcp::SamplerGateway& gateway) {
                const auto selected = gateway.selectDiskByHandle(handle);
                if (!selected.ok())
                    return mcp::Outcome<DiskView>::failure(selected.problem);
                return readDisks(gateway);
            },
            [this, alive = std::weak_ptr<int>(_alive)](const DiskView& view) {
                if (alive.expired())
                    return;
                BrowserState state = stateOf(view);
                if (const mcp::DiskEntry* current = state.currentDiskEntry())
                    state.selection = BrowserItem{BrowserItemKind::Disk, {}, current->name};
                _model.setBrowser(std::move(state));
            });
    }

    void FileBrowser::openFolder(std::vector<std::string> folder)
    {
        const std::string label = OPENING_LABEL + (folder.empty() ? std::string(ROOT_NAME) : pathText(folder));
        _controller.runOnSampler<mcp::DiskContents>(
            label, [folder](mcp::SamplerGateway& gateway) { return moveTo(gateway, folder); },
            [this, alive = std::weak_ptr<int>(_alive)](const mcp::DiskContents& contents) {
                if (alive.expired())
                    return;
                BrowserState state = _model.browser();
                record(state, contents);
                _model.setBrowser(std::move(state));
            });
    }

    void FileBrowser::select(BrowserItem item)
    {
        if (_auditioning && !(_auditioning->folder == item.folder && _auditioning->name == item.name))
            stopAudition();
        BrowserState state = _model.browser();
        state.selection = std::move(item);
        _model.setBrowser(std::move(state));
    }

    void FileBrowser::fold(const std::vector<std::string>& folder)
    {
        // [RQ-APP-079, TASK-APP-021]
        BrowserState state = _model.browser();
        state.folded.insert(pathText(folder));
        _model.setBrowser(std::move(state));
    }

    // [RQ-APP-011, RQ-APP-013, RQ-APP-028 to RQ-APP-034, ADR-APP-001 (DEC-APP-005, DEC-APP-007), TASK-APP-014]
    void FileBrowser::showFolder(const mcp::DiskContents& contents)
    {
        BrowserState state = _model.browser();
        record(state, contents);
        _model.setBrowser(std::move(state));
    }

    bool FileBrowser::refuseTakenName(const std::vector<std::string>& folder, const std::string& name, const std::string& action)
    {
        const FolderListing* listing = _model.browser().listing(folder);
        if (listing == nullptr)
            return false;
        const auto same = [&name](const std::string& other) { return sameName(other, name); };
        const bool taken = std::any_of(listing->folders.begin(), listing->folders.end(), same)
                           || std::any_of(listing->files.begin(), listing->files.end(),
                                          [&same](const mcp::DiskFileEntry& file) { return same(file.name); });
        if (taken)
            _model.setProblem(Problem{action, "A folder or a file named \"" + name + "\" is already in " + folderName(folder) + "."});
        return taken;
    }

    void FileBrowser::load(const BrowserItem& file, bool withDependents, mcp::SampleLoadMode mode)
    {
        const std::optional<MemoryKind> kind = memoryKindOf(fileKindOf(file.name));
        _controller.runOnSampler<LoadDone>(
            LOADING_LABEL + file.name,
            [file, withDependents, mode](mcp::SamplerGateway& gateway) {
                return loadAt(gateway, file.folder,
                              [&file, withDependents, mode](mcp::SamplerGateway& at) { return at.loadFile(file.name, withDependents, mode); });
            },
            [this, alive = std::weak_ptr<int>(_alive), kind](const LoadDone& done) {
                if (alive.expired())
                    return;
                showFolder(done.where);
                _model.memoryChanged(kind ? addedItem(done.outcome, *kind) : std::nullopt);
            });
    }

    void FileBrowser::loadFolder(const BrowserItem& folder)
    {
        const Confirmation question{"Load the folder?", folder.name,
                                    "Every file of the folder \"" + folder.name +
                                        "\" and of its sub-folders is loaded into the sampler's memory, more than the tree shows."};
        _controller.runOnSamplerConfirmed<LoadDone>(
            question, LOADING_LABEL + folder.name,
            [folder](mcp::SamplerGateway& gateway) {
                return loadAt(gateway, folder.folder, [&folder](mcp::SamplerGateway& at) { return at.loadFolder(folder.name); });
            },
            [this, alive = std::weak_ptr<int>(_alive)](const LoadDone& done) {
                if (alive.expired())
                    return;
                showFolder(done.where);
                _model.memoryChanged(std::nullopt);
            });
    }

    void FileBrowser::createFolder(const std::vector<std::string>& parent, const std::string& name)
    {
        const std::string label = CREATING_LABEL + name;
        if (refuseTakenName(parent, name, label))
            return;
        _controller.runOnSampler<mcp::DiskContents>(
            label,
            [parent, name](mcp::SamplerGateway& gateway) {
                const auto there = moveTo(gateway, parent);
                if (!there.ok())
                    return there;
                return gateway.createFolder(name);
            },
            [this, alive = std::weak_ptr<int>(_alive)](const mcp::DiskContents& contents) {
                if (!alive.expired())
                    showFolder(contents);
            });
    }

    void FileBrowser::rename(const BrowserItem& item, const std::string& newName)
    {
        const std::string label = RENAMING_LABEL + item.name;
        const bool isFile = item.kind == BrowserItemKind::File;
        const auto dot = item.name.rfind(EXTENSION_DOT);
        const std::string finalName = isFile && dot != std::string::npos ? newName + item.name.substr(dot) : newName;
        if (refuseTakenName(item.folder, finalName, label))
            return;
        _controller.runOnSampler<mcp::DiskContents>(
            label,
            [item, newName, isFile](mcp::SamplerGateway& gateway) {
                return changeAt(gateway, item.folder, [&item, &newName, isFile](mcp::SamplerGateway& at) {
                    return isFile ? at.renameFile(item.name, newName) : at.renameFolder(item.name, newName);
                });
            },
            [this, alive = std::weak_ptr<int>(_alive)](const mcp::DiskContents& contents) {
                if (!alive.expired())
                    showFolder(contents);
            });
    }

    void FileBrowser::remove(const BrowserItem& item)
    {
        const bool isFile = item.kind == BrowserItemKind::File;
        const Confirmation question{isFile ? "Delete the file?" : "Delete the folder?", item.name,
                                    isFile ? "The file \"" + item.name + "\" is deleted from the disk; this cannot be undone."
                                           : "The folder \"" + item.name +
                                                 "\" and everything it holds are deleted from the disk; this cannot be undone."};
        _controller.runOnSamplerConfirmed<mcp::DiskContents>(
            question, DELETING_LABEL + item.name,
            [item, isFile](mcp::SamplerGateway& gateway) {
                return changeAt(gateway, item.folder, [&item, isFile](mcp::SamplerGateway& at) {
                    // The person confirmed this very name: it is the gateway's typed confirmation. [DEC-APP-007]
                    return isFile ? at.deleteFile(item.name, item.name) : at.deleteFolder(item.name, item.name, true);
                });
            },
            [this, alive = std::weak_ptr<int>(_alive)](const mcp::DiskContents& contents) {
                if (!alive.expired())
                    showFolder(contents);
            });
    }

    void FileBrowser::audition(const BrowserItem& file)
    {
        const bool queued = _controller.runOnSampler<std::string>(
            AUDITIONING_LABEL + file.name,
            [file](mcp::SamplerGateway& gateway) {
                const auto there = moveTo(gateway, file.folder);
                if (!there.ok())
                    return mcp::Outcome<std::string>::failure(there.problem);
                return gateway.startFileAudition(file.name);
            },
            [](const std::string&) {});
        if (queued)
            _auditioning = file;
    }

    void FileBrowser::stopAudition()
    {
        if (!_auditioning)
            return;
        _auditioning.reset();
        _controller.runOnSampler<bool>(STOPPING_LABEL, [](mcp::SamplerGateway& gateway) { return gateway.stopFileAudition(); },
                                       [](const bool&) {});
    }

    void FileBrowser::refreshDisks()
    {
        const Confirmation question{"Refresh the list of disks?", "the list of disks",
                                    "The refresh once left a real S5000 answering nothing: it may have to be switched off and on."};
        _controller.runOnSamplerConfirmed<DiskView>(
            question, REFRESHING_LABEL,
            [](mcp::SamplerGateway& gateway) {
                const auto refreshed = gateway.listDisks(true);
                if (!refreshed.ok())
                    return mcp::Outcome<DiskView>::failure(refreshed.problem);
                return readDisks(gateway);
            },
            [this, alive = std::weak_ptr<int>(_alive)](const DiskView& view) {
                if (!alive.expired())
                    _model.setBrowser(stateOf(view));
            });
    }
}
