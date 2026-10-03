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

// The item catalogue (the generated table and its lookup) and what the generic encoder, range validator
// and REPLY decoder do with a record. The synthetic descriptors cover the value formats that no record of
// section 00 or of the two version items uses yet.
// [TASK-AKM-008, RQ-AKM-001, RQ-AKM-002, RQ-AKM-014, RQ-AKM-015, RQ-AKM-041, RQ-AKM-044,
// ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "TestBytes.hpp"
#include "akm/CommandResult.hpp"
#include "akm/ItemCatalogue.hpp"
#include "akm/ItemRequest.hpp"

using akm::ExpectedReply;
using akm::ItemDescriptor;
using akm::ItemId;
using akm::ItemKind;
using akm::RefusalReason;
using akm::ValueFormat;
using akm::ValueSpec;
using akm::test::Bytes;
using akm::test::bytes;

namespace
{
    // The catalogue as the spec's tables 5 to 7 give it: what each enumerator must resolve to.
    struct Expected
    {
        ItemId id;
        std::uint8_t section;
        std::uint8_t item;
        ItemKind kind;
        std::size_t argumentCount;
        std::optional<std::size_t> replyLength;
    };

    const std::array<Expected, 23> CATALOGUE{{
        {ItemId::SysExQuery, 0x00, 0x00, ItemKind::Set, 0, std::nullopt},
        {ItemId::SysExNotification, 0x00, 0x01, ItemKind::Set, 1, std::nullopt},
        {ItemId::SysExSyncLcd, 0x00, 0x03, ItemKind::Set, 1, std::nullopt},
        {ItemId::SysExChecksum, 0x00, 0x04, ItemKind::Set, 1, std::nullopt},
        {ItemId::SysExAutoScreenUpdate, 0x00, 0x05, ItemKind::Set, 1, std::nullopt},
        {ItemId::SysExEcho, 0x00, 0x06, ItemKind::Get, 4, 4},
        {ItemId::SysExStillAlive, 0x00, 0x07, ItemKind::Set, 1, std::nullopt},
        {ItemId::SystemOsVersion, 0x02, 0x00, ItemKind::Get, 0, 2},
        {ItemId::SystemOsSubVersion, 0x02, 0x01, ItemKind::Get, 0, 1},
        // The sampler name (TASK-AKM-048, RQ-AKM-052): a String has no fixed REPLY length.
        {ItemId::SystemSetName, 0x02, 0x02, ItemKind::Set, 1, std::nullopt},
        {ItemId::SystemGetName, 0x02, 0x03, ItemKind::Get, 0, std::nullopt},
        // The clock and date (TASK-AKM-050, RQ-AKM-054): seven values, the year a compound word, so eight
        // data bytes in a REPLY.
        {ItemId::SystemGetClock, 0x02, 0x05, ItemKind::Get, 0, 8},
        {ItemId::SystemSetClock, 0x02, 0x06, ItemKind::Set, 7, std::nullopt},
        // The Play Mode and the front-panel lock (TASK-AKM-051, RQ-AKM-055): one byte each.
        {ItemId::SystemSetPlayMode, 0x02, 0x10, ItemKind::Set, 1, std::nullopt},
        {ItemId::SystemSetFrontPanelLock, 0x02, 0x11, ItemKind::Set, 1, std::nullopt},
        {ItemId::SystemGetPlayMode, 0x02, 0x20, ItemKind::Get, 0, 1},
        {ItemId::SystemGetFrontPanelLock, 0x02, 0x21, ItemKind::Get, 0, 1},
        // The guarded Clear Sampler Memory (TASK-AKM-052, RQ-AKM-056): no argument, no REPLY.
        {ItemId::SystemClearMemory, 0x02, 0x32, ItemKind::Set, 0, std::nullopt},
        // The model and the available memory (TASK-AKM-049, RQ-AKM-053): one byte each, the byte counts
        // a compound double word of four data bytes.
        {ItemId::SystemGetModel, 0x02, 0x04, ItemKind::Get, 0, 1},
        {ItemId::SystemGetWaveMemoryPercent, 0x02, 0x30, ItemKind::Get, 0, 1},
        {ItemId::SystemGetMpksMemoryPercent, 0x02, 0x31, ItemKind::Get, 0, 1},
        {ItemId::SystemGetWaveMemoryTotal, 0x02, 0x33, ItemKind::Get, 0, 4},
        {ItemId::SystemGetWaveMemoryFree, 0x02, 0x34, ItemKind::Get, 0, 4},
    }};

