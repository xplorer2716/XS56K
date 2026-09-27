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
#include <map>
#include <optional>
#include <vector>

#include "akm/Scheduler.hpp"

namespace akm::detail
{
    /// The deadline-ordered list of tasks that RealScheduler and ManualScheduler share. Not thread-safe:
    /// each scheduler guards it with its own lock. Not part of the AKM API. [ADR-AKM-001 (DEC-AKM-006)]
    class TimerQueue
    {
    public:
        using Clock = Scheduler::Clock;

        struct Entry
        {
            Clock::time_point deadline;
            TimerHandle::CancelFlag cancelled;
            Task task;
        };
        using Entries = std::vector<Entry>;

        /// Adds a task and returns the handle that cancels it. Tasks with the same deadline keep the order
        /// they were pushed in.
        TimerHandle push(Clock::time_point deadline, Task task);

        /// Removes and returns the earliest task whose deadline is not after `limit`, or nothing. Cancelled
        /// tasks met on the way are moved to `discarded`, so that the caller destroys them (and whatever
        /// they captured) outside its lock.
        [[nodiscard]] std::optional<Entry> popDue(Clock::time_point limit, Entries& discarded);

        /// Deadline of the earliest task, cancelled or not.
        [[nodiscard]] std::optional<Clock::time_point> nextDeadline() const;

        /// Number of tasks that have not been cancelled.
        [[nodiscard]] std::size_t liveCount() const;

    private:
        std::multimap<Clock::time_point, Entry> _entries;
    };
}
