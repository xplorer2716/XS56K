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
#include "akm/harness/RealSamplerSuite.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "ScenarioSupport.hpp"
#include "akm/Command.hpp"
#include "akm/CommandOptions.hpp"
#include "akm/CommandResult.hpp"
#include "akm/SysExConfig.hpp"
#include "akm/harness/WireFormat.hpp"
#include "akm/harness/WireLog.hpp"

namespace akm::harness
{
    std::size_t RealSuiteResult::count(CheckOutcome outcome) const
    {
        return static_cast<std::size_t>(std::count_if(
            checks.begin(), checks.end(), [outcome](const CheckReport& report) { return report.outcome == outcome; }));
    }

    namespace
    {
        using namespace detail;

        // The scenario waits for what the session enforces the timeouts of itself, this many command timeouts: a wait
        // that runs out means a completion was lost, and is reported as such. An open is the discovery's window and
        // up to five commands, a close up to five (the first that times out ends it).
        constexpr int COMMAND_PATIENCE_IN_TIMEOUTS = 2;
        constexpr int OPEN_PATIENCE_IN_TIMEOUTS = 8;
        constexpr int CLOSE_PATIENCE_IN_TIMEOUTS = 4;

        // After a power cycle the sampler gets this many Echos to answer again. A sampler that came back with its own
        // settings makes a session that assumes checksums on fail three verifications, then read the next confirmation
        // as an unknown mode: the second Echo already answers.
        constexpr int POWER_CYCLE_ATTEMPTS = 4;

        // The sampler sends `F0 F7` about every second while it works (spec, section 00 footnote b): a command shorter
        // than this cannot be taken to have provoked one.
        constexpr Clock::duration STILL_ALIVE_PERIOD = std::chrono::seconds(1);

        // The slow but harmless command: "Update the list of disks connected" (section 10, item 01).
        constexpr std::uint8_t SECTION_DISK_TOOLS = 0x10;
        constexpr std::uint8_t ITEM_UPDATE_DISK_LIST = 0x01;

        // A check that did not go as it had to: thrown by an expectation, caught by the runner, which reports it.
        class CheckFailure : public std::runtime_error
        {
        public:
            using std::runtime_error::runtime_error;
        };

        // A failure of the opening that leaves nothing to talk to — no sampler at the target, several, a target that
        // is not a DeviceID — which ends the suite: nothing else is sent.
        class NoSamplerFailure final : public CheckFailure
        {
        public:
            using CheckFailure::CheckFailure;
        };

        // A check that was not run, and why; not a failure.
        class CheckSkipped final : public std::runtime_error
        {
        public:
            using std::runtime_error::runtime_error;
        };

        // Everything the checks share for one run.
        struct Rig
        {
            WireLog& log;
            ScenarioDriver& driver;
            LoggingInputPort& input;
            LoggingOutputPort& output;
            ScenarioDiagnostics& diagnostics;
            const RealSuiteOptions& options;
            RealSuiteResult& result;

            [[nodiscard]] Clock::duration commandPatience() const
            {
                return options.commandTimeout * COMMAND_PATIENCE_IN_TIMEOUTS;
            }

            [[nodiscard]] Clock::duration openPatience() const
            {
                return options.discoveryWindow + options.commandTimeout * OPEN_PATIENCE_IN_TIMEOUTS;
            }

            [[nodiscard]] Clock::duration closePatience() const { return options.commandTimeout * CLOSE_PATIENCE_IN_TIMEOUTS; }

            [[nodiscard]] SessionTiming timing() const
            {
                SessionTiming timing;
                timing.commandTimeout = options.commandTimeout;
                timing.discoveryWindow = options.discoveryWindow;
                return timing;
            }
        };

        std::string settingsText(const std::vector<SamplerSetting>& settings)
        {
            if (settings.empty())
                return "none";
            std::string text;
            for (const SamplerSetting setting : settings)
                text += (text.empty() ? "" : ", ") + std::string(describe(setting));
            return text;
        }

        bool sameSettings(std::vector<SamplerSetting> first, std::vector<SamplerSetting> second)
        {
            std::sort(first.begin(), first.end());
            std::sort(second.begin(), second.end());
            return first == second;
        }

        bool contains(const std::vector<SamplerSetting>& settings, SamplerSetting wanted)
        {
            return std::find(settings.begin(), settings.end(), wanted) != settings.end();
        }

