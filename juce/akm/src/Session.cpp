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
#include "akm/Session.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <future>
#include <set>
#include <utility>
#include <variant>

#include "akm/Command.hpp"
#include "akm/Confirmation.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/Protocol.hpp"
#include "akm/SamplerError.hpp"
#include "common/midi/MidiMessage.hpp"

namespace akm
{
    namespace
    {
        /// The session stamps one user-ref on each command and cycles the 7-bit range, so that two commands
        /// in a row never share one. [RQ-AKM-007]
        constexpr std::size_t SESSION_USER_REF_COUNT = 1;
        constexpr std::uint8_t FIRST_USER_REF = 0x00;
        /// How many completed commands are remembered, so that an ERROR arriving after a REPLY can be
        /// attributed to the command that REPLY had already completed (spec p. 6). [RQ-AKM-007]
        constexpr std::size_t COMPLETED_COMMANDS_REMEMBERED = 8;
        /// A command addressed to every sampler of the chain (spec p. 6). [RQ-AKM-012]
        constexpr std::uint8_t BROADCAST_DEVICE_ID = 0;
        /// `target` while no DeviceID is bound; every real one is a data byte, so no value is lost.
        constexpr int NO_TARGET = -1;

        /// What a command was, for as long as a late confirmation may still name it.
        struct CompletedCommand
        {
            std::uint8_t userRef = 0;
            std::uint8_t section = 0;
            std::uint8_t item = 0;
            bool completedByReply = false;
        };

        /// The order a closing puts the settings back in: the checksum mode first, so that what follows is framed as
        /// the sampler will then expect, then the settings in the order of the spec's items. [RQ-AKM-042]
        constexpr std::array<SamplerSetting, 5> RESTORE_ORDER{SamplerSetting::Checksums, SamplerSetting::StillAlive,
                                                              SamplerSetting::Notification, SamplerSetting::SyncLcd,
                                                              SamplerSetting::AutoScreenUpdate};

        /// How long a session destroyed without a close waits for its shutdown, in command timeouts and a grace: the
        /// first restoring command that times out ends the restoring, so one timeout is the worst case.
        constexpr int CLOSE_WAIT_IN_COMMAND_TIMEOUTS = 2;
        constexpr std::chrono::milliseconds CLOSE_WAIT_GRACE{1000};

        ItemId itemOf(SamplerSetting setting)
        {
            switch (setting)
            {
                case SamplerSetting::Checksums:
                    return ItemId::SysExChecksum;
                case SamplerSetting::Notification:
                    return ItemId::SysExNotification;
                case SamplerSetting::SyncLcd:
                    return ItemId::SysExSyncLcd;
                case SamplerSetting::AutoScreenUpdate:
                    return ItemId::SysExAutoScreenUpdate;
                case SamplerSetting::StillAlive:
                    return ItemId::SysExStillAlive;
            }
            return ItemId::SysExChecksum;
        }
    }

    /// Everything the session owns. Its state is touched on the executor's thread only, except the few
    /// atomics a caller may read from its own. [ADR-AKM-001 (DEC-AKM-004)]
    struct Session::Impl
    {
        /// Who a unit is for: the application, whose commands are refused while the session is not open or once it
        /// is closing, the opening, whose commands are the only ones sent before it has succeeded, or the closing,
        /// whose commands are the only ones sent once the session is closing. [RQ-AKM-039, RQ-AKM-042]
        enum class Purpose
        {
            Application,
            Opening,
            Closing,
        };

        /// One entry of the queue: a single command, or a sequence that runs as one with nothing
        /// interleaved. A single command is a sequence of one. [RQ-AKM-043, ADR-AKM-001 (DEC-AKM-010)]
        struct Unit
        {
            Purpose purpose = Purpose::Application;
            std::vector<CommandRequest> requests;
            std::vector<CommandResult> results;
            std::optional<std::size_t> failureIndex;
            SequenceCompletion completion;
            std::size_t next = 0;
        };

        /// The one command on the wire, and what is needed to recognise its confirmations and to ignore
        /// the timers of the commands before it. [RQ-AKM-007, RQ-AKM-008, ADR-AKM-001 (DEC-AKM-004)]
        struct InFlight
        {
            std::shared_ptr<Unit> unit;
            std::uint8_t userRef = 0;
            std::uint64_t generation = 0;
            std::uint8_t section = 0;
            std::uint8_t item = 0;
            bool broadcast = false;
            bool collecting = false;
            ChecksumMode decodeMode = ChecksumMode::Unknown;
            Scheduler::Clock::duration timeout{};
            TimerHandle commandTimer;
            TimerHandle totalTimer;
        };

