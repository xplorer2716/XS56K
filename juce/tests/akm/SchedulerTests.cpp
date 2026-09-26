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

// The Scheduler: the session's only source of time. The manual one is advanced by the tests, so the
// timeout scenarios run in no time; the real one runs on its own timer thread.
// [TASK-AKM-005, RQ-AKM-010, RQ-AKM-011, RQ-AKM-012, RQ-AKM-016, RQ-AKM-020, ADR-AKM-001 (DEC-AKM-006)]
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "akm/ManualScheduler.hpp"
#include "akm/RealScheduler.hpp"

using namespace std::chrono_literals;
using akm::ManualScheduler;
using akm::RealScheduler;
using akm::TimerHandle;

namespace
{
    // How long a test waits for something that must happen. It bounds a failure, never a success: the
    // wait ends as soon as the event has happened.
    constexpr auto eventTimeout = 10s;

    // A flag a test thread can wait on, with a bounded wait.
    class Signal
    {
    public:
        void set()
        {
            {
                const std::lock_guard lock(_mutex);
                _set = true;
            }
            _changed.notify_all();
        }

        [[nodiscard]] bool waitFor(std::chrono::milliseconds timeout)
        {
            std::unique_lock lock(_mutex);
            return _changed.wait_for(lock, timeout, [this] { return _set; });
        }

    private:
        std::mutex _mutex;
        std::condition_variable _changed;
        bool _set = false;
    };
}

TEST_CASE("Given the manual scheduler and a task scheduled 100 ms ahead, When time is advanced by 99 ms then 1 ms, Then the task runs only after the second advance [RQ-AKM-010]",
          "[akm][scheduler][manual]")
{
    ManualScheduler scheduler;
    int runs = 0;
    scheduler.scheduleAfter(100ms, [&runs] { ++runs; });

    scheduler.advance(99ms);
    CHECK(runs == 0);

    scheduler.advance(1ms);
    CHECK(runs == 1);
}

TEST_CASE("Given a task scheduled for 200 ms and then one for 100 ms, When time is advanced by 100 ms, Then the second runs and the first does not [RQ-AKM-010]",
          "[akm][scheduler][manual]")
{
    // A later deadline never hides an earlier one, whatever the order of scheduling.
    ManualScheduler scheduler;
    std::vector<int> order;
    scheduler.scheduleAfter(200ms, [&order] { order.push_back(200); });
    scheduler.scheduleAfter(100ms, [&order] { order.push_back(100); });

    scheduler.advance(100ms);
    CHECK(order == std::vector<int>{100});

    scheduler.advance(100ms);
    CHECK(order == std::vector<int>{100, 200});
}

TEST_CASE("Given tasks due within one advance, When time is advanced, Then they run in deadline order [RQ-AKM-010]",
          "[akm][scheduler][manual]")
{
    ManualScheduler scheduler;
    std::vector<int> order;
    scheduler.scheduleAfter(300ms, [&order] { order.push_back(300); });
    scheduler.scheduleAfter(100ms, [&order] { order.push_back(100); });
    scheduler.scheduleAfter(200ms, [&order] { order.push_back(200); });

    scheduler.advance(1s);

    CHECK(order == std::vector<int>{100, 200, 300});
}

TEST_CASE("Given tasks with the same deadline, When time is advanced, Then they run in the order they were scheduled [RQ-AKM-010]",
          "[akm][scheduler][manual]")
{
    ManualScheduler scheduler;
    std::vector<int> order;
    scheduler.scheduleAfter(100ms, [&order] { order.push_back(1); });
    scheduler.scheduleAfter(100ms, [&order] { order.push_back(2); });
    scheduler.scheduleAfter(100ms, [&order] { order.push_back(3); });

    scheduler.advance(100ms);

    CHECK(order == std::vector<int>{1, 2, 3});
}