        // A session opened through `Session::open` and closed through `Session::close`, whatever becomes of the check
        // that uses it: its destructor closes a session that is still open — which is what runs when a check fails
        // half way, a failed assertion being an exception — so the sampler is left as the closing leaves it. The ports
        // outlive it, and a session's input is stopped by its close, so the next check's session starts the same
        // ports again. [RQ-AKM-018, RQ-AKM-042]
        class GuardedSession
        {
        public:
            explicit GuardedSession(Rig& rig, std::optional<CloseResult>* closedInto = nullptr)
                : _rig(rig), _closedInto(closedInto),
                  _session(rig.timing(), rig.driver.executor(), rig.driver.scheduler(), rig.input, rig.output, rig.diagnostics)
            {
            }

            ~GuardedSession()
            {
                try
                {
                    const SessionState state = _session.state();
                    if (state != SessionState::Closed && state != SessionState::Closing)
                    {
                        _rig.log.note("  the check ended with its session still open: the guard closes it");
                        static_cast<void>(close());
                    }
                }
                catch (...)  // NOLINT: a destructor does not throw; a close that was lost has been noted by close()
                {
                }
            }

            GuardedSession(const GuardedSession&) = delete;
            GuardedSession& operator=(const GuardedSession&) = delete;

            [[nodiscard]] Session& session() { return _session; }

            // Session::open, waited for. It throws when the session is not ready: a NoSamplerFailure when there is
            // nothing at the target to talk to.
            Timed<OpenResult> open(const SessionConfig& config)
            {
                _rig.log.flush();
                const auto timed = awaitCompletion<OpenResult>(
                    _rig.driver, _rig.openPatience(), [this, &config](OpenCompletion done) { _session.open(config, std::move(done)); });
                if (!timed)
                    throw CheckFailure("the open did not complete within " + millisecondsText(_rig.openPatience())
                                       + ": the session lost it");
                const OpenResult& opened = timed->result;
                if (_rig.result.discoveredDeviceIds.empty())
                    _rig.result.discoveredDeviceIds = opened.responders;
                if (opened.ready())
                    return *timed;

                std::string text = "the open ended as " + std::string(describe(opened.status))
                                   + "; DeviceIDs that answered the discovery: " + listOfIds(opened.responders);
                if (opened.failedSetting)
                    text += "; failed setting: " + std::string(describe(*opened.failedSetting)) + " ("
                            + outcomeText(opened.failedResult) + ")";
                switch (opened.status)
                {
                    case OpenStatus::NoSamplerAtTarget:
                    case OpenStatus::AmbiguousSamplers:
                    case OpenStatus::InvalidDeviceId:
                    case OpenStatus::DiscoveryFailed:
                        throw NoSamplerFailure(text);
                    case OpenStatus::Ready:
                    case OpenStatus::ReadyDegraded:
                    case OpenStatus::SettingFailed:
                    case OpenStatus::AlreadyOpen:
                    case OpenStatus::Cancelled:
                        break;
                }
                throw CheckFailure(text);
            }

            // Session::close, waited for and logged. A close that does not complete leaves the sampler in a state that
            // nothing confirms, and is recorded as such.
            Timed<CloseResult> close()
            {
                _rig.log.flush();
                _rig.log.note("  closing: the session puts back the settings it changed");
                const auto timed = awaitCompletion<CloseResult>(_rig.driver, _rig.closePatience(), [this](CloseCompletion done) {
                    if (!_session.close(std::move(done)))
                        throw CheckFailure("the session refused to close");
                });
                if (!timed)
                {
                    _rig.result.knownStateRestored = false;
                    throw CheckFailure("the close did not complete within " + millisecondsText(_rig.closePatience())
                                       + ": the sampler may not be in the known state");
                }
                for (const SamplerSetting setting : timed->result.restored)
                    _rig.log.note("  put back: " + std::string(describe(setting)));
                for (const SamplerSetting setting : timed->result.notRestored)
                    _rig.log.note("  NOT put back: " + std::string(describe(setting)));
                if (!timed->result.restoredAll())
                    _rig.result.knownStateRestored = false;
                if (_closedInto != nullptr)
                    *_closedInto = timed->result;
                return *timed;
            }