    constexpr std::size_t SYSEX_CONFIG_ITEM_COUNT = 7;
    constexpr std::uint8_t SECTION_SYSEX_CONFIG = 0x00;

    // One argument of every numeric format, with the ranges the formats allow.
    constexpr std::array<ValueSpec, 7> EVERY_FORMAT{{
        {"byte", ValueFormat::Byte, 0, 127},
        {"word", ValueFormat::Word, 0, 16383},
        {"dword", ValueFormat::Dword, 0, 268435455},
        {"qword", ValueFormat::Qword, 0, 72057594037927935},
        {"signedByte", ValueFormat::SignedByte, -127, 127},
        {"signedWord", ValueFormat::SignedWord, -16383, 16383},
        {"signedDword", ValueFormat::SignedDword, -268435455, 268435455},
    }};
    constexpr ItemDescriptor EVERY_FORMAT_ITEM{"Every format", 0x10, 0x22, ItemKind::Get, EVERY_FORMAT, EVERY_FORMAT};

    // An argument whose range is narrower than its format, for the range validator.
    constexpr std::array<ValueSpec, 1> NARROW{{{"narrow", ValueFormat::Word, 10, 20}}};
    constexpr ItemDescriptor NARROW_ITEM{"Narrow", 0x10, 0x23, ItemKind::Set, NARROW, {}};

    // A single String value, 0-20 characters — the range TASK-AKM-015 gives the Program name items
    // (documents/_index/sysex_spec.kb.md, "Common value codes"); no record of this lot uses String yet
    // (ADR-AKM-001, DEC-AKM-013), so this descriptor, like EVERY_FORMAT_ITEM and NARROW_ITEM, is synthetic.
    constexpr std::array<ValueSpec, 1> STRING_VALUE{{{"text", ValueFormat::String, 0, 20}}};
    constexpr ItemDescriptor STRING_ITEM{"String", 0x10, 0x24, ItemKind::Get, STRING_VALUE, STRING_VALUE};
    // Exactly one reply value, but not a String: proves the check also looks at the format, not just
    // the count (EVERY_FORMAT_ITEM's six values already prove the count is checked).
    constexpr std::array<ValueSpec, 1> SINGLE_BYTE{{{"value", ValueFormat::Byte, 0, 127}}};
    constexpr ItemDescriptor NOT_A_STRING_ITEM{"Not a string", 0x10, 0x25, ItemKind::Get, {}, SINGLE_BYTE};

    using Values = std::vector<std::int64_t>;
}

TEST_CASE("Given the catalogue, When each enumerator is resolved, Then it gives the record of the spec with its section, item, kind and argument count [RQ-AKM-001, RQ-AKM-044]",
          "[akm][catalogue]")
{
    for (const Expected& expected : CATALOGUE)
    {
        const ItemDescriptor& record = akm::descriptor(expected.id);
        CHECK(record.section == expected.section);
        CHECK(record.item == expected.item);
        CHECK(record.kind == expected.kind);
        CHECK(record.args.size() == expected.argumentCount);
        CHECK_FALSE(record.name.empty());
    }
}

