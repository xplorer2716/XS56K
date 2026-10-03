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
#include "akm/SystemSetup.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "akm/ItemRequest.hpp"

namespace akm
{
    namespace
    {
        constexpr std::span<const std::int64_t> NO_VALUES{};
        // Table 7's values: &04's model byte and the 0-100 % range of &30 and &31.
        constexpr std::int64_t MODEL_S5000 = 0;
        constexpr std::int64_t MODEL_S6000 = 1;
        constexpr std::int64_t PERCENT_MAX = 100;

        // The decoded values of an item's REPLY, or nothing when the command did not complete on a REPLY
        // of the shape the catalogue gives it.
        std::optional<std::vector<std::int64_t>> replyValues(ItemId id, const CommandResult& outcome)
        {
            const auto* reply = std::get_if<Reply>(&outcome);
            if (reply == nullptr)
                return std::nullopt;
            return decodeReply(id, reply->data);
        }

        // Every Get of this file has no argument and a REPLY of one value: asks and hands `decode` that value.
        template <typename Result, typename Decode>
        void getSingleValue(Session& session, ItemId id, std::function<void(const Result&)> completion,
                            Decode decode)
        {
            session.submit(makeRequest(id, NO_VALUES),
                           [id, completion = std::move(completion), decode](const CommandResult& outcome) {
                               Result result{};
                               result.outcome = outcome;
                               if (const auto values = replyValues(id, outcome); values && values->size() == 1)
                                   decode(result, values->front());
                               if (completion)
                                   completion(result);
                           });
        }

        // The clock items' arguments, in the order of `ClockDate`'s fields and of `ClockField`.
        constexpr std::size_t CLOCK_FIELD_COUNT = 7;

        std::array<std::int64_t, CLOCK_FIELD_COUNT> clockValues(const ClockDate& clock)
        {
            return {clock.year, clock.month, clock.day, clock.dayOfWeek, clock.hours, clock.minutes, clock.seconds};
        }

        // The Play Mode and lock bytes of Table 6 (&10, &11, &20, &21): the range of each is the catalogue's.
        std::optional<PlayMode> playModeOf(std::int64_t value)
        {
            if (value < static_cast<std::int64_t>(PlayMode::Multi) || value > static_cast<std::int64_t>(PlayMode::Muted))
                return std::nullopt;
            return static_cast<PlayMode>(value);
        }

        std::optional<FrontPanelLock> lockOf(std::int64_t value)
        {
            if (value != static_cast<std::int64_t>(FrontPanelLock::Normal)
                && value != static_cast<std::int64_t>(FrontPanelLock::Locked))
                return std::nullopt;
            return static_cast<FrontPanelLock>(value);
        }

        void decodePercent(MemoryPercentResult& result, std::int64_t value)
        {
            if (value <= PERCENT_MAX)
                result.percent = static_cast<int>(value);
        }

        void decodeBytes(MemoryBytesResult& result, std::int64_t value)
        {
            result.bytes = static_cast<std::uint32_t>(value);
        }
    }

    void setSamplerName(Session& session, std::string_view name, CommandCompletion completion)
    {
        session.submit(makeStringRequest(ItemId::SystemSetName, name), std::move(completion));
    }

