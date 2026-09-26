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

// The serial Executor of a session: tasks run one at a time, in the order they were posted. The manual
// one is drained by the test; the real one runs a worker thread.
// [TASK-AKM-005, RQ-AKM-008, RQ-AKM-016, RQ-AKM-019, RQ-AKM-020, ADR-AKM-001 (DEC-AKM-004)]
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "akm/ManualExecutor.hpp"
#include "akm/ThreadExecutor.hpp"

using namespace std::chrono_literals;
using akm::ManualExecutor;
using akm::ThreadExecutor;

namespace
{
    // Bounds the wait for something that must happen: a failure ends here, a success ends at once.
    constexpr auto eventTimeout = 10s;

    // How many threads post, and how many tasks each one posts, in the concurrent tests.
    constexpr int producerCount = 3;
    constexpr int tasksPerProducer = 500;
}

TEST_CASE("Given the manual executor and posted tasks, When nothing drains it, Then none runs [RQ-AKM-016]",
          "[akm][executor][manual]")
{
    ManualExecutor executor;
    int runs = 0;

    executor.post([&runs] { ++runs; });
    executor.post([&runs] { ++runs; });

    CHECK(runs == 0);
}

TEST_CASE("Given the manual executor and posted tasks, When it is drained, Then they run in the order they were posted [RQ-AKM-016]",
          "[akm][executor][manual]")
{
    ManualExecutor executor;
    std::vector<int> order;
    executor.post([&order] { order.push_back(1); });
    executor.post([&order] { order.push_back(2); });
    executor.post([&order] { order.push_back(3); });

    const std::size_t count = executor.runUntilIdle();

    CHECK(order == std::vector<int>{1, 2, 3});
    CHECK(count == 3);
}

TEST_CASE("Given a task that posts another, When the manual executor is drained, Then the new task runs behind those already queued [RQ-AKM-020]",
          "[akm][executor][manual]")
{
    ManualExecutor executor;
    std::vector<int> order;
    executor.post([&] {
        order.push_back(1);
        executor.post([&order] { order.push_back(3); });
    });
    executor.post([&order] { order.push_back(2); });

    const std::size_t count = executor.runUntilIdle();

    CHECK(order == std::vector<int>{1, 2, 3});
    CHECK(count == 3);
}

TEST_CASE("Given an empty manual executor, When it is drained, Then no task runs [RQ-AKM-016]",
          "[akm][executor][manual]")
{
    ManualExecutor executor;

    CHECK(executor.runUntilIdle() == 0);
}

TEST_CASE("Given the manual executor, When a task asks whether it is on the executor's thread, Then only inside a task it is [RQ-AKM-020]",
          "[akm][executor][manual]")
{
    ManualExecutor executor;
    bool insideTask = false;
    executor.post([&] { insideTask = executor.isCurrentThread(); });

    CHECK_FALSE(executor.isCurrentThread());
    executor.runUntilIdle();

    CHECK(insideTask);
    CHECK_FALSE(executor.isCurrentThread());
}

TEST_CASE("Given a task posted from another thread, When the test thread drains the manual executor, Then it runs there [RQ-AKM-016, RQ-AKM-020]",
          "[akm][executor][manual]")
{
    ManualExecutor executor;
    std::thread::id runner;
    std::thread producer([&] { executor.post([&runner] { runner = std::this_thread::get_id(); }); });
    producer.join();

    executor.runUntilIdle();

    CHECK(runner == std::this_thread::get_id());
}

TEST_CASE("Given the real executor and a task, When it is posted, Then it runs on a thread other than the caller's [RQ-AKM-020]",
          "[akm][executor][real]")
{
    ThreadExecutor executor;
    std::promise<std::pair<std::thread::id, bool>> ran;
    executor.post([&] { ran.set_value(std::make_pair(std::this_thread::get_id(), executor.isCurrentThread())); });

    auto result = ran.get_future();
    REQUIRE(result.wait_for(eventTimeout) == std::future_status::ready);
    const auto [runner, isCurrent] = result.get();

    CHECK(runner != std::this_thread::get_id());
    CHECK(isCurrent);
    CHECK_FALSE(executor.isCurrentThread());
}

