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
#include "akm/testing/SimulatedMidiBackend.hpp"

#include <algorithm>
#include <atomic>
#include <future>
#include <mutex>
#include <utility>

#include "akm/ThreadExecutor.hpp"

namespace akm::testing
{
    using common::midi::MessageType;
    using common::midi::MidiInputCallbacks;
    using common::midi::MidiInputPort;
    using common::midi::MidiMessage;
    using common::midi::MidiOutputPort;

    namespace
    {
        using Bytes = std::vector<std::uint8_t>;

        // What a host's input port sees of the bus: its callbacks, and whether it is started or closed.
        struct InputBody
        {
            MidiInputCallbacks callbacks;
            std::atomic<bool> started{false};
            std::atomic<bool> closed{false};
        };
    }

    // The shared state of the bus, held by the backend and by every port it opened, so that a port can
    // outlive the backend, and a task scheduled on the scheduler can find out that the bus is gone.
    struct SimulatedMidiBackend::Bus : std::enable_shared_from_this<Bus>
    {
        std::mutex mutex;
        std::vector<std::unique_ptr<SimulatedSampler>> samplers;
        std::vector<std::weak_ptr<InputBody>> inputs;
        DeliveryMode mode = DeliveryMode::OnSendingThread;
        // The simulation's own thread, created when the mode asks for one, joined by shutdown().
        std::unique_ptr<ThreadExecutor> delivery;
        bool shutDown = false;
        std::vector<MidiMessage> sent;
        std::vector<MidiMessage> emitted;

        // A sampler puts a message on the bus (or a test injects one).
        void emit(Bytes bytes)
        {
            {
                const std::lock_guard lock(mutex);
                if (shutDown)
                    return;
                emitted.push_back(MidiMessage::fromRawBytes(bytes));
                if (mode != DeliveryMode::OnSendingThread && delivery)
                {
                    delivery->post([weak = weak_from_this(), bytes = std::move(bytes)] {
                        if (const auto bus = weak.lock())
                            bus->dispatch(bytes);
                    });
                    return;
                }
            }
            dispatch(bytes);
        }

        // Hands a message to the host's started input ports, on the calling thread. Only SysEx is carried.
        void dispatch(const Bytes& bytes)
        {
            const MidiMessage message = MidiMessage::fromRawBytes(bytes);
            if (message.type() != MessageType::SysEx)
                return;
            std::vector<std::shared_ptr<InputBody>> targets;
            {
                const std::lock_guard lock(mutex);
                for (const auto& weak : inputs)
                {
                    if (auto body = weak.lock())
                        targets.push_back(std::move(body));
                }
            }
            // Called with the lock released: a callback may send, or open and close ports.
            for (const auto& body : targets)
            {
                if (body->started && !body->closed && body->callbacks.onSysExMessage)
                    body->callbacks.onSysExMessage(message);
            }
        }

        // The host sends a message: every sampler receives it.
        void send(const MidiMessage& message)
        {
            std::vector<SimulatedSampler*> targets;
            DeliveryMode currentMode{};
            {
                const std::lock_guard lock(mutex);
                if (shutDown)
                    return;
                sent.push_back(message);
                for (const auto& sampler : samplers)
                    targets.push_back(sampler.get());
                currentMode = mode;
            }
            if (message.type() == MessageType::SysEx)
            {
                for (SimulatedSampler* sampler : targets)
                    sampler->receive(message.bytes());
            }
            if (currentMode == DeliveryMode::OnOtherThreadBeforeSendReturns)
                waitForDeliveries();
        }

        // Returns once what was posted to the delivery thread before this call has been delivered. The
        // promise belongs to the task alone, so that a task discarded by shutdown() releases the wait.
        void waitForDeliveries()
        {
            auto reached = std::make_shared<std::promise<void>>();
            std::future<void> done = reached->get_future();
            {
                const std::lock_guard lock(mutex);
                if (shutDown || !delivery)
                    return;
                delivery->post([reached = std::move(reached)] { reached->set_value(); });
            }
            done.wait();
        }

        void shutdown()
        {
            std::unique_ptr<ThreadExecutor> victim;
            {
                const std::lock_guard lock(mutex);
                shutDown = true;
                victim = std::move(delivery);
            }
            // Joined outside the lock: the delivery thread may be inside dispatch(), which takes it.
            victim.reset();
        }
    };