TEST_CASE("Given the catalogue, When counted, Then section 00 holds the seven items of the spec and no other record shares a section and item [RQ-AKM-012 to RQ-AKM-015]",
          "[akm][catalogue]")
{
    std::size_t sysexConfig = 0;
    for (const ItemDescriptor& record : akm::ITEM_TABLE)
    {
        if (record.section == SECTION_SYSEX_CONFIG)
            ++sysexConfig;
        CHECK(akm::findItem(record.section, record.item) == &record);
    }

    CHECK(sysexConfig == SYSEX_CONFIG_ITEM_COUNT);
    // CATALOGUE tracks only sections 00 and 02 (PLAN-AKM-006's items are added to it one task at a time);
    // TASK-AKM-015 to 023 added 95 records of section 0A (now
    // complete, TASK-AKM-023's guarded &07 included), TASK-AKM-026 to 032 added section 08's selection
    // (2), General Options (12), Pitch/Amp (10), Filter (12), Filter Envelope (18), Amplitude Envelope
    // (16) and Aux Envelope (10) — complete too (80/80 commands, 40/40 REPLY formats) — TASK-AKM-035
    // to 036 added section 06's 28 records: the 13 non-sample zone parameters (Level..Solo, Set and Get)
    // and sample assignment by name (&01/&21) — 06 now complete too (28/28 commands) — TASK-AKM-040
    // added 8 records of section 0E (the 6 lifecycle items of RQ-AKM-045 plus &13/&14 pulled in early),
    // TASK-AKM-041 added its guarded &07 (RQ-AKM-046), TASK-AKM-042 added &10-&12 (RQ-AKM-047's
    // remaining general-information items), TASK-AKM-043 added the 8 settable parameters and their
    // Gets (RQ-AKM-048: &20-&24, &28-&2A / &40-&44, &48-&4A) and TASK-AKM-044 added the 4 read-only
    // parameters and the two grouped-REPLY items (RQ-AKM-049: &30-&33, &34, &4B), 0E now 34/34,
    // complete; TASK-AKM-057 added section 10's first 3 records, disk discovery (RQ-AKM-060: &01, &04,
    // &05), TASK-AKM-058 added 6 more, selection and status (RQ-AKM-061: &02, &03, &06-&09), and
    // TASK-AKM-059 added 3 more, format/free space/name (RQ-AKM-062: &0A, &0B, &0E — the catalogue's
    // first `Qword`, ADR-AKM-001 DEC-AKM-017), TASK-AKM-060 added 7 more, folder navigation, listing
    // and management (RQ-AKM-063: &10-&14, &16, &18 — &18 the catalogue's first item with two `String`
    // arguments, ADR-AKM-001 DEC-AKM-018), TASK-AKM-061 added 1 more, Load Folder (RQ-AKM-064: &15),
    // and TASK-AKM-062 added 6 more, file listing/info/rename (RQ-AKM-065: &20-&24, &28 — &23's
    // Compound Double Word split into four Bytes like §0E's position/loop items, not a single `Dword`
    // like §02's Wave memory, since its own spec row decomposes into four comparable columns), and
    // TASK-AKM-063 added 2 more, Load File with and without dependents (RQ-AKM-066: &2A, &2B — &2A's
    // String-then-Byte shape built by hand, like `ProgramSetNumber`'s own conditional shape), and
    // TASK-AKM-064 added 2 more, Save Memory Item(s) to disk (RQ-AKM-067: &2C, &2D), and TASK-AKM-065
    // added 2 more, sample audition from disk (RQ-AKM-068: &30, &31), and TASK-AKM-066 added the last 3,
    // destructive command guards for Eject/Delete Sub-Folder/Delete File (RQ-AKM-069: &0D, &17, &29) —
    // completing §10's 35 command rows, and TASK-AKM-070 added 2 of section 20, Key Hold and Key Release
    // (RQ-AKM-073: &01, &02) and TASK-AKM-071 the last 2, the data wheel and the ASCII keyboard
    // (RQ-AKM-074: &03, &04) — which this count includes without tracking them here too (see
    // ProgramPrimitivesTests.cpp, SamplePrimitivesTests.cpp, SampleDeleteAllGuardTests.cpp,
    // SampleParametersTests.cpp, SampleReadOnlyParametersTests.cpp, DiskPrimitivesTests.cpp,
    // FrontPanelTests.cpp and the other test files).
    constexpr std::size_t PROGRAM_ITEM_COUNT =
        95 + 2 + 12 + 10 + 12 + 18 + 16 + 10 + 28 + 8 + 1 + 3 + 16 + 6 + 3 + 6 + 3 + 7 + 1 + 6 + 2 + 2 + 2 + 3 + 2 + 2;
    CHECK(akm::ITEM_TABLE.size() == CATALOGUE.size() + PROGRAM_ITEM_COUNT);
}

