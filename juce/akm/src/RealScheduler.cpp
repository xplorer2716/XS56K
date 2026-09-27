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
#include "akm/RealScheduler.hpp"

#include <algorithm>
#include <cassert>
#include <optional>
#include <utility>

namespace akm
{
    RealScheduler::RealScheduler()
        : _thread([this] { run(); })
    {
    }

    RealScheduler::~RealScheduler()
    {
        assert(_thread.get_id() != std::this_thread::get_id() && "a RealScheduler cannot be destroyed from its own task");
        {
            const std::lock_guard lock(_mutex);
            _stopping = true;
        }
        _wake.notify_all();
        _thread.join();
    }

    RealScheduler::Clock::time_point RealScheduler::now() const
    {
        return Clock::now();
    }

    TimerHandle RealScheduler::scheduleAfter(Clock::duration delay, Task task)
    {
        TimerHandle handle;
        {
            const std::lock_guard lock(_mutex);
            handle = _queue.push(Clock::now() + std::max(delay, Clock::duration::zero()), std::move(task));
        }
        _wake.notify_one();
        return handle;
    }

    void RealScheduler::run()
    {
        std::unique_lock lock(_mutex);
        while (!_stopping)
        {
            detail::TimerQueue::Entries discarded;
            std::optional<detail::TimerQueue::Entry> due = _queue.popDue(Clock::now(), discarded);
            const std::optional<Clock::time_point> next = _queue.nextDeadline();

            if (!discarded.empty() || due)
            {
                lock.unlock();
                // Cancelled tasks, and the task that has run, are destroyed outside the lock.
                discarded.clear();
                if (due && !due->cancelled->load())
                    due->task();
                due.reset();
                lock.lock();
                continue;
            }

            if (next)
                _wake.wait_until(lock, *next);
            else
                _wake.wait(lock);
        }
    }
}
