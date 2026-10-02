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
#include "akm/harness/SimulatedSampler.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

#include <array>

#include "akm/ByteReader.hpp"
#include "akm/ByteWriter.hpp"
#include "akm/Checksum.hpp"
#include "akm/ItemCatalogue.hpp"
#include "akm/ItemDescriptor.hpp"
#include "akm/Protocol.hpp"
#include "akm/SamplerError.hpp"
#include "common/midi/MidiMessage.hpp"

namespace akm::harness
{
    namespace
    {
        using Bytes = std::vector<std::uint8_t>;

        // Appends `src` to `dest` with a plain loop rather than `dest.insert(dest.end(), src.begin(),
        // src.end())`: GCC 11's Release build (-O2, -Werror) false-positives -Wstringop-overread on
        // that range-insert (linux-x64-release-canary on PR #4; not reproduced in Debug or on
        // MSVC/Clang, and unaffected by an emptiness guard around the call) — a known GCC inlining
        // bug, not a real out-of-bounds read.
        template <typename Container, typename Range>
        void appendBytes(Container& dest, const Range& src)
        {
            for (const auto& byte : src)
                dest.push_back(byte);
        }

        // Reply IDs of spec Table 2.
        constexpr std::uint8_t REPLY_OK = 0x4F;
        constexpr std::uint8_t REPLY_DONE = 0x44;
        constexpr std::uint8_t REPLY_REPLY = 0x52;
        constexpr std::uint8_t REPLY_ERROR = 0x45;

        // Section §02 (system) and its two version items, spec Tables 6 and 7.
        constexpr std::uint8_t SECTION_SYSTEM = 0x02;
        constexpr std::uint8_t ITEM_OS_VERSION = 0x00;
        constexpr std::uint8_t ITEM_OS_SUB_VERSION = 0x01;
        // The spec says the sub-version is always zero for now (Table 6, footnote a).
        constexpr std::uint8_t OS_SUB_VERSION = 0;
        // The sampler name items of TASK-AKM-048 (RQ-AKM-052).
        constexpr std::uint8_t ITEM_SET_SAMPLER_NAME = 0x02;
        constexpr std::uint8_t ITEM_GET_SAMPLER_NAME = 0x03;
        constexpr std::size_t MAX_NAME_LENGTH = 20;
        // The model and memory items of TASK-AKM-049 (RQ-AKM-053).
        constexpr std::uint8_t ITEM_GET_SAMPLER_MODEL = 0x04;
        constexpr std::uint8_t ITEM_GET_WAVE_MEMORY_PERCENT = 0x30;
        constexpr std::uint8_t ITEM_GET_MPKS_MEMORY_PERCENT = 0x31;
        constexpr std::uint8_t ITEM_GET_WAVE_MEMORY_TOTAL = 0x33;
        constexpr std::uint8_t ITEM_GET_WAVE_MEMORY_FREE = 0x34;
        constexpr std::uint32_t PERCENT_BASE = 100;
        // The clock items of TASK-AKM-050 (RQ-AKM-054): the eight data bytes of Table 6 and the range of each
        // (year from the MSB and LSB of a compound word, then month, day of month, day of week, hours,
        // minutes, seconds).
        constexpr std::uint8_t ITEM_GET_CLOCK = 0x05;
        constexpr std::uint8_t ITEM_SET_CLOCK = 0x06;
        constexpr std::size_t CLOCK_DATA_SIZE = 8;
        constexpr int CLOCK_YEAR_MIN = 1980;
        constexpr int CLOCK_YEAR_MAX = 2079;
        struct ClockByteRange
        {
            std::uint8_t min;
            std::uint8_t max;
        };
        constexpr std::array<ClockByteRange, 6> CLOCK_BYTE_RANGES{{{1, 12}, {1, 31}, {1, 7}, {0, 23}, {0, 59}, {0, 59}}};
        constexpr std::size_t CLOCK_FIRST_BYTE_AFTER_YEAR = 2;
        // The Play Mode and front-panel lock items of TASK-AKM-051 (RQ-AKM-055). The Play Mode takes the four
        // bytes the item's text defines, 0-3 (the spec's column lists 0-2: sysex_spec.kb.md, errata); the lock
        // is a toggle.
        constexpr std::uint8_t ITEM_SET_PLAY_MODE = 0x10;
        constexpr std::uint8_t ITEM_SET_FRONT_PANEL_LOCK = 0x11;
        constexpr std::uint8_t ITEM_GET_PLAY_MODE = 0x20;
        constexpr std::uint8_t ITEM_GET_FRONT_PANEL_LOCK = 0x21;
        // The guarded Clear Sampler Memory of TASK-AKM-052 (RQ-AKM-056): it needs every kind of memory, so it
        // is executed by `execute` itself rather than by `executeSystem`.
        constexpr std::uint8_t ITEM_CLEAR_SAMPLER_MEMORY = 0x32;
        constexpr std::uint8_t ALL_MPKS_FREE_PERCENT = 100;

        // Section §00 and its items, spec Table 5 (there is no item 02).
        constexpr std::uint8_t SECTION_SYSEX_CONFIG = 0x00;
        constexpr std::uint8_t ITEM_QUERY = 0x00;
        constexpr std::uint8_t ITEM_NOTIFICATION = 0x01;
        constexpr std::uint8_t ITEM_SYNC_LCD = 0x03;
        constexpr std::uint8_t ITEM_CHECKSUM = 0x04;
        constexpr std::uint8_t ITEM_AUTO_SCREEN_UPDATE = 0x05;
        constexpr std::uint8_t ITEM_ECHO = 0x06;
        constexpr std::uint8_t ITEM_STILL_ALIVE = 0x07;

        // OS versions that introduced an item (spec, modification history).
        constexpr OsVersion SYNC_LCD_SINCE{2, 0};
        constexpr OsVersion STILL_ALIVE_SINCE{2, 10};

        // Section §0A (Program), spec Tables 13-14: the lifecycle and general-information items of
        // TASK-AKM-015 (RQ-AKM-021, RQ-AKM-023). Other §0A items answer ERROR 0 until their own lot.
        constexpr std::uint8_t SECTION_PROGRAM = 0x0A;
        constexpr std::uint8_t ITEM_CREATE_PROGRAM = 0x02;
        constexpr std::uint8_t ITEM_CREATE_PROGRAM_WITH_KEYGROUPS = 0x03;
        constexpr std::uint8_t ITEM_SELECT_PROGRAM_BY_NAME = 0x05;
        constexpr std::uint8_t ITEM_SELECT_PROGRAM_BY_INDEX = 0x06;
        constexpr std::uint8_t ITEM_DELETE_CURRENT_PROGRAM = 0x08;
        constexpr std::uint8_t ITEM_RENAME_CURRENT_PROGRAM = 0x09;
        constexpr std::uint8_t ITEM_GET_NUMBER_OF_PROGRAMS = 0x10;
        constexpr std::uint8_t ITEM_GET_CURRENT_PROGRAM_NAME = 0x13;
        // Structure/identity items of TASK-AKM-016 (RQ-AKM-022, RQ-AKM-023).
        constexpr std::uint8_t ITEM_SET_PROGRAM_NUMBER = 0x0A;
        constexpr std::uint8_t ITEM_ADD_KEYGROUPS = 0x0B;
        constexpr std::uint8_t ITEM_DELETE_KEYGROUP = 0x0C;
        constexpr std::uint8_t ITEM_SET_CROSSFADE = 0x0D;
        constexpr std::uint8_t ITEM_GET_PROGRAM_NUMBER = 0x11;
        constexpr std::uint8_t ITEM_GET_KEYGROUP_COUNT = 0x14;
        constexpr std::uint8_t ITEM_GET_CROSSFADE = 0x15;
        // General program information of TASK-AKM-017 (RQ-AKM-023).
        constexpr std::uint8_t ITEM_GET_INDEX = 0x12;
        constexpr std::uint8_t ITEM_GET_ALL_NUMBERS = 0x18;
        constexpr std::uint8_t ITEM_GET_ALL_NAMES = 0x19;
        // Destructive guard of TASK-AKM-023 (RQ-AKM-025).
        constexpr std::uint8_t ITEM_DELETE_ALL = 0x07;

        // Section §08 (Keygroup), spec Tables 11-12: keygroup selection of TASK-AKM-026 (RQ-AKM-028).
        // Other §08 items answer ERROR 0 until their own lot.
        constexpr std::uint8_t SECTION_KEYGROUP = 0x08;
        constexpr std::uint8_t ITEM_SELECT_KEYGROUP = 0x01;
        constexpr std::uint8_t ITEM_GET_CURRENT_KEYGROUP = 0x02;
        // A newly current program defaults to this keygroup (spec silent; TASK-AKM-026's assumption).
        constexpr int DEFAULT_CURRENT_KEYGROUP = 1;

        // Section §06 (keygroup zone), spec Tables 9 and 10: the non-sample Set/Get parameter items of
        // TASK-AKM-035 (RQ-AKM-034) and sample assignment by name (TASK-AKM-036, RQ-AKM-035).
        constexpr std::uint8_t SECTION_ZONE = 0x06;
        constexpr std::uint8_t ITEM_SET_ZONE_SAMPLE = 0x01;
        constexpr std::uint8_t ITEM_GET_ZONE_SAMPLE = 0x21;

