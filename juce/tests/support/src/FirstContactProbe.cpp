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
#include "akm/harness/FirstContactProbe.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "akm/Command.hpp"
#include "akm/Confirmation.hpp"
#include "akm/Protocol.hpp"
#include "akm/SamplerError.hpp"

namespace akm::harness
{
    namespace
    {
        using Bytes = std::vector<std::uint8_t>;
        using Clock = Scheduler::Clock;

        // Sections and items of the sequence (spec Tables 5 and 6).
        constexpr std::uint8_t SECTION_SYSEX_CONFIG = 0x00;
        constexpr std::uint8_t SECTION_SYSTEM = 0x02;
        constexpr std::uint8_t ITEM_QUERY = 0x00;
        constexpr std::uint8_t ITEM_CHECKSUM = 0x04;
        constexpr std::uint8_t ITEM_ECHO = 0x06;
        constexpr std::uint8_t ITEM_STILL_ALIVE = 0x07;
        constexpr std::uint8_t ITEM_OS_VERSION = 0x00;
        constexpr std::uint8_t ITEM_OS_SUB_VERSION = 0x01;
        constexpr std::uint8_t SETTING_OFF = 0;
        constexpr std::uint8_t SETTING_ON = 1;
        constexpr std::uint32_t EVERY_SAMPLER = 0;
        const Bytes ECHO_PAYLOAD{0x01, 0x02, 0x03, 0x04};

        // Each step of the sequence gets a user-ref of its own, from this one on.
        constexpr std::uint8_t FIRST_USER_REF = 0x10;

        // The Reply IDs that end a command (Table 2), and the bytes that follow the Reply ID.
        constexpr std::uint8_t REPLY_DONE = 0x44;
        constexpr std::uint8_t REPLY_REPLY = 0x52;
        constexpr std::uint8_t REPLY_ERROR = 0x45;
        constexpr std::size_t FIELDS_AFTER_REPLY_ID = 3;  // Reply ID, section, item
        constexpr std::size_t MIN_OS_VERSION_BYTES = 2;

        enum class StepKind
        {
            Plain,
            OsVersion,
            OsSubVersion,
        };

        struct Step
        {
            std::string title;
            std::uint32_t deviceId = 0;
            Command command;
            ChecksumMode mode = ChecksumMode::Off;
            StepKind kind = StepKind::Plain;
        };

        std::vector<Step> buildSteps(const ProbeOptions& options)
        {
            const std::uint32_t target = options.target.deviceId;
            const std::string other = std::to_string(options.otherDeviceId);
            const Command switchChecksumOff{SECTION_SYSEX_CONFIG, ITEM_CHECKSUM, {SETTING_OFF}};
            const Command echo{SECTION_SYSEX_CONFIG, ITEM_ECHO, ECHO_PAYLOAD};
            const Command query{SECTION_SYSEX_CONFIG, ITEM_QUERY, {}};

            return {
                {"Checksums off on every sampler, the spec's own frame, to start from a known state", EVERY_SAMPLER,
                 switchChecksumOff, ChecksumMode::On},
                {"Query to DeviceID 0: every sampler answers, each with a DeviceID", EVERY_SAMPLER, query, ChecksumMode::Off},
                {"Query to DeviceID " + other + ": only a sampler with that DeviceID, or DeviceID 0, answers",
                 options.otherDeviceId, query, ChecksumMode::Off},
                {"Get operating system version (section 02, item 00)", target,
                 Command{SECTION_SYSTEM, ITEM_OS_VERSION, {}}, ChecksumMode::Off, StepKind::OsVersion},
                {"Get operating system sub-version (section 02, item 01)", target,
                 Command{SECTION_SYSTEM, ITEM_OS_SUB_VERSION, {}}, ChecksumMode::Off, StepKind::OsSubVersion},
                {"Echo 01 02 03 04, checksums off", target, echo, ChecksumMode::Off},
                {"Echo with a checksum the sampler does not expect: ignored while checksums are off", target, echo,
                 ChecksumMode::On},
                {"Checksums on (section 00, item 04), sent without checksum", target,
                 Command{SECTION_SYSEX_CONFIG, ITEM_CHECKSUM, {SETTING_ON}}, ChecksumMode::Off},
                {"Query with checksum, checksums on", target, query, ChecksumMode::On},
                {"Echo with checksum, checksums on", target, echo, ChecksumMode::On},
                {"Query without checksum while checksums are on: expect ERROR 129", target, query, ChecksumMode::Off},
                {"Checksums off on every sampler, sent with a checksum: works whichever the state", EVERY_SAMPLER,
                 switchChecksumOff, ChecksumMode::On},
                {"Query without checksum, checksums off again", target, query, ChecksumMode::Off},
                {"Still Alive on (section 00, item 07)", target,
                 Command{SECTION_SYSEX_CONFIG, ITEM_STILL_ALIVE, {SETTING_ON}}, ChecksumMode::Off},
                {"Still Alive off", target, Command{SECTION_SYSEX_CONFIG, ITEM_STILL_ALIVE, {SETTING_OFF}},
                 ChecksumMode::Off},
            };
        }