        Impl(SessionTiming sessionTiming, Executor& sessionExecutor, Scheduler& sessionScheduler,
             common::midi::MidiInputPort& inputPort, common::midi::MidiOutputPort& outputPort,
             DiagnosticSink& sink)
            : timing(sessionTiming), executor(sessionExecutor), scheduler(sessionScheduler), input(inputPort),
              output(outputPort), diagnostics(sink)
        {
        }

        const SessionTiming timing;
        Executor& executor;
        Scheduler& scheduler;
        common::midi::MidiInputPort& input;
        common::midi::MidiOutputPort& output;
        DiagnosticSink& diagnostics;

        /// Cleared when the session is destroyed: a task the scheduler or an input callback had already
        /// handed to the executor then does nothing instead of touching a dead session.
        const std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);

        /// One step of the establishment of the section 00 settings: what to send and what its failure means. An
        /// optional setting is one an older OS may lack, which answers ERROR 00 and is no reason to refuse the
        /// connection (RQ-AKM-040).
        struct Step
        {
            SamplerSetting setting = SamplerSetting::Checksums;
            ItemId item = ItemId::SysExChecksum;
            bool on = false;
            bool optional = false;
            CommandOptions options;
        };

        /// What an open in progress carries from one command to the next. [RQ-AKM-039, RQ-AKM-040]
        struct Opening
        {
            SessionConfig config;
            OpenCompletion completion;
            std::set<std::uint8_t> responders;
            std::vector<Step> steps;
            std::size_t nextStep = 0;
            OpenResult result;
        };

        // Written on the session thread, readable from any.
        std::atomic<int> target{NO_TARGET};
        std::atomic<ChecksumMode> mode{ChecksumMode::Unknown};
        std::atomic<bool> stillAlive{false};
        std::atomic<bool> closing{false};
        std::atomic<SessionState> lifecycle{SessionState::Unopened};

        // The session thread's own state: no lock, because nothing else touches it.
        std::deque<std::shared_ptr<Unit>> queue;
        std::optional<InFlight> inFlight;
        std::uint8_t nextUserRef = FIRST_USER_REF;
        std::uint64_t nextGeneration = 0;
        int checksumFailures = 0;
        std::deque<CompletedCommand> completed;
        std::optional<Opening> opening;

        /// What a close in progress carries from one restoring command to the next. [RQ-AKM-042]
        struct Closing
        {
            CloseCompletion completion;
            std::deque<SamplerSetting> pending;
            CloseResult result;
        };
        std::optional<Closing> closingContext;
        /// The settings the session has tried to change on the sampler, for the closing to put back. [RQ-AKM-042]
        std::set<SamplerSetting> changed;

        /// Runs `work` on the session thread, or not at all if the session is gone by then.
        void postToSession(Task work)
        {
            executor.post([flag = alive, work = std::move(work)] {
                if (flag->load())
                    work();
            });
        }

        /// Runs `work` on the session thread `delay` from now; the handle cancels it.
        TimerHandle scheduleSessionTask(Scheduler::Clock::duration delay, Task work)
        {
            return scheduler.scheduleAfter(delay, [flag = alive, this, work = std::move(work)] {
                if (flag->load())
                    postToSession(work);
            });
        }

        // --- the queue ---

        void enqueue(const std::shared_ptr<Unit>& unit)
        {
            // Once the session is closing, only the closing's own commands are sent.
            if (closing.load() && unit->purpose != Purpose::Closing)
            {
                refuseWhole(unit, RefusalReason::SessionClosed);
                return;
            }
            // While the session is opening, and after an open that failed, only the opening's commands are sent:
            // nothing the application submits may reach the sampler before the target is verified and the
            // settings are established (RQ-AKM-039).
            if (unit->purpose == Purpose::Application)
            {
                const SessionState state = lifecycle.load();
                if (state == SessionState::Opening || state == SessionState::OpenFailed)
                {
                    refuseWhole(unit, RefusalReason::SessionNotOpen);
                    return;
                }
            }
            queue.push_back(unit);
            pump();
        }

