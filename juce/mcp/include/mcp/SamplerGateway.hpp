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

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "akm/CommandOptions.hpp"
#include "akm/Session.hpp"
#include "common/midi/MidiPorts.hpp"
#include "mcp/ParameterCatalogue.hpp"

namespace mcp
{
    // The sampler as the tools see it: blocking calls over an AKM session, in the vocabulary of the catalogue. The
    // session is opened the first time a call needs it, and again at the next call when the opening failed; a call
    // that cannot be carried out answers a problem in plain words rather than throwing. Of the commands that change what
    // the sampler stores it sends only the creation, the renaming and the deletion of the current program, in memory;
    // never anything that saves, loads, touches the disk or deletes everything (RQ-MCP-014). [RQ-MCP-003, RQ-MCP-005,
    // RQ-MCP-006, RQ-MCP-007, RQ-MCP-009, RQ-MCP-014, RQ-MCP-015, RQ-MCP-016, RQ-MCP-017, ADR-MCP-001 (DEC-MCP-003,
    // DEC-MCP-004, DEC-MCP-006), ADR-MCP-002 (DEC-MCP-010, DEC-MCP-011)]

    /// How the sampler's screen behaves while the server runs (`--screen`): `Independent` switches Sync LCD off and Auto screen update on
    /// (the assistant has its own selection, the screen is redrawn after each edit), `Follow` switches both on (the screen and the
    /// assistant's selection follow each other), `AsIs` changes neither and sends nothing about them. A setting the session changed is
    /// put back at the close to the sampler's documented default (Sync LCD on, Auto screen update off), not to what it held before.
    /// [RQ-MCP-045, ADR-MCP-004 (DEC-MCP-027)]
    enum class ScreenMode
    {
        Independent,
        Follow,
        AsIs
    };

    struct GatewayConfig
    {
        std::string inputPort;   ///< what the sampler sends on
        std::string outputPort;  ///< what the sampler receives on
        std::uint32_t deviceId = 0;
        std::chrono::milliseconds commandTimeout = std::chrono::duration_cast<std::chrono::milliseconds>(akm::DEFAULT_COMMAND_TIMEOUT);
        /// What the session does with the sampler's screen settings. [RQ-MCP-045]
        ScreenMode screen = ScreenMode::Independent;
        /// How long a slow section 10 command (the refresh of the disk list, a load, a save) waits for the sampler: much
        /// longer than an ordinary command, and its silence is answered with a message about the power cycle.
        /// [ADR-MCP-003 (DEC-MCP-017)]
        std::chrono::milliseconds diskTimeout{120000};
    };

    /// A value, or the reason there is none.
    template <typename T>
    struct Outcome
    {
        std::optional<T> value;
        std::string problem;

        [[nodiscard]] bool ok() const { return value.has_value(); }
        [[nodiscard]] static Outcome success(T result) { return Outcome{std::move(result), {}}; }
        [[nodiscard]] static Outcome failure(std::string why) { return Outcome{std::nullopt, std::move(why)}; }
    };

    struct SamplerStatus
    {
        std::uint32_t deviceId = 0;
        int programCount = 0;
        std::optional<std::string> currentProgram;
        std::optional<int> keygroupCount;  ///< of the current program
    };

    struct ProgramEntry
    {
        int index = 0;  ///< zero-based, the sampler's own order
        std::string name;
    };

    struct ProgramInfo
    {
        std::string name;
        int keygroupCount = 0;
    };

    struct ProgramRename
    {
        std::string oldName;
        std::string newName;  ///< as the sampler reports it after the renaming
    };

    /// What `deleteCurrentProgram` did: `deleted` is false when the confirmation did not match, and `name` is then the
    /// name of the current program.
    struct ProgramDeletion
    {
        bool deleted = false;
        std::string name;
        std::optional<int> remaining;  ///< programs left in memory, when the sampler said
    };

    /// One sample of the sampler's memory: its position (from 0, the sampler's own order, alphabetical like the programs'
    /// is not assumed) and its name.
    struct SampleEntry
    {
        int index = 0;
        std::string name;
    };

    /// The samples in memory, in memory order, and the position of the current one when there is one.
    /// The sample a zone (1 to 4) of a keygroup (1-based) plays; empty when the zone has none. [RQ-MCP-034]
    struct ZoneSampleEntry
    {
        int keygroup = 0;
        int zone = 0;
        std::string sample;
    };

    /// The zone samples of the current program, keygroup-major and zone-minor.
    struct ZoneSamples
    {
        std::string program;
        std::vector<ZoneSampleEntry> entries;
    };

    /// What adding or deleting keygroups did to the current program: its name and how many keygroups it has after. A deletion whose
    /// `confirm` was not the program's exact name sends nothing and has `done` false. [RQ-MCP-035, RQ-MCP-042]
    struct KeygroupChange
    {
        bool done = true;
        std::string program;
        int keygroupCount = 0;
    };

    /// A sample or a multi renamed: its name before and the name the sampler then gives it. [RQ-MCP-036, RQ-MCP-037]
    struct RenamedItem
    {
        std::string before;
        std::string after;
    };

    /// A deletion of the current sample or multi: `done` is false when `confirm` was not its exact name (nothing was sent);
    /// `remaining` is how many of that kind the memory holds after. [RQ-MCP-036, RQ-MCP-037, RQ-MCP-042]
    struct DeletedItem
    {
        bool done = true;
        std::string name;
        int remaining = 0;
    };

    /// A multi created: its name, its number of parts and how many multis the memory holds then. [RQ-MCP-037]
    struct MultiCreation
    {
        std::string name;
        int partCount = 0;
        int total = 0;
    };

