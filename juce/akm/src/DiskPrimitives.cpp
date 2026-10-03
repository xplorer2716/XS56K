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
#include "akm/ByteWriter.hpp"
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

    void selectDisk(Session& session, int handle, CommandCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(handle) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(handle) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::DiskSelect, {msb, lsb}), std::move(completion));
    }

    void testDiskValid(Session& session, int handle, CommandCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(handle) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(handle) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::DiskTestValid, {msb, lsb}), std::move(completion));
    }

    void getCurrentDiskType(Session& session, DiskTypeCompletion completion)
    {
        session.submit(makeRequest(ItemId::DiskGetCurrentType, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskTypeResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::DiskGetCurrentType, rep->data);
                               if (values && values->size() == 1)
                                   result.type = static_cast<int>((*values)[0]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getDiskType(Session& session, int handle, DiskTypeCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(handle) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(handle) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::DiskGetType, {msb, lsb}),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskTypeResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::DiskGetType, rep->data);
                               if (values && values->size() == 1)
                                   result.type = static_cast<int>((*values)[0]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getCurrentDiskHandle(Session& session, DiskHandleCompletion completion)
    {
        session.submit(makeRequest(ItemId::DiskGetCurrentHandle, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskHandleResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::DiskGetCurrentHandle, rep->data);
                               if (values && values->size() == 2)
                                   result.handle = static_cast<int>((*values)[0] * DATA_BYTE_BASE + (*values)[1]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getCurrentDiskPath(Session& session, DiskPathCompletion completion)
    {
        CommandOptions options;
        options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
        session.submit(makeRequest(ItemId::DiskGetCurrentPath, NO_VALUES, std::move(options)),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskPathResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                               result.path = decodeStringReply(ItemId::DiskGetCurrentPath, rep->data);
                           if (completion)
                               completion(result);
                       });
    }

    void getCurrentDiskFormat(Session& session, DiskFormatCompletion completion)
    {
        session.submit(makeRequest(ItemId::DiskGetCurrentFormat, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskFormatResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::DiskGetCurrentFormat, rep->data);
                               if (values && values->size() == 1)
                                   result.format = static_cast<int>((*values)[0]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getCurrentDiskFreeSpace(Session& session, DiskFreeSpaceCompletion completion)
    {
        session.submit(makeRequest(ItemId::DiskGetFreeSpace, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskFreeSpaceResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::DiskGetFreeSpace, rep->data);
                               if (values && values->size() == 1)
                                   result.freeBytes = static_cast<std::uint64_t>((*values)[0]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getDiskName(Session& session, int handle, DiskNameCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(handle) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(handle) % DATA_BYTE_BASE;
        CommandOptions options;
        options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
        session.submit(makeRequest(ItemId::DiskGetName, {msb, lsb}, std::move(options)),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskNameResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                               result.name = decodeStringReply(ItemId::DiskGetName, rep->data);
                           if (completion)
                               completion(result);
                       });
    }

    void getFolderCount(Session& session, DiskFolderCountCompletion completion)
    {
        session.submit(makeRequest(ItemId::DiskGetFolderCount, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskFolderCountResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::DiskGetFolderCount, rep->data);
                               if (values && values->size() == 2)
                                   result.count = static_cast<int>((*values)[0] * DATA_BYTE_BASE + (*values)[1]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getFolderName(Session& session, int index, DiskFolderNameCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(index) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(index) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::DiskGetFolderName, {msb, lsb}),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskFolderNameResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                               result.name = decodeStringReply(ItemId::DiskGetFolderName, rep->data);
                           if (completion)
                               completion(result);
                       });
    }

    void getAllFolderNames(Session& session, DiskFolderNamesCompletion completion)
    {
        CommandOptions options;
        options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
        session.submit(makeRequest(ItemId::DiskGetAllFolderNames, NO_VALUES, std::move(options)),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskFolderNamesResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               ByteReader reader(rep->data);
                               result.names = reader.readStringList();
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void openFolder(Session& session, std::string_view name, CommandCompletion completion)
    {
        session.submit(makeStringRequest(ItemId::DiskOpenFolder, name), std::move(completion));
    }

    void closeFolder(Session& session, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::DiskCloseFolder, NO_VALUES), std::move(completion));
    }

    void createFolder(Session& session, std::string_view name, CommandCompletion completion)
    {
        session.submit(makeStringRequest(ItemId::DiskCreateFolder, name), std::move(completion));
    }

    void renameFolder(Session& session, std::string_view oldName, std::string_view newName, CommandCompletion completion)
    {
        session.submit(makeTwoStringRequest(ItemId::DiskRenameFolder, oldName, newName), std::move(completion));
    }

    void loadFolder(Session& session, std::string_view name, CommandCompletion completion)
    {
        session.submit(makeStringRequest(ItemId::DiskLoadFolder, name), std::move(completion));
    }

    void getFileCount(Session& session, DiskFileCountCompletion completion)
    {
        session.submit(makeRequest(ItemId::DiskGetFileCount, NO_VALUES),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskFileCountResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::DiskGetFileCount, rep->data);
                               if (values && values->size() == 2)
                                   result.count = static_cast<int>((*values)[0] * DATA_BYTE_BASE + (*values)[1]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getFileName(Session& session, int index, DiskFileNameCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(index) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(index) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::DiskGetFileName, {msb, lsb}),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskFileNameResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                               result.name = decodeStringReply(ItemId::DiskGetFileName, rep->data);
                           if (completion)
                               completion(result);
                       });
    }

    void getAllFileNames(Session& session, DiskFileNamesCompletion completion)
    {
        CommandOptions options;
        options.expectedReply = ExpectedReply::NeedsKnownChecksumMode;
        session.submit(makeRequest(ItemId::DiskGetAllFileNames, NO_VALUES, std::move(options)),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskFileNamesResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               ByteReader reader(rep->data);
                               result.names = reader.readStringList();
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getFileSize(Session& session, int index, DiskFileSizeCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(index) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(index) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::DiskGetFileSize, {msb, lsb}),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskFileSizeResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::DiskGetFileSize, rep->data);
                               if (values && values->size() == 4)
                                   result.sizeBytes = static_cast<std::uint32_t>(
                                       ((*values)[0] << 21) | ((*values)[1] << 14) | ((*values)[2] << 7) | (*values)[3]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void getFileIndexByName(Session& session, std::string_view name, DiskFileIndexCompletion completion)
    {
        session.submit(makeStringRequest(ItemId::DiskGetFileIndexByName, name),
                       [completion = std::move(completion)](const CommandResult& outcome) {
                           DiskFileIndexResult result{std::nullopt, outcome};
                           if (const auto* rep = std::get_if<Reply>(&outcome); rep != nullptr)
                           {
                               const auto values = decodeReply(ItemId::DiskGetFileIndexByName, rep->data);
                               if (values && values->size() == 2)
                                   result.index = static_cast<int>((*values)[0] * DATA_BYTE_BASE + (*values)[1]);
                           }
                           if (completion)
                               completion(result);
                       });
    }

    void renameFile(Session& session, std::string_view oldName, std::string_view newName, CommandCompletion completion)
    {
        session.submit(makeTwoStringRequest(ItemId::DiskRenameFile, oldName, newName), std::move(completion));
    }

    void loadFile(Session& session, std::string_view name, SampleLoadOption sampleLoadOption, CommandCompletion completion)
    {
        // &2A's shape (a String then a Byte) fits neither makeStringRequest (exactly one String) nor the
        // generic int64_t path (no String support), so it is written by hand, the same way
        // ProgramPrimitives::setProgramNumber builds its own conditional shape.
        const ItemDescriptor& item = descriptor(ItemId::DiskLoadFile);
        CommandRequest request;
        request.command.section = item.section;
        request.command.item = item.item;

        const auto length = static_cast<std::int64_t>(name.size());
        if (length < item.args[0].min || length > item.args[0].max)
        {
            request.refusal = RefusalReason::ArgumentOutOfRange;
            session.submit(std::move(request), std::move(completion));
            return;
        }

        ByteWriter writer;
        if (!writer.appendString(name))
        {
            request.refusal = RefusalReason::NotEncodable;
            session.submit(std::move(request), std::move(completion));
            return;
        }
        writer.appendByte(static_cast<std::uint32_t>(sampleLoadOption));
        request.command.data = writer.bytes();
        session.submit(std::move(request), std::move(completion));
    }

    void loadFileWithDependents(Session& session, std::string_view name, CommandCompletion completion)
    {
        session.submit(makeStringRequest(ItemId::DiskLoadFileWithDependents, name), std::move(completion));
    }

    void saveMemoryItem(Session& session, int index, SaveableMemoryType type, bool overwriteExisting,
                        bool saveChildren, CommandCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(index) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(index) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::DiskSaveMemoryItem, {msb, lsb, static_cast<std::int64_t>(type),
                                                                overwriteExisting ? 1 : 0, saveChildren ? 1 : 0}),
                       std::move(completion));
    }

    void saveAllMemoryItems(Session& session, SaveableMemoryType type, bool overwriteExisting, bool saveChildren,
                           CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::DiskSaveAllMemoryItems,
                                   {static_cast<std::int64_t>(type), overwriteExisting ? 1 : 0, saveChildren ? 1 : 0}),
                       std::move(completion));
    }

    void startFileAudition(Session& session, int index, CommandCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(index) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(index) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::DiskStartFileAudition, {msb, lsb}), std::move(completion));
    }

    void stopFileAudition(Session& session, CommandCompletion completion)
    {
        session.submit(makeRequest(ItemId::DiskStopFileAudition, NO_VALUES), std::move(completion));
    }

    void ejectDisk(Session& session, int handle, CommandCompletion completion)
    {
        const auto msb = static_cast<std::int64_t>(handle) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(handle) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::DiskEjectDisk, {msb, lsb, 0}), std::move(completion));
    }

    void ejectDiskDiscardingVirtualSamples(Session& session, int handle,
                                           std::optional<ConfirmEjectDiscardingVirtualSamples> confirmation,
                                           CommandCompletion completion)
    {
        const ItemDescriptor& item = descriptor(ItemId::DiskEjectDisk);
        if (!confirmation)
        {
            CommandRequest request;
            request.command.section = item.section;
            request.command.item = item.item;
            request.refusal = RefusalReason::NotConfirmed;
            session.submit(std::move(request), std::move(completion));
            return;
        }
        const auto msb = static_cast<std::int64_t>(handle) / DATA_BYTE_BASE;
        const auto lsb = static_cast<std::int64_t>(handle) % DATA_BYTE_BASE;
        session.submit(makeRequest(ItemId::DiskEjectDisk, {msb, lsb, 1}), std::move(completion));
    }

    void deleteSubFolder(Session& session, std::string_view name, std::optional<ConfirmDeleteSubFolder> confirmation,
                        CommandCompletion completion)
    {
        if (!confirmation)
        {
            const ItemDescriptor& item = descriptor(ItemId::DiskDeleteSubFolder);
            CommandRequest request;
            request.command.section = item.section;
            request.command.item = item.item;
            request.refusal = RefusalReason::NotConfirmed;
            session.submit(std::move(request), std::move(completion));
            return;
        }
        session.submit(makeStringRequest(ItemId::DiskDeleteSubFolder, name), std::move(completion));
    }

    void deleteFile(Session& session, std::string_view name, std::optional<ConfirmDeleteFile> confirmation,
                   CommandCompletion completion)
    {
        if (!confirmation)
        {
            const ItemDescriptor& item = descriptor(ItemId::DiskDeleteFile);
            CommandRequest request;
            request.command.section = item.section;
            request.command.item = item.item;
            request.refusal = RefusalReason::NotConfirmed;
            session.submit(std::move(request), std::move(completion));
            return;
        }
        session.submit(makeStringRequest(ItemId::DiskDeleteFile, name), std::move(completion));
    }
}