        /// Sends what can be sent: nothing while a command is in flight, and nothing beyond the head of
        /// the queue, which a sequence keeps until its last command. [RQ-AKM-008, RQ-AKM-043]
        void pump()
        {
            for (;;)
            {
                if (inFlight || queue.empty())
                    return;
                const std::shared_ptr<Unit> unit = queue.front();
                if (unit->next >= unit->requests.size())
                {
                    queue.pop_front();
                    finishUnit(unit);
                    continue;
                }
                if (!startCommand(unit))
                    continue;
                return;
            }
        }

        /// Tries to put the unit's next command on the wire. Returns false when it was refused instead,
        /// its result already recorded.
        bool startCommand(const std::shared_ptr<Unit>& unit)
        {
            const CommandRequest request = unit->requests[unit->next];
            if (request.refusal)
            {
                recordResult(unit, Refused{*request.refusal});
                return false;
            }
            const bool broadcast = request.options.addressing == Addressing::Broadcast;
            if (!broadcast && target.load() == NO_TARGET)
            {
                recordResult(unit, Refused{RefusalReason::NoTargetBound});
                return false;
            }
            if (request.options.expectedReply == ExpectedReply::NeedsKnownChecksumMode
                && mode.load() == ChecksumMode::Unknown)
            {
                recordResult(unit, Refused{RefusalReason::ChecksumModeUnknown});
                return false;
            }

            // A checksum-mode command carries a checksum whatever the mode in force, and its own
            // confirmations straddle the change, so they are decoded in mode Unknown (DEC-AKM-009).
            const bool changesChecksumMode = request.options.checksumModeAfterDone.has_value();
            const ChecksumMode sendMode = changesChecksumMode ? ChecksumMode::Unknown : mode.load();
            const std::uint8_t deviceId =
                broadcast ? BROADCAST_DEVICE_ID : static_cast<std::uint8_t>(target.load());
            const std::uint8_t userRef = takeUserRef();
            const std::array<std::uint8_t, SESSION_USER_REF_COUNT> userRefs{userRef};
            const EncodeResult encoded = encodeCommand(deviceId, userRefs, request.command, sendMode);
            if (!encoded.ok())
            {
                recordResult(unit, Refused{RefusalReason::NotEncodable});
                return false;
            }

            // Tried, whether or not the sampler confirms it: a command that timed out may well have been carried
            // out, and the closing puts back what may have changed (RQ-AKM-042).
            if (request.options.changesSetting)
                changed.insert(*request.options.changesSetting);

            InFlight flight;
            flight.unit = unit;
            flight.userRef = userRef;
            flight.generation = ++nextGeneration;
            flight.section = request.command.section;
            flight.item = request.command.item;
            flight.broadcast = broadcast;
            flight.collecting = request.options.collectionWindow.has_value();
            flight.decodeMode = changesChecksumMode ? ChecksumMode::Unknown : sendMode;
            flight.timeout = request.options.timeout.value_or(timing.commandTimeout);
            // Recorded before the frame goes out, as a second invariant (DEC-AKM-005).
            inFlight = flight;
            output.send(common::midi::MidiMessage::sysEx(encoded.bytes));

            // The clock starts when send() returns: a send lasts as long as the message takes to leave.
            const std::uint64_t generation = flight.generation;
            if (flight.collecting)
            {
                inFlight->commandTimer = scheduleSessionTask(*request.options.collectionWindow,
                                                             [this, generation] { onWindowEnded(generation); });
                return true;
            }
            inFlight->commandTimer =
                scheduleSessionTask(flight.timeout, [this, generation] { onTimeout(generation); });
            inFlight->totalTimer = scheduleSessionTask(request.options.maxTotalWait.value_or(timing.maxTotalWait),
                                                       [this, generation] { onTimeout(generation); });
            return true;
        }

        std::uint8_t takeUserRef()
        {
            const std::uint8_t userRef = nextUserRef;
            nextUserRef = static_cast<std::uint8_t>(userRef == DATA_BYTE_MAX ? FIRST_USER_REF : userRef + 1);
            return userRef;
        }

        /// Records one command's result and, when it did not succeed, cancels the rest of its sequence.
        /// [RQ-AKM-043]
        void recordResult(const std::shared_ptr<Unit>& unit, const CommandResult& result)
        {
            const std::size_t index = unit->next;
            unit->results.push_back(result);
            ++unit->next;
            if (succeeded(result))
                return;
            if (!unit->failureIndex)
                unit->failureIndex = index;
            while (unit->results.size() < unit->requests.size())
                unit->results.emplace_back(Cancelled{});
            unit->next = unit->requests.size();
        }