    void getSamplerName(Session& session, SamplerNameCompletion completion)
    {
        CommandOptions options;
        options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
        session.submit(makeRequest(ItemId::SystemGetName, NO_VALUES, std::move(options)),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           SamplerNameResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                               result.name = decodeStringReply(ItemId::SystemGetName, rep->data);
                           if (completion)
                               completion(result);
                       });
    }

    void getSamplerModel(Session& session, SamplerModelCompletion completion)
    {
        getSingleValue<SamplerModelResult>(session, ItemId::SystemGetModel, std::move(completion),
                                           [](SamplerModelResult& result, std::int64_t value) {
                                               if (value == MODEL_S5000)
                                                   result.model = SamplerModel::S5000;
                                               else if (value == MODEL_S6000)
                                                   result.model = SamplerModel::S6000;
                                           });
    }

    void getFreeWaveMemoryPercent(Session& session, MemoryPercentCompletion completion)
    {
        getSingleValue<MemoryPercentResult>(session, ItemId::SystemGetWaveMemoryPercent, std::move(completion),
                                            decodePercent);
    }

    void getFreeMpksMemoryPercent(Session& session, MemoryPercentCompletion completion)
    {
        getSingleValue<MemoryPercentResult>(session, ItemId::SystemGetMpksMemoryPercent, std::move(completion),
                                            decodePercent);
    }

    void getTotalWaveMemoryBytes(Session& session, MemoryBytesCompletion completion)
    {
        getSingleValue<MemoryBytesResult>(session, ItemId::SystemGetWaveMemoryTotal, std::move(completion),
                                          decodeBytes);
    }

    void getFreeWaveMemoryBytes(Session& session, MemoryBytesCompletion completion)
    {
        getSingleValue<MemoryBytesResult>(session, ItemId::SystemGetWaveMemoryFree, std::move(completion),
                                          decodeBytes);
    }

    std::optional<ClockField> invalidClockField(const ClockDate& clock)
    {
        const auto& args = descriptor(ItemId::SystemSetClock).args;
        const auto values = clockValues(clock);
        for (std::size_t index = 0; index < CLOCK_FIELD_COUNT; ++index)
        {
            if (values[index] < args[index].min || values[index] > args[index].max)
                return static_cast<ClockField>(index);
        }
        return std::nullopt;
    }

    std::string_view clockFieldName(ClockField field)
    {
        return descriptor(ItemId::SystemSetClock).args[static_cast<std::size_t>(field)].name;
    }

    void setClockDate(Session& session, const ClockDate& clock, CommandCompletion completion)
    {
        const auto values = clockValues(clock);
        session.submit(makeRequest(ItemId::SystemSetClock, values), std::move(completion));
    }

    void getClockDate(Session& session, ClockDateCompletion completion)
    {
        session.submit(makeRequest(ItemId::SystemGetClock, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           ClockDateResult result{std::nullopt, outcome};
                           if (const auto values = replyValues(ItemId::SystemGetClock, outcome);
                               values && values->size() == CLOCK_FIELD_COUNT)
                           {
                               const auto& v = *values;
                               result.clock = ClockDate{static_cast<int>(v[0]), static_cast<int>(v[1]),
                                                        static_cast<int>(v[2]), static_cast<int>(v[3]),
                                                        static_cast<int>(v[4]), static_cast<int>(v[5]),
                                                        static_cast<int>(v[6])};
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void setPlayMode(Session& session, PlayMode mode, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::SystemSetPlayMode, {static_cast<std::int64_t>(mode)}), std::move(completion));
    }

    void getPlayMode(Session& session, PlayModeCompletion completion)
    {
        getSingleValue<PlayModeResult>(session, ItemId::SystemGetPlayMode, std::move(completion),
                                       [](PlayModeResult& result, std::int64_t value) { result.mode = playModeOf(value); });
    }

    void setFrontPanelLock(Session& session, FrontPanelLock lock, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::SystemSetFrontPanelLock, {static_cast<std::int64_t>(lock)}),
                       std::move(completion));
    }

    void getFrontPanelLock(Session& session, FrontPanelLockCompletion completion)
    {
        getSingleValue<FrontPanelLockResult>(session, ItemId::SystemGetFrontPanelLock, std::move(completion),
                                             [](FrontPanelLockResult& result, std::int64_t value) {
                                                 result.lock = lockOf(value);
                                             });
    }

    void clearSamplerMemory(Session& session, std::optional<ConfirmClearSamplerMemory> confirmation,
                            CommandCompletion completion)
    {
        if (!confirmation)
        {
            const ItemDescriptor& item = descriptor(ItemId::SystemClearMemory);
            CommandRequest request;
            request.command.section = item.section;
            request.command.item = item.item;
            request.refusal = RefusalReason::NotConfirmed;
            session.submit(std::move(request), std::move(completion));
            return;
        }
        session.submit(makeRequest(ItemId::SystemClearMemory, NO_VALUES), std::move(completion));
    }
}
