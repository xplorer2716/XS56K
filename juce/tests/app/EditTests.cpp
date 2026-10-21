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
// The edit controller against the simulated sampler: what it reads when a sample is selected, what it does for a kind with no editor, the
// read again, the end of the connection. [RQ-APP-048, RQ-APP-050, RQ-APP-053, RQ-APP-086, RQ-APP-087, ADR-APP-004 (DEC-APP-025), TASK-APP-036]
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

#include "AppTestSupport.hpp"
#include "SimulatedPrograms.hpp"
#include "akm/harness/SimulatedSampler.hpp"
#include "app/EditController.hpp"
#include "app/MemoryController.hpp"
#include "mcp/ParameterCatalogue.hpp"

using app::ConnectionState;
using app::MemoryItem;
using app::MemoryKind;
using app::test::CoreRig;
using app::test::setupA;

namespace
{
    constexpr std::uint8_t SECTION_SAMPLE = 0x0E;
    constexpr std::uint8_t ITEM_SET_START = 0x20;
    constexpr std::uint8_t ITEM_SET_END = 0x21;
    constexpr std::uint8_t ITEM_SET_FINE_TUNE = 0x24;
    constexpr std::uint8_t ITEM_SET_LOOP_START = 0x29;
    constexpr std::uint8_t ITEM_SET_LOOP_END = 0x2A;
    constexpr std::uint8_t RAM = 0;
    constexpr std::uint8_t MONO = 1;
    constexpr std::uint32_t KICK_LENGTH = 22050;
    constexpr std::uint32_t KICK_RATE = 44100;
    constexpr std::int64_t FINE = 10;
    constexpr std::int64_t C3 = 60;
    constexpr std::size_t TRIM_GROUP_PARAMETERS = 2;
    constexpr std::size_t TAB_PARAMETERS = 8;
    constexpr std::size_t INFO_ROWS = 4;
    constexpr int PAD_KEYGROUPS = 2;
    constexpr std::size_t PROGRAM_TAB_PARAMETERS = 14;
    constexpr std::size_t KEYGROUP_TAB = 0;
    constexpr std::size_t ENVELOPES_TAB = 1;
    constexpr std::size_t FILTER_TAB = 2;
    constexpr std::size_t LFO_TAB = 3;
    constexpr std::size_t MODULATION_TAB = 4;
    constexpr std::size_t PROGRAM_TAB = 5;

    const mcp::ParameterDefinition& sampleParameter(const std::string& name)
    {
        const auto& rows = mcp::ParameterCatalogue::samples().parameters();
        return *std::ranges::find_if(rows, [&name](const mcp::ParameterDefinition& row) { return row.name == name; });
    }

    /// The program PAD (2 keygroups), the multi M1, the samples KICK (RAM, mono, 22050 points at 44100 Hz, played to its end, fine tune +10, root C3) and SNARE; connected.
    struct EditRig
    {
        EditRig()
        {
            mcp::test::seedSimulatedPrograms(rig.backend, {{"PAD", PAD_KEYGROUPS}}, 0);
            rig.sampler->setSampleNames({"KICK", "SNARE"});
            rig.sampler->setMultiNames({"M1"});
            rig.sampler->setSampleAttributes(0, RAM, MONO, KICK_LENGTH, KICK_RATE);
            set("sample end position", KICK_LENGTH);  // a real sample plays to its end
            set("sample fine tune", FINE);
            set("sample original pitch", C3);
            rig.controller.connect(setupA());
            REQUIRE(rig.settle());
            REQUIRE(rig.model.connection().state == ConnectionState::Connected);
        }

        /// A value of KICK put on the sampler from outside the application (the front panel).
        void set(const std::string& name, std::int64_t value)
        {
            REQUIRE(rig.gateway.selectSampleByName("KICK").ok());
            REQUIRE(rig.gateway.writeSampleParameter(sampleParameter(name), value).ok());
        }

        [[nodiscard]] std::optional<std::int64_t> shown(const std::string& name) const
        {
            for (const app::EditValue& value : rig.model.edit().values)
            {
                if (value.definition->name == name)
                    return value.value;
            }
            return std::nullopt;
        }

        [[nodiscard]] std::size_t index(const std::string& name) const
        {
            const auto& values = rig.model.edit().values;
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                if (values[i].definition->name == name)
                    return i;
            }
            FAIL_CHECK("no parameter " << name);
            return 0;
        }

        /// The value shown for `name` in the column of zone `zone` (1 to 4) of a zone grid. [RQ-APP-060, DEC-APP-033]
        [[nodiscard]] std::optional<std::int64_t> shownIn(const std::string& name, int zone) const
        {
            for (const app::EditValue& value : rig.model.edit().values)
            {
                if (value.definition->name == name && value.zone == zone)
                    return value.value;
            }
            return std::nullopt;
        }

        [[nodiscard]] std::size_t indexIn(const std::string& name, int zone) const
        {
            const auto& values = rig.model.edit().values;
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                if (values[i].definition->name == name && values[i].zone == zone)
                    return i;
            }
            FAIL_CHECK("no parameter " << name << " in zone " << zone);
            return 0;
        }

        [[nodiscard]] std::size_t sent(std::uint8_t item) const
        {
            const auto commands = rig.sampler->acceptedCommands();
            return static_cast<std::size_t>(std::count_if(commands.begin(), commands.end(), [item](const akm::harness::AcceptedCommand& command) {
                return command.section == SECTION_SAMPLE && command.item == item;
            }));
        }

        [[nodiscard]] std::size_t sentToSamples() const
        {
            const auto commands = rig.sampler->acceptedCommands();
            return static_cast<std::size_t>(std::count_if(commands.begin(), commands.end(), [](const akm::harness::AcceptedCommand& command) {
                return command.section == SECTION_SAMPLE;
            }));
        }

        CoreRig rig;
        app::MemoryController memory{rig.model, rig.controller};
        app::EditController editor{rig.model, rig.controller};
    };
}

