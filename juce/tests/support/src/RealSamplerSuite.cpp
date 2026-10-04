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
#include <cctype>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "ScenarioSupport.hpp"
#include "akm/Command.hpp"
#include "akm/CommandOptions.hpp"
#include "akm/CommandResult.hpp"
#include "akm/ItemCatalogue.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/DiskPrimitives.hpp"
#include "akm/FrontPanel.hpp"
#include "akm/KeygroupPrimitives.hpp"
#include "akm/MidiConfig.hpp"
#include "akm/MultiPrimitives.hpp"
#include "akm/ProgramPrimitives.hpp"
#include "akm/SamplePrimitives.hpp"
#include "akm/SceneListPrimitives.hpp"
#include "akm/SongPrimitives.hpp"
#include "akm/SysExConfig.hpp"
#include "akm/SystemSetup.hpp"
#include "akm/ZonePrimitives.hpp"
#include "akm/harness/ClockArithmetic.hpp"
#include "akm/harness/FrontPanelRemote.hpp"
#include "akm/harness/KeygroupParameterCases.hpp"
#include "akm/harness/MultiPartParameterCases.hpp"
#include "akm/harness/ProgramParameterCases.hpp"
#include "akm/harness/SampleParameterCases.hpp"
#include "akm/harness/WireFormat.hpp"
#include "akm/harness/WireLog.hpp"
#include "akm/harness/ZoneParameterCases.hpp"

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

        // Appends `src` to `dest` with a plain loop rather than `dest.insert(dest.end(), src.begin(),
        // src.end())`: GCC 11's Release build (-O2, -Werror) false-positives -Wstringop-overread on
        // that range-insert (linux-x64-release-canary on PR #4; not reproduced in Debug or on
        // MSVC/Clang) — a known GCC inlining bug, not a real out-of-bounds read.
        template <typename Container, typename Range>
        void appendAll(Container& dest, const Range& src)
        {
            for (const auto& element : src)
                dest.push_back(element);
        }

        // The scenario waits for what the session enforces the timeouts of itself, this many command timeouts: a wait
        // that runs out means a completion was lost, and is reported as such. An open is the discovery's window and
        // up to five commands, a close up to five (the first that times out ends it).
        constexpr int COMMAND_PATIENCE_IN_TIMEOUTS = 2;
        constexpr int OPEN_PATIENCE_IN_TIMEOUTS = 8;
        constexpr int CLOSE_PATIENCE_IN_TIMEOUTS = 4;
        // A key press is two commands, a Hold then a Release, each of which may take a whole timeout.
        constexpr int KEY_PRESS_PATIENCE_FACTOR = 2;

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

        // The keycodes of the keys a close did not release, as hex bytes, or "none".
        std::string keysText(const std::vector<std::uint8_t>& keycodes)
        {
            if (keycodes.empty())
                return "none";
            std::string text;
            for (const std::uint8_t keycode : keycodes)
                text += (text.empty() ? "" : ", ") + hex(std::array<std::uint8_t, 1>{keycode});
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

        std::string valuesText(const std::vector<std::int64_t>& values)
        {
            std::string text;
            for (const std::int64_t value : values)
                text += (text.empty() ? "" : ", ") + std::to_string(value);
            return "[" + text + "]";
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
                for (const std::uint8_t keycode : timed->result.keysReleased)
                    _rig.log.note("  released: front-panel key " + hex(std::array<std::uint8_t, 1>{keycode}));
                for (const std::uint8_t keycode : timed->result.keysNotReleased)
                    _rig.log.note("  NOT released: front-panel key " + hex(std::array<std::uint8_t, 1>{keycode}));
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

        // The reserved name every real-sampler test of this feature creates its program under (RQ-AKM-027); well
        // inside the 20-character limit observed on a real S5000 (documents/_index/sysex_spec.kb.md, "Common value
        // codes"), and unlikely to collide with a program already in the sampler's memory.
        constexpr std::string_view TEST_PROGRAM_NAME = "XS56K_SUITE_TEST";

        // A keygroup always has four zones (§06, spec Tables 9-10): the "1-4, or 0 for all four" domain
        // every zone item's first data byte carries. [RQ-AKM-034]
        constexpr int ZONE_COUNT = 4;

        // Wraps one program created under TEST_PROGRAM_NAME for the life of one check (RQ-AKM-027): on construction,
        // records the sampler's current program name, if any, then creates the test program, which Create makes
        // current. On destruction, even when the check throws half way, it reselects the test program by name and
        // deletes it, then reselects the program that was current before — logged, nothing let out of the
        // destructor, mirroring how GuardedSession always closes. `expectOnTestProgram` refuses a launch, without
        // sending it, when the current program is not the test one, so a check cannot act on a program the suite
        // did not create by mistake.
        class GuardedTestProgram
        {
        public:
            GuardedTestProgram(Rig& rig, Session& session) : _rig(rig), _session(session)
            {
                _originalName = currentProgramNameOrEmpty();
                expectCreated();
            }

            ~GuardedTestProgram()
            {
                try
                {
                    if (!isCurrent())
                        trySelectByName(std::string(TEST_PROGRAM_NAME), "reselect the test program before deleting it");
                    if (isCurrent())
                    {
                        const auto timed = awaitCompletion<CommandResult>(
                            _rig.driver, _rig.commandPatience(),
                            [this](CommandCompletion done) { deleteCurrentProgram(_session, std::move(done)); });
                        _rig.log.note(std::string("  test program deleted: ")
                                      + (timed && succeeded(timed->result) ? "done"
                                                                            : "failed ("
                                                                                  + timedOutcomeText(timed)
                                                                                  + ")"));
                    }
                    else
                        _rig.log.note("  the test program could not be reselected to delete it; it may already be gone");
                    if (_originalName)
                        trySelectByName(*_originalName, "reselect the program that was current before");
                }
                catch (...)  // NOLINT: a destructor does not throw
                {
                    _rig.log.note("  the test program guard could not fully restore the sampler; see the log above");
                }
            }

            GuardedTestProgram(const GuardedTestProgram&) = delete;
            GuardedTestProgram& operator=(const GuardedTestProgram&) = delete;

            [[nodiscard]] bool hadOriginalProgram() const { return _originalName.has_value(); }

            [[nodiscard]] bool isCurrent()
            {
                const auto name = currentProgramNameOrEmpty();
                return name && *name == std::string(TEST_PROGRAM_NAME);
            }

            // Navigates away from the test program to the one that was current before, to let a check prove
            // expectOnTestProgram refuses; false when there was none, or reselecting it failed.
            [[nodiscard]] bool selectOriginalProgram()
            {
                if (!_originalName)
                    return false;
                return trySelectByName(*_originalName, "select the original program (to prove the wrong-program refusal)");
            }

            [[nodiscard]] bool selectTestProgramAgain()
            {
                return trySelectByName(std::string(TEST_PROGRAM_NAME), "reselect the test program");
            }

            // Runs `launch` only when the test program is current; otherwise refuses before sending, and the check
            // fails. [RQ-AKM-027]
            void expectOnTestProgram(const std::string& title, const std::function<void(Session&, CommandCompletion)>& launch)
            {
                if (!isCurrent())
                    throw CheckFailure(title + ": refused before sending, the current program is not the test one");
                _rig.log.flush();
                const auto timed = awaitCompletion<CommandResult>(
                    _rig.driver, _rig.commandPatience(), [this, &launch](CommandCompletion done) { launch(_session, std::move(done)); });
                if (!timed)
                    throw CheckFailure(title + ": no completion within " + millisecondsText(_rig.commandPatience())
                                       + ": the session lost it");
                _rig.log.note("  " + title + ": " + outcomeText(timed->result) + " after " + millisecondsText(timed->latency));
                if (!succeeded(timed->result))
                    throw CheckFailure(title + ": " + outcomeText(timed->result));
            }

        private:
            [[nodiscard]] std::optional<std::string> currentProgramNameOrEmpty()
            {
                const auto timed = awaitCompletion<ProgramNameResult>(
                    _rig.driver, _rig.commandPatience(),
                    [this](ProgramNameCompletion done) { getCurrentProgramName(_session, std::move(done)); });
                if (!timed)
                    return std::nullopt;
                return timed->result.name;
            }

            void expectCreated()
            {
                _rig.log.flush();
                const auto timed = awaitCompletion<CommandResult>(_rig.driver, _rig.commandPatience(), [this](CommandCompletion done) {
                    createProgram(_session, std::string(TEST_PROGRAM_NAME), std::move(done));
                });
                if (!timed || !succeeded(timed->result))
                    throw CheckFailure("could not create the test program \"" + std::string(TEST_PROGRAM_NAME)
                                       + "\": " + timedOutcomeText(timed));
                _rig.log.note("  test program \"" + std::string(TEST_PROGRAM_NAME) + "\" created and current");
            }

            // Never throws: used both by the destructor and by the public navigation helpers, which report success
            // through their own return value instead.
            bool trySelectByName(const std::string& name, const std::string& title)
            {
                const auto timed = awaitCompletion<CommandResult>(_rig.driver, _rig.commandPatience(), [this, &name](CommandCompletion done) {
                    selectProgramByName(_session, name, std::move(done));
                });
                const bool ok = timed && succeeded(timed->result);
                _rig.log.note(std::string("  ") + title + ": "
                              + (ok ? "done" : "failed (" + timedOutcomeText(timed) + ")"));
                return ok;
            }

            Rig& _rig;
            Session& _session;
            std::optional<std::string> _originalName;
        };

        // The 8 settable §0E items (RQ-AKM-048), in the order &4B's grouped REPLY gives them — what
        // GuardedTestSample snapshots on entry (one &4B round trip) and restores on exit (one Set per
        // item, sliced from that snapshot by each item's own argument width).
        constexpr std::array<ItemId, 8> SETTABLE_PARAM_SET_IDS{{
            ItemId::SampleSetStartPosition, ItemId::SampleSetEndPosition, ItemId::SampleSetOriginalPitch,
            ItemId::SampleSetSemitoneTune, ItemId::SampleSetFineTune, ItemId::SampleSetPlaybackMode,
            ItemId::SampleSetLoopStart, ItemId::SampleSetLoopEnd,
        }};

        // Mirrors GuardedTestProgram, but restores rather than deletes: unlike a program, a real sample
        // cannot be thrown away and recreated (§0E has no "create" item, RQ-AKM-045's own note), so the
        // guard snapshots the operator's sample on entry (its settable parameters, one &4B round trip)
        // and puts them back on exit, however the check ends. `sampleName` is fixed for the whole
        // guard's lifetime: a check that renames the sample renames it back to `sampleName` itself,
        // before anything that might throw, the same discipline `programLifecycleOnTestProgram` already
        // follows for the reserved program name. Never sends `&07`/`&08` — nothing in this class can,
        // there is no method that does. [RQ-AKM-051]
        class GuardedTestSample
        {
        public:
            GuardedTestSample(Rig& rig, Session& session, std::string sampleName)
                : _rig(rig), _session(session), _sampleName(std::move(sampleName))
            {
                _originalCurrentName = currentSampleNameOrEmpty();
                expectSelected();
                _originalParameters = expectSettableParameters();
            }

            ~GuardedTestSample()
            {
                try
                {
                    if (!isCurrent())
                        trySelectByName(_sampleName, "reselect the test sample before restoring it");
                    if (isCurrent())
                        restoreParameters();
                    else
                        _rig.log.note("  the test sample could not be reselected to restore it; it may have been renamed");
                    if (hadOriginalSample())
                        trySelectByName(*_originalCurrentName, "reselect the sample that was current before");
                }
                catch (...)  // NOLINT: a destructor does not throw
                {
                    _rig.log.note("  the test sample guard could not fully restore the sampler; see the log above");
                }
            }

            GuardedTestSample(const GuardedTestSample&) = delete;
            GuardedTestSample& operator=(const GuardedTestSample&) = delete;

            [[nodiscard]] bool hadOriginalSample() const
            {
                return _originalCurrentName.has_value() && *_originalCurrentName != _sampleName;
            }

            // Navigates away from the test sample to the one that was current before, to let a check prove
            // expectOnTestSample refuses; false when there was none, or reselecting it failed.
            [[nodiscard]] bool selectOriginalSample()
            {
                if (!hadOriginalSample())
                    return false;
                return trySelectByName(*_originalCurrentName,
                                       "select the sample that was current before (to prove the wrong-sample refusal)");
            }

            [[nodiscard]] bool selectTestSampleAgain() { return trySelectByName(_sampleName, "reselect the test sample"); }

            /// The settable parameters read on construction, before the check changes anything — kept so the
            /// check can verify, after this guard is gone, that they made it back (the guard's own restore
            /// already used this snapshot internally; this is the same data, exposed for that verification).
            [[nodiscard]] const std::vector<std::int64_t>& originalParameters() const { return _originalParameters; }

            // Runs `launch` only when the test sample is current; otherwise refuses before sending, and the check
            // fails. [RQ-AKM-051]
            void expectOnTestSample(const std::string& title, const std::function<void(Session&, CommandCompletion)>& launch)
            {
                if (!isCurrent())
                    throw CheckFailure(title + ": refused before sending, the current sample is not the test one");
                _rig.log.flush();
                const auto timed = awaitCompletion<CommandResult>(
                    _rig.driver, _rig.commandPatience(), [this, &launch](CommandCompletion done) { launch(_session, std::move(done)); });
                if (!timed)
                    throw CheckFailure(title + ": no completion within " + millisecondsText(_rig.commandPatience())
                                       + ": the session lost it");
                _rig.log.note("  " + title + ": " + outcomeText(timed->result) + " after " + millisecondsText(timed->latency));
                if (!succeeded(timed->result))
                    throw CheckFailure(title + ": " + outcomeText(timed->result));
            }

        private:
            [[nodiscard]] bool isCurrent()
            {
                const auto name = currentSampleNameOrEmpty();
                return name && *name == _sampleName;
            }

            [[nodiscard]] std::optional<std::string> currentSampleNameOrEmpty()
            {
                const auto timed = awaitCompletion<SampleNameResult>(
                    _rig.driver, _rig.commandPatience(),
                    [this](SampleNameCompletion done) { getCurrentSampleName(_session, std::move(done)); });
                if (!timed)
                    return std::nullopt;
                return timed->result.name;
            }

            void expectSelected()
            {
                _rig.log.flush();
                const auto timed = awaitCompletion<CommandResult>(_rig.driver, _rig.commandPatience(), [this](CommandCompletion done) {
                    selectSampleByName(_session, _sampleName, std::move(done));
                });
                if (!timed || !succeeded(timed->result))
                    throw CheckFailure("could not select the test sample \"" + _sampleName
                                       + "\": " + timedOutcomeText(timed)
                                       + " (is it really in the sampler's memory?)");
                _rig.log.note("  test sample \"" + _sampleName + "\" selected and current");
            }

            // One &4B round trip: the 8 settable parameters this guard restores, in the order the spec's
            // grouped REPLY gives them (RQ-AKM-049).
            [[nodiscard]] std::vector<std::int64_t> expectSettableParameters()
            {
                const auto timed = awaitCompletion<CommandResult>(_rig.driver, _rig.commandPatience(), [this](CommandCompletion done) {
                    _session.submit(makeRequest(ItemId::SampleGetAllSettableParams, {}), std::move(done));
                });
                if (!timed || !succeeded(timed->result))
                    throw CheckFailure("could not read the test sample's settable parameters before changing them: "
                                       + timedOutcomeText(timed));
                const auto* replyData = std::get_if<Reply>(&timed->result);
                const auto decoded = replyData ? decodeReply(ItemId::SampleGetAllSettableParams, replyData->data) : std::nullopt;
                if (!decoded)
                    throw CheckFailure("could not decode the test sample's settable parameters before changing them");
                return *decoded;
            }

            // Never throws: used both by the destructor and by the public navigation helpers, which report success
            // through their own return value instead.
            bool trySelectByName(const std::string& name, const std::string& title)
            {
                const auto timed = awaitCompletion<CommandResult>(_rig.driver, _rig.commandPatience(), [this, &name](CommandCompletion done) {
                    selectSampleByName(_session, name, std::move(done));
                });
                const bool ok = timed && succeeded(timed->result);
                _rig.log.note(std::string("  ") + title + ": "
                              + (ok ? "done" : "failed (" + timedOutcomeText(timed) + ")"));
                return ok;
            }

            // Sets each of the 8 settable parameters back to `_originalParameters`, one Set per item, logged, best
            // effort — a failed restore is noted, not thrown (the destructor must not throw). Offsets into
            // `_originalParameters` follow `SETTABLE_PARAM_SET_IDS` (the wire/&4B order, fixed by the protocol),
            // but the Sets are *sent* with Loop End before Loop Start: on the real S5000 (2026-09-30, samples
            // "AMEN" and "Honesty"), Loop Start reproducibly read back wrong whenever Loop End was set after it
            // — see SampleParameterCases.cpp for the detail — so nothing is sent after Loop Start's own restore
            // that could disturb it again.
            void restoreParameters()
            {
                struct PendingRestore
                {
                    ItemId setId;
                    std::vector<std::int64_t> value;
                };
                std::vector<PendingRestore> pending;
                std::size_t offset = 0;
                for (const ItemId setId : SETTABLE_PARAM_SET_IDS)
                {
                    const std::size_t width = descriptor(setId).args.size();
                    if (offset + width > _originalParameters.size())
                    {
                        _rig.log.note("  could not restore the test sample's settable parameters: snapshot too short");
                        return;
                    }
                    pending.push_back({setId, std::vector<std::int64_t>(_originalParameters.begin() + static_cast<std::ptrdiff_t>(offset),
                                                                        _originalParameters.begin() + static_cast<std::ptrdiff_t>(offset + width))});
                    offset += width;
                }
                static_assert(SETTABLE_PARAM_SET_IDS[6] == ItemId::SampleSetLoopStart);
                static_assert(SETTABLE_PARAM_SET_IDS[7] == ItemId::SampleSetLoopEnd);
                std::swap(pending[6], pending[7]);
                for (const PendingRestore& item : pending)
                {
                    const auto timed = awaitCompletion<CommandResult>(
                        _rig.driver, _rig.commandPatience(), [this, &item](CommandCompletion done) {
                            _session.submit(makeRequest(item.setId, item.value), std::move(done));
                        });
                    const bool ok = timed && succeeded(timed->result);
                    _rig.log.note("  restore " + std::string(descriptor(item.setId).name) + ": " + (ok ? "done" : "failed"));
                }
            }

            Rig& _rig;
            Session& _session;
            std::string _sampleName;
            std::optional<std::string> _originalCurrentName;
            std::vector<std::int64_t> _originalParameters;
        };

        // The values the system setup check changes (RQ-AKM-058) and the instant the clock was read.
        struct SystemSetupSnapshot
        {
            std::string name;
            PlayMode playMode{};
            FrontPanelLock lock{};
            /// Empty when the clock could not be read — then it is neither set nor restored, and `clockProblem` says why.
            /// (The S5000 of the first real run, OS 2.14, answered &05 with a section byte of 0B in place of 02,
            /// which the session does not take for the answer to its command: RQ-AKM-007.)
            std::optional<ClockDate> clock{};
            std::string clockProblem;
            Clock::time_point clockReadAt{};
        };

        // The name the check gives the sampler for the length of a round trip: inside the 20 characters the
        // catalogue allows, and nothing an owner would keep. [RQ-AKM-052, RQ-AKM-058]
        constexpr std::string_view TEST_SAMPLER_NAME = "XS56K TEST";

        // A clock nothing real shows — Saturday 15 June 2030, 08:05:09 — whose year needs both data bytes of the
        // compound word, and whose weekday (7, 1 = Sunday) is the one of its date. [RQ-AKM-054, RQ-AKM-058]
        constexpr ClockDate TEST_CLOCK{2030, 6, 15, 7, 8, 5, 9};

        // The clock is read to the second and every command takes some milliseconds, so a clock that reads within
        // this many seconds of "what it was, advanced by the time elapsed" is a clock that was put back.
        // [RQ-AKM-058]
        constexpr std::int64_t CLOCK_RESTORE_TOLERANCE_SECONDS = 3;
        constexpr std::int64_t MILLISECONDS_PER_SECOND = 1000;

        std::string twoDigits(int value)
        {
            return (value < 10 ? "0" : "") + std::to_string(value);
        }

        std::string clockText(const ClockDate& clock)
        {
            return std::to_string(clock.year) + "-" + twoDigits(clock.month) + "-" + twoDigits(clock.day) + " "
                   + twoDigits(clock.hours) + ":" + twoDigits(clock.minutes) + ":" + twoDigits(clock.seconds)
                   + " (day of week " + std::to_string(clock.dayOfWeek) + ")";
        }

        std::string playModeName(PlayMode mode)
        {
            switch (mode)
            {
                case PlayMode::Multi:
                    return "Multi";
                case PlayMode::Program:
                    return "Program";
                case PlayMode::Sample:
                    return "Sample";
                case PlayMode::Muted:
                    return "Muted";
            }
            return "unknown";
        }

        std::string lockName(FrontPanelLock lock)
        {
            return lock == FrontPanelLock::Locked ? "locked" : "normal";
        }

        // The whole seconds that passed since `since`, on the scenario's clock.
        std::int64_t elapsedSeconds(Rig& rig, Clock::time_point since)
        {
            const std::int64_t milliseconds = millisecondsOf(rig.driver.scheduler().now() - since);
            return (milliseconds + MILLISECONDS_PER_SECOND / 2) / MILLISECONDS_PER_SECOND;
        }

        // Reads the four values the system setup check changes: the name, the Play Mode, the front-panel lock and the
        // clock. It changes nothing; `problem` says which of the first three failed to be read. The clock is allowed to
        // fail: it is then left out of the check, and the snapshot says why. [RQ-AKM-052, RQ-AKM-054, RQ-AKM-055]
        std::optional<SystemSetupSnapshot> readSystemSetup(Rig& rig, Session& session, std::string& problem)
        {
            SystemSetupSnapshot snapshot;
            const auto name = awaitCompletion<SamplerNameResult>(
                rig.driver, rig.commandPatience(), [&session](SamplerNameCompletion done) { getSamplerName(session, std::move(done)); });
            if (!name || !name->result.name)
            {
                problem = "could not read the sampler's name: " + (name ? outcomeText(name->result.outcome) : std::string("no completion"));
                return std::nullopt;
            }
            snapshot.name = *name->result.name;

            const auto mode = awaitCompletion<PlayModeResult>(
                rig.driver, rig.commandPatience(), [&session](PlayModeCompletion done) { getPlayMode(session, std::move(done)); });
            if (!mode || !mode->result.mode)
            {
                problem = "could not read the Play Mode: " + (mode ? outcomeText(mode->result.outcome) : std::string("no completion"));
                return std::nullopt;
            }
            snapshot.playMode = *mode->result.mode;

            const auto lock = awaitCompletion<FrontPanelLockResult>(
                rig.driver, rig.commandPatience(), [&session](FrontPanelLockCompletion done) { getFrontPanelLock(session, std::move(done)); });
            if (!lock || !lock->result.lock)
            {
                problem = "could not read the front-panel lock: " + (lock ? outcomeText(lock->result.outcome) : std::string("no completion"));
                return std::nullopt;
            }
            snapshot.lock = *lock->result.lock;

            const auto clock = awaitCompletion<ClockDateResult>(
                rig.driver, rig.commandPatience(), [&session](ClockDateCompletion done) { getClockDate(session, std::move(done)); });
            if (!clock || !clock->result.clock)
            {
                snapshot.clockProblem = "could not read the clock: "
                                        + (clock ? outcomeText(clock->result.outcome) : std::string("no completion"));
                return snapshot;
            }
            snapshot.clock = *clock->result.clock;
            snapshot.clockReadAt = rig.driver.scheduler().now();
            return snapshot;
        }

        // Wraps the sampler's system setup for the life of one check (RQ-AKM-058): on construction it reads the
        // name, the Play Mode, the front-panel lock and the clock, before anything is changed; on destruction, even
        // when the check throws half way, it puts each back — the lock first, so that the front panel is never left
        // locked by a check that fails, then the Play Mode, the name and the clock, advanced by the time elapsed
        // since it was read. Logged, best effort, nothing let out of the destructor, mirroring GuardedTestSample.
        // Never sends §02/&32 (Clear Sampler Memory): nothing in this class can, there is no method that does.
        class GuardedSystemSetup
        {
        public:
            GuardedSystemSetup(Rig& rig, Session& session) : _rig(rig), _session(session)
            {
                std::string problem;
                const auto snapshot = readSystemSetup(rig, session, problem);
                if (!snapshot)
                    throw CheckFailure(problem + " (nothing was changed)");
                _original = *snapshot;
                _rig.log.note("  system setup read before any change: name \"" + _original.name + "\", Play Mode "
                              + playModeName(_original.playMode) + ", front panel " + lockName(_original.lock) + ", clock "
                              + (_original.clock ? clockText(*_original.clock) : "unreadable (" + _original.clockProblem + ")"));
            }

            ~GuardedSystemSetup()
            {
                try
                {
                    restore();
                }
                catch (...)  // NOLINT: a destructor does not throw
                {
                    _rig.log.note("  the system setup guard could not fully restore the sampler; see the log above");
                }
            }

            GuardedSystemSetup(const GuardedSystemSetup&) = delete;
            GuardedSystemSetup& operator=(const GuardedSystemSetup&) = delete;

            /// What was read on construction, before the check changed anything.
            [[nodiscard]] const SystemSetupSnapshot& original() const { return _original; }

        private:
            template <typename Launch>
            void restoreStep(const std::string& title, Launch launch)
            {
                const auto timed = awaitCompletion<CommandResult>(_rig.driver, _rig.commandPatience(), launch);
                const bool ok = timed && succeeded(timed->result);
                _rig.log.note("  restore " + title + ": "
                              + (ok ? "done" : "failed (" + timedOutcomeText(timed) + ")"));
            }

            void restore()
            {
                restoreStep("the front panel (" + lockName(_original.lock) + ")", [this](CommandCompletion done) {
                    setFrontPanelLock(_session, _original.lock, std::move(done));
                });
                restoreStep("the Play Mode (" + playModeName(_original.playMode) + ")", [this](CommandCompletion done) {
                    setPlayMode(_session, _original.playMode, std::move(done));
                });
                restoreStep("the sampler's name (\"" + _original.name + "\")", [this](CommandCompletion done) {
                    setSamplerName(_session, _original.name, std::move(done));
                });
                if (!_original.clock)
                {
                    _rig.log.note("  the clock was not read, so it was never changed and is not restored");
                    return;
                }
                const std::int64_t elapsed = elapsedSeconds(_rig, _original.clockReadAt);
                const ClockDate target = addSeconds(*_original.clock, elapsed);
                restoreStep("the clock (" + clockText(target) + ", advanced by " + std::to_string(elapsed) + " s)",
                            [this, &target](CommandCompletion done) { setClockDate(_session, target, std::move(done)); });
            }

            Rig& _rig;
            Session& _session;
            SystemSetupSnapshot _original;
        };

        // The name the check gives a song file or a set list for the length of a round trip: inside the 20 characters
        // the other names allow, and nothing an owner would keep. [RQ-AKM-085]
        constexpr std::string_view TEST_SONG_FILES_NAME = "XS56K TEST";

        // How many names of each kind the check reads: a sampler may hold far more than a log needs to show.
        // [RQ-AKM-085]
        constexpr int SONG_FILES_NAME_READ_LIMIT = 16;

        // What the sampler holds in section 16, as read before a check changes anything: the two counts, the first names
        // of each kind and the current song file. `currentProblem` says why no song file is current (the sampler answers an
        // ERROR when none is). [RQ-AKM-083, RQ-AKM-084, RQ-AKM-085]
        struct SongFilesSnapshot
        {
            int songCount = 0;
            int setListCount = 0;
            std::vector<std::string> songNames;
            std::vector<std::string> setListNames;
            std::optional<int> currentSong{};
            std::string currentProblem;
        };

        std::string songFilesText(const SongFilesSnapshot& snapshot)
        {
            const auto list = [](const std::vector<std::string>& names) {
                std::string text;
                for (const std::string& name : names)
                    text += (text.empty() ? "\"" : ", \"") + name + "\"";
                return text.empty() ? std::string("none") : text;
            };
            return std::to_string(snapshot.songCount) + " song file(s): " + list(snapshot.songNames) + "; "
                   + std::to_string(snapshot.setListCount) + " set list(s): " + list(snapshot.setListNames) + "; current song file "
                   + (snapshot.currentSong ? std::to_string(*snapshot.currentSong) : "none (" + snapshot.currentProblem + ")");
        }

        // Reads the counts, the first names and the current song file. It changes nothing; `problem` says which read failed.
        // [RQ-AKM-083, RQ-AKM-084]
        std::optional<SongFilesSnapshot> readSongFiles(Rig& rig, Session& session, std::string& problem)
        {
            SongFilesSnapshot snapshot;
            const auto songs = awaitCompletion<SongCountResult>(
                rig.driver, rig.commandPatience(), [&session](SongCountCompletion done) { getSongCount(session, std::move(done)); });
            if (!songs || !songs->result.count)
            {
                problem = "could not read the number of song files (&10): " + (songs ? outcomeText(songs->result.outcome) : std::string("no completion"));
                return std::nullopt;
            }
            snapshot.songCount = *songs->result.count;

            const auto setLists = awaitCompletion<SetListCountResult>(
                rig.driver, rig.commandPatience(), [&session](SetListCountCompletion done) { getSetListCount(session, std::move(done)); });
            if (!setLists || !setLists->result.count)
            {
                problem = "could not read the number of set lists (&20): " + (setLists ? outcomeText(setLists->result.outcome) : std::string("no completion"));
                return std::nullopt;
            }
            snapshot.setListCount = *setLists->result.count;

            for (int index = 0; index < std::min(snapshot.songCount, SONG_FILES_NAME_READ_LIMIT); ++index)
            {
                const auto name = awaitCompletion<SongNameResult>(rig.driver, rig.commandPatience(), [&session, index](SongNameCompletion done) {
                    getSongNameByIndex(session, index, std::move(done));
                });
                if (!name || !name->result.name)
                {
                    problem = "could not read the name of song file " + std::to_string(index) + " (&11): "
                              + (name ? outcomeText(name->result.outcome) : std::string("no completion"));
                    return std::nullopt;
                }
                snapshot.songNames.push_back(*name->result.name);
            }
            for (int index = 0; index < std::min(snapshot.setListCount, SONG_FILES_NAME_READ_LIMIT); ++index)
            {
                const auto name = awaitCompletion<SetListNameResult>(rig.driver, rig.commandPatience(), [&session, index](SetListNameCompletion done) {
                    getSetListNameByIndex(session, index, std::move(done));
                });
                if (!name || !name->result.name)
                {
                    problem = "could not read the name of set list " + std::to_string(index) + " (&21): "
                              + (name ? outcomeText(name->result.outcome) : std::string("no completion"));
                    return std::nullopt;
                }
                snapshot.setListNames.push_back(*name->result.name);
            }

            const auto current = awaitCompletion<SongIndexResult>(
                rig.driver, rig.commandPatience(), [&session](SongIndexCompletion done) { getCurrentSongIndex(session, std::move(done)); });
            if (!current)
            {
                problem = "could not read the current song file's index (&13): no completion";
                return std::nullopt;
            }
            if (current->result.index)
                snapshot.currentSong = *current->result.index;
            else
                snapshot.currentProblem = outcomeText(current->result.outcome);
            return snapshot;
        }

        // Wraps the sampler's song files and set lists for the life of one check (RQ-AKM-085): the check says which name it
        // is about to change, before it changes it, and on destruction, even when the check throws half way, the guard puts
        // each such name back and selects again the song file that was current. Logged, best effort, nothing let out of the
        // destructor, mirroring GuardedSystemSetup. It never sends a deletion: nothing in this class can, there is no method
        // that does. A sampler that had no song file current cannot be put back to that: the selection has no "none".
        class GuardedSongFiles
        {
        public:
            GuardedSongFiles(Rig& rig, Session& session, SongFilesSnapshot original)
                : _rig(rig), _session(session), _original(std::move(original))
            {
            }

            ~GuardedSongFiles()
            {
                try
                {
                    restore();
                }
                catch (...)  // NOLINT: a destructor does not throw
                {
                    _rig.log.note("  the song files guard could not fully restore the sampler; see the log above");
                }
            }

            GuardedSongFiles(const GuardedSongFiles&) = delete;
            GuardedSongFiles& operator=(const GuardedSongFiles&) = delete;

            /// To call before the rename of song file `index` is sent: its name is put back whatever happens next.
            void noteSongRenamed(int index) { _renamedSong = index; }
            /// To call before the rename of set list `index` is sent.
            void noteSetListRenamed(int index) { _renamedSetList = index; }

        private:
            template <typename Launch>
            void restoreStep(const std::string& title, Launch launch)
            {
                const auto timed = awaitCompletion<CommandResult>(_rig.driver, _rig.commandPatience(), launch);
                const bool ok = timed && succeeded(timed->result);
                _rig.log.note("  restore " + title + ": " + (ok ? "done" : "failed (" + timedOutcomeText(timed) + ")"));
            }

            void restore()
            {
                if (_renamedSetList)
                {
                    const int index = *_renamedSetList;
                    const std::string name = _original.setListNames.at(static_cast<std::size_t>(index));
                    restoreStep("the name of set list " + std::to_string(index) + " (\"" + name + "\")",
                                [this, index, &name](CommandCompletion done) { renameSetList(_session, index, name, std::move(done)); });
                }
                if (_renamedSong)
                {
                    const int index = *_renamedSong;
                    const std::string name = _original.songNames.at(static_cast<std::size_t>(index));
                    restoreStep("the selection of song file " + std::to_string(index),
                                [this, index](CommandCompletion done) { selectSongByIndex(_session, index, std::move(done)); });
                    restoreStep("the name of song file " + std::to_string(index) + " (\"" + name + "\")",
                                [this, &name](CommandCompletion done) { renameCurrentSong(_session, name, std::move(done)); });
                }
                if (_original.currentSong)
                {
                    const int index = *_original.currentSong;
                    restoreStep("the current song file (" + std::to_string(index) + ")",
                                [this, index](CommandCompletion done) { selectSongByIndex(_session, index, std::move(done)); });
                }
                else if (_renamedSong)
                {
                    _rig.log.note("  no song file was current before the check, and a selection cannot be cleared: song file "
                                  + std::to_string(*_renamedSong) + " stays current");
                }
            }

            Rig& _rig;
            Session& _session;
            SongFilesSnapshot _original;
            std::optional<int> _renamedSong{};
            std::optional<int> _renamedSetList{};
        };

        // The name the check gives a scenelist for the length of a round trip: inside the 20 characters the other names
        // allow, and nothing an owner would keep. How many names it reads: a sampler may hold far more than a log needs to
        // show. [RQ-AKM-097]
        constexpr std::string_view TEST_SCENE_LIST_NAME = "XS56K TEST";
        constexpr int SCENE_LIST_NAME_READ_LIMIT = 16;

        // What the sampler holds in section 14, as read before a check changes anything: the count, the first names and the
        // current scenelist. `currentProblem` says why no scenelist is current (the sampler answers an ERROR when none is).
        // [RQ-AKM-096, RQ-AKM-097]
        struct SceneListsSnapshot
        {
            int count = 0;
            std::vector<std::string> names;
            std::optional<int> current{};
            std::string currentProblem;
        };

        std::string sceneListsText(const SceneListsSnapshot& snapshot)
        {
            std::string list;
            for (const std::string& name : snapshot.names)
                list += (list.empty() ? "\"" : ", \"") + name + "\"";
            return std::to_string(snapshot.count) + " scenelist(s): " + (list.empty() ? std::string("none") : list)
                   + "; current scenelist "
                   + (snapshot.current ? std::to_string(*snapshot.current) : "none (" + snapshot.currentProblem + ")");
        }

        // Reads the count, the first names and the current scenelist. It changes nothing; `problem` says which read failed.
        // [RQ-AKM-096]
        std::optional<SceneListsSnapshot> readSceneLists(Rig& rig, Session& session, std::string& problem)
        {
            SceneListsSnapshot snapshot;
            const auto count = awaitCompletion<SceneListCountResult>(
                rig.driver, rig.commandPatience(), [&session](SceneListCountCompletion done) { getSceneListCount(session, std::move(done)); });
            if (!count || !count->result.count)
            {
                problem = "could not read the number of scenelists (&10): " + (count ? outcomeText(count->result.outcome) : std::string("no completion"));
                return std::nullopt;
            }
            snapshot.count = *count->result.count;

            for (int index = 0; index < std::min(snapshot.count, SCENE_LIST_NAME_READ_LIMIT); ++index)
            {
                const auto name = awaitCompletion<SceneListNameResult>(rig.driver, rig.commandPatience(), [&session, index](SceneListNameCompletion done) {
                    getSceneListNameByIndex(session, index, std::move(done));
                });
                if (!name || !name->result.name)
                {
                    problem = "could not read the name of scenelist " + std::to_string(index) + " (&11): "
                              + (name ? outcomeText(name->result.outcome) : std::string("no completion"));
                    return std::nullopt;
                }
                snapshot.names.push_back(*name->result.name);
            }

            const auto current = awaitCompletion<SceneListIndexResult>(
                rig.driver, rig.commandPatience(), [&session](SceneListIndexCompletion done) { getCurrentSceneListIndex(session, std::move(done)); });
            if (!current)
            {
                problem = "could not read the current scenelist's index (&13): no completion";
                return std::nullopt;
            }
            if (current->result.index)
                snapshot.current = *current->result.index;
            else
                snapshot.currentProblem = outcomeText(current->result.outcome);
            return snapshot;
        }

        // Wraps the sampler's scenelists for the life of one check (RQ-AKM-097): the check says which scenelist it is about to
        // rename, before it renames it, and on destruction, even when the check throws half way, the guard puts the name back
        // and selects again the scenelist that was current. Logged, best effort, nothing let out of the destructor, as
        // GuardedSongFiles. It never sends a deletion: nothing in this class can. A sampler that had no scenelist current
        // cannot be put back to that: the selection has no "none".
        class GuardedSceneLists
        {
        public:
            GuardedSceneLists(Rig& rig, Session& session, SceneListsSnapshot original)
                : _rig(rig), _session(session), _original(std::move(original))
            {
            }

            ~GuardedSceneLists()
            {
                try
                {
                    restore();
                }
                catch (...)  // NOLINT: a destructor does not throw
                {
                    _rig.log.note("  the scenelists guard could not fully restore the sampler; see the log above");
                }
            }

            GuardedSceneLists(const GuardedSceneLists&) = delete;
            GuardedSceneLists& operator=(const GuardedSceneLists&) = delete;

            /// To call before the rename of scenelist `index` is sent: its name is put back whatever happens next.
            void noteRenamed(int index) { _renamed = index; }

        private:
            template <typename Launch>
            void restoreStep(const std::string& title, Launch launch)
            {
                const auto timed = awaitCompletion<CommandResult>(_rig.driver, _rig.commandPatience(), launch);
                const bool ok = timed && succeeded(timed->result);
                _rig.log.note("  restore " + title + ": " + (ok ? "done" : "failed (" + timedOutcomeText(timed) + ")"));
            }

            void restore()
            {
                if (_renamed)
                {
                    const int index = *_renamed;
                    const std::string name = _original.names.at(static_cast<std::size_t>(index));
                    restoreStep("the selection of scenelist " + std::to_string(index),
                                [this, index](CommandCompletion done) { selectSceneListByIndex(_session, index, std::move(done)); });
                    restoreStep("the name of scenelist " + std::to_string(index) + " (\"" + name + "\")",
                                [this, &name](CommandCompletion done) { renameCurrentSceneList(_session, name, std::move(done)); });
                }
                if (_original.current)
                {
                    const int index = *_original.current;
                    restoreStep("the current scenelist (" + std::to_string(index) + ")",
                                [this, index](CommandCompletion done) { selectSceneListByIndex(_session, index, std::move(done)); });
                }
                else if (_renamed)
                {
                    _rig.log.note("  no scenelist was current before the check, and a selection cannot be cleared: scenelist "
                                  + std::to_string(*_renamed) + " stays current");
                }
            }

            Rig& _rig;
            Session& _session;
            SceneListsSnapshot _original;
            std::optional<int> _renamed{};
        };

        // The reserved names the multi check creates its multi and renames it under (RQ-AKM-093), and the two parts it works
        // on: the part the shared parameter cases act on, and one more for the assignment by index. Inside the 20
        // characters the other names allow, and nothing an owner would keep.
        constexpr std::string_view TEST_MULTI_NAME = "XS56K_MULTI_TEST";
        constexpr std::string_view TEST_MULTI_RENAMED = "XS56K_MULTI_TEST2";
        constexpr int TEST_MULTI_PART = 3;
        constexpr int TEST_MULTI_PART_BY_INDEX = 4;
        constexpr int PROGRAM_NUMBER_FOR_THE_TEST = 5;

        // The sampler's multis as read before a check creates its own: their names in memory order and the current one
        // (`currentIndex` is empty when none is). [RQ-AKM-091, RQ-AKM-093]
        struct MultisSnapshot
        {
            std::vector<std::string> names;
            std::optional<int> currentIndex{};
            std::string currentProblem;
        };

        std::string multisText(const MultisSnapshot& snapshot)
        {
            std::string text = std::to_string(snapshot.names.size()) + " multi(s)";
            for (const std::string& name : snapshot.names)
                text += std::string(text.find(':') == std::string::npos ? ": \"" : ", \"") + name + "\"";
            return text + "; current multi "
                   + (snapshot.currentIndex ? std::to_string(*snapshot.currentIndex) : "none (" + snapshot.currentProblem + ")");
        }

        // Reads the names of all the multis (an empty list is a normal answer) and the current one. It changes nothing.
        std::optional<MultisSnapshot> readMultis(Rig& rig, Session& session, std::string& problem)
        {
            MultisSnapshot snapshot;
            const auto names = awaitCompletion<MultiNameListResult>(
                rig.driver, rig.commandPatience(), [&session](MultiNameListCompletion done) { getAllMultiNames(session, std::move(done)); });
            if (!names || !names->result.names)
            {
                problem = "could not read the names of all multis (&51): " + (names ? outcomeText(names->result.outcome) : std::string("no completion"));
                return std::nullopt;
            }
            snapshot.names = *names->result.names;
            const auto current = awaitCompletion<MultiIndexResult>(
                rig.driver, rig.commandPatience(), [&session](MultiIndexCompletion done) { getCurrentMultiIndex(session, std::move(done)); });
            if (!current)
            {
                problem = "could not read the current multi's index (&42): no completion";
                return std::nullopt;
            }
            if (current->result.index)
                snapshot.currentIndex = *current->result.index;
            else
                snapshot.currentProblem = outcomeText(current->result.outcome);
            return snapshot;
        }

        // Wraps one multi created under TEST_MULTI_NAME for the life of one check (RQ-AKM-093): on construction it reads the
        // sampler's multis, refuses to go on when one already bears a reserved name (it is never touched), then creates the
        // test multi, which Create makes current and which lands last in memory. On destruction, even when the check throws
        // half way, it selects the test multi again by its index and deletes it only if it still bears one of the two
        // reserved names, then selects again the multi that was current. Logged, best effort, nothing let out of the
        // destructor, mirroring GuardedTestProgram. It never sends §0C/&07 (Delete ALL Multis) or &01: nothing in this
        // class can, there is no method that does.
        class GuardedTestMulti
        {
        public:
            GuardedTestMulti(Rig& rig, Session& session) : _rig(rig), _session(session)
            {
                std::string problem;
                const auto before = readMultis(rig, session, problem);
                if (!before)
                    throw CheckFailure(problem + " (nothing was created)");
                _original = *before;
                for (const std::string& name : _original.names)
                    if (name == TEST_MULTI_NAME || name == TEST_MULTI_RENAMED)
                        throw CheckFailure("a multi named \"" + name + "\" already exists in the sampler: the check stops without touching it");
                _testIndex = static_cast<int>(_original.names.size());
                const auto created = awaitCompletion<CommandResult>(
                    rig.driver, rig.commandPatience(), [this](CommandCompletion done) { createMulti(_session, TEST_MULTI_NAME, std::move(done)); });
                if (!created || !succeeded(created->result))
                    throw CheckFailure("could not create the test multi \"" + std::string(TEST_MULTI_NAME) + "\": " + timedOutcomeText(created));
                _created = true;
            }

            ~GuardedTestMulti()
            {
                try
                {
                    cleanup();
                }
                catch (...)  // NOLINT: a destructor does not throw
                {
                    _rig.log.note("  the test multi guard could not fully restore the sampler; see the log above");
                }
            }

            GuardedTestMulti(const GuardedTestMulti&) = delete;
            GuardedTestMulti& operator=(const GuardedTestMulti&) = delete;

            /// The multis as they were before the test multi was created.
            [[nodiscard]] const MultisSnapshot& original() const { return _original; }
            /// The test multi's position in memory: the number of multis before it.
            [[nodiscard]] int testIndex() const { return _testIndex; }

        private:
            template <typename Launch>
            bool step(const std::string& title, Launch launch)
            {
                const auto timed = awaitCompletion<CommandResult>(_rig.driver, _rig.commandPatience(), launch);
                const bool ok = timed && succeeded(timed->result);
                _rig.log.note("  " + title + ": " + (ok ? "done" : "failed (" + timedOutcomeText(timed) + ")"));
                return ok;
            }

            void cleanup()
            {
                if (!_created)
                    return;
                const int index = _testIndex;
                if (step("select the test multi again by its index (" + std::to_string(index) + ")",
                         [this, index](CommandCompletion done) { selectMultiByIndex(_session, index, std::move(done)); }))
                {
                    const auto name = awaitCompletion<MultiNameResult>(
                        _rig.driver, _rig.commandPatience(), [this](MultiNameCompletion done) { getCurrentMultiName(_session, std::move(done)); });
                    if (name && name->result.name && (*name->result.name == TEST_MULTI_NAME || *name->result.name == TEST_MULTI_RENAMED))
                        step("delete the test multi \"" + *name->result.name + "\"",
                             [this](CommandCompletion done) { deleteCurrentMulti(_session, std::move(done)); });
                    else
                        _rig.log.note("  the multi at index " + std::to_string(index) + " is not the test one ("
                                      + (name && name->result.name ? "\"" + *name->result.name + "\"" : std::string("name unreadable"))
                                      + "): it is NOT deleted");
                }
                else
                    _rig.log.note("  the test multi could not be selected to delete it; it may already be gone");
                if (_original.currentIndex)
                {
                    const int original = *_original.currentIndex;
                    step("select again the multi that was current (" + std::to_string(original) + ")",
                         [this, original](CommandCompletion done) { selectMultiByIndex(_session, original, std::move(done)); });
                }
            }

            Rig& _rig;
            Session& _session;
            MultisSnapshot _original;
            int _testIndex = 0;
            bool _created = false;
        };

        // What the owner says the sampler's MIDI setup holds, in terms of the sampler's own MIDI SETUP and MIDI FILTER
        // pages (RQ-AKM-080). Section 04 has no Get, so this declaration is the only source of the values to put back.
        // `filterEvent` and `filterChannel` name the one filter the check exercises, `filterAllows` what it does now.
        struct MidiConfigDeclaration
        {
            bool programChangeEnabled = true;
            MultiSelectMode multiSelect = MultiSelectMode::Off;
            int multiSelectChannel = 0;
            int externalApmController = 0;
            AftertouchType aftertouch = AftertouchType::Channel;
            MidiFilterEvent filterEvent = MidiFilterEvent::NoteOn;
            int filterChannel = 0;
            bool filterAllows = true;
        };

        constexpr int MIDI_CHANNELS = 32;
        constexpr int MIDI_CHANNELS_PER_PORT = 16;
        constexpr int MULTI_SELECT_MODES = 3;
        constexpr int EXTERNAL_APM_CONTROLLERS = 128;

        // The channel code 0-31 as the sampler's screen writes it: 1A to 16A, then 1B to 16B.
        std::string midiChannelName(int channel)
        {
            return std::to_string(channel % MIDI_CHANNELS_PER_PORT + 1) + (channel < MIDI_CHANNELS_PER_PORT ? "A" : "B");
        }

        std::string multiSelectName(MultiSelectMode mode)
        {
            switch (mode)
            {
                case MultiSelectMode::Off:
                    return "OFF";
                case MultiSelectMode::ProgramChange:
                    return "PROG CHANGE";
                case MultiSelectMode::Bank:
                    return "BANK";
            }
            return "?";
        }

        std::string aftertouchName(AftertouchType type)
        {
            return type == AftertouchType::Channel ? "CHANNEL" : "POLYPHONIC";
        }

        std::string midiFilterEventName(MidiFilterEvent event)
        {
            switch (event)
            {
                case MidiFilterEvent::NoteOn:
                    return "NOTE ON";
                case MidiFilterEvent::Aftertouch:
                    return "AFTERTOUCH";
                case MidiFilterEvent::Wheels:
                    return "WHEELS";
                case MidiFilterEvent::Volume:
                    return "VOLUME";
            }
            return "?";
        }

        std::string onOffName(bool on)
        {
            return on ? "ON" : "OFF";
        }

        // Puts the owner's MIDI setup back (RQ-AKM-080). The check registers, before sending each change, the command that
        // undoes it; on destruction — even when the check throws half way — the registered commands run in the opposite
        // order, each logged, best effort, nothing let out of the destructor, mirroring GuardedSystemSetup. A restore that
        // fails clears `knownStateRestored`: the owner's MIDI setup is then not what it was declared to be.
        class GuardedMidiConfig
        {
        public:
            using Launch = std::function<void(Session&, CommandCompletion)>;

            GuardedMidiConfig(Rig& rig, Session& session) : _rig(rig), _session(session) {}

            ~GuardedMidiConfig()
            {
                try
                {
                    restore();
                }
                catch (...)  // NOLINT: a destructor does not throw
                {
                    _rig.result.knownStateRestored = false;
                    _rig.log.note("  the MIDI setup guard could not fully restore the sampler; see the log above");
                }
            }

            GuardedMidiConfig(const GuardedMidiConfig&) = delete;
            GuardedMidiConfig& operator=(const GuardedMidiConfig&) = delete;

            /// Registers how to undo a change that is about to be sent: registered first, so that a change the sampler
            /// registered without confirming it is put back too.
            void willRestore(std::string title, Launch launch)
            {
                _steps.push_back({std::move(title), std::move(launch)});
            }

        private:
            struct Step
            {
                std::string title;
                Launch launch;
            };

            void restore()
            {
                for (auto step = _steps.rbegin(); step != _steps.rend(); ++step)
                {
                    const auto timed = awaitCompletion<CommandResult>(
                        _rig.driver, _rig.commandPatience(),
                        [this, &step](CommandCompletion done) { step->launch(_session, std::move(done)); });
                    const bool ok = timed && succeeded(timed->result);
                    _rig.log.note("  restore " + step->title + ": " + (ok ? "done" : "failed (" + timedOutcomeText(timed) + ")"));
                    if (!ok)
                        _rig.result.knownStateRestored = false;
                }
                _steps.clear();
            }

            Rig& _rig;
            Session& _session;
            std::vector<Step> _steps;
        };

        // The disposable sub-folder the Disk Tools checks work in (RQ-AKM-071): created under the folder that is
        // current when the check starts, entered, and removed on destruction however the check ends. Only what this
        // guard created is removed: a folder already carrying the reserved name makes the create fail, and nothing is
        // deleted. If it cannot climb back out to where it started, it leaves the folder in place and clears
        // `knownStateRestored`, rather than claim a state it could not confirm.
        constexpr std::string_view TEST_FOLDER_NAME = "XS56K_SUITE_TEST";

        class GuardedTestFolder
        {
        public:
            GuardedTestFolder(Rig& rig, Session& session) : _rig(rig), _session(session)
            {
                createAndEnter();
            }

            ~GuardedTestFolder()
            {
                try
                {
                    // A check that ends early (failed or skipped) leaves the folder for the owner to look at on the sampler,
                    // when there is a way to ask: Enter lets the guard delete it as usual, skip keeps it for a delete by hand.
                    if (std::uncaught_exceptions() > 0 && _created && _rig.options.askOwner)
                    {
                        _rig.log.flush();
                        if (!_rig.options.askOwner("The check did not finish. Look at the sub-folder " + std::string(TEST_FOLDER_NAME)
                                                   + " on the sampler now. Press Enter to delete it, or type skip to keep it for you "
                                                     "to delete by hand."))
                            _keep = true;
                    }
                    removeIfCreated();
                }
                catch (...)  // NOLINT: a destructor does not throw
                {
                    _rig.result.knownStateRestored = false;
                    _rig.log.note("  the test folder guard could not fully clean up; see the log above");
                }
            }

            GuardedTestFolder(const GuardedTestFolder&) = delete;
            GuardedTestFolder& operator=(const GuardedTestFolder&) = delete;

            // Navigation below the test folder, counted so the guard can climb back out of whatever a check left open.
            [[nodiscard]] bool enterSubFolder(const std::string& name)
            {
                const bool ok = runCommand("open the sub-folder \"" + name + "\"", [name](Session& session, CommandCompletion done) {
                    openFolder(session, name, std::move(done));
                });
                if (ok)
                    ++_depth;
                return ok;
            }

            [[nodiscard]] bool leaveSubFolder()
            {
                if (_depth == 0)
                    return false;
                const bool ok = runCommand("close the sub-folder", [](Session& session, CommandCompletion done) {
                    closeFolder(session, std::move(done));
                });
                if (ok)
                    --_depth;
                return ok;
            }

        private:
            bool runCommand(const std::string& title, const std::function<void(Session&, CommandCompletion)>& launch)
            {
                _rig.log.flush();
                const auto timed = awaitCompletion<CommandResult>(_rig.driver, _rig.commandPatience(), [this, &launch](CommandCompletion done) {
                    launch(_session, std::move(done));
                });
                const bool ok = timed && succeeded(timed->result);
                _rig.log.note("  " + title + ": "
                              + (ok ? std::string("done")
                                    : "failed (" + timedOutcomeText(timed) + ")"));
                return ok;
            }

            void createAndEnter()
            {
                const std::string name(TEST_FOLDER_NAME);
                const bool created = runCommand("create the test folder \"" + name + "\" under the current folder",
                                                [name](Session& session, CommandCompletion done) {
                                                    createFolder(session, name, std::move(done));
                                                });
                if (!created)
                    throw CheckFailure("could not create the test folder \"" + name + "\" (it may already exist: nothing was deleted)");
                _created = true;
                const bool entered = runCommand("open the test folder", [name](Session& session, CommandCompletion done) {
                    openFolder(session, name, std::move(done));
                });
                if (!entered)
                    throw CheckFailure("could not open the test folder \"" + name + "\"");
                _inside = true;
            }

            void removeIfCreated()
            {
                if (!_created)
                    return;
                // Climb out of whatever the check left open, then out of the test folder itself.
                const int climbs = _depth + (_inside ? 1 : 0);
                for (int step = 0; step < climbs; ++step)
                {
                    const bool closed = runCommand("close the folder the check left open", [](Session& session, CommandCompletion done) {
                        closeFolder(session, std::move(done));
                    });
                    if (!closed)
                    {
                        _rig.result.knownStateRestored = false;
                        _rig.log.note("  the test folder is left in place: the sampler would not climb out of it; remove \""
                                      + std::string(TEST_FOLDER_NAME) + "\" by hand");
                        return;
                    }
                }
                if (_keep)
                {
                    _rig.log.note("  the test folder is kept, as the owner asked: remove \"" + std::string(TEST_FOLDER_NAME)
                                  + "\" by hand before the next run");
                    return;
                }
                _inside = false;
                _depth = 0;
                const bool deleted = runCommand("delete the test folder and everything in it", [](Session& session, CommandCompletion done) {
                    deleteSubFolder(session, TEST_FOLDER_NAME, ConfirmDeleteSubFolder::IUnderstandThisDeletesTheFolderAndEverythingInIt,
                                    std::move(done));
                });
                if (!deleted)
                {
                    _rig.result.knownStateRestored = false;
                    _rig.log.note("  the test folder could not be deleted; remove \"" + std::string(TEST_FOLDER_NAME) + "\" by hand");
                    return;
                }
                _created = false;
            }

            Rig& _rig;
            Session& _session;
            bool _created = false;
            bool _inside = false;
            bool _keep = false;
            int _depth = 0;
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
                if (_rig.options.programLifecycle)
                {
                    check("create, change and select a program under a reserved test name, then delete it",
                          &Suite::programLifecycleOnTestProgram);
                    check("a program check that fails half way still deletes the test program and restores the selection",
                          &Suite::failedProgramCheckLeavesTheKnownState);
                    check("add keygroups to the test program and round-trip every §08 parameter item, including keygroup 0 (all)",
                          &Suite::keygroupsOnTestProgram);
                    check("add a zone to a keygroup of the test program and round-trip every §06 parameter item, "
                          "including zone 0 (all four) and keygroup 0 + zone 0",
                          &Suite::zonesOnTestProgram);
                }
                if (_rig.options.sampleLifecycle)
                    check("select the test sample, round-trip every §0E lifecycle and settable-parameter item on it, "
                          "and restore its name and parameters",
                          &Suite::samplesOnTestSample);
                if (_rig.options.systemSetup)
                {
                    check("round-trip the sampler's name, Play Mode, front-panel lock and clock, and put them back",
                          &Suite::systemSetupRoundTrips);
                    check("a system setup check that fails half way and still puts back what it changed",
                          &Suite::failedSystemSetupCheckPutsBack);
                }
                if (_rig.options.diskTools)
                    check("create a disposable sub-folder, read the current disk, round-trip the folder items inside it, "
                          "and delete it again",
                          &Suite::diskToolsRoundTrips);
                if (_rig.options.diskTools && _rig.options.diskToolsFiles)
                    check("the file items of section 10 inside the disposable sub-folder: one save, the file read, renamed and "
                          "deleted",
                          &Suite::diskToolsFileItems);
                if (_rig.options.diskTools && _rig.options.diskToolsAudition)
                    check("the audition of the first .WAV file at the root of the selected disk: started with &30, stopped with &31 "
                          "after the audition duration",
                          &Suite::diskToolsAudition);
                if (_rig.options.diskTools && _rig.options.diskToolsSlow)
                    check("one long-running §10 item, sent inside the disposable sub-folder with Still Alive on, "
                          "then the sub-folder deleted",
                          &Suite::diskToolsSlowOperation);
                if (_rig.options.frontPanel)
                    check("the owner drives the sampler's front panel from the PC keyboard, on a screen the owner chose",
                          &Suite::frontPanelRemote);
                if (_rig.options.midiConfig)
                {
                    check("change every MIDI setup setting and one MIDI filter to another value, the owner confirming each on the "
                          "sampler's screen, and put them back to the values the owner declared",
                          &Suite::midiConfigRoundTrips);
                    check("a MIDI setup check that fails half way and still puts back what it changed",
                          &Suite::failedMidiConfigCheckPutsBack);
                }
                if (_rig.options.multiLifecycle)
                {
                    check("create a test program and a test multi, round-trip every §0C item on them, then delete both and "
                          "put back the multi that was current",
                          &Suite::multiLifecycleOnTestMulti);
                    check("a multi check that fails half way still deletes the test multi and the test program and restores the "
                          "selection",
                          &Suite::failedMultiCheckLeavesTheKnownState);
                }
                if (_rig.options.songFiles)
                {
                    check("read the song files and set lists, select each song file by index and by name, rename the first of each "
                          "and put every name and the selection back",
                          &Suite::songFilesRoundTrips);
                    check("a song files check that fails half way and still puts back what it changed",
                          &Suite::failedSongFilesCheckPutsBack);
                }
                if (_rig.options.sceneLists)
                {
                    check("read the scenelists, select each by index and by name, rename the first and put the name and the "
                          "selection back",
                          &Suite::sceneListsRoundTrips);
                    check("a scenelists check that fails half way and still puts back what it changed",
                          &Suite::failedSceneListsCheckPutsBack);
                }
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
                expect(closed.result.restoredAll(), "every setting the session changed was put back and every key it held released "
                                                        "(not put back: " + settingsText(closed.result.notRestored)
                                                        + "; keys not released: " + keysText(closed.result.keysNotReleased) + ")");
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

            // RQ-AKM-061, the selection the disk checks need: every disk operation acts on the disk selected over SysEx,
            // which the front panel's choice does not set (spec §10). Reads the connected disks (&04 and &05, read only),
            // tests each writable one with &03 (read only), and has the owner choose among the valid ones; the choice is
            // selected with &02. Nothing in §10 clears a selection, so the disk stays selected after the check, and the
            // log says so. No way to ask, or no valid writable disk, skips the check before anything is selected or
            // created. The list is not refreshed here: &01 is one of the long-running items, and the spec has the list
            // refreshed when the sampler is switched on.
            DiskInfo selectTestDisk(GuardedSession& guarded)
            {
                // Refused before anything is read from the disks, let alone selected: the owner picks the disk.
                if (!_rig.options.askOwnerChoice)
                    throw CheckSkipped("there is no way to ask the owner which disk to select, so nothing was selected or created");
                const DiskCountResult count = readDisk<DiskCountResult>("the number of disks connected", [&guarded](DiskCountCompletion done) {
                    getDiskCount(guarded.session(), std::move(done));
                });
                expect(count.count.has_value(), "the number of disks connected is read");
                const DiskListResult listed = readDisk<DiskListResult>("the disks connected", [&guarded](DiskListCompletion done) {
                    getConnectedDisks(guarded.session(), std::move(done));
                });
                expect(listed.disks.has_value(), "the list of the disks connected is read");
                for (const DiskInfo& disk : *listed.disks)
                    finding("disk " + std::to_string(disk.handle) + " \"" + disk.name + "\": type " + std::to_string(disk.type)
                            + ", " + (disk.writable ? "writable" : "read-only"));

                std::vector<DiskInfo> usable;
                for (const DiskInfo& disk : *listed.disks)
                {
                    if (!disk.writable)
                        continue;
                    _rig.log.flush();
                    const auto timed = awaitCompletion<CommandResult>(_rig.driver, _rig.commandPatience(), [&guarded, &disk](CommandCompletion done) {
                        testDiskValid(guarded.session(), disk.handle, std::move(done));
                    });
                    if (!timed)
                        throw CheckFailure("the validity of disk " + std::to_string(disk.handle) + ": no completion within "
                                           + millisecondsText(_rig.commandPatience()) + ": the session lost it");
                    finding("the validity of disk " + std::to_string(disk.handle) + ": " + outcomeText(timed->result) + " after "
                            + millisecondsText(timed->latency));
                    if (succeeded(timed->result))
                        usable.push_back(disk);
                }
                if (usable.empty())
                    throw CheckSkipped("the sampler lists no writable disk that it reports valid, so there is no disk to work on; "
                                       "nothing was selected or created");

                const auto typeText = [](int type) -> std::string {
                    if (type == 0)
                        return "floppy";
                    if (type == 1)
                        return "hard disk";
                    if (type == 2)
                        return "CD-ROM";
                    if (type == 3)
                        return "removable";
                    return "type " + std::to_string(type);
                };
                std::vector<std::string> choices;
                for (const DiskInfo& disk : usable)
                    choices.push_back("handle " + std::to_string(disk.handle) + " \"" + disk.name + "\", " + typeText(disk.type));
                _rig.log.note("  asking the owner which disk to select");
                _rig.log.flush();
                const std::optional<std::size_t> picked = _rig.options.askOwnerChoice(
                    "Choose the disk the disk checks select; the selection then stays on the sampler.", choices);
                if (!picked || *picked >= usable.size())
                    throw CheckSkipped("the owner did not choose a disk to select; nothing was selected or created");
                const DiskInfo chosen = usable[*picked];
                finding("the owner chose " + choices[*picked]);

                expectCommand(guarded, "select disk " + std::to_string(chosen.handle) + " \"" + chosen.name + "\"",
                              [handle = chosen.handle](Session& session, CommandCompletion done) {
                                  selectDisk(session, handle, std::move(done));
                              });
                finding("the selection stays on the sampler after the check: no command of section 10 clears it");
                return chosen;
            }

            // RQ-AKM-071, the safe half: selects a writable disk (selectTestDisk), reads the current disk, then, inside the
            // disposable sub-folder, creates, renames, enters and leaves a sub-folder, reads the folder items and the file
            // items of the empty folder, and deletes the whole sub-folder through the confirmed &17 guard. A file cannot be
            // made without saving one, which is one of the long-running items, so no file is listed here.
            void diskToolsRoundTrips()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());
                const DiskInfo disk = selectTestDisk(guarded);

                const DiskTypeResult type = readDisk<DiskTypeResult>("the current disk's type", [&guarded](DiskTypeCompletion done) {
                    getCurrentDiskType(guarded.session(), std::move(done));
                });
                expect(type.type.has_value(), "the current disk's type is read");
                const DiskHandleResult handle = readDisk<DiskHandleResult>("the current disk's handle", [&guarded](DiskHandleCompletion done) {
                    getCurrentDiskHandle(guarded.session(), std::move(done));
                });
                expect(handle.handle.has_value() && *handle.handle == disk.handle,
                       "the current disk is the one selected (handle " + std::to_string(disk.handle) + ")");
                // §10/&07 and &0E name a disk by its handle, so they read the one the list gave, without selecting anything.
                const DiskTypeResult listedType = readDisk<DiskTypeResult>("the type of disk " + std::to_string(disk.handle),
                                                                           [&guarded, &disk](DiskTypeCompletion done) {
                                                                               getDiskType(guarded.session(), disk.handle, std::move(done));
                                                                           });
                expect(listedType.type.has_value() && *listedType.type == disk.type,
                       "the type of disk " + std::to_string(disk.handle) + " is the one the list gave (" + std::to_string(disk.type) + ")");
                const DiskNameResult listedName = readDisk<DiskNameResult>("the name of disk " + std::to_string(disk.handle),
                                                                           [&guarded, &disk](DiskNameCompletion done) {
                                                                               getDiskName(guarded.session(), disk.handle, std::move(done));
                                                                           });
                expect(listedName.name.has_value() && *listedName.name == disk.name,
                       "the name of disk " + std::to_string(disk.handle) + " is \"" + disk.name + "\"");
                const DiskFormatResult format = readDisk<DiskFormatResult>("the current disk's format", [&guarded](DiskFormatCompletion done) {
                    getCurrentDiskFormat(guarded.session(), std::move(done));
                });
                expect(format.format.has_value(), "the current disk's format is read");
                const DiskFreeSpaceResult freeSpace = readDisk<DiskFreeSpaceResult>("the current disk's free space", [&guarded](DiskFreeSpaceCompletion done) {
                    getCurrentDiskFreeSpace(guarded.session(), std::move(done));
                });
                expect(freeSpace.freeBytes.has_value(), "the current disk's free space is read");
                const DiskPathResult path = readDisk<DiskPathResult>("the current disk's path", [&guarded](DiskPathCompletion done) {
                    getCurrentDiskPath(guarded.session(), std::move(done));
                });
                expect(path.path.has_value(), "the current disk's path is read");

                const int before = folderCountHere(guarded);
                {
                    GuardedTestFolder folder(_rig, guarded.session());
                    expect(folderCountHere(guarded) == 0, "the new test folder holds no sub-folder");
                    expectCommand(guarded, "create the sub-folder \"INNER\" inside it", [](Session& session, CommandCompletion done) {
                        createFolder(session, "INNER", std::move(done));
                    });
                    expect(folderCountHere(guarded) == 1, "the test folder now holds one sub-folder");
                    const DiskFolderNamesResult names = readDisk<DiskFolderNamesResult>("the names of the sub-folders", [&guarded](DiskFolderNamesCompletion done) {
                        getAllFolderNames(guarded.session(), std::move(done));
                    });
                    expect(names.names.has_value() && *names.names == std::vector<std::string>{"INNER"},
                           "the only sub-folder is \"INNER\"");
                    expectCommand(guarded, "rename \"INNER\" to \"INNER_2\"", [](Session& session, CommandCompletion done) {
                        renameFolder(session, "INNER", "INNER_2", std::move(done));
                    });
                    const DiskFolderNameResult renamed = readDisk<DiskFolderNameResult>("the name of the sub-folder at index 0", [&guarded](DiskFolderNameCompletion done) {
                        getFolderName(guarded.session(), 0, std::move(done));
                    });
                    expect(renamed.name.has_value() && *renamed.name == "INNER_2", "the renamed sub-folder reads back as \"INNER_2\"");

                    if (!folder.enterSubFolder("INNER_2"))
                        throw CheckFailure("could not open the sub-folder \"INNER_2\"");
                    expect(fileCountHere(guarded) == 0, "the sub-folder holds no file (none is created in this check)");
                    const DiskFileIndexResult missing = readDisk<DiskFileIndexResult>("the index of a file that is not there",
                                                                                        [&guarded](DiskFileIndexCompletion done) {
                                                                                            getFileIndexByName(guarded.session(), "NOT_THERE", std::move(done));
                                                                                        });
                    expect(!missing.index.has_value(), "no index is returned for a file that is not there");
                    if (!folder.leaveSubFolder())
                        throw CheckFailure("could not close the sub-folder \"INNER_2\"");
                }
                expect(folderCountHere(guarded) == before, "the test folder is gone: the folder count is back to what it was");
                closeAndVerify(guarded);
            }

            // RQ-AKM-065, RQ-AKM-069, RQ-AKM-071: the file items of §10 inside the disposable sub-folder, after one save of the
            // test program. The save is sent only when the owner can check the file on the sampler (refused before anything
            // is sent otherwise). The file is read, renamed, read again by name and deleted, then the sub-folder goes through
            // the guard. The audition (&30, &31) is not here: the spec has it for a sample from disk, and this check saves a
            // program, which cannot be auditioned.
            void diskToolsFileItems()
            {
                if (!_rig.options.askOwner)
                    throw CheckSkipped("this check saves a file, which only the owner can check on the sampler, and there is no way to ask the owner");
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());
                selectTestDisk(guarded);
                const int before = folderCountHere(guarded);
                {
                    GuardedTestFolder folder(_rig, guarded.session());
                    expect(fileCountHere(guarded) == 0, "the new test folder holds no file");
                    {
                        GuardedTestProgram program(_rig, guarded.session());
                        const int programIndex = currentProgramIndex(guarded);
                        expectCommand(guarded, "save the test program to the sub-folder (section 10, item 2C)",
                                      [programIndex](Session& session, CommandCompletion done) {
                                          saveMemoryItem(session, programIndex, SaveableMemoryType::Program, false, false, std::move(done));
                                      });
                    }
                    const std::vector<std::string> saved = fileNamesHere(guarded);
                    expect(saved.size() == 1, "the save left one file in the sub-folder");
                    const std::string file = saved.front();
                    ownerConfirms("On the sampler, open the sub-folder XS56K_SUITE_TEST under the current folder and check "
                                  "that it holds the file \"" + file + "\".");
                    expect(fileCountHere(guarded) == 1, "the sub-folder holds one file");

                    const DiskFileNameResult name = readDisk<DiskFileNameResult>("the name of the file at index 0",
                                                                                 [&guarded](DiskFileNameCompletion done) {
                                                                                     getFileName(guarded.session(), 0, std::move(done));
                                                                                 });
                    expect(name.name.has_value() && *name.name == file, "the file at index 0 is \"" + file + "\"");
                    const DiskFileSizeResult size = readDisk<DiskFileSizeResult>("the size of the file at index 0",
                                                                                 [&guarded](DiskFileSizeCompletion done) {
                                                                                     getFileSize(guarded.session(), 0, std::move(done));
                                                                                 });
                    expect(size.sizeBytes.has_value() && *size.sizeBytes > 0, "the file has a size");
                    const DiskFileIndexResult found = readDisk<DiskFileIndexResult>("the index of the file by name",
                                                                                    [&guarded, &file](DiskFileIndexCompletion done) {
                                                                                        getFileIndexByName(guarded.session(), file, std::move(done));
                                                                                    });
                    expect(found.index.has_value() && *found.index == 0, "the file is at index 0 by its name");

                    // The new name is given without its extension: the sampler appends the renamed file's own extension (on the
                    // S5000, a program file: "XS56K_RENAMED.AKP" given became "XS56K_RENAMED.AKP.AKP"). So the name to expect is
                    // built the same way; the rule is not yet seen for sample files (.WAV).
                    const std::size_t dot = file.find_last_of('.');
                    const std::string extension = dot == std::string::npos ? std::string() : file.substr(dot);
                    const std::string newBase = "XS56K_RENAMED";
                    const std::string expected = newBase + extension;
                    expectCommand(guarded, "rename the file to \"" + newBase + "\" (section 10, item 28)",
                                  [&file, &newBase](Session& session, CommandCompletion done) {
                                      renameFile(session, file, newBase, std::move(done));
                                  });
                    // What the sampler stores after the rename, read back from its own listing (&22), said in the log.
                    const std::vector<std::string> afterRename = fileNamesHere(guarded);
                    std::string listed;
                    for (const std::string& listedName : afterRename)
                        listed += (listed.empty() ? "" : ", ") + std::string("\"") + listedName + "\"";
                    finding("the names in the sub-folder after the rename: " + (listed.empty() ? std::string("none") : listed));
                    expect(afterRename.size() == 1 && afterRename.front() == expected,
                           "after the rename the sub-folder holds the file \"" + expected + "\"");
                    const std::string stored = afterRename.front();
                    const DiskFileIndexResult byStoredName = readDisk<DiskFileIndexResult>("the index of the renamed file",
                                                                                           [&guarded, &stored](DiskFileIndexCompletion done) {
                                                                                               getFileIndexByName(guarded.session(), stored, std::move(done));
                                                                                           });
                    expect(byStoredName.index.has_value() && *byStoredName.index == 0, "the renamed file is at index 0 by its name");

                    expectCommand(guarded, "delete the file \"" + stored + "\" (section 10, item 29)",
                                  [&stored](Session& session, CommandCompletion done) {
                                      deleteFile(session, stored, ConfirmDeleteFile::IUnderstandThisDeletesTheFile, std::move(done));
                                  });
                    expect(fileCountHere(guarded) == 0, "the sub-folder holds no file again");
                }
                expect(folderCountHere(guarded) == before, "the test folder is gone: the folder count is back to what it was");
                closeAndVerify(guarded);
            }

            // RQ-AKM-068: the audition of a sample from disk (§10/&30, &31). The spec has it for a sample file, so the check
            // looks for the first .WAV file at the root of the selected disk. Nothing is saved: the owner confirms that such a
            // file is there, the file is started with &30 and stopped with &31 after the audition duration. A stop the
            // sampler refuses is recorded, not failed: a sample shorter than the duration has ended already.
            void diskToolsAudition()
            {
                if (!_rig.options.askOwner)
                    throw CheckSkipped("this check plays a sample, which only the owner can hear on the sampler, and there is no way to ask the owner");
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());
                selectTestDisk(guarded);
                const DiskPathResult path = readDisk<DiskPathResult>("the current folder of the selected disk",
                                                                     [&guarded](DiskPathCompletion done) {
                                                                         getCurrentDiskPath(guarded.session(), std::move(done));
                                                                     });
                expect(path.path.has_value() && path.path->empty(), "the current folder is the root of the selected disk");
                ownerConfirms("Make sure the selected disk holds at least one .WAV file at its root (not in a folder), and confirm "
                              "that it is there on the sampler.");

                const std::vector<std::string> names = fileNamesHere(guarded);
                std::string listed;
                for (const std::string& listedName : names)
                    listed += (listed.empty() ? "" : ", ") + std::string("\"") + listedName + "\"";
                finding("the files at the root of the selected disk: " + (listed.empty() ? std::string("none") : listed));
                const auto isWav = [](const std::string& name) {
                    if (name.size() < 4)
                        return false;
                    std::string extension = name.substr(name.size() - 4);
                    for (char& character : extension)
                        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
                    return extension == ".WAV";
                };
                const auto wav = std::find_if(names.begin(), names.end(), isWav);
                expect(wav != names.end(), "a .WAV file is at the root of the selected disk");
                const std::string sample = *wav;

                // &30 takes the sample's position in the root's list (&22), counted from 0 over all its files: the owner's MIDIOX
                // test played S1 with position 2. &24 is not used: it finds a name without its extension (S1 found, S1.WAV
                // refused), and the owner's tests disagree on its answers.
                const int sampleIndex = static_cast<int>(std::distance(names.begin(), wav));
                finding("&30 is sent with position " + std::to_string(sampleIndex) + " in the list of the root's files");
                expectCommand(guarded, "start the audition of \"" + sample + "\" (section 10, item 30)",
                              [sampleIndex](Session& session, CommandCompletion done) { startFileAudition(session, sampleIndex, std::move(done)); });

                _rig.log.note("  listening for " + millisecondsText(_rig.options.auditionDuration));
                _rig.log.flush();
                std::this_thread::sleep_for(_rig.options.auditionDuration);

                const CommandResult stopped = observeCommand(guarded, "stop the audition (section 10, item 31)",
                                                             [](Session& session, CommandCompletion done) { stopFileAudition(session, std::move(done)); });
                if (!succeeded(stopped))
                    finding("the sampler refused the stop: the sample may have ended before the audition duration (a shorter sample)");
                closeAndVerify(guarded);
            }

            // A sampler key as the owner and the log read it: its name on the front panel and its keycode.
            static std::string keyText(FrontPanelKey key)
            {
                return std::string(remoteKeyName(key)) + " (" + hex(std::array<std::uint8_t, 1>{static_cast<std::uint8_t>(key)}) + ")";
            }

            // How a PC key is written in the log and to the owner.
            static std::string pcKeyText(int pcKey)
            {
                constexpr int FIRST_VISIBLE = 33;
                constexpr int LAST_VISIBLE = 126;
                if (pcKey >= FIRST_VISIBLE && pcKey <= LAST_VISIBLE)
                    return "'" + std::string(1, static_cast<char>(pcKey)) + "'";
                return "key code " + std::to_string(pcKey);
            }

            // RQ-AKM-076: the owner drives the sampler's front panel from the PC keyboard. The owner picks the screen the
            // sampler shows and confirms it before anything is sent; then each PC key sends only the §20 item the mapping gives
            // it (`FrontPanelRemote.hpp`) — nothing is sent on the suite's own initiative — and what the sampler answered is
            // said to the owner and written in the log. A short press is a Hold then a Release; Space holds ENT/PLAY until the
            // next Space, as the spec's own example. The check ends on the end key, or when the owner's input ends, and every
            // key still held is released then; if it fails or throws half way, the guard's close releases them (DEC-AKM-019).
            void frontPanelRemote()
            {
                if (!_rig.options.askOwner || !_rig.options.readOwnerKey)
                    throw CheckSkipped("this check is driven by the owner's keys, and there is no way to ask the owner or to read a key");
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());

                const auto tell = [this](const std::string& line) {
                    _rig.log.note("  " + line);
                    if (_rig.options.tellOwner)
                        _rig.options.tellOwner(line);
                };
                // The mapping is shown first, as a block of its own, before the owner is asked anything and before any key
                // is read: the owner reads it, then confirms, then has the keyboard.
                tell("");
                tell("KEYBOARD MAPPING for the sampler's front panel: every key you press is sent to the sampler as the "
                     "front-panel key it stands for; nothing is sent unless you press a key.");
                for (const std::string& line : remoteMappingLines())
                    tell(line);
                ownerConfirms("Read the mapping above, put the sampler on the screen of your choice, then confirm. "
                              "The keyboard is yours after that.");

                tell("ready: press keys on the PC keyboard; q ends the check");

                bool textMode = false;
                std::optional<FrontPanelKey> held;
                int keysPressed = 0;
                int framesSent = 0;
                const auto send = [&](const std::string& title, const std::function<void(Session&, CommandCompletion)>& launch) {
                    _rig.log.flush();
                    const auto timed = awaitCompletion<CommandResult>(
                        _rig.driver, _rig.commandPatience(),
                        [&guarded, &launch](CommandCompletion done) { launch(guarded.session(), std::move(done)); });
                    if (!timed)
                        throw CheckFailure(title + ": no completion within " + millisecondsText(_rig.commandPatience()) + ": the session lost it");
                    ++framesSent;
                    tell(title + ": " + outcomeText(timed->result) + " after " + millisecondsText(timed->latency));
                    return timed->result;
                };

                for (;;)
                {
                    const std::optional<int> pcKey = _rig.options.readOwnerKey(textMode);
                    if (!pcKey)
                    {
                        tell("the owner's input ended");
                        break;
                    }
                    const RemoteAction action = remoteAction(textMode, *pcKey);
                    const std::string pressed = "PC " + pcKeyText(*pcKey);
                    if (action.kind == RemoteActionKind::End)
                    {
                        tell(pressed + ": end of the check");
                        break;
                    }
                    ++keysPressed;
                    switch (action.kind)
                    {
                        case RemoteActionKind::None:
                            tell(pressed + ": no sampler key, nothing sent");
                            break;
                        case RemoteActionKind::ToggleTextMode:
                            textMode = true;
                            tell(pressed + ": text mode, printable keys go as ASCII (Tab or Escape to leave)");
                            break;
                        case RemoteActionKind::LeaveTextMode:
                            textMode = false;
                            tell(pressed + ": back to the normal mode");
                            break;
                        case RemoteActionKind::Press:
                        {
                            const FrontPanelKey key = action.key;
                            _rig.log.flush();
                            const auto timed = awaitCompletion<KeyPressResult>(
                                _rig.driver, _rig.commandPatience() * KEY_PRESS_PATIENCE_FACTOR,
                                [&guarded, key](KeyPressCompletion done) { pressKey(guarded.session(), key, std::move(done)); });
                            if (!timed)
                                throw CheckFailure(pressed + ": no completion for the press: the session lost it");
                            framesSent += 2;
                            tell(pressed + " -> sampler key " + keyText(key) + ": hold " + outcomeText(timed->result.hold) + ", release "
                                 + outcomeText(timed->result.release) + " after " + millisecondsText(timed->latency));
                            // A press of the key held with Space ends with its Release: it is no longer held.
                            if (held == key && succeeded(timed->result.release))
                                held.reset();
                            break;
                        }
                        case RemoteActionKind::ToggleHold:
                        {
                            const FrontPanelKey key = action.key;
                            if (held)
                            {
                                const CommandResult released = send(pressed + " -> release sampler key " + keyText(key),
                                                                    [key](Session& session, CommandCompletion done) {
                                                                        releaseKey(session, key, std::move(done));
                                                                    });
                                if (succeeded(released))
                                    held.reset();
                            }
                            else
                            {
                                // Held once the sampler queued the Hold: one it refused, or never answered, is not a key to
                                // toggle (the session still remembers a timed-out one, and its close releases it).
                                const CommandResult hold = send(pressed + " -> hold sampler key " + keyText(key),
                                                                [key](Session& session, CommandCompletion done) {
                                                                    holdKey(session, key, std::move(done));
                                                                });
                                if (succeeded(hold))
                                    held = key;
                            }
                            break;
                        }
                        case RemoteActionKind::Wheel:
                        {
                            const DataWheelDirection direction = action.direction;
                            const int clicks = action.clicks;
                            static_cast<void>(send(pressed + " -> data wheel " + (direction == DataWheelDirection::Forwards ? "forwards " : "backwards ")
                                                       + std::to_string(clicks) + (clicks == 1 ? " click" : " clicks"),
                                                   [direction, clicks](Session& session, CommandCompletion done) {
                                                       moveDataWheel(session, direction, clicks, std::move(done));
                                                   }));
                            break;
                        }
                        case RemoteActionKind::Ascii:
                        {
                            const int character = action.ascii;
                            static_cast<void>(send(pressed + " -> ASCII " + std::to_string(character),
                                                   [character](Session& session, CommandCompletion done) {
                                                       sendAsciiKey(session, character, std::move(done));
                                                   }));
                            break;
                        }
                        case RemoteActionKind::End:
                            break;
                    }
                }

                if (held)
                {
                    const FrontPanelKey key = *held;
                    static_cast<void>(send("end of the check: release the sampler key " + keyText(key) + " still held",
                                           [key](Session& session, CommandCompletion done) { releaseKey(session, key, std::move(done)); }));
                    held.reset();
                }
                finding(std::to_string(keysPressed) + " PC keys pressed by the owner, " + std::to_string(framesSent)
                        + " front-panel commands sent");
                closeAndVerify(guarded);
            }

            // RQ-AKM-070: one long-running §10 item, sent with Still Alive on, inside the disposable sub-folder, and the
            // sub-folder deleted afterwards whatever the item did. Each of the six needs a file or a program first, which
            // can only be made inside the sub-folder by a save: the save is sent through the same timed path.
            void diskToolsSlowOperation()
            {
                const DiskSlowOperation operation = *_rig.options.diskToolsSlow;
                const bool savesAFile = operation == DiskSlowOperation::LoadFile || operation == DiskSlowOperation::LoadFileWithDependents
                                        || operation == DiskSlowOperation::SaveMemoryItem || operation == DiskSlowOperation::SaveAllMemoryItems;
                // Refused before anything is sent: a save that nobody can check on the sampler is not worth sending.
                if (savesAFile && !_rig.options.askOwner)
                    throw CheckSkipped("this item saves a file, which only the owner can check on the sampler, and there is no way to ask the owner");
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());
                expect(guarded.session().stillAliveMonitoring(),
                       "Still Alive is on: a received F0 F7 restarts the pending command's timeout");
                selectTestDisk(guarded);
                const int before = folderCountHere(guarded);
                {
                    GuardedTestFolder folder(_rig, guarded.session());
                    switch (operation)
                    {
                        case DiskSlowOperation::UpdateList:
                            sendSlow(guarded, "update the list of disks connected (section 10, item 01)",
                                     [](Session& session, CommandCompletion done) { updateDiskList(session, std::move(done)); });
                            break;
                        case DiskSlowOperation::LoadFolder:
                            expectCommand(guarded, "create the sub-folder \"LOAD\" to load", [](Session& session, CommandCompletion done) {
                                createFolder(session, "LOAD", std::move(done));
                            });
                            sendSlow(guarded, "load the folder \"LOAD\" (section 10, item 15)",
                                     [](Session& session, CommandCompletion done) { loadFolder(session, "LOAD", std::move(done)); });
                            break;
                        case DiskSlowOperation::LoadFile:
                        case DiskSlowOperation::LoadFileWithDependents:
                        {
                            GuardedTestProgram program(_rig, guarded.session());
                            const int index = currentProgramIndex(guarded);
                            sendSlow(guarded, "save the test program to the sub-folder, to have a file to load (section 10, item 2C)",
                                     [index](Session& session, CommandCompletion done) {
                                         saveMemoryItem(session, index, SaveableMemoryType::Program, false, false, std::move(done));
                                     });
                            const std::string file = firstFileName(guarded);
                            // Between the save and the load: the owner sees the file on the sampler before anything loads it.
                            ownerConfirms("On the sampler, open the sub-folder XS56K_SUITE_TEST under the current folder and check "
                                          "that it holds the file \"" + file + "\".");
                            expectCommand(guarded, "take the test program out of memory, so the load brings back the only copy",
                                          [](Session& session, CommandCompletion done) { deleteCurrentProgram(session, std::move(done)); });
                            if (operation == DiskSlowOperation::LoadFile)
                                sendSlow(guarded, "load the file \"" + file + "\" (section 10, item 2A)",
                                         [file](Session& session, CommandCompletion done) {
                                             loadFile(session, file, SampleLoadOption::Normal, std::move(done));
                                         });
                            else
                                sendSlow(guarded, "load the file \"" + file + "\" with its dependents (section 10, item 2B)",
                                         [file](Session& session, CommandCompletion done) {
                                             loadFileWithDependents(session, file, std::move(done));
                                         });
                            break;
                        }
                        case DiskSlowOperation::SaveMemoryItem:
                        {
                            GuardedTestProgram program(_rig, guarded.session());
                            const int index = currentProgramIndex(guarded);
                            sendSlow(guarded, "save the test program to the sub-folder (section 10, item 2C)",
                                     [index](Session& session, CommandCompletion done) {
                                         saveMemoryItem(session, index, SaveableMemoryType::Program, false, false, std::move(done));
                                     });
                            const std::vector<std::string> saved = fileNamesHere(guarded);
                            expect(!saved.empty(), "the save left a file in the sub-folder");
                            ownerConfirms("On the sampler, open the sub-folder XS56K_SUITE_TEST under the current folder and check "
                                          "that it holds the file \"" + saved.front() + "\".");
                            break;
                        }
                        case DiskSlowOperation::SaveAllMemoryItems:
                        {
                            // Every program in memory is saved, the owner's included: each copy lands in the sub-folder
                            // and goes with it when it is deleted; nothing stored is changed.
                            GuardedTestProgram program(_rig, guarded.session());
                            sendSlow(guarded, "save every program in memory to the sub-folder (section 10, item 2D)",
                                     [](Session& session, CommandCompletion done) {
                                         saveAllMemoryItems(session, SaveableMemoryType::Program, false, false, std::move(done));
                                     });
                            const std::vector<std::string> saved = fileNamesHere(guarded);
                            expect(!saved.empty(), "the save left at least one file in the sub-folder");
                            std::string listed;
                            for (const std::string& name : saved)
                                listed += (listed.empty() ? "" : ", ") + std::string("\"") + name + "\"";
                            ownerConfirms("On the sampler, open the sub-folder XS56K_SUITE_TEST under the current folder and check "
                                          "that it holds the files " + listed + ".");
                            break;
                        }
                    }
                }
                expect(folderCountHere(guarded) == before, "the test folder is gone: the folder count is back to what it was");
                closeAndVerify(guarded);
            }

            // A command whose answer is what the check records, whatever it is: said in the log, never a failure by itself.
            CommandResult observeCommand(GuardedSession& guarded, const std::string& title,
                                         const std::function<void(Session&, CommandCompletion)>& launch)
            {
                _rig.log.flush();
                const auto timed = awaitCompletion<CommandResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded, &launch](CommandCompletion done) { launch(guarded.session(), std::move(done)); });
                if (!timed)
                    throw CheckFailure(title + ": no completion within " + millisecondsText(_rig.commandPatience()) + ": the session lost it");
                finding(title + ": " + outcomeText(timed->result) + " after " + millisecondsText(timed->latency));
                return timed->result;
            }

            // A read that changes nothing, said in the log; a read the sampler does not answer fails the check.
            template <typename Result, typename Launch>
            Result readDisk(const std::string& title, Launch launch)
            {
                _rig.log.flush();
                const auto timed = awaitCompletion<Result>(_rig.driver, _rig.commandPatience(), std::move(launch));
                if (!timed)
                    throw CheckFailure(title + ": no completion within " + millisecondsText(_rig.commandPatience()) + ": the session lost it");
                finding(title + ": " + outcomeText(timed->result.outcome) + " after " + millisecondsText(timed->latency));
                return timed->result;
            }

            [[nodiscard]] int folderCountHere(GuardedSession& guarded)
            {
                const DiskFolderCountResult counted = readDisk<DiskFolderCountResult>("the number of sub-folders here",
                                                                                      [&guarded](DiskFolderCountCompletion done) {
                                                                                          getFolderCount(guarded.session(), std::move(done));
                                                                                      });
                if (!counted.count)
                    throw CheckFailure("the number of sub-folders could not be read");
                return *counted.count;
            }

            [[nodiscard]] int fileCountHere(GuardedSession& guarded)
            {
                const DiskFileCountResult counted = readDisk<DiskFileCountResult>("the number of files here", [&guarded](DiskFileCountCompletion done) {
                    getFileCount(guarded.session(), std::move(done));
                });
                if (!counted.count)
                    throw CheckFailure("the number of files could not be read");
                return *counted.count;
            }

            [[nodiscard]] std::vector<std::string> fileNamesHere(GuardedSession& guarded)
            {
                const auto timed = awaitCompletion<DiskFileNamesResult>(_rig.driver, _rig.commandPatience(), [&guarded](DiskFileNamesCompletion done) {
                    getAllFileNames(guarded.session(), std::move(done));
                });
                if (!timed || !timed->result.names)
                    throw CheckFailure("the names of the files in the sub-folder could not be read");
                return *timed->result.names;
            }

            [[nodiscard]] std::string firstFileName(GuardedSession& guarded)
            {
                const std::vector<std::string> names = fileNamesHere(guarded);
                if (names.empty())
                    throw CheckFailure("the save left no file in the sub-folder to load");
                finding("file to load: \"" + names.front() + "\"");
                return names.front();
            }

            // The owner looks at the sampler and confirms what the check expects there: a file a save has just made, which
            // this layer cannot list without a save of its own. Declined, the check is skipped: nothing is sent after it,
            // and the guards put back what the check changed.
            void ownerConfirms(const std::string& instruction)
            {
                if (!_rig.options.askOwner)
                    throw CheckSkipped("there is no way to ask the owner to check the sampler");
                _rig.log.flush();
                _rig.log.note("  asking the owner to check the sampler: " + instruction);
                _rig.log.flush();
                if (!_rig.options.askOwner(instruction))
                    throw CheckSkipped("the owner did not confirm on the sampler: " + instruction);
                _rig.log.note("  the owner confirms on the sampler");
            }

            [[nodiscard]] int currentProgramIndex(GuardedSession& guarded)
            {
                const auto timed = awaitCompletion<ProgramIndexResult>(_rig.driver, _rig.commandPatience(), [&guarded](ProgramIndexCompletion done) {
                    getProgramIndex(guarded.session(), std::move(done));
                });
                if (!timed || !timed->result.index)
                    throw CheckFailure("the index of the test program could not be read");
                return *timed->result.index;
            }

            // One long-running command, with Still Alive on: its patience is the whole total wait the session allows, so a
            // slow command completes and only a silent sampler times out. A timeout, or a completion that is lost, clears
            // `knownStateRestored`: a sampler that stopped answering cannot be said to be in the known state.
            void sendSlow(GuardedSession& guarded, const std::string& title, const std::function<void(Session&, CommandCompletion)>& launch)
            {
                const Clock::duration patience = DEFAULT_MAX_TOTAL_WAIT + _rig.options.commandTimeout * COMMAND_PATIENCE_IN_TIMEOUTS;
                _rig.log.flush();
                const std::size_t stillAliveBefore = _rig.log.stillAliveMessages();
                const auto timed = awaitCompletion<CommandResult>(_rig.driver, patience, [&guarded, &launch](CommandCompletion done) {
                    launch(guarded.session(), std::move(done));
                });
                if (!timed)
                {
                    _rig.result.knownStateRestored = false;
                    throw CheckFailure(title + ": no completion within " + millisecondsText(patience) + ": the session lost it");
                }
                const std::size_t seen = _rig.log.stillAliveMessages() - stillAliveBefore;
                finding(title + ": " + outcomeText(timed->result) + " after " + millisecondsText(timed->latency)
                        + "; F0 F7 messages that reached the host meanwhile: " + std::to_string(seen));
                if (std::holds_alternative<Timeout>(timed->result))
                {
                    _rig.result.knownStateRestored = false;
                    throw CheckFailure(title + ": timed out although Still Alive is on: the sampler sent no F0 F7 while it worked, "
                                       "or the backend did not deliver them. The sampler may need a power cycle before anything else "
                                       "is asked (process/2.architecture/OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md)");
                }
                if (!succeeded(timed->result))
                    throw CheckFailure(title + ": " + outcomeText(timed->result));
            }

            // RQ-AKM-027: creates a program under the reserved test name (GuardedTestProgram's constructor),
            // changes it (add keygroups, crossfade, rename and back, every item of RQ-AKM-024's five parameter
            // groups), proves expectOnTestProgram refuses once the current program is no longer the test one, then
            // lets the guard delete it and restore the original selection — verified by the program count being
            // back to what it was.
            void programLifecycleOnTestProgram()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());

                const auto before = awaitCompletion<ProgramCountResult>(
                    _rig.driver, _rig.commandPatience(),
                    [&guarded](ProgramCountCompletion done) { getProgramCount(guarded.session(), std::move(done)); });
                if (!before || !before->result.count.has_value())
                    throw CheckFailure("could not read the number of programs before creating the test program");
                const int countBefore = *before->result.count;

                {
                    GuardedTestProgram program(_rig, guarded.session());
                    finding("test program \"" + std::string(TEST_PROGRAM_NAME) + "\" created and current");

                    program.expectOnTestProgram("add 3 keygroups", [](Session& session, CommandCompletion done) {
                        addKeygroupsToProgram(session, 3, std::move(done));
                    });
                    const auto keygroups = awaitCompletion<ProgramKeygroupCountResult>(
                        _rig.driver, _rig.commandPatience(), [&guarded](ProgramKeygroupCountCompletion done) {
                            getProgramKeygroupCount(guarded.session(), std::move(done));
                        });
                    expect(keygroups && keygroups->result.count == 4, "the keygroup count read back is 4 (1 default + 3 added)");

                    program.expectOnTestProgram("set crossfade on", [](Session& session, CommandCompletion done) {
                        setKeygroupCrossfade(session, true, std::move(done));
                    });
                    const auto crossfade = awaitCompletion<ProgramCrossfadeResult>(
                        _rig.driver, _rig.commandPatience(),
                        [&guarded](ProgramCrossfadeCompletion done) { getKeygroupCrossfade(guarded.session(), std::move(done)); });
                    expect(crossfade && crossfade->result.enabled == true, "crossfade read back on");

                    // RQ-AKM-021: rename, then rename back to the reserved name before anything else runs, so the
                    // guard (which tracks its program by that name) can still find it if this or a later step throws.
                    // Within the 20-character name limit observed on a real S5000 (kb.md, "Common value codes"):
                    // TEST_PROGRAM_NAME is 16 characters, leaving room for "_2" but not a longer suffix.
                    const std::string renamedTo = std::string(TEST_PROGRAM_NAME) + "_2";
                    program.expectOnTestProgram("rename the test program", [&renamedTo](Session& session, CommandCompletion done) {
                        renameCurrentProgram(session, renamedTo, std::move(done));
                    });
                    const auto renamedName = awaitCompletion<ProgramNameResult>(
                        _rig.driver, _rig.commandPatience(),
                        [&guarded](ProgramNameCompletion done) { getCurrentProgramName(guarded.session(), std::move(done)); });
                    expect(renamedName && renamedName->result.name == renamedTo, "the new name read back");
                    const auto renameBack = awaitCompletion<CommandResult>(
                        _rig.driver, _rig.commandPatience(), [&guarded](CommandCompletion done) {
                            renameCurrentProgram(guarded.session(), std::string(TEST_PROGRAM_NAME), std::move(done));
                        });
                    if (!renameBack || !succeeded(renameBack->result))
                        throw CheckFailure("could not rename the test program back to \"" + std::string(TEST_PROGRAM_NAME) + "\"");

                    // RQ-AKM-024: every item of the five parameter groups, Set then Get, verified by read-back.
                    for (const ProgramParameterCase& parameterCase : allProgramParameterCases())
                    {
                        const ItemDescriptor& getDescriptor = descriptor(parameterCase.getId);
                        const auto selectorCount = static_cast<std::ptrdiff_t>(getDescriptor.args.size());
                        const std::vector<std::int64_t> selector(parameterCase.values.begin(), parameterCase.values.begin() + selectorCount);
                        const std::vector<std::int64_t> expectedValue(parameterCase.values.begin() + selectorCount, parameterCase.values.end());
                        const std::string title(descriptor(parameterCase.setId).name);

                        program.expectOnTestProgram("set " + title, [&parameterCase](Session& session, CommandCompletion done) {
                            session.submit(makeRequest(parameterCase.setId, parameterCase.values), std::move(done));
                        });
                        const auto timed = awaitCompletion<CommandResult>(
                            _rig.driver, _rig.commandPatience(), [&guarded, &parameterCase, &selector](CommandCompletion done) {
                                guarded.session().submit(makeRequest(parameterCase.getId, selector), std::move(done));
                            });
                        if (!timed)
                            throw CheckFailure("get " + title + ": no completion within " + millisecondsText(_rig.commandPatience()));
                        if (!succeeded(timed->result))
                            throw CheckFailure("get " + title + ": " + outcomeText(timed->result));
                        const auto* replyData = std::get_if<Reply>(&timed->result);
                        const auto decoded = replyData ? decodeReply(parameterCase.getId, replyData->data) : std::nullopt;
                        if (!decoded || *decoded != expectedValue)
                            throw CheckFailure("get " + title + ": read back "
                                               + (decoded ? valuesText(*decoded) : std::string("nothing decodable")) + ", expected "
                                               + valuesText(expectedValue));
                    }
                    finding(std::to_string(allProgramParameterCases().size()) + " parameter items of the five groups round-tripped");

                    if (program.hadOriginalProgram())
                    {
                        expect(program.selectOriginalProgram(), "navigated away to the program that was current before");
                        bool refused = false;
                        try
                        {
                            program.expectOnTestProgram("set crossfade off (on the wrong program)",
                                                        [](Session& session, CommandCompletion done) {
                                                            setKeygroupCrossfade(session, false, std::move(done));
                                                        });
                        }
                        catch (const CheckFailure&)
                        {
                            refused = true;
                        }
                        expect(refused, "acting on the program that is current but not the test one was refused before sending");
                        expect(program.selectTestProgramAgain(), "reselected the test program");
                    }
                    else
                        finding("no program was current before: the wrong-program refusal is not exercised this run");
                }
                finding("test program deleted and the original selection restored by the guard");

                const auto after = awaitCompletion<ProgramCountResult>(
                    _rig.driver, _rig.commandPatience(),
                    [&guarded](ProgramCountCompletion done) { getProgramCount(guarded.session(), std::move(done)); });
                expect(after && after->result.count == countBefore,
                       "the number of programs is back to what it was before (" + std::to_string(countBefore) + ")");
                closeAndVerify(guarded);
            }

            // RQ-AKM-027: a program check that fails half way, with the test program current, still leaves the
            // sampler exactly as it held it before — the failure is thrown through the guard's scope, as a failed
            // assertion would be.
            void failedProgramCheckLeavesTheKnownState()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());

                const auto namesBeforeResult = awaitCompletion<AllProgramNamesResult>(
                    _rig.driver, _rig.commandPatience(),
                    [&guarded](AllProgramNamesCompletion done) { getAllProgramNames(guarded.session(), std::move(done)); });
                if (!namesBeforeResult || !namesBeforeResult->result.names)
                    throw CheckFailure("could not read the names of all programs before the test program is created");
                const std::vector<std::string> namesBefore = *namesBeforeResult->result.names;
                const auto originalResult = awaitCompletion<ProgramNameResult>(
                    _rig.driver, _rig.commandPatience(),
                    [&guarded](ProgramNameCompletion done) { getCurrentProgramName(guarded.session(), std::move(done)); });
                if (!originalResult)
                    throw CheckFailure("could not read the current program's name before the test program is created");
                const std::optional<std::string> originalName = originalResult->result.name;

                bool cleanedUp = false;
                try
                {
                    GuardedTestProgram program(_rig, guarded.session());
                    finding("test program created for a check that fails on purpose");
                    throw CheckFailure("this check fails on purpose, with the test program current");
                }
                catch (const CheckFailure& failure)
                {
                    // The GuardedTestProgram above has already been destroyed, its cleanup already run, by the time
                    // the exception reaches this catch clause: that is what stack unwinding does.
                    cleanedUp = true;
                    _rig.log.note(std::string("  the check failed: ") + failure.what());
                }
                expect(cleanedUp, "the guard's destructor ran when the check failed");

                const auto namesAfterResult = awaitCompletion<AllProgramNamesResult>(
                    _rig.driver, _rig.commandPatience(),
                    [&guarded](AllProgramNamesCompletion done) { getAllProgramNames(guarded.session(), std::move(done)); });
                expect(namesAfterResult && namesAfterResult->result.names == namesBefore,
                       "the sampler holds exactly the programs it held before (" + std::to_string(namesBefore.size()) + ")");
                const auto currentAfterResult = awaitCompletion<ProgramNameResult>(
                    _rig.driver, _rig.commandPatience(),
                    [&guarded](ProgramNameCompletion done) { getCurrentProgramName(guarded.session(), std::move(done)); });
                expect(currentAfterResult && currentAfterResult->result.name == originalName,
                       "the program that was current before is current again");
                closeAndVerify(guarded);
            }

            // Sets one parameter case on the test program, reads it back and fails unless the value read is the
            // one set (the selector being the first values the Get takes). Shared by the keygroup and zone checks.
            template <typename ParameterCase>
            void roundTripParameterCase(GuardedSession& guarded, GuardedTestProgram& program, const ParameterCase& parameterCase)
            {
                const ItemDescriptor& getDescriptor = descriptor(parameterCase.getId);
                const auto selectorCount = static_cast<std::ptrdiff_t>(getDescriptor.args.size());
                const std::vector<std::int64_t> selector(parameterCase.values.begin(), parameterCase.values.begin() + selectorCount);
                const std::vector<std::int64_t> expectedValue(parameterCase.values.begin() + selectorCount, parameterCase.values.end());
                const std::string title(descriptor(parameterCase.setId).name);

                program.expectOnTestProgram("set " + title, [&parameterCase](Session& session, CommandCompletion done) {
                    session.submit(makeRequest(parameterCase.setId, parameterCase.values), std::move(done));
                });
                const auto timed = awaitCompletion<CommandResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded, &parameterCase, &selector](CommandCompletion done) {
                        guarded.session().submit(makeRequest(parameterCase.getId, selector), std::move(done));
                    });
                if (!timed)
                    throw CheckFailure("get " + title + ": no completion within " + millisecondsText(_rig.commandPatience()));
                if (!succeeded(timed->result))
                    throw CheckFailure("get " + title + ": " + outcomeText(timed->result));
                const auto* replyData = std::get_if<Reply>(&timed->result);
                const auto decoded = replyData ? decodeReply(parameterCase.getId, replyData->data) : std::nullopt;
                if (!decoded || *decoded != expectedValue)
                    throw CheckFailure("get " + title + ": read back "
                                       + (decoded ? valuesText(*decoded) : std::string("nothing decodable")) + ", expected "
                                       + valuesText(expectedValue));
            }

            // The wrong-program guard: with a program current that is not the test one, a command on the test
            // program must be refused before it is sent. Not exercised when no program was current before.
            void expectRefusedOnWrongProgram(GuardedTestProgram& program, const std::string& title,
                                             const std::function<void(Session&, CommandCompletion)>& launch,
                                             const std::string& refusalMessage)
            {
                if (!program.hadOriginalProgram())
                {
                    finding("no program was current before: the wrong-program refusal is not exercised this run");
                    return;
                }
                expect(program.selectOriginalProgram(), "navigated away to the program that was current before");
                bool refused = false;
                try
                {
                    program.expectOnTestProgram(title, launch);
                }
                catch (const CheckFailure&)
                {
                    refused = true;
                }
                expect(refused, refusalMessage);
                expect(program.selectTestProgramAgain(), "reselected the test program");
            }

            // RQ-AKM-028, RQ-AKM-030, RQ-AKM-031, RQ-AKM-033: keygroups added to the test program, every
            // §08 parameter item round-tripped on one of them, then the keygroup-0 ("all") shape, and the
            // wrong-program refusal for a keygroup-level command — the same guard as the program lifecycle
            // check, so acting on a program the suite did not create is refused before sending.
            void keygroupsOnTestProgram()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());

                const auto before = awaitCompletion<ProgramCountResult>(
                    _rig.driver, _rig.commandPatience(),
                    [&guarded](ProgramCountCompletion done) { getProgramCount(guarded.session(), std::move(done)); });
                if (!before || !before->result.count.has_value())
                    throw CheckFailure("could not read the number of programs before creating the test program");
                const int countBefore = *before->result.count;

                // The guard's own destructor (reselect, delete, restore the original selection) must run before
                // closeAndVerify below closes the session it needs for that — hence this nested scope, the same
                // shape programLifecycleOnTestProgram uses. Missing it here first (found on the real S5000, not the
                // mock: TASK-AKM-033) left the test program undeleted, the guard's cleanup silently failing against
                // an already-closed session.
                {
                GuardedTestProgram program(_rig, guarded.session());
                finding("test program \"" + std::string(TEST_PROGRAM_NAME) + "\" created and current");

                program.expectOnTestProgram("add 2 keygroups", [](Session& session, CommandCompletion done) {
                    addKeygroupsToProgram(session, 2, std::move(done));
                });
                const auto keygroupCountResult = awaitCompletion<ProgramKeygroupCountResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](ProgramKeygroupCountCompletion done) {
                        getProgramKeygroupCount(guarded.session(), std::move(done));
                    });
                if (!keygroupCountResult || !keygroupCountResult->result.count.has_value())
                    throw CheckFailure("could not read the keygroup count after adding keygroups");
                const int keygroupCount = *keygroupCountResult->result.count;
                expect(keygroupCount == 3, "the keygroup count read back is 3 (1 default + 2 added)");

                program.expectOnTestProgram("select keygroup 2", [](Session& session, CommandCompletion done) {
                    selectKeygroup(session, 2, std::move(done));
                });
                const auto currentResult = awaitCompletion<CurrentKeygroupResult>(
                    _rig.driver, _rig.commandPatience(),
                    [&guarded](CurrentKeygroupCompletion done) { getCurrentKeygroup(guarded.session(), std::move(done)); });
                expect(currentResult && currentResult->result.keygroup == 2, "keygroup 2 read back as current");

                for (const KeygroupParameterCase& parameterCase : allKeygroupParameterCases())
                    roundTripParameterCase(guarded, program, parameterCase);
                finding(std::to_string(allKeygroupParameterCases().size())
                        + " keygroup parameter items of the six groups round-tripped on keygroup 2");

                // RQ-AKM-032: whether &61 (Velocity->Rate, Aux Rate 4) and &64 (Off Velocity->Rate, Aux Rate 4
                // only) share one stored value or are independent — still open after TASK-AKM-033's first run,
                // which never set both at Aux Rate 4. Observational only: it does not fail the check either way.
                program.expectOnTestProgram("set Aux Env. Velocity->Rate (Aux Rate 4) to a marker value",
                                            [](Session& session, CommandCompletion done) {
                                                session.submit(makeRequest(ItemId::KeygroupSetAuxEnvVelocityToRate, {4, 1, 99}),
                                                               std::move(done));
                                            });
                program.expectOnTestProgram("set Aux Env. Off Velocity->Rate (Aux Rate 4) to a different marker value",
                                            [](Session& session, CommandCompletion done) {
                                                session.submit(makeRequest(ItemId::KeygroupSetAuxEnvOffVelocityToRate, {4, 0, 5}),
                                                               std::move(done));
                                            });
                const auto velocityToRateAgain = awaitCompletion<CommandResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](CommandCompletion done) {
                        guarded.session().submit(makeRequest(ItemId::KeygroupGetAuxEnvVelocityToRate, {4}), std::move(done));
                    });
                const auto offVelocityToRateAgain = awaitCompletion<CommandResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](CommandCompletion done) {
                        guarded.session().submit(makeRequest(ItemId::KeygroupGetAuxEnvOffVelocityToRate, {4}), std::move(done));
                    });
                const auto* velocityReply = velocityToRateAgain ? std::get_if<Reply>(&velocityToRateAgain->result) : nullptr;
                const auto* offVelocityReply = offVelocityToRateAgain ? std::get_if<Reply>(&offVelocityToRateAgain->result) : nullptr;
                const auto velocityDecoded = velocityReply ? decodeReply(ItemId::KeygroupGetAuxEnvVelocityToRate, velocityReply->data) : std::nullopt;
                const auto offVelocityDecoded =
                    offVelocityReply ? decodeReply(ItemId::KeygroupGetAuxEnvOffVelocityToRate, offVelocityReply->data) : std::nullopt;
                finding("Aux Rate 4 cross-check: &69 (Velocity->Rate) reads "
                        + (velocityDecoded ? valuesText(*velocityDecoded) : std::string("nothing decodable")) + ", &6C (Off Velocity->Rate) reads "
                        + (offVelocityDecoded ? valuesText(*offVelocityDecoded) : std::string("nothing decodable"))
                        + " -- distinct if 1 99 and 0 5 respectively, aliased if both read the same value");

                // RQ-AKM-031: keygroup 0 ("all") — Set Low Note once, Get it back for every keygroup.
                program.expectOnTestProgram("select keygroup 0 (all)", [](Session& session, CommandCompletion done) {
                    selectKeygroup(session, 0, std::move(done));
                });
                program.expectOnTestProgram("set Low Note for all keygroups", [](Session& session, CommandCompletion done) {
                    session.submit(makeRequest(ItemId::KeygroupSetLowNote, {50}), std::move(done));
                });
                const auto allResult = awaitCompletion<AllKeygroupsResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded, keygroupCount](AllKeygroupsCompletion done) {
                        getForAllKeygroups(guarded.session(), ItemId::KeygroupGetLowNote, keygroupCount, std::move(done));
                    });
                const bool allLowNote50 = allResult && allResult->result.values
                                           && allResult->result.values->size() == static_cast<std::size_t>(keygroupCount)
                                           && std::all_of(allResult->result.values->begin(), allResult->result.values->end(),
                                                          [](const std::vector<std::int64_t>& record) {
                                                              return record == std::vector<std::int64_t>{50};
                                                          });
                expect(allLowNote50, "all " + std::to_string(keygroupCount) + " keygroups read back Low Note 50");

                expectRefusedOnWrongProgram(
                    program, "select keygroup 0 (on the wrong program)",
                    [](Session& session, CommandCompletion done) { selectKeygroup(session, 0, std::move(done)); },
                    "selecting keygroup 0 on the program that is current but not the test one was refused before sending");
                }
                finding("test program deleted and the original selection restored by the guard");

                const auto after = awaitCompletion<ProgramCountResult>(
                    _rig.driver, _rig.commandPatience(),
                    [&guarded](ProgramCountCompletion done) { getProgramCount(guarded.session(), std::move(done)); });
                expect(after && after->result.count == countBefore,
                       "the number of programs is back to what it was before (" + std::to_string(countBefore) + ")");
                closeAndVerify(guarded);
            }

            // RQ-AKM-034 to RQ-AKM-038: a keygroup added to the test program, every non-sample §06 parameter
            // item round-tripped on one of its zones, then the zone-0 ("all four") and keygroup-0 + zone-0
            // shapes (TASK-AKM-037), sample assignment when `_rig.options.sampleName` is given (skipped, not
            // failed, otherwise), and the wrong-program refusal for a zone-level command — the same guard as
            // the other test-program checks.
            void zonesOnTestProgram()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());

                const auto before = awaitCompletion<ProgramCountResult>(
                    _rig.driver, _rig.commandPatience(),
                    [&guarded](ProgramCountCompletion done) { getProgramCount(guarded.session(), std::move(done)); });
                if (!before || !before->result.count.has_value())
                    throw CheckFailure("could not read the number of programs before creating the test program");
                const int countBefore = *before->result.count;

                {
                GuardedTestProgram program(_rig, guarded.session());
                finding("test program \"" + std::string(TEST_PROGRAM_NAME) + "\" created and current");

                program.expectOnTestProgram("add 1 keygroup", [](Session& session, CommandCompletion done) {
                    addKeygroupsToProgram(session, 1, std::move(done));
                });
                const auto keygroupCountResult = awaitCompletion<ProgramKeygroupCountResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](ProgramKeygroupCountCompletion done) {
                        getProgramKeygroupCount(guarded.session(), std::move(done));
                    });
                if (!keygroupCountResult || !keygroupCountResult->result.count.has_value())
                    throw CheckFailure("could not read the keygroup count after adding a keygroup");
                const int keygroupCount = *keygroupCountResult->result.count;
                expect(keygroupCount == 2, "the keygroup count read back is 2 (1 default + 1 added)");

                program.expectOnTestProgram("select keygroup 2", [](Session& session, CommandCompletion done) {
                    selectKeygroup(session, 2, std::move(done));
                });

                for (const ZoneParameterCase& parameterCase : allZoneParameterCases())
                    roundTripParameterCase(guarded, program, parameterCase);
                finding(std::to_string(allZoneParameterCases().size()) + " zone parameter items round-tripped on zone 3 of keygroup 2");

                // RQ-AKM-036: zone 0 ("all four") on the current keygroup.
                program.expectOnTestProgram("set Zone Level for all zones (zone 0) of keygroup 2",
                                            [](Session& session, CommandCompletion done) {
                                                session.submit(makeRequest(ItemId::ZoneSetLevel, {0, 0, 77}), std::move(done));
                                            });
                const auto allZonesResult = awaitCompletion<AllZonesResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](AllZonesCompletion done) {
                        getForAllZones(guarded.session(), ItemId::ZoneGetLevel, ZONE_COUNT, std::move(done));
                    });
                const bool allZonesLevel77 = allZonesResult && allZonesResult->result.values
                                             && allZonesResult->result.values->size() == static_cast<std::size_t>(ZONE_COUNT)
                                             && std::all_of(allZonesResult->result.values->begin(), allZonesResult->result.values->end(),
                                                            [](const std::vector<std::int64_t>& record) {
                                                                return record == std::vector<std::int64_t>{0, 77};
                                                            });
                expect(allZonesLevel77, "all " + std::to_string(ZONE_COUNT) + " zones of keygroup 2 read back Level 77");

                // RQ-AKM-036: keygroup 0 ("all") + zone 0 ("all four") together.
                program.expectOnTestProgram("select keygroup 0 (all)", [](Session& session, CommandCompletion done) {
                    selectKeygroup(session, 0, std::move(done));
                });
                program.expectOnTestProgram("set Zone Level for all zones of all keygroups (keygroup 0, zone 0)",
                                            [](Session& session, CommandCompletion done) {
                                                session.submit(makeRequest(ItemId::ZoneSetLevel, {0, 0, 88}), std::move(done));
                                            });
                const auto nestedResult = awaitCompletion<AllZonesAllKeygroupsResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded, keygroupCount](AllZonesAllKeygroupsCompletion done) {
                        getForAllZonesAllKeygroups(guarded.session(), ItemId::ZoneGetLevel, keygroupCount, ZONE_COUNT, std::move(done));
                    });
                const bool nestedLevel88 =
                    nestedResult && nestedResult->result.values && nestedResult->result.values->size() == static_cast<std::size_t>(keygroupCount)
                    && std::all_of(nestedResult->result.values->begin(), nestedResult->result.values->end(),
                                   [](const std::vector<std::vector<std::int64_t>>& perKeygroup) {
                                       return perKeygroup.size() == static_cast<std::size_t>(ZONE_COUNT)
                                              && std::all_of(perKeygroup.begin(), perKeygroup.end(), [](const std::vector<std::int64_t>& record) {
                                                     return record == std::vector<std::int64_t>{0, 88};
                                                 });
                                   });
                expect(nestedLevel88, "all " + std::to_string(keygroupCount) + " keygroups' " + std::to_string(ZONE_COUNT)
                                          + " zones read back Level 88");

                // RQ-AKM-035, RQ-AKM-038: sample assignment, only when the owner configured a real sample name.
                if (_rig.options.sampleName)
                {
                    program.expectOnTestProgram("select keygroup 2", [](Session& session, CommandCompletion done) {
                        selectKeygroup(session, 2, std::move(done));
                    });
                    const std::string& sampleName = *_rig.options.sampleName;
                    program.expectOnTestProgram("assign sample \"" + sampleName + "\" to zone 1",
                                                [&sampleName](Session& session, CommandCompletion done) {
                                                    setZoneSample(session, 1, sampleName, std::move(done));
                                                });
                    const auto sampleResult = awaitCompletion<ZoneSampleResult>(
                        _rig.driver, _rig.commandPatience(), [&guarded](ZoneSampleCompletion done) { getZoneSample(guarded.session(), 1, std::move(done)); });
                    expect(sampleResult && sampleResult->result.name == sampleName,
                           "zone 1 reads back the sample name \"" + sampleName + "\"");
                    finding("sample assignment: \"" + sampleName + "\" assigned to zone 1 and read back");
                }
                else
                    finding("sample assignment: skipped (no --sample-name given)");

                expectRefusedOnWrongProgram(
                    program, "set Zone Level (on the wrong program)",
                    [](Session& session, CommandCompletion done) {
                        session.submit(makeRequest(ItemId::ZoneSetLevel, {1, 0, 1}), std::move(done));
                    },
                    "a zone command on the program that is current but not the test one was refused before sending");
                }
                finding("test program deleted and the original selection restored by the guard");

                const auto after = awaitCompletion<ProgramCountResult>(
                    _rig.driver, _rig.commandPatience(),
                    [&guarded](ProgramCountCompletion done) { getProgramCount(guarded.session(), std::move(done)); });
                expect(after && after->result.count == countBefore,
                       "the number of programs is back to what it was before (" + std::to_string(countBefore) + ")");
                closeAndVerify(guarded);
            }

            // RQ-AKM-051: selects the sample named by `--sample-name` (skipped, not failed, when empty —
            // there is nothing else for this check to test), renames it and back, starts and stops
            // auditioning it, round-trips every settable item on it (RQ-AKM-048), confirms the grouped
            // replies agree with the items they group (RQ-AKM-049), then lets the guard restore its
            // name and parameters and the sampler's original current-sample selection, verified again
            // once the guard is gone.
            void samplesOnTestSample()
            {
                if (!_rig.options.sampleName)
                    throw CheckSkipped("no --sample-name given");
                const std::string& sampleName = *_rig.options.sampleName;

                GuardedSession guarded(_rig);
                guarded.open(baseConfig());

                std::vector<std::int64_t> originalSnapshot;
                {
                    GuardedTestSample sample(_rig, guarded.session(), sampleName);
                    originalSnapshot = sample.originalParameters();
                    finding("sample \"" + sampleName + "\" selected as current");

                    // RQ-AKM-045: select by index too, not just by name — read the current index
                    // (&13), select by it (&06), and confirm by name (&14) that it is still the test
                    // sample.
                    const auto indexBefore = awaitCompletion<SampleIndexResult>(
                        _rig.driver, _rig.commandPatience(),
                        [&guarded](SampleIndexCompletion done) { getCurrentSampleIndex(guarded.session(), std::move(done)); });
                    if (!indexBefore || !indexBefore->result.index)
                        throw CheckFailure("could not read the test sample's current index (&13)");
                    sample.expectOnTestSample("select by index", [&indexBefore](Session& session, CommandCompletion done) {
                        selectSampleByIndex(session, *indexBefore->result.index, std::move(done));
                    });
                    const auto nameAfterIndex = awaitCompletion<SampleNameResult>(
                        _rig.driver, _rig.commandPatience(),
                        [&guarded](SampleNameCompletion done) { getCurrentSampleName(guarded.session(), std::move(done)); });
                    expect(nameAfterIndex && nameAfterIndex->result.name == sampleName,
                           "select by index (&06) landed back on the test sample, confirmed by &14");

                    // RQ-AKM-045: rename, then rename back immediately, so the guard's fixed anchor
                    // (sampleName) still finds it if anything below throws.
                    const std::string renamedTo = sampleName + "_2";
                    sample.expectOnTestSample("rename the test sample", [&renamedTo](Session& session, CommandCompletion done) {
                        renameCurrentSample(session, renamedTo, std::move(done));
                    });
                    const auto renamedResult = awaitCompletion<SampleNameResult>(
                        _rig.driver, _rig.commandPatience(),
                        [&guarded](SampleNameCompletion done) { getCurrentSampleName(guarded.session(), std::move(done)); });
                    expect(renamedResult && renamedResult->result.name == renamedTo, "the new name read back");
                    const auto renameBack = awaitCompletion<CommandResult>(
                        _rig.driver, _rig.commandPatience(), [&guarded, &sampleName](CommandCompletion done) {
                            renameCurrentSample(guarded.session(), sampleName, std::move(done));
                        });
                    if (!renameBack || !succeeded(renameBack->result))
                        throw CheckFailure("could not rename the test sample back to \"" + sampleName + "\"");

                    // RQ-AKM-045: audition start/stop.
                    sample.expectOnTestSample("start auditioning", [](Session& session, CommandCompletion done) {
                        startSampleAudition(session, std::move(done));
                    });
                    sample.expectOnTestSample("stop auditioning", [](Session& session, CommandCompletion done) {
                        stopSampleAudition(session, std::move(done));
                    });

                    // RQ-AKM-048: every settable parameter item, Set then Get, verified by read-back.
                    for (const SampleParameterCase& parameterCase : allSampleParameterCases())
                    {
                        const ItemDescriptor& getDescriptor = descriptor(parameterCase.getId);
                        const auto selectorCount = static_cast<std::ptrdiff_t>(getDescriptor.args.size());
                        const std::vector<std::int64_t> selector(parameterCase.values.begin(), parameterCase.values.begin() + selectorCount);
                        const std::vector<std::int64_t> expectedValue(parameterCase.values.begin() + selectorCount, parameterCase.values.end());
                        const std::string title(descriptor(parameterCase.setId).name);

                        sample.expectOnTestSample("set " + title, [&parameterCase](Session& session, CommandCompletion done) {
                            session.submit(makeRequest(parameterCase.setId, parameterCase.values), std::move(done));
                        });
                        const auto timed = awaitCompletion<CommandResult>(
                            _rig.driver, _rig.commandPatience(), [&guarded, &parameterCase, &selector](CommandCompletion done) {
                                guarded.session().submit(makeRequest(parameterCase.getId, selector), std::move(done));
                            });
                        if (!timed)
                            throw CheckFailure("get " + title + ": no completion within " + millisecondsText(_rig.commandPatience()));
                        if (!succeeded(timed->result))
                            throw CheckFailure("get " + title + ": " + outcomeText(timed->result));
                        const auto* replyData = std::get_if<Reply>(&timed->result);
                        const auto decoded = replyData ? decodeReply(parameterCase.getId, replyData->data) : std::nullopt;
                        if (!decoded || *decoded != expectedValue)
                            throw CheckFailure("get " + title + ": read back "
                                               + (decoded ? valuesText(*decoded) : std::string("nothing decodable")) + ", expected "
                                               + valuesText(expectedValue));
                    }
                    finding(std::to_string(allSampleParameterCases().size()) + " settable sample parameter items round-tripped");

                    // RQ-AKM-049: the grouped replies agree with the items they group.
                    std::vector<std::int64_t> basicParams;
                    for (const ItemId id :
                        {ItemId::SampleGetType, ItemId::SampleGetChannels, ItemId::SampleGetLength, ItemId::SampleGetRate})
                    {
                        const auto timed = awaitCompletion<CommandResult>(
                            _rig.driver, _rig.commandPatience(), [&guarded, id](CommandCompletion done) {
                                guarded.session().submit(makeRequest(id, {}), std::move(done));
                            });
                        const auto* replyData = timed ? std::get_if<Reply>(&timed->result) : nullptr;
                        const auto decoded = replyData ? decodeReply(id, replyData->data) : std::nullopt;
                        if (!decoded)
                            throw CheckFailure("get " + std::string(descriptor(id).name) + ": nothing decodable");
                        appendAll(basicParams, *decoded);
                    }
                    const auto groupedBasic = awaitCompletion<CommandResult>(
                        _rig.driver, _rig.commandPatience(), [&guarded](CommandCompletion done) {
                            guarded.session().submit(makeRequest(ItemId::SampleGetAllBasicParams, {}), std::move(done));
                        });
                    const auto* groupedBasicReply = groupedBasic ? std::get_if<Reply>(&groupedBasic->result) : nullptr;
                    const auto decodedGroupedBasic =
                        groupedBasicReply ? decodeReply(ItemId::SampleGetAllBasicParams, groupedBasicReply->data) : std::nullopt;
                    expect(decodedGroupedBasic && *decodedGroupedBasic == basicParams,
                           "&34 decodes to the same values as &30-&33 read individually");

                    std::vector<std::int64_t> settableParams;
                    for (const ItemId id :
                        {ItemId::SampleGetStartPosition, ItemId::SampleGetEndPosition, ItemId::SampleGetOriginalPitch,
                         ItemId::SampleGetSemitoneTune, ItemId::SampleGetFineTune, ItemId::SampleGetPlaybackMode,
                         ItemId::SampleGetLoopStart, ItemId::SampleGetLoopEnd})
                    {
                        const auto timed = awaitCompletion<CommandResult>(
                            _rig.driver, _rig.commandPatience(), [&guarded, id](CommandCompletion done) {
                                guarded.session().submit(makeRequest(id, {}), std::move(done));
                            });
                        const auto* replyData = timed ? std::get_if<Reply>(&timed->result) : nullptr;
                        const auto decoded = replyData ? decodeReply(id, replyData->data) : std::nullopt;
                        if (!decoded)
                            throw CheckFailure("get " + std::string(descriptor(id).name) + ": nothing decodable");
                        appendAll(settableParams, *decoded);
                    }
                    const auto groupedSettable = awaitCompletion<CommandResult>(
                        _rig.driver, _rig.commandPatience(), [&guarded](CommandCompletion done) {
                            guarded.session().submit(makeRequest(ItemId::SampleGetAllSettableParams, {}), std::move(done));
                        });
                    const auto* groupedSettableReply = groupedSettable ? std::get_if<Reply>(&groupedSettable->result) : nullptr;
                    const auto decodedGroupedSettable =
                        groupedSettableReply ? decodeReply(ItemId::SampleGetAllSettableParams, groupedSettableReply->data) : std::nullopt;
                    expect(decodedGroupedSettable && *decodedGroupedSettable == settableParams,
                           "&4B decodes to the same values as &40-&4A read individually");

                    if (sample.hadOriginalSample())
                    {
                        expect(sample.selectOriginalSample(), "navigated away to the sample that was current before");
                        bool refused = false;
                        try
                        {
                            sample.expectOnTestSample("start auditioning (on the wrong sample)",
                                                      [](Session& session, CommandCompletion done) {
                                                          startSampleAudition(session, std::move(done));
                                                      });
                        }
                        catch (const CheckFailure&)
                        {
                            refused = true;
                        }
                        expect(refused, "acting on the sample that is current but not the test one was refused before sending");
                        expect(sample.selectTestSampleAgain(), "reselected the test sample");
                    }
                    else
                        finding("no sample was current before: the wrong-sample refusal is not exercised this run");
                }
                finding("test sample's name and settable parameters restored by the guard");

                const auto reselected = awaitCompletion<CommandResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded, &sampleName](CommandCompletion done) {
                        selectSampleByName(guarded.session(), sampleName, std::move(done));
                    });
                expect(reselected && succeeded(reselected->result), "the test sample can still be selected by its original name");

                const auto nameAfter = awaitCompletion<SampleNameResult>(
                    _rig.driver, _rig.commandPatience(),
                    [&guarded](SampleNameCompletion done) { getCurrentSampleName(guarded.session(), std::move(done)); });
                expect(nameAfter && nameAfter->result.name == sampleName, "its name is back to \"" + sampleName + "\"");

                const auto paramsAfter = awaitCompletion<CommandResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](CommandCompletion done) {
                        guarded.session().submit(makeRequest(ItemId::SampleGetAllSettableParams, {}), std::move(done));
                    });
                const auto* paramsAfterReply = paramsAfter ? std::get_if<Reply>(&paramsAfter->result) : nullptr;
                const auto decodedParamsAfter =
                    paramsAfterReply ? decodeReply(ItemId::SampleGetAllSettableParams, paramsAfterReply->data) : std::nullopt;
                expect(decodedParamsAfter && *decodedParamsAfter == originalSnapshot,
                       "its settable parameters are back to what they were before");

                closeAndVerify(guarded);
            }

            // One question to the owner, said in the log with the answer; declined, the check is skipped (the guards put back
            // what it changed). [RQ-AKM-080]
            [[nodiscard]] std::size_t ownerChooses(const std::string& question, const std::vector<std::string>& choices)
            {
                _rig.log.flush();
                _rig.log.note("  asking the owner: " + question);
                _rig.log.flush();
                const auto picked = _rig.options.askOwnerChoice(question, choices);
                if (!picked || *picked >= choices.size())
                    throw CheckSkipped("the owner did not answer: " + question);
                _rig.log.note("  the owner answers: " + choices[*picked]);
                return *picked;
            }

            [[nodiscard]] int ownerNumber(const std::string& question, int minimum, int maximum)
            {
                _rig.log.flush();
                _rig.log.note("  asking the owner: " + question);
                _rig.log.flush();
                const auto number = _rig.options.askOwnerNumber(question, minimum, maximum);
                if (!number || *number < minimum || *number > maximum)
                    throw CheckSkipped("the owner did not answer: " + question);
                _rig.log.note("  the owner answers: " + std::to_string(*number));
                return *number;
            }

            // The channel labels of the sampler's screen, in the order of the channel codes 0-31.
            [[nodiscard]] static std::vector<std::string> midiChannelChoices()
            {
                std::vector<std::string> names;
                for (int channel = 0; channel < MIDI_CHANNELS; ++channel)
                    names.push_back(midiChannelName(channel));
                return names;
            }

            // RQ-AKM-080: what the owner says the MIDI SETUP and MIDI FILTER pages show, asked once per run and nothing sent
            // before it is known. The first decline skips this check and the next one without asking again.
            [[nodiscard]] MidiConfigDeclaration midiConfigDeclaration()
            {
                if (_midiDeclined)
                    throw CheckSkipped("the owner declined to declare the sampler's MIDI setup");
                if (_midiDeclaration)
                    return *_midiDeclaration;
                if (!_rig.options.askOwner || !_rig.options.askOwnerChoice || !_rig.options.askOwnerNumber)
                    throw CheckSkipped("section 04 cannot be read back, so this check needs the owner to declare the sampler's MIDI "
                                       "setup, and there is no way to ask");
                try
                {
                    ownerConfirms("Press UTILITIES, then MIDI SETUP, and note what PROGRAM CHANGE, MULTI SELECT, MULTI SLCT CH, "
                                  "EXT APM CONTROL and AFTERTOUCH show; then open MIDI FILTER and choose one filter to look at "
                                  "(its event type and its channel) and note whether it is on or off. You will be asked for these "
                                  "values now, and the check puts them back at the end. Nothing is sent before you have answered.");
                    MidiConfigDeclaration declared;
                    declared.programChangeEnabled =
                        ownerChooses("MIDI SETUP, PROGRAM CHANGE: what does the sampler show now?", {"ON", "OFF"}) == 0;
                    declared.multiSelect = static_cast<MultiSelectMode>(ownerChooses(
                        "MIDI SETUP, MULTI SELECT: what does the sampler show now?",
                        {multiSelectName(MultiSelectMode::Off), multiSelectName(MultiSelectMode::ProgramChange),
                         multiSelectName(MultiSelectMode::Bank)}));
                    declared.multiSelectChannel = static_cast<int>(
                        ownerChooses("MIDI SETUP, MULTI SLCT CH: which channel does the sampler show now?", midiChannelChoices()));
                    declared.externalApmController = ownerNumber(
                        "MIDI SETUP, EXT APM CONTROL: which controller number does the sampler show now?", 0,
                        EXTERNAL_APM_CONTROLLERS - 1);
                    declared.aftertouch = static_cast<AftertouchType>(ownerChooses(
                        "MIDI SETUP, AFTERTOUCH: what does the sampler show now?",
                        {aftertouchName(AftertouchType::Channel), aftertouchName(AftertouchType::Polyphonic)}));
                    declared.filterEvent = static_cast<MidiFilterEvent>(ownerChooses(
                        "MIDI FILTER, event type: which event type will the check exercise?",
                        {midiFilterEventName(MidiFilterEvent::NoteOn), midiFilterEventName(MidiFilterEvent::Aftertouch),
                         midiFilterEventName(MidiFilterEvent::Wheels), midiFilterEventName(MidiFilterEvent::Volume)}));
                    declared.filterChannel = static_cast<int>(
                        ownerChooses("MIDI FILTER, channel: on which channel?", midiChannelChoices()));
                    declared.filterAllows =
                        ownerChooses("MIDI FILTER, that filter: what does the sampler do with those messages now?",
                                     {"it allows them (they are received)", "it ignores them (they are filtered out)"}) == 0;
                    finding("declared by the owner: PROGRAM CHANGE " + onOffName(declared.programChangeEnabled) + ", MULTI SELECT "
                            + multiSelectName(declared.multiSelect) + ", MULTI SLCT CH " + midiChannelName(declared.multiSelectChannel)
                            + ", EXT APM CONTROL " + std::to_string(declared.externalApmController) + ", AFTERTOUCH "
                            + aftertouchName(declared.aftertouch) + ", filter " + midiFilterEventName(declared.filterEvent) + " on "
                            + midiChannelName(declared.filterChannel) + (declared.filterAllows ? " allows" : " ignores") + " its messages");
                    _midiDeclaration = declared;
                    return declared;
                }
                catch (const CheckSkipped&)
                {
                    _midiDeclined = true;
                    throw;
                }
            }

            // One setting, on its own: how to undo the change is registered, the change is sent, the owner says whether the
            // screen shows it, and the guard puts the setting back before the next one is touched — so that no setting is
            // tested while another is still changed (a setting may depend on another one: found on the real S5000 with
            // MULTI SELECT while PROGRAM CHANGE was off). A "no" is noted in `notSeen` and the check goes on with the other
            // settings; a declined question skips the check. [RQ-AKM-080]
            void changeMidiSetting(GuardedSession& guarded, std::vector<std::string>& notSeen, const std::string& setting,
                                   const std::string& declaredText, const std::string& newText,
                                   const GuardedMidiConfig::Launch& change, const GuardedMidiConfig::Launch& restore)
            {
                GuardedMidiConfig guard(_rig, guarded.session());
                guard.willRestore(setting + " (back to " + declaredText + ")", restore);
                expectCommand(guarded, "set " + setting + " to " + newText + " (the owner declared " + declaredText + ")", change);
                ownerSeesOrNotes(setting + " now shows " + newText, notSeen);
            }

            // The session of the MIDI checks: Auto screen update on (§00/&05, "automatic screen updating when a SysEx message
            // is processed"), so that the sampler redraws its MIDI SETUP and MIDI FILTER pages when a §04 item changes
            // them; the closing puts it back off. Without it the first real run saw only two of the seven items on the
            // screen. A run that must leave the LCD settings alone (--no-lcd) leaves this one too. [RQ-AKM-080]
            [[nodiscard]] SessionConfig midiConfigSessionConfig() const
            {
                SessionConfig config = baseConfig();
                if (_rig.options.touchLcdSettings)
                    config.autoScreenUpdate = SettingChoice::On;
                return config;
            }

            // The owner looks at the screen: a "no" is noted in `notSeen`, not thrown, so that the other settings are still
            // tried and the report names every one that was not seen. [RQ-AKM-080]
            void ownerSeesOrNotes(const std::string& what, std::vector<std::string>& notSeen)
            {
                // "Nothing changed" says the sampler took no notice of a command it answered DONE; "another value" says it
                // did something else: two different findings, kept apart in the log and in the report.
                const std::vector<std::string> choices{"Yes", "No, nothing changed on the screen", "No, the screen shows another value"};
                const std::size_t answer = ownerChooses("NOW LOOK AT THE SAMPLER: " + what + ". Does it?", choices);
                if (answer == 0)
                {
                    _rig.log.note("  as expected: the owner sees on the sampler: " + what);
                    return;
                }
                // A screen that is not redrawn by itself looks like a command that was ignored: the owner leaves the page and
                // opens it again, which redraws it from the sampler's own values, and looks once more.
                const std::size_t again = ownerChooses(
                    "LOOK AGAIN AT THE SAMPLER: press EXIT, open the page again (the screen may not redraw by itself), then look: " + what
                        + ". Does it now?",
                    {"Yes, after opening the page again", "No, still not"});
                if (again == 0)
                    finding("the screen showed it only after the page was opened again (not redrawn by itself): " + what);
                else
                {
                    _rig.log.note("  NOT MET: the owner sees on the sampler: " + what + " (" + choices[answer] + ", and still so after opening the page again)");
                    notSeen.push_back(what + " (" + choices[answer] + ", still so after opening the page again)");
                }
            }

            // RQ-AKM-078, RQ-AKM-079, RQ-AKM-080: every §04 item sent once, to a value other than the one the owner declared,
            // each confirmed on the sampler's own screen, then each put back to the declared value by the guard; the owner
            // confirms the original screens are back. §04 has no Get: the sampler's answer is DONE (queued), the screen is the
            // read-back.
            void midiConfigRoundTrips()
            {
                const MidiConfigDeclaration declared = midiConfigDeclaration();
                GuardedSession guarded(_rig);
                guarded.open(midiConfigSessionConfig());
                std::vector<std::string> notSeen;
                {
                    const bool newProgramChange = !declared.programChangeEnabled;
                    changeMidiSetting(guarded, notSeen, "MIDI SETUP, PROGRAM CHANGE", onOffName(declared.programChangeEnabled),
                                      onOffName(newProgramChange),
                                      [newProgramChange](Session& session, CommandCompletion done) {
                                          setProgramChangeEnabled(session, newProgramChange, std::move(done));
                                      },
                                      [declared](Session& session, CommandCompletion done) {
                                          setProgramChangeEnabled(session, declared.programChangeEnabled, std::move(done));
                                      });
                    const auto newMultiSelect = static_cast<MultiSelectMode>((static_cast<int>(declared.multiSelect) + 1) % MULTI_SELECT_MODES);
                    changeMidiSetting(guarded, notSeen, "MIDI SETUP, MULTI SELECT", multiSelectName(declared.multiSelect),
                                      multiSelectName(newMultiSelect),
                                      [newMultiSelect](Session& session, CommandCompletion done) {
                                          setMultiSelect(session, newMultiSelect, std::move(done));
                                      },
                                      [declared](Session& session, CommandCompletion done) {
                                          setMultiSelect(session, declared.multiSelect, std::move(done));
                                      });
                    const int newChannel = (declared.multiSelectChannel + 1) % MIDI_CHANNELS;
                    changeMidiSetting(guarded, notSeen, "MIDI SETUP, MULTI SLCT CH", midiChannelName(declared.multiSelectChannel),
                                      midiChannelName(newChannel),
                                      [newChannel](Session& session, CommandCompletion done) {
                                          setMultiSelectChannel(session, newChannel, std::move(done));
                                      },
                                      [declared](Session& session, CommandCompletion done) {
                                          setMultiSelectChannel(session, declared.multiSelectChannel, std::move(done));
                                      });
                    const int newController = (declared.externalApmController + 1) % EXTERNAL_APM_CONTROLLERS;
                    changeMidiSetting(guarded, notSeen, "MIDI SETUP, EXT APM CONTROL", std::to_string(declared.externalApmController),
                                      std::to_string(newController),
                                      [newController](Session& session, CommandCompletion done) {
                                          setExternalApmController(session, newController, std::move(done));
                                      },
                                      [declared](Session& session, CommandCompletion done) {
                                          setExternalApmController(session, declared.externalApmController, std::move(done));
                                      });
                    const AftertouchType newAftertouch =
                        declared.aftertouch == AftertouchType::Channel ? AftertouchType::Polyphonic : AftertouchType::Channel;
                    changeMidiSetting(guarded, notSeen, "MIDI SETUP, AFTERTOUCH", aftertouchName(declared.aftertouch),
                                      aftertouchName(newAftertouch),
                                      [newAftertouch](Session& session, CommandCompletion done) {
                                          setAftertouch(session, newAftertouch, std::move(done));
                                      },
                                      [declared](Session& session, CommandCompletion done) {
                                          setAftertouch(session, declared.aftertouch, std::move(done));
                                      });
                    const auto filterLaunch = [declared](bool allows) {
                        return [declared, allows](Session& session, CommandCompletion done) {
                            if (allows)
                                allowMidiEvents(session, declared.filterEvent, declared.filterChannel, std::move(done));
                            else
                                ignoreMidiEvents(session, declared.filterEvent, declared.filterChannel, std::move(done));
                        };
                    };
                    const std::string filterName = "MIDI FILTER, " + midiFilterEventName(declared.filterEvent) + " on "
                                                   + midiChannelName(declared.filterChannel);
                    changeMidiSetting(guarded, notSeen, filterName, declared.filterAllows ? "allowing" : "ignoring",
                                      declared.filterAllows ? "ignoring" : "allowing", filterLaunch(!declared.filterAllows),
                                      filterLaunch(declared.filterAllows));
                }
                ownerSeesOrNotes("the MIDI SETUP and MIDI FILTER pages show the values you declared again (every setting back as it was)",
                                 notSeen);
                closeAndVerify(guarded);
                if (!notSeen.empty())
                {
                    std::string list;
                    for (const std::string& what : notSeen)
                        list += (list.empty() ? "" : "; ") + what;
                    throw CheckFailure("not met: the owner did not see on the sampler: " + list);
                }
            }

            // RQ-AKM-080: a check that fails with a setting changed still puts it back. MULTI SELECT is changed, then the
            // check throws on purpose; the guard has restored it by the time the exception is caught, and the owner confirms.
            void failedMidiConfigCheckPutsBack()
            {
                const MidiConfigDeclaration declared = midiConfigDeclaration();
                GuardedSession guarded(_rig);
                guarded.open(midiConfigSessionConfig());

                bool cleanedUp = false;
                try
                {
                    GuardedMidiConfig guard(_rig, guarded.session());
                    const auto newMultiSelect = static_cast<MultiSelectMode>((static_cast<int>(declared.multiSelect) + 1) % MULTI_SELECT_MODES);
                    guard.willRestore("MIDI SETUP, MULTI SELECT (back to " + multiSelectName(declared.multiSelect) + ")",
                                      [declared](Session& session, CommandCompletion done) {
                                          setMultiSelect(session, declared.multiSelect, std::move(done));
                                      });
                    expectCommand(guarded, "set MIDI SETUP, MULTI SELECT to " + multiSelectName(newMultiSelect),
                                  [newMultiSelect](Session& session, CommandCompletion done) {
                                      setMultiSelect(session, newMultiSelect, std::move(done));
                                  });
                    throw CheckFailure("this check fails on purpose, with MULTI SELECT changed");
                }
                catch (const CheckFailure& failure)
                {
                    // The guard above has already been destroyed, its restoration already run, by the time the
                    // exception reaches this catch clause: that is what stack unwinding does.
                    cleanedUp = true;
                    _rig.log.note(std::string("  the check failed: ") + failure.what());
                }
                expect(cleanedUp, "the guard's destructor ran when the check failed");
                std::vector<std::string> notSeen;
                ownerSeesOrNotes("MIDI SETUP, MULTI SELECT shows " + multiSelectName(declared.multiSelect) + " again", notSeen);
                closeAndVerify(guarded);
                if (!notSeen.empty())
                    throw CheckFailure("not met: the owner did not see on the sampler: " + notSeen.front());
            }

            // Reads one value out of a result of the multi check: the member of the result that holds it, or a failure that
            // names `what`. [RQ-AKM-089, RQ-AKM-091]
            template <typename Result, typename Member, typename Launch>
            auto readMultiValue(const std::string& what, Member member, Launch launch)
            {
                const auto timed = awaitCompletion<Result>(_rig.driver, _rig.commandPatience(), launch);
                if (!timed || !(timed->result.*member).has_value())
                    throw CheckFailure("could not read " + what + ": " + (timed ? outcomeText(timed->result.outcome) : std::string("no completion")));
                return *(timed->result.*member);
            }

            std::string multiNameNow(GuardedSession& guarded)
            {
                return readMultiValue<MultiNameResult>("the current multi's name (&43)", &MultiNameResult::name,
                                                       [&guarded](MultiNameCompletion done) { getCurrentMultiName(guarded.session(), std::move(done)); });
            }

            int multiIndexNow(GuardedSession& guarded)
            {
                return readMultiValue<MultiIndexResult>("the current multi's index (&42)", &MultiIndexResult::index,
                                                        [&guarded](MultiIndexCompletion done) { getCurrentMultiIndex(guarded.session(), std::move(done)); });
            }

            std::string multiPartNameNow(GuardedSession& guarded, int part)
            {
                return readMultiValue<MultiNameResult>("the name of part " + std::to_string(part) + " (&45)", &MultiNameResult::name,
                                                       [&guarded, part](MultiNameCompletion done) { getMultiPartName(guarded.session(), part, std::move(done)); });
            }

            std::vector<int> multiValuesNow(GuardedSession& guarded, const std::string& what, void (*launch)(Session&, MultiValueListCompletion))
            {
                return readMultiValue<MultiValueListResult>(what, &MultiValueListResult::values,
                                                            [&guarded, launch](MultiValueListCompletion done) { launch(guarded.session(), std::move(done)); });
            }

            // RQ-AKM-087 to RQ-AKM-093: creates a test program and a test multi under reserved names, round-trips every §0C
            // item on them, then (the guards) deletes both and selects again the multi that was current, and verifies,
            // once the guards are gone, that the sampler holds the multis it held and has the same one current. Never
            // sends &07 or &01; every deletion is of the test multi, selected again by index and named first.
            void multiLifecycleOnTestMulti()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());

                std::string problem;
                const auto before = readMultis(_rig, guarded.session(), problem);
                if (!before)
                    throw CheckFailure(problem);
                finding("multis before: " + multisText(*before));
                {
                    GuardedTestProgram program(_rig, guarded.session());
                    finding("test program \"" + std::string(TEST_PROGRAM_NAME) + "\" created, to be assigned to parts of the test multi");
                    GuardedTestMulti multi(_rig, guarded.session());
                    finding("test multi \"" + std::string(TEST_MULTI_NAME) + "\" created and current, at index " + std::to_string(multi.testIndex()));

                    const int partCount = multiCreationChecks(guarded, multi);
                    multiPartParameterChecks(guarded);
                    multiMuteSoloChecks(guarded, partCount);
                    multiProgramNumberChecks(guarded);
                    multiPartAssignmentChecks(guarded, partCount);
                    multiRenameAndSelectionChecks(guarded, multi);
                }
                expectMultisRestored(guarded, *before);
                closeAndVerify(guarded);
            }

            // RQ-AKM-093: a check that fails with the test program and the test multi current still deletes both and
            // selects again the multi that was current, and leaves nothing changed.
            void failedMultiCheckLeavesTheKnownState()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());

                std::string problem;
                const auto before = readMultis(_rig, guarded.session(), problem);
                if (!before)
                    throw CheckFailure(problem);
                const auto programNamesBefore = awaitCompletion<AllProgramNamesResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](AllProgramNamesCompletion done) { getAllProgramNames(guarded.session(), std::move(done)); });
                if (!programNamesBefore || !programNamesBefore->result.names)
                    throw CheckFailure("could not read the names of all programs before the test program is created");

                bool cleanedUp = false;
                try
                {
                    GuardedTestProgram program(_rig, guarded.session());
                    GuardedTestMulti multi(_rig, guarded.session());
                    finding("test program and test multi created for a check that fails on purpose");
                    throw CheckFailure("this check fails on purpose, with the test multi current");
                }
                catch (const CheckFailure& failure)
                {
                    // The guards above have already been destroyed, their cleanup already run, by the time the exception
                    // reaches this catch clause: that is what stack unwinding does.
                    cleanedUp = true;
                    _rig.log.note(std::string("  the check failed: ") + failure.what());
                }
                expect(cleanedUp, "the guards' destructors ran when the check failed");
                expectMultisRestored(guarded, *before);
                const auto programNamesAfter = awaitCompletion<AllProgramNamesResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](AllProgramNamesCompletion done) { getAllProgramNames(guarded.session(), std::move(done)); });
                expect(programNamesAfter && programNamesAfter->result.names == programNamesBefore->result.names,
                       "the sampler holds exactly the programs it held before");
                closeAndVerify(guarded);
            }

            // RQ-AKM-087, RQ-AKM-091: what the creation did, read back through the Gets: the number of parts (an observation of
            // the stored setting for new multis, which no item reads), the count, the index, the name, and the three
            // all-multis Gets, each ending with the test multi. Returns the number of parts.
            int multiCreationChecks(GuardedSession& guarded, const GuardedTestMulti& multi)
            {
                const std::size_t countBefore = multi.original().names.size();
                const int partCount = readMultiValue<MultiPartCountResult>("the number of parts (&44)", &MultiPartCountResult::partCount,
                                                                           [&guarded](MultiPartCountCompletion done) { getCurrentMultiPartCount(guarded.session(), std::move(done)); });
                finding("the test multi has " + std::to_string(partCount) + " parts (the sampler's own setting for new multis)");
                expect(partCount == 32 || partCount == 64 || partCount == 128, "the number of parts is 32, 64 or 128");

                const int count = readMultiValue<MultiCountResult>("the number of multis (&40)", &MultiCountResult::count,
                                                                   [&guarded](MultiCountCompletion done) { getMultiCount(guarded.session(), std::move(done)); });
                expect(static_cast<std::size_t>(count) == countBefore + 1, "the number of multis is one more (" + std::to_string(count) + ")");
                expect(multiIndexNow(guarded) == multi.testIndex(), "the current multi's index is " + std::to_string(multi.testIndex()));
                expect(multiNameNow(guarded) == TEST_MULTI_NAME, "the current multi's name is the reserved one");

                const auto names = readMultiValue<MultiNameListResult>("the names of all multis (&51)", &MultiNameListResult::names,
                                                                       [&guarded](MultiNameListCompletion done) { getAllMultiNames(guarded.session(), std::move(done)); });
                expect(names.size() == countBefore + 1 && names.back() == TEST_MULTI_NAME, "the names of all multis end with the test multi");
                const auto counts = multiValuesNow(guarded, "the number of parts of all multis (&52)", getAllMultiPartCounts);
                expect(counts.size() == countBefore + 1 && counts.back() == partCount, "the numbers of parts of all multis end with the test multi's");
                const auto numbers = readMultiValue<MultiProgramNumbersResult>("the program numbers of all multis (&50)", &MultiProgramNumbersResult::numbers,
                                                                               [&guarded](MultiProgramNumbersCompletion done) { getAllMultiProgramNumbers(guarded.session(), std::move(done)); });
                expect(numbers.size() == countBefore + 1, "the program numbers of all multis are one per multi");
                return partCount;
            }

            // RQ-AKM-089, RQ-AKM-090: every one of the twelve part parameter items is set on part 3 then read back, and
            // `&47` returns the twelve values in the order of the Gets.
            void multiPartParameterChecks(GuardedSession& guarded)
            {
                std::vector<int> expected;
                for (const MultiPartParameterCase& parameterCase : allMultiPartParameterCases())
                {
                    const std::string title(descriptor(parameterCase.setId).name);
                    expectCommand(guarded, "set " + title, [&parameterCase](Session& session, CommandCompletion done) {
                        session.submit(makeRequest(parameterCase.setId, parameterCase.values), std::move(done));
                    });
                    const auto timed = awaitCompletion<CommandResult>(
                        _rig.driver, _rig.commandPatience(), [&guarded, &parameterCase](CommandCompletion done) {
                            guarded.session().submit(makeRequest(parameterCase.getId, {parameterCase.values.front()}), std::move(done));
                        });
                    if (!timed || !succeeded(timed->result))
                        throw CheckFailure("get " + title + ": " + timedOutcomeText(timed));
                    const auto* replyData = std::get_if<Reply>(&timed->result);
                    const auto decoded = replyData ? decodeReply(parameterCase.getId, replyData->data) : std::nullopt;
                    const std::vector<std::int64_t> expectedValue(parameterCase.values.begin() + 1, parameterCase.values.end());
                    if (!decoded || *decoded != expectedValue)
                        throw CheckFailure("get " + title + ": read back " + (decoded ? valuesText(*decoded) : std::string("nothing decodable"))
                                           + ", expected " + valuesText(expectedValue));
                    expected.push_back(static_cast<int>(parameterCase.values[1]));
                }
                finding(std::to_string(allMultiPartParameterCases().size()) + " part parameter items round-tripped on part " + std::to_string(TEST_MULTI_PART));
                const auto all = readMultiValue<MultiValueListResult>(
                    "all the parameters of part " + std::to_string(TEST_MULTI_PART) + " (&47)", &MultiValueListResult::values,
                    [&guarded](MultiValueListCompletion done) { getAllMultiPartParameters(guarded.session(), TEST_MULTI_PART, std::move(done)); });
                // Observed on the real S5000 (OS 2.14): setting a part's solo clears its mute, so the cases' own order (mute, then
                // solo) leaves the mute off. `&47` is therefore compared with the Sets for every value but the mute, whose
                // reading is a finding.
                constexpr std::size_t MUTE_POSITION = 1;
                bool sameButTheMute = all.size() == expected.size();
                for (std::size_t position = 0; sameButTheMute && position < expected.size(); ++position)
                    if (position != MUTE_POSITION && all[position] != expected[position])
                        sameButTheMute = false;
                expect(sameButTheMute, "&47 returns the twelve values the twelve Sets wrote, in the order of the Gets, but for the mute");
                finding("&47 reads the mute of part " + std::to_string(TEST_MULTI_PART) + " as " + std::to_string(all[MUTE_POSITION])
                        + (all[MUTE_POSITION] != expected[MUTE_POSITION] ? " (set to " + std::to_string(expected[MUTE_POSITION]) + " before the solo was set: setting the solo clears it)"
                                                                         : " (as set)"));
            }

            // RQ-AKM-090: the mute and solo status of every part. With mute and solo both on (what the parameter cases left
            // on part 3) the sampler's answer is an observation; then each alone is expected as the spec gives it.
            void multiMuteSoloChecks(GuardedSession& guarded, int partCount)
            {
                const auto both = multiValuesNow(guarded, "the mute and solo status of all parts (&48)", getMultiMuteSoloStatus);
                expect(static_cast<int>(both.size()) == partCount, "&48 returns one value per part (" + std::to_string(both.size()) + ")");
                finding("part " + std::to_string(TEST_MULTI_PART) + " with the mute set, then the solo, reads " + std::to_string(both[TEST_MULTI_PART])
                        + " (0 none, 1 mute, 2 solo)");

                expectCommand(guarded, "clear the mute of part 3", [](Session& session, CommandCompletion done) {
                    session.submit(makeRequest(ItemId::MultiSetMute, {TEST_MULTI_PART, 0}), std::move(done));
                });
                const auto soloOnly = multiValuesNow(guarded, "the mute and solo status of all parts (&48)", getMultiMuteSoloStatus);
                expect(soloOnly[TEST_MULTI_PART] == 2, "with solo alone on, part 3 reads 2 (solo)");
                expectCommand(guarded, "clear the solo of part 3", [](Session& session, CommandCompletion done) {
                    session.submit(makeRequest(ItemId::MultiSetSolo, {TEST_MULTI_PART, 0}), std::move(done));
                });
                expectCommand(guarded, "set the mute of part 3", [](Session& session, CommandCompletion done) {
                    session.submit(makeRequest(ItemId::MultiSetMute, {TEST_MULTI_PART, 1}), std::move(done));
                });
                const auto muteOnly = multiValuesNow(guarded, "the mute and solo status of all parts (&48)", getMultiMuteSoloStatus);
                expect(muteOnly[TEST_MULTI_PART] == 1, "with mute alone on, part 3 reads 1 (mute)");
                expectCommand(guarded, "clear the mute of part 3", [](Session& session, CommandCompletion done) {
                    session.submit(makeRequest(ItemId::MultiSetMute, {TEST_MULTI_PART, 0}), std::move(done));
                });
                const auto none = multiValuesNow(guarded, "the mute and solo status of all parts (&48)", getMultiMuteSoloStatus);
                expect(std::all_of(none.begin(), none.end(), [](int value) { return value == 0; }), "with both cleared, every part reads 0");
            }

            // RQ-AKM-092, RQ-AKM-091: the program number set, read and cleared.
            void multiProgramNumberChecks(GuardedSession& guarded)
            {
                expectCommand(guarded, "set the multi's program number to " + std::to_string(PROGRAM_NUMBER_FOR_THE_TEST),
                              [](Session& session, CommandCompletion done) { setMultiProgramNumber(session, PROGRAM_NUMBER_FOR_THE_TEST, std::move(done)); });
                const auto number = awaitCompletion<MultiProgramNumberResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](MultiProgramNumberCompletion done) { getMultiProgramNumber(guarded.session(), std::move(done)); });
                expect(number && number->result.frontPanelNumber == PROGRAM_NUMBER_FOR_THE_TEST,
                       "the program number reads " + std::to_string(PROGRAM_NUMBER_FOR_THE_TEST));
                expectCommand(guarded, "clear the multi's program number", [](Session& session, CommandCompletion done) {
                    setMultiProgramNumber(session, std::nullopt, std::move(done));
                });
                const auto off = awaitCompletion<MultiProgramNumberResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](MultiProgramNumberCompletion done) { getMultiProgramNumber(guarded.session(), std::move(done)); });
                expect(off && std::holds_alternative<Reply>(off->result.outcome) && !off->result.frontPanelNumber.has_value(),
                       "the program number reads off");
            }

            // RQ-AKM-092, RQ-AKM-091: the test program assigned to a part by name and to another by index, read back by
            // `&45` and `&46`, then deleted from both; what the sampler answers for an index and a name that name nothing is
            // an observation.
            void multiPartAssignmentChecks(GuardedSession& guarded, int partCount)
            {
                expectCommand(guarded, "assign the test program to part " + std::to_string(TEST_MULTI_PART) + " by name",
                              [](Session& session, CommandCompletion done) { setMultiPartByName(session, TEST_MULTI_PART, TEST_PROGRAM_NAME, std::move(done)); });
                expect(multiPartNameNow(guarded, TEST_MULTI_PART) == TEST_PROGRAM_NAME, "&45 reads the test program's name on that part");
                const auto names = readMultiValue<MultiNameListResult>("the names of all parts (&46)", &MultiNameListResult::names,
                                                                       [&guarded](MultiNameListCompletion done) { getAllMultiPartNames(guarded.session(), std::move(done)); });
                expect(static_cast<int>(names.size()) == partCount && names[TEST_MULTI_PART] == TEST_PROGRAM_NAME,
                       "&46 returns one name per part, the test program's on part " + std::to_string(TEST_MULTI_PART));
                expectCommand(guarded, "delete the program of part " + std::to_string(TEST_MULTI_PART), [](Session& session, CommandCompletion done) {
                    deleteMultiPart(session, TEST_MULTI_PART, std::move(done));
                });
                expect(multiPartNameNow(guarded, TEST_MULTI_PART).empty(), "&45 reads an empty name once the part is deleted");

                const int programIndex = readMultiValue<ProgramIndexResult>("the test program's index (§0A/&12)", &ProgramIndexResult::index,
                                                                           [&guarded](ProgramIndexCompletion done) { getProgramIndex(guarded.session(), std::move(done)); });
                expectCommand(guarded, "assign the program at index " + std::to_string(programIndex) + " to part " + std::to_string(TEST_MULTI_PART_BY_INDEX),
                              [programIndex](Session& session, CommandCompletion done) { setMultiPartByIndex(session, TEST_MULTI_PART_BY_INDEX, programIndex, std::move(done)); });
                expect(multiPartNameNow(guarded, TEST_MULTI_PART_BY_INDEX) == TEST_PROGRAM_NAME, "&45 reads the test program's name on that part");
                expectCommand(guarded, "delete the program of part " + std::to_string(TEST_MULTI_PART_BY_INDEX), [](Session& session, CommandCompletion done) {
                    deleteMultiPart(session, TEST_MULTI_PART_BY_INDEX, std::move(done));
                });

                observeIndexPastTheEnd("assign a program no memory holds to part 0 by name", [&guarded](CommandCompletion done) {
                    setMultiPartByName(guarded.session(), 0, "XS56K NO SUCH PROGRAM", std::move(done));
                });
            }

            // RQ-AKM-087, RQ-AKM-092: the test multi renamed and renamed back, the selection by index and by name (a multi of
            // the owner's is selected, never changed), and what the sampler answers to a name and an index that name nothing.
            void multiRenameAndSelectionChecks(GuardedSession& guarded, const GuardedTestMulti& multi)
            {
                expectCommand(guarded, "rename the test multi to \"" + std::string(TEST_MULTI_RENAMED) + "\"", [](Session& session, CommandCompletion done) {
                    renameCurrentMulti(session, TEST_MULTI_RENAMED, std::move(done));
                });
                expect(multiNameNow(guarded) == TEST_MULTI_RENAMED, "the current multi's name reads the new name");
                expectCommand(guarded, "rename the test multi back", [](Session& session, CommandCompletion done) {
                    renameCurrentMulti(session, TEST_MULTI_NAME, std::move(done));
                });
                expect(multiNameNow(guarded) == TEST_MULTI_NAME, "the current multi's name reads the reserved name again");

                if (!multi.original().names.empty())
                {
                    expectCommand(guarded, "select multi 0 by index (the owner's, not changed)", [](Session& session, CommandCompletion done) {
                        selectMultiByIndex(session, 0, std::move(done));
                    });
                    expect(multiIndexNow(guarded) == 0, "the current multi's index reads 0");
                    expect(multiNameNow(guarded) == multi.original().names.front(), "its name is the owner's first multi's");
                }
                expectCommand(guarded, "select the test multi by name", [](Session& session, CommandCompletion done) {
                    selectMultiByName(session, TEST_MULTI_NAME, std::move(done));
                });
                expect(multiIndexNow(guarded) == multi.testIndex(), "the current multi's index is the test multi's");

                const int pastTheEnd = multi.testIndex() + 1;
                observeIndexPastTheEnd("select multi " + std::to_string(pastTheEnd) + ", past the last", [&guarded, pastTheEnd](CommandCompletion done) {
                    selectMultiByIndex(guarded.session(), pastTheEnd, std::move(done));
                });
                observeIndexPastTheEnd("select a multi no memory holds by name", [&guarded](CommandCompletion done) {
                    selectMultiByName(guarded.session(), "XS56K NO SUCH MULTI", std::move(done));
                });
                expect(multiNameNow(guarded) == TEST_MULTI_NAME, "the test multi is still the current one");
            }

            void expectMultisRestored(GuardedSession& guarded, const MultisSnapshot& before)
            {
                std::string problem;
                const auto after = readMultis(_rig, guarded.session(), problem);
                if (!after)
                    throw CheckFailure("could not read the multis back to verify they were restored: " + problem);
                expect(after->names == before.names, "the sampler holds exactly the multis it held before (" + multisText(*after) + ")");
                if (before.currentIndex)
                    expect(after->currentIndex == before.currentIndex, "the current multi is back to " + std::to_string(*before.currentIndex));
                else
                    finding("no multi was current before the check, so the selection is left on the one the check chose last");
            }

            // RQ-AKM-082, RQ-AKM-083, RQ-AKM-084, RQ-AKM-085: reads the song files and the set lists, selects each song file
            // by index and by name, renames the first song file and the first set list and reads the new names back, each
            // under the guard that puts them back (and selects again the song file that was current), and verifies, once the
            // guard is gone, that every name and the selection are what they were. Nothing is deleted. Skipped when the
            // sampler holds neither a song file nor a set list: §16 cannot create one.
            void songFilesRoundTrips()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());

                std::string problem;
                const auto before = readSongFiles(_rig, guarded.session(), problem);
                if (!before)
                    throw CheckFailure(problem);
                finding("song files before: " + songFilesText(*before));
                if (before->songCount == 0 && before->setListCount == 0)
                {
                    observeEmptySongFiles(guarded);
                    static_cast<void>(guarded.close());
                    throw CheckSkipped("the sampler holds no song file and no set list: §16 cannot create one, so there is nothing to select or rename");
                }
                {
                    GuardedSongFiles guard(_rig, guarded.session(), *before);
                    if (before->songCount > 0)
                        songFileRoundTrips(guarded, *before, guard);
                    if (before->setListCount > 0)
                        setListRoundTrips(guarded, *before, guard);
                }
                expectSongFilesRestored(guarded, *before);
                closeAndVerify(guarded);
            }

            // RQ-AKM-085: a check that fails with a song file renamed still puts its name back, and the selection, and
            // leaves nothing changed. Skipped when the sampler holds no song file.
            void failedSongFilesCheckPutsBack()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());

                std::string problem;
                const auto before = readSongFiles(_rig, guarded.session(), problem);
                if (!before)
                    throw CheckFailure(problem);
                if (before->songCount == 0)
                {
                    static_cast<void>(guarded.close());
                    throw CheckSkipped("the sampler holds no song file to rename");
                }

                bool cleanedUp = false;
                try
                {
                    GuardedSongFiles guard(_rig, guarded.session(), *before);
                    expectCommand(guarded, "select song file 0", [](Session& session, CommandCompletion done) {
                        selectSongByIndex(session, 0, std::move(done));
                    });
                    guard.noteSongRenamed(0);
                    expectCommand(guarded, "rename the current song file", [](Session& session, CommandCompletion done) {
                        renameCurrentSong(session, TEST_SONG_FILES_NAME, std::move(done));
                    });
                    throw CheckFailure("this check fails on purpose, with a song file renamed");
                }
                catch (const CheckFailure& failure)
                {
                    // The guard above has already been destroyed, its restoration already run, by the time the
                    // exception reaches this catch clause: that is what stack unwinding does.
                    cleanedUp = true;
                    _rig.log.note(std::string("  the check failed: ") + failure.what());
                }
                expect(cleanedUp, "the guard's destructor ran when the check failed");
                expectSongFilesRestored(guarded, *before);
                closeAndVerify(guarded);
            }

            [[nodiscard]] int firstIndexNamed(const std::vector<std::string>& names, const std::string& name) const
            {
                return static_cast<int>(std::find(names.begin(), names.end(), name) - names.begin());
            }

            int currentSongIndexOf(GuardedSession& guarded)
            {
                const auto timed = awaitCompletion<SongIndexResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](SongIndexCompletion done) { getCurrentSongIndex(guarded.session(), std::move(done)); });
                if (!timed || !timed->result.index)
                    throw CheckFailure("could not read the current song file's index (&13): "
                                       + (timed ? outcomeText(timed->result.outcome) : std::string("no completion")));
                return *timed->result.index;
            }

            std::string currentSongNameOf(GuardedSession& guarded)
            {
                const auto timed = awaitCompletion<SongNameResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](SongNameCompletion done) { getCurrentSongName(guarded.session(), std::move(done)); });
                if (!timed || !timed->result.name)
                    throw CheckFailure("could not read the current song file's name (&14): "
                                       + (timed ? outcomeText(timed->result.outcome) : std::string("no completion")));
                return *timed->result.name;
            }

            std::string songNameAt(GuardedSession& guarded, int index)
            {
                const auto timed = awaitCompletion<SongNameResult>(_rig.driver, _rig.commandPatience(), [&guarded, index](SongNameCompletion done) {
                    getSongNameByIndex(guarded.session(), index, std::move(done));
                });
                if (!timed || !timed->result.name)
                    throw CheckFailure("could not read the name of song file " + std::to_string(index) + " (&11): "
                                       + (timed ? outcomeText(timed->result.outcome) : std::string("no completion")));
                return *timed->result.name;
            }

            std::string setListNameAt(GuardedSession& guarded, int index)
            {
                const auto timed = awaitCompletion<SetListNameResult>(_rig.driver, _rig.commandPatience(), [&guarded, index](SetListNameCompletion done) {
                    getSetListNameByIndex(guarded.session(), index, std::move(done));
                });
                if (!timed || !timed->result.name)
                    throw CheckFailure("could not read the name of set list " + std::to_string(index) + " (&21): "
                                       + (timed ? outcomeText(timed->result.outcome) : std::string("no completion")));
                return *timed->result.name;
            }

            // What the sampler answers for an index one past the last: an observation (the spec says nothing of it), never a failure.
            template <typename Launch>
            void observeIndexPastTheEnd(const std::string& title, Launch launch)
            {
                const auto timed = awaitCompletion<CommandResult>(_rig.driver, _rig.commandPatience(), launch);
                finding(title + ": " + (timed ? outcomeText(timed->result) : std::string("no completion")));
            }

            // What an empty sampler answers to the read and select items that name something: observations for the open
            // points of FTR-AKM-010, none of them a failure. Nothing is renamed or deleted, and a selection that finds nothing
            // changes nothing. [RQ-AKM-085]
            void observeEmptySongFiles(GuardedSession& guarded)
            {
                observeIndexPastTheEnd("read the name of song file 0, none held", [&guarded](CommandCompletion done) {
                    getSongNameByIndex(guarded.session(), 0, [done = std::move(done)](const SongNameResult& result) { done(result.outcome); });
                });
                observeIndexPastTheEnd("read the name of set list 0, none held", [&guarded](CommandCompletion done) {
                    getSetListNameByIndex(guarded.session(), 0, [done = std::move(done)](const SetListNameResult& result) { done(result.outcome); });
                });
                observeIndexPastTheEnd("select song file 0, none held", [&guarded](CommandCompletion done) {
                    selectSongByIndex(guarded.session(), 0, std::move(done));
                });
                observeIndexPastTheEnd("select the song file named \"" + std::string(TEST_SONG_FILES_NAME) + "\", none held",
                                       [&guarded](CommandCompletion done) { selectSongByName(guarded.session(), TEST_SONG_FILES_NAME, std::move(done)); });
            }

            void songFileRoundTrips(GuardedSession& guarded, const SongFilesSnapshot& before, GuardedSongFiles& guard)
            {
                const std::string first = before.songNames.front();
                expectCommand(guarded, "select song file 0 by index", [](Session& session, CommandCompletion done) {
                    selectSongByIndex(session, 0, std::move(done));
                });
                expect(currentSongIndexOf(guarded) == 0, "the current song file's index reads 0 after selecting index 0");
                expect(currentSongNameOf(guarded) == first, "the current song file's name reads \"" + first + "\", the name &11 gave for index 0");

                expectCommand(guarded, "select the song file named \"" + first + "\"", [first](Session& session, CommandCompletion done) {
                    selectSongByName(session, first, std::move(done));
                });
                const int expectedIndex = firstIndexNamed(before.songNames, first);
                expect(currentSongIndexOf(guarded) == expectedIndex,
                       "the current song file's index reads " + std::to_string(expectedIndex) + " after selecting it by name");

                if (before.songCount > 1)
                {
                    const int last = std::min(before.songCount, SONG_FILES_NAME_READ_LIMIT) - 1;
                    expectCommand(guarded, "select song file " + std::to_string(last) + " by index", [last](Session& session, CommandCompletion done) {
                        selectSongByIndex(session, last, std::move(done));
                    });
                    expect(currentSongIndexOf(guarded) == last, "the current song file's index reads " + std::to_string(last));
                    expect(currentSongNameOf(guarded) == before.songNames[static_cast<std::size_t>(last)],
                           "its name reads \"" + before.songNames[static_cast<std::size_t>(last)] + "\"");
                }

                const int pastTheEnd = before.songCount;
                observeIndexPastTheEnd("select song file " + std::to_string(pastTheEnd) + ", past the last",
                                       [&guarded, pastTheEnd](CommandCompletion done) { selectSongByIndex(guarded.session(), pastTheEnd, std::move(done)); });

                expectCommand(guarded, "select song file 0 again", [](Session& session, CommandCompletion done) {
                    selectSongByIndex(session, 0, std::move(done));
                });
                guard.noteSongRenamed(0);
                expectCommand(guarded, "rename the current song file to \"" + std::string(TEST_SONG_FILES_NAME) + "\"",
                              [](Session& session, CommandCompletion done) { renameCurrentSong(session, TEST_SONG_FILES_NAME, std::move(done)); });
                expect(currentSongNameOf(guarded) == TEST_SONG_FILES_NAME, "the current song file's name reads the new name");
                expect(songNameAt(guarded, 0) == TEST_SONG_FILES_NAME, "the name of song file 0 reads the new name");
            }

            void setListRoundTrips(GuardedSession& guarded, const SongFilesSnapshot& before, GuardedSongFiles& guard)
            {
                expect(setListNameAt(guarded, 0) == before.setListNames.front(),
                       "the name of set list 0 reads \"" + before.setListNames.front() + "\" again");
                const int pastTheEnd = before.setListCount;
                observeIndexPastTheEnd("read the name of set list " + std::to_string(pastTheEnd) + ", past the last",
                                       [&guarded, pastTheEnd](CommandCompletion done) {
                                           getSetListNameByIndex(guarded.session(), pastTheEnd, [done = std::move(done)](const SetListNameResult& result) { done(result.outcome); });
                                       });
                guard.noteSetListRenamed(0);
                expectCommand(guarded, "rename set list 0 to \"" + std::string(TEST_SONG_FILES_NAME) + "\"",
                              [](Session& session, CommandCompletion done) { renameSetList(session, 0, TEST_SONG_FILES_NAME, std::move(done)); });
                expect(setListNameAt(guarded, 0) == TEST_SONG_FILES_NAME, "the name of set list 0 reads the new name");
            }

            void expectSongFilesRestored(GuardedSession& guarded, const SongFilesSnapshot& before)
            {
                std::string problem;
                const auto after = readSongFiles(_rig, guarded.session(), problem);
                if (!after)
                    throw CheckFailure("could not read the song files back to verify they were restored: " + problem);
                expect(after->songCount == before.songCount && after->songNames == before.songNames,
                       "the song files are back to what they were (" + songFilesText(*after) + ")");
                expect(after->setListCount == before.setListCount && after->setListNames == before.setListNames,
                       "the set lists are back to what they were");
                if (before.currentSong)
                    expect(after->currentSong == before.currentSong,
                           "the current song file is back to " + std::to_string(*before.currentSong));
                else
                    finding("no song file was current before the check, so the selection is left on the one the check chose");
            }

            // RQ-AKM-095, RQ-AKM-096, RQ-AKM-097: reads the scenelists, selects each by index and by name, renames the first
            // and reads the new name back, under the guard that puts the name back (and selects again the scenelist that was
            // current), and verifies, once the guard is gone, that every name and the selection are what they were. Nothing
            // is deleted. Skipped when the sampler holds no scenelist: §14 cannot create one.
            void sceneListsRoundTrips()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());

                std::string problem;
                const auto before = readSceneLists(_rig, guarded.session(), problem);
                if (!before)
                    throw CheckFailure(problem);
                finding("scenelists before: " + sceneListsText(*before));
                if (before->count == 0)
                {
                    observeEmptySceneLists(guarded);
                    static_cast<void>(guarded.close());
                    throw CheckSkipped("the sampler holds no scenelist: §14 cannot create one, so there is nothing to select or rename");
                }
                {
                    GuardedSceneLists guard(_rig, guarded.session(), *before);
                    sceneListRoundTrips(guarded, *before, guard);
                }
                expectSceneListsRestored(guarded, *before);
                closeAndVerify(guarded);
            }

            // RQ-AKM-097: a check that fails with a scenelist renamed still puts its name back, and the selection, and
            // leaves nothing changed. Skipped when the sampler holds no scenelist.
            void failedSceneListsCheckPutsBack()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());

                std::string problem;
                const auto before = readSceneLists(_rig, guarded.session(), problem);
                if (!before)
                    throw CheckFailure(problem);
                if (before->count == 0)
                {
                    static_cast<void>(guarded.close());
                    throw CheckSkipped("the sampler holds no scenelist to rename");
                }

                bool cleanedUp = false;
                try
                {
                    GuardedSceneLists guard(_rig, guarded.session(), *before);
                    expectCommand(guarded, "select scenelist 0", [](Session& session, CommandCompletion done) {
                        selectSceneListByIndex(session, 0, std::move(done));
                    });
                    guard.noteRenamed(0);
                    expectCommand(guarded, "rename the current scenelist", [](Session& session, CommandCompletion done) {
                        renameCurrentSceneList(session, TEST_SCENE_LIST_NAME, std::move(done));
                    });
                    throw CheckFailure("this check fails on purpose, with a scenelist renamed");
                }
                catch (const CheckFailure& failure)
                {
                    // The guard above has already been destroyed, its restoration already run, by the time the
                    // exception reaches this catch clause: that is what stack unwinding does.
                    cleanedUp = true;
                    _rig.log.note(std::string("  the check failed: ") + failure.what());
                }
                expect(cleanedUp, "the guard's destructor ran when the check failed");
                expectSceneListsRestored(guarded, *before);
                closeAndVerify(guarded);
            }

            int currentSceneListIndexOf(GuardedSession& guarded)
            {
                const auto timed = awaitCompletion<SceneListIndexResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](SceneListIndexCompletion done) { getCurrentSceneListIndex(guarded.session(), std::move(done)); });
                if (!timed || !timed->result.index)
                    throw CheckFailure("could not read the current scenelist's index (&13): "
                                       + (timed ? outcomeText(timed->result.outcome) : std::string("no completion")));
                return *timed->result.index;
            }

            std::string currentSceneListNameOf(GuardedSession& guarded)
            {
                const auto timed = awaitCompletion<SceneListNameResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](SceneListNameCompletion done) { getCurrentSceneListName(guarded.session(), std::move(done)); });
                if (!timed || !timed->result.name)
                    throw CheckFailure("could not read the current scenelist's name (&14): "
                                       + (timed ? outcomeText(timed->result.outcome) : std::string("no completion")));
                return *timed->result.name;
            }

            std::string sceneListNameAt(GuardedSession& guarded, int index)
            {
                const auto timed = awaitCompletion<SceneListNameResult>(_rig.driver, _rig.commandPatience(), [&guarded, index](SceneListNameCompletion done) {
                    getSceneListNameByIndex(guarded.session(), index, std::move(done));
                });
                if (!timed || !timed->result.name)
                    throw CheckFailure("could not read the name of scenelist " + std::to_string(index) + " (&11): "
                                       + (timed ? outcomeText(timed->result.outcome) : std::string("no completion")));
                return *timed->result.name;
            }

            // What an empty sampler answers to the read and select items that name something: observations for the open
            // points of FTR-AKM-012, none of them a failure. Nothing is renamed or deleted, and a selection that finds nothing
            // changes nothing. [RQ-AKM-097]
            void observeEmptySceneLists(GuardedSession& guarded)
            {
                observeIndexPastTheEnd("read the name of scenelist 0, none held", [&guarded](CommandCompletion done) {
                    getSceneListNameByIndex(guarded.session(), 0, [done = std::move(done)](const SceneListNameResult& result) { done(result.outcome); });
                });
                observeIndexPastTheEnd("select scenelist 0, none held", [&guarded](CommandCompletion done) {
                    selectSceneListByIndex(guarded.session(), 0, std::move(done));
                });
                observeIndexPastTheEnd("select the scenelist named \"" + std::string(TEST_SCENE_LIST_NAME) + "\", none held",
                                       [&guarded](CommandCompletion done) { selectSceneListByName(guarded.session(), TEST_SCENE_LIST_NAME, std::move(done)); });
            }

            void sceneListRoundTrips(GuardedSession& guarded, const SceneListsSnapshot& before, GuardedSceneLists& guard)
            {
                const std::string first = before.names.front();
                expectCommand(guarded, "select scenelist 0 by index", [](Session& session, CommandCompletion done) {
                    selectSceneListByIndex(session, 0, std::move(done));
                });
                expect(currentSceneListIndexOf(guarded) == 0, "the current scenelist's index reads 0 after selecting index 0");
                expect(currentSceneListNameOf(guarded) == first, "the current scenelist's name reads \"" + first + "\", the name &11 gave for index 0");

                expectCommand(guarded, "select the scenelist named \"" + first + "\"", [first](Session& session, CommandCompletion done) {
                    selectSceneListByName(session, first, std::move(done));
                });
                const int expectedIndex = firstIndexNamed(before.names, first);
                expect(currentSceneListIndexOf(guarded) == expectedIndex,
                       "the current scenelist's index reads " + std::to_string(expectedIndex) + " after selecting it by name");

                if (before.count > 1)
                {
                    const int last = std::min(before.count, SCENE_LIST_NAME_READ_LIMIT) - 1;
                    expectCommand(guarded, "select scenelist " + std::to_string(last) + " by index", [last](Session& session, CommandCompletion done) {
                        selectSceneListByIndex(session, last, std::move(done));
                    });
                    expect(currentSceneListIndexOf(guarded) == last, "the current scenelist's index reads " + std::to_string(last));
                    expect(currentSceneListNameOf(guarded) == before.names[static_cast<std::size_t>(last)],
                           "its name reads \"" + before.names[static_cast<std::size_t>(last)] + "\"");
                }

                const int pastTheEnd = before.count;
                observeIndexPastTheEnd("select scenelist " + std::to_string(pastTheEnd) + ", past the last",
                                       [&guarded, pastTheEnd](CommandCompletion done) { selectSceneListByIndex(guarded.session(), pastTheEnd, std::move(done)); });

                expectCommand(guarded, "select scenelist 0 again", [](Session& session, CommandCompletion done) {
                    selectSceneListByIndex(session, 0, std::move(done));
                });
                guard.noteRenamed(0);
                expectCommand(guarded, "rename the current scenelist to \"" + std::string(TEST_SCENE_LIST_NAME) + "\"",
                              [](Session& session, CommandCompletion done) { renameCurrentSceneList(session, TEST_SCENE_LIST_NAME, std::move(done)); });
                expect(currentSceneListNameOf(guarded) == TEST_SCENE_LIST_NAME, "the current scenelist's name reads the new name");
                expect(sceneListNameAt(guarded, 0) == TEST_SCENE_LIST_NAME, "the name of scenelist 0 reads the new name");
            }

            void expectSceneListsRestored(GuardedSession& guarded, const SceneListsSnapshot& before)
            {
                std::string problem;
                const auto after = readSceneLists(_rig, guarded.session(), problem);
                if (!after)
                    throw CheckFailure("could not read the scenelists back to verify they were restored: " + problem);
                expect(after->count == before.count && after->names == before.names,
                       "the scenelists are back to what they were (" + sceneListsText(*after) + ")");
                if (before.current)
                    expect(after->current == before.current,
                           "the current scenelist is back to " + std::to_string(*before.current));
                else
                    finding("no scenelist was current before the check, so the selection is left on the one the check chose");
            }

            // RQ-AKM-052, RQ-AKM-053, RQ-AKM-054, RQ-AKM-055, RQ-AKM-057, RQ-AKM-058: reads the model and the memory
            // (so that the four data bytes of &33/&34 are decoded on the hardware), then round-trips the name, every Play
            // Mode, the front-panel lock and the clock, each under the guard that puts them back — the Play Mode 3 (Muted)
            // the spec's data column leaves out is sent too, its refusal being an observation, not a failure — and
            // verifies, once the guard is gone, that every value is what it was before.
            void systemSetupRoundTrips()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());

                SystemSetupSnapshot original;
                {
                    GuardedSystemSetup setup(_rig, guarded.session());
                    original = setup.original();
                    finding("system setup before: name \"" + original.name + "\", Play Mode " + playModeName(original.playMode)
                            + ", front panel " + lockName(original.lock) + ", clock "
                            + (original.clock ? clockText(*original.clock) : std::string("unreadable")));

                    observeModelAndMemory(guarded);
                    roundTripName(guarded);
                    roundTripPlayModes(guarded);
                    roundTripLock(guarded);
                    roundTripClock(guarded, original);
                }
                expectSystemSetupRestored(guarded, original);
                closeAndVerify(guarded);
            }

            // RQ-AKM-058: a check that fails with the front panel locked and the sampler renamed still puts both back
            // — the lock first — and leaves nothing changed.
            void failedSystemSetupCheckPutsBack()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());

                std::string problem;
                const auto original = readSystemSetup(_rig, guarded.session(), problem);
                if (!original)
                    throw CheckFailure(problem);

                bool cleanedUp = false;
                try
                {
                    GuardedSystemSetup setup(_rig, guarded.session());
                    expectCommand(guarded, "lock the front panel", [](Session& session, CommandCompletion done) {
                        setFrontPanelLock(session, FrontPanelLock::Locked, std::move(done));
                    });
                    expectCommand(guarded, "rename the sampler", [](Session& session, CommandCompletion done) {
                        setSamplerName(session, TEST_SAMPLER_NAME, std::move(done));
                    });
                    throw CheckFailure("this check fails on purpose, with the front panel locked");
                }
                catch (const CheckFailure& failure)
                {
                    // The guard above has already been destroyed, its restoration already run, by the time the
                    // exception reaches this catch clause: that is what stack unwinding does.
                    cleanedUp = true;
                    _rig.log.note(std::string("  the check failed: ") + failure.what());
                }
                expect(cleanedUp, "the guard's destructor ran when the check failed");
                expectSystemSetupRestored(guarded, *original);
                closeAndVerify(guarded);
            }

            // RQ-AKM-053: the model and the memory, read and said in the log. The byte counts are compound double
            // words of four data bytes (the spec writes their rows with two columns): a REPLY of another length is
            // refused by the decoder, so reading them is what shows the length on the hardware.
            void observeModelAndMemory(GuardedSession& guarded)
            {
                const auto model = awaitCompletion<SamplerModelResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](SamplerModelCompletion done) { getSamplerModel(guarded.session(), std::move(done)); });
                if (!model || !model->result.model)
                    throw CheckFailure("could not read the sampler's model (&04): "
                                       + (model ? outcomeText(model->result.outcome) : std::string("no completion")));
                finding(std::string("model: ") + (*model->result.model == SamplerModel::S6000 ? "S6000" : "S5000"));

                const auto wavePercent = awaitCompletion<MemoryPercentResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](MemoryPercentCompletion done) { getFreeWaveMemoryPercent(guarded.session(), std::move(done)); });
                const auto mpksPercent = awaitCompletion<MemoryPercentResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](MemoryPercentCompletion done) { getFreeMpksMemoryPercent(guarded.session(), std::move(done)); });
                const auto totalBytes = awaitCompletion<MemoryBytesResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](MemoryBytesCompletion done) { getTotalWaveMemoryBytes(guarded.session(), std::move(done)); });
                const auto freeBytes = awaitCompletion<MemoryBytesResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](MemoryBytesCompletion done) { getFreeWaveMemoryBytes(guarded.session(), std::move(done)); });
                if (!wavePercent || !wavePercent->result.percent)
                    throw CheckFailure("could not read the free Wave memory percentage (&30)");
                if (!mpksPercent || !mpksPercent->result.percent)
                    throw CheckFailure("could not read the free MPKS memory percentage (&31)");
                if (!totalBytes || !totalBytes->result.bytes)
                    throw CheckFailure("could not read the total bytes of Wave memory (&33): "
                                       + (totalBytes ? outcomeText(totalBytes->result.outcome) : std::string("no completion")));
                if (!freeBytes || !freeBytes->result.bytes)
                    throw CheckFailure("could not read the free bytes of Wave memory (&34): "
                                       + (freeBytes ? outcomeText(freeBytes->result.outcome) : std::string("no completion")));

                const std::uint32_t total = *totalBytes->result.bytes;
                const std::uint32_t free = *freeBytes->result.bytes;
                finding("Wave memory: " + std::to_string(free) + " of " + std::to_string(total) + " bytes free ("
                        + std::to_string(*wavePercent->result.percent) + " %, four data bytes decoded), MPKS memory "
                        + std::to_string(*mpksPercent->result.percent) + " % free");
                expect(total > 0 && free <= total, "the free Wave memory is a part of the total");
            }

            // RQ-AKM-052: Set then Get.
            void roundTripName(GuardedSession& guarded)
            {
                expectCommand(guarded, "set the sampler's name to \"" + std::string(TEST_SAMPLER_NAME) + "\"",
                              [](Session& session, CommandCompletion done) { setSamplerName(session, TEST_SAMPLER_NAME, std::move(done)); });
                const auto name = awaitCompletion<SamplerNameResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](SamplerNameCompletion done) { getSamplerName(guarded.session(), std::move(done)); });
                expect(name && name->result.name == std::string(TEST_SAMPLER_NAME), "the sampler's name read back is the one set");
            }

            // RQ-AKM-055, RQ-AKM-057: each Play Mode, Set then Get. Muted is the one the spec's data column leaves out:
            // if the sampler refuses it, that is what the erratum needed to know.
            void roundTripPlayModes(GuardedSession& guarded)
            {
                for (const PlayMode mode : {PlayMode::Multi, PlayMode::Program, PlayMode::Sample, PlayMode::Muted})
                {
                    const std::string title = "play mode " + std::to_string(static_cast<int>(mode)) + " (" + playModeName(mode) + ")";
                    _rig.log.flush();
                    const auto set = awaitCompletion<CommandResult>(_rig.driver, _rig.commandPatience(), [&guarded, mode](CommandCompletion done) {
                        setPlayMode(guarded.session(), mode, std::move(done));
                    });
                    if (!set)
                        throw CheckFailure("set " + title + ": no completion within " + millisecondsText(_rig.commandPatience()));
                    if (!succeeded(set->result))
                    {
                        if (mode == PlayMode::Muted && std::holds_alternative<Error>(set->result))
                        {
                            finding(title + " refused: " + outcomeText(set->result)
                                    + " - the spec's data column \"0, 1, 2\" is the sampler's range (erratum of RQ-AKM-057)");
                            continue;
                        }
                        throw CheckFailure("set " + title + ": " + outcomeText(set->result));
                    }
                    const auto read = awaitCompletion<PlayModeResult>(
                        _rig.driver, _rig.commandPatience(), [&guarded](PlayModeCompletion done) { getPlayMode(guarded.session(), std::move(done)); });
                    expect(read && read->result.mode == mode, title + " read back as set");
                    finding(title + " accepted and read back");
                }
            }

            // RQ-AKM-055, RQ-AKM-058: locked, read, then normal again at once, before anything else is sent.
            void roundTripLock(GuardedSession& guarded)
            {
                for (const FrontPanelLock lock : {FrontPanelLock::Locked, FrontPanelLock::Normal})
                {
                    expectCommand(guarded, "set the front panel " + lockName(lock),
                                  [lock](Session& session, CommandCompletion done) { setFrontPanelLock(session, lock, std::move(done)); });
                    const auto read = awaitCompletion<FrontPanelLockResult>(
                        _rig.driver, _rig.commandPatience(), [&guarded](FrontPanelLockCompletion done) { getFrontPanelLock(guarded.session(), std::move(done)); });
                    expect(read && read->result.lock == lock, "the front panel reads " + lockName(lock));
                }
            }

            // RQ-AKM-054: a clock of 2030 is set and read back to within a few seconds: the year read is the whole
            // year, so the two data bytes are the compound word the catalogue assumes.
            void roundTripClock(GuardedSession& guarded, const SystemSetupSnapshot& original)
            {
                if (!original.clock)
                {
                    finding("clock NOT TESTED: " + original.clockProblem + "; it was neither set nor restored");
                    return;
                }
                expectCommand(guarded, "set the clock to " + clockText(TEST_CLOCK),
                              [](Session& session, CommandCompletion done) { setClockDate(session, TEST_CLOCK, std::move(done)); });
                const auto read = awaitCompletion<ClockDateResult>(
                    _rig.driver, _rig.commandPatience(), [&guarded](ClockDateCompletion done) { getClockDate(guarded.session(), std::move(done)); });
                if (!read || !read->result.clock)
                    throw CheckFailure("could not read the clock back: " + (read ? outcomeText(read->result.outcome) : std::string("no completion")));
                const std::int64_t drift = secondsBetween(TEST_CLOCK, *read->result.clock);
                finding("clock read back as " + clockText(*read->result.clock) + " after setting " + clockText(TEST_CLOCK)
                        + " (the year read is the whole year: the compound word of RQ-AKM-054)");
                expect(drift >= 0 && drift <= CLOCK_RESTORE_TOLERANCE_SECONDS,
                       "the clock read back is the one set, advanced by no more than " + std::to_string(CLOCK_RESTORE_TOLERANCE_SECONDS)
                           + " s (drift " + std::to_string(drift) + " s)");
            }

            // RQ-AKM-058: once the guard is gone, every value is what it was before; the clock is what it was
            // advanced by the time elapsed, to a few seconds. A value that is not back fails the check and says which.
            void expectSystemSetupRestored(GuardedSession& guarded, const SystemSetupSnapshot& original)
            {
                std::string problem;
                const auto after = readSystemSetup(_rig, guarded.session(), problem);
                if (!after)
                    throw CheckFailure("could not read the system setup back to verify it was restored: " + problem);
                expect(after->name == original.name,
                       "the sampler's name is back to \"" + original.name + "\" (reads \"" + after->name + "\")");
                expect(after->playMode == original.playMode,
                       "the Play Mode is back to " + playModeName(original.playMode) + " (reads " + playModeName(after->playMode) + ")");
                expect(after->lock == original.lock,
                       "the front panel is back to " + lockName(original.lock) + " (reads " + lockName(after->lock) + ")");
                if (!original.clock)
                    return;
                if (!after->clock)
                    throw CheckFailure("the clock could not be read back to verify it was restored: " + after->clockProblem);
                const ClockDate expected = addSeconds(*original.clock, elapsedSeconds(_rig, original.clockReadAt));
                const std::int64_t drift = secondsBetween(expected, *after->clock);
                finding("clock restored: reads " + clockText(*after->clock) + ", expected " + clockText(expected) + " (drift "
                        + std::to_string(drift) + " s)");
                expect(drift >= -CLOCK_RESTORE_TOLERANCE_SECONDS && drift <= CLOCK_RESTORE_TOLERANCE_SECONDS,
                       "the clock is back to what it was, advanced by the time elapsed, to " + std::to_string(CLOCK_RESTORE_TOLERANCE_SECONDS)
                           + " s (drift " + std::to_string(drift) + " s)");
            }

            Rig& _rig;
            std::vector<std::string> _findings;
            bool _noSampler = false;
            // What the owner declared of the sampler's MIDI setup (asked once per run), or that they declined (RQ-AKM-080).
            std::optional<MidiConfigDeclaration> _midiDeclaration;
            bool _midiDeclined = false;
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
            if (options.programLifecycle)
                log.note(std::string("It also creates, changes, selects and deletes a program under the reserved name \"")
                         + std::string(TEST_PROGRAM_NAME) + "\" (--program-lifecycle, RQ-AKM-027), and adds keygroups to it to "
                         + "round-trip every section 08 item (RQ-AKM-030, RQ-AKM-033) and every non-sample section 06 item "
                         + "(RQ-AKM-034, RQ-AKM-036): the only checks that touch a stored program, and only one the suite "
                         + "created itself, always deleted again."
                         + (options.sampleName ? " It also assigns the sample \"" + *options.sampleName
                                                      + "\" to a zone of that program by name (--sample-name, RQ-AKM-035,"
                                                        " RQ-AKM-038): the sample itself is never created, changed or deleted."
                                                : " Sample assignment is skipped: no --sample-name was given."));
            if (options.sampleLifecycle)
                log.note(std::string("It also selects the sample named by --sample-name (RQ-AKM-051), skipped if it is empty, and ")
                         + "renames it and back, starts and stops auditioning it, round-trips every settable §0E item on it "
                         + "(RQ-AKM-048) and confirms the grouped replies &34/&4B agree with the items they group (RQ-AKM-049): "
                         + "the sample's name and every settable parameter, and the sampler's original current-sample selection, "
                         + "are restored before this check returns, even if it fails half way, and it never sends &07 or &08.");
            if (options.systemSetup)
                log.note(std::string("It also changes the sampler's own system setup (--system-setup, RQ-AKM-052 to RQ-AKM-055, RQ-AKM-058): ")
                         + "its name, its Play Mode (all four, Muted included), its front-panel lock for an instant, and its clock, "
                         + "each put back before the check returns, even if it fails half way — the lock first, the clock "
                         + "advanced by the time elapsed — and it never sends section 02's Clear Sampler Memory (&32).");
            if (options.diskTools)
                log.note("It also asks the owner which writable disk the sampler reports valid to select (--disk-tools, RQ-AKM-061), creates a "
                         "disposable sub-folder, XS56K_SUITE_TEST, under the current folder (RQ-AKM-071), works inside it, and "
                         "deletes it again through the confirmed &17 guard. The selection stays on the sampler: no section 10 "
                         "command clears it. It touches nothing that existed before.");
            if (options.diskToolsFiles)
                log.note("It also saves the test program into the disposable sub-folder (--disk-tools-files, RQ-AKM-065), has the owner "
                         "confirm the file on the sampler, reads, renames and deletes it.");
            if (options.diskToolsAudition)
                log.note("It also plays the first .WAV file at the root of the selected disk (--disk-tools-audition, RQ-AKM-068) "
                         "for " + millisecondsText(options.auditionDuration) + ", after the owner confirms that one is there.");
            if (options.frontPanel)
                log.note("It also lets the owner drive the sampler's front panel from the PC keyboard (--front-panel, RQ-AKM-076): the owner "
                         "chooses the screen and confirms it, then every PC key sends only the front-panel item the printed mapping gives it, "
                         "section 20 items only. Every key still held is released at the end, and by the session's close if the check fails.");
            if (options.midiConfig)
                log.note("It also changes the sampler's MIDI setup for an instant (--midi-config, RQ-AKM-078 to RQ-AKM-080): PROGRAM CHANGE, "
                         "MULTI SELECT, MULTI SLCT CH, EXT APM CONTROL, AFTERTOUCH and one MIDI filter, each to another value, each "
                         "confirmed by the owner on the sampler's screen, then put back to the value the owner declared (section 04 has "
                         "no Get: nothing is sent before the owner has declared them). Section 04 items only.");
            if (options.diskToolsSlow)
            {
                const char* name = "";
                switch (*options.diskToolsSlow)
                {
                    case DiskSlowOperation::UpdateList:
                        name = "update-list (section 10, item 01)";
                        break;
                    case DiskSlowOperation::LoadFolder:
                        name = "load-folder (section 10, item 15)";
                        break;
                    case DiskSlowOperation::LoadFile:
                        name = "load-file (sections 10, items 2C then 2A)";
                        break;
                    case DiskSlowOperation::LoadFileWithDependents:
                        name = "load-file-with-dependents (sections 10, items 2C then 2B)";
                        break;
                    case DiskSlowOperation::SaveMemoryItem:
                        name = "save-memory-item (section 10, item 2C)";
                        break;
                    case DiskSlowOperation::SaveAllMemoryItems:
                        name = "save-all-memory-items (section 10, item 2D)";
                        break;
                }
                log.note(std::string("It also sends one long-running §10 item, --disk-tools-slow ") + name + ", RQ-AKM-070: "
                         + "documented as potentially hanging the sampler (process/2.architecture/"
                         + "OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md, frames F4-F7).");
            }
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
