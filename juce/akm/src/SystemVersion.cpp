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
#include "akm/SystemVersion.hpp"

#include <span>
#include <utility>
#include <variant>

#include "akm/ItemRequest.hpp"

namespace akm
{
    namespace
    {
        constexpr std::span<const std::int64_t> NO_VALUES{};
        // Position of each number in the values of the two REPLYs (Table 7).
        constexpr std::size_t MAJOR_INDEX = 0;
        constexpr std::size_t MINOR_INDEX = 1;
        constexpr std::size_t SUB_VERSION_INDEX = 0;

        std::optional<std::vector<std::int64_t>> readNumbers(ItemId id, const CommandResult& result)
        {
            const auto* reply = std::get_if<Reply>(&result);
            if (reply == nullptr)
                return std::nullopt;
            return decodeReply(id, reply->data);
        }
    }

    void queryOsVersion(Session& session, OsVersionCompletion completion)
    {
        session.submit(
            makeRequest(ItemId::SystemOsVersion, NO_VALUES),
            [&session, completion = std::move(completion)](const CommandResult& first) {
                const auto numbers = readNumbers(ItemId::SystemOsVersion, first);
                if (!numbers)
                {
                    // Nothing is assumed about the version, and the sub-version is not asked for.
                    if (completion)
                        completion(OsVersionResult{std::nullopt, first});
                    return;
                }

                OsVersionReport report;
                report.major = static_cast<std::uint8_t>((*numbers)[MAJOR_INDEX]);
                report.minor = static_cast<std::uint8_t>((*numbers)[MINOR_INDEX]);
                session.submit(makeRequest(ItemId::SystemOsSubVersion, NO_VALUES),
                               [report, first, completion](const CommandResult& second) mutable {
                                   if (const auto sub = readNumbers(ItemId::SystemOsSubVersion, second))
                                       report.subVersion = static_cast<std::uint8_t>((*sub)[SUB_VERSION_INDEX]);
                                   if (completion)
                                       completion(OsVersionResult{report, first});
                               });
            });
    }
}