TEST_CASE("Given a section and an item, When looked up, Then a record is found and an item of the spec that is not catalogued is not [RQ-AKM-041]",
          "[akm][catalogue]")
{
    for (const Expected& expected : CATALOGUE)
        CHECK(akm::findItem(expected.section, expected.item) == &akm::descriptor(expected.id));

    // Section 00 has no item 02 (the spec skips it); sections 0A (TASK-AKM-023), 08 (TASK-AKM-032) and
    // 06 (TASK-AKM-036) are now all complete, so this uses section 02 (System): the spec has no item
    // &07 there, whatever PLAN-AKM-006 catalogues of the rest of the section.
    CHECK(akm::findItem(0x00, 0x02) == nullptr);
    CHECK(akm::findItem(0x02, 0x07) == nullptr);
    // The same item code in another section is another item: &0A is Set Program Number in §0A, not §02.
    CHECK(akm::findItem(0x02, 0x0A) == nullptr);
}

TEST_CASE("Given each record, When the length of its REPLY is asked, Then a Set has none and a Get has the total width of its values [RQ-AKM-041]",
          "[akm][catalogue]")
{
    for (const Expected& expected : CATALOGUE)
        CHECK(akm::descriptor(expected.id).fixedReplyLength() == expected.replyLength);
}

TEST_CASE("Given each value format, When its width is asked, Then it is the number of data bytes the spec gives it [RQ-AKM-002]",
          "[akm][catalogue]")
{
    CHECK(akm::valueWidth(ValueFormat::Byte) == 1);
    CHECK(akm::valueWidth(ValueFormat::Word) == 2);
    CHECK(akm::valueWidth(ValueFormat::Dword) == 4);
    CHECK(akm::valueWidth(ValueFormat::Qword) == 8);
    // A sign byte, then the magnitude in a byte, a word or a dword.
    CHECK(akm::valueWidth(ValueFormat::SignedByte) == 2);
    CHECK(akm::valueWidth(ValueFormat::SignedWord) == 3);
    CHECK(akm::valueWidth(ValueFormat::SignedDword) == 5);
    CHECK(EVERY_FORMAT_ITEM.fixedReplyLength() == 1 + 2 + 4 + 8 + 2 + 3 + 5);
}

TEST_CASE("Given the String value format, When its width is asked, Then it is undefined: a String has no fixed width [RQ-AKM-002]",
          "[akm][catalogue]")
{
    CHECK(akm::valueWidth(ValueFormat::String) == akm::UNDEFINED_VALUE_WIDTH);
}

TEST_CASE("Given a record whose REPLY carries a String, When the length of its REPLY is asked, Then it is not fixed [RQ-AKM-041]",
          "[akm][catalogue]")
{
    CHECK(STRING_ITEM.fixedReplyLength() == std::nullopt);
}

TEST_CASE("Given the values 01 23 45 67, When the Echo is encoded, Then the command carries section 00, item 06 and the four bytes [RQ-AKM-015]",
          "[akm][catalogue]")
{
    const akm::CommandRequest request = akm::makeRequest(ItemId::SysExEcho, {0x01, 0x23, 0x45, 0x67});

    CHECK_FALSE(request.refusal.has_value());
    CHECK(request.command.section == 0x00);
    CHECK(request.command.item == 0x06);
    CHECK(request.command.data == bytes({0x01, 0x23, 0x45, 0x67}));
    CHECK(request.options.expectedReply == ExpectedReply::Delimited);
}

