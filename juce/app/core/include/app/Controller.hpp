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

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "app/Model.hpp"
#include "app/SamplerWorker.hpp"
#include "app/UiDispatcher.hpp"
#include "app/UserPrompts.hpp"
#include "mcp/SamplerGateway.hpp"

namespace app
{
    /// Turns the person's actions into jobs for the sampler worker and their results into changes of the model. Called on the UI thread only:
    /// an action returns at once, the model names the job in progress, and the result comes back through the UI dispatcher. A failed call
    /// leaves the model as it was and records what failed and why. [RQ-APP-010, RQ-APP-011, RQ-APP-012, RQ-APP-013, ADR-APP-001 (DEC-APP-001,
    /// DEC-APP-003, DEC-APP-007)]
    class Controller
    {
    public:
        Controller(Model& model, UiDispatcher& dispatcher, UserPrompts& prompts);
        ~Controller();

        Controller(const Controller&) = delete;
        Controller& operator=(const Controller&) = delete;

        /// Queues `call` (it runs on the worker, so it may block on the sampler) under `label`; on success `onSuccess` gets the value on the
        /// UI thread, on failure the model gets the problem and `onSuccess` is not called. [RQ-APP-010, RQ-APP-012]
        template <typename T>
        void run(std::string label, std::function<mcp::Outcome<T>()> call, std::function<void(const T&)> onSuccess)
        {
            queue(std::move(label), [call = std::move(call), onSuccess = std::move(onSuccess)]() -> JobResult {
                mcp::Outcome<T> outcome = call();
                if (!outcome.ok())
                    return JobResult{std::move(outcome.problem), {}};
                return JobResult{{}, [onSuccess, value = std::move(*outcome.value)] { onSuccess(value); }};
            });
        }

        /// Asks `question` first, and queues the job only when the person confirms: a cancel sends nothing. [RQ-APP-011, DEC-APP-007]
        template <typename T>
        void runConfirmed(Confirmation question, std::string label, std::function<mcp::Outcome<T>()> call,
                          std::function<void(const T&)> onSuccess)
        {
            _prompts.confirm(question, [this, alive = std::weak_ptr<int>(_alive), label = std::move(label), call = std::move(call),
                                        onSuccess = std::move(onSuccess)](bool confirmed) mutable {
                if (confirmed && !alive.expired())
                    run<T>(std::move(label), std::move(call), std::move(onSuccess));
            });
        }

        /// True when the worker has nothing left to run and every result has been handed to the UI dispatcher.
        [[nodiscard]] bool idle() const;

    private:
        /// What a job's work gives back to the UI thread: a problem, or what to apply to the model.
        struct JobResult
        {
            std::string problem;
            std::function<void()> apply;
        };

        void queue(std::string label, std::function<JobResult()> work);
        void finished(JobResult result);

        Model& _model;
        UserPrompts& _prompts;
        /// The labels of the jobs queued and not finished, in order: the first is the job in progress.
        std::deque<std::string> _labels;
        /// Expires with the controller: a continuation posted before its destruction then does nothing.
        std::shared_ptr<int> _alive = std::make_shared<int>(0);
        SamplerWorker _worker;  // last: destroyed first, so no job outlives what it uses
    };
}