TEST_CASE("Given a scheduled task, When it is cancelled before its time, Then it never runs [RQ-AKM-010]",
          "[akm][scheduler][manual]")
{
    ManualScheduler scheduler;
    int runs = 0;
    const TimerHandle handle = scheduler.scheduleAfter(100ms, [&runs] { ++runs; });

    handle.cancel();
    scheduler.advance(1s);

    CHECK(runs == 0);
}

TEST_CASE("Given a scheduled task, When it is cancelled from inside another task, Then it never runs [RQ-AKM-010]",
          "[akm][scheduler][manual]")
{
    ManualScheduler scheduler;
    int runs = 0;
    const TimerHandle victim = scheduler.scheduleAfter(100ms, [&runs] { ++runs; });
    scheduler.scheduleAfter(50ms, [victim] { victim.cancel(); });

    scheduler.advance(1s);

    CHECK(runs == 0);
}

TEST_CASE("Given a task that has already run, When its handle is cancelled, Then nothing happens [RQ-AKM-010]",
          "[akm][scheduler][manual]")
{
    ManualScheduler scheduler;
    int runs = 0;
    const TimerHandle handle = scheduler.scheduleAfter(100ms, [&runs] { ++runs; });
    scheduler.advance(100ms);

    handle.cancel();
    scheduler.advance(1s);

    CHECK(runs == 1);
}

TEST_CASE("Given a default handle, When it is cancelled, Then nothing happens [RQ-AKM-010]", "[akm][scheduler][manual]")
{
    const TimerHandle empty;

    empty.cancel();

    SUCCEED();
}

TEST_CASE("Given a task that schedules another, When time is advanced, Then the new task runs at its own time [RQ-AKM-010, RQ-AKM-011]",
          "[akm][scheduler][manual]")
{
    ManualScheduler scheduler;
    std::vector<int> order;
    scheduler.scheduleAfter(100ms, [&] {
        order.push_back(100);
        scheduler.scheduleAfter(50ms, [&order] { order.push_back(150); });
    });

    scheduler.advance(100ms);
    CHECK(order == std::vector<int>{100});

    scheduler.advance(49ms);
    CHECK(order == std::vector<int>{100});

    scheduler.advance(1ms);
    CHECK(order == std::vector<int>{100, 150});
}

TEST_CASE("Given a task that schedules another inside one advance, When it is run, Then the clock reads the task's own deadline [RQ-AKM-010]",
          "[akm][scheduler][manual]")
{
    ManualScheduler scheduler;
    const auto start = scheduler.now();
    std::vector<ManualScheduler::Clock::time_point> seen;
    scheduler.scheduleAfter(100ms, [&] {
        seen.push_back(scheduler.now());
        scheduler.scheduleAfter(50ms, [&] { seen.push_back(scheduler.now()); });
    });

    scheduler.advance(500ms);

    REQUIRE(seen.size() == 2);
    CHECK(seen[0] == start + 100ms);
    CHECK(seen[1] == start + 150ms);
    CHECK(scheduler.now() == start + 500ms);
}

TEST_CASE("Given a task scheduled with no delay, When time is advanced by zero, Then it runs [RQ-AKM-010]",
          "[akm][scheduler][manual]")
{
    ManualScheduler scheduler;
    int runs = 0;
    scheduler.scheduleAfter(0ms, [&runs] { ++runs; });
    scheduler.scheduleAfter(-5ms, [&runs] { ++runs; });

    scheduler.advance(0ms);

    CHECK(runs == 2);
}

TEST_CASE("Given scheduled tasks, When some run or are cancelled, Then the count of pending tasks follows [RQ-AKM-010]",
          "[akm][scheduler][manual]")
{
    ManualScheduler scheduler;
    scheduler.scheduleAfter(100ms, [] {});
    const TimerHandle cancelled = scheduler.scheduleAfter(200ms, [] {});
    scheduler.scheduleAfter(300ms, [] {});
    CHECK(scheduler.pendingCount() == 3);

    cancelled.cancel();
    CHECK(scheduler.pendingCount() == 2);

    scheduler.advance(100ms);
    CHECK(scheduler.pendingCount() == 1);
}