        void finishUnit(const std::shared_ptr<Unit>& unit)
        {
            if (unit->completion)
                unit->completion(SequenceResult{unit->results, unit->failureIndex});
        }

        /// A unit submitted to a closed session: nothing of it is sent. [RQ-AKM-042]
        void refuseWhole(const std::shared_ptr<Unit>& unit, RefusalReason reason)
        {
            while (unit->results.size() < unit->requests.size())
            {
                if (!unit->failureIndex)
                    unit->failureIndex = unit->results.size();
                unit->results.emplace_back(Refused{reason});
            }
            unit->next = unit->requests.size();
            finishUnit(unit);
        }

        // --- received messages ---

        void onFrameReceived(const std::vector<std::uint8_t>& frame)
        {
            const ChecksumMode decodeMode = inFlight ? inFlight->decodeMode : mode.load();
            const DecodedMessage decoded = decodeMessage(frame, decodeMode);
            if (std::holds_alternative<StillAliveMessage>(decoded))
            {
                onStillAlive();
                return;
            }
            if (const auto* rejected = std::get_if<Rejected>(&decoded))
            {
                onRejected(*rejected);
                return;
            }
            onConfirmation(std::get<Confirmation>(decoded));
        }

        /// `F0 F7` while the sampler's Still Alive monitor is on: it is busy, so the command's timeout
        /// starts again — the maximum total wait, armed once, is not touched. [RQ-AKM-011]
        void onStillAlive()
        {
            if (!stillAlive.load() || !inFlight || inFlight->collecting)
                return;
            inFlight->commandTimer.cancel();
            const std::uint64_t generation = inFlight->generation;
            inFlight->commandTimer =
                scheduleSessionTask(inFlight->timeout, [this, generation] { onTimeout(generation); });
        }

        void onRejected(const Rejected& rejected)
        {
            Diagnostic diagnostic;
            diagnostic.kind = DiagnosticKind::RejectedMessage;
            diagnostic.rejection = rejected.reason;
            diagnostics.report(diagnostic);

            // A run of failed verifications is what a sampler that rebooted looks like from here.
            if (rejected.reason != RejectReason::BadChecksum || mode.load() != ChecksumMode::On)
                return;
            ++checksumFailures;
            if (checksumFailures >= timing.checksumFailuresBeforeUnknown)
                setChecksumMode(ChecksumMode::Unknown);
        }

        void onConfirmation(const Confirmation& confirmation)
        {
            checksumFailures = 0;
            if (inFlight && matches(*inFlight, confirmation))
            {
                onMatched(confirmation);
                return;
            }

            Diagnostic diagnostic;
            diagnostic.kind = confirmation.replyId == ReplyId::Error && wasCompletedByReply(confirmation)
                                  ? DiagnosticKind::LateErrorAfterReply
                                  : DiagnosticKind::UnsolicitedConfirmation;
            diagnostic.confirmation = confirmation;
            diagnostics.report(diagnostic);
        }

        /// A confirmation belongs to the command in flight when its echoed user-refs, section and item are
        /// its own and it comes from the bound target — the sampler answers with its own DeviceID, so a
        /// broadcast command accepts any. [RQ-AKM-007, RQ-AKM-012]
        [[nodiscard]] bool matches(const InFlight& flight, const Confirmation& confirmation) const
        {
            if (confirmation.userRefs.size() != SESSION_USER_REF_COUNT
                || confirmation.userRefs.front() != flight.userRef)
                return false;
            if (confirmation.section != flight.section || confirmation.item != flight.item)
                return false;
            if (flight.broadcast)
                return true;
            return confirmation.deviceId == static_cast<std::uint8_t>(target.load());
        }

        [[nodiscard]] bool wasCompletedByReply(const Confirmation& confirmation) const
        {
            if (confirmation.userRefs.size() != SESSION_USER_REF_COUNT)
                return false;
            const std::uint8_t userRef = confirmation.userRefs.front();
            return std::any_of(completed.begin(), completed.end(), [&](const CompletedCommand& candidate) {
                return candidate.completedByReply && candidate.userRef == userRef
                       && candidate.section == confirmation.section && candidate.item == confirmation.item;
            });
        }

