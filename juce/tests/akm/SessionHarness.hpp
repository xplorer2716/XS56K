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

// A session wired to the simulated sampler on a scenario driver, with what the session tests need to
// watch it: the frames the host put on the wire, the diagnostics, the results and the threads they ran
// on. [TASK-AKM-006, RQ-AKM-016, RQ-AKM-019, ADR-AKM-001 (DEC-AKM-008)]
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

#include "TestBytes.hpp"
#include "akm/Checksum.hpp"
#include "akm/Protocol.hpp"
#include "akm/Session.hpp"
#include "akm/harness/ScenarioDriver.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"
#include "common/midi/MidiMessage.hpp"

namespace akm::test
{
    // The spec items these tests drive, until TASK-AKM-008 generates the item catalogue.
    inline constexpr std::uint8_t SECTION_CONFIG = 0x00;
    inline constexpr std::uint8_t ITEM_QUERY = 0x00;
    inline constexpr std::uint8_t ITEM_NOTIFICATION = 0x01;
    inline constexpr std::uint8_t ITEM_SYNC_LCD = 0x03;
    inline constexpr std::uint8_t ITEM_CHECKSUM = 0x04;
    inline constexpr std::uint8_t ITEM_AUTO_SCREEN_UPDATE = 0x05;
    inline constexpr std::uint8_t ITEM_ECHO = 0x06;
    inline constexpr std::uint8_t ITEM_STILL_ALIVE = 0x07;
    inline constexpr std::uint8_t SECTION_SYSTEM = 0x02;
    inline constexpr std::uint8_t ITEM_OS_VERSION = 0x00;

    inline constexpr std::uint8_t TOGGLE_OFF = 0;
    inline constexpr std::uint8_t TOGGLE_ON = 1;

    // A command frame the session sends: F0 47 5E <dev> <user-ref> <section> <item> <data...> [<chk>] F7,
    // always with one user-ref.
    inline constexpr std::size_t SENT_USER_REF_INDEX = FIRST_USER_REF_INDEX;
    inline constexpr std::size_t SENT_SECTION_INDEX = FIRST_USER_REF_INDEX + 1;
    inline constexpr std::size_t SENT_ITEM_INDEX = FIRST_USER_REF_INDEX + 2;
    /// F0, the two IDs, <dev>, one user-ref, the section, the item and F7.
    inline constexpr std::size_t SENT_FRAME_OVERHEAD = 8;
    inline constexpr std::size_t CHECKSUM_SIZE = 1;
    // A confirmation: F0 47 5E <dev> <user-ref> <reply ID> <section> <item> <data...> [<chk>] F7.
    inline constexpr std::size_t CONFIRMATION_REPLY_INDEX = FIRST_USER_REF_INDEX + 1;
    inline constexpr std::size_t CONFIRMATION_SECTION_INDEX = FIRST_USER_REF_INDEX + 2;
    inline constexpr std::size_t CONFIRMATION_ITEM_INDEX = FIRST_USER_REF_INDEX + 3;
    inline constexpr std::size_t CONFIRMATION_DATA_INDEX = FIRST_USER_REF_INDEX + 4;

    /// A value a completion sets on the session's thread and the test reads on its own.
    template <typename T>
    class Latched
    {
    public:
        void set(T value)
        {
            const std::lock_guard lock(_mutex);
            _value = std::move(value);
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

    private:
        mutable std::mutex _mutex;
        std::optional<T> _value;
    };

    /// Keeps every diagnostic a session reported, in order.
    class RecordingDiagnostics final : public DiagnosticSink
    {
    public:
        void report(const Diagnostic& diagnostic) override
        {
            const std::lock_guard lock(_mutex);
            _events.push_back(diagnostic);
        }

        [[nodiscard]] std::vector<Diagnostic> events() const
        {
            const std::lock_guard lock(_mutex);
            return _events;
        }

        [[nodiscard]] std::size_t count(DiagnosticKind kind) const
        {
            const std::vector<Diagnostic> events = this->events();
            return static_cast<std::size_t>(
                std::count_if(events.begin(), events.end(), [kind](const Diagnostic& d) { return d.kind == kind; }));
        }

