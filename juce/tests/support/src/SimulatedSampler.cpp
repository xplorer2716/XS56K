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

        Outcome executeSystem(std::uint8_t item, const OsVersion& osVersion)
        {
            switch (item)
            {
                case ITEM_OS_VERSION:
                    return reply(Bytes{static_cast<std::uint8_t>(osVersion.major), static_cast<std::uint8_t>(osVersion.minor)});
                case ITEM_OS_SUB_VERSION:
                    return reply(Bytes{OS_SUB_VERSION});
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
        constexpr std::array<ParameterGroupRange, 5> KEYGROUP_PARAMETER_GROUP_RANGES{{
            {0x04, 0x09, 0x06},  // General Options
            {0x10, 0x14, 0x08},  // Pitch/Amp
            {0x20, 0x25, 0x08},  // Filter
            {0x30, 0x38, 0x10},  // Filter Envelope
            {0x50, 0x57, 0x08},  // Amplitude Envelope
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
                            concatenated.insert(concatenated.end(), value.begin(), value.end());
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

        // Only §00, the two version items of §02, the §0A items above and §08 keygroup selection are
        // modelled. A byte after the data an item expects is ignored, as the spec says of a checksum sent
        // while checksums are off.
        Outcome execute(std::uint8_t section, std::uint8_t item, const Bytes& data, SamplerSettings& settings,
                        const OsVersion& osVersion, std::vector<ProgramRecord>& programs,
                        std::optional<std::size_t>& currentProgram, std::optional<int>& currentKeygroup)
        {
            if (section == SECTION_SYSTEM)
                return executeSystem(item, osVersion);
            if (section == SECTION_PROGRAM)
                return executeProgram(item, data, programs, currentProgram, currentKeygroup);
            if (section == SECTION_KEYGROUP)
                return executeKeygroup(item, data, programs, currentProgram, currentKeygroup);
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
            frame.insert(frame.end(), userRefs.begin(), userRefs.end());
            frame.push_back(replyId);
            frame.push_back(section);
            frame.push_back(item);
            frame.insert(frame.end(), data.begin(), data.end());
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
        const auto confirmation = [&](std::uint8_t replyId, const Bytes& replyData, bool withChecksum) {
            return buildConfirmation(replyDeviceByte, userRefs, replyId, section, item, replyData, withChecksum);
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
                                    : execute(section, item, data, _settings, _config.osVersion, _programs, _currentProgram,
                                              _currentKeygroup);
        const bool resultChecksum = _behaviour.checksumChangeAppliesToOwnConfirmation ? _settings.checksum : before.checksum;
        confirmations.push_back(confirmation(outcome.replyId, outcome.data, resultChecksum));
        if (outcome.replyId == REPLY_REPLY && _behaviour.errorAfterReply)
            confirmations.push_back(confirmation(REPLY_ERROR, errorData(*_behaviour.errorAfterReply), resultChecksum));
        return confirmations;
    }
}
