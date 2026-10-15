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
#include "app/Controller.hpp"

#include <optional>

namespace app
{
    namespace
    {
        constexpr const char* CONNECTING_LABEL = "Connecting to the sampler";
        constexpr const char* DISCONNECTING_LABEL = "Disconnecting from the sampler";
        constexpr const char* QUITTING_LABEL = "Closing the connection before quitting";
        constexpr const char* UNKNOWN_MODEL = "unknown model";
        constexpr const char* UNKNOWN_VERSION = "unknown";

        /// What a connection reads once the session is open: which sampler answered. [RQ-APP-017]
        mcp::Outcome<SamplerIdentity> readIdentity(mcp::SamplerGateway& gateway, int deviceId)
        {
            const auto info = gateway.readSystemInfo();
            if (!info.ok())
                return mcp::Outcome<SamplerIdentity>::failure(info.problem);
            const auto settings = gateway.readSamplerSettings();
            if (!settings.ok())
                return mcp::Outcome<SamplerIdentity>::failure(settings.problem);
            return mcp::Outcome<SamplerIdentity>::success(SamplerIdentity{info.value->model.value_or(UNKNOWN_MODEL), settings.value->name,
                                                                          info.value->osVersion.value_or(UNKNOWN_VERSION), deviceId});
        }
    }

    // [RQ-APP-010, RQ-APP-011, RQ-APP-012, ADR-APP-001 (DEC-APP-003, DEC-APP-004, DEC-APP-007), TASK-APP-011]
    Controller::Controller(Model& model, UiDispatcher& dispatcher, UserPrompts& prompts, AppSettings& settings,
                           common::midi::MidiBackend& backend, mcp::GatewayConfig gatewayDefaults)
        : _model(model), _dispatcher(dispatcher), _prompts(prompts), _settings(settings), _backend(backend),
          _gatewayDefaults(std::move(gatewayDefaults)), _worker(dispatcher)
    {
    }

    Controller::~Controller() = default;

    bool Controller::idle() const
    {
        return _worker.idle();
    }

    void Controller::queue(std::string label, std::function<JobResult()> work)
    {
        _labels.push_back(label);
        _model.setBusy(_labels.front());
        _worker.submit([this, alive = std::weak_ptr<int>(_alive), work = std::move(work)]() -> SamplerWorker::Continuation {
            JobResult result = work();
            return [this, alive, result = std::move(result)]() mutable {
                if (!alive.expired())
                    finished(std::move(result));
            };
        });
    }

    void Controller::finished(JobResult result)
    {
        const std::string label = _labels.front();
        _labels.pop_front();
        _model.setBusy(_labels.empty() ? std::nullopt : std::optional<std::string>(_labels.front()));
        if (!result.problem.empty())
        {
            if (result.onFailure)
                result.onFailure(result.problem);
            else
                _model.setProblem(Problem{label, std::move(result.problem)});
            return;
        }
        result.apply();
    }

    // [RQ-APP-015 to RQ-APP-022, ADR-APP-001 (DEC-APP-008, DEC-APP-009), TASK-APP-012]
    bool Controller::connected() const
    {
        return _gateway && _model.connection().state == ConnectionState::Connected;
    }

    bool Controller::canConnect() const
    {
        const ConnectionState state = _model.connection().state;
        return state == ConnectionState::Disconnected || state == ConnectionState::Failed;
    }

    void Controller::connect(const MidiSetup& setup)
    {
        if (!canConnect())
            return;
        if (const auto problem = deviceIdProblem(setup.deviceId))
        {
            _model.setConnection(ConnectionStatus{ConnectionState::Failed, *problem, std::nullopt});
            return;
        }

        mcp::GatewayConfig config = _gatewayDefaults;
        config.inputPort = setup.inputPort;
        config.outputPort = setup.outputPort;
        config.deviceId = static_cast<std::uint32_t>(setup.deviceId);
        // The advice of a refusal in the application's words, not the MCP server's tool names. [RQ-APP-082, TASK-APP-023]
        if (!config.selectAdvice)
            config.selectAdvice = [](const std::string& kind) { return "Select a " + kind + " first."; };
        // On the MIDI backend's thread: only post, to the UI thread.
        config.portError = [this, alive = std::weak_ptr<int>(_alive), &dispatcher = _dispatcher](const std::string& port,
                                                                                                  const std::string& description) {
            dispatcher.post([this, alive, port, description] {
                if (!alive.expired())
                    portLost(port, description);
            });
        };
        _gateway = std::make_shared<mcp::SamplerGateway>(_backend, std::move(config));
        const std::uint64_t number = ++_connectionNumber;
        _model.setConnection(ConnectionStatus{ConnectionState::Connecting, {}, std::nullopt});

        const int deviceId = setup.deviceId;
        queue(CONNECTING_LABEL,
              wrap<SamplerIdentity>([gateway = _gateway, deviceId] { return readIdentity(*gateway, deviceId); },
                                    [this, number](const SamplerIdentity& identity) {
                                        if (number == _connectionNumber && _gateway)
                                            _model.setConnection(ConnectionStatus{ConnectionState::Connected, {}, identity});
                                    },
                                    [this, number](const std::string& reason) {
                                        if (number == _connectionNumber && _gateway)
                                            releaseGateway(ConnectionStatus{ConnectionState::Failed, reason, std::nullopt});
                                    }));
    }

    void Controller::disconnect()
    {
        if (_gateway)
            releaseGateway(ConnectionStatus{ConnectionState::Disconnected, {}, std::nullopt});
    }

    void Controller::applyMidiSetup(const MidiSetup& setup)
    {
        if (deviceIdProblem(setup.deviceId))
            return;
        storeMidiSetup(_settings, setup);
        if (!_gateway)
            return;
        disconnect();
        connect(setup);
    }

    void Controller::quit(std::function<void()> mayQuit)
    {
        disconnect();
        run<bool>(QUITTING_LABEL, [] { return mcp::Outcome<bool>::success(true); },
                  [mayQuit = std::move(mayQuit)](const bool&) { mayQuit(); });
    }

    void Controller::releaseGateway(ConnectionStatus after)
    {
        ++_connectionNumber;
        _model.setConnection(std::move(after));
        {
            const std::deque<SamplerWorker::Job> dropped = _worker.dropQueued();
            for (std::size_t i = 0; i < dropped.size(); ++i)
                _labels.pop_back();
        }
        _model.setBusy(_labels.empty() ? std::nullopt : std::optional<std::string>(_labels.front()));
        // The worker lets the gateway go, after the job in progress: its destruction closes the session (RQ-AKM-042), then the ports.
        queue(DISCONNECTING_LABEL, [gateway = std::move(_gateway)]() mutable -> JobResult {
            gateway.reset();
            return JobResult{{}, [] {}, {}};
        });
    }

    void Controller::portLost(const std::string& port, const std::string& description)
    {
        if (!_gateway)
            return;
        releaseGateway(ConnectionStatus{ConnectionState::Failed,
                                        "The MIDI port \"" + port + "\" reported an error: " + description +
                                            ". Nothing more is sent to the sampler; connect again once the port is back.",
                                        std::nullopt});
    }
}
