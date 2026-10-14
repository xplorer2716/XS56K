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
#include "app/Controller.hpp"

#include <optional>

namespace app
{
    // [RQ-APP-010, RQ-APP-011, RQ-APP-012, ADR-APP-001 (DEC-APP-003, DEC-APP-004, DEC-APP-007), TASK-APP-011]
    Controller::Controller(Model& model, UiDispatcher& dispatcher, UserPrompts& prompts)
        : _model(model), _prompts(prompts), _worker(dispatcher)
    {
    }

    Controller::~Controller() = default;

    bool Controller::idle() const
    {
        return _worker.idle();
    }

    void Controller::queue(std::string label, std::function<JobResult()> work)
    {
        _labels.push_back(label);
        _model.setBusy(_labels.front());
        _worker.submit([this, alive = std::weak_ptr<int>(_alive), work = std::move(work)]() -> SamplerWorker::Continuation {
            JobResult result = work();
            return [this, alive, result = std::move(result)]() mutable {
                if (!alive.expired())
                    finished(std::move(result));
            };
        });
    }

    void Controller::finished(JobResult result)
    {
        const std::string label = _labels.front();
        _labels.pop_front();
        _model.setBusy(_labels.empty() ? std::nullopt : std::optional<std::string>(_labels.front()));
        if (!result.problem.empty())
        {
            _model.setProblem(Problem{label, std::move(result.problem)});
            return;
        }
        result.apply();
    }
}
