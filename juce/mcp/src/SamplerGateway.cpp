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
#include "mcp/SamplerGateway.hpp"

#include <algorithm>
#include <functional>
#include <future>
#include <span>
#include <sstream>
#include <utility>
#include <variant>

#include "akm/CommandOptions.hpp"
#include "akm/DiagnosticSink.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/KeygroupPrimitives.hpp"
#include "akm/ProgramPrimitives.hpp"
#include "akm/MultiPrimitives.hpp"
#include "akm/RealScheduler.hpp"
#include "akm/SamplePrimitives.hpp"
#include "akm/SamplerError.hpp"
#include "akm/SystemSetup.hpp"
#include "akm/SystemVersion.hpp"
#include "akm/ThreadExecutor.hpp"
#include "akm/ZonePrimitives.hpp"
#include "GatewayDetail.hpp"

namespace mcp
{
    using detail::WAIT_MARGIN;
    using detail::await;
    using detail::explain;
    using detail::numberText;

    namespace
    {
        // The opening sends up to this many commands (the checksum mode and the settings) after the discovery.
        constexpr int OPENING_COMMAND_BUDGET = 8;
        constexpr int CLOSING_COMMAND_BUDGET = 8;
        constexpr std::int64_t ALL_KEYGROUPS = 0;
        constexpr std::int64_t ALL_ZONES = 0;
        constexpr int ZONES_PER_KEYGROUP = 4;
        // A program has 1 to 99 keygroups. [RQ-MCP-035]
        constexpr int MIN_KEYGROUPS_ADDED = 1;
        constexpr int MAX_KEYGROUPS_IN_PROGRAM = 99;
        constexpr std::uint32_t MAX_DEVICE_ID = 31;

        constexpr std::span<const std::int64_t> NO_VALUES{};

        std::string portList(const std::vector<std::string>& names)
        {
            if (names.empty())
                return "none";
            std::ostringstream out;
            for (std::size_t i = 0; i < names.size(); ++i)
                out << (i == 0 ? "" : ", ") << '"' << names[i] << '"';
            return out.str();
        }

        std::string describeOpening(const akm::OpenResult& opened, const GatewayConfig& config)
        {
            switch (opened.status)
            {
                case akm::OpenStatus::NoSamplerAtTarget:
                {
                    std::string text = "No sampler answered at DeviceID " + numberText(config.deviceId) + ".";
                    if (opened.responders.empty())
                        return text + " Nothing answered on the MIDI ports: check that the sampler is on and that the input \"" +
                               config.inputPort + "\" and the output \"" + config.outputPort + "\" are the right ones.";
                    text += " Samplers answered at DeviceID";
                    for (const std::uint8_t responder : opened.responders)
                        text += " " + numberText(responder);
                    return text + ": set the DeviceID of the server to one of them.";
                }
                case akm::OpenStatus::AmbiguousSamplers:
                    return "Several samplers answered; set the DeviceID of the server to the one to use.";
                case akm::OpenStatus::InvalidDeviceId:
                    return "DeviceID " + numberText(config.deviceId) + " is not valid: a sampler's DeviceID is 0 to " +
                           numberText(MAX_DEVICE_ID) + ".";
                case akm::OpenStatus::DiscoveryFailed:
                    return "The sampler did not answer the discovery within " + numberText(config.commandTimeout.count()) +
                           " ms. Check that it is on and connected.";
                case akm::OpenStatus::SettingFailed:
                    return "A setting of the MIDI link could not be established (" +
                           std::string(opened.failedSetting ? akm::describe(*opened.failedSetting) : "unknown") +
                           "): the sampler did not accept it or did not answer.";
                case akm::OpenStatus::Cancelled:
                    return "The connection was closed while it was opening.";
                case akm::OpenStatus::Ready:
                case akm::OpenStatus::ReadyDegraded:
                case akm::OpenStatus::AlreadyOpen:
                    break;
            }
            return std::string("The connection to the sampler failed: ") + std::string(akm::describe(opened.status)) + ".";
        }
    }

    // The ports, the executor and the scheduler outlive the session, which is destroyed first (members are destroyed
    // in reverse order): the scheduler's thread posts to the executor, so the executor comes first. [ADR-AKM-001
    // (DEC-AKM-004)]
    struct SamplerGateway::Connection
    {
        Connection(akm::SessionTiming timing, std::unique_ptr<common::midi::MidiInputPort> inputPort,
                   std::unique_ptr<common::midi::MidiOutputPort> outputPort)
            : input(std::move(inputPort)), output(std::move(outputPort)), session(timing, executor, scheduler, *input, *output, diagnostics)
        {
        }

        std::unique_ptr<common::midi::MidiInputPort> input;
        std::unique_ptr<common::midi::MidiOutputPort> output;
        akm::ThreadExecutor executor;
        akm::RealScheduler scheduler;
        akm::NullDiagnosticSink diagnostics;
        akm::Session session;
    };

    akm::Session& SamplerGateway::session()
    {
        return _connection->session;
    }

    SamplerGateway::SamplerGateway(common::midi::MidiBackend& backend, GatewayConfig config)
        : _config(std::move(config)), _backend(backend)
    {
    }

    SamplerGateway::~SamplerGateway()
    {
        close();
    }

    std::optional<std::string> SamplerGateway::connect()
    {
        if (_connection)
            return std::nullopt;

        auto input = _backend.openInput(_config.inputPort);
        if (!input)
            return "The MIDI input port \"" + _config.inputPort + "\" was not found. Available input ports: " +
                   portList(_backend.inputDeviceNames()) + ".";
        auto output = _backend.openOutput(_config.outputPort);
        if (!output)
            return "The MIDI output port \"" + _config.outputPort + "\" was not found. Available output ports: " +
                   portList(_backend.outputDeviceNames()) + ".";

        akm::SessionTiming timing;
        timing.commandTimeout = _config.commandTimeout;
        auto connection = std::make_unique<Connection>(timing, std::move(input), std::move(output));

        akm::SessionConfig sessionConfig;
        sessionConfig.targetDeviceId = _config.deviceId;
        if (_config.touchLcdSettings)
            sessionConfig.autoScreenUpdate = akm::SettingChoice::On;
        else
            sessionConfig.syncLcd = akm::SettingChoice::Unchanged;

        const auto deadline = akm::DEFAULT_DISCOVERY_WINDOW + _config.commandTimeout * OPENING_COMMAND_BUDGET + WAIT_MARGIN;
        const auto opened = await<akm::OpenResult>(
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline),
            [&](std::function<void(const akm::OpenResult&)> done) { connection->session.open(sessionConfig, std::move(done)); });

        std::optional<std::string> problem;
        if (!opened)
            problem = "The connection to the sampler did not finish opening in time.";
        else if (!opened->ready())
            problem = describeOpening(*opened, _config);