    /// Which program a part is to play: its name or its position in the list of programs, exactly one of the two. [RQ-MCP-038]
    struct ProgramReference
    {
        std::optional<std::string> name;
        std::optional<int> position;
    };

    /// A part (from 1) of a multi and the program it plays; empty when it plays none. [RQ-MCP-038]
    struct PartProgram
    {
        int part = 1;
        std::string program;
    };

    /// A program assigned to a part of a multi, as read back. [RQ-MCP-038]
    struct PartAssignment
    {
        std::string multi;
        int part = 1;
        std::string program;
    };

    /// What the parts of the current multi play: only the parts that play a program are listed. [RQ-MCP-038]
    struct PartPrograms
    {
        std::string multi;
        int partCount = 0;
        std::vector<PartProgram> assigned;
    };

    /// A part cleared: `done` is false when `confirm` was not the multi's exact name (nothing was sent). [RQ-MCP-038, RQ-MCP-042]
    struct PartClearing
    {
        bool done = true;
        std::string multi;
        int part = 1;
        std::string previous;
    };

    /// The program number of the current multi as the sampler reports it after a change; empty when it is switched off. [RQ-MCP-038]
    struct MultiProgramNumber
    {
        std::string multi;
        std::optional<int> number;
    };

    struct SampleListing
    {
        std::vector<SampleEntry> samples;
        std::optional<int> current;
    };

    /// One disk connected to the sampler, as section 10 lists it. [RQ-MCP-024]
    struct DiskEntry
    {
        int handle = 0;
        std::string name;
        int type = 0;    ///< 0 floppy, 1 hard disk, 2 CD-ROM, 3 removable
        int format = 0;  ///< 0 other, 1 MSDOS, 2 FAT32, 3 ISO9660, 4 S1000, 5 S3000, 6 EMU, 7 ROLAND
        bool writable = false;
        bool current = false;
    };

    struct DiskFileEntry
    {
        std::string name;
        std::uint32_t sizeBytes = 0;
    };

    /// The current folder of the current disk: its path (empty at the root), its sub-folders and its files.
    struct DiskContents
    {
        std::string diskName;
        std::string path;
        std::vector<std::string> folders;
        std::vector<DiskFileEntry> files;
    };

    /// How a sample file is loaded (section 10, `&2A`): as the sampler decides, into RAM, or as a virtual sample.
    enum class SampleLoadMode
    {
        Normal,
        Ram,
        Virtual,
    };

    /// The names of what the sampler holds in memory, by kind.
    struct MemoryNames
    {
        std::vector<std::string> programs;
        std::vector<std::string> samples;
        std::vector<std::string> multis;
        std::vector<std::string> songFiles;
        std::vector<std::string> setLists;
        std::vector<std::string> sceneLists;
    };

    /// What a load did: the disk and folder it acted on and the memory before and after.
    struct LoadOutcome
    {
        std::string diskName;
        std::string path;
        MemoryNames before;
        MemoryNames after;
    };

    /// The kind of memory item a save writes to the disk.
    enum class SaveKind
    {
        Program,
        Sample,
        Multi,
        SongFile,
        SetList,
        SceneList,
    };

    /// What a save did: the disk and folder it acted on, the kind and name (one item) or the number of items (all), and the
    /// files of the folder before and after.
    struct SaveOutcome
    {
        std::string diskName;
        std::string path;
        std::string itemName;  ///< empty for a save of every item
        int itemCount = 1;
        std::vector<DiskFileEntry> filesBefore;
        std::vector<DiskFileEntry> filesAfter;
        std::optional<DiskFileEntry> savedFile;  ///< one item: the file that bears its name afterwards
    };

    /// What a listing after a change must show: a file or a folder of that name is there, or is gone. [RQ-MCP-039, RQ-MCP-033]
    enum class ListingCheck
    {
        FileThere,
        FileGone,
        FolderThere,
        FolderGone
    };

    /// What a rename or a deletion of a file or a folder did on the current disk: `done` is false when `confirm` was not the exact name
    /// or when a folder that is not empty was not allowed to be deleted with its contents (`notEmpty`, with what it holds); nothing was
    /// sent then. For a rename `newName` is the name the sampler gave. [RQ-MCP-039, RQ-MCP-042]
    struct DiskChange
    {
        bool done = true;
        bool notEmpty = false;
        std::string diskName;
        std::string path;
        std::string name;
        std::string newName;
        int files = 0;    ///< a folder's files, when it was looked at
        int folders = 0;  ///< a folder's sub-folders, when it was looked at
    };

    /// What the sampler says about itself: its model and operating system when they could be read, and how much of its memory is free.
    /// [RQ-MCP-040]
    struct SystemInfo
    {
        std::optional<std::string> model;       ///< "AKAI S5000" or "AKAI S6000"; empty when the sampler answered a code that is neither
        std::optional<std::string> osVersion;   ///< "major.minor"; empty when it could not be read
        int freeWavePercent = 0;
        std::uint32_t freeWaveBytes = 0;
        std::uint32_t totalWaveBytes = 0;
        int freeMpksPercent = 0;                ///< programs, keygroups, samples and multis
    };

    /// The sampler's clock and date as it holds them: `dayOfWeek` is 1 to 7 with 1 = Sunday, a field of its own for the sampler.
    /// [RQ-MCP-046]
    struct SamplerClock
    {
        int year = 0;
        int month = 0;
        int day = 0;
        int dayOfWeek = 0;
        int hours = 0;
        int minutes = 0;
        int seconds = 0;
    };