        // Section §0E (Sample), spec Tables 18-19: the lifecycle items of TASK-AKM-040 (RQ-AKM-045),
        // plus &13/&14 pulled in early (RQ-AKM-047) the same way TASK-AKM-015 pulled in Program's
        // &10/&13, so this lot's own round trips have a Get to verify against. Other §0E items answer
        // ERROR 0 until their own task.
        constexpr std::uint8_t SECTION_SAMPLE = 0x0E;
        constexpr std::uint8_t ITEM_SELECT_SAMPLE_BY_NAME = 0x05;
        constexpr std::uint8_t ITEM_SELECT_SAMPLE_BY_INDEX = 0x06;
        constexpr std::uint8_t ITEM_DELETE_CURRENT_SAMPLE = 0x08;
        constexpr std::uint8_t ITEM_RENAME_CURRENT_SAMPLE = 0x09;
        constexpr std::uint8_t ITEM_START_SAMPLE_AUDITION = 0x0A;
        constexpr std::uint8_t ITEM_STOP_SAMPLE_AUDITION = 0x0B;
        constexpr std::uint8_t ITEM_GET_CURRENT_SAMPLE_INDEX = 0x13;
        constexpr std::uint8_t ITEM_GET_CURRENT_SAMPLE_NAME = 0x14;
        // Destructive guard of TASK-AKM-041 (RQ-AKM-046).
        constexpr std::uint8_t ITEM_DELETE_ALL_SAMPLES = 0x07;
        // General information of TASK-AKM-042 (RQ-AKM-047).
        constexpr std::uint8_t ITEM_GET_SAMPLE_COUNT = 0x10;
        constexpr std::uint8_t ITEM_GET_SAMPLE_NAME_BY_INDEX = 0x11;
        constexpr std::uint8_t ITEM_GET_ALL_SAMPLE_NAMES = 0x12;
        // Read-only parameters and grouped replies of TASK-AKM-044 (RQ-AKM-049).
        constexpr std::uint8_t ITEM_GET_SAMPLE_TYPE = 0x30;
        constexpr std::uint8_t ITEM_GET_SAMPLE_CHANNELS = 0x31;
        constexpr std::uint8_t ITEM_GET_SAMPLE_LENGTH = 0x32;
        constexpr std::uint8_t ITEM_GET_SAMPLE_RATE = 0x33;
        constexpr std::uint8_t ITEM_GET_ALL_BASIC_PARAMS = 0x34;
        constexpr std::uint8_t ITEM_GET_ALL_SETTABLE_PARAMS = 0x4B;
        // Compound double word items of TASK-AKM-043/044 (RQ-AKM-048/049): the settable-parameter Set
        // item codes this lot's &4B concatenates, in the order the spec's own grouped REPLY gives them.
        constexpr std::array<std::uint8_t, 8> SETTABLE_PARAM_SET_ITEMS{{0x20, 0x21, 0x22, 0x23, 0x24, 0x28, 0x29, 0x2A}};

        // Section §10 (Disk), spec Tables 20-21: disk discovery of TASK-AKM-057 (RQ-AKM-060). Other §10
        // items answer ERROR 0 until their own lot.
        constexpr std::uint8_t SECTION_DISK = 0x10;
        constexpr std::uint8_t ITEM_UPDATE_DISK_LIST = 0x01;
        constexpr std::uint8_t ITEM_SELECT_DISK = 0x02;
        constexpr std::uint8_t ITEM_TEST_DISK_VALID = 0x03;
        constexpr std::uint8_t ITEM_GET_DISK_COUNT = 0x04;
        constexpr std::uint8_t ITEM_GET_DISK_LIST = 0x05;
        // Selection and status items of TASK-AKM-058 (RQ-AKM-061).
        constexpr std::uint8_t ITEM_GET_CURRENT_DISK_TYPE = 0x06;
        constexpr std::uint8_t ITEM_GET_DISK_TYPE = 0x07;
        constexpr std::uint8_t ITEM_GET_CURRENT_DISK_HANDLE = 0x08;
        constexpr std::uint8_t ITEM_GET_CURRENT_DISK_PATH = 0x09;

        constexpr std::size_t ECHO_DATA_SIZE = 4;
        constexpr std::uint8_t TOGGLE_MAX = 1;
        constexpr std::size_t SECTION_AND_ITEM_SIZE = 2;
        constexpr std::size_t CHECKSUM_SIZE = 1;
        constexpr int MIN_REPLY_REPEAT = 1;
        constexpr std::uint8_t ERROR_NUMBER_MASK = 0x7F;

        const Bytes STILL_ALIVE_MESSAGE{common::midi::SYSEX_START, common::midi::SYSEX_END};

        // What executing a command answers: DONE, a REPLY with data, or an ERROR with its two bytes.
        struct Outcome
        {
            std::uint8_t replyId = REPLY_DONE;
            Bytes data;
        };

        Outcome done()
        {
            return Outcome{REPLY_DONE, {}};
        }

        Outcome reply(Bytes data)
        {
            return Outcome{REPLY_REPLY, std::move(data)};
        }

        Bytes errorData(std::uint16_t number)
        {
            return Bytes{static_cast<std::uint8_t>((number >> BITS_PER_DATA_BYTE) & ERROR_NUMBER_MASK),
                         static_cast<std::uint8_t>(number & ERROR_NUMBER_MASK)};
        }

        Outcome failure(std::uint16_t number)
        {
            return Outcome{REPLY_ERROR, errorData(number)};
        }

        // A toggle takes 0 or 1; nothing at all is an invalid message and another value is out of range.
        Outcome setToggle(const Bytes& data, bool& target)
        {
            if (data.empty())
                return failure(error_number::INVALID_FORMAT);
            if (data.front() > TOGGLE_MAX)
                return failure(error_number::OUT_OF_RANGE);
            target = data.front() == TOGGLE_MAX;
            return done();
        }

        // A compound double word (spec pp. 8-9): four 7-bit data bytes, most significant first — the
        // same shape the position/loop items of TASK-AKM-043 already split into separate catalogue
        // values, used to answer Length (&32), Rate (&33) and their place inside &34's grouped
        // REPLY (RQ-AKM-049) and the Wave memory byte counts of §02 (&33, &34, RQ-AKM-053) without a copy of
        // the four-argument split in the catalogue itself.
        void appendCompoundWord(akm::ByteWriter& writer, std::uint32_t value)
        {
            writer.appendByte((value >> 21) & 0x7F);
            writer.appendByte((value >> 14) & 0x7F);
            writer.appendByte((value >> 7) & 0x7F);
            writer.appendByte(value & 0x7F);
        }

        // The free percentage of the Wave memory, rounded down; 0 when the sampler holds none.
        std::uint8_t freeWavePercent(const SystemSetupState& system)
        {
            if (system.waveTotalBytes == 0)
                return 0;
            return static_cast<std::uint8_t>(static_cast<std::uint64_t>(system.waveFreeBytes) * PERCENT_BASE
                                             / system.waveTotalBytes);
        }

        // §02: the version items (RQ-AKM-044), the sampler name (RQ-AKM-052), the model and the memory
        // (RQ-AKM-053). A name with no terminator
        // is an invalid format; one past the 20 characters the S5000 keeps of a name is not stored longer.
        Outcome executeSystem(std::uint8_t item, const Bytes& data, const OsVersion& osVersion, SystemSetupState& system)
        {
            switch (item)
            {
                case ITEM_OS_VERSION:
                    return reply(Bytes{static_cast<std::uint8_t>(osVersion.major), static_cast<std::uint8_t>(osVersion.minor)});
                case ITEM_OS_SUB_VERSION:
                    return reply(Bytes{OS_SUB_VERSION});
                case ITEM_SET_SAMPLER_NAME:
                {
                    akm::ByteReader reader(data);
                    const auto name = reader.readString();
                    if (!name)
                        return failure(error_number::INVALID_FORMAT);
                    system.name = name->substr(0, MAX_NAME_LENGTH);
                    return done();
                }
                case ITEM_GET_SAMPLER_NAME:
                {
                    akm::ByteWriter writer;
                    writer.appendString(system.name);
                    return reply(writer.bytes());
                }
                case ITEM_SET_CLOCK:
                {
                    if (data.size() < CLOCK_DATA_SIZE)
                        return failure(error_number::INVALID_FORMAT);
                    const int year = (static_cast<int>(data[0]) << BITS_PER_DATA_BYTE) | data[1];
                    if (year < CLOCK_YEAR_MIN || year > CLOCK_YEAR_MAX)
                        return failure(error_number::OUT_OF_RANGE);
                    for (std::size_t index = 0; index < CLOCK_BYTE_RANGES.size(); ++index)
                    {
                        const std::uint8_t value = data[CLOCK_FIRST_BYTE_AFTER_YEAR + index];
                        if (value < CLOCK_BYTE_RANGES[index].min || value > CLOCK_BYTE_RANGES[index].max)
                            return failure(error_number::OUT_OF_RANGE);
                    }
                    std::copy_n(data.begin(), CLOCK_DATA_SIZE, system.clock.begin());
                    return done();
                }
                case ITEM_SET_PLAY_MODE:
                    if (data.empty())
                        return failure(error_number::INVALID_FORMAT);
                    if (data.front() > system.highestPlayMode)
                        return failure(error_number::OUT_OF_RANGE);
                    system.playMode = data.front();
                    return done();
                case ITEM_GET_PLAY_MODE:
                    return reply(Bytes{system.playMode});
                case ITEM_SET_FRONT_PANEL_LOCK:
                    if (data.empty())
                        return failure(error_number::INVALID_FORMAT);
                    if (data.front() > TOGGLE_MAX)
                        return failure(error_number::OUT_OF_RANGE);
                    system.frontPanelLock = data.front();
                    return done();
                case ITEM_GET_FRONT_PANEL_LOCK:
                    return reply(Bytes{system.frontPanelLock});
                case ITEM_GET_CLOCK:
                    return reply(Bytes(system.clock.begin(), system.clock.end()));
                case ITEM_GET_SAMPLER_MODEL:
                    return reply(Bytes{system.model});
                case ITEM_GET_WAVE_MEMORY_PERCENT:
                    return reply(Bytes{freeWavePercent(system)});
                case ITEM_GET_MPKS_MEMORY_PERCENT:
                    return reply(Bytes{system.mpksFreePercent});
                case ITEM_GET_WAVE_MEMORY_TOTAL:
                case ITEM_GET_WAVE_MEMORY_FREE:
                {
                    akm::ByteWriter writer;
                    appendCompoundWord(writer, item == ITEM_GET_WAVE_MEMORY_TOTAL ? system.waveTotalBytes
                                                                                : system.waveFreeBytes);
                    return reply(writer.bytes());
                }
                default:
                    return failure(error_number::NOT_SUPPORTED);
            }
        }

