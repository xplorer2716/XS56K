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

#include <atomic>
#include <cstddef>
#include <deque>
#include <mutex>
#include <thread>

#include "akm/Executor.hpp"

namespace akm
{
    /// An Executor that runs nothing until a test drains it, on the test's own thread: a deterministic
    /// single-thread run of the session code, with no wait and no race. Like MockMidiBackend, it is a test
    /// double that lives in the library. [RQ-AKM-016, RQ-AKM-020, ADR-AKM-001 (DEC-AKM-004)]
    ///
    /// `post()` may be called from any thread, so a simulated sampler can answer from its own; only one
    /// thread at a time may drain, and a task must not call `runUntilIdle()`.
    class ManualExecutor final : public Executor
    {
    public:
        void post(Task task) override;
        [[nodiscard]] bool isCurrentThread() const override;

        /// Runs the queued tasks, and those they post, in posting order until the queue is empty. Returns
        /// how many ran.
        std::size_t runUntilIdle();

    private:
        std::mutex _mutex;
        std::deque<Task> _queue;
        // The thread inside runUntilIdle(), or a default-constructed id when none is.
        std::atomic<std::thread::id> _runner{};
    };
}
