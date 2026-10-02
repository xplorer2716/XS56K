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
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "akm/CommandResult.hpp"
#include "akm/Session.hpp"

namespace akm
{
    // Disk discovery of section 10 (spec Tables 20-21, "General Disk Functions"): thin typed wrappers
    // over the catalogue's records (ADR-AKM-001, DEC-AKM-003, DEC-AKM-012), like every other primitives
    // file. Each returns at once and reports on the session's thread, like `Session::submit`. The
    // session must outlive every call. [RQ-AKM-060]

    /// Tells the sampler to refresh its list of connected disks (§10/&01). `getDiskCount` and
    /// `getConnectedDisks` are not guaranteed to reflect the sampler's actual disks until this has
    /// completed at least once in the session (spec Table 20, footnote b) — a precondition the spec
    /// states, not one this layer enforces. [RQ-AKM-060]
    void updateDiskList(Session& session, CommandCompletion completion);

    /// `count` is empty when the command did not complete on a REPLY of the length the catalogue gives
    /// it; `outcome` is the result of the command. [RQ-AKM-060]
    struct DiskCountResult
    {
        std::optional<int> count{};
        CommandResult outcome{};
    };
    using DiskCountCompletion = std::function<void(const DiskCountResult&)>;

    /// Gets the number of disks connected (§10/&04). [RQ-AKM-060]
    void getDiskCount(Session& session, DiskCountCompletion completion);

    /// One entry of `&05`'s REPLY, in the spec's own field order. [RQ-AKM-060]
    struct DiskInfo
    {
        int handle = 0;
        int type = 0;
        int format = 0;
        int scsiId = 0;
        bool writable = false;
        std::string name;

        friend bool operator==(const DiskInfo&, const DiskInfo&) = default;
    };

    /// `disks` is empty when the command did not complete on a decodable REPLY (not the same as a
    /// decoded REPLY naming zero disks, which is `disks` holding an empty vector); `outcome` is the
    /// result of the command. [RQ-AKM-060]
    struct DiskListResult
    {
        std::optional<std::vector<DiskInfo>> disks{};
        CommandResult outcome{};
    };
    using DiskListCompletion = std::function<void(const DiskListResult&)>;

    /// Gets the list of every connected disk (§10/&05): one `DiskInfo` per disk, in the order the
    /// sampler sent them. Refused as `ChecksumModeUnknown` while the port's checksum mode is unknown:
    /// the REPLY repeats a record of unknown count ending in a variable-length name, so it has no fixed
    /// length either (mirrors `getAllProgramNames`, ADR-AKM-001 DEC-AKM-014). [RQ-AKM-060, RQ-AKM-041]
    void getConnectedDisks(Session& session, DiskListCompletion completion);

    // Disk selection and status (§10/&02, &03, &06-&09): the disk a caller names by `handle`, the same
    // one `getConnectedDisks` reports for each entry. [RQ-AKM-061]

    /// Selects the disk named by `handle` (§10/&02). [RQ-AKM-061]
    void selectDisk(Session& session, int handle, CommandCompletion completion);

    /// Tests whether the disk named by `handle` is usable (§10/&03): completes `Done` when it is, an
    /// `Error` otherwise — never a `Reply` (spec Table 20, footnote a). [RQ-AKM-061]
    void testDiskValid(Session& session, int handle, CommandCompletion completion);

    /// `type` is empty when the command did not complete on a REPLY of the length the catalogue gives
    /// it; `outcome` is the result of the command. A decoded value outside 0 (floppy), 1 (hard disk),
    /// 2 (CD-ROM) or 3 (removable) is reported as is, not as malformed: the spec's own text for `&07`
    /// calls it "an unknown disk type", not an error. [RQ-AKM-061]
    struct DiskTypeResult
    {
        std::optional<int> type{};
        CommandResult outcome{};
    };
    using DiskTypeCompletion = std::function<void(const DiskTypeResult&)>;

    /// Gets the type of the currently selected disk (§10/&06). [RQ-AKM-061]
    void getCurrentDiskType(Session& session, DiskTypeCompletion completion);

    /// Gets the type of the disk named by `handle` (§10/&07), the same shape as `getCurrentDiskType`.
    /// [RQ-AKM-061]
    void getDiskType(Session& session, int handle, DiskTypeCompletion completion);

