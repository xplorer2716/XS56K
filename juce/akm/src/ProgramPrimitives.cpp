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
#include "akm/ProgramPrimitives.hpp"

#include <cstdint>
#include <span>
#include <utility>

#include "akm/ByteWriter.hpp"
#include "akm/ItemRequest.hpp"

namespace akm
{
    namespace
    {
        constexpr std::span<const std::int64_t> NO_VALUES{};
        // A zero-based index or a count is split into two 7-bit data bytes, most significant first
        // (spec pp. 8-9's compound word, read back byte by byte since the catalogue lists them as two
        // separate Byte values, not a Word, matching the spec's own Data1/Data2 columns).
        constexpr std::int64_t DATA_BYTE_BASE = 128;

        void submitProgramRequest(Session& session, CommandRequest request, CommandCompletion completion)
        {
            session.submit(std::move(request), std::move(completion));
        }
    }

    void createProgram(Session& session, std::string_view name, CommandCompletion completion)
    {
        submitProgramRequest(session, makeStringRequest(ItemId::ProgramCreate, name), std::move(completion));
    }

    void createProgramWithKeygroups(Session& session, int keygroupCount, std::string_view name,
                                    CommandCompletion completion)
    {
        const ItemDescriptor& item = descriptor(ItemId::ProgramCreateWithKeygroups);
        const ValueSpec& countSpec = item.args[0];
        const ValueSpec& nameSpec = item.args[1];

        CommandRequest request;
        request.command.section = item.section;
        request.command.item = item.item;

        const auto count = static_cast<std::int64_t>(keygroupCount);
        const auto length = static_cast<std::int64_t>(name.size());
        if (count < countSpec.min || count > countSpec.max || length < nameSpec.min || length > nameSpec.max)
        {
            request.refusal = RefusalReason::ArgumentOutOfRange;
            submitProgramRequest(session, std::move(request), std::move(completion));
            return;
        }

        ByteWriter writer;
        writer.appendByte(static_cast<std::uint32_t>(keygroupCount));
        if (!writer.appendString(name))
        {
            request.refusal = RefusalReason::NotEncodable;
            submitProgramRequest(session, std::move(request), std::move(completion));
            return;
        }
        request.command.data = writer.bytes();
        submitProgramRequest(session, std::move(request), std::move(completion));
    }

    void selectProgramByName(Session& session, std::string_view name, CommandCompletion completion)
    {
        submitProgramRequest(session, makeStringRequest(ItemId::ProgramSelectByName, name), std::move(completion));
    }

    void selectProgramByIndex(Session& session, int index, CommandCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(index) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(index) % DATA_BYTE_BASE;
        submitProgramRequest(session, makeRequest(ItemId::ProgramSelectByIndex, {msb, lsb}), std::move(completion));
    }

    void deleteCurrentProgram(Session& session, CommandCompletion completion)
    {
        submitProgramRequest(session, makeRequest(ItemId::ProgramDeleteCurrent, NO_VALUES), std::move(completion));
    }

    void renameCurrentProgram(Session& session, std::string_view name, CommandCompletion completion)
    {
        submitProgramRequest(session, makeStringRequest(ItemId::ProgramRenameCurrent, name), std::move(completion));
    }

    void getProgramCount(Session& session, ProgramCountCompletion completion)
    {
        session.submit(makeRequest(ItemId::ProgramGetCount, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           ProgramCountResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::ProgramGetCount, rep->data);
                               if (values && values->size() == 2)
                                   result.count = static_cast<int>((*values)[0] * DATA_BYTE_BASE + (*values)[1]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getCurrentProgramName(Session& session, ProgramNameCompletion completion)
    {
        CommandOptions options;
        options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
        session.submit(makeRequest(ItemId::ProgramGetCurrentName, NO_VALUES, std::move(options)),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           ProgramNameResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                               result.name = decodeStringReply(ItemId::ProgramGetCurrentName, rep->data);
                           if (completion)
                               completion(result);
                       });
    }
}