        // A name with no terminator is an invalid format; a byte after it (a checksum sent while checksums
        // are off) is ignored, as elsewhere in this model. One already held by another program cannot be
        // created again (the spec's own COULD_NOT_CREATE, undated by it).
        Outcome createProgram(const Bytes& data, int keygroupCount, std::vector<ProgramRecord>& programs,
                              std::optional<std::size_t>& current, std::optional<int>& currentKeygroup)
        {
            akm::ByteReader reader(data);
            const auto name = reader.readString();
            if (!name)
                return failure(error_number::INVALID_FORMAT);
            if (std::any_of(programs.begin(), programs.end(),
                            [&](const ProgramRecord& existing) { return existing.name == *name; }))
                return failure(error_number::COULD_NOT_CREATE);

            ProgramRecord record;
            record.name = *name;
            record.keygroupCount = keygroupCount;
            record.keygroups.assign(static_cast<std::size_t>(keygroupCount), KeygroupRecord{});
            programs.push_back(std::move(record));
            current = programs.size() - 1;
            currentKeygroup = DEFAULT_CURRENT_KEYGROUP;
            return done();
        }

        // Each Set item of the Output, MIDI/Tune, Pitch Bend, LFO and Keygroup Modulation Sources groups
        // (RQ-AKM-024) has its Get at a fixed offset within its group (spec Tables 13-14): a byte range,
        // not a per-item table, since the pattern is uniform within each group.
        struct ParameterGroupRange
        {
            std::uint8_t setFirst;
            std::uint8_t setLast;
            std::uint8_t offsetToGet;
        };
        constexpr std::array<ParameterGroupRange, 5> PARAMETER_GROUP_RANGES{{
            {0x20, 0x25, 0x08},  // Output
            {0x30, 0x34, 0x08},  // MIDI/Tune
            {0x40, 0x47, 0x08},  // Pitch Bend
            {0x50, 0x5F, 0x10},  // LFOs
            {0x70, 0x72, 0x04},  // Keygroup Modulation Sources
        }};

        std::size_t totalWidth(std::span<const akm::ValueSpec> values)
        {
            std::size_t total = 0;
            for (const akm::ValueSpec& value : values)
                total += akm::valueWidth(value.format);
            return total;
        }

