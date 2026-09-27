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

#include <cstddef>
#include <mutex>

#include "akm/Scheduler.hpp"
#include "akm/detail/TimerQueue.hpp"

namespace akm
{
    /// A Scheduler whose time only moves when a test advances it, so that a timeout of seconds runs in no
    /// time and the order of events is exact. Like MockMidiBackend, it is a test double that lives in the
    /// library. [RQ-AKM-010, RQ-AKM-011, RQ-AKM-012, RQ-AKM-016, ADR-AKM-001 (DEC-AKM-006)]
    ///
    /// Tasks run inside `advance()`, on the thread that calls it. `advance()` is not reentrant: a task
    /// must not call it.
    class ManualScheduler final : public Scheduler
    {
    public:
        [[nodiscard]] Clock::time_point now() const override;
        TimerHandle scheduleAfter(Clock::duration delay, Task task) override;

        /// Moves time forward by `amount` (zero or more), running every task that falls due, in deadline
        /// order, each with `now()` reading its own deadline. A task that schedules another one that also
        /// falls within `amount` sees it run in the same call. Afterwards `now()` is exactly `amount` later.
        void advance(Clock::duration amount);

        /// Tasks scheduled and neither run nor cancelled.
        [[nodiscard]] std::size_t pendingCount() const;

    private:
        mutable std::mutex _mutex;
        Clock::time_point _now{};
        detail::TimerQueue _queue;
    };
}
