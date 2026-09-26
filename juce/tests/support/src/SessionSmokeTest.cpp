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
#include "akm/harness/SessionSmokeTest.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "akm/CommandResult.hpp"
#include "akm/DiagnosticSink.hpp"
#include "akm/SamplerError.hpp"
#include "akm/SysExConfig.hpp"
#include "akm/harness/WireFormat.hpp"
#include "akm/harness/WireLog.hpp"

namespace akm::harness
{
    namespace
    {
        using Clock = Scheduler::Clock;

        // The scenario waits for the completion of a step this many times the step's own timeout, which the session
        // enforces itself: a wait that runs out means a completion was lost, and is reported as such.
        constexpr int PATIENCE_IN_STEP_TIMEOUTS = 2;
        constexpr Clock::duration CLOSE_PATIENCE = std::chrono::seconds(5);

        // The bytes of the first Echo; the timed ones change the first byte, so that a late REPLY cannot pass for
        // the answer to the next.
        const std::array<std::uint8_t, ECHO_DATA_SIZE> ECHO_PAYLOAD{{0x01, 0x23, 0x45, 0x67}};
        constexpr std::size_t ECHO_VARYING_INDEX = 0;
        constexpr int ECHO_VARYING_MASK = 0x7F;

        std::string modeName(ChecksumMode mode)
        {
            switch (mode)
            {
                case ChecksumMode::On:
                    return "on";
                case ChecksumMode::Off:
                    return "off";
                case ChecksumMode::Unknown:
                    return "unknown";
            }
            return "unknown";
        }

        std::string millisecondsText(Clock::duration duration)
        {
            return std::to_string(millisecondsOf(duration)) + " ms";
        }

        std::string listOfIds(const std::vector<std::uint8_t>& ids)
        {
            if (ids.empty())
                return "none";
            std::ostringstream text;
            for (std::size_t index = 0; index < ids.size(); ++index)
                text << (index == 0 ? "" : " ") << static_cast<unsigned int>(ids[index]);
            return text.str();
        }

        std::string outcomeText(const CommandResult& result)
        {
            if (std::holds_alternative<Done>(result))
                return "DONE";
            if (const auto* reply = std::get_if<Reply>(&result))
                return "REPLY data " + hexOrDash(reply->data);
            if (const auto* error = std::get_if<Error>(&result))
                return "ERROR " + std::to_string(error->number) + " (" + std::string(describeError(error->number).meaning) + ")";
            if (std::holds_alternative<Timeout>(result))
                return "TIMEOUT";
            if (const auto* refused = std::get_if<Refused>(&result))
                return "REFUSED (" + std::string(describe(refused->reason)) + ")";
            return "CANCELLED";
        }

        // What a step must come to. A sampler on an older OS lacks some items and answers ERROR to them, which
        // is something to observe, not a failure of the session.
        enum class Expect
        {
            Success,  ///< DONE or REPLY
            Answer,   ///< DONE, REPLY or ERROR: the sampler answered
        };

        bool acceptable(const CommandResult& result, Expect expect)
        {
            if (expect == Expect::Success)
                return succeeded(result);
            return succeeded(result) || std::holds_alternative<Error>(result);
        }

        // A value a completion produces on the session's thread and the scenario reads on its own.
        template <typename T>
        class Slot
        {
        public:
            void set(T value, Clock::time_point at)
            {
                const std::lock_guard lock(_mutex);
                _value = std::move(value);
                _at = at;
            }

            [[nodiscard]] bool isSet() const
            {
                const std::lock_guard lock(_mutex);
                return _value.has_value();
            }

            [[nodiscard]] std::optional<T> value() const
            {
                const std::lock_guard lock(_mutex);
                return _value;
            }

            [[nodiscard]] Clock::time_point at() const
            {
                const std::lock_guard lock(_mutex);
                return _at;
            }

        private:
            mutable std::mutex _mutex;
            std::optional<T> _value;
            Clock::time_point _at{};
        };

        template <typename Result>
        struct Timed
        {
            Result result;
            Clock::duration latency;
        };

        // Logs what the session reports besides results, and counts it.
        class SmokeDiagnostics final : public DiagnosticSink
        {
        public:
            explicit SmokeDiagnostics(WireLog& log) : _log(log) {}

