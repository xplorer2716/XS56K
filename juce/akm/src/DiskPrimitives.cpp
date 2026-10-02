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
#include "akm/DiskPrimitives.hpp"

#include <span>
#include <utility>
#include <vector>

#include "akm/ByteReader.hpp"
#include "akm/ItemRequest.hpp"

namespace akm
{
    namespace
    {
        constexpr std::span<const std::int64_t> NO_VALUES{};
        // A zero-based handle is split into two 7-bit data bytes, most significant first (spec pp. 8-9's
        // compound word), the same convention Program/Sample indices already use (ProgramPrimitives.cpp,
        // SamplePrimitives.cpp): the catalogue lists them as two separate Byte values, not a Word.
        constexpr std::int64_t DATA_BYTE_BASE = 128;
    }

    void updateDiskList(Session& session, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::DiskUpdateList, NO_VALUES), std::move(completion));
    }

    void getDiskCount(Session& session, DiskCountCompletion completion)
    {
        session.submit(makeRequest(ItemId::DiskGetCount, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskCountResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::DiskGetCount, rep->data);
                               if (values && values->size() == 1)
                                   result.count = static_cast<int>((*values)[0]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getConnectedDisks(Session& session, DiskListCompletion completion)
    {
        CommandOptions options;
        options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
        session.submit(makeRequest(ItemId::DiskGetList, NO_VALUES, std::move(options)),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskListResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               // §10/&05's REPLY repeats a 6-field-plus-name record once per disk (handle,
                               // type, format, SCSI ID, writable, name): no shape in the catalogue schema
                               // mixes fixed fields with a trailing String, so this is decoded the same
                               // custom way getAllProgramNumbers/getAllProgramNames already are, not
                               // through the generic decodeReply/decodeRepeatedReply.
                               ByteReader reader(rep->data);
                               std::vector<DiskInfo> disks;
                               bool malformed = false;
                               while (reader.remaining() > 0 && !malformed)
                               {
                                   const auto handleMsb = reader.readByte();
                                   const auto handleLsb = reader.readByte();
                                   const auto type = reader.readByte();
                                   const auto format = reader.readByte();
                                   const auto scsiId = reader.readByte();
                                   const auto writable = reader.readByte();
                                   const auto name = reader.readString();
                                   if (!handleMsb || !handleLsb || !type || !format || !scsiId || !writable || !name)
                                   {
                                       malformed = true;
                                       break;
                                   }
                                   DiskInfo disk;
                                   disk.handle = static_cast<int>(*handleMsb * DATA_BYTE_BASE + *handleLsb);
                                   disk.type = *type;
                                   disk.format = *format;
                                   disk.scsiId = *scsiId;
                                   disk.writable = *writable != 0;
                                   disk.name = *name;
                                   disks.push_back(std::move(disk));
                               }
                               if (!malformed)
                                   result.disks = std::move(disks);
                           }
                           if (completion)
                               completion(result);
                       });
    }
}