        [[nodiscard]] std::optional<Diagnostic> last(DiagnosticKind kind) const
        {
            const std::vector<Diagnostic> events = this->events();
            const auto found = std::find_if(events.rbegin(), events.rend(),
                                            [kind](const Diagnostic& d) { return d.kind == kind; });
            if (found == events.rend())
                return std::nullopt;
            return *found;
        }

    private:
        mutable std::mutex _mutex;
        std::vector<Diagnostic> _events;
    };

    /// Keeps the results completions received, in completion order, with the thread each ran on.
    class Recorder
    {
    public:
        [[nodiscard]] CommandCompletion completion()
        {
            return [this](const CommandResult& result) {
                const std::lock_guard lock(_mutex);
                _results.push_back(result);
                _threads.push_back(std::this_thread::get_id());
            };
        }

        [[nodiscard]] SequenceCompletion sequenceCompletion()
        {
            return [this](const SequenceResult& result) {
                const std::lock_guard lock(_mutex);
                _sequences.push_back(result);
                _threads.push_back(std::this_thread::get_id());
            };
        }

        [[nodiscard]] std::vector<CommandResult> results() const
        {
            const std::lock_guard lock(_mutex);
            return _results;
        }

        [[nodiscard]] std::vector<SequenceResult> sequences() const
        {
            const std::lock_guard lock(_mutex);
            return _sequences;
        }

        [[nodiscard]] std::size_t count() const
        {
            const std::lock_guard lock(_mutex);
            return _results.size() + _sequences.size();
        }

        [[nodiscard]] std::vector<std::thread::id> threads() const
        {
            const std::lock_guard lock(_mutex);
            return _threads;
        }

    private:
        mutable std::mutex _mutex;
        std::vector<CommandResult> _results;
        std::vector<SequenceResult> _sequences;
        std::vector<std::thread::id> _threads;
    };

    /// One session, one simulated sampler, one bus, on the driver the test chose.
    class SessionHarness
    {
    public:
        /// How long a helper waits for something to happen: long enough for the real driver, free on the
        /// manual one.
        static constexpr auto DEFAULT_WAIT = std::chrono::seconds(2);
        /// Longer than the session's own command timeout, which a close on a silent sampler waits out once.
        static constexpr auto CLOSE_WAIT = std::chrono::seconds(10);

        explicit SessionHarness(harness::ScenarioDriver& driver, SessionTiming timing = {},
                                harness::SamplerConfig config = {}, bool bindTargetAtStart = true)
            : _driver(driver), _backend(driver.scheduler()), _sampler(&_backend.addSampler(config)),
              _input(_backend.openInput(_backend.inputName())), _output(_backend.openOutput(_backend.outputName())),
              _session(timing, driver.executor(), driver.scheduler(), *_input, *_output, _diagnostics)
        {
            if (bindTargetAtStart)
                _session.bindTarget(config.deviceId);
        }

        ~SessionHarness() { closeAndWait(); }

        SessionHarness(const SessionHarness&) = delete;
        SessionHarness& operator=(const SessionHarness&) = delete;

        [[nodiscard]] Session& session() { return _session; }
        [[nodiscard]] harness::SimulatedSampler& sampler() { return *_sampler; }
        [[nodiscard]] harness::SimulatedMidiBackend& backend() { return _backend; }
        [[nodiscard]] RecordingDiagnostics& diagnostics() { return _diagnostics; }
        [[nodiscard]] Recorder& recorder() { return _recorder; }

        /// Runs what is already due and nothing more.
        void settle() { _driver.elapse(Scheduler::Clock::duration::zero()); }
        void elapse(Scheduler::Clock::duration duration) { _driver.elapse(duration); }
        [[nodiscard]] bool waitUntil(const std::function<bool()>& condition,
                                     Scheduler::Clock::duration timeout = DEFAULT_WAIT)
        {
            return _driver.waitUntil(condition, timeout);
        }

