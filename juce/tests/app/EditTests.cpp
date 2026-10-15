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

    const mcp::ParameterDefinition& sampleParameter(const std::string& name)
    {
        const auto& rows = mcp::ParameterCatalogue::samples().parameters();
        return *std::find_if(rows.begin(), rows.end(), [&name](const mcp::ParameterDefinition& row) { return row.name == name; });
    }

    /// The programs PAD, the samples KICK (RAM, mono, 22050 points at 44100 Hz, played to its end, fine tune +10, root C3) and SNARE; connected.
    struct EditRig
    {
        EditRig()
        {
            mcp::test::seedSimulatedPrograms(rig.backend, {{"PAD"}}, 0);
            rig.sampler->setSampleNames({"KICK", "SNARE"});
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

TEST_CASE("Given the program PAD, When it is selected, Then the panel names it and has no editor for it, and nothing of the sample section is "
          "sent [RQ-APP-048, TASK-APP-036]",
          "[app][edit]")
{
    EditRig fixture;
    const std::size_t before = fixture.sentToSamples();
    fixture.memory.select(MemoryItem{MemoryKind::Program, "PAD"});
    REQUIRE(fixture.rig.settle());

    const app::EditState& edit = fixture.rig.model.edit();
    CHECK(edit.item == std::optional<MemoryItem>(MemoryItem{MemoryKind::Program, "PAD"}));
    CHECK(edit.layout == nullptr);
    CHECK(edit.values.empty());
    CHECK(fixture.sentToSamples() == before);
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
