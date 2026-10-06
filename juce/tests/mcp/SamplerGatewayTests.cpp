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

// The sampler gateway of the MCP server on a real session and the simulated sampler: the connection opened when
// first needed, the programs, the keygroups, the parameters read, set and read back, and the sentences that say what
// went wrong. [TASK-MCP-004, RQ-MCP-003, RQ-MCP-005, RQ-MCP-006, RQ-MCP-007, RQ-MCP-009,
// ADR-MCP-001 (DEC-MCP-003, DEC-MCP-004, DEC-MCP-006)]
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "SimulatedPrograms.hpp"
#include "TestBytes.hpp"
#include "akm/ItemRequest.hpp"
#include "akm/RealScheduler.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"
#include "mcp/ParameterCatalogue.hpp"
#include "mcp/SamplerGateway.hpp"

using akm::harness::DeliveryMode;
using akm::harness::SimulatedMidiBackend;
using akm::harness::SimulatedSampler;
using mcp::KeygroupSelection;
using mcp::ParameterCatalogue;
using mcp::ParameterDefinition;
using mcp::ParameterValue;

namespace
{
    constexpr std::chrono::milliseconds COMMAND_TIMEOUT{300};

    /// A simulated sampler on a bus of its own, and the scheduler it is timed by.
    struct Rig
    {
        explicit Rig(bool withSampler = true)
        {
            if (withSampler)
                sampler = &backend.addSampler();
        }

        akm::RealScheduler scheduler;
        SimulatedMidiBackend backend{scheduler};
        SimulatedSampler* sampler = nullptr;
    };

    /// PAD (1 keygroup), BASS (3, cutoffs 30, 60, 90) and LEAD (2); BASS is current.
    void seedThreePrograms(Rig& rig)
    {
        mcp::test::seedThreePrograms(rig.backend);
    }

    mcp::GatewayConfig configFor(const Rig& rig)
    {
        mcp::GatewayConfig config;
        config.inputPort = rig.backend.inputName();
        config.outputPort = rig.backend.outputName();
        config.commandTimeout = COMMAND_TIMEOUT;
        return config;
    }

    const ParameterDefinition& named(const char* name)
    {
        const auto resolution = ParameterCatalogue::standard().resolveName(name);
        REQUIRE(resolution.parameter != nullptr);
        return *resolution.parameter;
    }

    std::vector<std::int64_t> valuesOf(const std::vector<ParameterValue>& read)
    {
        std::vector<std::int64_t> values;
        for (const ParameterValue& value : read)
            values.push_back(value.value);
        return values;
    }

    bool contains(const std::string& text, const char* part)
    {
        return text.find(part) != std::string::npos;
    }

    /// The commands the sampler accepted for an item, in order.
    std::vector<akm::harness::AcceptedCommand> acceptedFor(const Rig& rig, akm::ItemId item)
    {
        std::vector<akm::harness::AcceptedCommand> found;
        const akm::ItemDescriptor& wanted = akm::descriptor(item);
        for (const auto& command : rig.sampler->acceptedCommands())
        {
            if (command.section == wanted.section && command.item == wanted.item)
                found.push_back(command);
        }
        return found;
    }
}

TEST_CASE("Given a simulated sampler holding PAD, BASS and LEAD with BASS current, When the status is asked, Then the DeviceID, three programs, the current one and its three keygroups are answered [RQ-MCP-007]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    mcp::SamplerGateway gateway(rig.backend, configFor(rig));

    const auto status = gateway.status();

    REQUIRE(status.ok());
    CHECK(status.value->programCount == 3);
    CHECK(status.value->currentProgram == "BASS");
    CHECK(status.value->keygroupCount == 3);
    CHECK(status.value->deviceId == 0);
}

TEST_CASE("Given a sampler with no program, When the status is asked, Then the count is zero and there is no current program, and it is not a failure [RQ-MCP-007]",
          "[mcp][gateway]")
{
    Rig rig;
    mcp::SamplerGateway gateway(rig.backend, configFor(rig));

    const auto status = gateway.status();

    REQUIRE(status.ok());
    CHECK(status.value->programCount == 0);
    CHECK_FALSE(status.value->currentProgram.has_value());
    CHECK_FALSE(status.value->keygroupCount.has_value());

    const auto programs = gateway.listPrograms();
    REQUIRE(programs.ok());
    CHECK(programs.value->empty());
}