        struct Received
        {
            Bytes bytes;
            Clock::time_point at;
            std::string note;  ///< an error reported by the input port instead of a message
        };

        // What the input port received, from whichever thread it calls back on.
        class Inbox
        {
        public:
            void add(Received received)
            {
                const std::lock_guard lock(_mutex);
                _received.push_back(std::move(received));
            }

            [[nodiscard]] std::size_t size() const
            {
                const std::lock_guard lock(_mutex);
                return _received.size();
            }

            [[nodiscard]] std::vector<Received> from(std::size_t index) const
            {
                const std::lock_guard lock(_mutex);
                return std::vector<Received>(_received.begin() + static_cast<std::ptrdiff_t>(index), _received.end());
            }

        private:
            mutable std::mutex _mutex;
            std::vector<Received> _received;
        };

        // The fields of a message that has the shape of a confirmation, whatever its checksum mode.
        struct Shape
        {
            std::uint8_t replyId = 0;
            std::uint8_t deviceId = 0;
            std::uint8_t firstUserRef = 0;
        };

        std::optional<Shape> shapeOf(const Bytes& bytes)
        {
            if (bytes.size() < FIRST_USER_REF_INDEX + USER_REF_COUNT_MIN + FIELDS_AFTER_REPLY_ID + END_BYTE_SIZE
                || bytes.front() != common::midi::SYSEX_START || bytes[MANUFACTURER_ID_INDEX] != AKAI_MANUFACTURER_ID
                || bytes[MODEL_ID_INDEX] != SAMPLER_MODEL_ID)
                return std::nullopt;
            const std::uint8_t deviceByte = bytes[DEVICE_BYTE_INDEX];
            const std::size_t userRefCount = ((deviceByte >> USER_REF_COUNT_SHIFT) & USER_REF_COUNT_MASK) + USER_REF_COUNT_MIN;
            const std::size_t replyIndex = FIRST_USER_REF_INDEX + userRefCount;
            if (bytes.size() < replyIndex + FIELDS_AFTER_REPLY_ID + END_BYTE_SIZE)
                return std::nullopt;
            const std::uint8_t replyId = bytes[replyIndex];
            if (replyId != static_cast<std::uint8_t>(ReplyId::Ok) && replyId != REPLY_DONE && replyId != REPLY_REPLY
                && replyId != REPLY_ERROR)
                return std::nullopt;
            return Shape{replyId, static_cast<std::uint8_t>(deviceByte & DEVICE_ID_MASK), bytes[FIRST_USER_REF_INDEX]};
        }

        bool isStillAlive(const Bytes& bytes)
        {
            return bytes == Bytes{common::midi::SYSEX_START, common::midi::SYSEX_END};
        }

        bool isTerminalFor(const Received& received, std::uint8_t userRef)
        {
            const auto shape = shapeOf(received.bytes);
            return shape && shape->firstUserRef == userRef
                   && (shape->replyId == REPLY_DONE || shape->replyId == REPLY_REPLY || shape->replyId == REPLY_ERROR);
        }

        std::string hex(std::span<const std::uint8_t> bytes)
        {
            std::ostringstream text;
            text << std::hex << std::uppercase << std::setfill('0');
            for (std::size_t index = 0; index < bytes.size(); ++index)
                text << (index == 0 ? "" : " ") << std::setw(2) << static_cast<unsigned int>(bytes[index]);
            return text.str();
        }

        std::string hexOrDash(const Bytes& bytes)
        {
            return bytes.empty() ? "-" : hex(bytes);
        }

        std::string secondsText(Clock::duration elapsed)
        {
            std::ostringstream text;
            text << std::fixed << std::setprecision(3) << std::setw(9) << std::chrono::duration<double>(elapsed).count();
            return text.str();
        }

