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
#include "akm/MultiPrimitives.hpp"

#include <cstdint>
#include <span>
#include <utility>
#include <variant>

#include "akm/ByteReader.hpp"
#include "akm/ByteWriter.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/SamplerError.hpp"

namespace akm
{
    namespace
    {
        constexpr std::span<const std::int64_t> NO_VALUES{};
        // A zero-based index is split into two 7-bit data bytes, most significant first (spec pp. 8-9's compound
        // word), the same convention SamplePrimitives.cpp and SongPrimitives.cpp use.
        constexpr std::int64_t DATA_BYTE_BASE = 128;
        // The wire carries a part count as the count minus one (31, 63, 127), and a program number as the
        // front-panel number minus one (spec Table 17, notes a).
        constexpr int WIRE_OFFSET = 1;

        CommandOptions variableLengthReply()
        {
            CommandOptions options;
            options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
            return options;
        }

        // The "all the multis" Gets answer ERROR 4 (not found) rather than an empty REPLY when there is none,
        // as §0A's "all programs" Gets do (observed on the real S5000 for programs): an empty list, not a failure.
        bool answersEmptyMemory(const CommandResult& outcome)
        {
            const auto* error = std::get_if<Error>(&outcome);
            return error != nullptr && error->number == error_number::NOT_FOUND;
        }

        // A flag then a number, as `&41` and each record of `&50` carry: the front-panel number, or nothing
        // when the flag says the number is off.
        std::optional<int> frontPanelNumberOf(const std::vector<std::int64_t>& record)
        {
            return record[0] != 0 ? std::optional<int>{static_cast<int>(record[1]) + WIRE_OFFSET} : std::nullopt;
        }
    }

