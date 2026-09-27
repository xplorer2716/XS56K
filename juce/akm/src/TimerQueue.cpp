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
#include "akm/detail/TimerQueue.hpp"

#include <algorithm>
#include <utility>

namespace akm::detail
{
    TimerHandle TimerQueue::push(Clock::time_point deadline, Task task)
    {
        auto cancelled = std::make_shared<std::atomic<bool>>(false);
        // A multimap inserts an equivalent key after the ones already there: the order of pushing is kept.
        _entries.emplace(deadline, Entry{deadline, cancelled, std::move(task)});
        return TimerHandle(std::move(cancelled));
    }

    std::optional<TimerQueue::Entry> TimerQueue::popDue(Clock::time_point limit, Entries& discarded)
    {
        while (!_entries.empty())
        {
            const auto first = _entries.begin();
            if (first->second.cancelled->load())
            {
                discarded.push_back(std::move(first->second));
                _entries.erase(first);
                continue;
            }
            if (first->first > limit)
                return std::nullopt;
            Entry due = std::move(first->second);
            _entries.erase(first);
            return due;
        }
        return std::nullopt;
    }

    std::optional<TimerQueue::Clock::time_point> TimerQueue::nextDeadline() const
    {
        if (_entries.empty())
            return std::nullopt;
        return _entries.begin()->first;
    }

    std::size_t TimerQueue::liveCount() const
    {
        return static_cast<std::size_t>(std::count_if(_entries.begin(), _entries.end(),
                                                      [](const auto& item) { return !item.second.cancelled->load(); }));
    }
}
