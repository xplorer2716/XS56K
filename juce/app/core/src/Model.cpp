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
#include "app/Model.hpp"

#include <algorithm>
#include <utility>

#include "midiapp/service/Logger.hpp"

namespace app
{
    namespace
    {
        using midiapp::service::LogDomain;
        using midiapp::service::TraceLevel;
        constexpr const char* REASON_SEPARATOR = ": ";

        /// The connection's state in the log's words. [RQ-APP-085]
        const char* stateText(ConnectionState state)
        {
            switch (state)
            {
                case ConnectionState::Disconnected:
                    return "Disconnected";
                case ConnectionState::Connecting:
                    return "Connecting";
                case ConnectionState::Connected:
                    return "Connected";
                case ConnectionState::Failed:
                    return "Connection failed";
            }
            return "";
        }
    }

    // [RQ-APP-010, RQ-APP-012, ADR-APP-001 (DEC-APP-004), TASK-APP-011]
    void Model::setBusy(std::optional<std::string> label)
    {
        if (_busy == label)
            return;
        _busy = std::move(label);
        notify(ModelPart::Busy);
    }

    void Model::setProblem(Problem problem)
    {
        // Every problem shown is also written to the log. [RQ-APP-085, ADR-APP-001 (DEC-APP-023), TASK-APP-033]
        XS56K_LOG(LogDomain::ControllerCalls, TraceLevel::Error, problem.action + REASON_SEPARATOR + problem.reason);
        _problem = std::move(problem);
        notify(ModelPart::Problem);
    }

    void Model::clearProblem()
    {
        if (!_problem)
            return;
        _problem.reset();
        notify(ModelPart::Problem);
    }

    // [RQ-APP-019, TASK-APP-012]
    void Model::setConnection(ConnectionStatus status)
    {
        // [RQ-APP-085, TASK-APP-033]
        XS56K_LOG(LogDomain::ControllerCalls, TraceLevel::Info,
                  std::string(stateText(status.state)) + (status.reason.empty() ? std::string() : REASON_SEPARATOR + status.reason));
        _connection = std::move(status);
        notify(ModelPart::Connection);
    }

    // [RQ-APP-024 to RQ-APP-026, TASK-APP-013]
    void Model::setBrowser(BrowserState browser)
    {
        _browser = std::move(browser);
        notify(ModelPart::Browser);
    }

    // [RQ-APP-028, RQ-APP-035, RQ-APP-036, TASK-APP-014, TASK-APP-015]
    void Model::memoryChanged(std::optional<MemoryItem> select)
    {
        _memoryChange = MemoryChange{_memoryChange.number + 1, std::move(select)};
        notify(ModelPart::MemoryChanged);
    }

    void Model::setMemory(MemoryState memory)
    {
        _memory = std::move(memory);
        notify(ModelPart::Memory);
    }

    void Model::addListener(ModelListener& listener)
    {
        _listeners.push_back(&listener);
    }

    void Model::removeListener(ModelListener& listener)
    {
        std::erase(_listeners, &listener);
    }

    void Model::notify(ModelPart part)
    {
        // A listener may remove itself while it is told: tell a copy.
        const std::vector<ModelListener*> listeners = _listeners;
        for (ModelListener* listener : listeners)
            listener->modelChanged(part);
    }
}