TEST_CASE("Given each toggle and each value 0 and 1, When encoded, Then the command carries section 00, that item code and that data byte [RQ-AKM-014]",
          "[akm][catalogue]")
{
    const std::array<std::pair<ItemId, std::uint8_t>, 5> toggles{{{ItemId::SysExNotification, 0x01},
                                                                  {ItemId::SysExSyncLcd, 0x03},
                                                                  {ItemId::SysExChecksum, 0x04},
                                                                  {ItemId::SysExAutoScreenUpdate, 0x05},
                                                                  {ItemId::SysExStillAlive, 0x07}}};
    for (const auto& [id, itemCode] : toggles)
    {
        for (const std::int64_t value : {0, 1})
        {
            const akm::CommandRequest request = akm::makeRequest(id, {value});

            CHECK_FALSE(request.refusal.has_value());
            CHECK(request.command.section == 0x00);
            CHECK(request.command.item == itemCode);
            CHECK(request.command.data == bytes({static_cast<unsigned int>(value)}));
        }
    }
}

TEST_CASE("Given the value 2 for each toggle, When encoded, Then it is refused as out of range and no data is produced [RQ-AKM-014]",
          "[akm][catalogue]")
{
    for (const ItemId id : {ItemId::SysExNotification, ItemId::SysExSyncLcd, ItemId::SysExChecksum,
                            ItemId::SysExAutoScreenUpdate, ItemId::SysExStillAlive})
    {
        const akm::CommandRequest request = akm::makeRequest(id, {2});

        REQUIRE(request.refusal.has_value());
        CHECK(*request.refusal == RefusalReason::ArgumentOutOfRange);
        CHECK(request.command.data.empty());
    }
}

TEST_CASE("Given a byte 80 or a negative value for the Echo, When encoded, Then it is refused as out of range [RQ-AKM-015]",
          "[akm][catalogue]")
{
    for (const Values& values : {Values{0x01, 0x23, 0x45, 0x80}, Values{-1, 0, 0, 0}, Values{0, 0, 0, 1000}})
    {
        const akm::CommandRequest request = akm::makeRequest(ItemId::SysExEcho, values);

        REQUIRE(request.refusal.has_value());
        CHECK(*request.refusal == RefusalReason::ArgumentOutOfRange);
    }
}

TEST_CASE("Given too few or too many values, When encoded, Then the command is refused for its number of arguments [RQ-AKM-001, RQ-AKM-015]",
          "[akm][catalogue]")
{
    const std::array<std::pair<ItemId, Values>, 4> wrong{{{ItemId::SysExEcho, {0x01, 0x23, 0x45}},
                                                          {ItemId::SysExEcho, {0x01, 0x23, 0x45, 0x67, 0x00}},
                                                          {ItemId::SysExQuery, {0}},
                                                          {ItemId::SysExNotification, {}}}};
    for (const auto& [id, values] : wrong)
    {
        const akm::CommandRequest request = akm::makeRequest(id, values);

        REQUIRE(request.refusal.has_value());
        CHECK(*request.refusal == RefusalReason::WrongArgumentCount);
    }
}

TEST_CASE("Given an item without arguments, When encoded, Then it carries no data [RQ-AKM-012, RQ-AKM-044]",
          "[akm][catalogue]")
{
    for (const ItemId id : {ItemId::SysExQuery, ItemId::SystemOsVersion, ItemId::SystemOsSubVersion})
    {
        const akm::CommandRequest request = akm::makeRequest(id, std::span<const std::int64_t>{});

        CHECK_FALSE(request.refusal.has_value());
        CHECK(request.command.data.empty());
    }
}

TEST_CASE("Given options for the command, When it is encoded, Then they are kept on the request [RQ-AKM-012, RQ-AKM-013]",
          "[akm][catalogue]")
{
    akm::CommandOptions options;
    options.addressing = akm::Addressing::Broadcast;
    options.checksumModeAfterDone = true;

    const akm::CommandRequest request = akm::makeRequest(ItemId::SysExChecksum, {1}, options);

    CHECK(request.options.addressing == akm::Addressing::Broadcast);
    CHECK(request.options.checksumModeAfterDone == true);
}