        private:
            Rig& _rig;
            std::optional<CloseResult>* _closedInto;
            Session _session;
        };

        // The checks, one after the other.
        class Suite
        {
        public:
            explicit Suite(Rig& rig) : _rig(rig) {}

            void run()
            {
                check("open a session and close it", &Suite::openAndClose);
                check("Echo returns the bytes sent", &Suite::echoReturnsTheBytes);
                check(std::to_string(_rig.options.echoRepeats) + " Echo round trips, timed", &Suite::echoLatencies);
                check("the operating system version is read", &Suite::osVersion);
                check("checksums on and off through the session", &Suite::checksumsOnAndOff);
                check("closing puts back every setting the session changed", &Suite::closePutsBack);
                check("a check that fails half way leaves the sampler in the known state",
                      &Suite::failedCheckLeavesTheKnownState);
                if (_rig.options.slowOperation)
                    check("a slow operation with Still Alive on", &Suite::slowOperation);
                if (_rig.options.powerCycle)
                    check("a power cycle while a session is open", &Suite::powerCycle);
            }

            // The observations block: what each check found, then what RQ-AKM-017 asks to be recorded.
            void observe()
            {
                RealSuiteResult& result = _rig.result;
                result.framesSent = _rig.log.framesSent();
                result.framesReceived = _rig.log.framesReceived();
                result.stillAliveMessagesSeen = _rig.log.stillAliveMessages();
                result.rejectedMessages = _rig.diagnostics.rejected();
                result.unsolicitedConfirmations = _rig.diagnostics.unsolicited();
                result.lateErrors = _rig.diagnostics.lateErrors();

                _rig.log.note("observations");
                int number = 0;
                for (const CheckReport& report : result.checks)
                    _rig.log.note("observation: check " + std::to_string(++number) + " " + outcomeName(report.outcome) + ": "
                                  + report.title + (report.detail.empty() ? "" : " - " + report.detail));
                _rig.log.note("observation: " + std::to_string(result.count(CheckOutcome::Passed)) + " checks passed, "
                              + std::to_string(result.count(CheckOutcome::Failed)) + " failed, "
                              + std::to_string(result.count(CheckOutcome::Skipped)) + " skipped");
                _rig.log.note("observation: discovery answered by DeviceIDs: " + listOfIds(result.discoveredDeviceIds)
                              + "; the target DeviceID is " + std::to_string(_rig.options.target.deviceId)
                              + ", and the session accepts only confirmations that carry it");
                if (result.osVersion)
                {
                    const OsVersionReport& version = *result.osVersion;
                    _rig.log.note("observation: OS version " + std::to_string(version.major) + "."
                                  + std::to_string(version.minor)
                                  + (version.subVersion ? " (sub-version " + std::to_string(*version.subVersion) + ")"
                                                        : " (sub-version unavailable)"));
                }
                else
                    _rig.log.note("observation: OS version not obtained");
                if (_rig.options.echoRepeats > 0)
                {
                    if (result.echoLatencies.empty())
                        _rig.log.note("observation: 0 of " + std::to_string(_rig.options.echoRepeats)
                                      + " Echo round trips succeeded");
                    else
                        _rig.log.note("observation: " + std::to_string(result.echoRoundTrips) + " Echo round trips of "
                                      + std::to_string(_rig.options.echoRepeats) + ": " + latencyText(latencyStats(result.echoLatencies)));
                }
                _rig.log.note("observation: F0 F7 messages seen: " + std::to_string(result.stillAliveMessagesSeen));
                _rig.log.note("observation: messages rejected by the session: " + std::to_string(result.rejectedMessages)
                              + "; unsolicited confirmations: " + std::to_string(result.unsolicitedConfirmations)
                              + "; late ERRORs after a REPLY: " + std::to_string(result.lateErrors));
                _rig.log.note("observation: checksum mode as the sessions followed it: " + _rig.diagnostics.modeChanges());
                _rig.log.note(result.knownStateRestored
                                  ? "observation: sampler left in the known state: " + knownStateText()
                                  : "observation: sampler NOT confirmed in the known state (" + knownStateText() + ")");
                _rig.log.note("observation: frames sent " + std::to_string(result.framesSent) + ", received "
                              + std::to_string(result.framesReceived));
                _rig.log.flush();
            }

        private:
            using Body = void (Suite::*)();