    /// `handle` is empty when the command did not complete on a REPLY of the length the catalogue gives
    /// it; `outcome` is the result of the command. [RQ-AKM-061]
    struct DiskHandleResult
    {
        std::optional<int> handle{};
        CommandResult outcome{};
    };
    using DiskHandleCompletion = std::function<void(const DiskHandleResult&)>;

    /// Gets the handle of the currently selected disk (§10/&08) — the one last given to `selectDisk`.
    /// Named "Get index of current disk" by the spec's own command table (Table 20), but its REPLY
    /// (Table 21) names the same two bytes the handle, which is what this returns: a wording mismatch
    /// confirmed by the spec's own tables alone, nothing left to settle on real hardware. [RQ-AKM-061]
    void getCurrentDiskHandle(Session& session, DiskHandleCompletion completion);

    /// `path` is empty when the command did not complete on a REPLY holding exactly one null-terminated
    /// path; `outcome` is the result of the command. An empty (not absent) path means the root folder
    /// is selected (spec: "a single byte = 0"), which `decodeStringReply` already returns as an empty
    /// string with no extra handling needed. [RQ-AKM-061]
    struct DiskPathResult
    {
        std::optional<std::string> path{};
        CommandResult outcome{};
    };
    using DiskPathCompletion = std::function<void(const DiskPathResult&)>;

    /// Gets the current path on the currently selected disk (§10/&09). Refused as `ChecksumModeUnknown`
    /// while the port's checksum mode is unknown, for the same reason as `getCurrentProgramName`.
    /// [RQ-AKM-061, RQ-AKM-041]
    void getCurrentDiskPath(Session& session, DiskPathCompletion completion);

    // Disk format, free space and name (§10/&0A, &0B, &0E). [RQ-AKM-062]

    /// `format` is empty when the command did not complete on a REPLY of the length the catalogue gives
    /// it; `outcome` is the result of the command. 0 = other, 1 = MSDOS, 2 = FAT32, 3 = ISO9660, 4 =
    /// S1000, 5 = S3000, 6 = EMU, 7 = ROLAND. [RQ-AKM-062]
    struct DiskFormatResult
    {
        std::optional<int> format{};
        CommandResult outcome{};
    };
    using DiskFormatCompletion = std::function<void(const DiskFormatResult&)>;

    /// Gets the format of the currently selected disk (§10/&0A). [RQ-AKM-062]
    void getCurrentDiskFormat(Session& session, DiskFormatCompletion completion);

    /// `freeBytes` is empty when the command did not complete on a REPLY of the length the catalogue
    /// gives it; `outcome` is the result of the command. The REPLY is the catalogue's first `Qword`
    /// (ADR-AKM-001, DEC-AKM-017): the generic `std::int64_t` decode path already fits its 56-bit range,
    /// so `freeBytes` only widens it to `std::uint64_t` for a byte count that cannot be negative.
    /// [RQ-AKM-062]
    struct DiskFreeSpaceResult
    {
        std::optional<std::uint64_t> freeBytes{};
        CommandResult outcome{};
    };
    using DiskFreeSpaceCompletion = std::function<void(const DiskFreeSpaceResult&)>;

    /// Gets the free space, in bytes, of the currently selected disk (§10/&0B). [RQ-AKM-062]
    void getCurrentDiskFreeSpace(Session& session, DiskFreeSpaceCompletion completion);

    /// `name` is empty when the command did not complete on a REPLY holding exactly one null-terminated
    /// name; `outcome` is the result of the command. [RQ-AKM-062]
    struct DiskNameResult
    {
        std::optional<std::string> name{};
        CommandResult outcome{};
    };
    using DiskNameCompletion = std::function<void(const DiskNameResult&)>;

    /// Gets the name of the disk named by `handle` (§10/&0E). Refused as `ChecksumModeUnknown` while the
    /// port's checksum mode is unknown, for the same reason as `getCurrentProgramName`. [RQ-AKM-062,
    /// RQ-AKM-041]
    void getDiskName(Session& session, int handle, DiskNameCompletion completion);

    // Folder navigation, listing and management (§10/&10-&14, &16, &18) on the currently selected disk's
    // currently selected folder. [RQ-AKM-063]

    /// `count` is empty when the command did not complete on a REPLY of the length the catalogue gives
    /// it; `outcome` is the result of the command. [RQ-AKM-063]
    struct DiskFolderCountResult
    {
        std::optional<int> count{};
        CommandResult outcome{};
    };
    using DiskFolderCountCompletion = std::function<void(const DiskFolderCountResult&)>;

