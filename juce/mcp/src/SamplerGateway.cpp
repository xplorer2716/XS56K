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
#include "akm/RealScheduler.hpp"
#include "akm/SamplerError.hpp"
#include "akm/ThreadExecutor.hpp"

namespace mcp
{
    namespace
    {
        // What is added to the session's own timeouts before the gateway gives up waiting for a completion that
        // should always come (a session completes every command, DEC-AKM-004): it only guards against a lost one.
        constexpr std::chrono::milliseconds WAIT_MARGIN{3000};
        // The opening sends up to this many commands (the checksum mode and the settings) after the discovery.
        constexpr int OPENING_COMMAND_BUDGET = 8;
        constexpr int CLOSING_COMMAND_BUDGET = 8;
        constexpr std::int64_t ALL_KEYGROUPS = 0;
        constexpr std::uint32_t MAX_DEVICE_ID = 31;

        constexpr std::span<const std::int64_t> NO_VALUES{};

        std::string numberText(std::int64_t value)
        {
            return std::to_string(value);
        }

        /// Waits for the one completion of an asynchronous AKM call, with a deadline; nothing when it did not come.
        template <typename Result>
        std::optional<Result> await(std::chrono::milliseconds deadline,
                                    const std::function<void(std::function<void(const Result&)>)>& start)
        {
            auto promise = std::make_shared<std::promise<Result>>();
            std::future<Result> future = promise->get_future();
            start([promise](const Result& result) { promise->set_value(result); });
            if (future.wait_for(deadline) != std::future_status::ready)
                return std::nullopt;
            return future.get();
        }

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

        /// What a command's outcome says when it did not succeed, as one sentence for the person.
        std::string explain(const akm::CommandResult& outcome, const std::string& doing, const GatewayConfig& config,
                            bool needsCurrentProgram)
        {
            if (std::holds_alternative<akm::Timeout>(outcome))
                return "The sampler did not answer while " + doing + " (no reply within " + numberText(config.commandTimeout.count()) +
                       " ms). Check that it is on and connected.";
            if (const auto* refusal = std::get_if<akm::Refused>(&outcome))
                return "The command was not sent while " + doing + ": " + std::string(akm::describe(refusal->reason)) + ".";
            if (const auto* error = std::get_if<akm::Error>(&outcome))
            {
                const akm::ErrorInfo info = akm::describeError(error->number);
                std::string text = "The sampler refused while " + doing + ": " + std::string(info.meaning) + " (error " +
                                   numberText(error->number) + ").";
                if (error->number == akm::error_number::KEYGROUP_NOT_IN_PROGRAM)
                    text += " The current program does not have that keygroup.";
                else if (needsCurrentProgram && error->number == akm::error_number::NOT_FOUND)
                    text += " Is a program selected? Use select_program first.";
                return text;
            }
            if (std::holds_alternative<akm::Cancelled>(outcome))
                return "The command was cancelled while " + doing + ": the connection is closing.";
            return "The sampler's answer to the command while " + doing + " was not understood.";
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
                                                                               bool needsCurrentProgram)
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
                                                     needsCurrentProgram));
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

        // A Get while "all keygroups" is selected answers one record per keygroup, which only a session that knows its
        // checksum mode can delimit (DEC-AKM-014, RQ-AKM-031).
        akm::CommandRequest getRequest(const ParameterDefinition& parameter, bool answersPerKeygroup)
        {
            akm::CommandOptions options;
            if (answersPerKeygroup)
                options.expectedReply = akm::ExpectedReply::NeedsKnownChecksumMode;
            return akm::makeRequest(parameter.getItem, parameter.leadingArguments, options);
        }

        akm::CommandRequest setRequest(const ParameterDefinition& parameter, std::int64_t value)
        {
            std::vector<std::int64_t> values = parameter.leadingArguments;
            const std::vector<std::int64_t> wire = toItemValues(parameter, value);
            values.insert(values.end(), wire.begin(), wire.end());
            return akm::makeRequest(parameter.setItem, values);
        }

        std::string unreadable(const ParameterDefinition& parameter)
        {
            return "The sampler's answer for " + parameter.name + " could not be read.";
        }
    }

    Outcome<std::vector<ParameterValue>> SamplerGateway::editParameter(const ParameterDefinition& parameter,
                                                                       std::optional<std::int64_t> valueToSet,
                                                                       KeygroupSelection selection)
    {
        using Values = std::vector<ParameterValue>;
        const bool onKeygroups = parameter.scope == ParameterScope::Keygroup;
        int keygroups = 0;

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
            requests.push_back(setRequest(parameter, *valueToSet));
            steps.push_back("setting " + parameter.name);
        }
        const bool repeated = onKeygroups && !selection.keygroup.has_value();
        requests.push_back(getRequest(parameter, repeated));
        steps.push_back("reading " + parameter.name);

        const auto replies = runSequence(std::move(requests), steps, true);
        if (!replies.ok())
            return Outcome<Values>::failure(replies.problem);

        Values values;
        const std::vector<std::uint8_t>& answer = replies.value->back();
        if (repeated)
        {
            const auto sets = akm::decodeRepeatedReply(parameter.getItem, answer);
            if (!sets || static_cast<int>(sets->size()) != keygroups)
                return Outcome<Values>::failure(unreadable(parameter));
            for (std::size_t i = 0; i < sets->size(); ++i)
            {
                const auto value = fromItemValues(parameter, (*sets)[i]);
                if (!value)
                    return Outcome<Values>::failure(unreadable(parameter));
                values.push_back(ParameterValue{static_cast<int>(i) + 1, *value});
            }
        }
        else
        {
            const auto set = akm::decodeReply(parameter.getItem, answer);
            const auto value = set ? fromItemValues(parameter, *set) : std::nullopt;
            if (!value)
                return Outcome<Values>::failure(unreadable(parameter));
            values.push_back(ParameterValue{onKeygroups ? selection.keygroup : std::nullopt, *value});
        }

        if (valueToSet)
        {
            for (const ParameterValue& read : values)
            {
                if (read.value != *valueToSet)
                    return Outcome<Values>::failure("The sampler did not keep the value: " + parameter.name + " was set to " +
                                                    describeValue(parameter, *valueToSet) + " but reads " +
                                                    describeValue(parameter, read.value) +
                                                    (read.keygroup ? " for keygroup " + numberText(*read.keygroup) : "") + ".");
            }
        }
        return Outcome<Values>::success(std::move(values));
    }

    Outcome<std::vector<ParameterValue>> SamplerGateway::readParameter(const ParameterDefinition& parameter,
                                                                       KeygroupSelection selection)
    {
        return editParameter(parameter, std::nullopt, selection);
    }

    Outcome<std::vector<ParameterValue>> SamplerGateway::writeParameter(const ParameterDefinition& parameter, std::int64_t value,
                                                                        KeygroupSelection selection)
    {
        return editParameter(parameter, value, selection);
    }
}
