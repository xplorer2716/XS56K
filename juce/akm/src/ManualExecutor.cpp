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
#include "akm/ManualExecutor.hpp"

#include <cassert>
#include <utility>

namespace akm
{
    void ManualExecutor::post(Task task)
    {
        const std::lock_guard lock(_mutex);
        _queue.push_back(std::move(task));
    }

    bool ManualExecutor::isCurrentThread() const
    {
        return _runner.load() == std::this_thread::get_id();
    }

    std::size_t ManualExecutor::runUntilIdle()
    {
        assert(_runner.load() == std::thread::id{} && "runUntilIdle() must not be reentrant or concurrent");
        _runner.store(std::this_thread::get_id());

        std::size_t count = 0;
        for (;;)
        {
            Task task;
            {
                const std::lock_guard lock(_mutex);
                if (_queue.empty())
                    break;
                task = std::move(_queue.front());
                _queue.pop_front();
            }
            task();
            // Destroyed here, still on the executor's thread and outside the lock.
            task = nullptr;
            ++count;
        }

        _runner.store(std::thread::id{});
        return count;
    }
}