TEST_CASE("Given the real executor and tasks posted from three threads, When they run, Then they never overlap and each thread's tasks keep their order [RQ-AKM-008, RQ-AKM-020]",
          "[akm][executor][real]")
{
    ThreadExecutor executor;
    std::atomic<int> running{0};
    std::atomic<int> overlaps{0};
    std::mutex resultsMutex;
    std::vector<std::vector<int>> seen(producerCount);

    std::vector<std::thread> producers;
    for (int producer = 0; producer < producerCount; ++producer)
    {
        producers.emplace_back([&, producer] {
            for (int sequence = 0; sequence < tasksPerProducer; ++sequence)
            {
                executor.post([&, producer, sequence] {
                    if (++running != 1)
                        ++overlaps;
                    {
                        const std::lock_guard lock(resultsMutex);
                        seen[static_cast<std::size_t>(producer)].push_back(sequence);
                    }
                    --running;
                });
            }
        });
    }
    for (auto& producer : producers)
        producer.join();

    // Everything posted by the producers is queued: a last task marks the end.
    std::promise<void> finished;
    executor.post([&finished] { finished.set_value(); });
    REQUIRE(finished.get_future().wait_for(eventTimeout) == std::future_status::ready);

    CHECK(overlaps == 0);
    const std::lock_guard lock(resultsMutex);
    for (const auto& perProducer : seen)
    {
        REQUIRE(perProducer.size() == tasksPerProducer);
        for (int sequence = 0; sequence < tasksPerProducer; ++sequence)
            CHECK(perProducer[static_cast<std::size_t>(sequence)] == sequence);
    }
}

TEST_CASE("Given a task that posts another on the real executor, When they run, Then the new task runs behind those already queued [RQ-AKM-020]",
          "[akm][executor][real]")
{
    ThreadExecutor executor;
    std::promise<void> release;
    std::shared_future<void> released = release.get_future().share();
    std::promise<void> started;
    std::mutex mutex;
    std::vector<int> order;
    std::promise<void> finished;

    // The first task waits, so that the second is queued before the first posts the third.
    executor.post([&] {
        started.set_value();
        released.wait();
        {
            const std::lock_guard lock(mutex);
            order.push_back(1);
        }
        executor.post([&] {
            {
                const std::lock_guard lock(mutex);
                order.push_back(3);
            }
            finished.set_value();
        });
    });
    REQUIRE(started.get_future().wait_for(eventTimeout) == std::future_status::ready);
    executor.post([&] {
        const std::lock_guard lock(mutex);
        order.push_back(2);
    });
    release.set_value();

    REQUIRE(finished.get_future().wait_for(eventTimeout) == std::future_status::ready);
    const std::lock_guard lock(mutex);
    CHECK(order == std::vector<int>{1, 2, 3});
}

TEST_CASE("Given the real executor destroyed with a task pending, When it is destroyed, Then it returns without running that task [RQ-AKM-020]",
          "[akm][executor][real]")
{
    // A running task holds the worker while the victim is queued behind it; the executor is destroyed
    // from another thread, and only then is the running task let go.
    auto executor = std::make_unique<ThreadExecutor>();
    std::promise<void> release;
    std::shared_future<void> released = release.get_future().share();
    std::promise<void> holding;
    std::atomic<bool> victimRan{false};
    std::atomic<bool> holderFinished{false};

    executor->post([&] {
        holding.set_value();
        released.wait();
        holderFinished = true;
    });
    REQUIRE(holding.get_future().wait_for(eventTimeout) == std::future_status::ready);
    executor->post([&victimRan] { victimRan = true; });

    std::thread destroyer([&executor] { executor.reset(); });
    // The destroyer needs a moment to mark the executor as stopping before the worker is let go; the
    // worker cannot finish the task in progress before then, so the wait only has to be long enough
    // for a thread to be scheduled.
    std::this_thread::sleep_for(300ms);
    release.set_value();
    destroyer.join();

    CHECK(holderFinished);
    CHECK_FALSE(victimRan);
}

TEST_CASE("Given an idle real executor, When it is destroyed, Then it returns [RQ-AKM-020]", "[akm][executor][real]")
{
    const auto start = std::chrono::steady_clock::now();
    {
        const ThreadExecutor executor;
    }

    CHECK(std::chrono::steady_clock::now() - start < 30s);
}
