/*
 * XS56K - Editor for AKAI S5000/S6000 samplers
 * Copyright (C) 2026 xplorer2716
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// The effects board: get_fx_board, set_fx_channel_mute, set_fx_module, get_fx_parameter and set_fx_parameter, over a real session and the
// simulated sampler with an EB20 layout (and with no board). These tools have been run on the simulated sampler only: the owner has no
// effects board. [TASK-MCP-052, RQ-MCP-053, ADR-MCP-005 (DEC-MCP-033)]
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "ToolRig.hpp"

using json = nlohmann::json;
using mcp::test::hasText;
using mcp::test::ToolRig;
using mcp::test::toolFailed;
using mcp::test::toolText;

namespace
{
    constexpr const char* GET_BOARD = "get_fx_board";
    constexpr const char* SET_MUTE = "set_fx_channel_mute";
    constexpr const char* SET_MODULE = "set_fx_module";
    constexpr const char* GET_PARAMETER = "get_fx_parameter";
    constexpr const char* SET_PARAMETER = "set_fx_parameter";
    constexpr int MODULATION = 2;  // the module of a channel 0 or 1 whose type may change, and the one that holds the chorus
    constexpr int DELAY = 3;       // the other one
    constexpr int NONE_MODULE = 4;  // a module of the fixture layout that holds no effect
    constexpr int CHORUS_RATE = 0;
    constexpr int CHORUS_DEPTH = 1;
    constexpr int CHORUS_FEEDBACK = 2;
    constexpr std::uint8_t CODE_CHORUS = 0x02;
    constexpr std::uint8_t CODE_FLANGE = 0x03;
    constexpr std::uint8_t CODE_STEREO_DELAY = 0x0D;

    /// The tool rig with an EB20 laid out as the fixture does (channels 0 and 1: ring modulator, EQ, chorus, mono delay, none, output mix;
    /// channels 2 and 3: reverb input, reverb) and the multi LIVE selected, the effects being the current multi's.
    struct FxRig : ToolRig
    {
        FxRig()
        {
            sampler->setFxBoard(akm::harness::eb20Layout());
            REQUIRE_FALSE(toolFailed(call("select_multi", {{"name", "LIVE"}})));
        }
    };

    json module(FxRig& rig, int channel, int index, const json& extra)
    {
        json arguments = {{"channel", channel}, {"module", index}};
        arguments.update(extra);
        return rig.call(SET_MODULE, arguments);
    }

    json parameter(FxRig& rig, int channel, int index, const json& which, const json& value)
    {
        return rig.call(SET_PARAMETER, {{"channel", channel}, {"module", index}, {"parameter", which}, {"value", value}});
    }

    std::size_t fxCommandsSentExceptCard(const ToolRig& rig)
    {
        std::size_t sent = 0;
        for (const akm::ItemId item : {akm::ItemId::FxGetChannelCount, akm::ItemId::FxGetModuleCount, akm::ItemId::FxSetChannelMute,
                                       akm::ItemId::FxGetChannelMute, akm::ItemId::FxSetModuleType, akm::ItemId::FxGetModuleType,
                                       akm::ItemId::FxSetModuleEnabled, akm::ItemId::FxGetModuleEnabled, akm::ItemId::FxSetParameter,
                                       akm::ItemId::FxGetParameter})
            sent += rig.accepted(item);
        return sent;
    }
}

TEST_CASE("Given the server, When the tools are listed, Then the two effects reads read and the three sets change the sampler without deleting anything [RQ-MCP-053, RQ-MCP-013]",
          "[mcp][fx]")
{
    ToolRig rig;
    for (const char* name : {GET_BOARD, GET_PARAMETER})
    {
        const json tool = rig.tool(name);
        INFO(name);
        REQUIRE_FALSE(tool.is_null());
        CHECK(tool["annotations"]["readOnlyHint"].get<bool>());
    }
    for (const char* name : {SET_MUTE, SET_MODULE, SET_PARAMETER})
    {
        const json tool = rig.tool(name);
        INFO(name);
        REQUIRE_FALSE(tool.is_null());
        CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
        CHECK_FALSE(tool["annotations"]["destructiveHint"].get<bool>());
        CHECK(tool["annotations"]["idempotentHint"].get<bool>());
    }
}

TEST_CASE("Given a sampler that reports no effects board, When each effects tool is called, Then the answer says there is none and nothing but the card query is sent [RQ-MCP-053]",
          "[mcp][fx]")
{
    ToolRig rig;
    REQUIRE_FALSE(toolFailed(rig.call("select_multi", {{"name", "LIVE"}})));
    const std::vector<json> answers = {
        rig.call(GET_BOARD),
        rig.call(SET_MUTE, {{"channel", 0}, {"muted", true}}),
        rig.call(SET_MODULE, {{"channel", 0}, {"module", 2}, {"type", "flange"}}),
        rig.call(GET_PARAMETER, {{"channel", 0}, {"module", 2}}),
        rig.call(SET_PARAMETER, {{"channel", 0}, {"module", 2}, {"parameter", "rate"}, {"value", 10}}),
    };
    for (const json& answer : answers)
    {
        CHECK(toolFailed(answer));
        CHECK(hasText(toolText(answer), "no effects board"));
    }
    CHECK(rig.accepted(akm::ItemId::FxGetCard) == answers.size());
    CHECK(fxCommandsSentExceptCard(rig) == 0);
}

TEST_CASE("Given an EB20 and a current multi, When the board is read, Then the card, the four channels and their modules with type and state are listed [RQ-MCP-053]",
          "[mcp][fx]")
{
    FxRig rig;
    const std::string text = toolText(rig.call(GET_BOARD));
    CHECK(hasText(text, "Effects board: EB20, 4 channels."));
    CHECK(hasText(text, "Channel 0 (on):"));
    CHECK(hasText(text, "module 0: ringmod_distortion (enabled)"));
    CHECK(hasText(text, "module 2: chorus (enabled)"));
    CHECK(hasText(text, "module 3: mono_delay (enabled)"));
    CHECK(hasText(text, "module 4: none (enabled)"));
    CHECK(hasText(text, "Channel 2 (on):"));
    CHECK(hasText(text, "module 0: reverb_input (enabled)"));
    CHECK(hasText(text, "module 1: reverb (enabled)"));
}

TEST_CASE("Given no current multi, When the board is read or changed, Then the answer says to select a multi first [RQ-MCP-053]",
          "[mcp][fx]")
{
    ToolRig rig;
    rig.sampler->setFxBoard(akm::harness::eb20Layout());
    const json read = rig.call(GET_BOARD);
    CHECK(toolFailed(read));
    CHECK(hasText(toolText(read), "select_multi"));
    const json set = rig.call(SET_MUTE, {{"channel", 0}, {"muted", true}});
    CHECK(toolFailed(set));
    CHECK(hasText(toolText(set), "select_multi"));
}

TEST_CASE("Given an EB20, When a channel is muted and unmuted, Then the simulated sampler holds it and the board reads it back; a channel the board does not have sends nothing [RQ-MCP-053]",
          "[mcp][fx]")
{
    FxRig rig;
    const json muted = rig.call(SET_MUTE, {{"channel", 1}, {"muted", true}});
    CHECK_FALSE(toolFailed(muted));
    CHECK(hasText(toolText(muted), "effects channel 1 is muted"));
    CHECK(rig.sampler->fxState().channels[1].muted);
    CHECK(hasText(toolText(rig.call(GET_BOARD)), "Channel 1 (muted):"));
    const json on = rig.call(SET_MUTE, {{"channel", 1}, {"muted", false}});
    CHECK(hasText(toolText(on), "effects channel 1 is on"));
    CHECK_FALSE(rig.sampler->fxState().channels[1].muted);

    const std::size_t before = rig.accepted(akm::ItemId::FxSetChannelMute);
    const json beyond = rig.call(SET_MUTE, {{"channel", 4}, {"muted", true}});
    CHECK(toolFailed(beyond));
    CHECK(hasText(toolText(beyond), "channels 0 to 3"));
    CHECK(rig.accepted(akm::ItemId::FxSetChannelMute) == before);
}

TEST_CASE("Given an EB20, When the type of the modulation module is set to flange and the delay module to stereo_delay, Then the simulated sampler holds the codes and the answers read them back [RQ-MCP-053]",
          "[mcp][fx]")
{
    FxRig rig;
    const json flange = module(rig, 0, MODULATION, {{"type", "flange"}});
    CHECK_FALSE(toolFailed(flange));
    CHECK(hasText(toolText(flange), "type flange"));
    CHECK(rig.sampler->fxState().channels[0].modules[MODULATION].type == CODE_FLANGE);
    CHECK_FALSE(toolFailed(module(rig, 1, DELAY, {{"type", "Stereo Delay"}})));
    CHECK(rig.sampler->fxState().channels[1].modules[DELAY].type == CODE_STEREO_DELAY);
    CHECK(rig.accepted(akm::ItemId::FxSetModuleType) == 2);
}

TEST_CASE("Given an EB20, When a module is bypassed and enabled, Then the simulated sampler holds the state and the answer reads it back; a type and a state may be set in one call [RQ-MCP-053]",
          "[mcp][fx]")
{
    FxRig rig;
    const json bypassed = module(rig, 0, 1, {{"enabled", false}});
    CHECK_FALSE(toolFailed(bypassed));
    CHECK(hasText(toolText(bypassed), "bypassed"));
    CHECK_FALSE(rig.sampler->fxState().channels[0].modules[1].enabled);
    CHECK(hasText(toolText(rig.call(GET_BOARD)), "module 1: eq (bypassed)"));
    CHECK_FALSE(toolFailed(module(rig, 0, 1, {{"enabled", true}})));
    CHECK(rig.sampler->fxState().channels[0].modules[1].enabled);

    const json both = module(rig, 1, MODULATION, {{"type", "phase"}, {"enabled", false}});
    CHECK_FALSE(toolFailed(both));
    CHECK(rig.accepted(akm::ItemId::FxSetModuleType) == 1);
    CHECK(rig.accepted(akm::ItemId::FxSetModuleEnabled) == 3);
}

TEST_CASE("Given the EB20 rule that only modules 2 and 3 of channels 0 and 1 may change type, When another module or a type that module cannot take is asked for, Then nothing is sent and the answer says which are allowed [RQ-MCP-053]",
          "[mcp][fx]")
{
    FxRig rig;
    const json fixed = module(rig, 0, 1, {{"type", "chorus"}});
    CHECK(toolFailed(fixed));
    CHECK(hasText(toolText(fixed), "modules 2 and 3 of channels 0 and 1"));
    CHECK(toolFailed(module(rig, 2, 1, {{"type", "reverb"}})));
    const json wrongKind = module(rig, 0, MODULATION, {{"type", "reverb"}});
    CHECK(toolFailed(wrongKind));
    CHECK(hasText(toolText(wrongKind), "chorus, flange, phase, rotary_speaker, fmod_autopan, pitch_shift, pitch_shift_feedback"));
    const json wrongDelay = module(rig, 0, DELAY, {{"type", "chorus"}});
    CHECK(toolFailed(wrongDelay));
    CHECK(hasText(toolText(wrongDelay), "mono_delay, mono_left_right, mono_crossover, stereo_delay"));
    const json unknown = module(rig, 0, MODULATION, {{"type", "wobble"}});
    CHECK(toolFailed(unknown));
    CHECK(hasText(toolText(unknown), "ringmod_distortion"));
    CHECK(rig.accepted(akm::ItemId::FxSetModuleType) == 0);
}

TEST_CASE("Given a channel or a module the board does not have, or nothing to set, When set_fx_module is called, Then nothing is sent and the answer gives the layout [RQ-MCP-053]",
          "[mcp][fx]")
{
    FxRig rig;
    const json channel = module(rig, 4, 0, {{"enabled", true}});
    CHECK(toolFailed(channel));
    CHECK(hasText(toolText(channel), "channels 0 to 3"));
    const json beyond = module(rig, 2, MODULATION, {{"enabled", true}});
    CHECK(toolFailed(beyond));
    CHECK(hasText(toolText(beyond), "modules 0 to 1"));
    const json nothing = module(rig, 0, MODULATION, json::object());
    CHECK(toolFailed(nothing));
    CHECK(hasText(toolText(nothing), "type"));
    CHECK(rig.accepted(akm::ItemId::FxSetModuleType) == 0);
    CHECK(rig.accepted(akm::ItemId::FxSetModuleEnabled) == 0);
}

TEST_CASE("Given the chorus on channel 0, When its parameters are set by name and by index, Then the simulated sampler holds them and the answers read them back, negative values included [RQ-MCP-053]",
          "[mcp][fx]")
{
    FxRig rig;
    const json rate = parameter(rig, 0, MODULATION, "rate", 15);
    CHECK_FALSE(toolFailed(rate));
    CHECK(hasText(toolText(rate), "rate set to 15"));
    CHECK_FALSE(toolFailed(parameter(rig, 0, MODULATION, CHORUS_DEPTH, 60)));
    CHECK_FALSE(toolFailed(parameter(rig, 0, MODULATION, "Feedback", -20)));
    const auto fx = rig.sampler->fxState();
    const auto& values = fx.channels[0].modules[MODULATION].parameters;
    CHECK(values.at(CHORUS_RATE) == 15);
    CHECK(values.at(CHORUS_DEPTH) == 60);
    CHECK(values.at(CHORUS_FEEDBACK) == -20);
    CHECK(rig.accepted(akm::ItemId::FxSetParameter) == 3);
}

TEST_CASE("Given a value outside the range of a parameter, an unknown parameter or a module with no effect, When set_fx_parameter is called, Then nothing is sent and the answer gives what is accepted [RQ-MCP-053]",
          "[mcp][fx]")
{
    FxRig rig;
    const json high = parameter(rig, 0, MODULATION, "rate", 100);
    CHECK(toolFailed(high));
    CHECK(hasText(toolText(high), "0 to 99"));
    CHECK(toolFailed(parameter(rig, 0, MODULATION, "depth", 101)));
    CHECK(toolFailed(parameter(rig, 0, MODULATION, "depth", -1)));
    CHECK(toolFailed(parameter(rig, 0, MODULATION, "feedback", -51)));
    CHECK(toolFailed(parameter(rig, 0, MODULATION, "feedback", 51)));
    const json unknown = parameter(rig, 0, MODULATION, "wobble", 1);
    CHECK(toolFailed(unknown));
    CHECK(hasText(toolText(unknown), "rate, depth, feedback"));
    CHECK(toolFailed(parameter(rig, 0, MODULATION, 3, 1)));
    const json none = parameter(rig, 0, NONE_MODULE, 0, 1);
    CHECK(toolFailed(none));
    CHECK(hasText(toolText(none), "no parameter"));
    CHECK(toolFailed(rig.call(SET_PARAMETER, {{"channel", 0}, {"module", MODULATION}, {"parameter", "rate"}, {"value", "15"}})));
    CHECK(toolFailed(rig.call(SET_PARAMETER, {{"channel", 0}, {"module", MODULATION}, {"parameter", "rate"}, {"value", 1.5}})));
    CHECK(rig.accepted(akm::ItemId::FxSetParameter) == 0);
}

TEST_CASE("Given the ranges of Table 25, When an extreme value of a few modules is set, Then it is accepted at the bounds and refused one beyond [RQ-MCP-053]",
          "[mcp][fx]")
{
    FxRig rig;
    CHECK_FALSE(toolFailed(parameter(rig, 0, 0, "rmod_frequency", 5000)));
    CHECK(toolFailed(parameter(rig, 0, 0, "rmod_frequency", 5001)));
    CHECK(toolFailed(parameter(rig, 0, 0, "rmod_frequency", 0)));
    CHECK_FALSE(toolFailed(parameter(rig, 0, 1, "low_gain", -37)));
    CHECK(toolFailed(parameter(rig, 0, 1, "low_gain", -38)));
    CHECK_FALSE(toolFailed(parameter(rig, 0, 1, "high_gain", 12)));
    CHECK(toolFailed(parameter(rig, 0, 1, "high_gain", 13)));
    CHECK_FALSE(toolFailed(parameter(rig, 2, 1, "reverb_type", 6)));
    CHECK(toolFailed(parameter(rig, 2, 1, "reverb_type", 7)));
    CHECK_FALSE(toolFailed(parameter(rig, 2, 0, "rv_input", 4)));
    CHECK(toolFailed(parameter(rig, 2, 0, "rv_input", 5)));
}

TEST_CASE("Given parameters that were set, When get_fx_parameter is called for one or for the whole module, Then the values are read back from the sampler with their ranges [RQ-MCP-053]",
          "[mcp][fx]")
{
    FxRig rig;
    REQUIRE_FALSE(toolFailed(parameter(rig, 0, MODULATION, "rate", 15)));
    REQUIRE_FALSE(toolFailed(parameter(rig, 0, MODULATION, "feedback", -20)));
    const json one = rig.call(GET_PARAMETER, {{"channel", 0}, {"module", MODULATION}, {"parameter", "rate"}});
    CHECK_FALSE(toolFailed(one));
    CHECK(hasText(toolText(one), "rate = 15"));
    CHECK(hasText(toolText(one), "0 to 99"));
    const json all = rig.call(GET_PARAMETER, {{"channel", 0}, {"module", MODULATION}});
    CHECK_FALSE(toolFailed(all));
    CHECK(hasText(toolText(all), "chorus"));
    CHECK(hasText(toolText(all), "rate = 15"));
    CHECK(hasText(toolText(all), "depth = 0"));
    CHECK(hasText(toolText(all), "feedback = -20"));
    // Two reads back from the two sets above, one for the single parameter and three for the chorus's whole module.
    CHECK(rig.accepted(akm::ItemId::FxGetParameter) == 6);
    const json none = rig.call(GET_PARAMETER, {{"channel", 0}, {"module", NONE_MODULE}});
    CHECK(toolFailed(none));
    CHECK(hasText(toolText(none), "no parameter"));
}

TEST_CASE("Given the type of a module changed, When its parameters are asked for, Then they are those of the new type [RQ-MCP-053]",
          "[mcp][fx]")
{
    FxRig rig;
    REQUIRE_FALSE(toolFailed(module(rig, 0, MODULATION, {{"type", "flange"}})));
    CHECK(rig.sampler->fxState().channels[0].modules[MODULATION].type != CODE_CHORUS);
    const json all = rig.call(GET_PARAMETER, {{"channel", 0}, {"module", MODULATION}});
    CHECK(hasText(toolText(all), "flange"));
    REQUIRE_FALSE(toolFailed(module(rig, 0, DELAY, {{"type", "stereo_delay"}})));
    const json delay = rig.call(GET_PARAMETER, {{"channel", 0}, {"module", DELAY}});
    CHECK(hasText(toolText(delay), "left_delay_time"));
    CHECK_FALSE(toolFailed(parameter(rig, 0, DELAY, "left_delay_time", 335)));
    CHECK(toolFailed(parameter(rig, 0, DELAY, "left_delay_time", 336)));
}

TEST_CASE("Given a missing, an unknown or a badly typed argument, When an effects tool is called, Then it is refused and nothing is sent [RQ-MCP-053]",
          "[mcp][fx]")
{
    FxRig rig;
    CHECK(toolFailed(rig.call(SET_MUTE)));
    CHECK(toolFailed(rig.call(SET_MUTE, {{"channel", 0}})));
    CHECK(toolFailed(rig.call(SET_MUTE, {{"channel", "0"}, {"muted", true}})));
    CHECK(toolFailed(rig.call(SET_MUTE, {{"channel", 0}, {"muted", "yes"}})));
    CHECK(toolFailed(rig.call(SET_MUTE, {{"channel", -1}, {"muted", true}})));
    CHECK(toolFailed(rig.call(SET_MUTE, {{"channel", 0}, {"muted", true}, {"extra", 1}})));
    CHECK(toolFailed(rig.call(SET_MODULE)));
    CHECK(toolFailed(rig.call(SET_MODULE, {{"channel", 0}})));
    CHECK(toolFailed(rig.call(SET_MODULE, {{"channel", 0}, {"module", 2}, {"type", 5}})));
    CHECK(toolFailed(rig.call(SET_MODULE, {{"channel", 0}, {"module", 2}, {"enabled", "no"}})));
    CHECK(toolFailed(rig.call(GET_PARAMETER)));
    CHECK(toolFailed(rig.call(GET_PARAMETER, {{"channel", 0}, {"module", 2}, {"parameter", true}})));
    CHECK(toolFailed(rig.call(SET_PARAMETER, {{"channel", 0}, {"module", 2}, {"value", 1}})));
    CHECK(toolFailed(rig.call(SET_PARAMETER, {{"channel", 0}, {"module", 2}, {"parameter", "rate"}})));
    CHECK(toolFailed(rig.call(GET_BOARD, {{"extra", 1}})));
    CHECK(rig.accepted(akm::ItemId::FxSetChannelMute) == 0);
    CHECK(rig.accepted(akm::ItemId::FxSetModuleType) == 0);
    CHECK(rig.accepted(akm::ItemId::FxSetModuleEnabled) == 0);
    CHECK(rig.accepted(akm::ItemId::FxSetParameter) == 0);
}