    /// The sampler's Play Mode; `Muted` plays nothing. [RQ-MCP-046]
    enum class SamplerPlayMode
    {
        Multi,
        Program,
        Sample,
        Muted,
    };

    /// The front-panel lock-out state: a locked panel is unlocked only by another Set. [RQ-MCP-046]
    enum class SamplerPanel
    {
        Normal,
        Locked,
    };

    /// The sampler's own settings, as it reports them. [RQ-MCP-046]
    struct SamplerSettings
    {
        std::string name;
        SamplerClock clock;
        /// Empty when the sampler answered an ERROR to the Get of the play mode, which a real S5000 does on some pages of its
        /// screen; `playModeProblem` then says why. [TASK-MCP-055]
        std::optional<SamplerPlayMode> playMode;
        std::string playModeProblem;
        SamplerPanel panel = SamplerPanel::Normal;
    };

    /// The switches of the sampler's MIDI setup (section 04). The section has no Get: what a switch held before cannot be read. [RQ-MCP-047]
    enum class MidiSwitch
    {
        ProgramChange,         ///< 0 off, 1 on
        MultiSelect,           ///< 0 off, 1 program change, 2 bank
        MultiSelectChannel,    ///< 0 to 31: 1A = 0 ... 16B = 31
        ExternalApmController, ///< 0 to 127
        Aftertouch,            ///< 0 channel, 1 polyphonic
    };

    /// The MIDI events a filter acts on, with the byte each travels as. [RQ-MCP-047]
    enum class MidiFilterKind
    {
        NoteOn = 0,
        Aftertouch = 1,
        Wheels = 2,
        Volume = 3,
    };

    /// The three lists of named things the sampler keeps besides its programs, samples and multis. Song files and scenelists have a
    /// current one; the sampler has no current set list. [RQ-MCP-048]
    enum class NamedListKind
    {
        SongFile,
        SetList,
        SceneList,
    };

    /// What a kind of list is called in the answers: "song file", "set list" or "scenelist". [RQ-MCP-048]
    [[nodiscard]] const char* namedListNoun(NamedListKind kind);

    /// One item of such a list: its position (from 0) and its name. [RQ-MCP-048]
    struct NamedListEntry
    {
        int index = 0;
        std::string name;
    };

    /// A list in the sampler's order, and the position of the current item when the kind has one and one is selected. [RQ-MCP-048]
    struct NamedListing
    {
        std::vector<NamedListEntry> entries;
        std::optional<int> current;
    };

    /// A set list renamed: its position, its name before and the name the sampler then gives it. [RQ-MCP-048]
    struct SetListRenaming
    {
        int index = 0;
        std::string before;
        std::string after;
    };

    /// The kinds of item the sampler can delete all of at once. [RQ-MCP-051]
    enum class BulkKind
    {
        Program,
        Sample,
        Multi,
    };

    /// A "delete all" of a kind: `done` is false when nothing was sent (the sampler held none, or `confirm` was not their number); `count` is
    /// how many items of the kind the sampler held when it was asked. [RQ-MCP-051]
    struct BulkDeletion
    {
        bool done = true;
        int count = 0;
    };

    /// The effects board the sampler reports (section 12). [RQ-MCP-053]
    enum class FxCardKind
    {
        None,
        Eb20,
    };

    /// The layout of the board: the card, and for each channel how many modules it has (empty when there is no board). [RQ-MCP-053]
    struct FxLayoutInfo
    {
        FxCardKind card = FxCardKind::None;
        std::vector<int> moduleCounts;
    };

    /// One module: the code of its type (Table 24) and whether it is enabled (not bypassed). [RQ-MCP-053]
    struct FxModuleState
    {
        int type = 0;
        bool enabled = true;
    };

    /// One channel: whether it is muted, and its modules in order. [RQ-MCP-053]
    struct FxChannelState
    {
        bool muted = false;
        std::vector<FxModuleState> modules;
    };

    /// The whole board of the current multi; `channels` is empty when the sampler reports no board. [RQ-MCP-053]
    struct FxBoardState
    {
        FxLayoutInfo layout;
        std::vector<FxChannelState> channels;
    };

    /// A Clear Sampler Memory: `done` is false when nothing was sent (the memory held none of the three, or `confirm` was not their total); the
    /// counts are what the sampler held when it was asked. [RQ-MCP-052]
    struct MemoryClearing
    {
        bool done = true;
        int programs = 0;
        int samples = 0;
        int multis = 0;

        [[nodiscard]] int total() const { return programs + samples + multis; }
    };

    /// The directions of the data wheel. [RQ-MCP-054]
    enum class WheelDirection
    {
        Forwards,
        Backwards,
    };

    /// The names of the front-panel keys a client may give, in the order of Table 31 of the specification: the mode keys, F1 to F16, the digits 0 to 9
    /// and the other keys. [RQ-MCP-054]
    [[nodiscard]] std::vector<std::string> panelKeyNames();

    /// A song file, set list or scenelist deletion: `done` is false when `confirm` was not its exact name (nothing was sent); `remaining` is what
    /// the list holds after. [RQ-MCP-049]
    struct NamedDeletion
    {
        bool done = true;
        int index = 0;
        std::string name;
        std::vector<NamedListEntry> remaining;
    };

    /// The free space of the current disk. [RQ-MCP-040]
    struct DiskSpace
    {
        std::string diskName;
        std::uint64_t freeBytes = 0;
    };

    /// One multi of the sampler's memory: its position (from 0) and its name.
    struct MultiEntry
    {
        int index = 0;
        std::string name;
        int partCount = 0;  ///< of the current multi, as reported when it is selected
    };