TEST_CASE("Given three programs, When they are listed, Then the names come in the sampler's alphabetical order with their positions [RQ-MCP-007]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    mcp::SamplerGateway gateway(rig.backend, configFor(rig));

    const auto programs = gateway.listPrograms();

    REQUIRE(programs.ok());
    REQUIRE(programs.value->size() == 3);
    CHECK(programs.value->at(0).index == 0);
    CHECK(programs.value->at(0).name == "BASS");
    CHECK(programs.value->at(1).name == "LEAD");
    CHECK(programs.value->at(2).index == 2);
    CHECK(programs.value->at(2).name == "PAD");
}

TEST_CASE("Given three programs, When LEAD is selected by name and PAD by index, Then each answers its name and keygroup count; a name or an index that no program has is a problem that says so [RQ-MCP-007]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    mcp::SamplerGateway gateway(rig.backend, configFor(rig));

    const auto lead = gateway.selectProgramByName("LEAD");
    REQUIRE(lead.ok());
    CHECK(lead.value->name == "LEAD");
    CHECK(lead.value->keygroupCount == 2);
    CHECK(gateway.status().value->currentProgram == "LEAD");

    const auto pad = gateway.selectProgramByIndex(2);
    REQUIRE(pad.ok());
    CHECK(pad.value->name == "PAD");
    CHECK(pad.value->keygroupCount == 1);

    const auto missing = gateway.selectProgramByName("NOPE");
    CHECK_FALSE(missing.ok());
    CHECK(contains(missing.problem, "No program is named \"NOPE\""));
    CHECK(contains(missing.problem, "list_programs"));
    const auto farIndex = gateway.selectProgramByIndex(9);
    CHECK_FALSE(farIndex.ok());
    CHECK(contains(farIndex.problem, "No program is at index 9"));
    CHECK(gateway.status().value->currentProgram == "PAD");
}

TEST_CASE("Given BASS with filter cutoffs 30, 60 and 90, When the cutoff is read for all keygroups and for keygroup 2, Then the values are 30, 60, 90 numbered from 1, and 60 [RQ-MCP-005]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    mcp::SamplerGateway gateway(rig.backend, configFor(rig));

    const auto all = gateway.readParameter(named("filter cutoff"), KeygroupSelection::all());
    REQUIRE(all.ok());
    CHECK(valuesOf(*all.value) == std::vector<std::int64_t>{30, 60, 90});
    CHECK(all.value->at(0).keygroup == 1);
    CHECK(all.value->at(2).keygroup == 3);

    const auto second = gateway.readParameter(named("filter cutoff"), KeygroupSelection::of(2));
    REQUIRE(second.ok());
    REQUIRE(second.value->size() == 1);
    CHECK(second.value->front().value == 60);
    CHECK(second.value->front().keygroup == 2);
}

TEST_CASE("Given BASS, When the cutoff is set to 80 for all keygroups, Then the read-back is 80, 80, 80 and a later reading agrees [RQ-MCP-006]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    mcp::SamplerGateway gateway(rig.backend, configFor(rig));

    const auto written = gateway.writeParameter(named("filter cutoff"), 80, KeygroupSelection::all());

    REQUIRE(written.ok());
    CHECK(valuesOf(*written.value) == std::vector<std::int64_t>{80, 80, 80});
    CHECK(valuesOf(*gateway.readParameter(named("filter cutoff"), KeygroupSelection::all()).value) ==
          std::vector<std::int64_t>{80, 80, 80});
}

TEST_CASE("Given BASS, When the cutoff is set to 5 for keygroup 3 only, Then only keygroup 3 changes [RQ-MCP-006]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    mcp::SamplerGateway gateway(rig.backend, configFor(rig));

    const auto written = gateway.writeParameter(named("filter cutoff"), 5, KeygroupSelection::of(3));

    REQUIRE(written.ok());
    CHECK(valuesOf(*written.value) == std::vector<std::int64_t>{5});
    CHECK(valuesOf(*gateway.readParameter(named("filter cutoff"), KeygroupSelection::all()).value) ==
          std::vector<std::int64_t>{30, 60, 5});
}