        /// Waits until `count` completions have been recorded.
        [[nodiscard]] bool waitForCompletions(std::size_t count, Scheduler::Clock::duration timeout = DEFAULT_WAIT)
        {
            return waitUntil([this, count] { return _recorder.count() >= count; }, timeout);
        }

        void submit(CommandRequest request) { _session.submit(std::move(request), _recorder.completion()); }
        void submit(Command command, CommandOptions options = {})
        {
            submit(CommandRequest{std::move(command), std::move(options)});
        }

        /// Submits one command and waits for its result.
        [[nodiscard]] std::optional<CommandResult> submitAndWait(CommandRequest request,
                                                                 Scheduler::Clock::duration timeout = DEFAULT_WAIT)
        {
            auto latched = std::make_shared<Latched<CommandResult>>();
            _session.submit(std::move(request), [latched](const CommandResult& result) { latched->set(result); });
            static_cast<void>(waitUntil([latched] { return latched->isSet(); }, timeout));
            return latched->value();
        }

        [[nodiscard]] std::optional<SequenceResult> submitSequenceAndWait(
            std::vector<CommandRequest> requests, Scheduler::Clock::duration timeout = DEFAULT_WAIT)
        {
            auto latched = std::make_shared<Latched<SequenceResult>>();
            _session.submitSequence(std::move(requests),
                                    [latched](const SequenceResult& result) { latched->set(result); });
            static_cast<void>(waitUntil([latched] { return latched->isSet(); }, timeout));
            return latched->value();
        }

        /// Opens the session with `config` (TASK-AKM-009) and waits for the result.
        [[nodiscard]] std::optional<OpenResult> openAndWait(SessionConfig config,
                                                            Scheduler::Clock::duration timeout = DEFAULT_WAIT)
        {
            auto latched = std::make_shared<Latched<OpenResult>>();
            _session.open(std::move(config), [latched](const OpenResult& result) { latched->set(result); });
            static_cast<void>(waitUntil([latched] { return latched->isSet(); }, timeout));
            return latched->value();
        }

        /// Runs the §00/&04 command that switches the port's checksum mode, as TASK-AKM-008's helper will.
        [[nodiscard]] std::optional<CommandResult> establishChecksumMode(bool on)
        {
            CommandOptions options;
            options.checksumModeAfterDone = on;
            return submitAndWait(CommandRequest{Command{SECTION_CONFIG, ITEM_CHECKSUM, bytes({on ? TOGGLE_ON : TOGGLE_OFF})},
                                                std::move(options)});
        }

        /// Runs the §00/&07 command that switches the sampler's Still Alive monitor, and with it the
        /// session's reading of `F0 F7`.
        [[nodiscard]] std::optional<CommandResult> establishStillAlive(bool on)
        {
            CommandOptions options;
            options.stillAliveAfterDone = on;
            return submitAndWait(CommandRequest{Command{SECTION_CONFIG, ITEM_STILL_ALIVE, bytes({on ? TOGGLE_ON : TOGGLE_OFF})},
                                                std::move(options)});
        }

        /// Closes the session and waits for it, so that no completion can run after the test's recorders die.
        /// The wait is long enough for a sampler that never answers: the closing gives up after one timeout.
        void closeAndWait() { static_cast<void>(closeForResult()); }

        /// Closes the session, waits for it, and returns how it went; a session already closed by this harness
        /// gives the result it had.
        [[nodiscard]] std::optional<CloseResult> closeForResult(Scheduler::Clock::duration timeout = CLOSE_WAIT)
        {
            if (_closed)
                return _closeResult;
            auto done = std::make_shared<Latched<CloseResult>>();
            if (_session.close([done](const CloseResult& result) { done->set(result); }))
                static_cast<void>(waitUntil([done] { return done->isSet(); }, timeout));
            _closed = true;
            _closeResult = done->value();
            return _closeResult;
        }

        // --- what went on the wire ---

        [[nodiscard]] std::vector<Bytes> sentFrames() const
        {
            std::vector<Bytes> frames;
            for (const common::midi::MidiMessage& message : _backend.sentByHost())
                frames.push_back(message.toBytes());
            return frames;
        }

