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

// Private to the test-support library: what the scenarios that drive a real Session against a sampler share —
// a value a completion produces on the session's thread and the scenario reads on its own, the wait for such a
// value with its timing, the text of a result, and the diagnostic sink that logs what the session reports and
// counts it. Used by the session smoke test and by the real-sampler suite. [TASK-AKM-010, TASK-AKM-013,
// RQ-AKM-017, ADR-AKM-001 (DEC-AKM-008)]
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "akm/Checksum.hpp"
#include "akm/CommandResult.hpp"
#include "akm/DiagnosticSink.hpp"
#include "akm/SamplerError.hpp"
#include "akm/Scheduler.hpp"
#include "akm/SysExConfig.hpp"
#include "akm/harness/ScenarioDriver.hpp"
#include "akm/harness/WireFormat.hpp"
#include "akm/harness/WireLog.hpp"

namespace akm::harness::detail
{
    using Clock = Scheduler::Clock;

    /// The bytes of the first Echo; the timed ones change the first byte, so that a late REPLY cannot pass for the
    /// answer to the next.
    inline const std::array<std::uint8_t, ECHO_DATA_SIZE> ECHO_PAYLOAD{{0x01, 0x23, 0x45, 0x67}};
    inline constexpr std::size_t ECHO_VARYING_INDEX = 0;
    inline constexpr int ECHO_VARYING_MASK = 0x7F;

    /// The payload of the Echo number `index` of a run.
    inline std::array<std::uint8_t, ECHO_DATA_SIZE> echoPayload(int index)
    {
        std::array<std::uint8_t, ECHO_DATA_SIZE> payload = ECHO_PAYLOAD;
        payload[ECHO_VARYING_INDEX] = static_cast<std::uint8_t>(index & ECHO_VARYING_MASK);
        return payload;
    }

    inline std::string modeName(ChecksumMode mode)
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

    inline std::string millisecondsText(Clock::duration duration)
    {
        return std::to_string(millisecondsOf(duration)) + " ms";
    }

    inline std::string listOfIds(const std::vector<std::uint8_t>& ids)
    {
        if (ids.empty())
            return "none";
        std::ostringstream text;
        for (std::size_t index = 0; index < ids.size(); ++index)
            text << (index == 0 ? "" : " ") << static_cast<unsigned int>(ids[index]);
        return text.str();
    }

    inline std::string outcomeText(const CommandResult& result)
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

    /// A value a completion produces on the session's thread and the scenario reads on its own.
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

    /// A result and the time it took from the submit to the completion.
    template <typename Result>
    struct Timed
    {
        Result result;
        Clock::duration latency;
    };

    /// Runs `launch` with a completion that fills a slot, waits for it for at most `patience` and times it from
    /// the submit to the completion, as the scenario's clock reads them. Nothing when the wait ran out: the
    /// session lost a completion, which it must never do.
    template <typename Result, typename Launch>
    std::optional<Timed<Result>> awaitCompletion(ScenarioDriver& driver, Clock::duration patience, Launch&& launch)
    {
        const auto slot = std::make_shared<Slot<Result>>();
        Scheduler& clock = driver.scheduler();
        const Clock::time_point submitted = clock.now();
        launch(std::function<void(const Result&)>([slot, &clock](const Result& result) { slot->set(result, clock.now()); }));
        if (!driver.waitUntil([slot] { return slot->isSet(); }, patience))
            return std::nullopt;
        return Timed<Result>{*slot->value(), slot->at() - submitted};
    }

    /// The minimum, the median, the 95th percentile and the maximum of a set of durations (at least one).
    struct LatencyStats
    {
        Clock::duration min{};
        Clock::duration median{};
        Clock::duration p95{};
        Clock::duration max{};
    };

    inline LatencyStats latencyStats(std::vector<Clock::duration> durations)
    {
        std::sort(durations.begin(), durations.end());
        constexpr std::size_t PERCENTILE = 95;
        constexpr std::size_t HUNDRED = 100;
        const std::size_t p95Index = std::min(durations.size() - 1, (durations.size() * PERCENTILE + HUNDRED - 1) / HUNDRED - 1);
        return LatencyStats{durations.front(), durations[durations.size() / 2], durations[p95Index], durations.back()};
    }

    inline std::string latencyText(const LatencyStats& stats)
    {
        return "min " + millisecondsText(stats.min) + ", median " + millisecondsText(stats.median) + ", 95th percentile "
               + millisecondsText(stats.p95) + ", max " + millisecondsText(stats.max);
    }

    /// Logs what the session reports besides results, and counts it.
    class ScenarioDiagnostics final : public DiagnosticSink
    {
    public:
        explicit ScenarioDiagnostics(WireLog& log) : _log(log) {}

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

        /// The modes the session went through, from `unknown`, as "unknown -> off -> on".
        [[nodiscard]] std::string modeChanges() const
        {
            const std::lock_guard lock(_mutex);
            std::string chain = "unknown";
            for (const std::string& mode : _modeChanges)
                chain += " -> " + mode;
            return chain;
        }

        /// How many changes of the checksum mode were reported so far.
        [[nodiscard]] std::size_t modeChangeCount() const
        {
            const std::lock_guard lock(_mutex);
            return _modeChanges.size();
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
}