TEST_CASE("Given KICK, When it is selected in the memory view, Then the sample editor shows its eight parameters read from the sampler in the "
          "layout's order and its information, the positions bounded by its length [RQ-APP-086, RQ-APP-087, RQ-APP-053, TASK-APP-036]",
          "[app][edit]")
{
    EditRig fixture;
    fixture.memory.select(MemoryItem{MemoryKind::Sample, "KICK"});
    REQUIRE(fixture.rig.settle());

    const app::EditState& edit = fixture.rig.model.edit();
    REQUIRE(edit.item == std::optional<MemoryItem>(MemoryItem{MemoryKind::Sample, "KICK"}));
    REQUIRE(edit.layout != nullptr);
    CHECK(edit.layout->editor == "sample");
    REQUIRE(edit.values.size() == TAB_PARAMETERS);
    CHECK(edit.values[0].definition->name == "sample start position");
    CHECK(edit.values[TRIM_GROUP_PARAMETERS].definition->name == "sample loop start");
    CHECK(fixture.shown("sample fine tune") == std::optional<std::int64_t>(FINE));
    CHECK(fixture.shown("sample original pitch") == std::optional<std::int64_t>(C3));
    CHECK(edit.values[0].max == KICK_LENGTH);  // a position stays within the sample [RQ-APP-088]
    REQUIRE(edit.info.size() == INFO_ROWS);
    CHECK(edit.info[0].value == RAM);
    CHECK(edit.info[1].value == MONO);
    CHECK(edit.info[2].value == KICK_LENGTH);
    CHECK(edit.info[3].value == KICK_RATE);
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given the multi M1, When it is selected, Then the panel names it and has no editor for it, and nothing of the sample section is "
          "sent [RQ-APP-048, TASK-APP-036]",
          "[app][edit]")
{
    EditRig fixture;
    const std::size_t before = fixture.sentToSamples();
    fixture.memory.select(MemoryItem{MemoryKind::Multi, "M1"});
    REQUIRE(fixture.rig.settle());

    const app::EditState& edit = fixture.rig.model.edit();
    CHECK(edit.item == std::optional<MemoryItem>(MemoryItem{MemoryKind::Multi, "M1"}));
    CHECK(edit.layout == nullptr);
    CHECK(edit.values.empty());
    CHECK(fixture.sentToSamples() == before);
}

namespace
{
    const mcp::ParameterDefinition& programParameter(const std::string& name)
    {
        const auto& rows = mcp::ParameterCatalogue::standard().parameters();
        return *std::ranges::find_if(rows, [&name](const mcp::ParameterDefinition& row) { return row.name == name; });
    }

    constexpr std::int64_t PAD_LOUDNESS = 80;
    constexpr std::int64_t LEGATO_ON = 1;

    /// PAD with its loudness 80 and legato on, put on the sampler from outside the application, then selected in the memory view.
    void showPad(EditRig& fixture)
    {
        REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
        REQUIRE(fixture.rig.gateway.writeParameter(programParameter("program loudness"), PAD_LOUDNESS, mcp::KeygroupSelection::all()).ok());
        REQUIRE(fixture.rig.gateway.writeParameter(programParameter("legato"), LEGATO_ON, mcp::KeygroupSelection::all()).ok());
        fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
        REQUIRE(fixture.rig.settle());
        fixture.editor.showTab(PROGRAM_TAB);
        REQUIRE(fixture.rig.settle());
    }
}

TEST_CASE("Given the program PAD of 2 keygroups, When it is selected, Then the program editor shows its keygroup count and the tab Program its "
          "fourteen parameters read from the sampler [RQ-APP-093, RQ-APP-048, TASK-APP-043]",
          "[app][edit]")
{
    EditRig fixture;
    showPad(fixture);

    const app::EditState& edit = fixture.rig.model.edit();
    REQUIRE(edit.layout != nullptr);
    CHECK(edit.layout->editor == "program");
    CHECK(edit.tab == PROGRAM_TAB);
    REQUIRE(edit.values.size() == PROGRAM_TAB_PARAMETERS);
    CHECK(edit.values[0].definition->name == "program loudness");
    CHECK(fixture.shown("program loudness") == std::optional<std::int64_t>(PAD_LOUDNESS));
    CHECK(edit.keygroups == std::optional<int>(PAD_KEYGROUPS));
    CHECK(app::infoText(edit) == "2 keygroups");
    CHECK(app::valueText(edit.values[fixture.index("legato")]) == "ON");
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given PAD shown, When the person sets its loudness to 70 and types legato off, Then each is sent, read back and shown "
          "[RQ-APP-093, RQ-APP-052, TASK-APP-043]",
          "[app][edit]")
{
    EditRig fixture;
    showPad(fixture);

    constexpr std::int64_t NEW_LOUDNESS = 70;
    fixture.editor.set(fixture.index("program loudness"), NEW_LOUDNESS);
    const auto off = app::valueFrom(fixture.rig.model.edit().values[fixture.index("legato")], "off");
    REQUIRE(off == std::optional<std::int64_t>(0));
    fixture.editor.set(fixture.index("legato"), *off);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shown("program loudness") == std::optional<std::int64_t>(NEW_LOUDNESS));
    CHECK(fixture.shown("legato") == std::optional<std::int64_t>(0));
    REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
    const auto read = fixture.rig.gateway.readParameter(programParameter("program loudness"), mcp::KeygroupSelection::all());
    REQUIRE(read.ok());
    CHECK(read.value->front().value == NEW_LOUDNESS);
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given KICK shown, When its fine tune is changed on the front panel and the person asks to read again, Then the new value is shown; "
          "When the session ends, Then the panel is empty [RQ-APP-053, RQ-APP-019, TASK-APP-036]",
          "[app][edit]")
{
    EditRig fixture;
    fixture.memory.select(MemoryItem{MemoryKind::Sample, "KICK"});
    REQUIRE(fixture.rig.settle());

    constexpr std::int64_t NEW_FINE = 20;
    fixture.set("sample fine tune", NEW_FINE);
    fixture.editor.readAgain();
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shown("sample fine tune") == std::optional<std::int64_t>(NEW_FINE));

    fixture.rig.controller.disconnect();
    REQUIRE(fixture.rig.settle());
    CHECK_FALSE(fixture.rig.model.edit().item.has_value());
}

TEST_CASE("Given KICK shown, When the person sets its fine tune to 15, Then it is pending until the sampler answers, then the value read back "
          "is shown [RQ-APP-052, RQ-APP-086, TASK-APP-037]",
          "[app][edit]")
{
    EditRig fixture;
    fixture.memory.select(MemoryItem{MemoryKind::Sample, "KICK"});
    REQUIRE(fixture.rig.settle());

    constexpr std::int64_t NEW_FINE = 15;
    const std::size_t fine = fixture.index("sample fine tune");
    const std::size_t before = fixture.sent(ITEM_SET_FINE_TUNE);  // the fixture's own seeding wrote it once
    fixture.editor.set(fine, NEW_FINE);
    CHECK(fixture.rig.model.edit().values[fine].pending);
    REQUIRE(fixture.rig.settle());
    CHECK_FALSE(fixture.rig.model.edit().values[fine].pending);
    CHECK(fixture.shown("sample fine tune") == std::optional<std::int64_t>(NEW_FINE));
    CHECK(fixture.sent(ITEM_SET_FINE_TUNE) == before + 1);
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given KICK of 22050 points, When the person sets its end to 30000, Then nothing is sent and the range 0 to 22050 is shown "
          "[RQ-APP-088, RQ-APP-051, TASK-APP-037]",
          "[app][edit]")
{
    EditRig fixture;
    fixture.memory.select(MemoryItem{MemoryKind::Sample, "KICK"});
    REQUIRE(fixture.rig.settle());

    constexpr std::int64_t BEYOND = 30000;
    const std::size_t ends = fixture.sent(ITEM_SET_END);
    fixture.editor.set(fixture.index("sample end position"), BEYOND);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_SET_END) == ends);
    REQUIRE(fixture.rig.model.problem().has_value());
    CHECK(fixture.rig.model.problem()->reason.find("0 to 22050") != std::string::npos);
}

