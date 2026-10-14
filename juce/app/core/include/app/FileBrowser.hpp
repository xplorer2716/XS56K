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

#include <memory>
#include <string>
#include <vector>

#include "app/Browser.hpp"
#include "app/Controller.hpp"
#include "app/Model.hpp"

namespace app
{
    /// The file browser's controller: it reads the sampler's disks and folders through the controller's jobs and writes what it read into
    /// the model's browser part. It fills the browser when the connection opens and empties it when the connection ends. Called on the UI
    /// thread only. [RQ-APP-017, RQ-APP-024 to RQ-APP-027, ADR-APP-001 (DEC-APP-001, DEC-APP-004)]
    class FileBrowser final : public ModelListener
    {
    public:
        FileBrowser(Model& model, Controller& controller);
        ~FileBrowser() override;

        FileBrowser(const FileBrowser&) = delete;
        FileBrowser& operator=(const FileBrowser&) = delete;

        /// Lists the disks again (the sampler's own list, no refresh of it) and, when one is current, its current folder. [RQ-APP-024]
        void refresh();
        /// Makes the disk the sampler's current disk and reads its format, its free space and its current folder. [RQ-APP-024]
        void selectDisk(int handle);
        /// Makes `folder` (from the root of the current disk) the sampler's current folder and lists it: the sampler goes up to the folder
        /// both paths share, then down. [RQ-APP-025]
        void openFolder(std::vector<std::string> folder);
        /// Selects a disk, a folder or a file in the browser; nothing is sent to the sampler. [RQ-APP-026, RQ-APP-027]
        void select(BrowserItem item);

        void modelChanged(ModelPart part) override;

    private:
        Model& _model;
        Controller& _controller;
        ConnectionState _lastState = ConnectionState::Disconnected;
        /// Expires with the browser: a result that comes back after it is ignored.
        std::shared_ptr<int> _alive = std::make_shared<int>(0);
    };
}