TEST_CASE("Given a program of 2 keygroups, When keygroup 3 is asked for, Then nothing is selected or set and the problem says the program has 2 [RQ-MCP-007]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    mcp::SamplerGateway gateway(rig.backend, configFor(rig));
    REQUIRE(gateway.selectProgramByName("LEAD").ok());
    const auto selectsBefore = acceptedFor(rig, akm::ItemId::KeygroupSelect).size();
    const auto setsBefore = acceptedFor(rig, akm::ItemId::KeygroupSetFilterCutoff).size();

    const auto read = gateway.readParameter(named("filter cutoff"), KeygroupSelection::of(3));
    const auto written = gateway.writeParameter(named("filter cutoff"), 80, KeygroupSelection::of(3));
    const auto zero = gateway.writeParameter(named("filter cutoff"), 80, KeygroupSelection::of(0));

    for (const auto& outcome : {read, written, zero})
    {
        CHECK_FALSE(outcome.ok());
        CHECK(contains(outcome.problem, "has 2 keygroups"));
    }
    CHECK(acceptedFor(rig, akm::ItemId::KeygroupSelect).size() == selectsBefore);
    CHECK(acceptedFor(rig, akm::ItemId::KeygroupSetFilterCutoff).size() == setsBefore);
}

TEST_CASE("Given a signed parameter, When filter envelope depth is set to -40 for keygroup 2, Then the sampler is sent sign 1 and magnitude 40, and the read-back is -40 [RQ-MCP-006]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    mcp::SamplerGateway gateway(rig.backend, configFor(rig));

    const auto written = gateway.writeParameter(named("filter envelope depth"), -40, KeygroupSelection::of(2));

    REQUIRE(written.ok());
    CHECK(valuesOf(*written.value) == std::vector<std::int64_t>{-40});
    const auto sent = acceptedFor(rig, akm::ItemId::KeygroupSetFilterEnvDepth);
    REQUIRE(sent.size() == 1);
    CHECK(sent.front().data == akm::test::bytes({1, 40}));
}

TEST_CASE("Given the program's LFOs, When LFO 1 rate is set to 55 and LFO 2 waveform to 4, Then the commands carry the LFO number first, no keygroup is selected, and each reads back [RQ-MCP-006]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    mcp::SamplerGateway gateway(rig.backend, configFor(rig));
    const auto selectsBefore = acceptedFor(rig, akm::ItemId::KeygroupSelect).size();

    const auto rate = gateway.writeParameter(named("lfo 1 rate"), 55, KeygroupSelection::all());
    const auto waveform = gateway.writeParameter(named("lfo 2 waveform"), 4, KeygroupSelection::all());

    REQUIRE(rate.ok());
    REQUIRE(rate.value->size() == 1);
    CHECK(rate.value->front().value == 55);
    CHECK_FALSE(rate.value->front().keygroup.has_value());
    REQUIRE(waveform.ok());
    CHECK(waveform.value->front().value == 4);
    CHECK(acceptedFor(rig, akm::ItemId::ProgramSetLfoRate).front().data == akm::test::bytes({1, 55}));
    CHECK(acceptedFor(rig, akm::ItemId::ProgramSetLfoWaveform).front().data == akm::test::bytes({2, 4}));
    CHECK(acceptedFor(rig, akm::ItemId::KeygroupSelect).size() == selectsBefore);
    CHECK(gateway.readParameter(named("lfo 1 rate"), KeygroupSelection::all()).value->front().value == 55);
}

TEST_CASE("Given a filter type, a stepped attenuation and a switch, When each is set, Then the sampler is sent the codes and each reads back in the catalogue's units [RQ-MCP-006]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    mcp::SamplerGateway gateway(rig.backend, configFor(rig));

    CHECK(gateway.writeParameter(named("filter type"), 2, KeygroupSelection::all()).value->front().value == 2);
    CHECK(acceptedFor(rig, akm::ItemId::KeygroupSetFilterMode).front().data == akm::test::bytes({2}));
    CHECK(gateway.writeParameter(named("filter attenuation"), 12, KeygroupSelection::of(1)).value->front().value == 12);
    CHECK(acceptedFor(rig, akm::ItemId::KeygroupSetFilterAttenuation).front().data == akm::test::bytes({2}));
    CHECK(gateway.writeParameter(named("lfo 1 sync"), 1, KeygroupSelection::all()).value->front().value == 1);
    CHECK(acceptedFor(rig, akm::ItemId::ProgramSetLfoSync).front().data == akm::test::bytes({1, 1}));
}