        long long millisecondsOf(Clock::duration duration)
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        }

        // How a received message reads under one checksum mode.
        std::string reading(const Bytes& bytes, ChecksumMode mode)
        {
            const DecodedMessage decoded = decodeMessage(bytes, mode);
            if (std::holds_alternative<StillAliveMessage>(decoded))
                return "still alive (F0 F7)";
            if (const auto* rejected = std::get_if<Rejected>(&decoded))
                return "rejected: " + std::string(describe(rejected->reason));
            const Confirmation& confirmation = std::get<Confirmation>(decoded);
            std::ostringstream text;
            switch (confirmation.replyId)
            {
                case ReplyId::Ok:
                    text << "OK";
                    break;
                case ReplyId::Done:
                    text << "DONE";
                    break;
                case ReplyId::Reply:
                    text << "REPLY";
                    break;
                case ReplyId::Error:
                {
                    const auto number = errorNumber(confirmation);
                    if (number)
                        text << "ERROR " << *number << " (" << describeError(*number).meaning << ")";
                    else
                        text << "ERROR (no error number)";
                    break;
                }
            }
            text << " dev " << static_cast<unsigned int>(confirmation.deviceId) << " ref " << hex(confirmation.userRefs)
                 << " sec " << hex(std::vector<std::uint8_t>{confirmation.section}) << " item "
                 << hex(std::vector<std::uint8_t>{confirmation.item}) << " data " << hexOrDash(confirmation.data);
            return text.str();
        }

        // The data of the terminal REPLY of a step, read as if checksums were off (they are, at the steps
        // that ask for the OS version).
        std::optional<Bytes> replyData(const std::vector<Received>& received, std::uint8_t userRef)
        {
            for (const Received& item : received)
            {
                if (!isTerminalFor(item, userRef))
                    continue;
                const DecodedMessage decoded = decodeMessage(item.bytes, ChecksumMode::Off);
                const auto* confirmation = std::get_if<Confirmation>(&decoded);
                if (confirmation && confirmation->replyId == ReplyId::Reply)
                    return confirmation->data;
                return std::nullopt;
            }
            return std::nullopt;
        }