    /// Gets the number of sub-folders in the current folder (§10/&10). [RQ-AKM-063]
    void getFolderCount(Session& session, DiskFolderCountCompletion completion);

    /// `name` is empty when the command did not complete on a REPLY holding exactly one null-terminated
    /// name; `outcome` is the result of the command. [RQ-AKM-063]
    struct DiskFolderNameResult
    {
        std::optional<std::string> name{};
        CommandResult outcome{};
    };
    using DiskFolderNameCompletion = std::function<void(const DiskFolderNameResult&)>;

    /// Gets the name of the sub-folder at zero-based `index` in the current folder (§10/&11). [RQ-AKM-063]
    void getFolderName(Session& session, int index, DiskFolderNameCompletion completion);

    /// One entry per sub-folder of the current folder, in the order the sampler sent them. Empty when the
    /// command did not complete on a decodable REPLY. [RQ-AKM-063]
    struct DiskFolderNamesResult
    {
        std::optional<std::vector<std::string>> names{};
        CommandResult outcome{};
    };
    using DiskFolderNamesCompletion = std::function<void(const DiskFolderNamesResult&)>;

    /// Gets the names of every sub-folder of the current folder (§10/&12). Refused as
    /// `ChecksumModeUnknown` while the port's checksum mode is unknown, for the same reason as
    /// `getAllProgramNames`. [RQ-AKM-063, RQ-AKM-041]
    void getAllFolderNames(Session& session, DiskFolderNamesCompletion completion);

    /// Opens the sub-folder named `name` of the current folder, making it current (§10/&13); an empty
    /// `name` selects the root folder, the spec's own `<Data1> = 0` convention — which a null-terminated
    /// empty string already encodes with no special case needed. [RQ-AKM-063]
    void openFolder(Session& session, std::string_view name, CommandCompletion completion);

    /// Closes the current folder, making its parent current (§10/&14); completes `Error` when the
    /// current folder is already the root (the spec's own behaviour, not this layer's choice).
    /// [RQ-AKM-063]
    void closeFolder(Session& session, CommandCompletion completion);

    /// Creates a sub-folder named `name` in the current folder (§10/&16). [RQ-AKM-063]
    void createFolder(Session& session, std::string_view name, CommandCompletion completion);

    /// Renames the sub-folder named `oldName` of the current folder to `newName` (§10/&18): the
    /// catalogue's first item carrying two consecutive `String` arguments (ADR-AKM-001, DEC-AKM-018).
    /// [RQ-AKM-063]
    void renameFolder(Session& session, std::string_view oldName, std::string_view newName, CommandCompletion completion);

    /// Loads the sub-folder named `name` of the current folder, and everything it contains including
    /// its own sub-folders, into memory (§10/&15). SHALL NOT be sent to the real sampler except through
    /// the guard of `RQ-AKM-070` — the spec itself calls this operation potentially long-running, and
    /// one real-hardware run of `&01` (the same section's other such item) left the sampler answering no
    /// SysEx at all (`process/2.architecture/OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`).
    /// [RQ-AKM-064, RQ-AKM-070]
    void loadFolder(Session& session, std::string_view name, CommandCompletion completion);

    // File listing, info and rename (§10/&20-&24, &28) in the current folder. [RQ-AKM-065]

    /// `count` is empty when the command did not complete on a REPLY of the length the catalogue gives
    /// it; `outcome` is the result of the command. [RQ-AKM-065]
    struct DiskFileCountResult
    {
        std::optional<int> count{};
        CommandResult outcome{};
    };
    using DiskFileCountCompletion = std::function<void(const DiskFileCountResult&)>;

    /// Gets the number of files in the current folder (§10/&20). [RQ-AKM-065]
    void getFileCount(Session& session, DiskFileCountCompletion completion);

    /// `name` is empty when the command did not complete on a REPLY holding exactly one null-terminated
    /// name; `outcome` is the result of the command. [RQ-AKM-065]
    struct DiskFileNameResult
    {
        std::optional<std::string> name{};
        CommandResult outcome{};
    };
    using DiskFileNameCompletion = std::function<void(const DiskFileNameResult&)>;

    /// Gets the name of the file at zero-based `index` in the current folder (§10/&21). [RQ-AKM-065]
    void getFileName(Session& session, int index, DiskFileNameCompletion completion);