            void report(const Diagnostic& diagnostic) override
            {
                std::string text(describe(diagnostic.kind));
                {
                    const std::lock_guard lock(_mutex);
                    switch (diagnostic.kind)
                    {
                        case DiagnosticKind::RejectedMessage:
                            ++_rejected;
                            if (diagnostic.rejection)
                                text += ": " + std::string(describe(*diagnostic.rejection));
                            break;
                        case DiagnosticKind::UnsolicitedConfirmation:
                            ++_unsolicited;
                            break;
                        case DiagnosticKind::LateErrorAfterReply:
                            ++_lateErrors;
                            break;
                        case DiagnosticKind::ChecksumModeChanged:
                            if (diagnostic.checksumMode)
                            {
                                text += ": " + modeName(*diagnostic.checksumMode);
                                _modeChanges.push_back(modeName(*diagnostic.checksumMode));
                            }
                            break;
                    }
                }
                _log.note("diagnostic: " + text);
            }

            [[nodiscard]] std::size_t rejected() const { return read(_rejected); }
            [[nodiscard]] std::size_t unsolicited() const { return read(_unsolicited); }
            [[nodiscard]] std::size_t lateErrors() const { return read(_lateErrors); }

            [[nodiscard]] std::string modeChanges() const
            {
                const std::lock_guard lock(_mutex);
                std::string chain = "unknown";
                for (const std::string& mode : _modeChanges)
                    chain += " -> " + mode;
                return chain;
            }

        private:
            [[nodiscard]] std::size_t read(const std::size_t& counter) const
            {
                const std::lock_guard lock(_mutex);
                return counter;
            }

            WireLog& _log;
            mutable std::mutex _mutex;
            std::size_t _rejected = 0;
            std::size_t _unsolicited = 0;
            std::size_t _lateErrors = 0;
            std::vector<std::string> _modeChanges;
        };

        // The sequence itself, on a session that is already wired to the ports.
        class Smoke
        {
        public:
            Smoke(WireLog& log, ScenarioDriver& driver, Session& session, SmokeDiagnostics& diagnostics,
                  const SessionSmokeOptions& options, SessionSmokeResult& result)
                : _log(log), _driver(driver), _session(session), _diagnostics(diagnostics), _options(options),
                  _result(result)
            {
            }

            void run()
            {
                if (!discovery())
                    return;
                checksumAndVersion();
                echoes();
                checksumOnAndOff();
                toggles();
                closing();
            }

            void closeSession()
            {
                const auto closed = std::make_shared<Slot<bool>>();
                Scheduler& clock = _driver.scheduler();
                if (_session.close([closed, &clock] { closed->set(true, clock.now()); }))
                    static_cast<void>(_driver.waitUntil([closed] { return closed->isSet(); }, CLOSE_PATIENCE));
                _result.finalChecksumMode = _session.checksumMode();
            }

            void observe()
            {
                _result.framesSent = _log.framesSent();
                _result.framesReceived = _log.framesReceived();
                _result.stillAliveMessagesSeen = _log.stillAliveMessages();
                _result.rejectedMessages = _diagnostics.rejected();
                _result.unsolicitedConfirmations = _diagnostics.unsolicited();
                _result.lateErrors = _diagnostics.lateErrors();

                _log.note("observations");
                _log.note("observation: discovery answered by DeviceIDs: " + listOfIds(_result.discoveredDeviceIds));
                if (_result.osVersion)
                {
                    const OsVersionReport& version = *_result.osVersion;
                    _log.note("observation: OS version " + std::to_string(version.major) + "." + std::to_string(version.minor)
                              + (version.subVersion ? " (sub-version " + std::to_string(*version.subVersion) + ")"
                                                    : " (sub-version unavailable)"));
                }
                else
                    _log.note("observation: OS version not obtained");
                if (_options.echoRepeats > 0)
                {
                    const std::string count = std::to_string(_options.echoRepeats);
                    if (_result.echoLatencies.empty())
                        _log.note("observation: 0 of " + count + " Echo round trips succeeded");
                    else
                        _log.note("observation: " + std::to_string(_result.echoRoundTrips) + " Echo round trips: "
                                  + latencySummary());
                }
                _log.note("observation: F0 F7 messages seen: " + std::to_string(_result.stillAliveMessagesSeen));
                _log.note("observation: messages rejected by the session: " + std::to_string(_result.rejectedMessages)
                          + "; unsolicited confirmations: " + std::to_string(_result.unsolicitedConfirmations)
                          + "; late ERRORs after a REPLY: " + std::to_string(_result.lateErrors));
                _log.note("observation: checksum mode as the session followed it: " + _diagnostics.modeChanges());
                _log.note("observation: steps that did not go as they had to: " + failedStepsText());
                if (!_result.samplerFound)
                    _log.note("observation: no setting was changed: nothing was sent after the discovery");
                else
                    _log.note(_result.knownStateRestored ? "observation: sampler left in the known state: " + knownStateText()
                                                         : "observation: sampler NOT confirmed in the known state ("
                                                               + knownStateText() + ")");
                _log.note("observation: frames sent " + std::to_string(_result.framesSent) + ", received "
                          + std::to_string(_result.framesReceived));
            }