TEST_CASE("Given the manual scheduler, When time is advanced, Then now() moves by exactly that amount [RQ-AKM-010]",
          "[akm][scheduler][manual]")
{
    ManualScheduler scheduler;
    const auto start = scheduler.now();

    scheduler.advance(30ms);
    scheduler.advance(12ms);

    CHECK(scheduler.now() == start + 42ms);
}

TEST_CASE("Given the real scheduler and a task scheduled 50 ms ahead, When 200 ms of wall-clock time pass, Then it ran exactly once, on a thread other than the caller's [RQ-AKM-010, RQ-AKM-020]",
          "[akm][scheduler][real]")
{
    RealScheduler scheduler;
    std::atomic<int> runs{0};
    std::thread::id runner;
    Signal ran;
    scheduler.scheduleAfter(50ms, [&] {
        runner = std::this_thread::get_id();
        ++runs;
        ran.set();
    });

    REQUIRE(ran.waitFor(eventTimeout));
    // The rest of the 200 ms: a second run would show here.
    std::this_thread::sleep_for(150ms);

    CHECK(runs == 1);
    CHECK(runner != std::this_thread::get_id());
}

TEST_CASE("Given the real scheduler and a task scheduled for 200 ms and one for 50 ms, When they are due, Then they run in deadline order [RQ-AKM-010]",
          "[akm][scheduler][real]")
{
    RealScheduler scheduler;
    std::mutex mutex;
    std::vector<int> order;
    Signal both;
    const auto record = [&](int value) {
        const std::lock_guard lock(mutex);
        order.push_back(value);
        if (order.size() == 2)
            both.set();
    };
    scheduler.scheduleAfter(200ms, [&] { record(200); });
    scheduler.scheduleAfter(50ms, [&] { record(50); });

    REQUIRE(both.waitFor(eventTimeout));

    const std::lock_guard lock(mutex);
    CHECK(order == std::vector<int>{50, 200});
}

TEST_CASE("Given the real scheduler and a task cancelled before its time, When its time has passed, Then it never ran [RQ-AKM-010]",
          "[akm][scheduler][real]")
{
    RealScheduler scheduler;
    std::atomic<int> runs{0};
    Signal marker;
    const TimerHandle handle = scheduler.scheduleAfter(50ms, [&runs] { ++runs; });
    handle.cancel();
    // A later task marks the point in time by which the cancelled one would have run.
    scheduler.scheduleAfter(150ms, [&marker] { marker.set(); });

    REQUIRE(marker.waitFor(eventTimeout));

    CHECK(runs == 0);
}

TEST_CASE("Given the real scheduler and a task that schedules another, When they are due, Then both run [RQ-AKM-010]",
          "[akm][scheduler][real]")
{
    RealScheduler scheduler;
    Signal second;
    scheduler.scheduleAfter(20ms, [&] { scheduler.scheduleAfter(20ms, [&second] { second.set(); }); });

    CHECK(second.waitFor(eventTimeout));
}

TEST_CASE("Given the real scheduler, When now() is read twice, Then the second reading is not earlier [RQ-AKM-010]",
          "[akm][scheduler][real]")
{
    const RealScheduler scheduler;

    const auto first = scheduler.now();
    const auto second = scheduler.now();

    CHECK(second >= first);
}

TEST_CASE("Given the real scheduler destroyed with a task pending, When it is destroyed, Then it returns without running the task [RQ-AKM-020]",
          "[akm][scheduler][real]")
{
    std::atomic<bool> ran{false};
    const auto start = std::chrono::steady_clock::now();
    {
        RealScheduler scheduler;
        scheduler.scheduleAfter(1h, [&ran] { ran = true; });
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;

    CHECK_FALSE(ran);
    // It did not wait for the hour.
    CHECK(elapsed < 30s);
}