namespace
{
    constexpr std::int64_t LOOP_START = 5000;
    constexpr std::int64_t LOOP_END = 15000;

    /// KICK shown with the loop 5000 to 15000, on a sampler whose Set Loop End keeps the loop length, as the real S5000's does.
    void showLoopingKick(EditRig& fixture)
    {
        fixture.rig.sampler->setSampleLoopEndKeepsLength(0, true);
        fixture.set("sample loop end", LOOP_END);
        fixture.set("sample loop start", LOOP_START);
        fixture.memory.select(MemoryItem{MemoryKind::Sample, "KICK"});
        REQUIRE(fixture.rig.settle());
        REQUIRE(fixture.shown("sample loop start") == std::optional<std::int64_t>(LOOP_START));
        REQUIRE(fixture.shown("sample loop end") == std::optional<std::int64_t>(LOOP_END));
    }
}

TEST_CASE("Given KICK looping from 5000 to 15000 on a sampler that keeps the loop length, When the person sets the loop end to 20000, Then the "
          "loop reads 5000 to 20000 [RQ-APP-090, TASK-APP-041]",
          "[app][edit]")
{
    EditRig fixture;
    showLoopingKick(fixture);

    constexpr std::int64_t NEW_END = 20000;
    fixture.editor.set(fixture.index("sample loop end"), NEW_END);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shown("sample loop start") == std::optional<std::int64_t>(LOOP_START));
    CHECK(fixture.shown("sample loop end") == std::optional<std::int64_t>(NEW_END));
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given KICK looping from 5000 to 15000, When the person sets the loop end below the loop start or the loop start beyond the loop end, "
          "Then nothing is sent and the bound is shown [RQ-APP-090, TASK-APP-041]",
          "[app][edit]")
{
    EditRig fixture;
    showLoopingKick(fixture);
    const std::size_t ends = fixture.sent(ITEM_SET_LOOP_END);
    const std::size_t starts = fixture.sent(ITEM_SET_LOOP_START);

    constexpr std::int64_t BELOW_START = 4000;
    fixture.editor.set(fixture.index("sample loop end"), BELOW_START);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_SET_LOOP_END) == ends);
    REQUIRE(fixture.rig.model.problem().has_value());
    CHECK(fixture.rig.model.problem()->reason.find("from 5000 to 22050") != std::string::npos);

    constexpr std::int64_t BEYOND_END = 16000;
    fixture.editor.set(fixture.index("sample loop start"), BEYOND_END);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_SET_LOOP_START) == starts);
    REQUIRE(fixture.rig.model.problem().has_value());
    CHECK(fixture.rig.model.problem()->reason.find("from 0 to 15000") != std::string::npos);
}

TEST_CASE("Given KICK trimmed 1000 to 20000 and looping 5000 to 15000, When the person sets the end below the loop end or the start beyond the "
          "loop start, Then nothing is sent and the range its neighbours leave is shown [RQ-APP-092, TASK-APP-042]",
          "[app][edit]")
{
    EditRig fixture;
    constexpr std::int64_t TRIM_START = 1000;
    constexpr std::int64_t TRIM_END = 20000;
    fixture.set("sample end position", TRIM_END);
    fixture.set("sample start position", TRIM_START);
    showLoopingKick(fixture);
    const std::size_t ends = fixture.sent(ITEM_SET_END);
    const std::size_t starts = fixture.sent(ITEM_SET_START);

    constexpr std::int64_t BELOW_LOOP_END = 14000;
    fixture.editor.set(fixture.index("sample end position"), BELOW_LOOP_END);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_SET_END) == ends);
    REQUIRE(fixture.rig.model.problem().has_value());
    CHECK(fixture.rig.model.problem()->reason.find("from 15000 to 22050") != std::string::npos);

    constexpr std::int64_t BEYOND_LOOP_START = 6000;
    fixture.editor.set(fixture.index("sample start position"), BEYOND_LOOP_START);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_SET_START) == starts);
    REQUIRE(fixture.rig.model.problem().has_value());
    CHECK(fixture.rig.model.problem()->reason.find("from 0 to 5000") != std::string::npos);

    constexpr std::int64_t NEW_END = 18000;
    fixture.editor.set(fixture.index("sample end position"), NEW_END);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shown("sample end position") == std::optional<std::int64_t>(NEW_END));
}

TEST_CASE("Given KICK shown, When the fine tune is moved through 40 values before the sampler answers, Then at most one write waits, only the "
          "first and the last are sent, and the last is the value read back [RQ-APP-054, TASK-APP-037]",
          "[app][edit]")
{
    EditRig fixture;
    fixture.memory.select(MemoryItem{MemoryKind::Sample, "KICK"});
    REQUIRE(fixture.rig.settle());

    constexpr std::int64_t FIRST = -20;
    constexpr std::int64_t LAST = 19;
    const std::size_t fine = fixture.index("sample fine tune");
    const std::size_t before = fixture.sent(ITEM_SET_FINE_TUNE);
    for (std::int64_t value = FIRST; value <= LAST; ++value)
        fixture.editor.set(fine, value);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.sent(ITEM_SET_FINE_TUNE) == before + 2);
    CHECK(fixture.shown("sample fine tune") == std::optional<std::int64_t>(LAST));
    CHECK_FALSE(fixture.rig.model.edit().values[fine].pending);
}

TEST_CASE("Given KICK shown and its semitone tune changed on the front panel, When the person sets its fine tune, Then every value of the tab is "
          "read back, the semitone tune included [RQ-APP-089, TASK-APP-037]",
          "[app][edit]")
{
    EditRig fixture;
    fixture.memory.select(MemoryItem{MemoryKind::Sample, "KICK"});
    REQUIRE(fixture.rig.settle());

    constexpr std::int64_t SEMITONES = 7;
    constexpr std::int64_t NEW_FINE = 5;
    fixture.set("sample semitone tune", SEMITONES);
    fixture.editor.set(fixture.index("sample fine tune"), NEW_FINE);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shown("sample semitone tune") == std::optional<std::int64_t>(SEMITONES));
    CHECK(fixture.shown("sample fine tune") == std::optional<std::int64_t>(NEW_FINE));
}

namespace
{
    app::EditValue valueOf(const std::string& name, std::int64_t value)
    {
        const mcp::ParameterDefinition& definition = sampleParameter(name);
        return app::EditValue{&definition, nullptr, value, definition.min, definition.max, false};
    }
}

