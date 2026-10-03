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
#include "akm/ItemCatalogue.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/DiskPrimitives.hpp"
#include "akm/KeygroupPrimitives.hpp"
#include "akm/ProgramPrimitives.hpp"
#include "akm/SamplePrimitives.hpp"
#include "akm/SysExConfig.hpp"
#include "akm/SystemSetup.hpp"
#include "akm/ZonePrimitives.hpp"
#include "akm/harness/ClockArithmetic.hpp"
#include "akm/harness/KeygroupParameterCases.hpp"
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
                                                                                  + (timed ? outcomeText(timed->result)
                                                                                          : std::string("no completion"))
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
                                       + "\": " + (timed ? outcomeText(timed->result) : std::string("no completion")));
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
                              + (ok ? "done" : "failed (" + (timed ? outcomeText(timed->result) : std::string("no completion")) + ")"));
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
                                       + "\": " + (timed ? outcomeText(timed->result) : std::string("no completion"))
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
                                       + (timed ? outcomeText(timed->result) : std::string("no completion")));
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
                              + (ok ? "done" : "failed (" + (timed ? outcomeText(timed->result) : std::string("no completion")) + ")"));
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
                              + (ok ? "done" : "failed (" + (timed ? outcomeText(timed->result) : std::string("no completion")) + ")"));
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
                                    : "failed (" + (timed ? outcomeText(timed->result) : std::string("no completion")) + ")"));
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
                if (_rig.options.diskTools && _rig.options.diskToolsSlow)
                    check("one long-running §10 item, sent inside the disposable sub-folder with Still Alive on, "
                          "then the sub-folder deleted",
                          &Suite::diskToolsSlowOperation);
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

            // RQ-AKM-071, the safe half: reads the current disk without changing it, then, inside the disposable
            // sub-folder, creates, renames, enters and leaves a sub-folder, reads the folder items and the file items of
            // the empty folder, and deletes the whole sub-folder through the confirmed &17 guard. A file cannot be made
            // without saving one, which is one of the long-running items, so no file is listed here.
            void diskToolsRoundTrips()
            {
                GuardedSession guarded(_rig);
                guarded.open(baseConfig());

                const DiskTypeResult type = readDisk<DiskTypeResult>("the current disk's type", [&guarded](DiskTypeCompletion done) {
                    getCurrentDiskType(guarded.session(), std::move(done));
                });
                expect(type.type.has_value(), "the current disk's type is read");
                const DiskHandleResult handle = readDisk<DiskHandleResult>("the current disk's handle", [&guarded](DiskHandleCompletion done) {
                    getCurrentDiskHandle(guarded.session(), std::move(done));
                });
                expect(handle.handle.has_value(), "the current disk's handle is read");
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
                if (!before || !before->result.count)
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
                if (!before || !before->result.count)
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
                if (!keygroupCountResult || !keygroupCountResult->result.count)
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

                if (program.hadOriginalProgram())
                {
                    expect(program.selectOriginalProgram(), "navigated away to the program that was current before");
                    bool refused = false;
                    try
                    {
                        program.expectOnTestProgram("select keygroup 0 (on the wrong program)",
                                                    [](Session& session, CommandCompletion done) {
                                                        selectKeygroup(session, 0, std::move(done));
                                                    });
                    }
                    catch (const CheckFailure&)
                    {
                        refused = true;
                    }
                    expect(refused, "selecting keygroup 0 on the program that is current but not the test one was refused before sending");
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
                if (!before || !before->result.count)
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
                if (!keygroupCountResult || !keygroupCountResult->result.count)
                    throw CheckFailure("could not read the keygroup count after adding a keygroup");
                const int keygroupCount = *keygroupCountResult->result.count;
                expect(keygroupCount == 2, "the keygroup count read back is 2 (1 default + 1 added)");

                program.expectOnTestProgram("select keygroup 2", [](Session& session, CommandCompletion done) {
                    selectKeygroup(session, 2, std::move(done));
                });

                for (const ZoneParameterCase& parameterCase : allZoneParameterCases())
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

                if (program.hadOriginalProgram())
                {
                    expect(program.selectOriginalProgram(), "navigated away to the program that was current before");
                    bool refused = false;
                    try
                    {
                        program.expectOnTestProgram("set Zone Level (on the wrong program)", [](Session& session, CommandCompletion done) {
                            session.submit(makeRequest(ItemId::ZoneSetLevel, {1, 0, 1}), std::move(done));
                        });
                    }
                    catch (const CheckFailure&)
                    {
                        refused = true;
                    }
                    expect(refused, "a zone command on the program that is current but not the test one was refused before sending");
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
                log.note("It also creates a disposable sub-folder, XS56K_SUITE_TEST, under the current folder (--disk-tools, "
                         "RQ-AKM-071), works inside it, and deletes it again through the confirmed &17 guard. It selects no other "
                         "disk and touches nothing that existed before.");
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
