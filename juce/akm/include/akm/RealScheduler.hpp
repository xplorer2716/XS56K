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
#include <mutex>
#include <thread>

#include "akm/Scheduler.hpp"
#include "akm/detail/TimerQueue.hpp"

namespace akm
{
    /// The production Scheduler: one timer thread built on the standard library (not `juce::Timer`, which
    /// needs the JUCE message thread and would put a JUCE type in the interface). Tasks run one at a
    /// time on that thread, so they must be short: they post to an Executor and return.
    /// [RQ-AKM-010, RQ-AKM-020, ADR-AKM-001 (DEC-AKM-004, DEC-AKM-006)]
    ///
    /// Destruction discards the tasks still pending, waits for the one that is running and joins the
    /// thread; it must not be done from a task of this scheduler.
    class RealScheduler final : public Scheduler
    {
    public:
        RealScheduler();
        ~RealScheduler() override;

        RealScheduler(const RealScheduler&) = delete;
        RealScheduler& operator=(const RealScheduler&) = delete;

        [[nodiscard]] Clock::time_point now() const override;
        TimerHandle scheduleAfter(Clock::duration delay, Task task) override;

    private:
        void run();

        std::mutex _mutex;
        std::condition_variable _wake;
        detail::TimerQueue _queue;
        bool _stopping = false;
        std::thread _thread;  // last: it starts using the members above as soon as it exists
    };
}