TEST_CASE("Given sample values, When they are written for the person, Then a root is a note name, a tune is signed, a mode is its label and a "
          "position is its number [RQ-APP-051, RQ-APP-086, TASK-APP-038]",
          "[app][edit]")
{
    constexpr std::int64_t A_MINUS_1 = 21;
    constexpr std::int64_t G8 = 127;
    constexpr std::int64_t C_SHARP_3 = 61;
    constexpr std::int64_t DOWN = -5;
    constexpr std::int64_t ONE_SHOT = 1;
    constexpr std::int64_t POINT = 1024;
    CHECK(app::valueText(valueOf("sample original pitch", C3)) == "C3");
    CHECK(app::valueText(valueOf("sample original pitch", A_MINUS_1)) == "A-1");
    CHECK(app::valueText(valueOf("sample original pitch", G8)) == "G8");
    CHECK(app::valueText(valueOf("sample original pitch", C_SHARP_3)) == "C#3");
    CHECK(app::valueText(valueOf("sample fine tune", FINE)) == "+10");
    CHECK(app::valueText(valueOf("sample fine tune", DOWN)) == "-5");
    CHECK(app::valueText(valueOf("sample fine tune", 0)) == "0");
    CHECK(app::valueText(valueOf("sample playback mode", ONE_SHOT)) == "ONE SHOT");
    CHECK(app::valueText(valueOf("sample start position", POINT)) == "1024");
}

TEST_CASE("Given a text typed by the person, When it is read for a parameter, Then a note name, a signed number, a label or a number gives its "
          "value, and anything else gives none [RQ-APP-051, RQ-APP-086, TASK-APP-038]",
          "[app][edit]")
{
    CHECK(app::valueFrom(valueOf("sample original pitch", 0), "C3") == std::optional<std::int64_t>(C3));
    CHECK(app::valueFrom(valueOf("sample original pitch", 0), "c#3") == std::optional<std::int64_t>(C3 + 1));
    CHECK(app::valueFrom(valueOf("sample original pitch", 0), "60") == std::optional<std::int64_t>(C3));
    CHECK(app::valueFrom(valueOf("sample fine tune", 0), "+10") == std::optional<std::int64_t>(FINE));
    CHECK(app::valueFrom(valueOf("sample fine tune", 0), "-5") == std::optional<std::int64_t>(-5));
    CHECK(app::valueFrom(valueOf("sample playback mode", 0), "one shot") == std::optional<std::int64_t>(1));
    CHECK(app::valueFrom(valueOf("sample start position", 0), " 2048 ") == std::optional<std::int64_t>(2048));
    CHECK_FALSE(app::valueFrom(valueOf("sample start position", 0), "abc").has_value());
    CHECK_FALSE(app::valueFrom(valueOf("sample original pitch", 0), "H3").has_value());
}

TEST_CASE("Given KICK read, Then its information line reads RAM, mono, its length in points and its rate in Hz [RQ-APP-087, TASK-APP-038]",
          "[app][edit]")
{
    EditRig fixture;
    fixture.memory.select(MemoryItem{MemoryKind::Sample, "KICK"});
    REQUIRE(fixture.rig.settle());
    CHECK(app::infoText(fixture.rig.model.edit()) == "RAM, mono, 22050 points, 44100 Hz");
}

namespace
{
    constexpr std::int64_t FIRST_ATTACK = 10;
    constexpr std::int64_t SECOND_ATTACK = 40;
    constexpr int SECOND_KEYGROUP = 2;

    /// PAD's amp attacks 10 and 40, put on the sampler from outside the application, then PAD selected and its Envelopes tab shown.
    void showPadEnvelopes(EditRig& fixture)
    {
        const mcp::ParameterDefinition& attack = programParameter("amplitude envelope attack");
        REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
        REQUIRE(fixture.rig.gateway.writeParameter(attack, FIRST_ATTACK, mcp::KeygroupSelection::of(1)).ok());
        REQUIRE(fixture.rig.gateway.writeParameter(attack, SECOND_ATTACK, mcp::KeygroupSelection::of(SECOND_KEYGROUP)).ok());
        fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
        REQUIRE(fixture.rig.settle());
        fixture.editor.showTab(ENVELOPES_TAB);
        REQUIRE(fixture.rig.settle());
    }

    std::int64_t attackOf(EditRig& fixture, int keygroup)
    {
        REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
        const auto read = fixture.rig.gateway.readParameter(programParameter("amplitude envelope attack"), mcp::KeygroupSelection::of(keygroup));
        REQUIRE(read.ok());
        return read.value->front().value;
    }
}

TEST_CASE("Given PAD of 2 keygroups with amp attacks 10 and 40, When it is selected, Then the Envelopes tab shows keygroup 1's attack; When "
          "keygroup 2 is chosen, Then it shows 40; When the person sets it to 50, Then keygroup 2 reads 50 and keygroup 1 still 10 "
          "[RQ-APP-094, RQ-APP-058, TASK-APP-044]",
          "[app][edit]")
{
    EditRig fixture;
    showPadEnvelopes(fixture);
    const app::EditState& edit = fixture.rig.model.edit();
    REQUIRE(edit.layout != nullptr);
    CHECK(edit.layout->tabs[edit.tab].title == "Envelopes");
    CHECK(edit.keygroup == 1);
    CHECK(fixture.shown("amplitude envelope attack") == std::optional<std::int64_t>(FIRST_ATTACK));

    fixture.editor.chooseKeygroup(SECOND_KEYGROUP);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.rig.model.edit().keygroup == SECOND_KEYGROUP);
    CHECK(fixture.shown("amplitude envelope attack") == std::optional<std::int64_t>(SECOND_ATTACK));

    constexpr std::int64_t NEW_ATTACK = 50;
    fixture.editor.set(fixture.index("amplitude envelope attack"), NEW_ATTACK);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shown("amplitude envelope attack") == std::optional<std::int64_t>(NEW_ATTACK));
    CHECK(attackOf(fixture, SECOND_KEYGROUP) == NEW_ATTACK);
    CHECK(attackOf(fixture, 1) == FIRST_ATTACK);
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given PAD of 2 keygroups shown, When the person chooses keygroup 3 or 0, Then nothing changes [RQ-APP-058, TASK-APP-044]", "[app][edit]")
{
    EditRig fixture;
    showPadEnvelopes(fixture);
    for (const int none : {0, PAD_KEYGROUPS + 1})
    {
        fixture.editor.chooseKeygroup(none);
        REQUIRE(fixture.rig.settle());
        CHECK(fixture.rig.model.edit().keygroup == 1);
    }
}

TEST_CASE("Given PAD shown on its Envelopes tab, When the person shows the Program tab, Then its values are read; When the tab is shown again, "
          "Then nothing is read [RQ-APP-049, RQ-APP-053, TASK-APP-044]",
          "[app][edit]")
{
    EditRig fixture;
    showPadEnvelopes(fixture);
    fixture.editor.showTab(PROGRAM_TAB);
    REQUIRE(fixture.rig.settle());
    const app::EditState& edit = fixture.rig.model.edit();
    CHECK(edit.tab == PROGRAM_TAB);
    REQUIRE(edit.values.size() == PROGRAM_TAB_PARAMETERS);
    CHECK(edit.values[0].definition->name == "program loudness");
    CHECK(edit.keygroups == std::optional<int>(PAD_KEYGROUPS));

    const std::size_t before = fixture.rig.sampler->acceptedCommands().size();
    fixture.editor.showTab(PROGRAM_TAB);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
}