    /// The multis in memory, in memory order, the position of the current one when there is one, and its number of parts.
    struct MultiListing
    {
        std::vector<MultiEntry> multis;
        std::optional<int> current;
        std::optional<int> currentPartCount;
    };

    /// Which part (1 to the multi's part count) of the current multi an edit or a reading is about, or every part.
    struct PartSelection
    {
        std::optional<int> part;  ///< empty: all parts

        [[nodiscard]] static PartSelection all() { return {}; }
        [[nodiscard]] static PartSelection of(int part) { return PartSelection{part}; }
    };

    /// One value of a part parameter: the part is numbered from 1, as on the front panel (the wire's part number is one
    /// less).
    struct PartValue
    {
        int part = 1;
        std::int64_t value = 0;
    };

    /// Which keygroup an edit or a reading is about: one (1 to the program's count), or all of them.
    struct KeygroupSelection
    {
        std::optional<int> keygroup;  ///< empty: all keygroups

        [[nodiscard]] static KeygroupSelection all() { return {}; }
        [[nodiscard]] static KeygroupSelection of(int keygroup) { return KeygroupSelection{keygroup}; }
    };

    /// Which zone (1 to 4) of a keygroup an edit or a reading is about, or all four of them.
    struct ZoneSelection
    {
        std::optional<int> zone;  ///< empty: all zones

        [[nodiscard]] static ZoneSelection all() { return {}; }
        [[nodiscard]] static ZoneSelection of(int zone) { return ZoneSelection{zone}; }
    };

    /// One value of a parameter: of a zone (1-based) of a keygroup (1-based) for a zone parameter, of a keygroup for a
    /// keygroup parameter, or of the program.
    struct ParameterValue
    {
        std::optional<int> keygroup;
        std::optional<int> zone;
        std::int64_t value = 0;
    };

    /// Not thread-safe: one call at a time, from a thread that is neither the session's nor the MIDI backend's.
    class SamplerGateway
    {
    public:
        SamplerGateway(common::midi::MidiBackend& backend, GatewayConfig config);
        ~SamplerGateway();

        SamplerGateway(const SamplerGateway&) = delete;
        SamplerGateway& operator=(const SamplerGateway&) = delete;

        /// The number of programs and the current one, if any. [RQ-MCP-007]
        [[nodiscard]] Outcome<SamplerStatus> status();

        /// The names of the programs in memory, in memory order. [RQ-MCP-007]
        [[nodiscard]] Outcome<std::vector<ProgramEntry>> listPrograms();

        /// Makes a program current. A name or an index that no program has is a problem that says so. [RQ-MCP-007]
        [[nodiscard]] Outcome<ProgramInfo> selectProgramByName(std::string_view name);
        [[nodiscard]] Outcome<ProgramInfo> selectProgramByIndex(int index);

        /// Creates a program with `keygroups` keygroups in the sampler's memory; it becomes the current program, and
        /// the answer is what the sampler then reports. The name and the count must have been checked by the caller
        /// (a refusal of the AKM layer is a problem that says so). [RQ-MCP-015]
        [[nodiscard]] Outcome<ProgramInfo> createProgram(std::string_view name, int keygroups);

        /// Renames the current program; the answer holds the name before and the name read back. [RQ-MCP-016]
        [[nodiscard]] Outcome<ProgramRename> renameCurrentProgram(std::string_view name);

        /// Deletes the current program, and only when `confirm` is exactly the name the sampler reports for it at this
        /// moment; otherwise nothing is sent and the answer says which program is current. [RQ-MCP-017]
        [[nodiscard]] Outcome<ProgramDeletion> deleteCurrentProgram(std::string_view confirm);

        /// The samples in memory and the current one. [RQ-MCP-020]
        [[nodiscard]] Outcome<SampleListing> listSamples();

        /// Makes a sample current, by name or by position, and answers it. A name or a position that no sample has is a
        /// problem that says so. [RQ-MCP-020]
        [[nodiscard]] Outcome<SampleEntry> selectSampleByName(std::string_view name);
        [[nodiscard]] Outcome<SampleEntry> selectSampleByIndex(int index);

        /// The disks connected to the sampler, the current one marked. The sampler's refresh of its disk list is sent only
        /// when `refresh` is true: it is the command that hung a real S5000, and it waits for the disk timeout. [RQ-MCP-024]
        [[nodiscard]] Outcome<std::vector<DiskEntry>> listDisks(bool refresh);

        /// Makes a disk current, by name or by handle. A name or a handle that no disk has is a problem that lists the disks.
        /// [RQ-MCP-024]
        [[nodiscard]] Outcome<DiskEntry> selectDiskByName(std::string_view name);
        [[nodiscard]] Outcome<DiskEntry> selectDiskByHandle(int handle);

        /// What the current folder of the current disk holds. With no disk selected the problem says to select one.
        /// [RQ-MCP-024]
        [[nodiscard]] Outcome<DiskContents> listDiskContents();

        /// Descends into a sub-folder of the current folder, or goes back up one level, and answers the new contents; a
        /// folder that is not there, or the root, is a problem and nothing is sent. [RQ-MCP-024]
        [[nodiscard]] Outcome<DiskContents> openFolder(std::string_view name);
        [[nodiscard]] Outcome<DiskContents> closeFolder();

        /// Creates a sub-folder of the current folder of the current disk (§10/&16) and answers the folder's listing afterwards,
        /// without opening it. A disk that is not selected or not writable, a name a folder or a file of the folder already
        /// bears (compared without case, spaces or hyphens) are problems and nothing is sent. The name is checked in the
        /// listing afterwards. [RQ-MCP-032]
        [[nodiscard]] Outcome<DiskContents> createFolder(std::string_view name);