        void onMatched(const Confirmation& confirmation)
        {
            // Discovery collects instead of completing: its window decides when it is over (RQ-AKM-012).
            if (inFlight->collecting)
            {
                const CommandOptions& options = inFlight->unit->requests[inFlight->unit->next].options;
                if (options.onConfirmation)
                    options.onConfirmation(confirmation);
                return;
            }

            switch (confirmation.replyId)
            {
                case ReplyId::Ok:
                    // An acknowledgement, not a completion: it can even be switched off (§00/&01).
                    return;
                case ReplyId::Done:
                    complete(Done{});
                    return;
                case ReplyId::Reply:
                    complete(Reply{confirmation.data});
                    return;
                case ReplyId::Error:
                    complete(Error{errorNumber(confirmation).value_or(error_number::UNKNOWN_ERROR)});
                    return;
            }
        }

        // --- completion ---

        void complete(const CommandResult& result)
        {
            const InFlight flight = *inFlight;
            flight.commandTimer.cancel();
            flight.totalTimer.cancel();
            inFlight.reset();

            const std::shared_ptr<Unit> unit = flight.unit;
            const CommandOptions& options = unit->requests[unit->next].options;
            // What the protocol cannot read back, the command's own outcome tells (DEC-AKM-011).
            if (options.checksumModeAfterDone)
                setChecksumMode(succeeded(result)
                                    ? (*options.checksumModeAfterDone ? ChecksumMode::On : ChecksumMode::Off)
                                    : ChecksumMode::Unknown);
            if (options.stillAliveAfterDone && succeeded(result))
                stillAlive.store(*options.stillAliveAfterDone);
            // A setting the sampler does not have has not been changed: there is nothing for a close to put back.
            if (options.changesSetting && notSupported(result))
                changed.erase(*options.changesSetting);

            remember(CompletedCommand{flight.userRef, flight.section, flight.item,
                                      std::holds_alternative<Reply>(result)});
            recordResult(unit, result);
            pump();
        }

        void remember(const CompletedCommand& command)
        {
            completed.push_back(command);
            if (completed.size() > COMPLETED_COMMANDS_REMEMBERED)
                completed.pop_front();
        }

        void setChecksumMode(ChecksumMode next)
        {
            checksumFailures = 0;
            if (mode.exchange(next) == next)
                return;
            Diagnostic diagnostic;
            diagnostic.kind = DiagnosticKind::ChecksumModeChanged;
            diagnostic.checksumMode = next;
            diagnostics.report(diagnostic);
        }

        void onTimeout(std::uint64_t generation)
        {
            if (!inFlight || inFlight->generation != generation)
                return;
            complete(Timeout{});
        }

        void onWindowEnded(std::uint64_t generation)
        {
            if (!inFlight || inFlight->generation != generation)
                return;
            complete(Done{});
        }

        // --- opening (DEC-AKM-007) ---

        /// Sends a command on behalf of the session itself — the opening or the closing — which the gates on the
        /// application's commands let through.
        void submitInternal(Purpose purpose, CommandRequest request, CommandCompletion completion)
        {
            auto unit = std::make_shared<Unit>();
            unit->purpose = purpose;
            unit->requests.push_back(std::move(request));
            unit->completion = [completion = std::move(completion)](const SequenceResult& outcome) {
                completion(outcome.results.front());
            };
            enqueue(unit);
        }

        void submitForOpening(CommandRequest request, CommandCompletion completion)
        {
            submitInternal(Purpose::Opening, std::move(request), std::move(completion));
        }

        /// The settings to establish, in order: the checksum mode first, always, then each other setting that is
        /// not left unchanged, in the order of the spec's items. [RQ-AKM-040]
        static std::vector<Step> stepsOf(const SessionConfig& config)
        {
            std::vector<Step> steps;
            Step checksums;
            checksums.setting = SamplerSetting::Checksums;
            checksums.item = ItemId::SysExChecksum;
            checksums.on = config.checksums;
            checksums.options.checksumModeAfterDone = config.checksums;
            checksums.options.changesSetting = SamplerSetting::Checksums;
            steps.push_back(std::move(checksums));

            const auto add = [&steps](SamplerSetting setting, ItemId item, SettingChoice choice, bool optional,
                                      std::optional<bool> CommandOptions::*trackedByTheSession) {
                if (choice == SettingChoice::Unchanged)
                    return;
                Step step;
                step.setting = setting;
                step.item = item;
                step.on = choice == SettingChoice::On;
                step.optional = optional;
                step.options.changesSetting = setting;
                if (trackedByTheSession != nullptr)
                    step.options.*trackedByTheSession = step.on;
                steps.push_back(std::move(step));
            };
            add(SamplerSetting::Notification, ItemId::SysExNotification, config.notification, false, nullptr);
            add(SamplerSetting::SyncLcd, ItemId::SysExSyncLcd, config.syncLcd, true, nullptr);
            add(SamplerSetting::AutoScreenUpdate, ItemId::SysExAutoScreenUpdate, config.autoScreenUpdate, true, nullptr);
            add(SamplerSetting::StillAlive, ItemId::SysExStillAlive, config.stillAlive, true,
                &CommandOptions::stillAliveAfterDone);
            return steps;
        }