TEST_CASE("Given PAD of 2 keygroups with cutoffs 50 and 70, When keygroup 2 is chosen and the Filter tab shown, Then it shows keygroup 2's "
          "filter and its group reads KEYGROUP [RQ-APP-057, RQ-APP-058, TASK-APP-045]",
          "[app][edit]")
{
    constexpr std::int64_t FIRST_CUTOFF = 50;
    constexpr std::int64_t SECOND_CUTOFF = 70;
    constexpr std::size_t FILTER_PARAMETERS = 5;
    EditRig fixture;
    const mcp::ParameterDefinition& cutoff = programParameter("filter cutoff");
    REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
    REQUIRE(fixture.rig.gateway.writeParameter(cutoff, FIRST_CUTOFF, mcp::KeygroupSelection::of(1)).ok());
    REQUIRE(fixture.rig.gateway.writeParameter(cutoff, SECOND_CUTOFF, mcp::KeygroupSelection::of(SECOND_KEYGROUP)).ok());
    fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
    REQUIRE(fixture.rig.settle());

    fixture.editor.chooseKeygroup(SECOND_KEYGROUP);
    REQUIRE(fixture.rig.settle());
    fixture.editor.showTab(FILTER_TAB);
    REQUIRE(fixture.rig.settle());
    const app::EditState& edit = fixture.rig.model.edit();
    REQUIRE(edit.layout != nullptr);
    REQUIRE(edit.layout->tabs.size() > FILTER_TAB);
    const app::layouts::LayoutTab& tab = edit.layout->tabs[FILTER_TAB];
    CHECK(tab.title == "Filter");
    REQUIRE(tab.groups.size() == 1);
    CHECK(tab.groups[0].scope == app::layouts::Scope::Keygroup);
    CHECK(edit.tab == FILTER_TAB);
    CHECK(edit.values.size() == FILTER_PARAMETERS);
    CHECK(fixture.shown("filter cutoff") == std::optional<std::int64_t>(SECOND_CUTOFF));
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given PAD shown on its Filter tab, When the person chooses 4-POLE LP, Then it is sent, read back and shown, the layout drawing its "
          "shape [RQ-APP-096, RQ-APP-052, TASK-APP-051]",
          "[app][edit]")
{
    constexpr std::int64_t FOUR_POLE_LOW_PASS = 1;
    EditRig fixture;
    fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
    REQUIRE(fixture.rig.settle());
    fixture.editor.showTab(FILTER_TAB);
    REQUIRE(fixture.rig.settle());
    fixture.editor.set(fixture.index("filter type"), FOUR_POLE_LOW_PASS);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shown("filter type") == std::optional<std::int64_t>(FOUR_POLE_LOW_PASS));
    REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
    const auto read = fixture.rig.gateway.readParameter(programParameter("filter type"), mcp::KeygroupSelection::of(1));
    REQUIRE(read.ok());
    CHECK(read.value->front().value == FOUR_POLE_LOW_PASS);

    const app::EditState& edit = fixture.rig.model.edit();
    const app::EditValue& type = edit.values[fixture.index("filter type")];
    const auto& parameter = edit.layout->tabs[FILTER_TAB].groups[0].parameters[0];
    CHECK_FALSE(app::layouts::shapeOf(parameter, type.definition->labels[static_cast<std::size_t>(type.value)]).empty());
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given PAD's Filter tab, When the person sets the attenuation to 12, Then it is sent and reads 12; When the person sets it to 1, "
          "Then nothing is sent and the problem says the steps [RQ-APP-097, DEC-APP-036, TASK-APP-052]",
          "[app][edit]")
{
    constexpr std::int64_t TWELVE_DB = 12;
    constexpr std::int64_t ONE_DB = 1;
    EditRig fixture;
    fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
    REQUIRE(fixture.rig.settle());
    fixture.editor.showTab(FILTER_TAB);
    REQUIRE(fixture.rig.settle());
    fixture.editor.set(fixture.index("filter attenuation"), TWELVE_DB);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shown("filter attenuation") == std::optional<std::int64_t>(TWELVE_DB));
    CHECK_FALSE(fixture.rig.model.problem().has_value());

    const std::size_t before = fixture.rig.sampler->acceptedCommands().size();
    fixture.editor.set(fixture.index("filter attenuation"), ONE_DB);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
    CHECK(fixture.shown("filter attenuation") == std::optional<std::int64_t>(TWELVE_DB));
    REQUIRE(fixture.rig.model.problem().has_value());
    CHECK(fixture.rig.model.problem()->reason.find("0, 6, 12, 18, 24 or 30 dB") != std::string::npos);
    CHECK(fixture.rig.model.problem()->reason.find("nothing was sent") != std::string::npos);
}

TEST_CASE("Given a choice and a value in steps, When a control steps them, Then a choice goes to its previous or next code and a number by "
          "its step, and nothing lies past the ends [RQ-APP-098, RQ-APP-097, DEC-APP-036, TASK-APP-053]",
          "[app][edit]")
{
    constexpr int PREVIOUS = -1;
    constexpr int NEXT = 1;
    constexpr std::int64_t TWELVE_DB = 12;
    constexpr std::int64_t SIX_DB = 6;
    constexpr std::int64_t EIGHTEEN_DB = 18;
    constexpr std::int64_t THIRTY_DB = 30;
    const mcp::ParameterDefinition& type = programParameter("filter type");
    const auto last = static_cast<std::int64_t>(type.labels.size()) - 1;
    const app::EditValue first{&type, nullptr, 0, type.min, type.max, false, 0};
    CHECK(app::steppedValue(first, NEXT) == std::optional<std::int64_t>(1));
    CHECK_FALSE(app::steppedValue(first, PREVIOUS).has_value());
    const app::EditValue lastType{&type, nullptr, last, type.min, type.max, false, 0};
    CHECK(app::steppedValue(lastType, PREVIOUS) == std::optional<std::int64_t>(last - 1));
    CHECK_FALSE(app::steppedValue(lastType, NEXT).has_value());

    const mcp::ParameterDefinition& attenuation = programParameter("filter attenuation");
    const app::EditValue twelve{&attenuation, nullptr, TWELVE_DB, attenuation.min, attenuation.max, false, 0};
    CHECK(app::steppedValue(twelve, NEXT) == std::optional<std::int64_t>(EIGHTEEN_DB));
    CHECK(app::steppedValue(twelve, PREVIOUS) == std::optional<std::int64_t>(SIX_DB));
    const app::EditValue thirty{&attenuation, nullptr, THIRTY_DB, attenuation.min, attenuation.max, false, 0};
    CHECK_FALSE(app::steppedValue(thirty, NEXT).has_value());
}