        /// Renames a file of the current folder of the current writable disk: `newName` is given without the extension, which the sampler
        /// keeps and adds (seen on the S5000); a new name that carries the extension, one that a file or a folder already bears, a file
        /// that is not there, a disk that is not selected or not writable are problems and nothing is sent. The new name is checked in
        /// the listing afterwards. [RQ-MCP-039]
        [[nodiscard]] Outcome<DiskChange> renameFile(std::string_view name, std::string_view newName);

        /// The same for a sub-folder of the current folder. [RQ-MCP-039]
        [[nodiscard]] Outcome<DiskChange> renameFolder(std::string_view name, std::string_view newName);

        /// Deletes a file of the current folder, and only when `confirm` is exactly the name the listing gives it: otherwise nothing is
        /// sent and `done` is false. The deletion is checked in the listing afterwards. [RQ-MCP-039, RQ-MCP-042]
        [[nodiscard]] Outcome<DiskChange> deleteFile(std::string_view name, std::string_view confirm);

        /// Deletes a sub-folder of the current folder, and only when `confirm` is exactly its name; a folder that holds files or folders
        /// is deleted only when `deleteContents` is true, otherwise nothing is sent, `done` and `notEmpty` say so and the counts are
        /// given (the folder is opened to count and closed again). [RQ-MCP-039, RQ-MCP-042]
        [[nodiscard]] Outcome<DiskChange> deleteFolder(std::string_view name, std::string_view confirm, bool deleteContents);

        /// Loads a file of the current folder of the current disk (its extension decides whether it is a program, a sample, a
        /// multi...), with the files it depends on when `withDependents`, and answers the memory before and after. The name
        /// must be one the folder lists, or nothing is sent. The command waits for the disk timeout; a silent sampler is
        /// answered with the message about the power cycle and nothing is retried. [RQ-MCP-025]
        [[nodiscard]] Outcome<LoadOutcome> loadFile(std::string_view name, bool withDependents, SampleLoadMode mode);

        /// Loads a sub-folder of the current folder and everything it holds, as `loadFile` does. [RQ-MCP-025]
        [[nodiscard]] Outcome<LoadOutcome> loadFolder(std::string_view name);

        /// Saves one item of the sampler's memory (found by its name) to the current folder of the current disk, which must
        /// be writable. Unless `overwrite`, a file bearing the item's name in the folder refuses the save and nothing is
        /// sent. The answer holds the folder's files before and after, and the file that bears the item's name. The command
        /// waits for the disk timeout, as a load does. [RQ-MCP-026]
        [[nodiscard]] Outcome<SaveOutcome> saveMemoryItem(SaveKind kind, std::string_view name, bool overwrite, bool saveChildren);

        /// Saves every item of a kind, only when `confirm` is the number of such items in memory; unless `overwrite`, files
        /// bearing their names refuse the save. [RQ-MCP-027]
        [[nodiscard]] Outcome<SaveOutcome> saveAllMemoryItems(SaveKind kind, int confirm, bool overwrite, bool saveChildren);

        /// The multis in memory and the current one with its number of parts. [RQ-MCP-021]
        [[nodiscard]] Outcome<MultiListing> listMultis();

        /// Makes a multi current, by name or by position, and answers it with its number of parts. A name or a position
        /// that no multi has is a problem that says so. [RQ-MCP-021]
        [[nodiscard]] Outcome<MultiEntry> selectMultiByName(std::string_view name);
        [[nodiscard]] Outcome<MultiEntry> selectMultiByIndex(int index);

        /// The value of a part parameter (a row of the multi catalogue) for one part of the current multi or for each
        /// part. A part beyond the multi's count is a problem that gives the count, and nothing is sent but the question
        /// that says so. With no current multi the problem says to select one. [RQ-MCP-021]
        [[nodiscard]] Outcome<std::vector<PartValue>> readMultiParameter(const ParameterDefinition& parameter, PartSelection parts);

        /// Sets a part parameter for one part or for every part (one Set per part) and reads it back; the answer is what
        /// the sampler reports for each. [RQ-MCP-021]
        [[nodiscard]] Outcome<std::vector<PartValue>> writeMultiParameter(const ParameterDefinition& parameter, std::int64_t value,
                                                                          PartSelection parts);

        /// The value of a parameter of the current sample (a row of the sample catalogue), or the problem. With no
        /// current sample the problem says to select one. [RQ-MCP-020]
        [[nodiscard]] Outcome<std::int64_t> readSampleParameter(const ParameterDefinition& parameter);

        /// Sets a parameter of the current sample (the value must have been resolved by the catalogue) and reads it back;
        /// the answer is what the sampler reports. A read-only parameter is a problem that says so and nothing is sent.
        /// [RQ-MCP-020]
        [[nodiscard]] Outcome<std::int64_t> writeSampleParameter(const ParameterDefinition& parameter, std::int64_t value);

        /// The value of a parameter of the current program, for one keygroup or for each. A keygroup beyond the
        /// program's is a problem that gives the count, and nothing is sent to the sampler but the questions that
        /// say so. A program parameter has no keygroup: its one value carries none. [RQ-MCP-005, RQ-MCP-007]
        [[nodiscard]] Outcome<std::vector<ParameterValue>> readParameter(const ParameterDefinition& parameter,
                                                                         KeygroupSelection selection,
                                                                         ZoneSelection zones = ZoneSelection::all());