        void beginOpen(SessionConfig config, OpenCompletion completion)
        {
            OpenResult refused;
            refused.targetDeviceId = config.targetDeviceId;
            const SessionState state = lifecycle.load();
            if (closing.load())
                refused.status = OpenStatus::Cancelled;
            else if (state == SessionState::Opening || state == SessionState::Open)
                refused.status = OpenStatus::AlreadyOpen;
            else if (config.targetDeviceId > DEVICE_ID_MAX)
                refused.status = OpenStatus::InvalidDeviceId;
            else
            {
                lifecycle.store(SessionState::Opening);
                // A retry after a failed open starts from nothing bound.
                target.store(NO_TARGET);
                Opening context;
                context.steps = stepsOf(config);
                context.result.targetDeviceId = config.targetDeviceId;
                context.config = std::move(config);
                context.completion = std::move(completion);
                opening = std::move(context);
                startDiscovery();
                return;
            }
            completion(refused);
        }

        /// A Query to every sampler, collecting the DeviceID of every confirmation of the window — an ERROR
        /// answer counts as a sampler being present. [RQ-AKM-012, RQ-AKM-039]
        void startDiscovery()
        {
            CommandOptions options;
            options.addressing = Addressing::Broadcast;
            options.collectionWindow = timing.discoveryWindow;
            options.onConfirmation = [this](const Confirmation& confirmation) {
                if (opening)
                    opening->responders.insert(confirmation.deviceId);
            };
            submitForOpening(makeRequest(ItemId::SysExQuery, std::span<const std::int64_t>{}, std::move(options)),
                             [this](const CommandResult& result) { onDiscovered(result); });
        }

        static bool wasCancelled(const CommandResult& result)
        {
            const auto* refused = std::get_if<Refused>(&result);
            return std::holds_alternative<Cancelled>(result)
                   || (refused != nullptr && refused->reason == RefusalReason::SessionClosed);
        }

        void onDiscovered(const CommandResult& result)
        {
            if (!succeeded(result))
            {
                opening->result.status = wasCancelled(result) ? OpenStatus::Cancelled : OpenStatus::DiscoveryFailed;
                finishOpen();
                return;
            }

            OpenResult& outcome = opening->result;
            outcome.responders.assign(opening->responders.begin(), opening->responders.end());
            const auto answeredAs = [&outcome](std::uint32_t deviceId) {
                return std::find(outcome.responders.begin(), outcome.responders.end(), deviceId) != outcome.responders.end();
            };
            const std::uint32_t wanted = opening->config.targetDeviceId;
            // A target of 0, or a sampler answering as 0, reaches every sampler: with more than one present, it
            // would edit the wrong machine (DEC-AKM-007), which is told before the target is found absent.
            if (outcome.responders.size() > 1 && (wanted == 0 || answeredAs(0)))
                outcome.status = OpenStatus::AmbiguousSamplers;
            else if (!answeredAs(wanted))
                outcome.status = OpenStatus::NoSamplerAtTarget;
            else
            {
                target.store(static_cast<int>(wanted));
                continueOpening();
                return;
            }
            finishOpen();
        }

        /// Goes on with the next setting on a turn of its own, so that a step refused on the spot cannot make the
        /// opening recurse.
        void continueOpening()
        {
            postToSession([this] {
                if (opening)
                    establishNext();
            });
        }

        void establishNext()
        {
            if (opening->nextStep >= opening->steps.size())
            {
                opening->result.status =
                    opening->result.unsupported.empty() ? OpenStatus::Ready : OpenStatus::ReadyDegraded;
                finishOpen();
                return;
            }
            const Step& step = opening->steps[opening->nextStep];
            submitForOpening(makeRequest(step.item, {step.on ? 1 : 0}, step.options),
                             [this](const CommandResult& result) { onStepDone(result); });
        }