TEST_CASE("Given a value of each numeric format, When encoded, Then the bytes are those the spec gives [RQ-AKM-002]",
          "[akm][catalogue]")
{
    const std::array<std::int64_t, 7> values{{5, 385, 268435455, 128, -5, -37, 1}};

    const akm::CommandRequest request = akm::makeRequest(EVERY_FORMAT_ITEM, values);

    CHECK_FALSE(request.refusal.has_value());
    // byte 5 | word 385 = 03 01 | dword 128^4-1 | qword 128 = 00*6 01 00 | signed byte -5 = 01 05 |
    // signed word -37 = 01 00 25 | signed dword +1.
    CHECK(request.command.data == bytes({0x05, 0x03, 0x01, 0x7F, 0x7F, 0x7F, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x01, 0x00, 0x01, 0x05, 0x01, 0x00, 0x25, 0x00, 0x00, 0x00, 0x00, 0x01}));
}

TEST_CASE("Given a value outside the range of the record but inside its format, When encoded, Then it is refused, and the bounds themselves are accepted [RQ-AKM-001]",
          "[akm][catalogue]")
{
    for (const std::int64_t value : {9, 21, 16383})
    {
        const std::array<std::int64_t, 1> values{{value}};
        const akm::CommandRequest request = akm::makeRequest(NARROW_ITEM, values);

        REQUIRE(request.refusal.has_value());
        CHECK(*request.refusal == RefusalReason::ArgumentOutOfRange);
    }
    for (const std::int64_t value : {10, 15, 20})
    {
        const std::array<std::int64_t, 1> values{{value}};
        CHECK_FALSE(akm::makeRequest(NARROW_ITEM, values).refusal.has_value());
    }
}

TEST_CASE("Given an ASCII name inside its range, When makeStringRequest encodes it, Then the command carries the name followed by 00 [RQ-AKM-002]",
          "[akm][catalogue]")
{
    const akm::CommandRequest request = akm::makeStringRequest(STRING_ITEM, "TESTPRG");

    CHECK_FALSE(request.refusal.has_value());
    CHECK(request.command.section == 0x10);
    CHECK(request.command.item == 0x24);
    // "TESTPRG" in ASCII.
    CHECK(request.command.data == bytes({0x54, 0x45, 0x53, 0x54, 0x50, 0x52, 0x47, 0x00}));
}

TEST_CASE("Given a name longer than the item's character-count range, When makeStringRequest encodes it, Then it is refused as out of range and no data is produced [RQ-AKM-001]",
          "[akm][catalogue]")
{
    const std::string tooLong(21, 'A');

    const akm::CommandRequest request = akm::makeStringRequest(STRING_ITEM, tooLong);

    REQUIRE(request.refusal.has_value());
    CHECK(*request.refusal == RefusalReason::ArgumentOutOfRange);
    CHECK(request.command.data.empty());
}

TEST_CASE("Given a name that is not 7-bit ASCII or contains a 00 byte, When makeStringRequest encodes it, Then it is refused as not encodable [RQ-AKM-002]",
          "[akm][catalogue]")
{
    for (const std::string& text : {std::string("A\x80"), std::string("A\0B", 3)})
    {
        const akm::CommandRequest request = akm::makeStringRequest(STRING_ITEM, text);

        REQUIRE(request.refusal.has_value());
        CHECK(*request.refusal == RefusalReason::NotEncodable);
    }
}

TEST_CASE("Given an item without exactly one String argument, When makeStringRequest is called, Then it is refused for its number of arguments [RQ-AKM-001]",
          "[akm][catalogue]")
{
    CHECK(*akm::makeStringRequest(NARROW_ITEM, "text").refusal == RefusalReason::WrongArgumentCount);
    CHECK(*akm::makeStringRequest(EVERY_FORMAT_ITEM, "text").refusal == RefusalReason::WrongArgumentCount);
}

