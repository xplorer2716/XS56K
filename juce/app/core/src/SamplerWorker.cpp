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
#include "app/SamplerWorker.hpp"

#include <utility>

namespace app
{
    // [RQ-APP-012, ADR-APP-001 (DEC-APP-003), TASK-APP-011]
    SamplerWorker::SamplerWorker(UiDispatcher& dispatcher) : _dispatcher(dispatcher), _thread([this] { loop(); }) {}

    SamplerWorker::~SamplerWorker()
    {
        {
            const std::lock_guard lock(_mutex);
            _stopping = true;
            _jobs.clear();
        }
        _wake.notify_all();
        _thread.join();
    }

    void SamplerWorker::submit(Job job)
    {
        {
            const std::lock_guard lock(_mutex);
            _jobs.push_back(std::move(job));
        }
        _wake.notify_one();
    }

    bool SamplerWorker::idle() const
    {
        const std::lock_guard lock(_mutex);
        return _jobs.empty() && !_running;
    }

    void SamplerWorker::loop()
    {
        for (;;)
        {
            Job job;
            {
                std::unique_lock lock(_mutex);
                _wake.wait(lock, [this] { return _stopping || !_jobs.empty(); });
                if (_stopping)
                    return;
                job = std::move(_jobs.front());
                _jobs.pop_front();
                _running = true;
            }
            Continuation continuation = job();
            _dispatcher.post(std::move(continuation));
            {
                const std::lock_guard lock(_mutex);
                _running = false;
            }
        }
    }
}