        static bool notSupported(const CommandResult& result)
        {
            const auto* error = std::get_if<Error>(&result);
            return error != nullptr && error->number == error_number::NOT_SUPPORTED;
        }

        void onStepDone(const CommandResult& result)
        {
            const Step& step = opening->steps[opening->nextStep];
            if (succeeded(result))
            {
                // Nothing to keep: the session remembered that it tried to change the setting when it sent it.
            }
            else if (wasCancelled(result))
            {
                opening->result.status = OpenStatus::Cancelled;
                finishOpen();
                return;
            }
            else if (step.optional && notSupported(result))
                opening->result.unsupported.push_back(step.setting);
            else
            {
                opening->result.status = OpenStatus::SettingFailed;
                opening->result.failedSetting = step.setting;
                opening->result.failedResult = result;
                finishOpen();
                return;
            }
            ++opening->nextStep;
            continueOpening();
        }

        void finishOpen()
        {
            OpenResult result = std::move(opening->result);
            const OpenCompletion completion = std::move(opening->completion);
            opening.reset();
            lifecycle.store(result.ready() ? SessionState::Open : SessionState::OpenFailed);
            if (completion)
                completion(result);
        }

        // --- closing (DEC-AKM-004, DEC-AKM-007) ---

        /// The last task that is the application's: what is in flight or queued ends as `Cancelled`, then the closing
        /// puts back what the session changed, one command at a time. [RQ-AKM-042]
        void beginClose(CloseCompletion completion)
        {
            cancelEverything();
            Closing context;
            context.completion = std::move(completion);
            for (const SamplerSetting setting : RESTORE_ORDER)
            {
                if (changed.count(setting) != 0)
                    context.pending.push_back(setting);
            }
            closingContext = std::move(context);
            restoreNext();
        }

        void restoreNext()
        {
            if (closingContext->pending.empty())
            {
                finishClose();
                return;
            }
            const SamplerSetting setting = closingContext->pending.front();
            const bool on = samplerDefault(setting);
            CommandOptions options;
            // What the session follows about the port follows the restoring too.
            if (setting == SamplerSetting::Checksums)
                options.checksumModeAfterDone = on;
            if (setting == SamplerSetting::StillAlive)
                options.stillAliveAfterDone = on;
            submitInternal(Purpose::Closing, makeRequest(itemOf(setting), {on ? 1 : 0}, std::move(options)),
                           [this, setting](const CommandResult& result) { onRestored(setting, result); });
        }

        void onRestored(SamplerSetting setting, const CommandResult& result)
        {
            CloseResult& outcome = closingContext->result;
            closingContext->pending.pop_front();
            if (succeeded(result))
                outcome.restored.push_back(setting);
            else
            {
                outcome.notRestored.push_back(setting);
                if (std::holds_alternative<Timeout>(result))
                {
                    // The sampler is not answering: a port that did not answer this one will not answer the others.
                    for (const SamplerSetting left : closingContext->pending)
                        outcome.notRestored.push_back(left);
                    closingContext->pending.clear();
                }
            }
            // On a turn of its own, so that a refusal on the spot cannot make the closing recurse.
            postToSession([this] {
                if (closingContext)
                    restoreNext();
            });
        }

        void finishClose()
        {
            // The input is stopped from the session's own thread and never from the backend's callback: a JUCE input
            // cannot be stopped from inside its own callback.
            input.stop();
            lifecycle.store(SessionState::Closed);
            CloseResult result = std::move(closingContext->result);
            const CloseCompletion completion = std::move(closingContext->completion);
            closingContext.reset();
            if (completion)
                completion(result);
        }

