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

#include <cstdint>
#include <span>
#include <utility>
#include <variant>

#include "akm/ItemRequest.hpp"

namespace akm
{
    namespace
    {
        constexpr std::span<const std::int64_t> NO_VALUES{};
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
}