        std::string listOfIds(const std::set<std::uint8_t>& ids)
        {
            if (ids.empty())
                return "none";
            std::ostringstream text;
            bool first = true;
            for (const std::uint8_t id : ids)
            {
                text << (first ? "" : " ") << static_cast<unsigned int>(id);
                first = false;
            }
            return text.str();
        }
    }

    ProbeResult runFirstContactProbe(common::midi::MidiBackend& backend, ScenarioDriver& driver,
                                     const ProbeOptions& options, std::ostream& log)
    {
        ProbeResult result;

        log << "# XS56K AKM first-contact probe\n";
        if (!options.startedAt.empty())
            log << "# started " << options.startedAt << "\n";
        log << "# target: in=\"" << options.target.inputPortName << "\" out=\"" << options.target.outputPortName
            << "\" device-id=" << options.target.deviceId << "\n";
        log << "# The probe switches checksums and Still Alive on and off, and ends with both off.\n";

        auto input = backend.openInput(options.target.inputPortName);
        if (!input)
        {
            log << "# error: input port not found: " << options.target.inputPortName << "\n";
            return result;
        }
        auto output = backend.openOutput(options.target.outputPortName);
        if (!output)
        {
            log << "# error: output port not found: " << options.target.outputPortName << "\n";
            return result;
        }
        result.portsOpened = true;

        Inbox inbox;
        common::midi::MidiInputCallbacks callbacks;
        callbacks.onSysExMessage = [&inbox, &driver](const common::midi::MidiMessage& message) {
            inbox.add(Received{message.toBytes(), driver.scheduler().now(), {}});
        };
        callbacks.onError = [&inbox, &driver](const std::string& description) {
            inbox.add(Received{{}, driver.scheduler().now(), description});
        };
        input->setCallbacks(std::move(callbacks));
        input->start();

        const Clock::time_point start = driver.scheduler().now();
        std::optional<int> osMajor;
        std::optional<int> osMinor;
        std::optional<int> osSub;

        const std::vector<Step> steps = buildSteps(options);
        for (std::size_t index = 0; index < steps.size(); ++index)
        {
            const Step& step = steps[index];
            const auto userRef = static_cast<std::uint8_t>(FIRST_USER_REF + index);
            log << "# step " << index + 1 << ": " << step.title << "\n";

            const EncodeResult frame = encodeCommand(step.deviceId, Bytes{userRef}, step.command, step.mode);
            if (!frame.ok())
            {
                log << "# error: the frame cannot be encoded for DeviceID " << step.deviceId << ", step skipped\n";
                continue;
            }

            const std::size_t mark = inbox.size();
            const Clock::time_point sentAt = driver.scheduler().now();
            output->send(common::midi::MidiMessage::sysEx(frame.bytes));
            ++result.framesSent;
            log << secondsText(sentAt - start) << "  OUT  " << hex(frame.bytes) << "\n";

            const auto terminalSeen = [&] {
                const auto received = inbox.from(mark);
                return std::any_of(received.begin(), received.end(),
                                   [userRef](const Received& item) { return isTerminalFor(item, userRef); });
            };
            const bool terminal = driver.waitUntil(terminalSeen, options.answerTimeout);
            if (terminal)
                driver.elapse(options.settleTime);

            const std::vector<Received> received = inbox.from(mark);
            std::optional<Clock::time_point> firstAnswer;
            std::optional<Clock::time_point> terminalAt;
            std::string terminalKind;
            for (const Received& item : received)
            {
                if (!item.note.empty())
                {
                    log << "# input error: " << item.note << "\n";
                    continue;
                }
                ++result.framesReceived;
                log << secondsText(item.at - start) << "  IN   " << hex(item.bytes) << " | off: "
                    << reading(item.bytes, ChecksumMode::Off) << " | on: " << reading(item.bytes, ChecksumMode::On) << "\n";
                if (isStillAlive(item.bytes))
                {
                    ++result.stillAliveMessagesSeen;
                    continue;
                }
                const auto shape = shapeOf(item.bytes);
                if (!shape)
                    continue;
                result.anySamplerAnswered = true;
                result.confirmationDeviceIds.insert(shape->deviceId);
                if (shape->firstUserRef != userRef)
                    continue;
                if (!firstAnswer)
                    firstAnswer = item.at;
                if (!terminalAt && isTerminalFor(item, userRef))
                {
                    terminalAt = item.at;
                    terminalKind = shape->replyId == REPLY_DONE ? "DONE" : (shape->replyId == REPLY_REPLY ? "REPLY" : "ERROR");
                }
            }

            if (!firstAnswer)
                log << "#   no answer within " << millisecondsOf(options.answerTimeout) << " ms\n";
            else
            {
                log << "#   answered after " << millisecondsOf(*firstAnswer - sentAt) << " ms";
                if (terminalAt)
                {
                    log << ", " << terminalKind << " after " << millisecondsOf(*terminalAt - sentAt) << " ms\n";
                    result.maxLatency = std::max(result.maxLatency, *terminalAt - sentAt);
                }
                else
                    log << ", but no DONE, REPLY or ERROR within " << millisecondsOf(options.answerTimeout) << " ms\n";
            }

            if (step.kind != StepKind::Plain)
            {
                const auto data = replyData(received, userRef);
                if (step.kind == StepKind::OsVersion && data && data->size() >= MIN_OS_VERSION_BYTES)
                {
                    osMajor = (*data)[0];
                    osMinor = (*data)[1];
                }
                if (step.kind == StepKind::OsSubVersion && data && !data->empty())
                    osSub = (*data)[0];
            }
        }

        // No callback may run once the inbox is gone.
        input->stop();

        if (osMajor && osMinor)
        {
            result.osVersion = SamplerOsVersion{*osMajor, *osMinor, osSub};
        }

        log << "# observations\n";
        if (result.osVersion)
        {
            log << "# observation: OS version " << result.osVersion->major << "." << result.osVersion->minor << " (";
            if (result.osVersion->subVersion)
                log << "sub-version " << *result.osVersion->subVersion;
            else
                log << "sub-version unavailable";
            log << ")\n";
        }
        else
            log << "# observation: OS version not obtained\n";
        log << "# observation: DeviceIDs carried by confirmations: " << listOfIds(result.confirmationDeviceIds) << "\n";
        log << "# observation: F0 F7 messages seen: " << result.stillAliveMessagesSeen << "\n";
        log << "# observation: longest DONE, REPLY or ERROR latency: " << millisecondsOf(result.maxLatency) << " ms\n";
        log << "# observation: frames sent " << result.framesSent << ", received " << result.framesReceived << "\n";
        log << (result.anySamplerAnswered ? "# observation: a sampler answered\n" : "# observation: no sampler answered\n");
        return result;
    }
}