        private:
            [[nodiscard]] Clock::duration patience() const
            {
                return _options.stepTimeout * PATIENCE_IN_STEP_TIMEOUTS + _options.discoveryWindow;
            }

            // Runs `launch` with a completion, waits for it and times it from the submit to the completion.
            template <typename Result, typename Launch>
            std::optional<Timed<Result>> await(Launch&& launch)
            {
                const auto slot = std::make_shared<Slot<Result>>();
                Scheduler& clock = _driver.scheduler();
                const Clock::time_point submitted = clock.now();
                launch(std::function<void(const Result&)>(
                    [slot, &clock](const Result& result) { slot->set(result, clock.now()); }));
                if (!_driver.waitUntil([slot] { return slot->isSet(); }, patience()))
                    return std::nullopt;
                return Timed<Result>{*slot->value(), slot->at() - submitted};
            }

            void begin(const std::string& title)
            {
                ++_step;
                _log.note("step " + std::to_string(_step) + ": " + title);
            }

            void end(const std::string& title, bool ok, const std::string& text, Clock::duration latency)
            {
                _log.note("  " + text + " after " + millisecondsText(latency));
                if (!ok)
                    fail(title);
            }

            void lost(const std::string& title)
            {
                _log.note("  no completion within " + millisecondsText(patience()) + ": the session lost it");
                fail(title);
            }

            void fail(const std::string& title)
            {
                _log.note("  this step did not go as it had to");
                _result.failedSteps.push_back(title);
            }

            CommandResult command(const std::string& title, Expect expect,
                                  const std::function<void(CommandCompletion)>& launch)
            {
                begin(title);
                const auto timed = await<CommandResult>(launch);
                if (!timed)
                {
                    lost(title);
                    return Timeout{};
                }
                end(title, acceptable(timed->result, expect), outcomeText(timed->result), timed->latency);
                return timed->result;
            }

            // An Echo, returned as the session's result; the text says what came back.
            std::optional<Timed<akm::EchoResult>> echoOnce(const std::array<std::uint8_t, ECHO_DATA_SIZE>& payload)
            {
                return await<akm::EchoResult>([this, &payload](EchoCompletion done) { echo(_session, payload, std::move(done)); });
            }

            static std::string echoText(const akm::EchoResult& result, const std::array<std::uint8_t, ECHO_DATA_SIZE>& payload)
            {
                if (result.succeeded())
                    return "REPLY " + hex(payload) + " as sent";
                if (result.mismatch)
                    return "MISMATCH sent " + hex(result.mismatch->sent) + ", received " + hexOrDash(result.mismatch->received);
                return outcomeText(result.outcome);
            }

            void echoStep(const std::string& title)
            {
                begin(title);
                const auto timed = echoOnce(ECHO_PAYLOAD);
                if (!timed)
                {
                    lost(title);
                    return;
                }
                end(title, timed->result.succeeded(), echoText(timed->result, ECHO_PAYLOAD), timed->latency);
            }

            void versionStep(const std::string& title)
            {
                begin(title);
                const auto timed = await<OsVersionResult>(
                    [this](OsVersionCompletion done) { queryOsVersion(_session, std::move(done)); });
                if (!timed)
                {
                    lost(title);
                    return;
                }
                const OsVersionResult& result = timed->result;
                if (!result.version)
                {
                    end(title, false, outcomeText(result.outcome), timed->latency);
                    return;
                }
                if (!_result.osVersion)
                    _result.osVersion = result.version;
                end(title, true,
                    "OS " + std::to_string(result.version->major) + "." + std::to_string(result.version->minor)
                        + (result.version->subVersion ? ", sub-version " + std::to_string(*result.version->subVersion)
                                                      : ", sub-version unavailable"),
                    timed->latency);
            }

            using Setter = void (*)(Session&, bool, CommandCompletion);

