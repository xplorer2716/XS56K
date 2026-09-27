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
#include <chrono>
#include <memory>

#include "akm/Task.hpp"

namespace akm
{
    /// Cancels a task scheduled on a Scheduler. Copies share the task; a default-constructed handle
    /// refers to none. [RQ-AKM-010, ADR-AKM-001 (DEC-AKM-004, DEC-AKM-006)]
    class TimerHandle
    {
    public:
        /// Set by cancel(), read by the scheduler that owns the task.
        using CancelFlag = std::shared_ptr<std::atomic<bool>>;

        TimerHandle() = default;
        explicit TimerHandle(CancelFlag flag) : _flag(std::move(flag)) {}

        /// Prevents the task from starting. Never blocks; may be called from any thread, including from
        /// inside a scheduled task, and after the task has run (then it does nothing). A task that has
        /// already started keeps running: whoever needs to ignore a late task carries a generation
        /// token (DEC-AKM-004).
        void cancel() const
        {
            if (_flag)
                _flag->store(true);
        }

    private:
        CancelFlag _flag;
    };

    /// The session's only source of time: it never reads a clock or starts a timer itself, so the
    /// timeouts, the discovery window and the Still Alive restart run in no time under a manual
    /// scheduler. [RQ-AKM-010, RQ-AKM-011, RQ-AKM-012, RQ-AKM-016, ADR-AKM-001 (DEC-AKM-006)]
    class Scheduler
    {
    public:
        using Clock = std::chrono::steady_clock;

        virtual ~Scheduler() = default;

        [[nodiscard]] virtual Clock::time_point now() const = 0;

        /// Runs `task` once, `delay` from now (a delay of zero or less means as soon as possible), on a
        /// thread the scheduler owns or is driven by. Callable from any thread, and from a task.
        virtual TimerHandle scheduleAfter(Clock::duration delay, Task task) = 0;
    };
}