            static std::string outcomeName(CheckOutcome outcome)
            {
                switch (outcome)
                {
                    case CheckOutcome::Passed:
                        return "PASSED";
                    case CheckOutcome::Failed:
                        return "FAILED";
                    case CheckOutcome::Skipped:
                        return "SKIPPED";
                }
                return "FAILED";
            }

            [[nodiscard]] std::string knownStateText() const
            {
                return _rig.options.touchLcdSettings
                           ? "checksums off, Still Alive off, Notification on, Sync LCD on, Auto screen update off"
                           : "checksums off, Still Alive off, Notification on";
            }

            // Runs one check: the log says what it is, then how it ended. A check that throws leaves its session to
            // the guard; the suite goes on with the next check, unless there was no sampler to talk to.
            void check(const std::string& title, Body body)
            {
                _rig.log.flush();
                _rig.log.note("check " + std::to_string(_rig.result.checks.size() + 1) + ": " + title);
                CheckReport report;
                report.title = title;
                _findings.clear();
                if (_noSampler)
                {
                    report.outcome = CheckOutcome::Skipped;
                    report.detail = "no sampler at the target: nothing else is sent";
                }
                else
                {
                    try
                    {
                        (this->*body)();
                        report.outcome = CheckOutcome::Passed;
                        report.detail = findingsText();
                    }
                    catch (const NoSamplerFailure& failure)
                    {
                        _noSampler = true;
                        report.outcome = CheckOutcome::Failed;
                        report.detail = failure.what();
                    }
                    catch (const CheckFailure& failure)
                    {
                        report.outcome = CheckOutcome::Failed;
                        report.detail = failure.what();
                    }
                    catch (const CheckSkipped& skipped)
                    {
                        report.outcome = CheckOutcome::Skipped;
                        report.detail = skipped.what();
                    }
                    catch (const std::exception& unexpected)
                    {
                        report.outcome = CheckOutcome::Failed;
                        report.detail = std::string("unexpected exception: ") + unexpected.what();
                    }
                }
                _rig.log.note("  " + outcomeName(report.outcome) + (report.detail.empty() ? "" : ": " + report.detail));
                _rig.result.checks.push_back(std::move(report));
                _rig.log.flush();
            }

            // What the check found, said in the log and kept for its report.
            void finding(const std::string& text)
            {
                _rig.log.note("  " + text);
                _findings.push_back(text);
            }

            [[nodiscard]] std::string findingsText() const
            {
                std::string text;
                for (const std::string& entry : _findings)
                    text += (text.empty() ? "" : "; ") + entry;
                return text;
            }

            // An expectation: met, it is said in the log; not met, the check fails.
            void expect(bool met, const std::string& what)
            {
                if (met)
                {
                    _rig.log.note("  as expected: " + what);
                    return;
                }
                _rig.log.note("  NOT MET: " + what);
                throw CheckFailure("not met: " + what);
            }

            [[nodiscard]] SessionConfig baseConfig() const
            {
                SessionConfig config;
                config.targetDeviceId = _rig.options.target.deviceId;
                if (!_rig.options.touchLcdSettings)
                    config.syncLcd = SettingChoice::Unchanged;
                return config;
            }

            // The close of a check that ends well: every setting the session changed was put back.
            void closeAndVerify(GuardedSession& guarded)
            {
                const Timed<CloseResult> closed = guarded.close();
                finding("closed after " + millisecondsText(closed.latency) + ", put back: " + settingsText(closed.result.restored));
                expect(closed.result.restoredAll(), "every setting the session changed was put back (not put back: "
                                                        + settingsText(closed.result.notRestored) + ")");
                expect(guarded.session().state() == SessionState::Closed, "the session is closed");
            }

            std::optional<Timed<akm::EchoResult>> echoOnce(GuardedSession& guarded, const std::array<std::uint8_t, ECHO_DATA_SIZE>& payload)
            {
                return awaitCompletion<akm::EchoResult>(_rig.driver, _rig.commandPatience(), [&guarded, &payload](EchoCompletion done) {
                    echo(guarded.session(), payload, std::move(done));
                });
            }