TEST_CASE("Given PAD's Filter tab on 2-POLE LP, When the right arrow steps it, Then 4-POLE LP is sent and read back; When the left arrow steps "
          "it twice, Then 2-POLE LP is sent once and nothing more [RQ-APP-098, DEC-APP-036, TASK-APP-053]",
          "[app][edit]")
{
    constexpr int PREVIOUS = -1;
    constexpr int NEXT = 1;
    constexpr std::int64_t FOUR_POLE_LOW_PASS = 1;
    EditRig fixture;
    fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
    REQUIRE(fixture.rig.settle());
    fixture.editor.showTab(FILTER_TAB);
    REQUIRE(fixture.rig.settle());
    REQUIRE(fixture.shown("filter type") == std::optional<std::int64_t>(0));
    const auto step = [&fixture](int direction) {
        const std::size_t index = fixture.index("filter type");
        if (const auto value = app::steppedValue(fixture.rig.model.edit().values[index], direction))
            fixture.editor.set(index, *value);
        REQUIRE(fixture.rig.settle());
    };
    step(NEXT);
    CHECK(fixture.shown("filter type") == std::optional<std::int64_t>(FOUR_POLE_LOW_PASS));
    step(PREVIOUS);
    CHECK(fixture.shown("filter type") == std::optional<std::int64_t>(0));
    const std::size_t before = fixture.rig.sampler->acceptedCommands().size();
    step(PREVIOUS);
    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
    CHECK(fixture.shown("filter type") == std::optional<std::int64_t>(0));
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

namespace
{
    constexpr int ZONES = 4;
    constexpr int SECOND_ZONE = 2;
    constexpr std::int64_t SECOND_ZONE_LEVEL = -3;

    /// PAD's keygroup 1 with zone 1 playing KICK and zone 2's level -3, put on the sampler from outside the application, then PAD selected:
    /// its first tab, Keygroup.
    void showPadZones(EditRig& fixture)
    {
        REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
        REQUIRE(fixture.rig.gateway.assignZoneSample(1, 1, "KICK").ok());
        const mcp::ParameterDefinition& level = programParameter("zone level");
        const auto zone = mcp::ZoneSelection::of(SECOND_ZONE);
        REQUIRE(fixture.rig.gateway.writeParameter(level, SECOND_ZONE_LEVEL, mcp::KeygroupSelection::of(1), zone).ok());
        fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
        REQUIRE(fixture.rig.settle());
        REQUIRE(fixture.rig.model.edit().tab == KEYGROUP_TAB);
    }

    /// The value of a zone's sample: 1 plus the sample's place among those in memory, 0 for none. [RQ-APP-060, DEC-APP-033]
    std::int64_t sampleValue(const EditRig& fixture, const std::string& name)
    {
        const auto& samples = fixture.rig.model.edit().samples;
        const auto found = std::ranges::find(samples, name);
        REQUIRE(found != samples.end());
        return static_cast<std::int64_t>(std::distance(samples.begin(), found)) + 1;
    }

    std::string zoneSampleOf(EditRig& fixture, int zone)
    {
        REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
        const auto read = fixture.rig.gateway.readZoneSamples(mcp::KeygroupSelection::of(1));
        REQUIRE(read.ok());
        const auto found = std::ranges::find_if(read.value->entries, [zone](const mcp::ZoneSampleEntry& entry) { return entry.zone == zone; });
        REQUIRE(found != read.value->entries.end());
        return found->sample;
    }
}

TEST_CASE("Given PAD's keygroup 1 with KICK in zone 1 and zone 2 at level -3, When PAD is selected, Then the Keygroup tab shows each zone in its "
          "column: zone 1 KICK, zones 2 to 4 no sample, zone 2's level -3 and zone 1's 0 [RQ-APP-060, RQ-APP-057, DEC-APP-033, TASK-APP-046]",
          "[app][edit]")
{
    EditRig fixture;
    showPadZones(fixture);
    const app::EditState& edit = fixture.rig.model.edit();
    CHECK(edit.layout->tabs[edit.tab].title == "Keygroup");
    CHECK(fixture.shownIn("zone sample", 1) == std::optional<std::int64_t>(sampleValue(fixture, "KICK")));
    for (int zone = 2; zone <= ZONES; ++zone)
        CHECK(fixture.shownIn("zone sample", zone) == std::optional<std::int64_t>(0));
    CHECK(fixture.shownIn("zone level", SECOND_ZONE) == std::optional<std::int64_t>(SECOND_ZONE_LEVEL));
    CHECK(fixture.shownIn("zone level", 1) == std::optional<std::int64_t>(0));
    CHECK_FALSE(fixture.shownIn("keygroup low note", 1).has_value());  // a keygroup value has no zone
    CHECK(fixture.shown("keygroup low note").has_value());
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given PAD's Keygroup tab, When the person assigns SNARE to zone 2, Then it is sent by name and read back, and zone 2 reads SNARE "
          "[RQ-APP-060, RQ-APP-052, DEC-APP-033, TASK-APP-046]",
          "[app][edit]")
{
    EditRig fixture;
    showPadZones(fixture);
    const std::int64_t snare = sampleValue(fixture, "SNARE");
    fixture.editor.set(fixture.indexIn("zone sample", SECOND_ZONE), snare);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shownIn("zone sample", SECOND_ZONE) == std::optional<std::int64_t>(snare));
    CHECK(zoneSampleOf(fixture, SECOND_ZONE) == "SNARE");
    CHECK(zoneSampleOf(fixture, 1) == "KICK");
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given PAD's Keygroup tab, When the person sets zone 3's level to 10, Then zone 3 alone reads 10 [RQ-APP-057, DEC-APP-033, TASK-APP-046]",
          "[app][edit]")
{
    constexpr int THIRD_ZONE = 3;
    constexpr std::int64_t LEVEL = 10;
    EditRig fixture;
    showPadZones(fixture);
    fixture.editor.set(fixture.indexIn("zone level", THIRD_ZONE), LEVEL);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shownIn("zone level", THIRD_ZONE) == std::optional<std::int64_t>(LEVEL));
    CHECK(fixture.shownIn("zone level", SECOND_ZONE) == std::optional<std::int64_t>(SECOND_ZONE_LEVEL));
    REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
    const mcp::ParameterDefinition& level = programParameter("zone level");
    const auto read = fixture.rig.gateway.readParameter(level, mcp::KeygroupSelection::of(1), mcp::ZoneSelection::of(THIRD_ZONE));
    REQUIRE(read.ok());
    CHECK(read.value->front().value == LEVEL);
}

TEST_CASE("Given zone 1 playing velocities 0 to 63, When the person sets zone 1's low velocity to 70, Then nothing is sent; zone 2's low velocity "
          "may be 70, its high velocity being 127 [RQ-APP-092, DEC-APP-029, DEC-APP-033, TASK-APP-046]",
          "[app][edit]")
{
    constexpr std::int64_t HIGH = 63;
    constexpr std::int64_t LOW = 70;
    EditRig fixture;
    REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
    constexpr std::int64_t TOP = 127;
    const mcp::ParameterDefinition& high = programParameter("zone high velocity");
    REQUIRE(fixture.rig.gateway.writeParameter(high, HIGH, mcp::KeygroupSelection::of(1), mcp::ZoneSelection::of(1)).ok());
    REQUIRE(fixture.rig.gateway.writeParameter(high, TOP, mcp::KeygroupSelection::of(1), mcp::ZoneSelection::of(SECOND_ZONE)).ok());
    showPadZones(fixture);
    const std::size_t before = fixture.rig.sampler->acceptedCommands().size();
    fixture.editor.set(fixture.indexIn("zone low velocity", 1), LOW);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
    CHECK(fixture.rig.model.problem().has_value());
    fixture.rig.model.clearProblem();
    fixture.editor.set(fixture.indexIn("zone low velocity", SECOND_ZONE), LOW);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shownIn("zone low velocity", SECOND_ZONE) == std::optional<std::int64_t>(LOW));
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given PAD's Keygroup tab with KICK in zone 1, When the person chooses no sample for it, Then nothing is sent and a problem says the "
          "editor cannot remove a zone's sample yet [RQ-APP-060, DEC-APP-033, TASK-APP-046]",
          "[app][edit]")
{
    EditRig fixture;
    showPadZones(fixture);
    const std::size_t before = fixture.rig.sampler->acceptedCommands().size();
    fixture.editor.set(fixture.indexIn("zone sample", 1), 0);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
    REQUIRE(fixture.rig.model.problem().has_value());
    CHECK(zoneSampleOf(fixture, 1) == "KICK");
}

