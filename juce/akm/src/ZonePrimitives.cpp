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
#include "akm/ZonePrimitives.hpp"

#include <cstdint>
#include <utility>

#include "akm/ByteWriter.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/SamplerError.hpp"

namespace akm
{
    void setZoneSample(Session& session, int zone, std::string_view name, CommandCompletion completion)
    {
        const ItemDescriptor& item = descriptor(ItemId::ZoneSetSample);
        const ValueSpec& zoneSpec = item.args[0];
        const ValueSpec& nameSpec = item.args[1];

        CommandRequest request;
        request.command.section = item.section;
        request.command.item = item.item;

        const auto zoneValue = static_cast<std::int64_t>(zone);
        const auto length = static_cast<std::int64_t>(name.size());
        if (zoneValue < zoneSpec.min || zoneValue > zoneSpec.max || length < nameSpec.min || length > nameSpec.max)
        {
            request.refusal = RefusalReason::ArgumentOutOfRange;
            session.submit(std::move(request), std::move(completion));
            return;
        }

        ByteWriter writer;
        writer.appendByte(static_cast<std::uint32_t>(zone));
        if (!writer.appendString(name))
        {
            request.refusal = RefusalReason::NotEncodable;
            session.submit(std::move(request), std::move(completion));
            return;
        }
        request.command.data = writer.bytes();
        session.submit(std::move(request), std::move(completion));
    }

    void getZoneSample(Session& session, int zone, ZoneSampleCompletion completion)
    {
        CommandOptions options;
        options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
        session.submit(makeRequest(ItemId::ZoneGetSample, {static_cast<std::int64_t>(zone)}, std::move(options)),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           ZoneSampleResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                               result.name = decodeStringReply(ItemId::ZoneGetSample, rep->data);
                           if (completion)
                               completion(result);
                       });
    }
}