    /// One entry per file in the current folder, in the order the sampler sent them. Empty when the
    /// command did not complete on a decodable REPLY. [RQ-AKM-065]
    struct DiskFileNamesResult
    {
        std::optional<std::vector<std::string>> names{};
        CommandResult outcome{};
    };
    using DiskFileNamesCompletion = std::function<void(const DiskFileNamesResult&)>;

    /// Gets the names of every file in the current folder (§10/&22). Refused as `ChecksumModeUnknown`
    /// while the port's checksum mode is unknown, for the same reason as `getAllProgramNames`.
    /// [RQ-AKM-065, RQ-AKM-041]
    void getAllFileNames(Session& session, DiskFileNamesCompletion completion);

    /// `sizeBytes` is empty when the command did not complete on a REPLY of the length the catalogue
    /// gives it; `outcome` is the result of the command. The REPLY is a Compound Double Word, the same
    /// shape §0E's position and loop items use (four separate `Byte` values in the catalogue, combined
    /// here), not the single `Dword` §02's Wave memory uses — this item's own spec row decomposes
    /// cleanly into four byte-sized columns, so it is catalogued and verified that way rather than
    /// skipping the coverage check the way an ellipsis-shortened row does. [RQ-AKM-065]
    struct DiskFileSizeResult
    {
        std::optional<std::uint32_t> sizeBytes{};
        CommandResult outcome{};
    };
    using DiskFileSizeCompletion = std::function<void(const DiskFileSizeResult&)>;

    /// Gets the size, in bytes, of the file at zero-based `index` in the current folder (§10/&23).
    /// [RQ-AKM-065]
    void getFileSize(Session& session, int index, DiskFileSizeCompletion completion);

    /// `index` is empty when the command did not complete on a REPLY of the length the catalogue gives
    /// it (which includes the file not being found: the spec answers ERROR, not an empty REPLY, per its
    /// own text for `&24`); `outcome` is the result of the command. [RQ-AKM-065]
    struct DiskFileIndexResult
    {
        std::optional<int> index{};
        CommandResult outcome{};
    };
    using DiskFileIndexCompletion = std::function<void(const DiskFileIndexResult&)>;

    /// Gets the index of the file named `name` in the current folder (§10/&24). [RQ-AKM-065]
    void getFileIndexByName(Session& session, std::string_view name, DiskFileIndexCompletion completion);

    /// Renames the file named `oldName` in the current folder to `newName` (§10/&28); `newName`'s
    /// extension, if any, is sent exactly as given — the spec's own footnote says the sampler appends
    /// one automatically, so repeating it is a caller mistake, not something this layer corrects.
    /// [RQ-AKM-065]
    void renameFile(Session& session, std::string_view oldName, std::string_view newName, CommandCompletion completion);

    // Load File, with and without dependent children (§10/&2A, &2B). [RQ-AKM-066]

    /// The sample-loading option of `loadFile` (§10/&2A), with the byte it travels as: only relevant
    /// when the loaded file is a sample. [RQ-AKM-066]
    enum class SampleLoadOption
    {
        Normal = 0,
        Ram = 1,
        Virtual = 2,
    };

    /// Loads the file named `name` (§10/&2A) — its extension decides whether it is a multi, program,
    /// sample or scenelist; `sampleLoadOption` matters only when it is a sample. No file it depends on
    /// is loaded automatically (spec footnote d); use `loadFileWithDependents` for that. SHALL NOT be
    /// sent to the real sampler except through the guard of `RQ-AKM-070`. [RQ-AKM-066, RQ-AKM-070]
    void loadFile(Session& session, std::string_view name, SampleLoadOption sampleLoadOption,
                 CommandCompletion completion);

    /// Loads the file named `name` together with every file it depends on (§10/&2B, spec footnote e —
    /// e.g. a program's own samples). SHALL NOT be sent to the real sampler except through the guard of
    /// `RQ-AKM-070`. [RQ-AKM-066, RQ-AKM-070]
    void loadFileWithDependents(Session& session, std::string_view name, CommandCompletion completion);

    // Save Memory Item(s) to disk (§10/&2C, &2D). [RQ-AKM-067]

    /// The kind of memory item `saveMemoryItem`/`saveAllMemoryItems` saves, with the byte it travels as.
    /// [RQ-AKM-067]
    enum class SaveableMemoryType
    {
        Multi = 1,
        Program = 2,
        Sample = 3,
        Smf = 4,
        Setlist = 5,
        Scenelist = 6,
    };

