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

namespace app
{
    /// Runs a task on the UI thread, later: the JUCE message thread in the application (`MessageManager::callAsync`), a queue the test drains
    /// in the tests. `post` may be called from any thread and never waits for the task. [RQ-APP-012, RQ-APP-014, ADR-APP-001 (DEC-APP-003)]
    class UiDispatcher
    {
    public:
        virtual ~UiDispatcher() = default;

        virtual void post(std::function<void()> task) = 0;
    };
}