    namespace
    {
        class SimulatedInputPort final : public MidiInputPort
        {
        public:
            using Bus = SimulatedMidiBackend::Bus;

            SimulatedInputPort(std::shared_ptr<Bus> bus, std::string name)
                : _bus(std::move(bus)), _name(std::move(name)), _body(std::make_shared<InputBody>())
            {
                const std::lock_guard lock(_bus->mutex);
                _bus->inputs.push_back(_body);
            }

            ~SimulatedInputPort() override
            {
                _body->started = false;
                _body->closed = true;
                const std::lock_guard lock(_bus->mutex);
                std::erase_if(_bus->inputs, [this](const std::weak_ptr<InputBody>& candidate) {
                    return candidate.expired() || candidate.lock() == _body;
                });
            }

            [[nodiscard]] std::string deviceName() const override { return _name; }
            void setCallbacks(MidiInputCallbacks callbacks) override { _body->callbacks = std::move(callbacks); }
            void start() override { _body->started = true; }
            void stop() override { _body->started = false; }
            [[nodiscard]] bool isStarted() const override { return _body->started; }

        private:
            std::shared_ptr<Bus> _bus;
            std::string _name;
            std::shared_ptr<InputBody> _body;
        };

        class SimulatedOutputPort final : public MidiOutputPort
        {
        public:
            using Bus = SimulatedMidiBackend::Bus;

            SimulatedOutputPort(std::shared_ptr<Bus> bus, std::string name) : _bus(std::move(bus)), _name(std::move(name)) {}

            [[nodiscard]] std::string deviceName() const override { return _name; }
            void send(const MidiMessage& message) override { _bus->send(message); }

        private:
            std::shared_ptr<Bus> _bus;
            std::string _name;
        };
    }

    SimulatedMidiBackend::SimulatedMidiBackend(Scheduler& scheduler, std::string inputName, std::string outputName)
        : _scheduler(scheduler), _inputName(std::move(inputName)), _outputName(std::move(outputName)),
          _bus(std::make_shared<Bus>())
    {
    }

    SimulatedMidiBackend::~SimulatedMidiBackend()
    {
        _bus->shutdown();
    }

    SimulatedSampler& SimulatedMidiBackend::addSampler(SamplerConfig config)
    {
        auto sampler = std::make_unique<SimulatedSampler>(config, _scheduler, [weak = std::weak_ptr<Bus>(_bus)](Bytes bytes) {
            if (const auto bus = weak.lock())
                bus->emit(std::move(bytes));
        });
        SimulatedSampler& reference = *sampler;
        const std::lock_guard lock(_bus->mutex);
        _bus->samplers.push_back(std::move(sampler));
        return reference;
    }

    void SimulatedMidiBackend::setDeliveryMode(DeliveryMode mode)
    {
        const std::lock_guard lock(_bus->mutex);
        _bus->mode = mode;
        if (mode != DeliveryMode::OnSendingThread && !_bus->delivery && !_bus->shutDown)
            _bus->delivery = std::make_unique<ThreadExecutor>();
    }

    void SimulatedMidiBackend::injectToHost(std::vector<std::uint8_t> bytes)
    {
        _bus->emit(std::move(bytes));
    }

    std::vector<MidiMessage> SimulatedMidiBackend::sentByHost() const
    {
        const std::lock_guard lock(_bus->mutex);
        return _bus->sent;
    }

    std::vector<MidiMessage> SimulatedMidiBackend::emittedBySamplers() const
    {
        const std::lock_guard lock(_bus->mutex);
        return _bus->emitted;
    }

    std::vector<std::string> SimulatedMidiBackend::inputDeviceNames() const
    {
        return {_inputName};
    }

    std::vector<std::string> SimulatedMidiBackend::outputDeviceNames() const
    {
        return {_outputName};
    }

    std::unique_ptr<MidiInputPort> SimulatedMidiBackend::openInput(const std::string& deviceName)
    {
        if (deviceName != _inputName)
            return nullptr;
        return std::make_unique<SimulatedInputPort>(_bus, deviceName);
    }

    std::unique_ptr<MidiOutputPort> SimulatedMidiBackend::openOutput(const std::string& deviceName)
    {
        if (deviceName != _outputName)
            return nullptr;
        return std::make_unique<SimulatedOutputPort>(_bus, deviceName);
    }
}
