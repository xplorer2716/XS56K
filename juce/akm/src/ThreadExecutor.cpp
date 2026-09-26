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
#include "akm/ThreadExecutor.hpp"

#include <cassert>
#include <utility>

namespace akm
{
    ThreadExecutor::ThreadExecutor()
        : _thread([this] { run(); })
    {
        // The worker only reads this after taking a task from the queue, which is posted after the
        // constructor returns and goes through the mutex.
        _workerId = _thread.get_id();
    }

    ThreadExecutor::~ThreadExecutor()
    {
        assert(!isCurrentThread() && "a ThreadExecutor cannot be destroyed from its own task");
        {
            const std::lock_guard lock(_mutex);
            _stopping = true;
        }
        _wake.notify_all();
        _thread.join();
    }

    void ThreadExecutor::post(Task task)
    {
        {
            const std::lock_guard lock(_mutex);
            _queue.push_back(std::move(task));
        }
        _wake.notify_one();
    }

    bool ThreadExecutor::isCurrentThread() const
    {
        return std::this_thread::get_id() == _workerId;
    }

    void ThreadExecutor::run()
    {
        std::unique_lock lock(_mutex);
        for (;;)
        {
            _wake.wait(lock, [this] { return _stopping || !_queue.empty(); });
            if (_stopping)
                return;

            Task task = std::move(_queue.front());
            _queue.pop_front();
            lock.unlock();
            task();
            // Destroyed outside the lock, on the worker thread.
            task = nullptr;
            lock.lock();
        }
    }
}