        /// The command in flight and everything queued end as `Cancelled`. [RQ-AKM-042]
        void cancelEverything()
        {
            if (inFlight)
            {
                const InFlight flight = *inFlight;
                flight.commandTimer.cancel();
                flight.totalTimer.cancel();
                inFlight.reset();
                recordResult(flight.unit, Cancelled{});
            }
            while (!queue.empty())
            {
                const std::shared_ptr<Unit> unit = queue.front();
                queue.pop_front();
                while (unit->results.size() < unit->requests.size())
                {
                    if (!unit->failureIndex)
                        unit->failureIndex = unit->results.size();
                    unit->results.emplace_back(Cancelled{});
                }
                unit->next = unit->requests.size();
                finishUnit(unit);
            }
        }
    };

    Session::Session(SessionTiming timing, Executor& executor, Scheduler& scheduler,
                     common::midi::MidiInputPort& input, common::midi::MidiOutputPort& output,
                     DiagnosticSink& diagnostics)
        : _impl(std::make_unique<Impl>(timing, executor, scheduler, input, output, diagnostics))
    {
        Impl* impl = _impl.get();
        common::midi::MidiInputCallbacks callbacks;
        // The backend's thread only copies and enqueues: nothing is decoded, sent or completed on it
        // (RQ-AKM-020, DEC-AKM-005).
        callbacks.onSysExMessage = [impl](const common::midi::MidiMessage& message) {
            impl->postToSession(
                [impl, frame = message.toBytes()] { impl->onFrameReceived(frame); });
        };
        input.setCallbacks(std::move(callbacks));
        // Started before the first send can happen (DEC-AKM-005).
        input.start();
    }

    Session::~Session()
    {
        Impl* impl = _impl.get();
        // A session that was not closed shuts down as close() does — where its executor can run the work without
        // anyone driving it, and from a thread that is not its own (RQ-AKM-042).
        if (impl->executor.runsOnItsOwnThread() && !impl->executor.isCurrentThread())
        {
            const auto done = std::make_shared<std::promise<void>>();
            std::future<void> finished = done->get_future();
            if (close(CloseCompletion([done](const CloseResult&) { done->set_value(); })))
                static_cast<void>(
                    finished.wait_for(impl->timing.commandTimeout * CLOSE_WAIT_IN_COMMAND_TIMEOUTS + CLOSE_WAIT_GRACE));
        }
        impl->alive->store(false);
        impl->input.stop();
        impl->input.setCallbacks({});
    }

    void Session::open(SessionConfig config, OpenCompletion completion)
    {
        Impl* impl = _impl.get();
        impl->postToSession([impl, config = std::move(config), completion = std::move(completion)]() mutable {
            impl->beginOpen(std::move(config), std::move(completion));
        });
    }

    SessionState Session::state() const
    {
        const SessionState state = _impl->lifecycle.load();
        if (state != SessionState::Closed && _impl->closing.load())
            return SessionState::Closing;
        return state;
    }

    void Session::bindTarget(std::uint8_t deviceId)
    {
        Impl* impl = _impl.get();
        impl->postToSession([impl, deviceId] { impl->target.store(deviceId); });
    }

    std::optional<std::uint8_t> Session::boundTarget() const
    {
        const int bound = _impl->target.load();
        if (bound == NO_TARGET)
            return std::nullopt;
        return static_cast<std::uint8_t>(bound);
    }

    ChecksumMode Session::checksumMode() const
    {
        return _impl->mode.load();
    }

    bool Session::stillAliveMonitoring() const
    {
        return _impl->stillAlive.load();
    }

    void Session::submit(CommandRequest request, CommandCompletion completion)
    {
        std::vector<CommandRequest> requests;
        requests.push_back(std::move(request));
        submitSequence(std::move(requests),
                       [completion = std::move(completion)](const SequenceResult& outcome) {
                           if (completion && !outcome.results.empty())
                               completion(outcome.results.front());
                       });
    }

    void Session::submitSequence(std::vector<CommandRequest> requests, SequenceCompletion completion)
    {
        auto unit = std::make_shared<Impl::Unit>();
        unit->requests = std::move(requests);
        unit->completion = std::move(completion);
        Impl* impl = _impl.get();
        impl->postToSession([impl, unit] { impl->enqueue(unit); });
    }

    bool Session::close(CloseCompletion onClosed)
    {
        // Closing from a completion would wait on the thread that has to do the work (DEC-AKM-004).
        if (_impl->executor.isCurrentThread())
            return false;
        if (_impl->closing.exchange(true))
            return false;

        Impl* impl = _impl.get();
        impl->postToSession([impl, onClosed = std::move(onClosed)]() mutable { impl->beginClose(std::move(onClosed)); });
        return true;
    }

    bool Session::close(Task onClosed)
    {
        return close(CloseCompletion([onClosed = std::move(onClosed)](const CloseResult&) {
            if (onClosed)
                onClosed();
        }));
    }
}