    void setNewMultiPartCount(Session& session, MultiPartCount partCount, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::MultiSetPartCount, {static_cast<std::int64_t>(partCount)}), std::move(completion));
    }

    void createMulti(Session& session, std::string_view name, CommandCompletion completion)
    {
        session.submit(makeStringRequest(ItemId::MultiCreate, name), std::move(completion));
    }

    void selectMultiByName(Session& session, std::string_view name, CommandCompletion completion)
    {
        session.submit(makeStringRequest(ItemId::MultiSelectByName, name), std::move(completion));
    }

    void selectMultiByIndex(Session& session, int index, CommandCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(index) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(index) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::MultiSelectByIndex, {msb, lsb}), std::move(completion));
    }

    void deleteCurrentMulti(Session& session, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::MultiDeleteCurrent, NO_VALUES), std::move(completion));
    }

    void deleteAllMultis(Session& session, std::optional<ConfirmDeleteAllMultis> confirmation, CommandCompletion completion)
    {
        if (!confirmation)
        {
            const ItemDescriptor& item = descriptor(ItemId::MultiDeleteAll);
            CommandRequest request;
            request.command.section = item.section;
            request.command.item = item.item;
            request.refusal = RefusalReason::NotConfirmed;
            session.submit(std::move(request), std::move(completion));
            return;
        }
        session.submit(makeRequest(ItemId::MultiDeleteAll, NO_VALUES), std::move(completion));
    }

    void getCurrentMultiIndex(Session& session, MultiIndexCompletion completion)
    {
        session.submit(makeRequest(ItemId::MultiGetCurrentIndex, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           MultiIndexResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::MultiGetCurrentIndex, rep->data);
                               if (values && values->size() == 2)
                                   result.index = static_cast<int>((*values)[0] * DATA_BYTE_BASE + (*values)[1]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getCurrentMultiName(Session& session, MultiNameCompletion completion)
    {
        CommandOptions options;
        options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
        session.submit(makeRequest(ItemId::MultiGetCurrentName, NO_VALUES, std::move(options)),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           MultiNameResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                               result.name = decodeStringReply(ItemId::MultiGetCurrentName, rep->data);
                           if (completion)
                               completion(result);
                       });
    }

    void renameCurrentMulti(Session& session, std::string_view name, CommandCompletion completion)
    {
        session.submit(makeStringRequest(ItemId::MultiRename, name), std::move(completion));
    }

    void setMultiProgramNumber(Session& session, std::optional<int> frontPanelNumber, CommandCompletion completion)
    {
        // The front-panel range (spec Table 16, footnote a); &31's Data1/Data2 shape is conditional on Data1, so it
        // is written by hand rather than through makeRequest, as ProgramPrimitives::setProgramNumber does for §0A.
        constexpr int FRONT_PANEL_MIN = 1;
        constexpr int FRONT_PANEL_MAX = 128;
        const ItemDescriptor& item = descriptor(ItemId::MultiSetProgramNumber);

        CommandRequest request;
        request.command.section = item.section;
        request.command.item = item.item;

        ByteWriter writer;
        if (!frontPanelNumber.has_value())
        {
            // Off carries the number byte too, as 0: the spec says the number is "only required if Data1=1", but the
            // real S5000 (OS 2.14) answers ERROR 2 (out of range) to the flag alone (observed, TASK-AKM-094).
            writer.appendByte(0);
            writer.appendByte(0);
        }
        else if (*frontPanelNumber < FRONT_PANEL_MIN || *frontPanelNumber > FRONT_PANEL_MAX)
        {
            request.refusal = RefusalReason::ArgumentOutOfRange;
            session.submit(std::move(request), std::move(completion));
            return;
        }
        else
        {
            writer.appendByte(1);
            writer.appendByte(static_cast<std::uint32_t>(*frontPanelNumber - WIRE_OFFSET));
        }
        request.command.data = writer.bytes();
        session.submit(std::move(request), std::move(completion));
    }

    void setMultiPartByIndex(Session& session, int part, int programIndex, CommandCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(programIndex) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(programIndex) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::MultiSetPartByIndex, {static_cast<std::int64_t>(part), msb, lsb}),
                       std::move(completion));
    }

    void setMultiPartByName(Session& session, int part, std::string_view name, CommandCompletion completion)
    {
        // &33's shape (a part, then a String) fits neither makeStringRequest (exactly one String) nor the generic
        // int64_t path (no String support), so it is written by hand, the same way ZonePrimitives::setZoneSample
        // builds §06's &01 (ADR-AKM-001, DEC-AKM-013).
        const ItemDescriptor& item = descriptor(ItemId::MultiSetPartByName);
        const ValueSpec& partSpec = item.args[0];
        const ValueSpec& nameSpec = item.args[1];

        CommandRequest request;
        request.command.section = item.section;
        request.command.item = item.item;

        const auto partValue = static_cast<std::int64_t>(part);
        const auto length = static_cast<std::int64_t>(name.size());
        if (partValue < partSpec.min || partValue > partSpec.max || length < nameSpec.min || length > nameSpec.max)
        {
            request.refusal = RefusalReason::ArgumentOutOfRange;
            session.submit(std::move(request), std::move(completion));
            return;
        }

        ByteWriter writer;
        writer.appendByte(static_cast<std::uint32_t>(part));
        if (!writer.appendString(name))
        {
            request.refusal = RefusalReason::NotEncodable;
            session.submit(std::move(request), std::move(completion));
            return;
        }
        request.command.data = writer.bytes();
        session.submit(std::move(request), std::move(completion));
    }

    void deleteMultiPart(Session& session, int part, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::MultiDeletePart, {static_cast<std::int64_t>(part)}), std::move(completion));
    }

    void getMultiCount(Session& session, MultiCountCompletion completion)
    {
        session.submit(makeRequest(ItemId::MultiGetCount, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           MultiCountResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::MultiGetCount, rep->data);
                               if (values && values->size() == 2)
                                   result.count = static_cast<int>((*values)[0] * DATA_BYTE_BASE + (*values)[1]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getMultiProgramNumber(Session& session, MultiProgramNumberCompletion completion)
    {
        session.submit(makeRequest(ItemId::MultiGetProgramNumber, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           MultiProgramNumberResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::MultiGetProgramNumber, rep->data);
                               if (values && values->size() == 2)
                                   result.frontPanelNumber = frontPanelNumberOf(*values);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getCurrentMultiPartCount(Session& session, MultiPartCountCompletion completion)
    {
        session.submit(makeRequest(ItemId::MultiGetPartCount, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           MultiPartCountResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::MultiGetPartCount, rep->data);
                               if (values && values->size() == 1)
                                   result.partCount = static_cast<int>((*values)[0]) + WIRE_OFFSET;
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getMultiPartName(Session& session, int part, MultiNameCompletion completion)
    {
        session.submit(makeRequest(ItemId::MultiGetPartName, {static_cast<std::int64_t>(part)}, variableLengthReply()),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           MultiNameResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                               result.name = decodeStringReply(ItemId::MultiGetPartName, rep->data);
                           if (completion)
                               completion(result);
                       });
    }

    namespace
    {
        void submitNameList(Session& session, ItemId id, MultiNameListCompletion completion)
        {
            session.submit(makeRequest(id, NO_VALUES, variableLengthReply()),
                           [completion = std::move(completion)](const CommandResult& outcome) {
                               MultiNameListResult result{std::nullopt, outcome};
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

        // One value per record of a REPLY that repeats a one-byte record, plus `offset`.
        void submitRepeatedByte(Session& session, ItemId id, int offset, MultiValueListCompletion completion)
        {
            session.submit(makeRequest(id, NO_VALUES, variableLengthReply()),
                           [id, offset, completion = std::move(completion)](const CommandResult& outcome) {
                               MultiValueListResult result{std::nullopt, outcome};
                               if (answersEmptyMemory(outcome))
                                   result.values = std::vector<int>{};
                               else if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                               {
                                   if (const auto records = decodeRepeatedReply(id, rep->data))
                                   {
                                       std::vector<int> values;
                                       for (const auto& record : *records)
                                           values.push_back(static_cast<int>(record[0]) + offset);
                                       result.values = std::move(values);
                                   }
                               }
                               if (completion)
                                   completion(result);
                           });
        }
    }

    void getAllMultiPartNames(Session& session, MultiNameListCompletion completion)
    {
        submitNameList(session, ItemId::MultiGetAllPartNames, std::move(completion));
    }

    void getAllMultiNames(Session& session, MultiNameListCompletion completion)
    {
        submitNameList(session, ItemId::MultiGetAllNames, std::move(completion));
    }

    void getAllMultiPartParameters(Session& session, int part, MultiValueListCompletion completion)
    {
        session.submit(makeRequest(ItemId::MultiGetAllPartParameters, {static_cast<std::int64_t>(part)}),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           MultiValueListResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               if (const auto values = decodeReply(ItemId::MultiGetAllPartParameters, rep->data))
                               {
                                   std::vector<int> parameters;
                                   for (const std::int64_t value : *values)
                                       parameters.push_back(static_cast<int>(value));
                                   result.values = std::move(parameters);
                               }
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getMultiMuteSoloStatus(Session& session, MultiValueListCompletion completion)
    {
        submitRepeatedByte(session, ItemId::MultiGetMuteSolo, 0, std::move(completion));
    }

    void getAllMultiPartCounts(Session& session, MultiValueListCompletion completion)
    {
        submitRepeatedByte(session, ItemId::MultiGetAllPartCounts, WIRE_OFFSET, std::move(completion));
    }

    void getAllMultiProgramNumbers(Session& session, MultiProgramNumbersCompletion completion)
    {
        session.submit(makeRequest(ItemId::MultiGetAllProgramNumbers, NO_VALUES, variableLengthReply()),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           MultiProgramNumbersResult result{std::nullopt, outcome};
                           if (answersEmptyMemory(outcome))
                               result.numbers = std::vector<std::optional<int>>{};
                           else if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               if (const auto records = decodeRepeatedReply(ItemId::MultiGetAllProgramNumbers, rep->data))
                               {
                                   std::vector<std::optional<int>> numbers;
                                   for (const auto& record : *records)
                                       numbers.push_back(frontPanelNumberOf(record));
                                   result.numbers = std::move(numbers);
                               }
                           }
                           if (completion)
                               completion(result);
                       });
    }
}
