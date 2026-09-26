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
#include "akm/SysExConfig.hpp"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "akm/ItemRequest.hpp"

namespace akm
{
    namespace
    {
        constexpr std::int64_t TOGGLE_OFF = 0;
        constexpr std::int64_t TOGGLE_ON = 1;
        constexpr std::span<const std::int64_t> NO_VALUES{};

        void submitToggle(Session& session, ItemId id, bool on, CommandOptions options, CommandCompletion completion)
        {
            session.submit(makeRequest(id, {on ? TOGGLE_ON : TOGGLE_OFF}, std::move(options)), std::move(completion));
        }
    }

    void discover(Session& session, DiscoveryCompletion completion, Scheduler::Clock::duration window)
    {
        // Shared by the observer and the completion, both on the session's thread.
        const auto answered = std::make_shared<std::set<std::uint8_t>>();
        CommandOptions options;
        options.addressing = Addressing::Broadcast;
        options.collectionWindow = window;
        options.onConfirmation = [answered](const Confirmation& confirmation) { answered->insert(confirmation.deviceId); };

        session.submit(makeRequest(ItemId::SysExQuery, NO_VALUES, std::move(options)),
                       [answered, completion = std::move(completion)](const CommandResult& outcome) {
                           if (completion)
                               completion(DiscoveryResult{std::vector<std::uint8_t>(answered->begin(), answered->end()),
                                                          outcome});
                       });
    }

    void setChecksumMode(Session& session, bool on, CommandCompletion completion)
    {
        CommandOptions options;
        options.checksumModeAfterDone = on;
        submitToggle(session, ItemId::SysExChecksum, on, std::move(options), std::move(completion));
    }

    void setNotification(Session& session, bool on, CommandCompletion completion)
    {
        submitToggle(session, ItemId::SysExNotification, on, {}, std::move(completion));
    }

    void setSyncLcd(Session& session, bool on, CommandCompletion completion)
    {
        submitToggle(session, ItemId::SysExSyncLcd, on, {}, std::move(completion));
    }

    void setAutoScreenUpdate(Session& session, bool on, CommandCompletion completion)
    {
        submitToggle(session, ItemId::SysExAutoScreenUpdate, on, {}, std::move(completion));
    }

    void setStillAlive(Session& session, bool on, CommandCompletion completion)
    {
        CommandOptions options;
        options.stillAliveAfterDone = on;
        submitToggle(session, ItemId::SysExStillAlive, on, std::move(options), std::move(completion));
    }

    void echo(Session& session, const std::array<std::uint8_t, ECHO_DATA_SIZE>& data, EchoCompletion completion)
    {
        std::array<std::int64_t, ECHO_DATA_SIZE> values{};
        std::copy(data.begin(), data.end(), values.begin());

        session.submit(makeRequest(ItemId::SysExEcho, values),
                       [sent = std::vector<std::uint8_t>(data.begin(), data.end()),
                        completion = std::move(completion)](const CommandResult& outcome) {
                           EchoResult result{outcome, std::nullopt};
                           // A REPLY that is not exactly what was sent: too few or too many bytes count as well.
                           if (const auto* reply = std::get_if<Reply>(&outcome); reply != nullptr && reply->data != sent)
                               result.mismatch = EchoMismatch{sent, reply->data};
                           if (completion)
                               completion(result);
                       });
    }
}