        [[nodiscard]] std::size_t sentCount() const { return _backend.sentByHost().size(); }

        [[nodiscard]] std::uint8_t userRefOf(std::size_t index) const
        {
            return sentFrames().at(index).at(SENT_USER_REF_INDEX);
        }

        /// Whether the frame at `index` ends with a valid checksum, knowing how many data bytes its command
        /// had: one byte too long for its data, and that byte covering what precedes it.
        [[nodiscard]] bool carriesChecksum(std::size_t index, std::size_t dataLength) const
        {
            const Bytes frame = sentFrames().at(index);
            if (frame.size() != SENT_FRAME_OVERHEAD + dataLength + CHECKSUM_SIZE)
                return false;
            const std::span<const std::uint8_t> covered =
                std::span<const std::uint8_t>(frame).subspan(FIRST_USER_REF_INDEX,
                                                             frame.size() - END_BYTE_SIZE - CHECKSUM_SIZE
                                                                 - FIRST_USER_REF_INDEX);
            return frame.at(frame.size() - END_BYTE_SIZE - CHECKSUM_SIZE) == checksum(covered);
        }

        /// A confirmation for the command of the frame at `index`, as a sampler would build it.
        [[nodiscard]] Bytes confirmationFor(std::size_t index, ReplyId replyId, const Bytes& data = {},
                                            std::uint8_t deviceId = 0, bool withChecksum = false) const
        {
            const Bytes sent = sentFrames().at(index);
            // Written into storage of its final size rather than grown with insert(): GCC 11 at -O2 reads the
            // reallocation path of vector::insert on a short vector as a read past its end, a false positive
            // of -Wstringop-overread that -Werror turns into an error (linux-x64-release-canary, TASK-AKM-006).
            const std::size_t checksumSize = withChecksum ? CHECKSUM_SIZE : 0;
            Bytes frame(CONFIRMATION_DATA_INDEX + data.size() + checksumSize + END_BYTE_SIZE);
            frame[0] = common::midi::SYSEX_START;
            frame[MANUFACTURER_ID_INDEX] = AKAI_MANUFACTURER_ID;
            frame[MODEL_ID_INDEX] = SAMPLER_MODEL_ID;
            frame[DEVICE_BYTE_INDEX] = deviceId;
            frame[FIRST_USER_REF_INDEX] = sent.at(SENT_USER_REF_INDEX);
            frame[CONFIRMATION_REPLY_INDEX] = static_cast<std::uint8_t>(replyId);
            frame[CONFIRMATION_SECTION_INDEX] = sent.at(SENT_SECTION_INDEX);
            frame[CONFIRMATION_ITEM_INDEX] = sent.at(SENT_ITEM_INDEX);
            std::copy(data.begin(), data.end(), frame.begin() + CONFIRMATION_DATA_INDEX);
            if (withChecksum)
            {
                const std::size_t checksumIndex = frame.size() - END_BYTE_SIZE - CHECKSUM_SIZE;
                frame[checksumIndex] = checksum(std::span<const std::uint8_t>(frame).subspan(
                    FIRST_USER_REF_INDEX, checksumIndex - FIRST_USER_REF_INDEX));
            }
            frame.back() = common::midi::SYSEX_END;
            return frame;
        }

        /// Puts a message on the host's input, as a sampler would.
        void inject(Bytes frame) { _backend.injectToHost(std::move(frame)); }

        /// Whether the session's input port is still delivering: `close()` stops it.
        [[nodiscard]] bool inputStarted() const { return _input->isStarted(); }

    private:
        harness::ScenarioDriver& _driver;
        RecordingDiagnostics _diagnostics;
        Recorder _recorder;
        harness::SimulatedMidiBackend _backend;
        harness::SimulatedSampler* _sampler;
        std::unique_ptr<common::midi::MidiInputPort> _input;
        std::unique_ptr<common::midi::MidiOutputPort> _output;
        bool _closed = false;
        std::optional<CloseResult> _closeResult;
        // Last, so that it is destroyed first: it stops the input port it was given.
        Session _session;
    };
}