TEST_CASE("Given a simulated sampler that answers nothing, When the status is asked, Then the problem says nothing answered, and the next call opens the connection again once the sampler answers [RQ-MCP-003, RQ-MCP-009]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    akm::harness::SamplerBehaviour silent;
    silent.silent = true;
    rig.sampler->setBehaviour(silent);
    mcp::SamplerGateway gateway(rig.backend, configFor(rig));

    const auto first = gateway.status();
    CHECK_FALSE(first.ok());
    CHECK(contains(first.problem, "answer"));
    CHECK(rig.sampler->settings().stillAlive == false);

    rig.sampler->setBehaviour({});
    const auto second = gateway.status();
    REQUIRE(second.ok());
    CHECK(second.value->programCount == 3);
}

TEST_CASE("Given an open connection, When the sampler stops answering and a parameter is set, Then the problem says so within the command timeout, and the next call works once it answers again [RQ-MCP-009]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    mcp::SamplerGateway gateway(rig.backend, configFor(rig));
    REQUIRE(gateway.status().ok());
    akm::harness::SamplerBehaviour silent;
    silent.silent = true;
    rig.sampler->setBehaviour(silent);

    const auto started = std::chrono::steady_clock::now();
    const auto written = gateway.writeParameter(named("filter cutoff"), 80, KeygroupSelection::all());
    const auto elapsed = std::chrono::steady_clock::now() - started;

    CHECK_FALSE(written.ok());
    CHECK(contains(written.problem, "did not answer"));
    CHECK(elapsed < std::chrono::seconds(3));

    rig.sampler->setBehaviour({});
    CHECK(gateway.writeParameter(named("filter cutoff"), 80, KeygroupSelection::all()).ok());
}

TEST_CASE("Given an ERROR 04 for a keygroup of a sampler with no current program, When a parameter is read, Then the problem says to select a program first [RQ-MCP-009]",
          "[mcp][gateway]")
{
    Rig rig;
    mcp::SamplerGateway gateway(rig.backend, configFor(rig));

    const auto read = gateway.readParameter(named("filter cutoff"), KeygroupSelection::all());

    CHECK_FALSE(read.ok());
    CHECK(contains(read.problem, "select_program"));
}

TEST_CASE("Given an open session, When the gateway is closed, Then the section 00 settings are back to their defaults, and a later call opens a new session [RQ-MCP-003]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    mcp::SamplerGateway gateway(rig.backend, configFor(rig));
    REQUIRE(gateway.status().ok());
    CHECK(rig.sampler->settings().stillAlive);
    CHECK_FALSE(rig.sampler->settings().syncLcd);
    CHECK(rig.sampler->settings().autoScreenUpdate);

    gateway.close();
    gateway.close();  // twice is harmless

    CHECK(rig.sampler->settings() == akm::harness::SamplerSettings{});
    CHECK(gateway.status().ok());
}

namespace
{
    constexpr std::uint8_t SECTION_SETUP = 0x00;
    constexpr std::uint8_t ITEM_SYNC_LCD = 0x03;
    constexpr std::uint8_t ITEM_AUTO_SCREEN_UPDATE = 0x05;

    /// The data of every command of the section 00 item `item` the simulated sampler accepted, in order.
    std::vector<std::vector<std::uint8_t>> sentSetting(const akm::harness::SimulatedSampler& sampler, std::uint8_t item)
    {
        std::vector<std::vector<std::uint8_t>> sent;
        for (const auto& command : sampler.acceptedCommands())
        {
            if (command.section == SECTION_SETUP && command.item == item)
                sent.push_back(command.data);
        }
        return sent;
    }
}