namespace
{
    constexpr int ADDED = 2;
    constexpr int THIRD_KEYGROUP = 3;

    int padKeygroupsOnSampler(EditRig& fixture)
    {
        const auto selected = fixture.rig.gateway.selectProgramByName("PAD");
        REQUIRE(selected.ok());
        return selected.value->keygroupCount;
    }

    void selectPad(EditRig& fixture)
    {
        fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
        REQUIRE(fixture.rig.settle());
    }
}

TEST_CASE("Given PAD with 2 keygroups, When the person adds 2, Then the selector goes from 1 to 4 and PAD has 4 keygroups on the sampler; the "
          "question offers 1 to 97 [RQ-APP-061, DEC-APP-034, TASK-APP-047]",
          "[app][edit]")
{
    constexpr int MOST = 97;
    EditRig fixture;
    selectPad(fixture);
    fixture.rig.prompts.counts.emplace_back(ADDED);
    fixture.editor.addKeygroups();
    REQUIRE(fixture.rig.settle());
    REQUIRE(fixture.rig.prompts.countsAsked.size() == 1);
    CHECK(fixture.rig.prompts.countsAsked.front().min == 1);
    CHECK(fixture.rig.prompts.countsAsked.front().max == MOST);
    CHECK(fixture.rig.model.edit().keygroups == std::optional<int>(PAD_KEYGROUPS + ADDED));
    CHECK(padKeygroupsOnSampler(fixture) == PAD_KEYGROUPS + ADDED);
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given PAD with 2 keygroups, When the person cancels the count, Then nothing is sent [RQ-APP-061, DEC-APP-034, TASK-APP-047]",
          "[app][edit]")
{
    EditRig fixture;
    selectPad(fixture);
    const std::size_t before = fixture.rig.sampler->acceptedCommands().size();
    fixture.editor.addKeygroups();
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
    CHECK(fixture.rig.model.edit().keygroups == std::optional<int>(PAD_KEYGROUPS));
}

TEST_CASE("Given PAD with 4 keygroups, When the person deletes keygroup 3 and confirms, Then the question names keygroup 3 and PAD, and the "
          "selector goes from 1 to 3 [RQ-APP-061, RQ-APP-011, DEC-APP-034, TASK-APP-047]",
          "[app][edit]")
{
    EditRig fixture;
    selectPad(fixture);
    fixture.rig.prompts.counts.emplace_back(ADDED);
    fixture.editor.addKeygroups();
    REQUIRE(fixture.rig.settle());
    fixture.editor.chooseKeygroup(THIRD_KEYGROUP);
    REQUIRE(fixture.rig.settle());
    fixture.rig.prompts.answers.push_back(true);
    fixture.editor.deleteKeygroup();
    REQUIRE(fixture.rig.settle());
    REQUIRE(fixture.rig.prompts.asked.size() == 1);
    const app::Confirmation& question = fixture.rig.prompts.asked.front();
    CHECK(question.question.find("keygroup 3") != std::string::npos);
    CHECK(question.question.find("PAD") != std::string::npos);
    CHECK(fixture.rig.model.edit().keygroups == std::optional<int>(THIRD_KEYGROUP));
    CHECK(fixture.rig.model.edit().keygroup == THIRD_KEYGROUP);
    CHECK(padKeygroupsOnSampler(fixture) == THIRD_KEYGROUP);
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given PAD with 2 keygroups, When the person cancels the deletion, Then nothing is sent; Given PAD down to 1 keygroup, When the person "
          "deletes it, Then no question is asked and nothing is sent [RQ-APP-061, DEC-APP-034, TASK-APP-047]",
          "[app][edit]")
{
    EditRig fixture;
    selectPad(fixture);
    std::size_t before = fixture.rig.sampler->acceptedCommands().size();
    fixture.editor.deleteKeygroup();
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
    CHECK(fixture.rig.model.edit().keygroups == std::optional<int>(PAD_KEYGROUPS));

    fixture.rig.prompts.answers.push_back(true);
    fixture.editor.deleteKeygroup();
    REQUIRE(fixture.rig.settle());
    REQUIRE(fixture.rig.model.edit().keygroups == std::optional<int>(1));
    const std::size_t questions = fixture.rig.prompts.asked.size();
    before = fixture.rig.sampler->acceptedCommands().size();
    fixture.editor.deleteKeygroup();
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.rig.prompts.asked.size() == questions);
    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
    CHECK(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given PAD with LFO 1's rate 43, When the LFO tab is shown, Then it holds LFO 1 and LFO 2, each group reading PROGRAM, and "
          "the rate reads 43 [RQ-APP-056, RQ-APP-057, TASK-APP-048]",
          "[app][edit]")
{
    constexpr std::int64_t RATE = 43;
    constexpr std::size_t LFO_PARAMETERS = 14;
    EditRig fixture;
    REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
    REQUIRE(fixture.rig.gateway.writeParameter(programParameter("lfo 1 rate"), RATE, mcp::KeygroupSelection::all()).ok());
    fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
    REQUIRE(fixture.rig.settle());
    fixture.editor.showTab(LFO_TAB);
    REQUIRE(fixture.rig.settle());
    const app::EditState& edit = fixture.rig.model.edit();
    const app::layouts::LayoutTab& tab = edit.layout->tabs[edit.tab];
    CHECK(tab.title == "LFO");
    REQUIRE(tab.groups.size() == 2);
    for (const app::layouts::LayoutGroup& group : tab.groups)
        CHECK(group.scope == app::layouts::Scope::Program);
    CHECK(edit.values.size() == LFO_PARAMETERS);
    CHECK(fixture.shown("lfo 1 rate") == std::optional<std::int64_t>(RATE));
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given the Aux envelope routed to PAD's filter input 1 with an amount of 50 on keygroup 1, When the Modulation tab is shown, Then it "
          "shows the routing; When the person sets its amount to 70, Then it is sent and read back on keygroup 1 [RQ-APP-057, DEC-APP-035, "
          "TASK-APP-049]",
          "[app][edit]")
{
    constexpr std::int64_t AUX_ENV = 11;  // the source's code (spec Table 13)
    constexpr std::int64_t AMOUNT = 50;
    constexpr std::int64_t NEW_AMOUNT = 70;
    constexpr std::size_t ROUTING_VALUES = 34;
    EditRig fixture;
    REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
    REQUIRE(fixture.rig.gateway.writeParameter(programParameter("filter modulation 1 source"), AUX_ENV, mcp::KeygroupSelection::all()).ok());
    REQUIRE(fixture.rig.gateway.writeParameter(programParameter("filter modulation 1 amount"), AMOUNT, mcp::KeygroupSelection::of(1)).ok());
    fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
    REQUIRE(fixture.rig.settle());
    fixture.editor.showTab(MODULATION_TAB);
    REQUIRE(fixture.rig.settle());
    const app::EditState& edit = fixture.rig.model.edit();
    CHECK(edit.layout->tabs[edit.tab].title == "Modulation");
    CHECK(edit.values.size() == ROUTING_VALUES);
    CHECK(fixture.shown("filter modulation 1 source") == std::optional<std::int64_t>(AUX_ENV));
    CHECK(fixture.shown("filter modulation 1 amount") == std::optional<std::int64_t>(AMOUNT));

    fixture.editor.set(fixture.index("filter modulation 1 amount"), NEW_AMOUNT);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shown("filter modulation 1 amount") == std::optional<std::int64_t>(NEW_AMOUNT));
    REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
    const auto read = fixture.rig.gateway.readParameter(programParameter("filter modulation 1 amount"), mcp::KeygroupSelection::of(1));
    REQUIRE(read.ok());
    CHECK(read.value->front().value == NEW_AMOUNT);
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