        /// Sets a parameter of the current program (the value must have been resolved by the catalogue) and reads it
        /// back; the answer is what the sampler reports. A reading that differs from the value set is a problem that
        /// says so. The keygroup the sampler has selected is left as the edit set it. [RQ-MCP-006]
        [[nodiscard]] Outcome<std::vector<ParameterValue>> writeParameter(const ParameterDefinition& parameter,
                                                                          std::int64_t value, KeygroupSelection selection,
                                                                          ZoneSelection zones = ZoneSelection::all());

        /// The sample each zone plays, for one keygroup of the current program or for each; the keygroup the sampler has selected is
        /// left on the last one read. A keygroup the program does not have is a problem that gives the count. [RQ-MCP-034]
        [[nodiscard]] Outcome<ZoneSamples> readZoneSamples(KeygroupSelection selection);

        /// Assigns the sample named `sample` (found without regard to case, spaces or hyphens, and sent under the name the
        /// sampler lists) to zone `zone` (1 to 4) of keygroup `keygroup` (1-based) of the current program, and reads it back; a
        /// sample not in memory, a zone or a keygroup that does not exist is a problem and nothing is sent. [RQ-MCP-034]
        [[nodiscard]] Outcome<ZoneSamples> assignZoneSample(int keygroup, int zone, std::string_view sample);

        /// Adds `count` keygroups (1 up to the 99 a program can have) to the current program and answers how many it has then; more
        /// than it can take is a problem that gives what it has. [RQ-MCP-035]
        [[nodiscard]] Outcome<KeygroupChange> addKeygroups(int count);

        /// Deletes keygroup `keygroup` (1-based) of the current program, and only when `confirm` is exactly the program's name:
        /// otherwise nothing is sent and `done` is false. A keygroup the program does not have, and its last keygroup, are
        /// problems and nothing is sent. [RQ-MCP-035, RQ-MCP-042]
        [[nodiscard]] Outcome<KeygroupChange> deleteKeygroup(int keygroup, std::string_view confirm);

        /// Renames the current sample and reads the name back. No current sample, and a name that another sample bears (compared
        /// without regard to case, spaces or hyphens), are problems and nothing is sent. [RQ-MCP-036]
        [[nodiscard]] Outcome<RenamedItem> renameCurrentSample(std::string_view name);

        /// Deletes the current sample, and only when `confirm` is exactly its name: otherwise nothing is sent and `done` is false.
        /// [RQ-MCP-036, RQ-MCP-042]
        [[nodiscard]] Outcome<DeletedItem> deleteCurrentSample(std::string_view confirm);

        /// Creates a multi named `name`, which becomes the current multi. A name that another multi bears (compared without regard to
        /// case, spaces or hyphens) is a problem and nothing is sent. [RQ-MCP-037]
        [[nodiscard]] Outcome<MultiCreation> createMulti(std::string_view name);

        /// Renames the current multi and reads the name back; no current multi and a name another multi bears are problems and nothing
        /// is sent. [RQ-MCP-037]
        [[nodiscard]] Outcome<RenamedItem> renameCurrentMulti(std::string_view name);

        /// Deletes the current multi, and only when `confirm` is exactly its name: otherwise nothing is sent and `done` is false.
        /// [RQ-MCP-037, RQ-MCP-042]
        [[nodiscard]] Outcome<DeletedItem> deleteCurrentMulti(std::string_view confirm);

        /// The parts of the current multi that play a program, numbered from 1. [RQ-MCP-038]
        [[nodiscard]] Outcome<PartPrograms> readPartPrograms();

        /// Makes part `part` (from 1, up to the multi's part count) of the current multi play a program of the sampler's memory and
        /// reads the part back; a program or a part that does not exist is a problem and nothing is sent. [RQ-MCP-038]
        [[nodiscard]] Outcome<PartAssignment> assignPartProgram(int part, const ProgramReference& program);

        /// Removes the program of part `part` of the current multi, and only when `confirm` is exactly the multi's name; a part that
        /// plays nothing is a problem and nothing is sent. [RQ-MCP-038, RQ-MCP-042]
        [[nodiscard]] Outcome<PartClearing> clearPart(int part, std::string_view confirm);

        /// Sets the current multi's program number (1 to 128 as on the front panel) or switches it off, and reads it back. [RQ-MCP-038]
        [[nodiscard]] Outcome<MultiProgramNumber> setMultiProgramNumber(std::optional<int> number);

        /// The sampler's model, operating system and free memory (section 02). [RQ-MCP-040]
        [[nodiscard]] Outcome<SystemInfo> readSystemInfo();

        /// The sampler's name, clock, play mode and front-panel lock (section 02). An ERROR answered to the Get of the play mode leaves
        /// the play mode out, with the reason, instead of failing the whole read. [RQ-MCP-046, TASK-MCP-055]
        [[nodiscard]] Outcome<SamplerSettings> readSamplerSettings();

        /// Each of these sends one Set of section 02 and answers what the sampler reports afterwards for that setting. The caller checks
        /// the value first; the AKM layer refuses a name over 20 characters or beyond 7-bit ASCII, and a clock field out of range, and
        /// nothing is then sent. [RQ-MCP-046]
        [[nodiscard]] Outcome<std::string> setSamplerName(std::string_view name);
        [[nodiscard]] Outcome<SamplerClock> setSamplerClock(const SamplerClock& clock);
        [[nodiscard]] Outcome<SamplerPlayMode> setSamplerPlayMode(SamplerPlayMode mode);
        [[nodiscard]] Outcome<SamplerPanel> setSamplerPanel(SamplerPanel panel);

        /// Each of these sends one Set of section 04 and answers true once the sampler has accepted it; the section has no Get, so nothing
        /// is read back. The caller checks the value (the range is in `MidiSwitch`); the AKM layer refuses one out of range and nothing is
        /// then sent. [RQ-MCP-047]
        [[nodiscard]] Outcome<bool> setMidiSwitch(MidiSwitch which, int value);
        [[nodiscard]] Outcome<bool> setMidiFilter(MidiFilterKind event, int channel, bool allow);

