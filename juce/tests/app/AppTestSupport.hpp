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

// What the tests of `xs56k_app_core` put in the place of the JUCE application: a UI dispatcher whose queue the test drains (the test's
// own thread is the UI thread), scripted answers to the questions, settings in memory, and a rig of the core over the simulated sampler.
// [RQ-APP-014, ADR-APP-001 (DEC-APP-003, DEC-APP-007, DEC-APP-009, DEC-APP-010), TASK-APP-011]

#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "akm/RealScheduler.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"
#include "app/AppSettings.hpp"
#include "app/Controller.hpp"
#include "app/Model.hpp"
#include "app/UiDispatcher.hpp"
#include "app/UserPrompts.hpp"
#include "mcp/SamplerGateway.hpp"

namespace app::test
{
    inline constexpr std::chrono::milliseconds TEST_COMMAND_TIMEOUT{300};
    inline constexpr std::chrono::milliseconds TEST_DISK_TIMEOUT{1500};
    inline constexpr std::chrono::seconds SETTLE_DEADLINE{15};
    inline constexpr std::chrono::milliseconds SETTLE_POLL{5};

    /// The UI thread of the tests: the continuations wait in a queue until the test drains it.
    class QueueDispatcher final : public UiDispatcher
    {
    public:
        void post(std::function<void()> task) override
        {
            const std::lock_guard lock(_mutex);
            _tasks.push_back(std::move(task));
        }

        /// Runs what was posted, and what that posted in turn; answers how many tasks ran.
        std::size_t drain()
        {
            std::size_t ran = 0;
            for (;;)
            {
                std::deque<std::function<void()>> tasks;
                {
                    const std::lock_guard lock(_mutex);
                    tasks.swap(_tasks);
                }
                if (tasks.empty())
                    return ran;
                for (auto& task : tasks)
                {
                    task();
                    ++ran;
                }
            }
        }

        [[nodiscard]] bool empty() const
        {
            const std::lock_guard lock(_mutex);
            return _tasks.empty();
        }

    private:
        mutable std::mutex _mutex;
        std::deque<std::function<void()>> _tasks;
    };

    /// The person's answers, given in order; a question with no answer left is cancelled. Every question is kept.
    class ScriptedPrompts final : public UserPrompts
    {
    public:
        void confirm(const Confirmation& question, std::function<void(bool)> answer) override
        {
            asked.push_back(question);
            bool confirmed = false;
            if (!answers.empty())
            {
                confirmed = answers.front();
                answers.pop_front();
            }
            answer(confirmed);
        }

        std::deque<bool> answers;
        std::vector<Confirmation> asked;
    };

    /// Settings that live as long as the test.
    class MemorySettings final : public AppSettings
    {
    public:
        [[nodiscard]] std::optional<std::string> read(std::string_view key) const override
        {
            const auto found = values.find(std::string(key));
            if (found == values.end())
                return std::nullopt;
            return found->second;
        }

        void write(std::string_view key, std::string value) override { values[std::string(key)] = std::move(value); }

        std::map<std::string, std::string> values;
    };

    /// The core over a gateway on the simulated sampler: the scheduler, the bus, the sampler, the gateway and the test's UI.
    struct CoreRig
    {
        explicit CoreRig(std::chrono::milliseconds commandTimeout = TEST_COMMAND_TIMEOUT)
            : sampler(&backend.addSampler()), gateway(backend, configFor(backend, commandTimeout))
        {
        }

        static mcp::GatewayConfig configFor(const akm::harness::SimulatedMidiBackend& bus, std::chrono::milliseconds commandTimeout)
        {
            mcp::GatewayConfig config;
            config.inputPort = bus.inputName();
            config.outputPort = bus.outputName();
            config.commandTimeout = commandTimeout;
            config.diskTimeout = TEST_DISK_TIMEOUT;
            return config;
        }

        /// Drains the UI queue until the worker has nothing left to do and nothing waits to be run; false at the deadline.
        bool settle()
        {
            const auto deadline = std::chrono::steady_clock::now() + SETTLE_DEADLINE;
            while (std::chrono::steady_clock::now() < deadline)
            {
                dispatcher.drain();
                if (controller.idle() && dispatcher.empty())
                    return true;
                std::this_thread::sleep_for(SETTLE_POLL);
            }
            return false;
        }

        akm::RealScheduler scheduler;
        akm::harness::SimulatedMidiBackend backend{scheduler};
        akm::harness::SimulatedSampler* sampler;
        mcp::SamplerGateway gateway;
        QueueDispatcher dispatcher;
        ScriptedPrompts prompts;
        Model model;
        Controller controller{model, dispatcher, prompts};
    };
}
