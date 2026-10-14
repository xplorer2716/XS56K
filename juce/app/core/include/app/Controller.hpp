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

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "app/AppSettings.hpp"
#include "app/MidiSetup.hpp"
#include "app/Model.hpp"
#include "app/SamplerWorker.hpp"
#include "app/UiDispatcher.hpp"
#include "app/UserPrompts.hpp"
#include "common/midi/MidiPorts.hpp"
#include "mcp/SamplerGateway.hpp"

namespace app
{
    /// Turns the person's actions into jobs for the sampler worker and their results into changes of the model. Called on the UI thread only:
    /// an action returns at once, the model names the job in progress, and the result comes back through the UI dispatcher. A failed call
    /// leaves the model as it was and records what failed and why. It owns the connection: one gateway per connection, created by "Connect",
    /// destroyed on the worker by "Disconnect", a change of setup, a lost port or quitting. [RQ-APP-010 to RQ-APP-013, RQ-APP-015 to
    /// RQ-APP-022, ADR-APP-001 (DEC-APP-001, DEC-APP-003, DEC-APP-007, DEC-APP-008, DEC-APP-009)]
    class Controller
    {
    public:
        /// `gatewayDefaults` gives the timeouts and the screen mode of every gateway; the ports, the DeviceID and the port-error callback are
        /// the controller's.
        Controller(Model& model, UiDispatcher& dispatcher, UserPrompts& prompts, AppSettings& settings, common::midi::MidiBackend& backend,
                   mcp::GatewayConfig gatewayDefaults = {});
        ~Controller();

        Controller(const Controller&) = delete;
        Controller& operator=(const Controller&) = delete;

        /// Queues `call` (it runs on the worker, so it may block on the sampler) under `label`; on success `onSuccess` gets the value on the
        /// UI thread, on failure the model gets the problem and `onSuccess` is not called. [RQ-APP-010, RQ-APP-012]
        template <typename T>
        void run(std::string label, std::function<mcp::Outcome<T>()> call, std::function<void(const T&)> onSuccess)
        {
            queue(std::move(label), wrap<T>(std::move(call), std::move(onSuccess), {}));
        }

        /// Asks `question` first, and queues the job only when the person confirms: a cancel sends nothing. [RQ-APP-011, DEC-APP-007]
        template <typename T>
        void runConfirmed(Confirmation question, std::string label, std::function<mcp::Outcome<T>()> call,
                          std::function<void(const T&)> onSuccess)
        {
            _prompts.confirm(question, [this, alive = std::weak_ptr<int>(_alive), label = std::move(label), call = std::move(call),
                                        onSuccess = std::move(onSuccess)](bool confirmed) mutable {
                if (confirmed && !alive.expired())
                    run<T>(std::move(label), std::move(call), std::move(onSuccess));
            });
        }

        /// Queues a call to the gateway of the connection, as `run` does; while the application is not connected it queues nothing and
        /// answers false. [RQ-APP-019]
        template <typename T>
        bool runOnSampler(std::string label, std::function<mcp::Outcome<T>(mcp::SamplerGateway&)> call, std::function<void(const T&)> onSuccess)
        {
            if (!connected())
                return false;
            run<T>(std::move(label), [gateway = _gateway, call = std::move(call)] { return call(*gateway); }, std::move(onSuccess));
            return true;
        }

        /// True while the application is disconnected or its connection failed: "Connect" is offered then. [RQ-APP-007, RQ-APP-019]
        [[nodiscard]] bool canConnect() const;

        /// Opens the ports and the session with `setup` and reads which sampler answered; the model says connecting, then connected or
        /// failed with the reason. [RQ-APP-017, RQ-APP-018]
        void connect(const MidiSetup& setup);
        /// Drops the jobs not started, then closes the session and releases the ports on the worker, after the job in progress. [RQ-APP-021]
        void disconnect();
        /// Stores the setup the person validated and, when connected, connects again with it once the old session is closed and its
        /// ports released. A DeviceID that is not 0 to 31 is not stored. [RQ-APP-016, RQ-APP-020]
        void applyMidiSetup(const MidiSetup& setup);
        /// Disconnects, then calls `mayQuit` on the UI thread once the session is closed and the ports released; the UI thread is never
        /// blocked meanwhile. [RQ-APP-021]
        void quit(std::function<void()> mayQuit);

        /// True when the worker has nothing left to run and every result has been handed to the UI dispatcher.
        [[nodiscard]] bool idle() const;

    private:
        /// What a job's work gives back to the UI thread: a problem, or what to apply to the model. `onFailure`, when set, takes the problem
        /// in the place of the model's problem.
        struct JobResult
        {
            std::string problem;
            std::function<void()> apply;
            std::function<void(const std::string&)> onFailure;
        };

        template <typename T>
        static std::function<JobResult()> wrap(std::function<mcp::Outcome<T>()> call, std::function<void(const T&)> onSuccess,
                                               std::function<void(const std::string&)> onFailure)
        {
            return [call = std::move(call), onSuccess = std::move(onSuccess), onFailure = std::move(onFailure)]() -> JobResult {
                mcp::Outcome<T> outcome = call();
                if (!outcome.ok())
                    return JobResult{std::move(outcome.problem), {}, onFailure};
                return JobResult{{}, [onSuccess, value = std::move(*outcome.value)] { onSuccess(value); }, {}};
            };
        }

        [[nodiscard]] bool connected() const;
        void queue(std::string label, std::function<JobResult()> work);
        void finished(JobResult result);
        /// Sets the connection state at once, then drops the jobs not started and destroys the gateway on the worker. [DEC-APP-008]
        void releaseGateway(ConnectionStatus after);
        void portLost(const std::string& port, const std::string& description);

        Model& _model;
        UiDispatcher& _dispatcher;
        UserPrompts& _prompts;
        AppSettings& _settings;
        common::midi::MidiBackend& _backend;
        const mcp::GatewayConfig _gatewayDefaults;
        /// The gateway of the connection, or nothing while disconnected. Jobs hold copies; the last one is let go on the worker.
        std::shared_ptr<mcp::SamplerGateway> _gateway;
        /// Counts the connections: the answer of a connection that was since closed is ignored.
        std::uint64_t _connectionNumber = 0;
        /// The labels of the jobs queued and not finished, in order: the first is the job in progress.
        std::deque<std::string> _labels;
        /// Expires with the controller: a continuation posted before its destruction then does nothing.
        std::shared_ptr<int> _alive = std::make_shared<int>(0);
        SamplerWorker _worker;  // last: destroyed first, so no job outlives what it uses
    };
}
