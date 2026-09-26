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
#include "akm/testing/ScenarioDriver.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

namespace akm::testing
{
    namespace
    {
        using Clock = Scheduler::Clock;

        // The manual driver moves time in steps of this size and drains the executor after each, so that
        // what a timer posts runs before the next timer falls due.
        constexpr auto MANUAL_STEP = std::chrono::milliseconds(1);

        // The real driver looks at its condition this often.
        constexpr auto REAL_POLL_INTERVAL = std::chrono::milliseconds(1);
    }

    void ManualScenarioDriver::elapse(Clock::duration duration)
    {
        const Clock::time_point end = _scheduler.now() + duration;
        for (;;)
        {
            _executor.runUntilIdle();
            const Clock::duration remaining = end - _scheduler.now();
            if (remaining <= Clock::duration::zero())
                return;
            _scheduler.advance(std::min<Clock::duration>(MANUAL_STEP, remaining));
        }
    }

    bool ManualScenarioDriver::waitUntil(const std::function<bool()>& condition, Clock::duration timeout)
    {
        const Clock::time_point deadline = _scheduler.now() + timeout;
        for (;;)
        {
            _executor.runUntilIdle();
            if (condition())
                return true;
            const Clock::duration remaining = deadline - _scheduler.now();
            if (remaining <= Clock::duration::zero())
                return false;
            _scheduler.advance(std::min<Clock::duration>(MANUAL_STEP, remaining));
        }
    }

    void RealScenarioDriver::elapse(Clock::duration duration)
    {
        std::this_thread::sleep_for(duration);
    }

    bool RealScenarioDriver::waitUntil(const std::function<bool()>& condition, Clock::duration timeout)
    {
        const Clock::time_point deadline = Clock::now() + timeout;
        while (!condition())
        {
            if (Clock::now() >= deadline)
                return false;
            std::this_thread::sleep_for(REAL_POLL_INTERVAL);
        }
        return true;
    }
}