        /// The song files, the set lists or the scenelists in the sampler's order, with the current one when the kind has one. The names are
        /// read one by one: the AKM layer has no read of them all for these kinds. [RQ-MCP-048]
        [[nodiscard]] Outcome<NamedListing> listNamedItems(NamedListKind kind);

        /// Makes a song file or a scenelist the current one, by name or by position (from 0), and answers the current one read back; a name
        /// or a position the sampler does not have is a problem and the selection is unchanged. A set list has no current one: refused.
        /// [RQ-MCP-048]
        [[nodiscard]] Outcome<NamedListEntry> selectNamedItemByName(NamedListKind kind, std::string_view name);
        [[nodiscard]] Outcome<NamedListEntry> selectNamedItemByIndex(NamedListKind kind, int index);

        /// Renames the current song file or scenelist and answers its name before and the name read back; nothing is sent when none is
        /// current or when another item of the list already bears the name (without regard to case, spaces or hyphens). [RQ-MCP-048]
        [[nodiscard]] Outcome<RenamedItem> renameCurrentNamedItem(NamedListKind kind, std::string_view newName);

        /// Renames the set list called `name` (exactly, else the only one that differs by case, spaces or hyphens only) and answers its
        /// position and the names; nothing is sent when no set list or several have that name, or when another already bears the new name.
        /// [RQ-MCP-048]
        [[nodiscard]] Outcome<SetListRenaming> renameSetList(std::string_view name, std::string_view newName);

        /// Deletes the current song file or scenelist, and only when `confirm` is exactly its name; otherwise nothing is sent and the answer
        /// says which one is current. Nothing is sent either when none is current. [RQ-MCP-049, RQ-MCP-042]
        [[nodiscard]] Outcome<NamedDeletion> deleteCurrentNamedItem(NamedListKind kind, std::string_view confirm);

        /// Deletes the set list found by `name` (as `renameSetList` finds it), and only when `confirm` is exactly its name. [RQ-MCP-049]
        [[nodiscard]] Outcome<NamedDeletion> deleteSetList(std::string_view name, std::string_view confirm);

        /// Deletes every program, sample or multi in memory, and only when `confirm` is exactly how many the sampler holds now (read just before);
        /// otherwise, and when it holds none, nothing is sent and the answer gives the count. [RQ-MCP-051, RQ-MCP-042]
        [[nodiscard]] Outcome<BulkDeletion> deleteAllMemoryItems(BulkKind kind, int confirm);

        /// The effects board of the current multi (section 12; the effects belong to the multi that is current). The card and the layout are read
        /// first and, when the sampler reports no board, nothing else is sent. A sampler with no current multi answers ERROR 04 to the rest, which
        /// is reported as "select a multi first". [RQ-MCP-053]
        [[nodiscard]] Outcome<FxLayoutInfo> readFxLayout();
        [[nodiscard]] Outcome<FxBoardState> readFxBoard();
        [[nodiscard]] Outcome<FxModuleState> readFxModule(int channel, int module);
        [[nodiscard]] Outcome<int> readFxParameter(int channel, int module, int parameter);

        /// Each of these sends the Set of section 12 and answers what the sampler then reports. The caller checks the values against the layout and
        /// the module's parameters first; the AKM layer refuses a number outside 0 to 127 and a value beyond 16383 in magnitude. [RQ-MCP-053]
        [[nodiscard]] Outcome<bool> setFxChannelMute(int channel, bool muted);
        [[nodiscard]] Outcome<FxModuleState> setFxModule(int channel, int module, std::optional<int> type, std::optional<bool> enabled);
        [[nodiscard]] Outcome<int> setFxParameter(int channel, int module, int parameter, int value);

        /// The front-panel keys (section 20), behind `--allow-front-panel`. A key is found by its name (`panelKeyNames`), in other letters or with a
        /// space or a hyphen for the underscore; an unknown one is a problem and nothing is sent. The sampler only QUEUES what it receives: `true`/the
        /// name answered means the command was accepted, not that the sampler acted on it. A key held is released when the session closes.
        /// [RQ-MCP-054, ADR-MCP-005 (DEC-MCP-032)]
        [[nodiscard]] Outcome<std::string> pressPanelKey(std::string_view key);
        [[nodiscard]] Outcome<std::string> holdPanelKey(std::string_view key);
        [[nodiscard]] Outcome<std::string> releasePanelKey(std::string_view key);
        [[nodiscard]] Outcome<bool> turnDataWheel(WheelDirection direction, int clicks);
        [[nodiscard]] Outcome<bool> sendAsciiKey(int ascii);

        /// Clears the sampler's memory (every program, sample and multi), and only when `confirm` is exactly how many of the three the sampler holds
        /// now (read just before); otherwise, and when it holds none, nothing is sent and the answer gives the counts. Song files, set lists and
        /// scenelists are not counted. [RQ-MCP-052, RQ-MCP-042]
        [[nodiscard]] Outcome<MemoryClearing> clearSamplerMemory(int confirm);

        /// The free space, in bytes, of the current disk; with no disk selected the problem says to select one. [RQ-MCP-040]
        [[nodiscard]] Outcome<DiskSpace> readDiskSpace();

        /// Starts the audition of the current sample and answers its name; no current sample is a problem and nothing is sent. The
        /// sample plays until the audition is stopped. [RQ-MCP-041]
        [[nodiscard]] Outcome<std::string> startSampleAudition();