            CommandResult setting(const std::string& title, Expect expect, Setter set, bool on)
            {
                return command(title, expect, [this, set, on](CommandCompletion done) { set(_session, on, std::move(done)); });
            }

            // DEC-AKM-007: discovery first, then the target's DeviceID verified, then bound.
            bool discovery()
            {
                const std::string title = "Discovery: a Query to DeviceID 0, every sampler answers with its own DeviceID";
                begin(title);
                const auto timed = await<DiscoveryResult>([this](DiscoveryCompletion done) {
                    discover(_session, std::move(done), _options.discoveryWindow);
                });
                if (!timed)
                {
                    lost(title);
                    return false;
                }
                _result.discoveredDeviceIds = timed->result.deviceIds;
                _log.note("  answered by DeviceIDs: " + listOfIds(timed->result.deviceIds) + " after "
                          + millisecondsText(timed->latency));

                const auto target = static_cast<std::uint8_t>(_options.target.deviceId);
                const std::string targetText = std::to_string(_options.target.deviceId);
                if (timed->result.deviceIds.empty())
                {
                    _log.note("error: no sampler answered the discovery within " + millisecondsText(_options.discoveryWindow)
                              + ": nothing else is sent");
                    return false;
                }
                if (std::find(timed->result.deviceIds.begin(), timed->result.deviceIds.end(), target)
                    == timed->result.deviceIds.end())
                {
                    _log.note("error: no sampler with DeviceID " + targetText + " answered the discovery; a sampler answered as "
                              + listOfIds(timed->result.deviceIds) + " (use --device-id with that number): nothing else is sent");
                    return false;
                }
                _result.samplerFound = true;
                _session.bindTarget(target);
                _log.note("  DeviceID " + targetText + " is bound as the target");
                return true;
            }

            void checksumAndVersion()
            {
                setting("Checksums off (section 00, item 04), sent with a checksum whatever the sampler's state",
                        Expect::Success, &setChecksumMode, false);
                versionStep("Operating system version (section 02, items 00 and 01)");
            }

            void echoes()
            {
                echoStep("Echo 01 23 45 67, checksums off");
                echoRoundTrips();
            }

            void echoRoundTrips()
            {
                if (_options.echoRepeats <= 0)
                    return;
                const std::string title = std::to_string(_options.echoRepeats)
                                          + " Echo round trips, one after the other, timed from submit to completion";
                begin(title);
                for (int index = 0; index < _options.echoRepeats; ++index)
                {
                    std::array<std::uint8_t, ECHO_DATA_SIZE> payload = ECHO_PAYLOAD;
                    payload[ECHO_VARYING_INDEX] = static_cast<std::uint8_t>(index & ECHO_VARYING_MASK);
                    const auto timed = echoOnce(payload);
                    if (!timed || !timed->result.succeeded())
                    {
                        _log.note("  round trip " + std::to_string(index + 1) + " failed: "
                                  + (timed ? echoText(timed->result, payload) : std::string("no completion")));
                        fail(title);
                        break;
                    }
                    _result.echoLatencies.push_back(timed->latency);
                    ++_result.echoRoundTrips;
                }
                _log.note("  " + std::to_string(_result.echoRoundTrips) + " of " + std::to_string(_options.echoRepeats)
                          + " round trips succeeded" + (_result.echoLatencies.empty() ? "" : "; " + latencySummary()));
            }

            void checksumOnAndOff()
            {
                setting("Checksums on (section 00, item 04)", Expect::Success, &setChecksumMode, true);
                echoStep("Echo with checksums on: the REPLY carries a checksum");
                versionStep("Operating system version with checksums on");
                setting("Checksums off again", Expect::Success, &setChecksumMode, false);
                echoStep("Echo with checksums off again");
            }

            void toggles()
            {
                setting("Notification off (section 00, item 01)", Expect::Success, &setNotification, false);
                echoStep("Echo with Notification off: only the REPLY is expected, no OK");
                setting("Notification on", Expect::Success, &setNotification, true);
                if (_options.touchLcdSettings)
                {
                    setting("Sync LCD off (section 00, item 03)", Expect::Answer, &setSyncLcd, false);
                    setting("Sync LCD on", Expect::Answer, &setSyncLcd, true);
                    setting("Auto screen update on (section 00, item 05)", Expect::Answer, &setAutoScreenUpdate, true);
                    setting("Auto screen update off", Expect::Answer, &setAutoScreenUpdate, false);
                }
                setting("Still Alive on (section 00, item 07)", Expect::Answer, &setStillAlive, true);
                setting("Still Alive off", Expect::Answer, &setStillAlive, false);
            }

