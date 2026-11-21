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
#include <optional>
#include <string>
#include <vector>

#include "app/AppSettings.hpp"
#include "app/Controller.hpp"
#include "app/Model.hpp"
#include "app/Save.hpp"

namespace app
{
    /// The save's controller: it proposes the item, refuses a disk the sampler cannot write, asks "check names" questions through the prompts,
    /// saves into the browser's current folder in one job of the sampler worker (the gateway's slow-operation timeout), then shows the
    /// folder again and records the report in the model. Called on the UI thread only. [RQ-APP-043 to RQ-APP-047, RQ-APP-013,
    /// ADR-APP-005 (DEC-APP-039, DEC-APP-040), ADR-APP-001 (DEC-APP-001, DEC-APP-003, DEC-APP-007)]
    class SaveController
    {
    public:
        SaveController(Model& model, Controller& controller, AppSettings& settings);

        /// The memory view's selection, else the current program; nothing when there is neither. [RQ-APP-043]
        [[nodiscard]] std::optional<MemoryItem> proposedItem() const;
        /// Why nothing can be saved now (not connected, no disk, a disk that is not MSDOS or FAT32, a read-only disk), or nothing.
        /// [RQ-APP-047]
        [[nodiscard]] std::optional<std::string> refusal() const;
        /// "Check names": on unless the person turned it off; remembered between runs. [RQ-APP-045]
        [[nodiscard]] bool checkNames() const;
        void setCheckNames(bool on);

        /// Saves as `request` says; a refusal is shown as the problem with nothing sent, a cancel sends nothing. [RQ-APP-043 to RQ-APP-047]
        void save(const SaveRequest& request);

    private:
        /// The items `request` saves, in the order they are sent. [RQ-APP-044, DEC-APP-040]
        [[nodiscard]] std::vector<MemoryItem> itemsOf(const SaveRequest& request) const;
        /// The files of the current folder's listing that the save of `items` would write. [RQ-APP-045]
        [[nodiscard]] std::vector<std::string> filesThere(const std::vector<MemoryItem>& items) const;
        void run(const SaveRequest& request, std::vector<MemoryItem> items, mcp::ExistingFile existing);

        Model& _model;
        Controller& _controller;
        AppSettings& _settings;
        /// Expires with the controller: a result that comes back after it is ignored.
        std::shared_ptr<int> _alive = std::make_shared<int>(0);
    };
}