    /// Saves the memory item at zero-based `index` of kind `type` to the current folder (§10/&2C).
    /// `overwriteExisting` has no default (the caller SHALL pass it explicitly — the safer failure mode
    /// is to skip a file that already exists, not to silently replace it); `saveChildren` asks for
    /// dependent files (e.g. a program's samples) to be saved alongside it. SHALL NOT be sent to the
    /// real sampler except through the guard of `RQ-AKM-070`. [RQ-AKM-067, RQ-AKM-070]
    void saveMemoryItem(Session& session, int index, SaveableMemoryType type, bool overwriteExisting,
                        bool saveChildren, CommandCompletion completion);

    /// Saves every memory item of kind `type` to the current folder (§10/&2D), the same guarantees as
    /// `saveMemoryItem` about `overwriteExisting`. SHALL NOT be sent to the real sampler except through
    /// the guard of `RQ-AKM-070`. [RQ-AKM-067, RQ-AKM-070]
    void saveAllMemoryItems(Session& session, SaveableMemoryType type, bool overwriteExisting, bool saveChildren,
                           CommandCompletion completion);

    // Sample audition from disk (§10/&30, &31) — a sample file played without being loaded into
    // memory, distinct from `startSampleAudition`/`stopSampleAudition` (SamplePrimitives.hpp, §0E),
    // which audition the current sample already in memory. [RQ-AKM-068]

    /// Starts auditioning the file at zero-based `index` in the current folder (§10/&30), without
    /// loading it into memory. [RQ-AKM-068]
    void startFileAudition(Session& session, int index, CommandCompletion completion);

    /// Stops the current audition (§10/&31). [RQ-AKM-068]
    void stopFileAudition(Session& session, CommandCompletion completion);

    // Destructive command guards for Eject, Delete Sub-Folder and Delete File (§10/&0D, &17, &29).
    // [RQ-AKM-069]

    /// Ejects the disk named by `handle` (§10/&0D), failing rather than discarding anything if the
    /// drive is in use (`<Data3> = 0`) — needs no confirmation, since it cannot lose data on its own.
    /// Use `ejectDiskDiscardingVirtualSamples` to allow it to discard instead. [RQ-AKM-069]
    void ejectDisk(Session& session, int handle, CommandCompletion completion);

    /// Passed to `ejectDiskDiscardingVirtualSamples` to prove the caller means it. No default: sent
    /// only when it is the enumerator; `std::nullopt` refuses the command as `NotConfirmed` without
    /// sending. [RQ-AKM-069]
    enum class ConfirmEjectDiscardingVirtualSamples
    {
        IUnderstandThisDiscardsEveryVirtualSampleOnThisDisk,
    };

    /// Ejects the disk named by `handle` (§10/&0D, `<Data3> = 1`), discarding any virtual sample open
    /// from it rather than failing because the drive is in use. No real-sampler test of any feature
    /// calls this. [RQ-AKM-069]
    void ejectDiskDiscardingVirtualSamples(Session& session, int handle,
                                           std::optional<ConfirmEjectDiscardingVirtualSamples> confirmation,
                                           CommandCompletion completion);

    /// Passed to `deleteSubFolder` to prove the caller means it. No default: sent only when it is the
    /// enumerator; `std::nullopt` refuses the command as `NotConfirmed` without sending. [RQ-AKM-069]
    enum class ConfirmDeleteSubFolder
    {
        IUnderstandThisDeletesTheFolderAndEverythingInIt,
    };

    /// Deletes the sub-folder named `name` of the current folder (§10/&17) — irreversible, and takes
    /// everything inside it with it. No real-sampler test of any feature calls this. [RQ-AKM-069]
    void deleteSubFolder(Session& session, std::string_view name, std::optional<ConfirmDeleteSubFolder> confirmation,
                        CommandCompletion completion);

    /// Passed to `deleteFile` to prove the caller means it. No default: sent only when it is the
    /// enumerator; `std::nullopt` refuses the command as `NotConfirmed` without sending. [RQ-AKM-069]
    enum class ConfirmDeleteFile
    {
        IUnderstandThisDeletesTheFile,
    };

    /// Deletes the file named `name` in the current folder (§10/&29) — irreversible. No real-sampler
    /// test of any feature calls this. [RQ-AKM-069]
    void deleteFile(Session& session, std::string_view name, std::optional<ConfirmDeleteFile> confirmation,
                   CommandCompletion completion);
}