TEST_CASE("Given the data of an OS version REPLY, When decoded, Then the major and minor numbers come back [RQ-AKM-044]",
          "[akm][catalogue]")
{
    const auto version = akm::decodeReply(ItemId::SystemOsVersion, bytes({0x02, 0x0E}));
    const auto subVersion = akm::decodeReply(ItemId::SystemOsSubVersion, bytes({0x00}));

    REQUIRE(version.has_value());
    CHECK(*version == Values{2, 14});
    REQUIRE(subVersion.has_value());
    CHECK(*subVersion == Values{0});
}

TEST_CASE("Given the data of an Echo REPLY, When decoded, Then the four bytes come back [RQ-AKM-015]", "[akm][catalogue]")
{
    const auto echoed = akm::decodeReply(ItemId::SysExEcho, bytes({0x01, 0x23, 0x45, 0x66}));

    REQUIRE(echoed.has_value());
    CHECK(*echoed == Values{0x01, 0x23, 0x45, 0x66});
}

TEST_CASE("Given reply data that is too short, too long or not made of data bytes, When decoded, Then nothing is returned [RQ-AKM-044]",
          "[akm][catalogue]")
{
    CHECK_FALSE(akm::decodeReply(ItemId::SystemOsVersion, bytes({0x02})).has_value());
    CHECK_FALSE(akm::decodeReply(ItemId::SystemOsVersion, bytes({0x02, 0x0E, 0x5F})).has_value());
    CHECK_FALSE(akm::decodeReply(ItemId::SystemOsVersion, bytes({0x02, 0x8E})).has_value());
    CHECK_FALSE(akm::decodeReply(ItemId::SystemOsVersion, Bytes{}).has_value());
}

TEST_CASE("Given the data of a REPLY of every numeric format, When decoded, Then the values of the spec's examples come back [RQ-AKM-002]",
          "[akm][catalogue]")
{
    const Bytes data = bytes({0x05, 0x03, 0x01, 0x7F, 0x7F, 0x7F, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
                              0x01, 0x05, 0x01, 0x00, 0x25, 0x00, 0x00, 0x00, 0x00, 0x01});

    const auto values = akm::decodeReply(EVERY_FORMAT_ITEM, data);

    REQUIRE(values.has_value());
    CHECK(*values == Values{5, 385, 268435455, 128, -5, -37, 1});
}

TEST_CASE("Given the data of a String REPLY, When decodeStringReply reads it, Then the name comes back [RQ-AKM-002]",
          "[akm][catalogue]")
{
    // "TESTPRG" in ASCII.
    const auto text = akm::decodeStringReply(STRING_ITEM, bytes({0x54, 0x45, 0x53, 0x54, 0x50, 0x52, 0x47, 0x00}));

    REQUIRE(text.has_value());
    CHECK(*text == "TESTPRG");
}

TEST_CASE("Given reply data that is not exactly one null-terminated string, When decodeStringReply reads it, Then nothing is returned [RQ-AKM-002]",
          "[akm][catalogue]")
{
    // No terminator; a terminator followed by trailing bytes; empty data.
    CHECK_FALSE(akm::decodeStringReply(STRING_ITEM, bytes({0x41, 0x42})).has_value());
    CHECK_FALSE(akm::decodeStringReply(STRING_ITEM, bytes({0x41, 0x42, 0x00, 0x43})).has_value());
    CHECK_FALSE(akm::decodeStringReply(STRING_ITEM, Bytes{}).has_value());
}

TEST_CASE("Given an item without exactly one String reply, When decodeStringReply is called, Then nothing is returned [RQ-AKM-002]",
          "[akm][catalogue]")
{
    CHECK_FALSE(akm::decodeStringReply(EVERY_FORMAT_ITEM, bytes({0x41, 0x00})).has_value());
    CHECK_FALSE(akm::decodeStringReply(NOT_A_STRING_ITEM, bytes({0x41, 0x00})).has_value());
}