TEST_CASE("Given the screen mode independent, When a session is opened and closed, Then Sync LCD is sent off and Auto screen update on, and both are put back to their defaults [RQ-MCP-045]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    mcp::GatewayConfig config = configFor(rig);
    config.screen = mcp::ScreenMode::Independent;
    mcp::SamplerGateway gateway(rig.backend, config);

    REQUIRE(gateway.status().ok());
    CHECK_FALSE(rig.sampler->settings().syncLcd);
    CHECK(rig.sampler->settings().autoScreenUpdate);
    REQUIRE(sentSetting(*rig.sampler, ITEM_SYNC_LCD).size() == 1);
    CHECK(sentSetting(*rig.sampler, ITEM_SYNC_LCD).front() == std::vector<std::uint8_t>{0});
    REQUIRE(sentSetting(*rig.sampler, ITEM_AUTO_SCREEN_UPDATE).size() == 1);
    CHECK(sentSetting(*rig.sampler, ITEM_AUTO_SCREEN_UPDATE).front() == std::vector<std::uint8_t>{1});

    gateway.close();
    CHECK(rig.sampler->settings().syncLcd);
    CHECK_FALSE(rig.sampler->settings().autoScreenUpdate);
}

TEST_CASE("Given the screen mode follow, When a session is opened and closed, Then Sync LCD and Auto screen update are both sent on, and put back to their defaults [RQ-MCP-045]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    mcp::GatewayConfig config = configFor(rig);
    config.screen = mcp::ScreenMode::Follow;
    mcp::SamplerGateway gateway(rig.backend, config);

    REQUIRE(gateway.status().ok());
    REQUIRE(sentSetting(*rig.sampler, ITEM_SYNC_LCD).size() == 1);
    CHECK(sentSetting(*rig.sampler, ITEM_SYNC_LCD).front() == std::vector<std::uint8_t>{1});
    REQUIRE(sentSetting(*rig.sampler, ITEM_AUTO_SCREEN_UPDATE).size() == 1);
    CHECK(sentSetting(*rig.sampler, ITEM_AUTO_SCREEN_UPDATE).front() == std::vector<std::uint8_t>{1});
    CHECK(rig.sampler->settings().syncLcd);
    CHECK(rig.sampler->settings().autoScreenUpdate);

    gateway.close();
    CHECK(rig.sampler->settings().syncLcd);
    CHECK_FALSE(rig.sampler->settings().autoScreenUpdate);
}

TEST_CASE("Given the screen mode as-is, When a session is opened and closed, Then nothing is sent about Sync LCD and Auto screen update, before or after [RQ-MCP-045, RQ-MCP-003]",
          "[mcp][gateway]")
{
    Rig rig;
    seedThreePrograms(rig);
    mcp::GatewayConfig config = configFor(rig);
    config.screen = mcp::ScreenMode::AsIs;
    mcp::SamplerGateway gateway(rig.backend, config);

    REQUIRE(gateway.status().ok());
    CHECK(rig.sampler->settings().syncLcd);
    CHECK_FALSE(rig.sampler->settings().autoScreenUpdate);
    gateway.close();

    CHECK(sentSetting(*rig.sampler, ITEM_SYNC_LCD).empty());
    CHECK(sentSetting(*rig.sampler, ITEM_AUTO_SCREEN_UPDATE).empty());
}

TEST_CASE("Given a MIDI port that does not exist, When the status is asked, Then the problem names the port and the ports there are [RQ-MCP-003, RQ-MCP-009]",
          "[mcp][gateway]")
{
    Rig rig;
    mcp::GatewayConfig config = configFor(rig);
    config.inputPort = "No Such Port";
    mcp::SamplerGateway gateway(rig.backend, config);

    const auto status = gateway.status();

    CHECK_FALSE(status.ok());
    CHECK(contains(status.problem, "\"No Such Port\""));
    CHECK(contains(status.problem, rig.backend.inputName().c_str()));
}

TEST_CASE("Given a DeviceID no sampler has, When the status is asked, Then the problem names the DeviceID and the ones that answered [RQ-MCP-003, RQ-MCP-009]",
          "[mcp][gateway]")
{
    Rig rig;
    mcp::GatewayConfig config = configFor(rig);
    config.deviceId = 5;
    mcp::SamplerGateway gateway(rig.backend, config);

    const auto status = gateway.status();

    CHECK_FALSE(status.ok());
    CHECK(contains(status.problem, "DeviceID 5"));
    CHECK(contains(status.problem, "answered at DeviceID 0"));
}
