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
#include <vector>

#include "akm/ByteReader.hpp"
#include "akm/ByteWriter.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/SamplerError.hpp"

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

        // §0A's "all Programs in memory" replies (&18, &19) answer ERROR 4 (not found) instead of an
        // empty REPLY when there are none, unlike &10 (Get Number of Programs), which answers a normal
        // REPLY of 0 (observed on a real S5000, documents/_index/sysex_spec.kb.md, "Common value codes").
        // getAllProgramNumbers/getAllProgramNames treat it as an empty list, not a failure, so both
        // report "how many" the same way whether the sampler says so with data or with this error.
        bool answersEmptyMemory(const CommandResult& outcome)
        {
            const auto* error = std::get_if<Error>(&outcome);
            return error != nullptr && error->number == error_number::NOT_FOUND;
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

    void setProgramNumber(Session& session, std::optional<int> frontPanelNumber, CommandCompletion completion)
    {
        // The front-panel range (spec Table 13, footnote a); &0A's Data1/Data2 shape is conditional on
        // Data1, so it is written by hand rather than through makeRequest.
        constexpr int FRONT_PANEL_MIN = 1;
        constexpr int FRONT_PANEL_MAX = 128;
        const ItemDescriptor& item = descriptor(ItemId::ProgramSetNumber);

        CommandRequest request;
        request.command.section = item.section;
        request.command.item = item.item;

        ByteWriter writer;
        if (!frontPanelNumber.has_value())
        {
            writer.appendByte(0);
        }
        else if (*frontPanelNumber < FRONT_PANEL_MIN || *frontPanelNumber > FRONT_PANEL_MAX)
        {
            request.refusal = RefusalReason::ArgumentOutOfRange;
            submitProgramRequest(session, std::move(request), std::move(completion));
            return;
        }
        else
        {
            writer.appendByte(1);
            writer.appendByte(static_cast<std::uint32_t>(*frontPanelNumber - 1));
        }
        request.command.data = writer.bytes();
        submitProgramRequest(session, std::move(request), std::move(completion));
    }

    void getProgramNumber(Session& session, ProgramNumberCompletion completion)
    {
        session.submit(makeRequest(ItemId::ProgramGetNumber, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           ProgramNumberResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::ProgramGetNumber, rep->data);
                               if (values && values->size() == 2 && (*values)[0] != 0)
                                   result.frontPanelNumber = static_cast<int>((*values)[1] + 1);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void addKeygroupsToProgram(Session& session, int count, CommandCompletion completion)
    {
        submitProgramRequest(session, makeRequest(ItemId::ProgramAddKeygroups, {static_cast<std::int64_t>(count)}),
                             std::move(completion));
    }

    void deleteKeygroupFromProgram(Session& session, int keygroup, CommandCompletion completion)
    {
        submitProgramRequest(session, makeRequest(ItemId::ProgramDeleteKeygroup, {static_cast<std::int64_t>(keygroup)}),
                             std::move(completion));
    }

    void setKeygroupCrossfade(Session& session, bool on, CommandCompletion completion)
    {
        submitProgramRequest(session, makeRequest(ItemId::ProgramSetCrossfade, {on ? 1 : 0}), std::move(completion));
    }

    void getProgramKeygroupCount(Session& session, ProgramKeygroupCountCompletion completion)
    {
        session.submit(makeRequest(ItemId::ProgramGetKeygroupCount, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           ProgramKeygroupCountResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::ProgramGetKeygroupCount, rep->data);
                               if (values && values->size() == 1)
                                   result.count = static_cast<int>((*values)[0]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getKeygroupCrossfade(Session& session, ProgramCrossfadeCompletion completion)
    {
        session.submit(makeRequest(ItemId::ProgramGetCrossfade, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           ProgramCrossfadeResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::ProgramGetCrossfade, rep->data);
                               if (values && values->size() == 1)
                                   result.enabled = (*values)[0] != 0;
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getProgramIndex(Session& session, ProgramIndexCompletion completion)
    {
        session.submit(makeRequest(ItemId::ProgramGetIndex, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           ProgramIndexResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::ProgramGetIndex, rep->data);
                               if (values && values->size() == 2)
                                   result.index = static_cast<int>((*values)[0] * DATA_BYTE_BASE + (*values)[1]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    namespace
    {
        // One (enabled, wire number) pair per program; nothing at all when a pair is cut short.
        std::optional<std::vector<std::optional<int>>> decodeProgramNumbers(std::span<const std::uint8_t> data)
        {
            ByteReader reader(data);
            std::vector<std::optional<int>> numbers;
            while (reader.remaining() > 0)
            {
                const auto enabled = reader.readByte();
                const auto number = reader.readByte();
                if (!enabled.has_value() || !number.has_value())
                    return std::nullopt;
                // Table 14, footnote a: the wire number is the front-panel one minus one.
                numbers.push_back(*enabled != 0 ? std::optional<int>(*number + 1) : std::nullopt);
            }
            return numbers;
        }
    }

    void getAllProgramNumbers(Session& session, AllProgramNumbersCompletion completion)
    {
        CommandOptions options;
        options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
        session.submit(makeRequest(ItemId::ProgramGetAllNumbers, NO_VALUES, std::move(options)),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           AllProgramNumbersResult result{std::nullopt, outcome};
                           if (answersEmptyMemory(outcome))
                               result.numbers = std::vector<std::optional<int>>{};
                           else if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                               result.numbers = decodeProgramNumbers(rep->data);
                           if (completion)
                               completion(result);
                       });
    }

    void getAllProgramNames(Session& session, AllProgramNamesCompletion completion)
    {
        CommandOptions options;
        options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
        session.submit(makeRequest(ItemId::ProgramGetAllNames, NO_VALUES, std::move(options)),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           AllProgramNamesResult result{std::nullopt, outcome};
                           if (answersEmptyMemory(outcome))
                               result.names = std::vector<std::string>{};
                           else if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               ByteReader reader(rep->data);
                               result.names = reader.readStringList();
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void deleteAllPrograms(Session& session, std::optional<ConfirmDeleteAllPrograms> confirmation,
                           CommandCompletion completion)
    {
        if (!confirmation)
        {
            const ItemDescriptor& item = descriptor(ItemId::ProgramDeleteAll);
            CommandRequest request;
            request.command.section = item.section;
            request.command.item = item.item;
            request.refusal = RefusalReason::NotConfirmed;
            submitProgramRequest(session, std::move(request), std::move(completion));
            return;
        }
        submitProgramRequest(session, makeRequest(ItemId::ProgramDeleteAll, NO_VALUES), std::move(completion));
    }
}
