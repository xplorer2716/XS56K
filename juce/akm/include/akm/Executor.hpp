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

#include "akm/Task.hpp"

namespace akm
{
    /// The serial executor of a session: tasks run one at a time, in the order they were posted, so
    /// everything that reads or changes the session's state, sends a frame or invokes a completion is a
    /// task on it and needs no lock. The backend's input callback, the scheduler's timer thread and the
    /// callers of `submit()` only post tasks. [RQ-AKM-008, RQ-AKM-020, ADR-AKM-001 (DEC-AKM-004,
    /// DEC-AKM-005)]
    class Executor
    {
    public:
        virtual ~Executor() = default;

        /// Queues `task` and returns at once. Callable from any thread, and from a task, in which case
        /// the new task runs behind the ones already queued.
        virtual void post(Task task) = 0;

        /// Whether the calling thread is inside a task of this executor. A session uses it to refuse
        /// (and assert against) a close() from one of its own completions.
        [[nodiscard]] virtual bool isCurrentThread() const = 0;
    };
}