        if (problem)
        {
            // Whatever the opening changed on the sampler is put back before the session is let go.
            _connection = std::move(connection);
            disconnect();
            return problem;
        }
        _connection = std::move(connection);
        return std::nullopt;
    }

    std::optional<akm::CloseResult> SamplerGateway::disconnect()
    {
        if (!_connection)
            return std::nullopt;
        const auto deadline = _config.commandTimeout * CLOSING_COMMAND_BUDGET + WAIT_MARGIN;
        const auto closed = await<akm::CloseResult>(deadline, [&](std::function<void(const akm::CloseResult&)> done) {
            const auto refusedCompletion = done;
            // A close that is refused (already closing) calls nothing: the wait must not run to its deadline.
            if (!_connection->session.close(std::move(done)))
                refusedCompletion(akm::CloseResult{});
        });
        _connection.reset();
        return closed;
    }

    std::optional<akm::CloseResult> SamplerGateway::close()
    {
        return disconnect();
    }

    std::chrono::milliseconds SamplerGateway::waitFor(int commands) const
    {
        return _config.commandTimeout * std::max(commands, 1) + WAIT_MARGIN;
    }

    Outcome<std::vector<std::vector<std::uint8_t>>> SamplerGateway::runSequence(std::vector<akm::CommandRequest> requests,
                                                                               const std::vector<std::string>& steps,
                                                                               bool needsCurrentProgram, const char* currentObject)
    {
        using Replies = std::vector<std::vector<std::uint8_t>>;
        if (const auto problem = connect())
            return Outcome<Replies>::failure(*problem);

        const int count = static_cast<int>(requests.size());
        const auto result = await<akm::SequenceResult>(waitFor(count), [&](std::function<void(const akm::SequenceResult&)> done) {
            _connection->session.submitSequence(std::move(requests), std::move(done));
        });
        if (!result)
            return Outcome<Replies>::failure("The sampler session did not complete the commands in time.");
        if (!result->allSucceeded())
        {
            const std::size_t failed = *result->failureIndex;
            return Outcome<Replies>::failure(explain(result->results[failed], steps[std::min(failed, steps.size() - 1)], _config,
                                                     needsCurrentProgram, currentObject));
        }

        Replies replies;
        for (const akm::CommandResult& outcome : result->results)
        {
            const auto* reply = std::get_if<akm::Reply>(&outcome);
            replies.push_back(reply != nullptr ? reply->data : std::vector<std::uint8_t>{});
        }
        return Outcome<Replies>::success(std::move(replies));
    }

    Outcome<int> SamplerGateway::keygroupCount()
    {
        const auto replies = runSequence({akm::makeRequest(akm::ItemId::ProgramGetKeygroupCount, NO_VALUES)},
                                         {"counting the keygroups of the current program"}, true);
        if (!replies.ok())
            return Outcome<int>::failure(replies.problem);
        const auto values = akm::decodeReply(akm::ItemId::ProgramGetKeygroupCount, replies.value->front());
        if (!values || values->size() != 1)
            return Outcome<int>::failure("The sampler's answer about the keygroups of the current program was not understood.");
        const int count = static_cast<int>(values->front());
        if (count < 1)
            return Outcome<int>::failure("The current program has no keygroup.");
        return Outcome<int>::success(count);
    }

    Outcome<std::string> SamplerGateway::currentProgramName()
    {
        const auto name = await<akm::ProgramNameResult>(waitFor(1), [&](std::function<void(const akm::ProgramNameResult&)> done) {
            akm::getCurrentProgramName(_connection->session, std::move(done));
        });
        if (!name)
            return Outcome<std::string>::failure("The sampler session did not complete the command in time.");
        if (!name->name)
            return Outcome<std::string>::failure(explain(name->outcome, "reading the name of the current program", _config, true));
        return Outcome<std::string>::success(*name->name);
    }

    Outcome<ProgramInfo> SamplerGateway::currentProgramInfo()
    {
        const auto name = await<akm::ProgramNameResult>(waitFor(1), [&](std::function<void(const akm::ProgramNameResult&)> done) {
            akm::getCurrentProgramName(_connection->session, std::move(done));
        });
        if (!name)
            return Outcome<ProgramInfo>::failure("The sampler session did not complete the command in time.");
        if (!name->name)
            return Outcome<ProgramInfo>::failure(explain(name->outcome, "reading the name of the current program", _config, true));
        const auto groups = keygroupCount();
        if (!groups.ok())
            return Outcome<ProgramInfo>::failure(groups.problem);
        return Outcome<ProgramInfo>::success(ProgramInfo{*name->name, *groups.value});
    }

    Outcome<SamplerStatus> SamplerGateway::status()
    {
        if (const auto problem = connect())
            return Outcome<SamplerStatus>::failure(*problem);

        const auto count = await<akm::ProgramCountResult>(waitFor(1), [&](std::function<void(const akm::ProgramCountResult&)> done) {
            akm::getProgramCount(_connection->session, std::move(done));
        });
        if (!count)
            return Outcome<SamplerStatus>::failure("The sampler session did not complete the command in time.");
        if (!count->count)
            return Outcome<SamplerStatus>::failure(explain(count->outcome, "counting the programs", _config, false));

        SamplerStatus status;
        status.deviceId = _config.deviceId;
        status.programCount = *count->count;
        if (status.programCount == 0)
            return Outcome<SamplerStatus>::success(std::move(status));

        const auto name = await<akm::ProgramNameResult>(waitFor(1), [&](std::function<void(const akm::ProgramNameResult&)> done) {
            akm::getCurrentProgramName(_connection->session, std::move(done));
        });
        if (!name)
            return Outcome<SamplerStatus>::failure("The sampler session did not complete the command in time.");
        const auto* error = std::get_if<akm::Error>(&name->outcome);
        const bool noCurrentProgram = error != nullptr && error->number == akm::error_number::NOT_FOUND;
        if (name->name)
        {
            status.currentProgram = *name->name;
            const auto groups = keygroupCount();
            if (!groups.ok())
                return Outcome<SamplerStatus>::failure(groups.problem);
            status.keygroupCount = *groups.value;
        }
        else if (!noCurrentProgram)
            return Outcome<SamplerStatus>::failure(explain(name->outcome, "reading the name of the current program", _config, false));
        return Outcome<SamplerStatus>::success(std::move(status));
    }

    Outcome<std::vector<ProgramEntry>> SamplerGateway::listPrograms()
    {
        using Entries = std::vector<ProgramEntry>;
        if (const auto problem = connect())
            return Outcome<Entries>::failure(*problem);

        const auto names = await<akm::AllProgramNamesResult>(waitFor(1), [&](std::function<void(const akm::AllProgramNamesResult&)> done) {
            akm::getAllProgramNames(_connection->session, std::move(done));
        });
        if (!names)
            return Outcome<Entries>::failure("The sampler session did not complete the command in time.");
        if (!names->names)
            return Outcome<Entries>::failure(explain(names->outcome, "reading the program names", _config, false));

        Entries entries;
        for (std::size_t i = 0; i < names->names->size(); ++i)
            entries.push_back(ProgramEntry{static_cast<int>(i), (*names->names)[i]});
        return Outcome<Entries>::success(std::move(entries));
    }

    Outcome<ProgramInfo> SamplerGateway::selectProgramByName(std::string_view name)
    {
        if (const auto problem = connect())
            return Outcome<ProgramInfo>::failure(*problem);

        const auto selected = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::selectProgramByName(_connection->session, name, std::move(done));
        });
        if (!selected)
            return Outcome<ProgramInfo>::failure("The sampler session did not complete the command in time.");
        const auto* error = std::get_if<akm::Error>(&*selected);
        if (error != nullptr && error->number == akm::error_number::NOT_FOUND)
            return Outcome<ProgramInfo>::failure("No program is named \"" + std::string(name) +
                                                 "\". Use list_programs to see the names.");
        if (!akm::succeeded(*selected))
            return Outcome<ProgramInfo>::failure(explain(*selected, "selecting the program \"" + std::string(name) + "\"", _config, false));
        return currentProgramInfo();
    }

    Outcome<ProgramInfo> SamplerGateway::selectProgramByIndex(int index)
    {
        if (const auto problem = connect())
            return Outcome<ProgramInfo>::failure(*problem);

        const auto selected = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::selectProgramByIndex(_connection->session, index, std::move(done));
        });
        if (!selected)
            return Outcome<ProgramInfo>::failure("The sampler session did not complete the command in time.");
        const auto* error = std::get_if<akm::Error>(&*selected);
        if (error != nullptr && error->number == akm::error_number::NOT_FOUND)
            return Outcome<ProgramInfo>::failure("No program is at index " + numberText(index) +
                                                 ". Use list_programs to see the positions.");
        if (!akm::succeeded(*selected))
            return Outcome<ProgramInfo>::failure(explain(*selected, "selecting the program at index " + numberText(index), _config, false));
        return currentProgramInfo();
    }

    Outcome<ProgramInfo> SamplerGateway::createProgram(std::string_view name, int keygroups)
    {
        if (const auto problem = connect())
            return Outcome<ProgramInfo>::failure(*problem);

        const auto created = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::createProgramWithKeygroups(_connection->session, keygroups, name, std::move(done));
        });
        if (!created)
            return Outcome<ProgramInfo>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*created))
            return Outcome<ProgramInfo>::failure(explain(*created, "creating the program \"" + std::string(name) + "\"", _config, false));
        return currentProgramInfo();
    }

    Outcome<ProgramRename> SamplerGateway::renameCurrentProgram(std::string_view name)
    {
        if (const auto problem = connect())
            return Outcome<ProgramRename>::failure(*problem);

        const auto before = currentProgramName();
        if (!before.ok())
            return Outcome<ProgramRename>::failure(before.problem);

        const auto renamed = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::renameCurrentProgram(_connection->session, name, std::move(done));
        });
        if (!renamed)
            return Outcome<ProgramRename>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*renamed))
            return Outcome<ProgramRename>::failure(explain(*renamed, "renaming the program \"" + *before.value + "\"", _config, true));

        const auto after = currentProgramName();
        if (!after.ok())
            return Outcome<ProgramRename>::failure(after.problem);
        return Outcome<ProgramRename>::success(ProgramRename{*before.value, *after.value});
    }

    Outcome<ProgramDeletion> SamplerGateway::deleteCurrentProgram(std::string_view confirm)
    {
        if (const auto problem = connect())
            return Outcome<ProgramDeletion>::failure(*problem);

        const auto current = currentProgramName();
        if (!current.ok())
            return Outcome<ProgramDeletion>::failure(current.problem);
        if (*current.value != confirm)
            return Outcome<ProgramDeletion>::success(ProgramDeletion{false, *current.value, std::nullopt});

        const auto deleted = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::deleteCurrentProgram(_connection->session, std::move(done));
        });
        if (!deleted)
            return Outcome<ProgramDeletion>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*deleted))
            return Outcome<ProgramDeletion>::failure(explain(*deleted, "deleting the program \"" + *current.value + "\"", _config, true));

        ProgramDeletion deletion{true, *current.value, std::nullopt};
        const auto count = await<akm::ProgramCountResult>(waitFor(1), [&](std::function<void(const akm::ProgramCountResult&)> done) {
            akm::getProgramCount(_connection->session, std::move(done));
        });
        if (count && count->count)
            deletion.remaining = *count->count;
        return Outcome<ProgramDeletion>::success(std::move(deletion));
    }

    namespace
    {
        akm::CommandRequest selectKeygroupRequest(std::int64_t keygroup)
        {
            return akm::makeRequest(akm::ItemId::KeygroupSelect, {keygroup});
        }

        // The arguments that come before the value: for a zone parameter the zone first (0 is all four), then the row's own.
        std::vector<std::int64_t> leadingArguments(const ParameterDefinition& parameter, ZoneSelection zones)
        {
            std::vector<std::int64_t> leading;
            if (parameter.scope == ParameterScope::Zone)
                leading.push_back(zones.zone.value_or(static_cast<int>(ALL_ZONES)));
            leading.insert(leading.end(), parameter.leadingArguments.begin(), parameter.leadingArguments.end());
            return leading;
        }

        // A Get that answers several records (one per keygroup while "all keygroups" is selected, one per zone for zone 0)
        // can only be delimited by a session that knows its checksum mode (DEC-AKM-014, RQ-AKM-031, RQ-AKM-036).
        akm::CommandRequest getRequest(const ParameterDefinition& parameter, ZoneSelection zones, bool answersSeveralRecords)
        {
            akm::CommandOptions options;
            if (answersSeveralRecords)
                options.expectedReply = akm::ExpectedReply::NeedsKnownChecksumMode;
            return akm::makeRequest(parameter.getItem, leadingArguments(parameter, zones), options);
        }

        akm::CommandRequest setRequest(const ParameterDefinition& parameter, std::int64_t value, ZoneSelection zones)
        {
            std::vector<std::int64_t> values = leadingArguments(parameter, zones);
            const std::vector<std::int64_t> wire = toItemValues(parameter, value);
            values.insert(values.end(), wire.begin(), wire.end());
            return akm::makeRequest(parameter.setItem, values);
        }

        std::string unreadable(const ParameterDefinition& parameter)
        {
            return "The sampler's answer for " + parameter.name + " could not be read.";
        }
    }

    Outcome<SystemInfo> SamplerGateway::readSystemInfo()
    {
        if (const auto problem = connect())
            return Outcome<SystemInfo>::failure(*problem);
        SystemInfo info;

        const auto model = await<akm::SamplerModelResult>(waitFor(1), [&](std::function<void(const akm::SamplerModelResult&)> done) {
            akm::getSamplerModel(_connection->session, std::move(done));
        });
        if (!model)
            return Outcome<SystemInfo>::failure("The sampler session did not complete the command in time.");
        if (model->model)
            info.model = *model->model == akm::SamplerModel::S5000 ? "AKAI S5000" : "AKAI S6000";
        else if (!akm::succeeded(model->outcome))
            return Outcome<SystemInfo>::failure(explain(model->outcome, "reading the sampler's model", _config, false));

        const auto version = await<akm::OsVersionResult>(waitFor(2), [&](std::function<void(const akm::OsVersionResult&)> done) {
            akm::queryOsVersion(_connection->session, std::move(done));
        });
        if (!version)
            return Outcome<SystemInfo>::failure("The sampler session did not complete the command in time.");
        if (version->version)
            info.osVersion = numberText(version->version->major) + "." + numberText(version->version->minor);

        const auto percentOf = [&](void (*get)(akm::Session&, akm::MemoryPercentCompletion), const char* what) -> Outcome<int> {
            const auto read = await<akm::MemoryPercentResult>(waitFor(1), [&](std::function<void(const akm::MemoryPercentResult&)> done) {
                get(_connection->session, std::move(done));
            });
            if (!read)
                return Outcome<int>::failure("The sampler session did not complete the command in time.");
            if (!read->percent)
                return Outcome<int>::failure(explain(read->outcome, what, _config, false));
            return Outcome<int>::success(*read->percent);
        };
        const auto bytesOf = [&](void (*get)(akm::Session&, akm::MemoryBytesCompletion), const char* what) -> Outcome<std::uint32_t> {
            const auto read = await<akm::MemoryBytesResult>(waitFor(1), [&](std::function<void(const akm::MemoryBytesResult&)> done) {
                get(_connection->session, std::move(done));
            });
            if (!read)
                return Outcome<std::uint32_t>::failure("The sampler session did not complete the command in time.");
            if (!read->bytes)
                return Outcome<std::uint32_t>::failure(explain(read->outcome, what, _config, false));
            return Outcome<std::uint32_t>::success(*read->bytes);
        };

        const auto freeWavePercent = percentOf(akm::getFreeWaveMemoryPercent, "reading the free wave memory");
        if (!freeWavePercent.ok())
            return Outcome<SystemInfo>::failure(freeWavePercent.problem);
        const auto freeWaveBytes = bytesOf(akm::getFreeWaveMemoryBytes, "reading the free wave memory in bytes");
        if (!freeWaveBytes.ok())
            return Outcome<SystemInfo>::failure(freeWaveBytes.problem);
        const auto totalWaveBytes = bytesOf(akm::getTotalWaveMemoryBytes, "reading the total wave memory");
        if (!totalWaveBytes.ok())
            return Outcome<SystemInfo>::failure(totalWaveBytes.problem);
        const auto freeMpksPercent = percentOf(akm::getFreeMpksMemoryPercent, "reading the free program and sample memory");
        if (!freeMpksPercent.ok())
            return Outcome<SystemInfo>::failure(freeMpksPercent.problem);
        info.freeWavePercent = *freeWavePercent.value;
        info.freeWaveBytes = *freeWaveBytes.value;
        info.totalWaveBytes = *totalWaveBytes.value;
        info.freeMpksPercent = *freeMpksPercent.value;
        return Outcome<SystemInfo>::success(std::move(info));
    }

    Outcome<ZoneSamples> SamplerGateway::readZoneSamples(KeygroupSelection selection)
    {
        const auto count = keygroupCount();
        if (!count.ok())
            return Outcome<ZoneSamples>::failure(count.problem);
        if (selection.keygroup && (*selection.keygroup < 1 || *selection.keygroup > *count.value))
            return Outcome<ZoneSamples>::failure("The current program has " + numberText(*count.value) + " keygroup" +
                                                 (*count.value == 1 ? "" : "s") + "; keygroup " + numberText(*selection.keygroup) +
                                                 " does not exist.");
        const auto program = currentProgramName();
        if (!program.ok())
            return Outcome<ZoneSamples>::failure(program.problem);

        ZoneSamples result;
        result.program = *program.value;
        const int first = selection.keygroup.value_or(1);
        const int last = selection.keygroup.value_or(*count.value);
        for (int keygroup = first; keygroup <= last; ++keygroup)
        {
            const auto selected = runSequence({selectKeygroupRequest(keygroup)}, {"selecting keygroup " + numberText(keygroup)}, true);
            if (!selected.ok())
                return Outcome<ZoneSamples>::failure(selected.problem);
            for (int zone = 1; zone <= ZONES_PER_KEYGROUP; ++zone)
            {
                const auto read = await<akm::ZoneSampleResult>(waitFor(1), [&](std::function<void(const akm::ZoneSampleResult&)> done) {
                    akm::getZoneSample(_connection->session, zone, std::move(done));
                });
                if (!read)
                    return Outcome<ZoneSamples>::failure("The sampler session did not complete the command in time.");
                if (!read->name)
                    return Outcome<ZoneSamples>::failure(explain(read->outcome, "reading the sample of zone " + numberText(zone) +
                                                                                    " of keygroup " + numberText(keygroup),
                                                                 _config, true));
                result.entries.push_back(ZoneSampleEntry{keygroup, zone, *read->name});
            }
        }
        return Outcome<ZoneSamples>::success(std::move(result));
    }

    Outcome<ZoneSamples> SamplerGateway::assignZoneSample(int keygroup, int zone, std::string_view sample)
    {
        if (zone < 1 || zone > ZONES_PER_KEYGROUP)
            return Outcome<ZoneSamples>::failure("Zone " + numberText(zone) + " does not exist: a keygroup has zones 1 to " +
                                                 numberText(ZONES_PER_KEYGROUP) + ".");
        const auto count = keygroupCount();
        if (!count.ok())
            return Outcome<ZoneSamples>::failure(count.problem);
        if (keygroup < 1 || keygroup > *count.value)
            return Outcome<ZoneSamples>::failure("The current program has " + numberText(*count.value) + " keygroup" +
                                                 (*count.value == 1 ? "" : "s") + "; keygroup " + numberText(keygroup) +
                                                 " does not exist.");
        const auto program = currentProgramName();
        if (!program.ok())
            return Outcome<ZoneSamples>::failure(program.problem);

        const auto samples = listSamples();
        if (!samples.ok())
            return Outcome<ZoneSamples>::failure(samples.problem);
        const std::string wanted = normalizeText(sample);
        const auto found = std::find_if(samples.value->samples.begin(), samples.value->samples.end(),
                                        [&wanted](const SampleEntry& entry) { return normalizeText(entry.name) == wanted; });
        if (found == samples.value->samples.end())
        {
            std::string text = "No sample is named \"" + std::string(sample) + "\" in the sampler's memory.";
            if (samples.value->samples.empty())
                return Outcome<ZoneSamples>::failure(text + " The memory holds none: load one first.");
            text += " It holds: ";
            for (std::size_t i = 0; i < samples.value->samples.size(); ++i)
                text += (i == 0 ? "" : ", ") + std::string("\"") + samples.value->samples[i].name + "\"";
            return Outcome<ZoneSamples>::failure(text + ".");
        }
        const std::string listed = found->name;

        const auto selected = runSequence({selectKeygroupRequest(keygroup)}, {"selecting keygroup " + numberText(keygroup)}, true);
        if (!selected.ok())
            return Outcome<ZoneSamples>::failure(selected.problem);
        const auto assigned = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::setZoneSample(_connection->session, zone, listed, std::move(done));
        });
        if (!assigned)
            return Outcome<ZoneSamples>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*assigned))
            return Outcome<ZoneSamples>::failure(explain(*assigned, "assigning the sample \"" + listed + "\" to zone " + numberText(zone) +
                                                                        " of keygroup " + numberText(keygroup),
                                                         _config, true));
        const auto read = await<akm::ZoneSampleResult>(waitFor(1), [&](std::function<void(const akm::ZoneSampleResult&)> done) {
            akm::getZoneSample(_connection->session, zone, std::move(done));
        });
        if (!read)
            return Outcome<ZoneSamples>::failure("The sampler session did not complete the command in time.");
        if (!read->name)
            return Outcome<ZoneSamples>::failure(explain(read->outcome, "reading back the sample of zone " + numberText(zone), _config, true));
        if (*read->name != listed)
            return Outcome<ZoneSamples>::failure("The sampler accepted the assignment but reports that zone " + numberText(zone) + " of keygroup " +
                                                 numberText(keygroup) + " plays " +
                                                 (read->name->empty() ? std::string("no sample") : "\"" + *read->name + "\"") + ", not \"" + listed +
                                                 "\".");
        ZoneSamples result;
        result.program = *program.value;
        result.entries.push_back(ZoneSampleEntry{keygroup, zone, *read->name});
        return Outcome<ZoneSamples>::success(std::move(result));
    }

    Outcome<KeygroupChange> SamplerGateway::addKeygroups(int count)
    {
        const auto before = keygroupCount();
        if (!before.ok())
            return Outcome<KeygroupChange>::failure(before.problem);
        if (count < MIN_KEYGROUPS_ADDED || *before.value + count > MAX_KEYGROUPS_IN_PROGRAM)
            return Outcome<KeygroupChange>::failure("A program has at most " + numberText(MAX_KEYGROUPS_IN_PROGRAM) + " keygroups and the current one has " +
                                                    numberText(*before.value) + ": " + numberText(MAX_KEYGROUPS_IN_PROGRAM - *before.value) +
                                                    " can still be added, not " + numberText(count) + ".");
        const auto program = currentProgramName();
        if (!program.ok())
            return Outcome<KeygroupChange>::failure(program.problem);

        const auto added = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::addKeygroupsToProgram(_connection->session, count, std::move(done));
        });
        if (!added)
            return Outcome<KeygroupChange>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*added))
            return Outcome<KeygroupChange>::failure(explain(*added, "adding " + numberText(count) + " keygroups to the program \"" + *program.value + "\"", _config, true));

        const auto after = keygroupCount();
        if (!after.ok())
            return Outcome<KeygroupChange>::failure(after.problem);
        return Outcome<KeygroupChange>::success(KeygroupChange{true, *program.value, *after.value});
    }

    Outcome<KeygroupChange> SamplerGateway::deleteKeygroup(int keygroup, std::string_view confirm)
    {
        const auto count = keygroupCount();
        if (!count.ok())
            return Outcome<KeygroupChange>::failure(count.problem);
        const auto program = currentProgramName();
        if (!program.ok())
            return Outcome<KeygroupChange>::failure(program.problem);
        if (keygroup < 1 || keygroup > *count.value)
            return Outcome<KeygroupChange>::failure("The current program \"" + *program.value + "\" has " + numberText(*count.value) + " keygroup" +
                                                    (*count.value == 1 ? "" : "s") + "; keygroup " + numberText(keygroup) + " does not exist.");
        if (*count.value == 1)
            return Outcome<KeygroupChange>::failure("Keygroup 1 is the last keygroup of the program \"" + *program.value +
                                                    "\": a program keeps at least one, so nothing was deleted. Delete the program (delete_program) instead.");
        if (*program.value != confirm)
            return Outcome<KeygroupChange>::success(KeygroupChange{false, *program.value, *count.value});

        const auto deleted = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::deleteKeygroupFromProgram(_connection->session, keygroup - 1, std::move(done));
        });
        if (!deleted)
            return Outcome<KeygroupChange>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*deleted))
            return Outcome<KeygroupChange>::failure(explain(*deleted, "deleting keygroup " + numberText(keygroup) + " of the program \"" + *program.value + "\"", _config, true));

        const auto after = keygroupCount();
        if (!after.ok())
            return Outcome<KeygroupChange>::failure(after.problem);
        return Outcome<KeygroupChange>::success(KeygroupChange{true, *program.value, *after.value});
    }

    Outcome<std::vector<ParameterValue>> SamplerGateway::editParameter(const ParameterDefinition& parameter,
                                                                       std::optional<std::int64_t> valueToSet,
                                                                       KeygroupSelection selection, ZoneSelection zones)
    {
        using Values = std::vector<ParameterValue>;
        const bool onZones = parameter.scope == ParameterScope::Zone;
        const bool onKeygroups = parameter.scope == ParameterScope::Keygroup || onZones;
        int keygroups = 0;

        if (onZones && zones.zone && (*zones.zone < 1 || *zones.zone > ZONES_PER_KEYGROUP))
            return Outcome<Values>::failure("Zone " + numberText(*zones.zone) + " does not exist: a keygroup has zones 1 to " +
                                            numberText(ZONES_PER_KEYGROUP) + " (or all of them).");

        if (onKeygroups)
        {
            const auto count = keygroupCount();
            if (!count.ok())
                return Outcome<Values>::failure(count.problem);
            keygroups = *count.value;
            if (selection.keygroup && (*selection.keygroup < 1 || *selection.keygroup > keygroups))
                return Outcome<Values>::failure("The current program has " + numberText(keygroups) + " keygroup" +
                                                (keygroups == 1 ? "" : "s") + "; keygroup " + numberText(*selection.keygroup) +
                                                " does not exist.");
        }

        std::vector<akm::CommandRequest> requests;
        std::vector<std::string> steps;
        if (onKeygroups)
        {
            const std::string which = selection.keygroup ? "keygroup " + numberText(*selection.keygroup) : "all keygroups";
            requests.push_back(selectKeygroupRequest(selection.keygroup.value_or(ALL_KEYGROUPS)));
            steps.push_back("selecting " + which);
        }
        if (valueToSet)
        {
            requests.push_back(setRequest(parameter, *valueToSet, zones));
            steps.push_back("setting " + parameter.name);
        }
        // How many records the Get answers: one per keygroup while "all keygroups" is selected, times one per zone for
        // zone 0 (keygroup-major, zone-minor, RQ-MCP-019).
        const int keygroupRecords = onKeygroups && !selection.keygroup ? keygroups : 1;
        const int zoneRecords = onZones && !zones.zone ? ZONES_PER_KEYGROUP : 1;
        const int records = keygroupRecords * zoneRecords;
        requests.push_back(getRequest(parameter, zones, records > 1));
        steps.push_back("reading " + parameter.name);

        const auto replies = runSequence(std::move(requests), steps, true);
        if (!replies.ok())
            return Outcome<Values>::failure(replies.problem);

        Values values;
        const std::vector<std::uint8_t>& answer = replies.value->back();
        if (records > 1)
        {
            const auto sets = akm::decodeRepeatedReply(parameter.getItem, answer);
            if (!sets || static_cast<int>(sets->size()) != records)
                return Outcome<Values>::failure(unreadable(parameter));
            for (std::size_t i = 0; i < sets->size(); ++i)
            {
                const auto value = fromItemValues(parameter, (*sets)[i]);
                if (!value)
                    return Outcome<Values>::failure(unreadable(parameter));
                ParameterValue read;
                read.value = *value;
                if (onKeygroups)
                    read.keygroup = selection.keygroup ? selection.keygroup : std::optional<int>(static_cast<int>(i) / zoneRecords + 1);
                if (onZones)
                    read.zone = zones.zone ? zones.zone : std::optional<int>(static_cast<int>(i) % zoneRecords + 1);
                values.push_back(read);
            }
        }
        else
        {
            const auto set = akm::decodeReply(parameter.getItem, answer);
            const auto value = set ? fromItemValues(parameter, *set) : std::nullopt;
            if (!value)
                return Outcome<Values>::failure(unreadable(parameter));
            ParameterValue read;
            read.value = *value;
            if (onKeygroups)
                read.keygroup = selection.keygroup;
            if (onZones)
                read.zone = zones.zone;
            values.push_back(read);
        }

        if (valueToSet)
        {
            for (const ParameterValue& read : values)
            {
                if (read.value != *valueToSet)
                    return Outcome<Values>::failure("The sampler did not keep the value: " + parameter.name + " was set to " +
                                                    describeValue(parameter, *valueToSet) + " but reads " +
                                                    describeValue(parameter, read.value) +
                                                    (read.keygroup ? " for keygroup " + numberText(*read.keygroup) : "") +
                                                    (read.zone ? " zone " + numberText(*read.zone) : "") + ".");
            }
        }
        return Outcome<Values>::success(std::move(values));
    }

    Outcome<std::vector<ParameterValue>> SamplerGateway::readParameter(const ParameterDefinition& parameter,
                                                                       KeygroupSelection selection, ZoneSelection zones)
    {
        return editParameter(parameter, std::nullopt, selection, zones);
    }

    Outcome<std::vector<ParameterValue>> SamplerGateway::writeParameter(const ParameterDefinition& parameter, std::int64_t value,
                                                                        KeygroupSelection selection, ZoneSelection zones)
    {
        return editParameter(parameter, value, selection, zones);
    }

    // ---- Samples (section 0E): the sampler's own "current sample", like the current program. [RQ-MCP-020] ----

    Outcome<SampleEntry> SamplerGateway::currentSampleEntry()
    {
        const auto index = await<akm::SampleIndexResult>(waitFor(1), [&](std::function<void(const akm::SampleIndexResult&)> done) {
            akm::getCurrentSampleIndex(_connection->session, std::move(done));
        });
        if (!index)
            return Outcome<SampleEntry>::failure("The sampler session did not complete the command in time.");
        if (!index->index)
            return Outcome<SampleEntry>::failure(explain(index->outcome, "reading the position of the current sample", _config, true, "sample"));
        const auto name = await<akm::SampleNameResult>(waitFor(1), [&](std::function<void(const akm::SampleNameResult&)> done) {
            akm::getCurrentSampleName(_connection->session, std::move(done));
        });
        if (!name)
            return Outcome<SampleEntry>::failure("The sampler session did not complete the command in time.");
        if (!name->name)
            return Outcome<SampleEntry>::failure(explain(name->outcome, "reading the name of the current sample", _config, true, "sample"));
        return Outcome<SampleEntry>::success(SampleEntry{*index->index, *name->name});
    }

    Outcome<SampleListing> SamplerGateway::listSamples()
    {
        if (const auto problem = connect())
            return Outcome<SampleListing>::failure(*problem);

        // The count first: with no sample in memory the Get of all the names answers ERROR 3 (seen on a real S5000, not an empty
        // list), so it is only asked when there is one to name. [OBSERVATIONS-RQ-MCP-012-real-sampler.md]
        const auto count = await<akm::SampleCountResult>(waitFor(1), [&](std::function<void(const akm::SampleCountResult&)> done) {
            akm::getSampleCount(_connection->session, std::move(done));
        });
        if (!count)
            return Outcome<SampleListing>::failure("The sampler session did not complete the command in time.");
        if (!count->count)
            return Outcome<SampleListing>::failure(explain(count->outcome, "counting the samples", _config, false));
        SampleListing listing;
        if (*count->count == 0)
            return Outcome<SampleListing>::success(std::move(listing));

        const auto names = await<akm::AllSampleNamesResult>(waitFor(1), [&](std::function<void(const akm::AllSampleNamesResult&)> done) {
            akm::getAllSampleNames(_connection->session, std::move(done));
        });
        if (!names)
            return Outcome<SampleListing>::failure("The sampler session did not complete the command in time.");
        if (!names->names)
            return Outcome<SampleListing>::failure(explain(names->outcome, "reading the sample names", _config, false));
        for (std::size_t i = 0; i < names->names->size(); ++i)
            listing.samples.push_back(SampleEntry{static_cast<int>(i), (*names->names)[i]});
        if (listing.samples.empty())
            return Outcome<SampleListing>::success(std::move(listing));

        const auto index = await<akm::SampleIndexResult>(waitFor(1), [&](std::function<void(const akm::SampleIndexResult&)> done) {
            akm::getCurrentSampleIndex(_connection->session, std::move(done));
        });
        if (!index)
            return Outcome<SampleListing>::failure("The sampler session did not complete the command in time.");
        if (index->index)
            listing.current = *index->index;
        else
        {
            const auto* error = std::get_if<akm::Error>(&index->outcome);
            if (error == nullptr || error->number != akm::error_number::NOT_FOUND)
                return Outcome<SampleListing>::failure(explain(index->outcome, "reading the current sample", _config, false));
        }
        return Outcome<SampleListing>::success(std::move(listing));
    }

    Outcome<RenamedItem> SamplerGateway::renameCurrentSample(std::string_view name)
    {
        const auto listing = listSamples();
        if (!listing.ok())
            return Outcome<RenamedItem>::failure(listing.problem);
        if (!listing.value->current)
            return Outcome<RenamedItem>::failure("No sample is current: use select_sample first.");
        const std::size_t current = static_cast<std::size_t>(*listing.value->current);
        const std::string before = listing.value->samples[current].name;
        const std::string wanted = normalizeText(name);
        for (std::size_t i = 0; i < listing.value->samples.size(); ++i)
        {
            if (i != current && normalizeText(listing.value->samples[i].name) == wanted)
                return Outcome<RenamedItem>::failure("The sampler already holds a sample named \"" + listing.value->samples[i].name +
                                                      "\": nothing was renamed. Choose another name.");
        }

        const auto renamed = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::renameCurrentSample(_connection->session, name, std::move(done));
        });
        if (!renamed)
            return Outcome<RenamedItem>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*renamed))
            return Outcome<RenamedItem>::failure(explain(*renamed, "renaming the sample \"" + before + "\"", _config, true, "sample"));

        const auto after = listSamples();
        if (!after.ok())
            return Outcome<RenamedItem>::failure(after.problem);
        if (!after.value->current)
            return Outcome<RenamedItem>::failure("The sampler accepted the new name but no sample is current afterwards.");
        return Outcome<RenamedItem>::success(RenamedItem{before, after.value->samples[static_cast<std::size_t>(*after.value->current)].name});
    }

    Outcome<DeletedItem> SamplerGateway::deleteCurrentSample(std::string_view confirm)
    {
        const auto listing = listSamples();
        if (!listing.ok())
            return Outcome<DeletedItem>::failure(listing.problem);
        if (!listing.value->current)
            return Outcome<DeletedItem>::failure("No sample is current: use select_sample first.");
        const std::string name = listing.value->samples[static_cast<std::size_t>(*listing.value->current)].name;
        if (name != confirm)
            return Outcome<DeletedItem>::success(DeletedItem{false, name, static_cast<int>(listing.value->samples.size())});

        const auto deleted = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::deleteCurrentSample(_connection->session, std::move(done));
        });
        if (!deleted)
            return Outcome<DeletedItem>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*deleted))
            return Outcome<DeletedItem>::failure(explain(*deleted, "deleting the sample \"" + name + "\"", _config, true, "sample"));

        const auto after = listSamples();
        if (!after.ok())
            return Outcome<DeletedItem>::failure(after.problem);
        return Outcome<DeletedItem>::success(DeletedItem{true, name, static_cast<int>(after.value->samples.size())});
    }

    Outcome<SampleEntry> SamplerGateway::selectSampleByName(std::string_view name)
    {
        if (const auto problem = connect())
            return Outcome<SampleEntry>::failure(*problem);

        const auto selected = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::selectSampleByName(_connection->session, name, std::move(done));
        });
        if (!selected)
            return Outcome<SampleEntry>::failure("The sampler session did not complete the command in time.");
        const auto* error = std::get_if<akm::Error>(&*selected);
        if (error != nullptr && error->number == akm::error_number::NOT_FOUND)
            return Outcome<SampleEntry>::failure("No sample is named \"" + std::string(name) + "\". Use list_samples to see the names.");
        if (!akm::succeeded(*selected))
            return Outcome<SampleEntry>::failure(explain(*selected, "selecting the sample \"" + std::string(name) + "\"", _config, false));
        return currentSampleEntry();
    }

    Outcome<SampleEntry> SamplerGateway::selectSampleByIndex(int index)
    {
        if (const auto problem = connect())
            return Outcome<SampleEntry>::failure(*problem);

        const auto selected = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::selectSampleByIndex(_connection->session, index, std::move(done));
        });
        if (!selected)
            return Outcome<SampleEntry>::failure("The sampler session did not complete the command in time.");
        const auto* error = std::get_if<akm::Error>(&*selected);
        if (error != nullptr && error->number == akm::error_number::NOT_FOUND)
            return Outcome<SampleEntry>::failure("No sample is at index " + numberText(index) + ". Use list_samples to see the positions.");
        if (!akm::succeeded(*selected))
            return Outcome<SampleEntry>::failure(explain(*selected, "selecting the sample at index " + numberText(index), _config, false));
        return currentSampleEntry();
    }

    Outcome<std::int64_t> SamplerGateway::readSampleParameter(const ParameterDefinition& parameter)
    {
        const auto replies = runSequence({getRequest(parameter, ZoneSelection::all(), false)}, {"reading " + parameter.name}, true, "sample");
        if (!replies.ok())
            return Outcome<std::int64_t>::failure(replies.problem);
        const auto set = akm::decodeReply(parameter.getItem, replies.value->front());
        const auto value = set ? fromItemValues(parameter, *set) : std::nullopt;
        if (!value)
            return Outcome<std::int64_t>::failure("The sampler's answer for " + parameter.name + " could not be read.");
        return Outcome<std::int64_t>::success(*value);
    }

    Outcome<std::int64_t> SamplerGateway::writeSampleParameter(const ParameterDefinition& parameter, std::int64_t value)
    {
        if (parameter.readOnly)
            return Outcome<std::int64_t>::failure(parameter.name + " is read-only: the sampler reports it and it cannot be set.");
        const auto replies = runSequence({setRequest(parameter, value, ZoneSelection::all()), getRequest(parameter, ZoneSelection::all(), false)},
                                         {"setting " + parameter.name, "reading " + parameter.name}, true, "sample");
        if (!replies.ok())
            return Outcome<std::int64_t>::failure(replies.problem);
        const auto set = akm::decodeReply(parameter.getItem, replies.value->back());
        const auto read = set ? fromItemValues(parameter, *set) : std::nullopt;
        if (!read)
            return Outcome<std::int64_t>::failure("The sampler's answer for " + parameter.name + " could not be read.");
        if (*read != value)
            return Outcome<std::int64_t>::failure("The sampler did not keep the value: " + parameter.name + " was set to " +
                                                  describeValue(parameter, value) + " but reads " + describeValue(parameter, *read) + ".");
        return Outcome<std::int64_t>::success(*read);
    }

    // ---- Multis (section 0C): the sampler's own "current multi", and the parts of it. [RQ-MCP-021] ----

    Outcome<MultiEntry> SamplerGateway::currentMultiEntry()
    {
        // The tools that act on the current multi may be the first call of a session. [RQ-MCP-037, RQ-MCP-038]
        if (const auto problem = connect())
            return Outcome<MultiEntry>::failure(*problem);
        const auto index = await<akm::MultiIndexResult>(waitFor(1), [&](std::function<void(const akm::MultiIndexResult&)> done) {
            akm::getCurrentMultiIndex(_connection->session, std::move(done));
        });
        if (!index)
            return Outcome<MultiEntry>::failure("The sampler session did not complete the command in time.");
        if (!index->index)
            return Outcome<MultiEntry>::failure(explain(index->outcome, "reading the position of the current multi", _config, true, "multi"));
        const auto name = await<akm::MultiNameResult>(waitFor(1), [&](std::function<void(const akm::MultiNameResult&)> done) {
            akm::getCurrentMultiName(_connection->session, std::move(done));
        });
        if (!name)
            return Outcome<MultiEntry>::failure("The sampler session did not complete the command in time.");
        if (!name->name)
            return Outcome<MultiEntry>::failure(explain(name->outcome, "reading the name of the current multi", _config, true, "multi"));
        const auto parts = await<akm::MultiPartCountResult>(waitFor(1), [&](std::function<void(const akm::MultiPartCountResult&)> done) {
            akm::getCurrentMultiPartCount(_connection->session, std::move(done));
        });
        if (!parts)
            return Outcome<MultiEntry>::failure("The sampler session did not complete the command in time.");
        if (!parts->partCount)
            return Outcome<MultiEntry>::failure(explain(parts->outcome, "counting the parts of the current multi", _config, true, "multi"));
        return Outcome<MultiEntry>::success(MultiEntry{*index->index, *name->name, *parts->partCount});
    }

    Outcome<MultiListing> SamplerGateway::listMultis()
    {
        if (const auto problem = connect())
            return Outcome<MultiListing>::failure(*problem);

        // The count first, as for the samples: an empty memory is then "no multi", whatever the Get of the names answers.
        const auto count = await<akm::MultiCountResult>(waitFor(1), [&](std::function<void(const akm::MultiCountResult&)> done) {
            akm::getMultiCount(_connection->session, std::move(done));
        });
        if (!count)
            return Outcome<MultiListing>::failure("The sampler session did not complete the command in time.");
        if (!count->count)
            return Outcome<MultiListing>::failure(explain(count->outcome, "counting the multis", _config, false));
        MultiListing listing;
        if (*count->count == 0)
            return Outcome<MultiListing>::success(std::move(listing));

        const auto names = await<akm::MultiNameListResult>(waitFor(1), [&](std::function<void(const akm::MultiNameListResult&)> done) {
            akm::getAllMultiNames(_connection->session, std::move(done));
        });
        if (!names)
            return Outcome<MultiListing>::failure("The sampler session did not complete the command in time.");
        if (!names->names)
            return Outcome<MultiListing>::failure(explain(names->outcome, "reading the multi names", _config, false));
        for (std::size_t i = 0; i < names->names->size(); ++i)
            listing.multis.push_back(MultiEntry{static_cast<int>(i), (*names->names)[i], 0});
        if (listing.multis.empty())
            return Outcome<MultiListing>::success(std::move(listing));

        const auto current = currentMultiEntry();
        if (current.ok())
        {
            listing.current = current.value->index;
            listing.currentPartCount = current.value->partCount;
        }
        return Outcome<MultiListing>::success(std::move(listing));
    }

    Outcome<MultiCreation> SamplerGateway::createMulti(std::string_view name)
    {
        const auto listing = listMultis();
        if (!listing.ok())
            return Outcome<MultiCreation>::failure(listing.problem);
        const std::string wanted = normalizeText(name);
        for (const MultiEntry& multi : listing.value->multis)
        {
            if (normalizeText(multi.name) == wanted)
                return Outcome<MultiCreation>::failure("The sampler already holds a multi named \"" + multi.name + "\": nothing was created. Choose another name.");
        }

        const auto created = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::createMulti(_connection->session, name, std::move(done));
        });
        if (!created)
            return Outcome<MultiCreation>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*created))
            return Outcome<MultiCreation>::failure(explain(*created, "creating the multi \"" + std::string(name) + "\"", _config, false));

        const auto current = currentMultiEntry();
        if (!current.ok())
            return Outcome<MultiCreation>::failure(current.problem);
        const auto after = listMultis();
        if (!after.ok())
            return Outcome<MultiCreation>::failure(after.problem);
        return Outcome<MultiCreation>::success(MultiCreation{current.value->name, current.value->partCount, static_cast<int>(after.value->multis.size())});
    }

    Outcome<RenamedItem> SamplerGateway::renameCurrentMulti(std::string_view name)
    {
        const auto current = currentMultiEntry();
        if (!current.ok())
            return Outcome<RenamedItem>::failure(current.problem);
        const auto listing = listMultis();
        if (!listing.ok())
            return Outcome<RenamedItem>::failure(listing.problem);
        const std::string wanted = normalizeText(name);
        for (const MultiEntry& multi : listing.value->multis)
        {
            if (multi.index != current.value->index && normalizeText(multi.name) == wanted)
                return Outcome<RenamedItem>::failure("The sampler already holds a multi named \"" + multi.name + "\": nothing was renamed. Choose another name.");
        }

        const auto renamed = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::renameCurrentMulti(_connection->session, name, std::move(done));
        });
        if (!renamed)
            return Outcome<RenamedItem>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*renamed))
            return Outcome<RenamedItem>::failure(explain(*renamed, "renaming the multi \"" + current.value->name + "\"", _config, true, "multi"));

        const auto after = currentMultiEntry();
        if (!after.ok())
            return Outcome<RenamedItem>::failure(after.problem);
        return Outcome<RenamedItem>::success(RenamedItem{current.value->name, after.value->name});
    }

    Outcome<DeletedItem> SamplerGateway::deleteCurrentMulti(std::string_view confirm)
    {
        const auto current = currentMultiEntry();
        if (!current.ok())
            return Outcome<DeletedItem>::failure(current.problem);
        if (current.value->name != confirm)
            return Outcome<DeletedItem>::success(DeletedItem{false, current.value->name, 0});

        const auto deleted = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::deleteCurrentMulti(_connection->session, std::move(done));
        });
        if (!deleted)
            return Outcome<DeletedItem>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*deleted))
            return Outcome<DeletedItem>::failure(explain(*deleted, "deleting the multi \"" + current.value->name + "\"", _config, true, "multi"));

        const auto after = listMultis();
        if (!after.ok())
            return Outcome<DeletedItem>::failure(after.problem);
        return Outcome<DeletedItem>::success(DeletedItem{true, current.value->name, static_cast<int>(after.value->multis.size())});
    }

    Outcome<PartPrograms> SamplerGateway::readPartPrograms()
    {
        const auto current = currentMultiEntry();
        if (!current.ok())
            return Outcome<PartPrograms>::failure(current.problem);
        const auto names = await<akm::MultiNameListResult>(waitFor(1), [&](std::function<void(const akm::MultiNameListResult&)> done) {
            akm::getAllMultiPartNames(_connection->session, std::move(done));
        });
        if (!names)
            return Outcome<PartPrograms>::failure("The sampler session did not complete the command in time.");
        if (!names->names)
            return Outcome<PartPrograms>::failure(explain(names->outcome, "reading what the parts of the multi play", _config, true, "multi"));
        PartPrograms result;
        result.multi = current.value->name;
        result.partCount = static_cast<int>(names->names->size());
        for (std::size_t i = 0; i < names->names->size(); ++i)
        {
            if (!(*names->names)[i].empty())
                result.assigned.push_back(PartProgram{static_cast<int>(i) + 1, (*names->names)[i]});
        }
        return Outcome<PartPrograms>::success(std::move(result));
    }

    namespace
    {
        // The message of a part that the current multi does not have. [RQ-MCP-038]
        std::string noSuchPart(const MultiEntry& multi, int part)
        {
            return "The multi \"" + multi.name + "\" has " + numberText(multi.partCount) + " parts, numbered 1 to " + numberText(multi.partCount) +
                   "; part " + numberText(part) + " does not exist.";
        }
    }

    Outcome<PartAssignment> SamplerGateway::assignPartProgram(int part, const ProgramReference& program)
    {
        const auto current = currentMultiEntry();
        if (!current.ok())
            return Outcome<PartAssignment>::failure(current.problem);
        if (part < 1 || part > current.value->partCount)
            return Outcome<PartAssignment>::failure(noSuchPart(*current.value, part));

        const auto programs = listPrograms();
        if (!programs.ok())
            return Outcome<PartAssignment>::failure(programs.problem);
        std::string listed;
        int position = 0;
        if (program.name)
        {
            const std::string wanted = normalizeText(*program.name);
            const auto found = std::find_if(programs.value->begin(), programs.value->end(),
                                            [&wanted](const ProgramEntry& entry) { return normalizeText(entry.name) == wanted; });
            if (found == programs.value->end())
            {
                std::string text = "No program is named \"" + *program.name + "\" in the sampler's memory.";
                if (programs.value->empty())
                    return Outcome<PartAssignment>::failure(text + " The memory holds none.");
                text += " It holds: ";
                for (std::size_t i = 0; i < programs.value->size(); ++i)
                    text += (i == 0 ? "" : ", ") + std::string("\"") + (*programs.value)[i].name + "\"";
                return Outcome<PartAssignment>::failure(text + ".");
            }
            listed = found->name;
        }
        else
        {
            position = program.position.value_or(-1);
            if (position < 0 || position >= static_cast<int>(programs.value->size()))
                return Outcome<PartAssignment>::failure("There is no program at position " + numberText(position) + ": the sampler holds " +
                                                     numberText(programs.value->size()) + " (positions 0 to " +
                                                     numberText(static_cast<int>(programs.value->size()) - 1) + "; see list_programs).");
            listed = (*programs.value)[static_cast<std::size_t>(position)].name;
        }

        const int wirePart = part - 1;
        const auto assigned = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            if (program.name)
                akm::setMultiPartByName(_connection->session, wirePart, listed, std::move(done));
            else
                akm::setMultiPartByIndex(_connection->session, wirePart, position, std::move(done));
        });
        if (!assigned)
            return Outcome<PartAssignment>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*assigned))
            return Outcome<PartAssignment>::failure(explain(*assigned, "assigning the program \"" + listed + "\" to part " + numberText(part), _config, true, "multi"));

        const auto read = await<akm::MultiNameResult>(waitFor(1), [&](std::function<void(const akm::MultiNameResult&)> done) {
            akm::getMultiPartName(_connection->session, wirePart, std::move(done));
        });
        if (!read)
            return Outcome<PartAssignment>::failure("The sampler session did not complete the command in time.");
        if (!read->name)
            return Outcome<PartAssignment>::failure(explain(read->outcome, "reading back part " + numberText(part), _config, true, "multi"));
        if (*read->name != listed)
            return Outcome<PartAssignment>::failure("The sampler accepted the assignment but reports that part " + numberText(part) + " plays " +
                                                 (read->name->empty() ? std::string("no program") : "\"" + *read->name + "\"") + ", not \"" + listed + "\".");
        return Outcome<PartAssignment>::success(PartAssignment{current.value->name, part, listed});
    }

    Outcome<PartClearing> SamplerGateway::clearPart(int part, std::string_view confirm)
    {
        const auto current = currentMultiEntry();
        if (!current.ok())
            return Outcome<PartClearing>::failure(current.problem);
        if (part < 1 || part > current.value->partCount)
            return Outcome<PartClearing>::failure(noSuchPart(*current.value, part));
        if (current.value->name != confirm)
            return Outcome<PartClearing>::success(PartClearing{false, current.value->name, part, ""});

        const int wirePart = part - 1;
        const auto before = await<akm::MultiNameResult>(waitFor(1), [&](std::function<void(const akm::MultiNameResult&)> done) {
            akm::getMultiPartName(_connection->session, wirePart, std::move(done));
        });
        if (!before)
            return Outcome<PartClearing>::failure("The sampler session did not complete the command in time.");
        if (!before->name)
            return Outcome<PartClearing>::failure(explain(before->outcome, "reading part " + numberText(part), _config, true, "multi"));
        if (before->name->empty())
            return Outcome<PartClearing>::failure("Part " + numberText(part) + " of the multi \"" + current.value->name +
                                                  "\" plays no program: nothing to clear.");

        const auto cleared = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::deleteMultiPart(_connection->session, wirePart, std::move(done));
        });
        if (!cleared)
            return Outcome<PartClearing>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*cleared))
            return Outcome<PartClearing>::failure(explain(*cleared, "clearing part " + numberText(part), _config, true, "multi"));

        const auto after = await<akm::MultiNameResult>(waitFor(1), [&](std::function<void(const akm::MultiNameResult&)> done) {
            akm::getMultiPartName(_connection->session, wirePart, std::move(done));
        });
        if (!after)
            return Outcome<PartClearing>::failure("The sampler session did not complete the command in time.");
        if (!after->name)
            return Outcome<PartClearing>::failure(explain(after->outcome, "reading back part " + numberText(part), _config, true, "multi"));
        if (!after->name->empty())
            return Outcome<PartClearing>::failure("The sampler accepted the removal but reports that part " + numberText(part) + " still plays \"" +
                                                  *after->name + "\".");
        return Outcome<PartClearing>::success(PartClearing{true, current.value->name, part, *before->name});
    }

    Outcome<MultiProgramNumber> SamplerGateway::setMultiProgramNumber(std::optional<int> number)
    {
        const auto current = currentMultiEntry();
        if (!current.ok())
            return Outcome<MultiProgramNumber>::failure(current.problem);

        const auto set = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::setMultiProgramNumber(_connection->session, number, std::move(done));
        });
        if (!set)
            return Outcome<MultiProgramNumber>::failure("The sampler session did not complete the command in time.");
        if (!akm::succeeded(*set))
            return Outcome<MultiProgramNumber>::failure(explain(*set, "setting the program number of the multi \"" + current.value->name + "\"", _config, true, "multi"));

        const auto read = await<akm::MultiProgramNumberResult>(waitFor(1), [&](std::function<void(const akm::MultiProgramNumberResult&)> done) {
            akm::getMultiProgramNumber(_connection->session, std::move(done));
        });
        if (!read)
            return Outcome<MultiProgramNumber>::failure("The sampler session did not complete the command in time.");
        const auto* error = std::get_if<akm::Error>(&read->outcome);
        if (!read->frontPanelNumber && (error != nullptr || !akm::succeeded(read->outcome)))
            return Outcome<MultiProgramNumber>::failure(explain(read->outcome, "reading back the program number", _config, true, "multi"));
        if (read->frontPanelNumber != number)
            return Outcome<MultiProgramNumber>::failure("The sampler accepted the program number but reports " +
                                                        (read->frontPanelNumber ? numberText(*read->frontPanelNumber) : std::string("none")) + ".");
        return Outcome<MultiProgramNumber>::success(MultiProgramNumber{current.value->name, read->frontPanelNumber});
    }

    Outcome<MultiEntry> SamplerGateway::selectMultiByName(std::string_view name)
    {
        if (const auto problem = connect())
            return Outcome<MultiEntry>::failure(*problem);

        const auto selected = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::selectMultiByName(_connection->session, name, std::move(done));
        });
        if (!selected)
            return Outcome<MultiEntry>::failure("The sampler session did not complete the command in time.");
        const auto* error = std::get_if<akm::Error>(&*selected);
        if (error != nullptr && error->number == akm::error_number::NOT_FOUND)
            return Outcome<MultiEntry>::failure("No multi is named \"" + std::string(name) + "\". Use list_multis to see the names.");
        if (!akm::succeeded(*selected))
            return Outcome<MultiEntry>::failure(explain(*selected, "selecting the multi \"" + std::string(name) + "\"", _config, false));
        return currentMultiEntry();
    }

    Outcome<MultiEntry> SamplerGateway::selectMultiByIndex(int index)
    {
        if (const auto problem = connect())
            return Outcome<MultiEntry>::failure(*problem);

        const auto selected = await<akm::CommandResult>(waitFor(1), [&](std::function<void(const akm::CommandResult&)> done) {
            akm::selectMultiByIndex(_connection->session, index, std::move(done));
        });
        if (!selected)
            return Outcome<MultiEntry>::failure("The sampler session did not complete the command in time.");
        const auto* error = std::get_if<akm::Error>(&*selected);
        if (error != nullptr && error->number == akm::error_number::NOT_FOUND)
            return Outcome<MultiEntry>::failure("No multi is at index " + numberText(index) + ". Use list_multis to see the positions.");
        if (!akm::succeeded(*selected))
            return Outcome<MultiEntry>::failure(explain(*selected, "selecting the multi at index " + numberText(index), _config, false));
        return currentMultiEntry();
    }

    Outcome<std::vector<PartValue>> SamplerGateway::editMultiParameter(const ParameterDefinition& parameter,
                                                                       std::optional<std::int64_t> valueToSet, PartSelection parts)
    {
        using Values = std::vector<PartValue>;
        if (const auto problem = connect())
            return Outcome<Values>::failure(*problem);

        const auto count = await<akm::MultiPartCountResult>(waitFor(1), [&](std::function<void(const akm::MultiPartCountResult&)> done) {
            akm::getCurrentMultiPartCount(_connection->session, std::move(done));
        });
        if (!count)
            return Outcome<Values>::failure("The sampler session did not complete the command in time.");
        if (!count->partCount)
            return Outcome<Values>::failure(explain(count->outcome, "counting the parts of the current multi", _config, true, "multi"));
        const int partCount = *count->partCount;
        if (parts.part && (*parts.part < 1 || *parts.part > partCount))
            return Outcome<Values>::failure("The current multi has " + numberText(partCount) + " parts: parts 1 to " + numberText(partCount) +
                                            " exist, part " + numberText(*parts.part) + " does not.");

        std::vector<int> wanted;
        if (parts.part)
            wanted.push_back(*parts.part);
        else
        {
            for (int part = 1; part <= partCount; ++part)
                wanted.push_back(part);
        }

        // The wire's part number starts at 0 where the front panel's starts at 1 (an assumption, see the tests).
        std::vector<akm::CommandRequest> requests;
        std::vector<std::string> steps;
        for (const int part : wanted)
        {
            const std::int64_t wirePart = part - 1;
            if (valueToSet)
            {
                std::vector<std::int64_t> values{wirePart};
                const std::vector<std::int64_t> wire = toItemValues(parameter, *valueToSet);
                values.insert(values.end(), wire.begin(), wire.end());
                requests.push_back(akm::makeRequest(parameter.setItem, values));
                steps.push_back("setting " + parameter.name + " of part " + numberText(part));
            }
            requests.push_back(akm::makeRequest(parameter.getItem, {wirePart}));
            steps.push_back("reading " + parameter.name + " of part " + numberText(part));
        }

        const auto replies = runSequence(std::move(requests), steps, true, "multi");
        if (!replies.ok())
            return Outcome<Values>::failure(replies.problem);

        Values values;
        const std::size_t stride = valueToSet ? 2 : 1;
        for (std::size_t i = 0; i < wanted.size(); ++i)
        {
            const auto set = akm::decodeReply(parameter.getItem, (*replies.value)[i * stride + stride - 1]);
            const auto value = set ? fromItemValues(parameter, *set) : std::nullopt;
            if (!value)
                return Outcome<Values>::failure("The sampler's answer for " + parameter.name + " of part " + numberText(wanted[i]) +
                                                " could not be read.");
            values.push_back(PartValue{wanted[i], *value});
        }
        if (valueToSet)
        {
            for (const PartValue& read : values)
            {
                if (read.value != *valueToSet)
                    return Outcome<Values>::failure("The sampler did not keep the value: " + parameter.name + " was set to " +
                                                    describeValue(parameter, *valueToSet) + " but reads " + describeValue(parameter, read.value) +
                                                    " for part " + numberText(read.part) + ".");
            }
        }
        return Outcome<Values>::success(std::move(values));
    }

    Outcome<std::vector<PartValue>> SamplerGateway::readMultiParameter(const ParameterDefinition& parameter, PartSelection parts)
    {
        return editMultiParameter(parameter, std::nullopt, parts);
    }

    Outcome<std::vector<PartValue>> SamplerGateway::writeMultiParameter(const ParameterDefinition& parameter, std::int64_t value,
                                                                        PartSelection parts)
    {
        return editMultiParameter(parameter, value, parts);
    }
}