            static std::string echoText(const akm::EchoResult& result, const std::array<std::uint8_t, ECHO_DATA_SIZE>& payload)
            {
                if (result.succeeded())
                    return "REPLY " + hex(payload) + " as sent";
                if (result.mismatch)
                    return "MISMATCH sent " + hex(result.mismatch->sent) + ", received " + hexOrDash(result.mismatch->received);
                return outcomeText(result.outcome);
            }

            // An Echo that must come back right, said in the log.
            Clock::duration expectEcho(GuardedSession& guarded, const std::string& title,
                                       const std::array<std::uint8_t, ECHO_DATA_SIZE>& payload)
            {
                _rig.log.flush();
                const auto timed = echoOnce(guarded, payload);
                if (!timed)
                    throw CheckFailure(title + ": no completion within " + millisecondsText(_rig.commandPatience()) + ": the session lost it");
                finding(title + ": " + echoText(timed->result, payload) + " after " + millisecondsText(timed->latency));
                if (!timed->result.succeeded())
                    throw CheckFailure(title + ": " + echoText(timed->result, payload));
                return timed->latency;
            }

            // A command that must succeed, said in the log.
            void expectCommand(GuardedSession& guarded, const std::string& title, const std::function<void(Session&, CommandCompletion)>& launch)
            {
                _rig.log.flush();
                const auto timed = awaitCompletion<CommandResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded, &launch](CommandCompletion done) { launch(guarded.session(), std::move(done)); });
                if (!timed)
                    throw CheckFailure(title + ": no completion within " + millisecondsText(_rig.commandPatience()) + ": the session lost it");
                finding(title + ": " + outcomeText(timed->result) + " after " + millisecondsText(timed->latency));
                if (!succeeded(timed->result))
                    throw CheckFailure(title + ": " + outcomeText(timed->result));
            }

