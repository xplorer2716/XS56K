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

#include <functional>

#include "akm/Executor.hpp"
#include "akm/ManualExecutor.hpp"
#include "akm/ManualScheduler.hpp"
#include "akm/RealScheduler.hpp"
#include "akm/Scheduler.hpp"
#include "akm/ThreadExecutor.hpp"

namespace akm::harness
{
    /// What a scenario needs to wait, whichever backend it runs on: the scheduler and the executor a
    /// session is given, and a way to let time pass and to wait for something. A scenario is written
    /// against `MidiBackend&` and a ScenarioDriver, so the same source runs in CI on the simulated
    /// sampler with manual time and against the real S5000 with real time. [RQ-AKM-016, RQ-AKM-019,
    /// ADR-AKM-001 (DEC-AKM-008)]
    class ScenarioDriver
    {
    public:
        virtual ~ScenarioDriver() = default;

        [[nodiscard]] virtual Scheduler& scheduler() = 0;
        [[nodiscard]] virtual Executor& executor() = 0;

        /// Lets `duration` of scenario time go by, running what falls due.
        virtual void elapse(Scheduler::Clock::duration duration) = 0;

        /// Waits until `condition` holds, for at most `timeout` of scenario time; returns whether it held.
        /// The condition is evaluated on the calling thread, between steps.
        [[nodiscard]] virtual bool waitUntil(const std::function<bool()>& condition,
                                             Scheduler::Clock::duration timeout) = 0;
    };

    /// Manual time and a manual executor, on one thread: nothing runs unless the scenario waits, and
    /// thirty seconds of timeout cost no real time. Time moves in steps of one millisecond, and the
    /// executor is drained after each. It suits a simulation that delivers on the sending thread or from the
    /// scheduler; a simulation that delivers from its own thread needs RealScenarioDriver, since manual
    /// time does not wait for another thread.
    class ManualScenarioDriver final : public ScenarioDriver
    {
    public:
        [[nodiscard]] Scheduler& scheduler() override { return _scheduler; }
        [[nodiscard]] Executor& executor() override { return _executor; }

        void elapse(Scheduler::Clock::duration duration) override;
        [[nodiscard]] bool waitUntil(const std::function<bool()>& condition, Scheduler::Clock::duration timeout) override;

    private:
        ManualScheduler _scheduler;
        ManualExecutor _executor;
    };

    /// The real scheduler and executor, and wall-clock waiting: what a scenario against the real S5000
    /// runs on.
    class RealScenarioDriver final : public ScenarioDriver
    {
    public:
        [[nodiscard]] Scheduler& scheduler() override { return _scheduler; }
        [[nodiscard]] Executor& executor() override { return _executor; }

        void elapse(Scheduler::Clock::duration duration) override;
        [[nodiscard]] bool waitUntil(const std::function<bool()>& condition, Scheduler::Clock::duration timeout) override;

    private:
        // The executor first, so that it is destroyed last: the scheduler's thread posts to it.
        ThreadExecutor _executor;
        RealScheduler _scheduler;
    };
}
