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
#include <cstdio>
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
#include "app/MidiSetup.hpp"
#include "app/Model.hpp"
#include "common/midi/MidiPorts.hpp"
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

        void askCount(const CountQuestion& question, std::function<void(std::optional<int>)> answer) override
        {
            countsAsked.push_back(question);
            std::optional<int> count;
            if (!counts.empty())
            {
                count = counts.front();
                counts.pop_front();
            }
            answer(count);
        }

        std::deque<bool> answers;
        std::vector<Confirmation> asked;
        std::deque<int> counts;  ///< a count asked with none left is cancelled [RQ-APP-061]
        std::vector<CountQuestion> countsAsked;
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

    /// The computer's MIDI ports as the tests see them: two inputs "IN A" and "IN B" and two outputs "OUT A" and "OUT B", every one of
    /// them reaching the simulated bus. It records, in order, each port opened, each message sent and each port released, and can make
    /// an open input report an error as an unplugged device does. [RQ-APP-015, RQ-APP-020, RQ-APP-021, RQ-APP-022, TASK-APP-012]
    class RecordingBackend final : public common::midi::MidiBackend
    {
    public:
        explicit RecordingBackend(akm::harness::SimulatedMidiBackend& bus) : _bus(bus) {}

        [[nodiscard]] std::vector<std::string> inputDeviceNames() const override { return {"IN A", "IN B"}; }
        [[nodiscard]] std::vector<std::string> outputDeviceNames() const override { return {"OUT A", "OUT B"}; }

        [[nodiscard]] std::unique_ptr<common::midi::MidiInputPort> openInput(const std::string& deviceName) override
        {
            if (deviceName != "IN A" && deviceName != "IN B")
                return nullptr;
            auto port = _bus.openInput(_bus.inputName());
            if (!port)
                return nullptr;
            record("open " + deviceName);
            return std::make_unique<Input>(*this, deviceName, std::move(port));
        }

        [[nodiscard]] std::unique_ptr<common::midi::MidiOutputPort> openOutput(const std::string& deviceName) override
        {
            if (deviceName != "OUT A" && deviceName != "OUT B")
                return nullptr;
            auto port = _bus.openOutput(_bus.outputName());
            if (!port)
                return nullptr;
            record("open " + deviceName);
            return std::make_unique<Output>(*this, deviceName, std::move(port));
        }

        /// What happened to the ports, in order: "open <port>", "send <port>", "release <port>".
        [[nodiscard]] std::vector<std::string> events() const
        {
            const std::lock_guard lock(_mutex);
            return _events;
        }

        /// The position of the first event equal to `event` from `from` on, or nothing.
        [[nodiscard]] std::optional<std::size_t> find(const std::string& event, std::size_t from = 0) const
        {
            const auto all = events();
            for (std::size_t i = from; i < all.size(); ++i)
            {
                if (all[i] == event)
                    return i;
            }
            return std::nullopt;
        }

        /// Makes the open input report an error, as the device does when it is unplugged; false when no input is open.
        bool raiseInputError(const std::string& description)
        {
            std::function<void(const std::string&)> onError;
            {
                const std::lock_guard lock(_mutex);
                onError = _inputError;
            }
            if (!onError)
                return false;
            onError(description);
            return true;
        }

    private:
        class Input final : public common::midi::MidiInputPort
        {
        public:
            Input(RecordingBackend& owner, std::string name, std::unique_ptr<common::midi::MidiInputPort> port)
                : _owner(owner), _name(std::move(name)), _port(std::move(port))
            {
            }
            ~Input() override
            {
                try  // a destructor must not throw [TASK-QLT-021, RQ-QLT-001]
                {
                    _owner.setInputError({});
                    _port.reset();
                    _owner.record("release " + _name);
                }
                catch (...)
                {
                    std::fputs("AppTestSupport: a port could not record its release\n", stderr);  // a destructor must not throw
                }
            }
            Input(const Input&) = delete;
            Input& operator=(const Input&) = delete;

            [[nodiscard]] std::string deviceName() const override { return _name; }
            void setCallbacks(common::midi::MidiInputCallbacks callbacks) override
            {
                _owner.setInputError(callbacks.onError);
                _port->setCallbacks(std::move(callbacks));
            }
            void start() override { _port->start(); }
            void stop() override { _port->stop(); }
            [[nodiscard]] bool isStarted() const override { return _port->isStarted(); }

        private:
            RecordingBackend& _owner;
            std::string _name;
            std::unique_ptr<common::midi::MidiInputPort> _port;
        };

        class Output final : public common::midi::MidiOutputPort
        {
        public:
            Output(RecordingBackend& owner, std::string name, std::unique_ptr<common::midi::MidiOutputPort> port)
                : _owner(owner), _name(std::move(name)), _port(std::move(port))
            {
            }
            ~Output() override
            {
                try  // a destructor must not throw [TASK-QLT-021, RQ-QLT-001]
                {
                    _port.reset();
                    _owner.record("release " + _name);
                }
                catch (...)
                {
                    std::fputs("AppTestSupport: a port could not record its release\n", stderr);  // a destructor must not throw
                }
            }
            Output(const Output&) = delete;
            Output& operator=(const Output&) = delete;

            [[nodiscard]] std::string deviceName() const override { return _name; }
            void send(const common::midi::MidiMessage& message) override
            {
                _owner.record("send " + _name);
                _port->send(message);
            }

        private:
            RecordingBackend& _owner;
            std::string _name;
            std::unique_ptr<common::midi::MidiOutputPort> _port;
        };

        void record(std::string event)
        {
            const std::lock_guard lock(_mutex);
            _events.push_back(std::move(event));
        }

        void setInputError(std::function<void(const std::string&)> onError)
        {
            const std::lock_guard lock(_mutex);
            _inputError = std::move(onError);
        }

        akm::harness::SimulatedMidiBackend& _bus;
        mutable std::mutex _mutex;
        std::vector<std::string> _events;
        std::function<void(const std::string&)> _inputError;
    };

    /// The MIDI setup on the first pair of ports.
    inline MidiSetup setupA(int deviceId = 0)
    {
        return MidiSetup{"IN A", "OUT A", deviceId};
    }

    /// The core over a gateway on the simulated sampler: the scheduler, the bus, the sampler, the gateway and the test's UI. The
    /// controller reaches the bus through the recording backend; `gateway` is a gateway of the test's own on the same bus.
    struct CoreRig
    {
        explicit CoreRig(std::chrono::milliseconds commandTimeout = TEST_COMMAND_TIMEOUT, akm::harness::SamplerConfig samplerConfig = {})
            : sampler(&backend.addSampler(samplerConfig)), gateway(backend, configFor(backend, commandTimeout)),
              controller(model, dispatcher, prompts, settings, recording, configFor(backend, commandTimeout))
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
        RecordingBackend recording{backend};
        akm::harness::SimulatedSampler* sampler;
        mcp::SamplerGateway gateway;
        QueueDispatcher dispatcher;
        ScriptedPrompts prompts;
        MemorySettings settings;
        Model model;
        Controller controller;
    };
}
