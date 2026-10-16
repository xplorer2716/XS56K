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

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

#include "app/UiDispatcher.hpp"

namespace app
{
    /// The one thread that calls the sampler gateway: it runs the jobs one at a time, in the order they were submitted, which is the gateway's
    /// rule and the sampler's (RQ-AKM-008). A job's work runs on this thread and gives back a continuation, which the worker posts to the UI
    /// dispatcher. Destroying the worker lets the job in progress end, drops the jobs not started and joins the thread.
    /// [RQ-APP-012, ADR-APP-001 (DEC-APP-003)]
    class SamplerWorker
    {
    public:
        using Continuation = std::function<void()>;
        using Job = std::function<Continuation()>;

        explicit SamplerWorker(UiDispatcher& dispatcher);
        ~SamplerWorker();

        SamplerWorker(const SamplerWorker&) = delete;
        SamplerWorker& operator=(const SamplerWorker&) = delete;

        /// Queues a job and returns at once.
        void submit(Job job);

        /// Takes back the jobs not started yet, in order; the job in progress, if any, runs on. The caller destroys what it gets back.
        /// [RQ-APP-020, RQ-APP-021, RQ-APP-022, ADR-APP-001 (DEC-APP-008)]
        [[nodiscard]] std::deque<Job> dropQueued();

        /// True when no job waits, none runs, and the continuation of the last one has been posted.
        [[nodiscard]] bool idle() const;

    private:
        void loop();

        UiDispatcher& _dispatcher;
        mutable std::mutex _mutex;
        std::condition_variable _wake;
        std::deque<Job> _jobs;
        bool _running = false;
        bool _stopping = false;
        std::jthread _thread;  // last: started once the members above exist; joined by the destructor before the members go
    };
}
