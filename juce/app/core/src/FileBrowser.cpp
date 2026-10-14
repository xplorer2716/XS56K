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
        BrowserState state = _model.browser();
        state.selection = std::move(item);
        _model.setBrowser(std::move(state));
    }
}