        // A Set writes [selector bytes][value bytes] (the paired Get's own args and reply give their
        // widths); a Get reads back the value stored for its selector, or width-many zero bytes when
        // nothing was set yet. Generic over every item of the five groups: a new item is a new catalogue
        // record, not new code here either. An item outside all five ranges is not supported, whether or
        // not a program is current; one inside them needs a current program, checked only once it is
        // known to be recognised. [RQ-AKM-024, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]
        Outcome executeParameterGroup(std::uint8_t item, const Bytes& data, std::vector<ProgramRecord>& programs,
                                      std::optional<std::size_t>& current)
        {
            for (const ParameterGroupRange& range : PARAMETER_GROUP_RANGES)
            {
                if (item >= range.setFirst && item <= range.setLast)
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    ProgramRecord& program = programs[*current];
                    const auto getItem = static_cast<std::uint8_t>(item + range.offsetToGet);
                    const akm::ItemDescriptor* getDescriptor = akm::findItem(SECTION_PROGRAM, getItem);
                    if (getDescriptor == nullptr)
                        return failure(error_number::NOT_SUPPORTED);
                    const std::size_t selectorWidth = totalWidth(getDescriptor->args);
                    const std::size_t valueWidth = totalWidth(getDescriptor->reply);
                    if (data.size() < selectorWidth + valueWidth)
                        return failure(error_number::INVALID_FORMAT);
                    Bytes key(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(selectorWidth));
                    Bytes value(data.begin() + static_cast<std::ptrdiff_t>(selectorWidth),
                               data.begin() + static_cast<std::ptrdiff_t>(selectorWidth + valueWidth));
                    program.parameters[{item, std::move(key)}] = std::move(value);
                    return done();
                }
                const auto getFirst = static_cast<std::uint8_t>(range.setFirst + range.offsetToGet);
                const auto getLast = static_cast<std::uint8_t>(range.setLast + range.offsetToGet);
                if (item >= getFirst && item <= getLast)
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    const ProgramRecord& program = programs[*current];
                    const auto setItem = static_cast<std::uint8_t>(item - range.offsetToGet);
                    const akm::ItemDescriptor* getDescriptor = akm::findItem(SECTION_PROGRAM, item);
                    if (getDescriptor == nullptr)
                        return failure(error_number::NOT_SUPPORTED);
                    const std::size_t selectorWidth = totalWidth(getDescriptor->args);
                    const std::size_t valueWidth = totalWidth(getDescriptor->reply);
                    if (data.size() < selectorWidth)
                        return failure(error_number::INVALID_FORMAT);
                    const Bytes key(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(selectorWidth));
                    const auto found = program.parameters.find({setItem, key});
                    if (found != program.parameters.end())
                        return reply(found->second);
                    return reply(Bytes(valueWidth, 0));
                }
            }
            return failure(error_number::NOT_SUPPORTED);
        }

        Outcome executeProgram(std::uint8_t item, const Bytes& data, std::vector<ProgramRecord>& programs,
                               std::optional<std::size_t>& current, std::optional<int>& currentKeygroup)
        {
            constexpr int PLAIN_CREATE_KEYGROUP_COUNT = 1;
            switch (item)
            {
                case ITEM_CREATE_PROGRAM:
                    return createProgram(data, PLAIN_CREATE_KEYGROUP_COUNT, programs, current, currentKeygroup);
                case ITEM_CREATE_PROGRAM_WITH_KEYGROUPS:
                {
                    if (data.empty())
                        return failure(error_number::INVALID_FORMAT);
                    return createProgram(Bytes(data.begin() + 1, data.end()), data.front(), programs, current, currentKeygroup);
                }
                case ITEM_SELECT_PROGRAM_BY_NAME:
                {
                    akm::ByteReader reader(data);
                    const auto name = reader.readString();
                    if (!name)
                        return failure(error_number::INVALID_FORMAT);
                    const auto found = std::find_if(programs.begin(), programs.end(),
                                                    [&](const ProgramRecord& p) { return p.name == *name; });
                    if (found == programs.end())
                        return failure(error_number::NOT_FOUND);
                    current = static_cast<std::size_t>(found - programs.begin());
                    currentKeygroup = DEFAULT_CURRENT_KEYGROUP;
                    return done();
                }
                case ITEM_SELECT_PROGRAM_BY_INDEX:
                {
                    akm::ByteReader reader(data);
                    const auto index = reader.readWord();
                    if (!index)
                        return failure(error_number::INVALID_FORMAT);
                    if (*index >= programs.size())
                        return failure(error_number::NOT_FOUND);
                    current = *index;
                    currentKeygroup = DEFAULT_CURRENT_KEYGROUP;
                    return done();
                }
                case ITEM_DELETE_CURRENT_PROGRAM:
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    programs.erase(programs.begin() + static_cast<std::ptrdiff_t>(*current));
                    current.reset();
                    currentKeygroup.reset();
                    return done();
                case ITEM_DELETE_ALL:
                    programs.clear();
                    current.reset();
                    currentKeygroup.reset();
                    return done();
                case ITEM_RENAME_CURRENT_PROGRAM:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    akm::ByteReader reader(data);
                    const auto name = reader.readString();
                    if (!name)
                        return failure(error_number::INVALID_FORMAT);
                    programs[*current].name = *name;
                    return done();
                }
                case ITEM_SET_PROGRAM_NUMBER:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    if (data.empty())
                        return failure(error_number::INVALID_FORMAT);
                    if (data.front() == 0)
                    {
                        programs[*current].frontPanelNumber.reset();
                        return done();
                    }
                    if (data.size() < 2)
                        return failure(error_number::INVALID_FORMAT);
                    // The wire number is the front-panel one minus one (spec Table 13, footnote a).
                    programs[*current].frontPanelNumber = data[1] + 1;
                    return done();
                }
                case ITEM_ADD_KEYGROUPS:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    if (data.empty())
                        return failure(error_number::INVALID_FORMAT);
                    ProgramRecord& program = programs[*current];
                    program.keygroupCount += data.front();
                    program.keygroups.resize(program.keygroups.size() + data.front());
                    return done();
                }
                case ITEM_DELETE_KEYGROUP:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    if (data.empty())
                        return failure(error_number::INVALID_FORMAT);
                    ProgramRecord& program = programs[*current];
                    if (data.front() >= program.keygroupCount)
                        return failure(error_number::KEYGROUP_NOT_IN_PROGRAM);
                    --program.keygroupCount;
                    program.keygroups.erase(program.keygroups.begin() + data.front());
                    return done();
                }
                case ITEM_SET_CROSSFADE:
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    return setToggle(data, programs[*current].crossfade);
                case ITEM_GET_PROGRAM_NUMBER:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    const std::optional<int>& number = programs[*current].frontPanelNumber;
                    akm::ByteWriter writer;
                    writer.appendByte(number ? 1 : 0);
                    writer.appendByte(number ? static_cast<std::uint32_t>(*number - 1) : 0);
                    return reply(writer.bytes());
                }
                case ITEM_GET_KEYGROUP_COUNT:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    akm::ByteWriter writer;
                    writer.appendByte(static_cast<std::uint32_t>(programs[*current].keygroupCount));
                    return reply(writer.bytes());
                }
                case ITEM_GET_CROSSFADE:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    akm::ByteWriter writer;
                    writer.appendByte(programs[*current].crossfade ? 1 : 0);
                    return reply(writer.bytes());
                }
                case ITEM_GET_NUMBER_OF_PROGRAMS:
                {
                    akm::ByteWriter writer;
                    writer.appendWord(static_cast<std::uint32_t>(programs.size()));
                    return reply(writer.bytes());
                }
                case ITEM_GET_INDEX:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    akm::ByteWriter writer;
                    writer.appendWord(static_cast<std::uint32_t>(*current));
                    return reply(writer.bytes());
                }
                case ITEM_GET_ALL_NUMBERS:
                {
                    // Observed on a real S5000: ERROR 4 with 0 programs, not an empty REPLY, unlike &10
                    // (documents/_index/sysex_spec.kb.md, "Common value codes").
                    if (programs.empty())
                        return failure(error_number::NOT_FOUND);
                    // One (enabled, wire number) pair per program, memory order (Table 14, footnote b).
                    akm::ByteWriter writer;
                    for (const ProgramRecord& program : programs)
                    {
                        writer.appendByte(program.frontPanelNumber ? 1 : 0);
                        writer.appendByte(program.frontPanelNumber
                                              ? static_cast<std::uint32_t>(*program.frontPanelNumber - 1)
                                              : 0);
                    }
                    return reply(writer.bytes());
                }
                case ITEM_GET_ALL_NAMES:
                {
                    if (programs.empty())
                        return failure(error_number::NOT_FOUND);
                    akm::ByteWriter writer;
                    for (const ProgramRecord& program : programs)
                        writer.appendString(program.name);
                    return reply(writer.bytes());
                }
                case ITEM_GET_CURRENT_PROGRAM_NAME:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    akm::ByteWriter writer;
                    writer.appendString(programs[*current].name);
                    return reply(writer.bytes());
                }
                default:
                    return executeParameterGroup(item, data, programs, current);
            }
        }

        // §08 keygroup selection of TASK-AKM-026 (RQ-AKM-028): `&01` selects 1-99, or 0 for "all
        // keygroups"; `&02` gets which is current. Every other §08 item answers ERROR 0 until its own lot.
        // Each Set item of §08's parameter groups (RQ-AKM-030) has its Get at a fixed offset within its
        // group (spec Tables 11-12), the same contiguous-range shape as §0A's five (PARAMETER_GROUP_RANGES
        // above). One row per group, grown one at a time as TASK-AKM-027 to 032 catalogue each.
        constexpr std::array<ParameterGroupRange, 6> KEYGROUP_PARAMETER_GROUP_RANGES{{
            {0x04, 0x09, 0x06},  // General Options
            {0x10, 0x14, 0x08},  // Pitch/Amp
            {0x20, 0x25, 0x08},  // Filter
            {0x30, 0x38, 0x10},  // Filter Envelope
            {0x50, 0x57, 0x08},  // Amplitude Envelope
            {0x60, 0x64, 0x08},  // Aux Envelope
        }};

        // A Set while keygroup 0 ("all") is current writes every keygroup of the current program with the
        // same value (RQ-AKM-028's AC); a Get while keygroup 0 is current answers one value set per
        // keygroup, in keygroup order (RQ-AKM-031) — the client decodes that shape with
        // `decodeRepeatedReply` (DEC-AKM-015). Otherwise both act on the one selected keygroup, reading
        // back width-many zero bytes when nothing was set yet, exactly as `executeParameterGroup` does for
        // a program. [RQ-AKM-028, RQ-AKM-030, RQ-AKM-031]
        Outcome executeKeygroupParameterGroup(std::uint8_t item, const Bytes& data, std::vector<ProgramRecord>& programs,
                                              const std::optional<std::size_t>& currentProgram,
                                              const std::optional<int>& currentKeygroup)
        {
            for (const ParameterGroupRange& range : KEYGROUP_PARAMETER_GROUP_RANGES)
            {
                if (item >= range.setFirst && item <= range.setLast)
                {
                    if (!currentProgram || !currentKeygroup)
                        return failure(error_number::NOT_FOUND);
                    ProgramRecord& program = programs[*currentProgram];
                    const auto getItem = static_cast<std::uint8_t>(item + range.offsetToGet);
                    const akm::ItemDescriptor* getDescriptor = akm::findItem(SECTION_KEYGROUP, getItem);
                    if (getDescriptor == nullptr)
                        return failure(error_number::NOT_SUPPORTED);
                    const std::size_t selectorWidth = totalWidth(getDescriptor->args);
                    const std::size_t valueWidth = totalWidth(getDescriptor->reply);
                    if (data.size() < selectorWidth + valueWidth)
                        return failure(error_number::INVALID_FORMAT);
                    Bytes key(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(selectorWidth));
                    Bytes value(data.begin() + static_cast<std::ptrdiff_t>(selectorWidth),
                               data.begin() + static_cast<std::ptrdiff_t>(selectorWidth + valueWidth));
                    if (*currentKeygroup == 0)
                    {
                        for (KeygroupRecord& keygroup : program.keygroups)
                            keygroup.parameters[{item, key}] = value;
                    }
                    else
                    {
                        program.keygroups[static_cast<std::size_t>(*currentKeygroup - 1)].parameters[{item, std::move(key)}] =
                            std::move(value);
                    }
                    return done();
                }
                const auto getFirst = static_cast<std::uint8_t>(range.setFirst + range.offsetToGet);
                const auto getLast = static_cast<std::uint8_t>(range.setLast + range.offsetToGet);
                if (item >= getFirst && item <= getLast)
                {
                    if (!currentProgram || !currentKeygroup)
                        return failure(error_number::NOT_FOUND);
                    const ProgramRecord& program = programs[*currentProgram];
                    const auto setItem = static_cast<std::uint8_t>(item - range.offsetToGet);
                    const akm::ItemDescriptor* getDescriptor = akm::findItem(SECTION_KEYGROUP, item);
                    if (getDescriptor == nullptr)
                        return failure(error_number::NOT_SUPPORTED);
                    const std::size_t selectorWidth = totalWidth(getDescriptor->args);
                    const std::size_t valueWidth = totalWidth(getDescriptor->reply);
                    if (data.size() < selectorWidth)
                        return failure(error_number::INVALID_FORMAT);
                    const Bytes key(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(selectorWidth));
                    const auto valueOf = [&](const KeygroupRecord& keygroup) {
                        const auto found = keygroup.parameters.find({setItem, key});
                        return found != keygroup.parameters.end() ? found->second : Bytes(valueWidth, 0);
                    };
                    if (*currentKeygroup == 0)
                    {
                        Bytes concatenated;
                        for (const KeygroupRecord& keygroup : program.keygroups)
                        {
                            const Bytes value = valueOf(keygroup);
                            appendBytes(concatenated, value);
                        }
                        return reply(std::move(concatenated));
                    }
                    return reply(valueOf(program.keygroups[static_cast<std::size_t>(*currentKeygroup - 1)]));
                }
            }
            return failure(error_number::NOT_SUPPORTED);
        }

        Outcome executeKeygroup(std::uint8_t item, const Bytes& data, std::vector<ProgramRecord>& programs,
                                std::optional<std::size_t>& currentProgram, std::optional<int>& currentKeygroup)
        {
            switch (item)
            {
                case ITEM_SELECT_KEYGROUP:
                {
                    if (!currentProgram)
                        return failure(error_number::NOT_FOUND);
                    if (data.empty())
                        return failure(error_number::INVALID_FORMAT);
                    const int keygroup = data.front();
                    if (keygroup != 0 && keygroup > programs[*currentProgram].keygroupCount)
                        return failure(error_number::KEYGROUP_NOT_IN_PROGRAM);
                    currentKeygroup = keygroup;
                    return done();
                }
                case ITEM_GET_CURRENT_KEYGROUP:
                {
                    if (!currentProgram || !currentKeygroup)
                        return failure(error_number::NOT_FOUND);
                    akm::ByteWriter writer;
                    writer.appendByte(static_cast<std::uint32_t>(*currentKeygroup));
                    return reply(writer.bytes());
                }
                default:
                    return executeKeygroupParameterGroup(item, data, programs, currentProgram, currentKeygroup);
            }
        }

        // §06 zone parameters (RQ-AKM-034): the 13 non-sample Set/Get items (Level..Solo; &01 Sample is
        // TASK-AKM-036's own String path), the same contiguous-range shape as §0A/§08 above, but stored in
        // `KeygroupRecord::zoneParameters` rather than `parameters` (§06 and §08 item codes overlap) and
        // keyed by the zone number the item's own args/reply already carry as their first byte. Zone 0
        // ("all four") fans out over zones 1-4 exactly as keygroup 0 already fans out over
        // `program.keygroups` — the two dimensions combine, so keygroup 0 + zone 0 writes or reads every
        // zone of every keygroup, keygroup-major (RQ-AKM-036, TASK-AKM-037): the shape
        // `getForAllZonesAllKeygroups` expects from `decodeRepeatedReply` on the client side.
        // [TASK-AKM-035, TASK-AKM-037]
        constexpr std::array<ParameterGroupRange, 1> ZONE_PARAMETER_GROUP_RANGES{{
            {0x02, 0x0E, 0x20},  // Level .. Solo
        }};
        constexpr std::array<std::uint8_t, 4> EVERY_ZONE{{1, 2, 3, 4}};

        Outcome executeZoneParameterGroup(std::uint8_t item, const Bytes& data, std::vector<ProgramRecord>& programs,
                                          const std::optional<std::size_t>& currentProgram,
                                          const std::optional<int>& currentKeygroup)
        {
            for (const ParameterGroupRange& range : ZONE_PARAMETER_GROUP_RANGES)
            {
                if (item >= range.setFirst && item <= range.setLast)
                {
                    if (!currentProgram || !currentKeygroup)
                        return failure(error_number::NOT_FOUND);
                    ProgramRecord& program = programs[*currentProgram];
                    const auto getItem = static_cast<std::uint8_t>(item + range.offsetToGet);
                    const akm::ItemDescriptor* getDescriptor = akm::findItem(SECTION_ZONE, getItem);
                    if (getDescriptor == nullptr)
                        return failure(error_number::NOT_SUPPORTED);
                    const std::size_t selectorWidth = totalWidth(getDescriptor->args);
                    const std::size_t valueWidth = totalWidth(getDescriptor->reply);
                    if (data.size() < selectorWidth + valueWidth)
                        return failure(error_number::INVALID_FORMAT);
                    const std::uint8_t zone = data[0];
                    const Bytes value(data.begin() + static_cast<std::ptrdiff_t>(selectorWidth),
                                      data.begin() + static_cast<std::ptrdiff_t>(selectorWidth + valueWidth));
                    const auto writeToKeygroup = [&](KeygroupRecord& keygroup) {
                        if (zone == 0)
                        {
                            for (const std::uint8_t z : EVERY_ZONE)
                                keygroup.zoneParameters[{item, Bytes{z}}] = value;
                        }
                        else
                        {
                            keygroup.zoneParameters[{item, Bytes{zone}}] = value;
                        }
                    };
                    if (*currentKeygroup == 0)
                    {
                        for (KeygroupRecord& keygroup : program.keygroups)
                            writeToKeygroup(keygroup);
                    }
                    else
                    {
                        writeToKeygroup(program.keygroups[static_cast<std::size_t>(*currentKeygroup - 1)]);
                    }
                    return done();
                }
                const auto getFirst = static_cast<std::uint8_t>(range.setFirst + range.offsetToGet);
                const auto getLast = static_cast<std::uint8_t>(range.setLast + range.offsetToGet);
                if (item >= getFirst && item <= getLast)
                {
                    if (!currentProgram || !currentKeygroup)
                        return failure(error_number::NOT_FOUND);
                    const ProgramRecord& program = programs[*currentProgram];
                    const auto setItem = static_cast<std::uint8_t>(item - range.offsetToGet);
                    const akm::ItemDescriptor* getDescriptor = akm::findItem(SECTION_ZONE, item);
                    if (getDescriptor == nullptr)
                        return failure(error_number::NOT_SUPPORTED);
                    const std::size_t selectorWidth = totalWidth(getDescriptor->args);
                    const std::size_t valueWidth = totalWidth(getDescriptor->reply);
                    if (data.size() < selectorWidth)
                        return failure(error_number::INVALID_FORMAT);
                    const std::uint8_t zone = data[0];
                    const auto valueOf = [&](const KeygroupRecord& keygroup, std::uint8_t z) {
                        const auto found = keygroup.zoneParameters.find({setItem, Bytes{z}});
                        return found != keygroup.zoneParameters.end() ? found->second : Bytes(valueWidth, 0);
                    };
                    const auto readFromKeygroup = [&](const KeygroupRecord& keygroup) {
                        if (zone != 0)
                            return valueOf(keygroup, zone);
                        Bytes concatenated;
                        for (const std::uint8_t z : EVERY_ZONE)
                        {
                            const Bytes value = valueOf(keygroup, z);
                            appendBytes(concatenated, value);
                        }
                        return concatenated;
                    };
                    if (*currentKeygroup == 0)
                    {
                        Bytes concatenated;
                        for (const KeygroupRecord& keygroup : program.keygroups)
                        {
                            const Bytes value = readFromKeygroup(keygroup);
                            appendBytes(concatenated, value);
                        }
                        return reply(std::move(concatenated));
                    }
                    return reply(readFromKeygroup(program.keygroups[static_cast<std::size_t>(*currentKeygroup - 1)]));
                }
            }
            return failure(error_number::NOT_SUPPORTED);
        }

        // §06/&01 (Set Zone Sample) and &21 (Get Zone Sample), RQ-AKM-035: a String value, so it is
        // decoded/encoded by hand here rather than through the generic byte-width mechanism
        // `executeZoneParameterGroup` uses for the other 13 items — the same reason `setZoneSample`
        // itself does not go through `makeStringRequest` (ADR-AKM-001, DEC-AKM-013). `sampleNames` models
        // the sampler's own sample memory: assigning a name not in it fails as ERROR 04 ("requested item
        // not found"), the spec's own wording for the case. Stored in `zoneParameters` too, keyed by the
        // Set item so a Get finds it the same way, value bytes being the encoded name plus its `00`
        // terminator — so an unset zone's default (`Bytes{0}`, a lone terminator) already decodes as the
        // spec's "no sample assigned" REPLY without a separate code path. Keygroup 0 ("all") is not
        // modelled for this item: not exercised by RQ-AKM-035's own acceptance criteria, unlike the
        // numeric zone items, which get it "for free" from `executeZoneParameterGroup`'s shared code.
        Outcome executeZone(std::uint8_t item, const Bytes& data, std::vector<ProgramRecord>& programs,
                            const std::optional<std::size_t>& currentProgram, const std::optional<int>& currentKeygroup,
                            const std::vector<std::string>& sampleNames)
        {
            switch (item)
            {
                case ITEM_SET_ZONE_SAMPLE:
                {
                    if (!currentProgram || !currentKeygroup || *currentKeygroup == 0)
                        return failure(error_number::NOT_FOUND);
                    akm::ByteReader reader(data);
                    const auto zone = reader.readByte();
                    const auto name = reader.readString();
                    if (!zone || !name)
                        return failure(error_number::INVALID_FORMAT);
                    if (std::find(sampleNames.begin(), sampleNames.end(), *name) == sampleNames.end())
                        return failure(error_number::NOT_FOUND);
                    akm::ByteWriter writer;
                    writer.appendString(*name);
                    programs[*currentProgram].keygroups[static_cast<std::size_t>(*currentKeygroup - 1)]
                        .zoneParameters[{ITEM_SET_ZONE_SAMPLE, Bytes{*zone}}] = writer.bytes();
                    return done();
                }
                case ITEM_GET_ZONE_SAMPLE:
                {
                    if (!currentProgram || !currentKeygroup || *currentKeygroup == 0)
                        return failure(error_number::NOT_FOUND);
                    if (data.empty())
                        return failure(error_number::INVALID_FORMAT);
                    const KeygroupRecord& keygroup =
                        programs[*currentProgram].keygroups[static_cast<std::size_t>(*currentKeygroup - 1)];
                    const auto found = keygroup.zoneParameters.find({ITEM_SET_ZONE_SAMPLE, Bytes{data.front()}});
                    return reply(found != keygroup.zoneParameters.end() ? found->second : Bytes{0});
                }
                default:
                    return executeZoneParameterGroup(item, data, programs, currentProgram, currentKeygroup);
            }
        }

        // Each Set item of the §0E settable-parameter group (RQ-AKM-048) has its Get at a fixed offset
        // (spec Tables 18-19), the same shape as Program's own PARAMETER_GROUP_RANGES — none of these
        // items take a selector (they all act on the current sample), the same "no selector" shape as
        // Program's Output group, so the generic function below already handles it without change.
        constexpr std::array<ParameterGroupRange, 2> SAMPLE_PARAMETER_GROUP_RANGES{{
            {0x20, 0x24, 0x20},  // Start/End Position, Original Pitch, Semitone/Fine Tune
            {0x28, 0x2A, 0x20},  // Playback Mode, Loop Start/End
        }};

        // Mirrors executeParameterGroup (Program) exactly, substituted for the current sample instead of
        // the current program: a Set writes [value bytes] (no selector), a Get reads back the value
        // stored, or width-many zero bytes when nothing was set yet. [RQ-AKM-048, ADR-AKM-001
        // (DEC-AKM-003, DEC-AKM-012)]
        Outcome executeSampleParameterGroup(std::uint8_t item, const Bytes& data, std::vector<SampleRecord>& samples,
                                            const std::optional<std::size_t>& current)
        {
            for (const ParameterGroupRange& range : SAMPLE_PARAMETER_GROUP_RANGES)
            {
                if (item >= range.setFirst && item <= range.setLast)
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    SampleRecord& sample = samples[*current];
                    const auto getItem = static_cast<std::uint8_t>(item + range.offsetToGet);
                    const akm::ItemDescriptor* getDescriptor = akm::findItem(SECTION_SAMPLE, getItem);
                    if (getDescriptor == nullptr)
                        return failure(error_number::NOT_SUPPORTED);
                    const std::size_t selectorWidth = totalWidth(getDescriptor->args);
                    const std::size_t valueWidth = totalWidth(getDescriptor->reply);
                    if (data.size() < selectorWidth + valueWidth)
                        return failure(error_number::INVALID_FORMAT);
                    Bytes key(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(selectorWidth));
                    Bytes value(data.begin() + static_cast<std::ptrdiff_t>(selectorWidth),
                               data.begin() + static_cast<std::ptrdiff_t>(selectorWidth + valueWidth));
                    sample.parameters[{item, std::move(key)}] = std::move(value);
                    return done();
                }
                const auto getFirst = static_cast<std::uint8_t>(range.setFirst + range.offsetToGet);
                const auto getLast = static_cast<std::uint8_t>(range.setLast + range.offsetToGet);
                if (item >= getFirst && item <= getLast)
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    const SampleRecord& sample = samples[*current];
                    const auto setItem = static_cast<std::uint8_t>(item - range.offsetToGet);
                    const akm::ItemDescriptor* getDescriptor = akm::findItem(SECTION_SAMPLE, item);
                    if (getDescriptor == nullptr)
                        return failure(error_number::NOT_SUPPORTED);
                    const std::size_t selectorWidth = totalWidth(getDescriptor->args);
                    const std::size_t valueWidth = totalWidth(getDescriptor->reply);
                    if (data.size() < selectorWidth)
                        return failure(error_number::INVALID_FORMAT);
                    const Bytes key(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(selectorWidth));
                    const auto found = sample.parameters.find({setItem, key});
                    if (found != sample.parameters.end())
                        return reply(found->second);
                    return reply(Bytes(valueWidth, 0));
                }
            }
            return failure(error_number::NOT_SUPPORTED);
        }

        // §0E sample lifecycle (RQ-AKM-045): select by name/index, delete/rename the current sample,
        // start/stop auditioning it, plus &13/&14 (RQ-AKM-047, pulled in early — see the constants
        // above). §0E has its own sampler-wide "current sample" selection state, the same pattern as
        // §0A's current program (documents/_index/sysex_spec.kb.md line 91), not the zone-number-as-
        // first-byte pattern of §06. Unlike a program, a sample cannot be created here: `samples` is
        // only ever seeded by `setSampleNames`. Audition is assumed to need a current sample, like every
        // other "act on the current one" item of this section (spec silent either way). [TASK-AKM-040]
        Outcome executeSample(std::uint8_t item, const Bytes& data, std::vector<SampleRecord>& samples,
                              std::optional<std::size_t>& current)
        {
            switch (item)
            {
                case ITEM_SELECT_SAMPLE_BY_NAME:
                {
                    akm::ByteReader reader(data);
                    const auto name = reader.readString();
                    if (!name)
                        return failure(error_number::INVALID_FORMAT);
                    const auto found = std::find_if(samples.begin(), samples.end(),
                                                    [&](const SampleRecord& s) { return s.name == *name; });
                    if (found == samples.end())
                        return failure(error_number::NOT_FOUND);
                    current = static_cast<std::size_t>(found - samples.begin());
                    return done();
                }
                case ITEM_SELECT_SAMPLE_BY_INDEX:
                {
                    akm::ByteReader reader(data);
                    const auto index = reader.readWord();
                    if (!index)
                        return failure(error_number::INVALID_FORMAT);
                    if (*index >= samples.size())
                        return failure(error_number::NOT_FOUND);
                    current = *index;
                    return done();
                }
                case ITEM_DELETE_CURRENT_SAMPLE:
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    samples.erase(samples.begin() + static_cast<std::ptrdiff_t>(*current));
                    current.reset();
                    return done();
                case ITEM_RENAME_CURRENT_SAMPLE:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    akm::ByteReader reader(data);
                    const auto name = reader.readString();
                    if (!name)
                        return failure(error_number::INVALID_FORMAT);
                    samples[*current].name = *name;
                    return done();
                }
                case ITEM_START_SAMPLE_AUDITION:
                case ITEM_STOP_SAMPLE_AUDITION:
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    return done();
                case ITEM_GET_CURRENT_SAMPLE_INDEX:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    akm::ByteWriter writer;
                    writer.appendWord(static_cast<std::uint32_t>(*current));
                    return reply(writer.bytes());
                }
                case ITEM_GET_CURRENT_SAMPLE_NAME:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    akm::ByteWriter writer;
                    writer.appendString(samples[*current].name);
                    return reply(writer.bytes());
                }
                case ITEM_DELETE_ALL_SAMPLES:
                    samples.clear();
                    current.reset();
                    return done();
                case ITEM_GET_SAMPLE_COUNT:
                {
                    akm::ByteWriter writer;
                    writer.appendWord(static_cast<std::uint32_t>(samples.size()));
                    return reply(writer.bytes());
                }
                case ITEM_GET_SAMPLE_NAME_BY_INDEX:
                {
                    akm::ByteReader reader(data);
                    const auto index = reader.readWord();
                    if (!index)
                        return failure(error_number::INVALID_FORMAT);
                    if (*index >= samples.size())
                        return failure(error_number::NOT_FOUND);
                    akm::ByteWriter writer;
                    writer.appendString(samples[*index].name);
                    return reply(writer.bytes());
                }
                case ITEM_GET_ALL_SAMPLE_NAMES:
                {
                    akm::ByteWriter writer;
                    for (const SampleRecord& sample : samples)
                        writer.appendString(sample.name);
                    return reply(writer.bytes());
                }
                case ITEM_GET_SAMPLE_TYPE:
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    return reply(Bytes{samples[*current].type});
                case ITEM_GET_SAMPLE_CHANNELS:
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    return reply(Bytes{samples[*current].channels});
                case ITEM_GET_SAMPLE_LENGTH:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    akm::ByteWriter writer;
                    appendCompoundWord(writer, samples[*current].length);
                    return reply(writer.bytes());
                }
                case ITEM_GET_SAMPLE_RATE:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    akm::ByteWriter writer;
                    appendCompoundWord(writer, samples[*current].rate);
                    return reply(writer.bytes());
                }
                case ITEM_GET_ALL_BASIC_PARAMS:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    const SampleRecord& sample = samples[*current];
                    akm::ByteWriter writer;
                    writer.appendByte(sample.type);
                    writer.appendByte(sample.channels);
                    appendCompoundWord(writer, sample.length);
                    appendCompoundWord(writer, sample.rate);
                    return reply(writer.bytes());
                }
                case ITEM_GET_ALL_SETTABLE_PARAMS:
                {
                    if (!current)
                        return failure(error_number::NOT_FOUND);
                    const SampleRecord& sample = samples[*current];
                    Bytes concatenated;
                    for (const std::uint8_t setItem : SETTABLE_PARAM_SET_ITEMS)
                    {
                        const akm::ItemDescriptor* getDescriptor =
                            akm::findItem(SECTION_SAMPLE, static_cast<std::uint8_t>(setItem + 0x20));
                        const std::size_t valueWidth = getDescriptor == nullptr ? 0 : totalWidth(getDescriptor->reply);
                        const auto found = sample.parameters.find({setItem, Bytes{}});
                        const Bytes value = found != sample.parameters.end() ? found->second : Bytes(valueWidth, 0);
                        appendBytes(concatenated, value);
                    }
                    return reply(std::move(concatenated));
                }
                default:
                    return executeSampleParameterGroup(item, data, samples, current);
            }
        }

        // §02/&32: every program, multi and sample is deleted, and the memory they held is free again — what
        // the Wave memory and MPKS memory Gets then report (the spec says nothing of it; a modelling choice).
        // [RQ-AKM-056]
        Outcome executeClearMemory(SystemSetupState& system, std::vector<ProgramRecord>& programs,
                                 std::optional<std::size_t>& currentProgram, std::optional<int>& currentKeygroup,
                                 std::vector<SampleRecord>& samples, std::optional<std::size_t>& currentSample,
                                 std::vector<std::string>& multis)
        {
            programs.clear();
            currentProgram.reset();
            currentKeygroup.reset();
            samples.clear();
            currentSample.reset();
            multis.clear();
            system.waveFreeBytes = system.waveTotalBytes;
            system.mpksFreePercent = ALL_MPKS_FREE_PERCENT;
            return done();
        }

        // The index of the disk named by `handle`, or nothing when none matches (RQ-AKM-061).
        std::optional<std::size_t> findDiskByHandle(const std::vector<DiskRecord>& disks, int handle)
        {
            for (std::size_t index = 0; index < disks.size(); ++index)
                if (disks[index].handle == handle)
                    return index;
            return std::nullopt;
        }

        // §10 disk discovery of TASK-AKM-057 (RQ-AKM-060) and selection/status of TASK-AKM-058
        // (RQ-AKM-061): &01 is a no-op (this model always answers &04/&05 from `disks`, refresh or
        // not); &04 answers the count; &05 answers one record per disk (handle as two Bytes, type,
        // format, SCSI ID, writable, name), concatenated in order; &02/&03 act on the handle named by
        // the command's own data bytes, not necessarily the current selection (spec Table 20, footnote
        // a reads "the currently selected disk", but &03's own data columns carry a handle like &02's,
        // which is what this model follows); &06/&08/&09 act on the current selection, answering
        // ERROR 4 (not found) when there is none, like a current program or sample with nothing
        // selected; &09 always answers the root folder's empty path, since no folder is modelled yet
        // (TASK-AKM-060 changes that).
        Outcome executeDisk(std::uint8_t item, const Bytes& data, const std::vector<DiskRecord>& disks,
                            std::optional<std::size_t>& currentDisk)
        {
            switch (item)
            {
                case ITEM_UPDATE_DISK_LIST:
                    return done();
                case ITEM_GET_DISK_COUNT:
                    return reply(Bytes{static_cast<std::uint8_t>(disks.size())});
                case ITEM_GET_DISK_LIST:
                {
                    akm::ByteWriter writer;
                    for (const DiskRecord& disk : disks)
                    {
                        writer.appendWord(static_cast<std::uint32_t>(disk.handle));
                        writer.appendByte(disk.type);
                        writer.appendByte(disk.format);
                        writer.appendByte(disk.scsiId);
                        writer.appendByte(disk.writable ? 1 : 0);
                        writer.appendString(disk.name);
                    }
                    return reply(writer.bytes());
                }
                case ITEM_SELECT_DISK:
                {
                    akm::ByteReader reader(data);
                    const auto handle = reader.readWord();
                    if (!handle)
                        return failure(error_number::INVALID_FORMAT);
                    const auto found = findDiskByHandle(disks, static_cast<int>(*handle));
                    if (!found)
                        return failure(error_number::NOT_FOUND);
                    currentDisk = found;
                    return done();
                }
                case ITEM_TEST_DISK_VALID:
                {
                    akm::ByteReader reader(data);
                    const auto handle = reader.readWord();
                    if (!handle)
                        return failure(error_number::INVALID_FORMAT);
                    return findDiskByHandle(disks, static_cast<int>(*handle)) ? done() : failure(error_number::NOT_FOUND);
                }
                case ITEM_GET_CURRENT_DISK_TYPE:
                    if (!currentDisk)
                        return failure(error_number::NOT_FOUND);
                    return reply(Bytes{disks[*currentDisk].type});
                case ITEM_GET_DISK_TYPE:
                {
                    akm::ByteReader reader(data);
                    const auto handle = reader.readWord();
                    if (!handle)
                        return failure(error_number::INVALID_FORMAT);
                    const auto found = findDiskByHandle(disks, static_cast<int>(*handle));
                    if (!found)
                        return failure(error_number::NOT_FOUND);
                    return reply(Bytes{disks[*found].type});
                }
                case ITEM_GET_CURRENT_DISK_HANDLE:
                {
                    if (!currentDisk)
                        return failure(error_number::NOT_FOUND);
                    akm::ByteWriter writer;
                    writer.appendWord(static_cast<std::uint32_t>(disks[*currentDisk].handle));
                    return reply(writer.bytes());
                }
                case ITEM_GET_CURRENT_DISK_PATH:
                {
                    if (!currentDisk)
                        return failure(error_number::NOT_FOUND);
                    akm::ByteWriter writer;
                    writer.appendString("");
                    return reply(writer.bytes());
                }
                default:
                    return failure(error_number::NOT_SUPPORTED);
            }
        }

        // Only §00, the two version items of §02, the §0A items above, §08 keygroup selection, §06's
        // parameters (RQ-AKM-034, RQ-AKM-035), §0E's lifecycle (RQ-AKM-045) and §10's disk discovery
        // and selection (RQ-AKM-060, RQ-AKM-061) are modelled. A byte after the data an item expects is
        // ignored, as the spec says of a checksum sent while checksums are off.
        Outcome execute(std::uint8_t section, std::uint8_t item, const Bytes& data, SamplerSettings& settings,
                        const OsVersion& osVersion, SystemSetupState& system, std::vector<ProgramRecord>& programs,
                        std::optional<std::size_t>& currentProgram, std::optional<int>& currentKeygroup,
                        std::vector<SampleRecord>& samples, std::optional<std::size_t>& currentSample,
                        std::vector<std::string>& multis, const std::vector<DiskRecord>& disks,
                        std::optional<std::size_t>& currentDisk)
        {
            if (section == SECTION_SYSTEM && item == ITEM_CLEAR_SAMPLER_MEMORY)
                return executeClearMemory(system, programs, currentProgram, currentKeygroup, samples, currentSample,
                                      multis);
            if (section == SECTION_ZONE)
            {
                std::vector<std::string> sampleNames;
                sampleNames.reserve(samples.size());
                for (const SampleRecord& sample : samples)
                    sampleNames.push_back(sample.name);
                return executeZone(item, data, programs, currentProgram, currentKeygroup, sampleNames);
            }
            if (section == SECTION_SYSTEM)
                return executeSystem(item, data, osVersion, system);
            if (section == SECTION_PROGRAM)
                return executeProgram(item, data, programs, currentProgram, currentKeygroup);
            if (section == SECTION_KEYGROUP)
                return executeKeygroup(item, data, programs, currentProgram, currentKeygroup);
            if (section == SECTION_SAMPLE)
                return executeSample(item, data, samples, currentSample);
            if (section == SECTION_DISK)
                return executeDisk(item, data, disks, currentDisk);
            if (section != SECTION_SYSEX_CONFIG)
                return failure(error_number::NOT_SUPPORTED);
            switch (item)
            {
                case ITEM_QUERY:
                    return done();
                case ITEM_NOTIFICATION:
                    return setToggle(data, settings.notification);
                case ITEM_SYNC_LCD:
                    if (osVersion < SYNC_LCD_SINCE)
                        return failure(error_number::NOT_SUPPORTED);
                    return setToggle(data, settings.syncLcd);
                case ITEM_CHECKSUM:
                    return setToggle(data, settings.checksum);
                case ITEM_AUTO_SCREEN_UPDATE:
                    return setToggle(data, settings.autoScreenUpdate);
                case ITEM_ECHO:
                    if (data.size() < ECHO_DATA_SIZE)
                        return failure(error_number::INVALID_FORMAT);
                    return reply(Bytes(data.begin(), data.begin() + ECHO_DATA_SIZE));
                case ITEM_STILL_ALIVE:
                    if (osVersion < STILL_ALIVE_SINCE)
                        return failure(error_number::NOT_SUPPORTED);
                    return setToggle(data, settings.stillAlive);
                default:
                    return failure(error_number::NOT_SUPPORTED);
            }
        }

        Bytes buildConfirmation(std::uint8_t deviceByte, const Bytes& userRefs, std::uint8_t replyId, std::uint8_t section,
                                std::uint8_t item, const Bytes& data, bool withChecksum)
        {
            Bytes frame{common::midi::SYSEX_START, AKAI_MANUFACTURER_ID, SAMPLER_MODEL_ID, deviceByte};
            appendBytes(frame, userRefs);
            frame.push_back(replyId);
            frame.push_back(section);
            frame.push_back(item);
            appendBytes(frame, data);
            if (withChecksum)
                frame.push_back(checksum(std::span<const std::uint8_t>(frame).subspan(FIRST_USER_REF_INDEX)));
            frame.push_back(common::midi::SYSEX_END);
            return frame;
        }
    }

    SimulatedSampler::SimulatedSampler(SamplerConfig config, Scheduler& scheduler, Emit emit)
        : _config(config), _scheduler(scheduler), _emit(std::move(emit))
    {
    }

    void SimulatedSampler::setBehaviour(SamplerBehaviour behaviour)
    {
        const std::lock_guard lock(_mutex);
        _behaviour = std::move(behaviour);
    }

    void SimulatedSampler::setSampleNames(std::vector<std::string> names)
    {
        const std::lock_guard lock(_mutex);
        _samples.clear();
        _samples.reserve(names.size());
        for (std::string& name : names)
        {
            SampleRecord record;
            record.name = std::move(name);
            _samples.push_back(std::move(record));
        }
        _currentSample.reset();
    }

    void SimulatedSampler::setMultiNames(std::vector<std::string> names)
    {
        const std::lock_guard lock(_mutex);
        _multis = std::move(names);
    }

    std::size_t SimulatedSampler::multiCount() const
    {
        const std::lock_guard lock(_mutex);
        return _multis.size();
    }

    void SimulatedSampler::setSampleAttributes(std::size_t index, std::uint8_t type, std::uint8_t channels,
                                               std::uint32_t length, std::uint32_t rate)
    {
        const std::lock_guard lock(_mutex);
        if (index >= _samples.size())
            return;
        _samples[index].type = type;
        _samples[index].channels = channels;
        _samples[index].length = length;
        _samples[index].rate = rate;
    }

    void SimulatedSampler::setPlayMode(std::uint8_t playMode)
    {
        const std::lock_guard lock(_mutex);
        _system.playMode = playMode;
    }

    void SimulatedSampler::setFrontPanelLock(std::uint8_t lock)
    {
        const std::lock_guard guard(_mutex);
        _system.frontPanelLock = lock;
    }

    void SimulatedSampler::setHighestPlayMode(std::uint8_t highest)
    {
        const std::lock_guard lock(_mutex);
        _system.highestPlayMode = highest;
    }

    SystemSetupState SimulatedSampler::systemSetup() const
    {
        const std::lock_guard lock(_mutex);
        return _system;
    }

    void SimulatedSampler::setModel(std::uint8_t model)
    {
        const std::lock_guard lock(_mutex);
        _system.model = model;
    }

    void SimulatedSampler::setMemory(std::uint32_t waveTotalBytes, std::uint32_t waveFreeBytes,
                                     std::uint8_t mpksFreePercent)
    {
        const std::lock_guard lock(_mutex);
        _system.waveTotalBytes = waveTotalBytes;
        _system.waveFreeBytes = waveFreeBytes;
        _system.mpksFreePercent = mpksFreePercent;
    }

    void SimulatedSampler::setDisks(std::vector<DiskRecord> disks)
    {
        const std::lock_guard lock(_mutex);
        _disks = std::move(disks);
        _currentDisk.reset();
    }

    SamplerBehaviour SimulatedSampler::behaviour() const
    {
        const std::lock_guard lock(_mutex);
        return _behaviour;
    }

    SamplerSettings SimulatedSampler::settings() const
    {
        const std::lock_guard lock(_mutex);
        return _settings;
    }

    void SimulatedSampler::powerCycle()
    {
        const std::lock_guard lock(_mutex);
        _settings = SamplerSettings{};
    }

    std::vector<std::vector<std::uint8_t>> SimulatedSampler::receivedFrames() const
    {
        const std::lock_guard lock(_mutex);
        return _received;
    }

    std::vector<AcceptedCommand> SimulatedSampler::acceptedCommands() const
    {
        const std::lock_guard lock(_mutex);
        return _accepted;
    }

    void SimulatedSampler::receive(std::span<const std::uint8_t> message)
    {
        std::vector<Bytes> confirmations;
        SamplerBehaviour behaviour;
        bool stillAlive = false;
        {
            const std::lock_guard lock(_mutex);
            _received.emplace_back(message.begin(), message.end());
            stillAlive = _settings.stillAlive;
            confirmations = process(message);
            behaviour = _behaviour;
        }
        if (confirmations.empty() || behaviour.silent)
            return;

        std::vector<Bytes> outgoing = behaviour.junkBeforeReply;
        for (const Bytes& confirmation : confirmations)
        {
            for (int repeat = 0; repeat < std::max(behaviour.timesEachReply, MIN_REPLY_REPEAT); ++repeat)
                outgoing.push_back(confirmation);
        }

        if (behaviour.replyDelay <= Scheduler::Clock::duration::zero())
        {
            for (Bytes& frame : outgoing)
                _emit(std::move(frame));
            return;
        }

        // A sampler with Still Alive on says so about every second while it works; the scheduled tasks
        // capture the emit function, not the sampler, so they are safe when the bus is gone.
        if (stillAlive && behaviour.stillAliveInterval > Scheduler::Clock::duration::zero())
        {
            for (auto at = behaviour.stillAliveInterval; at < behaviour.replyDelay; at += behaviour.stillAliveInterval)
                _scheduler.scheduleAfter(at, [emit = _emit] { emit(STILL_ALIVE_MESSAGE); });
        }
        _scheduler.scheduleAfter(behaviour.replyDelay, [emit = _emit, frames = std::move(outgoing)] {
            for (const Bytes& frame : frames)
                emit(frame);
        });
    }

    std::vector<std::vector<std::uint8_t>> SimulatedSampler::process(std::span<const std::uint8_t> message)
    {
        // A sampler ignores what is not F0 47 5E ... F7 made of data bytes and long enough to hold a section and an item.
        constexpr std::size_t minimumSize = FIRST_USER_REF_INDEX + USER_REF_COUNT_MIN + SECTION_AND_ITEM_SIZE + END_BYTE_SIZE;
        if (message.size() < minimumSize || message.front() != common::midi::SYSEX_START
            || message.back() != common::midi::SYSEX_END || message[MANUFACTURER_ID_INDEX] != AKAI_MANUFACTURER_ID
            || message[MODEL_ID_INDEX] != SAMPLER_MODEL_ID
            || !allDataBytes(message.subspan(START_BYTE_SIZE, message.size() - START_BYTE_SIZE - END_BYTE_SIZE)))
            return {};

        const std::uint8_t deviceByte = message[DEVICE_BYTE_INDEX];
        const std::uint8_t messageDeviceId = deviceByte & DEVICE_ID_MASK;
        const std::size_t userRefCount = ((deviceByte >> USER_REF_COUNT_SHIFT) & USER_REF_COUNT_MASK) + USER_REF_COUNT_MIN;
        const std::size_t sectionIndex = FIRST_USER_REF_INDEX + userRefCount;
        if (message.size() < sectionIndex + SECTION_AND_ITEM_SIZE + END_BYTE_SIZE)
            return {};

        // Spec p. 4: DeviceID 0 on either side matches; otherwise the two must be equal.
        if (_config.deviceId != 0 && messageDeviceId != 0 && messageDeviceId != _config.deviceId)
            return {};

        const Bytes userRefs(message.begin() + FIRST_USER_REF_INDEX, message.begin() + static_cast<std::ptrdiff_t>(sectionIndex));
        const std::uint8_t section = message[sectionIndex];
        const std::uint8_t item = message[sectionIndex + 1];
        const std::size_t endIndex = message.size() - END_BYTE_SIZE;
        Bytes data(message.begin() + static_cast<std::ptrdiff_t>(sectionIndex + SECTION_AND_ITEM_SIZE),
                   message.begin() + static_cast<std::ptrdiff_t>(endIndex));

        // Confirmations echo the count of user-refs, and carry the sampler's own DeviceID or the message's.
        const std::uint8_t replyDeviceId =
            _behaviour.confirmationDeviceId == ConfirmationDeviceId::Own ? _config.deviceId : messageDeviceId;
        const auto replyDeviceByte = static_cast<std::uint8_t>((deviceByte & ~DEVICE_ID_MASK) | replyDeviceId);
        // A REPLY of an item the sampler tags differently carries that section; every other confirmation, the command's.
        const auto sectionOf = [&](std::uint8_t replyId) {
            if (replyId == REPLY_REPLY)
            {
                for (const ReplySectionOverride& override : _behaviour.replySectionOverrides)
                {
                    if (override.section == section && override.item == item)
                        return override.replySection;
                }
            }
            return section;
        };
        const auto confirmation = [&](std::uint8_t replyId, const Bytes& replyData, bool withChecksum) {
            return buildConfirmation(replyDeviceByte, userRefs, replyId, sectionOf(replyId), item, replyData, withChecksum);
        };

        const SamplerSettings before = _settings;
        std::vector<Bytes> confirmations;

        if (before.checksum)
        {
            // The last byte before F7 is the checksum of what precedes it, from the first user-ref on.
            const bool valid =
                !data.empty()
                && data.back() == checksum(message.subspan(FIRST_USER_REF_INDEX, endIndex - CHECKSUM_SIZE - FIRST_USER_REF_INDEX));
            if (!valid)
            {
                // Observed on the S5000: OK first, since it is sent as soon as the frame arrives, then the ERROR;
                // both carry a checksum, checksums being on.
                if (before.notification)
                    confirmations.push_back(confirmation(REPLY_OK, {}, true));
                confirmations.push_back(confirmation(REPLY_ERROR, errorData(error_number::CHECKSUM_INVALID), true));
                return confirmations;
            }
            data.pop_back();
        }

        _accepted.push_back(AcceptedCommand{messageDeviceId, userRefs, section, item, data});

        // OK goes out as soon as the message is accepted, before it runs, so it follows the previous settings.
        if (before.notification)
            confirmations.push_back(confirmation(REPLY_OK, {}, before.checksum));

        // An item the sampler was told to refuse fails with its ERROR and does not run.
        const auto refused = std::find_if(_behaviour.itemErrors.begin(), _behaviour.itemErrors.end(),
                                          [section, item](const ItemError& candidate) {
                                              return candidate.section == section && candidate.item == item;
                                          });
        const Outcome outcome = refused != _behaviour.itemErrors.end()
                                    ? failure(refused->number)
                                    : execute(section, item, data, _settings, _config.osVersion, _system, _programs, _currentProgram,
                                              _currentKeygroup, _samples, _currentSample, _multis, _disks, _currentDisk);
        const bool resultChecksum = _behaviour.checksumChangeAppliesToOwnConfirmation ? _settings.checksum : before.checksum;
        confirmations.push_back(confirmation(outcome.replyId, outcome.data, resultChecksum));
        if (outcome.replyId == REPLY_REPLY && _behaviour.errorAfterReply)
            confirmations.push_back(confirmation(REPLY_ERROR, errorData(*_behaviour.errorAfterReply), resultChecksum));
        return confirmations;
    }
}