namespace
{
    constexpr std::int64_t SHARED_CUTOFF = 50;
    constexpr std::int64_t OTHER_CUTOFF = 70;

    /// PAD grown to 3 keygroups with cutoffs 50, 50 and 70, put on the sampler from outside the application, then shown on its Filter tab
    /// with all keygroups chosen. [RQ-APP-059, TASK-APP-050]
    void showAllKeygroups(EditRig& fixture)
    {
        REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
        REQUIRE(fixture.rig.gateway.addKeygroups(1).ok());
        const mcp::ParameterDefinition& cutoff = programParameter("filter cutoff");
        for (const auto& [keygroup, value] : {std::pair{1, SHARED_CUTOFF}, std::pair{2, SHARED_CUTOFF}, std::pair{THIRD_KEYGROUP, OTHER_CUTOFF}})
            REQUIRE(fixture.rig.gateway.writeParameter(cutoff, value, mcp::KeygroupSelection::of(keygroup)).ok());
        fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
        REQUIRE(fixture.rig.settle());
        fixture.editor.showTab(FILTER_TAB);
        REQUIRE(fixture.rig.settle());
        fixture.editor.chooseKeygroup(app::ALL_KEYGROUPS);
        REQUIRE(fixture.rig.settle());
    }

    bool differs(const EditRig& fixture, const std::string& name)
    {
        return fixture.rig.model.edit().values[fixture.index(name)].differs;
    }
}

TEST_CASE("Given PAD's 3 keygroups with cutoffs 50, 50 and 70, When the person chooses all keygroups, Then the cutoff says the values differ and "
          "the attenuation, shared, does not; When the person sets the cutoff to 60, Then the three keygroups read 60 [RQ-APP-059, TASK-APP-050]",
          "[app][edit]")
{
    constexpr std::int64_t NEW_CUTOFF = 60;
    EditRig fixture;
    showAllKeygroups(fixture);
    REQUIRE(fixture.rig.model.edit().keygroup == app::ALL_KEYGROUPS);
    CHECK(fixture.rig.model.edit().keygroups == std::optional<int>(THIRD_KEYGROUP));
    CHECK(differs(fixture, "filter cutoff"));
    CHECK(app::valueText(fixture.rig.model.edit().values[fixture.index("filter cutoff")]) == "VARIES");
    CHECK_FALSE(differs(fixture, "filter attenuation"));
    CHECK_FALSE(fixture.rig.model.problem().has_value());

    fixture.editor.set(fixture.index("filter cutoff"), NEW_CUTOFF);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shown("filter cutoff") == std::optional<std::int64_t>(NEW_CUTOFF));
    CHECK_FALSE(differs(fixture, "filter cutoff"));
    REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
    const auto read = fixture.rig.gateway.readParameter(programParameter("filter cutoff"), mcp::KeygroupSelection::all());
    REQUIRE(read.ok());
    REQUIRE(read.value->size() == static_cast<std::size_t>(THIRD_KEYGROUP));
    for (const mcp::ParameterValue& value : *read.value)
        CHECK(value.value == NEW_CUTOFF);
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}

TEST_CASE("Given all keygroups chosen, When the person chooses keygroup 3, Then its own cutoff 70 is shown and nothing differs; When the person "
          "deletes with all keygroups chosen, Then nothing is sent and a problem asks for one keygroup [RQ-APP-059, RQ-APP-061, TASK-APP-050]",
          "[app][edit]")
{
    EditRig fixture;
    showAllKeygroups(fixture);
    const std::size_t before = fixture.rig.sampler->acceptedCommands().size();
    fixture.editor.deleteKeygroup();
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.rig.sampler->acceptedCommands().size() == before);
    REQUIRE(fixture.rig.model.problem().has_value());
    CHECK(fixture.rig.model.problem()->reason.find("one keygroup") != std::string::npos);
    CHECK(fixture.rig.model.edit().keygroups == std::optional<int>(THIRD_KEYGROUP));

    fixture.editor.chooseKeygroup(THIRD_KEYGROUP);
    REQUIRE(fixture.rig.settle());
    CHECK(fixture.shown("filter cutoff") == std::optional<std::int64_t>(OTHER_CUTOFF));
    CHECK_FALSE(differs(fixture, "filter cutoff"));
}

TEST_CASE("Given KICK in zone 1 of PAD's keygroup 1 only, When all keygroups are chosen on the Keygroup tab, Then zone 1's sample says the "
          "values differ and zone 2's, empty everywhere, does not [RQ-APP-059, RQ-APP-060, TASK-APP-050]",
          "[app][edit]")
{
    EditRig fixture;
    REQUIRE(fixture.rig.gateway.selectProgramByName("PAD").ok());
    REQUIRE(fixture.rig.gateway.assignZoneSample(1, 1, "KICK").ok());
    fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
    REQUIRE(fixture.rig.settle());
    REQUIRE(fixture.rig.model.edit().tab == KEYGROUP_TAB);
    fixture.editor.chooseKeygroup(app::ALL_KEYGROUPS);
    REQUIRE(fixture.rig.settle());
    const auto& values = fixture.rig.model.edit().values;
    CHECK(values[fixture.indexIn("zone sample", 1)].differs);
    CHECK_FALSE(values[fixture.indexIn("zone sample", SECOND_ZONE)].differs);
    CHECK_FALSE(fixture.rig.model.problem().has_value());
}
