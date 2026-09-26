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
#include <mutex>
#include <thread>

#include "akm/Executor.hpp"

namespace akm
{
    /// The production Executor: one worker thread that runs the posted tasks one at a time, in posting
    /// order. [RQ-AKM-008, RQ-AKM-020, ADR-AKM-001 (DEC-AKM-004)]
    ///
    /// Destruction lets the task in progress finish, discards the tasks not yet started (a session's
    /// close() posts its final task and waits for it before the executor goes) and joins the thread; it
    /// must not be done from a task of this executor.
    class ThreadExecutor final : public Executor
    {
    public:
        ThreadExecutor();
        ~ThreadExecutor() override;

        ThreadExecutor(const ThreadExecutor&) = delete;
        ThreadExecutor& operator=(const ThreadExecutor&) = delete;

        void post(Task task) override;
        [[nodiscard]] bool isCurrentThread() const override;

    private:
        void run();

        std::mutex _mutex;
        std::condition_variable _wake;
        std::deque<Task> _queue;
        bool _stopping = false;
        std::thread::id _workerId;  // written once, by the constructor, before any task can be posted
        std::thread _thread;        // last: it starts using the members above as soon as it exists
    };
}