            // Sent whatever happened before, and the checksum command first: it works whichever the sampler's
            // real state (spec p. 4), so the others are framed the way the sampler expects.
            void closing()
            {
                _log.note("closing: leave the sampler in a known state, whatever happened above");
                bool restored = true;
                const auto restore = [this, &restored](const std::string& title, Expect expect, Setter set, bool on) {
                    const bool accepted = acceptable(setting(title, expect, set, on), expect);
                    restored = restored && accepted;
                };
                restore("Closing: checksums off, sent with a checksum whatever the state", Expect::Success,
                        &setChecksumMode, false);
                restore("Closing: Still Alive off", Expect::Answer, &setStillAlive, false);
                restore("Closing: Notification on", Expect::Success, &setNotification, true);
                if (_options.touchLcdSettings)
                {
                    restore("Closing: Sync LCD on", Expect::Answer, &setSyncLcd, true);
                    restore("Closing: Auto screen update off", Expect::Answer, &setAutoScreenUpdate, false);
                }
                _result.knownStateRestored = restored;
            }

            [[nodiscard]] std::string latencySummary() const
            {
                std::vector<Clock::duration> sorted = _result.echoLatencies;
                std::sort(sorted.begin(), sorted.end());
                return "min " + millisecondsText(sorted.front()) + ", median " + millisecondsText(sorted[sorted.size() / 2])
                       + ", max " + millisecondsText(sorted.back());
            }

            [[nodiscard]] std::string failedStepsText() const
            {
                if (_result.failedSteps.empty())
                    return "none";
                std::string text;
                for (const std::string& title : _result.failedSteps)
                    text += (text.empty() ? "" : "; ") + title;
                return text;
            }

            [[nodiscard]] std::string knownStateText() const
            {
                return _options.touchLcdSettings
                           ? "checksums off, Still Alive off, Notification on, Sync LCD on, Auto screen update off"
                           : "checksums off, Still Alive off, Notification on";
            }

            WireLog& _log;
            ScenarioDriver& _driver;
            Session& _session;
            SmokeDiagnostics& _diagnostics;
            const SessionSmokeOptions& _options;
            SessionSmokeResult& _result;
            int _step = 0;
        };

        void writeHeader(WireLog& log, const SessionSmokeOptions& options)
        {
            log.note("XS56K AKM session smoke test");
            if (!options.startedAt.empty())
                log.note("started " + options.startedAt);
            log.note("target: in=\"" + options.target.inputPortName + "\" out=\"" + options.target.outputPortName
                     + "\" device-id=" + std::to_string(options.target.deviceId));
            log.note(std::string("It drives a Session through the section 00 primitives. It switches checksums, Notification")
                     + (options.touchLcdSettings ? ", Sync LCD, Auto screen update" : "")
                     + " and Still Alive on and off, and ends by putting them back: checksums off, Still Alive off,"
                     + " Notification on" + (options.touchLcdSettings ? ", Sync LCD on, Auto screen update off." : "."));
        }
    }

    SessionSmokeResult runSessionSmokeTest(common::midi::MidiBackend& backend, ScenarioDriver& driver,
                                           const SessionSmokeOptions& options, std::ostream& out)
    {
        SessionSmokeResult result;
        WireLog log(out, driver.scheduler());
        writeHeader(log, options);

        auto input = backend.openInput(options.target.inputPortName);
        if (!input)
        {
            log.note("error: input port not found: " + options.target.inputPortName);
            return result;
        }
        auto output = backend.openOutput(options.target.outputPortName);
        if (!output)
        {
            log.note("error: output port not found: " + options.target.outputPortName);
            return result;
        }
        result.portsOpened = true;

        // Declared in the order they must outlive one another: the session last, so that it is destroyed first and
        // stops the input port before anything the input's callback touches is gone.
        LoggingInputPort loggedInput(*input, log);
        LoggingOutputPort loggedOutput(*output, log);
        SmokeDiagnostics diagnostics(log);
        SessionTiming timing;
        timing.commandTimeout = options.stepTimeout;
        Session session(timing, driver.executor(), driver.scheduler(), loggedInput, loggedOutput, diagnostics);

        Smoke smoke(log, driver, session, diagnostics, options, result);
        smoke.run();
        smoke.closeSession();
        smoke.observe();
        return result;
    }
}