        /// Stops the audition of the sample. [RQ-MCP-041]
        [[nodiscard]] Outcome<bool> stopSampleAudition();

        /// Starts the audition of a file of the current folder of the current disk (found without regard to case, spaces or hyphens) and
        /// answers the name the listing gives it; a file that is not there, or no disk selected, is a problem and nothing is sent. The
        /// file plays until the audition is stopped. [RQ-MCP-041]
        [[nodiscard]] Outcome<std::string> startFileAudition(std::string_view name);

        /// Stops the audition of the file. [RQ-MCP-041]
        [[nodiscard]] Outcome<bool> stopFileAudition();

        /// Closes the session, if one is open: the sampler's section 00 settings are put back, and what was and was not
        /// put back is answered (nothing when no session was open). Safe to call twice; the next call that needs the
        /// sampler opens a new session. [RQ-MCP-003]
        std::optional<akm::CloseResult> close();

    private:
        struct Connection;

        /// Opens the session if there is none; the reason when it cannot be.
        [[nodiscard]] std::optional<std::string> connect();
        std::optional<akm::CloseResult> disconnect();

        /// How long to wait for the completion of `commands` commands before giving up on a lost one.
        [[nodiscard]] std::chrono::milliseconds waitFor(int commands) const;

        /// Runs the commands in order with nothing between them, and answers the data of each one's reply (empty for a
        /// DONE); the first that does not succeed ends it with a sentence naming it by `steps`.
        [[nodiscard]] Outcome<std::vector<std::vector<std::uint8_t>>> runSequence(std::vector<akm::CommandRequest> requests,
                                                                                  const std::vector<std::string>& steps,
                                                                                  bool needsCurrentProgram,
                                                                                  const char* currentObject = "program");
        [[nodiscard]] Outcome<SampleEntry> currentSampleEntry();
        [[nodiscard]] Outcome<MultiEntry> currentMultiEntry();

        /// One read each of a song file, set list or scenelist list, on the open session. [RQ-MCP-048]
        [[nodiscard]] Outcome<int> readNamedCount(NamedListKind kind);
        /// How many programs, samples or multis the sampler holds. [RQ-MCP-051, RQ-MCP-052]
        [[nodiscard]] Outcome<int> readMemoryCount(BulkKind kind);
        [[nodiscard]] Outcome<std::string> readNamedName(NamedListKind kind, int index);
        /// The position of the current song file or scenelist; empty when none is current. [RQ-MCP-048]
        [[nodiscard]] Outcome<std::optional<int>> readCurrentNamedIndex(NamedListKind kind);
        [[nodiscard]] Outcome<NamedListEntry> currentNamedEntry(NamedListKind kind);

        /// One Get of section 02 each, on the open session. [RQ-MCP-046]
        [[nodiscard]] Outcome<std::string> readSamplerName();
        [[nodiscard]] Outcome<SamplerClock> readSamplerClock();
        /// `samplerError`, when given, receives the number of an ERROR the sampler answered. [TASK-MCP-055]
        [[nodiscard]] Outcome<SamplerPlayMode> readSamplerPlayMode(std::optional<std::uint16_t>* samplerError = nullptr);
        [[nodiscard]] Outcome<SamplerPanel> readSamplerPanel();

        /// The open session, for the units of the gateway; only called while a connection is open.
        [[nodiscard]] akm::Session& session();

        // The disk unit (SamplerGatewayDisk.cpp).
        /// How long to wait for a slow disk command, and what its options carry so that the session waits as long.
        [[nodiscard]] std::chrono::milliseconds waitForDisk() const;
        [[nodiscard]] akm::CommandOptions diskOptions() const;
        /// As `explain`, for a slow disk command: a timeout says the sampler may have to be switched off and on.
        [[nodiscard]] std::string explainDisk(const akm::CommandResult& outcome, const std::string& doing) const;
        [[nodiscard]] Outcome<std::vector<std::string>> folderNames();

        /// Closes and opens again the current folder (the root is opened again by the empty name) and lists it: the S5000 keeps a
        /// folder's file list until the folder is opened, and a save into a folder that already held a file does not refresh it
        /// (seen on 2026-10-06). [RQ-MCP-033]
        [[nodiscard]] Outcome<DiskContents> reopenCurrentFolder();

        /// Lists the current folder; when the listing does not show what the change did, the folder is closed and opened again and listed
        /// once more (the sampler keeps a folder's list until it is opened). The caller checks the answer again. [RQ-MCP-033]
        [[nodiscard]] Outcome<DiskContents> listAfterChange(ListingCheck check, std::string_view name);
        [[nodiscard]] Outcome<MemoryNames> memoryNames();
        /// The current folder when its disk is selected and writable, or the problem that says why a save cannot start.
        [[nodiscard]] Outcome<DiskContents> writableFolder();
        [[nodiscard]] Outcome<std::vector<PartValue>> editMultiParameter(const ParameterDefinition& parameter,
                                                                         std::optional<std::int64_t> valueToSet, PartSelection parts);
        [[nodiscard]] Outcome<int> keygroupCount();
        [[nodiscard]] Outcome<std::string> currentProgramName();
        [[nodiscard]] Outcome<ProgramInfo> currentProgramInfo();
        [[nodiscard]] Outcome<std::vector<ParameterValue>> editParameter(const ParameterDefinition& parameter,
                                                                         std::optional<std::int64_t> valueToSet,
                                                                         KeygroupSelection selection, ZoneSelection zones);

        GatewayConfig _config;
        common::midi::MidiBackend& _backend;
        std::unique_ptr<Connection> _connection;
    };
}
