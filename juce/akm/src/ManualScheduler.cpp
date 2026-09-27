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
#include "akm/ManualScheduler.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace akm
{
    ManualScheduler::Clock::time_point ManualScheduler::now() const
    {
        const std::lock_guard lock(_mutex);
        return _now;
    }

    TimerHandle ManualScheduler::scheduleAfter(Clock::duration delay, Task task)
    {
        const std::lock_guard lock(_mutex);
        return _queue.push(_now + std::max(delay, Clock::duration::zero()), std::move(task));
    }

    void ManualScheduler::advance(Clock::duration amount)
    {
        std::unique_lock lock(_mutex);
        const Clock::time_point target = _now + std::max(amount, Clock::duration::zero());
        for (;;)
        {
            detail::TimerQueue::Entries discarded;
            std::optional<detail::TimerQueue::Entry> due = _queue.popDue(target, discarded);
            // The clock reads the deadline of the task about to run, and the target once none is left.
            _now = due ? std::max(_now, due->deadline) : target;
            lock.unlock();

            // Cancelled tasks, and the task that has run, are destroyed outside the lock.
            discarded.clear();
            if (!due)
                return;
            if (!due->cancelled->load())
                due->task();
            due.reset();
            lock.lock();
        }
    }

    std::size_t ManualScheduler::pendingCount() const
    {
        const std::lock_guard lock(_mutex);
        return _queue.liveCount();
    }
}