            // The OS version, read and said in the log; kept for the observations the first time.
            OsVersionReport expectOsVersion(GuardedSession& guarded, const std::string& title)
            {
                _rig.log.flush();
                const auto timed = awaitCompletion<OsVersionResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](OsVersionCompletion done) { queryOsVersion(guarded.session(), std::move(done)); });
                if (!timed)
                    throw CheckFailure(title + ": no completion within " + millisecondsText(_rig.commandPatience()) + ": the session lost it");
                if (!timed->result.version)
                    throw CheckFailure(title + ": " + outcomeText(timed->result.outcome));
                const OsVersionReport version = *timed->result.version;
                if (!_rig.result.osVersion)
                    _rig.result.osVersion = version;
                finding(title + ": OS " + std::to_string(version.major) + "." + std::to_string(version.minor)
                        + (version.subVersion ? ", sub-version " + std::to_string(*version.subVersion) : ", sub-version unavailable")
                        + " after " + millisecondsText(timed->latency));
                return version;
            }

            // RQ-AKM-039, RQ-AKM-040 and RQ-AKM-042 on the hardware: the opening the application uses, and the closing.
            void openAndClose()
            {
                GuardedSession guarded(_rig);
                const Timed<OpenResult> opened = guarded.open(baseConfig());
                finding("opened as " + std::string(describe(opened.result.status)) + " after " + millisecondsText(opened.latency)
                        + "; DeviceIDs that answered the discovery: " + listOfIds(opened.result.responders)
                        + (opened.result.unsupported.empty() ? "" : "; unsupported: " + settingsText(opened.result.unsupported)));
                expect(guarded.session().state() == SessionState::Open, "the session is open");
                expect(guarded.session().boundTarget() == std::optional<std::uint8_t>(static_cast<std::uint8_t>(_rig.options.target.deviceId)),
                       "the session is bound to the target DeviceID");
                expect(guarded.session().checksumMode() == ChecksumMode::Off, "the session knows the checksums are off");
                closeAndVerify(guarded);
            }

            // RQ-AKM-015, RQ-AKM-017: the Echo's REPLY equals the bytes sent.
            void echoReturnsTheBytes()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());
                static_cast<void>(expectEcho(guarded, "Echo " + hex(ECHO_PAYLOAD), ECHO_PAYLOAD));
                closeAndVerify(guarded);
            }

            // RQ-AKM-017, RQ-AKM-010, RQ-AKM-012: the latency of the Echo over repeated round trips, from which the
            // timeout and the discovery window are set.
            void echoLatencies()
            {
                if (_rig.options.echoRepeats <= 0)
                    throw CheckSkipped("no round trips were asked for");
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());
                for (int index = 0; index < _rig.options.echoRepeats; ++index)
                {
                    _rig.log.flush();
                    const std::array<std::uint8_t, ECHO_DATA_SIZE> payload = echoPayload(index);
                    const auto timed = echoOnce(guarded, payload);
                    if (!timed || !timed->result.succeeded())
                    {
                        const std::string reason = timed ? echoText(timed->result, payload) : std::string("no completion");
                        _rig.log.note("  round trip " + std::to_string(index + 1) + " failed: " + reason);
                        throw CheckFailure("round trip " + std::to_string(index + 1) + " of " + std::to_string(_rig.options.echoRepeats)
                                           + " failed: " + reason);
                    }
                    _rig.result.echoLatencies.push_back(timed->latency);
                    ++_rig.result.echoRoundTrips;
                }
                const LatencyStats stats = latencyStats(_rig.result.echoLatencies);
                finding(std::to_string(_rig.result.echoRoundTrips) + " Echo round trips: " + latencyText(stats));
                expect(stats.max < _rig.options.commandTimeout,
                       "the slowest round trip is shorter than the command timeout in use (" + millisecondsText(_rig.options.commandTimeout) + ")");
                closeAndVerify(guarded);
            }

            // RQ-AKM-044: the OS version, which tells the items the sampler has.
            void osVersion()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());
                static_cast<void>(expectOsVersion(guarded, "OS version (section 02, items 00 and 01)"));
                closeAndVerify(guarded);
            }

            // RQ-AKM-013, RQ-AKM-041: the checksum mode changes the framing, and the session follows it.
            void checksumsOnAndOff()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());
                expectCommand(guarded, "checksums on", [](Session& session, CommandCompletion done) { setChecksumMode(session, true, std::move(done)); });
                expect(guarded.session().checksumMode() == ChecksumMode::On, "the session follows the sampler: checksums on");
                static_cast<void>(expectEcho(guarded, "Echo with checksums on (the REPLY carries a checksum)", ECHO_PAYLOAD));
                static_cast<void>(expectOsVersion(guarded, "OS version with checksums on"));
                expectCommand(guarded, "checksums off again", [](Session& session, CommandCompletion done) { setChecksumMode(session, false, std::move(done)); });
                expect(guarded.session().checksumMode() == ChecksumMode::Off, "the session follows the sampler: checksums off");
                static_cast<void>(expectEcho(guarded, "Echo with checksums off again", ECHO_PAYLOAD));
                closeAndVerify(guarded);
            }

            // RQ-AKM-042 with every setting switched: the close puts back exactly what the open changed, and the
            // sampler accepts each restoring command.
            void closePutsBack()
            {
                SessionConfig config = baseConfig();
                config.checksums = true;
                config.notification = SettingChoice::Off;
                config.stillAlive = SettingChoice::On;
                std::vector<SamplerSetting> changed{SamplerSetting::Checksums, SamplerSetting::Notification, SamplerSetting::StillAlive};
                if (_rig.options.touchLcdSettings)
                {
                    config.syncLcd = SettingChoice::Off;
                    config.autoScreenUpdate = SettingChoice::On;
                    changed.push_back(SamplerSetting::SyncLcd);
                    changed.push_back(SamplerSetting::AutoScreenUpdate);
                }

                GuardedSession guarded(_rig);
                const Timed<OpenResult> opened = guarded.open(config);
                finding("opened as " + std::string(describe(opened.result.status)) + " with checksums on, Notification off, Still Alive on"
                        + (_rig.options.touchLcdSettings ? ", Sync LCD off and Auto screen update on" : "")
                        + (opened.result.unsupported.empty() ? "" : "; unsupported: " + settingsText(opened.result.unsupported)));
                expect(guarded.session().checksumMode() == ChecksumMode::On, "the session follows the checksum mode it established: on");
                for (const SamplerSetting unsupported : opened.result.unsupported)
                    changed.erase(std::remove(changed.begin(), changed.end(), unsupported), changed.end());

                const Timed<CloseResult> closed = guarded.close();
                finding("closed after " + millisecondsText(closed.latency) + ", put back: " + settingsText(closed.result.restored));
                expect(closed.result.restoredAll(), "every setting was put back (not put back: " + settingsText(closed.result.notRestored) + ")");
                expect(sameSettings(closed.result.restored, changed),
                       "the settings put back are the ones the open changed (" + settingsText(changed) + ")");
            }

            // RQ-AKM-018: a check that fails half way — its session open with the checksums on — leaves them off. The
            // failure is thrown through the scope of the guard, as a failed assertion would be.
            void failedCheckLeavesTheKnownState()
            {
                std::optional<CloseResult> closedByTheGuard;
                SessionConfig config = baseConfig();
                config.checksums = true;
                try
                {
                    GuardedSession guarded(_rig, &closedByTheGuard);
                    guarded.open(config);
                    expect(guarded.session().checksumMode() == ChecksumMode::On, "the session follows the checksum mode it established: on");
                    throw CheckFailure("this check fails on purpose, with the checksums on");
                }
                catch (const CheckFailure& failure)
                {
                    _rig.log.note(std::string("  the check failed: ") + failure.what());
                }
                expect(closedByTheGuard.has_value(), "the guard closed the session when the check failed");
                if (!closedByTheGuard)
                    return;
                finding("put back by the guard: " + settingsText(closedByTheGuard->restored));
                expect(contains(closedByTheGuard->restored, SamplerSetting::Checksums),
                       "the checksum mode was set off and the sampler confirmed it with a DONE");
                expect(closedByTheGuard->restoredAll(), "every setting was put back (not put back: " + settingsText(closedByTheGuard->notRestored) + ")");
            }

            // RQ-AKM-010, RQ-AKM-011, RQ-AKM-017: whether `F0 F7` reaches the host, and what the session does with it,
            // when the sampler works for a while. Only a slow command can show it, and the section 00 and 02 items are
            // not slow: this one sends "update the list of disks", which reads and changes nothing that is stored.
            void slowOperation()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());
                expect(guarded.session().stillAliveMonitoring(), "Still Alive is on: a received F0 F7 restarts the pending command's timeout");

                CommandRequest request;
                request.command = Command{SECTION_DISK_TOOLS, ITEM_UPDATE_DISK_LIST, {}};
                const Clock::duration patience = DEFAULT_MAX_TOTAL_WAIT + _rig.options.commandTimeout * COMMAND_PATIENCE_IN_TIMEOUTS;
                _rig.log.flush();
                const std::size_t stillAliveBefore = _rig.log.stillAliveMessages();
                const auto timed = awaitCompletion<CommandResult>(_rig.driver, patience, [&guarded, &request](CommandCompletion done) {
                    guarded.session().submit(request, std::move(done));
                });
                if (!timed)
                    throw CheckFailure("the command did not complete within " + millisecondsText(patience) + ": the session lost it");
                const std::size_t seen = _rig.log.stillAliveMessages() - stillAliveBefore;
                finding("update the list of disks (section 10, item 01): " + outcomeText(timed->result) + " after "
                        + millisecondsText(timed->latency) + "; F0 F7 messages that reached the host meanwhile: " + std::to_string(seen));

                if (std::holds_alternative<Timeout>(timed->result))
                    throw CheckFailure("timed out although Still Alive is on: the sampler sent no F0 F7 while it worked, or the backend did not deliver them");
                if (!succeeded(timed->result) && !std::holds_alternative<Error>(timed->result))
                    throw CheckFailure("the command ended as " + outcomeText(timed->result));
                if (seen > 0)
                    finding("F0 F7 reached the host: the backend delivers them, and the session did not time out");
                else if (std::holds_alternative<Error>(timed->result))
                    finding("the sampler refused the command: it provoked no F0 F7, nothing is observed");
                else if (timed->latency < STILL_ALIVE_PERIOD)
                    finding("the command was too fast to provoke an F0 F7 (the sampler sends one about every second): nothing is observed");
                else
                    finding("no F0 F7 came although the command took over a second: this operation may not send them, or the backend drops them");
                closeAndVerify(guarded);
            }

            // RQ-AKM-017 (what survives a power cycle), RQ-AKM-041 (a sampler that restarted): a session open with the
            // checksums on and Notification off goes on being used after the owner has power-cycled the sampler.
            void powerCycle()
            {
                if (!_rig.options.askOwner)
                    throw CheckSkipped("there is no way to ask the owner to power-cycle the sampler");
                SessionConfig config = baseConfig();
                config.checksums = true;
                config.notification = SettingChoice::Off;

                GuardedSession guarded(_rig);
                guarded.open(config);
                expect(guarded.session().checksumMode() == ChecksumMode::On, "the session follows the checksum mode it established: on");
                static_cast<void>(expectEcho(guarded, "Echo before the power cycle, checksums on and Notification off", ECHO_PAYLOAD));

                _rig.log.note("  asking the owner to power-cycle the sampler");
                _rig.log.flush();
                if (!_rig.options.askOwner("Switch the sampler off and on again, and wait until it has finished starting. The session stays open meanwhile."))
                    throw CheckSkipped("the owner declined to power-cycle the sampler");
                _rig.log.note("  the owner says the sampler is back");

                std::optional<int> answeredAt;
                for (int attempt = 1; attempt <= POWER_CYCLE_ATTEMPTS && !answeredAt; ++attempt)
                {
                    _rig.log.flush();
                    const std::array<std::uint8_t, ECHO_DATA_SIZE> payload = echoPayload(attempt);
                    const auto timed = echoOnce(guarded, payload);
                    const bool answered = timed && timed->result.succeeded();
                    finding("Echo " + std::to_string(attempt) + " after the power cycle: "
                            + (timed ? echoText(timed->result, payload) + " after " + millisecondsText(timed->latency) : std::string("no completion"))
                            + "; checksum mode as the session assumes it: " + modeName(guarded.session().checksumMode()));
                    if (answered)
                        answeredAt = attempt;
                }
                expect(answeredAt.has_value(), "the sampler answered an Echo again within " + std::to_string(POWER_CYCLE_ATTEMPTS) + " attempts");
                if (*answeredAt == 1 && guarded.session().checksumMode() == ChecksumMode::On)
                    finding("the checksum mode survived the power cycle: the first Echo was answered with checksums still on");
                else
                    finding("the checksum mode did not survive the power cycle: the session recovered at Echo " + std::to_string(*answeredAt)
                            + " (mode changes: " + _rig.diagnostics.modeChanges() + "); read the log for an OK before each REPLY to see whether Notification came back on");
                closeAndVerify(guarded);
            }

            Rig& _rig;
            std::vector<std::string> _findings;
            bool _noSampler = false;
        };

        void writeHeader(WireLog& log, const RealSuiteOptions& options)
        {
            log.note("XS56K AKM real-sampler suite");
            if (!options.startedAt.empty())
                log.note("started " + options.startedAt);
            log.note("target: in=\"" + options.target.inputPortName + "\" out=\"" + options.target.outputPortName
                     + "\" device-id=" + std::to_string(options.target.deviceId));
            log.note(std::string("Each check opens a session with Session::open and closes it with Session::close. The suite changes only section 00")
                     + " settings (checksums, Notification, Still Alive" + (options.touchLcdSettings ? ", Sync LCD, Auto screen update" : "")
                     + "), never a stored program, multi or sample, and leaves them as the closing does: checksums off, Still Alive off,"
                     + " Notification on" + (options.touchLcdSettings ? ", Sync LCD on, Auto screen update off." : "."));
            if (options.slowOperation)
                log.note("It also sends one command outside sections 00 and 02, asked for with --slow-operation: update the list of disks (section 10, item 01).");
            if (options.powerCycle)
                log.note("It also asks you to power-cycle the sampler while a session is open (--power-cycle).");
            log.note("The log is written between the steps, never while a command is in flight, so that writing it "
                     "cannot delay the exchanges it records: the times of the frames are those of the wire.");
        }
    }

    RealSuiteResult runRealSamplerSuite(common::midi::MidiBackend& backend, ScenarioDriver& driver, const RealSuiteOptions& options,
                                        std::ostream& out)
    {
        RealSuiteResult result;
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

        // Declared in the order they must outlive one another: the sessions of the checks are made and destroyed one at a
        // time inside the suite, so each is gone, its input stopped, before anything its callback touches is.
        LoggingInputPort loggedInput(*input, log);
        LoggingOutputPort loggedOutput(*output, log);
        ScenarioDiagnostics diagnostics(log);
        Rig rig{log, driver, loggedInput, loggedOutput, diagnostics, options, result};

        Suite suite(rig);
        suite.run();
        suite.observe();
        return result;
    }
}
