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

// The sampler's MIDI setup: set_midi_setting and set_midi_filter (section 04, which has no Get), over a real session and the
// simulated sampler. [TASK-MCP-046, RQ-MCP-047, ADR-MCP-005 (DEC-MCP-030)]
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
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
    constexpr std::size_t FILTER_EVENT_TYPES = 4;
    constexpr const char* TOOL_SET_SETTING = "set_midi_setting";
    constexpr const char* TOOL_SET_FILTER = "set_midi_filter";
    constexpr const char* SWITCH_PROGRAM_CHANGE = "program_change";
    constexpr const char* SWITCH_MULTI_SELECT = "multi_select";
    constexpr const char* SWITCH_MULTI_SELECT_CHANNEL = "multi_select_channel";
    constexpr const char* SWITCH_EXTERNAL_APM_CONTROLLER = "external_apm_controller";
    constexpr const char* SWITCH_AFTERTOUCH = "aftertouch";
    constexpr const char* EVENT_NOTE_ON = "note_on";
    constexpr const char* EVENT_AFTERTOUCH = "aftertouch";
    constexpr const char* EVENT_WHEELS = "wheels";
    constexpr const char* EVENT_VOLUME = "volume";
    constexpr const char* ACTION_ALLOW = "allow";
    constexpr const char* ACTION_IGNORE = "ignore";

    json setting(ToolRig& rig, const char* name, const std::string& value)
    {
        return rig.call(TOOL_SET_SETTING, {{"setting", name}, {"value", value}});
    }

    json filter(ToolRig& rig, const char* event, const json& channel, const char* action)
    {
        return rig.call(TOOL_SET_FILTER, {{"event", event}, {"channel", channel}, {"action", action}});
    }

    /// How many §04 items of any kind the simulated sampler has been sent.
    std::size_t midiItemsSent(const ToolRig& rig)
    {
        return rig.sampler->midiConfig().events.size();
    }

    void checkPreviousValueSaid(const std::string& answer)
    {
        CHECK(hasText(answer, "could not be read"));
        CHECK(hasText(answer, "cannot be put back"));
    }
}

TEST_CASE("Given the server, When the tools are listed, Then set_midi_setting and set_midi_filter change the sampler without deleting anything and neither reads [RQ-MCP-047, RQ-MCP-013]",
          "[mcp][midi]")
{
    ToolRig rig;
    for (const char* name : {TOOL_SET_SETTING, TOOL_SET_FILTER})
    {
        const json tool = rig.tool(name);
        INFO(name);
        REQUIRE_FALSE(tool.is_null());
        CHECK_FALSE(tool["annotations"]["readOnlyHint"].get<bool>());
        CHECK_FALSE(tool["annotations"]["destructiveHint"].get<bool>());
        CHECK(tool["annotations"]["idempotentHint"].get<bool>());
    }
}

TEST_CASE("Given the program change switch, When it is set off then on, Then the simulated sampler holds each and the answer says the previous value is unknown [RQ-MCP-047]",
          "[mcp][midi]")
{
    ToolRig rig;
    const std::string off = toolText(setting(rig, SWITCH_PROGRAM_CHANGE, "off"));
    CHECK(hasText(off, "program_change"));
    CHECK(hasText(off, "off"));
    checkPreviousValueSaid(off);
    CHECK(rig.sampler->midiConfig().programChangeEnable == 0);
    const json on = setting(rig, SWITCH_PROGRAM_CHANGE, "on");
    CHECK_FALSE(toolFailed(on));
    CHECK(rig.sampler->midiConfig().programChangeEnable == 1);
    CHECK(rig.accepted(akm::ItemId::MidiProgramChangeEnable) == 2);
}

TEST_CASE("Given each multi select mode, When it is set, Then the simulated sampler holds it as the byte the sampler uses [RQ-MCP-047]",
          "[mcp][midi]")
{
    ToolRig rig;
    const std::vector<std::pair<const char*, std::uint8_t>> modes = {{"off", 0}, {"program_change", 1}, {"bank", 2}};
    for (const auto& [mode, byte] : modes)
    {
        const json answer = setting(rig, SWITCH_MULTI_SELECT, mode);
        INFO(mode);
        CHECK_FALSE(toolFailed(answer));
        CHECK(hasText(toolText(answer), mode));
        CHECK(rig.sampler->midiConfig().multiSelect == byte);
    }
    CHECK(rig.accepted(akm::ItemId::MidiMultiSelect) == 3);
}

TEST_CASE("Given the multi select channel 3, When it is set, Then the simulated sampler holds 3, the answer gives it as 3 (= 4A) and says the previous value is unknown; 0, 15, 16 and 31 are held as they are [RQ-MCP-047]",
          "[mcp][midi]")
{
    ToolRig rig;
    const std::string three = toolText(setting(rig, SWITCH_MULTI_SELECT_CHANNEL, "3"));
    CHECK(hasText(three, "3 (= 4A)"));
    checkPreviousValueSaid(three);
    CHECK(rig.sampler->midiConfig().multiSelectChannel == 3);

    const std::vector<std::pair<const char*, std::uint8_t>> channels = {{"0", 0}, {"15", 15}, {"16", 16}, {"31", 31}};
    for (const auto& [channel, byte] : channels)
    {
        const json answer = setting(rig, SWITCH_MULTI_SELECT_CHANNEL, channel);
        INFO(channel);
        CHECK_FALSE(toolFailed(answer));
        CHECK(rig.sampler->midiConfig().multiSelectChannel == byte);
    }
    CHECK(rig.accepted(akm::ItemId::MidiMultiSelectChannel) == 5);
}

TEST_CASE("Given the external APM controller, When 0, 74 and 127 are set, Then the simulated sampler holds each [RQ-MCP-047]",
          "[mcp][midi]")
{
    ToolRig rig;
    for (const std::uint8_t controller : std::initializer_list<std::uint8_t>{0, 74, 127})
    {
        const json answer = setting(rig, SWITCH_EXTERNAL_APM_CONTROLLER, std::to_string(controller));
        INFO(static_cast<int>(controller));
        CHECK_FALSE(toolFailed(answer));
        CHECK(hasText(toolText(answer), std::to_string(controller).c_str()));
        CHECK(rig.sampler->midiConfig().externalApmController == controller);
    }
    CHECK(rig.accepted(akm::ItemId::MidiExternalApmController) == 3);
}

TEST_CASE("Given the aftertouch type, When channel and polyphonic are set, Then the simulated sampler holds 0 and 1 [RQ-MCP-047]",
          "[mcp][midi]")
{
    ToolRig rig;
    CHECK_FALSE(toolFailed(setting(rig, SWITCH_AFTERTOUCH, "polyphonic")));
    CHECK(rig.sampler->midiConfig().aftertouch == 1);
    CHECK_FALSE(toolFailed(setting(rig, SWITCH_AFTERTOUCH, "Channel")));
    CHECK(rig.sampler->midiConfig().aftertouch == 0);
    CHECK(rig.accepted(akm::ItemId::MidiAftertouch) == 2);
}

TEST_CASE("Given a value the sampler does not take, When a MIDI setting is set, Then nothing is sent and the answer says what is accepted [RQ-MCP-047]",
          "[mcp][midi]")
{
    struct Case
    {
        const char* setting;
        const char* value;
        const char* accepted;
    };
    const std::vector<Case> cases = {
        {SWITCH_PROGRAM_CHANGE, "maybe", "on, off"},
        {SWITCH_MULTI_SELECT, "loud", "off, program_change, bank"},
        {SWITCH_MULTI_SELECT_CHANNEL, "32", "0 to 31"},
        {SWITCH_MULTI_SELECT_CHANNEL, "100", "0 to 31"},
        {SWITCH_MULTI_SELECT_CHANNEL, "-1", "0 to 31"},
        {SWITCH_MULTI_SELECT_CHANNEL, "3A", "0 to 31"},
        {SWITCH_MULTI_SELECT_CHANNEL, "", "0 to 31"},
        {SWITCH_MULTI_SELECT_CHANNEL, "A", "0 to 31"},
        {SWITCH_MULTI_SELECT_CHANNEL, "3.5", "0 to 31"},
        {SWITCH_EXTERNAL_APM_CONTROLLER, "128", "0 to 127"},
        {SWITCH_EXTERNAL_APM_CONTROLLER, "-1", "0 to 127"},
        {SWITCH_EXTERNAL_APM_CONTROLLER, "cutoff", "0 to 127"},
        {SWITCH_EXTERNAL_APM_CONTROLLER, "7.5", "0 to 127"},
        {SWITCH_AFTERTOUCH, "monophonic", "channel, polyphonic"},
    };
    ToolRig rig;
    for (const Case& test : cases)
    {
        const json answer = setting(rig, test.setting, test.value);
        INFO(test.setting << " " << test.value);
        CHECK(toolFailed(answer));
        CHECK(hasText(toolText(answer), test.accepted));
    }
    CHECK(midiItemsSent(rig) == 0);
}

TEST_CASE("Given a setting the tool does not have, a missing or a badly typed argument, When set_midi_setting is called, Then it is refused and nothing is sent [RQ-MCP-047]",
          "[mcp][midi]")
{
    ToolRig rig;
    const json unknown = rig.call(TOOL_SET_SETTING, {{"setting", "volume"}, {"value", "10"}});
    CHECK(toolFailed(unknown));
    CHECK(hasText(toolText(unknown), "program_change, multi_select, multi_select_channel, external_apm_controller, aftertouch"));
    CHECK(toolFailed(rig.call(TOOL_SET_SETTING)));
    CHECK(toolFailed(rig.call(TOOL_SET_SETTING, {{"setting", SWITCH_AFTERTOUCH}})));
    CHECK(toolFailed(rig.call(TOOL_SET_SETTING, {{"value", "bank"}})));
    CHECK(toolFailed(rig.call(TOOL_SET_SETTING, {{"setting", SWITCH_EXTERNAL_APM_CONTROLLER}, {"value", 5}})));
    CHECK(toolFailed(rig.call(TOOL_SET_SETTING, {{"setting", SWITCH_AFTERTOUCH}, {"value", "channel"}, {"extra", 1}})));
    CHECK(midiItemsSent(rig) == 0);
}

TEST_CASE("Given each event type, When it is ignored and then allowed on channel 5, Then the simulated sampler holds the filter on that channel only, and the answer names the event, the channel as 5 (= 6A) and that the previous value is unknown [RQ-MCP-047]",
          "[mcp][midi]")
{
    const std::vector<const char*> events = {EVENT_NOTE_ON, EVENT_AFTERTOUCH, EVENT_WHEELS, EVENT_VOLUME};
    ToolRig rig;
    for (std::size_t type = 0; type < FILTER_EVENT_TYPES; ++type)
    {
        const std::string ignored = toolText(filter(rig, events[type], 5, ACTION_IGNORE));
        INFO(events[type]);
        CHECK(hasText(ignored, events[type]));
        CHECK(hasText(ignored, "channel 5 (= 6A)"));
        CHECK(hasText(ignored, ACTION_IGNORE));
        checkPreviousValueSaid(ignored);
        CHECK_FALSE(rig.sampler->midiConfig().filterAllowed[type][5]);
        CHECK(rig.sampler->midiConfig().filterAllowed[type][4]);
        CHECK(rig.sampler->midiConfig().filterAllowed[type][6]);

        CHECK_FALSE(toolFailed(filter(rig, events[type], 5, ACTION_ALLOW)));
        CHECK(rig.sampler->midiConfig().filterAllowed[type][5]);
    }
    CHECK(rig.accepted(akm::ItemId::MidiFilterIgnore) == FILTER_EVENT_TYPES);
    CHECK(rig.accepted(akm::ItemId::MidiFilterAllow) == FILTER_EVENT_TYPES);
}

TEST_CASE("Given the channels 0, 15, 16 and 31 as numbers or as text, When a filter is set, Then the simulated sampler holds the filter on those channels [RQ-MCP-047]",
          "[mcp][midi]")
{
    ToolRig rig;
    const std::vector<std::pair<json, std::size_t>> channels = {{0, 0}, {"15", 15}, {16, 16}, {"31", 31}};
    for (const auto& [channel, index] : channels)
    {
        INFO(channel.dump());
        CHECK_FALSE(toolFailed(filter(rig, EVENT_WHEELS, channel, ACTION_IGNORE)));
        CHECK_FALSE(rig.sampler->midiConfig().filterAllowed[2][index]);
    }
}

TEST_CASE("Given a channel of 32 or -1, a channel with a letter, an unknown event or action, When a filter is set, Then nothing is sent and the answer says what is accepted [RQ-MCP-047]",
          "[mcp][midi]")
{
    ToolRig rig;
    const json tooHigh = filter(rig, EVENT_NOTE_ON, 32, ACTION_IGNORE);
    CHECK(toolFailed(tooHigh));
    CHECK(hasText(toolText(tooHigh), "0 to 31"));
    CHECK(toolFailed(filter(rig, EVENT_NOTE_ON, -1, ACTION_IGNORE)));
    CHECK(toolFailed(filter(rig, EVENT_NOTE_ON, "3A", ACTION_IGNORE)));
    CHECK(toolFailed(filter(rig, EVENT_NOTE_ON, "16B", ACTION_IGNORE)));
    CHECK(toolFailed(filter(rig, EVENT_NOTE_ON, 2.5, ACTION_IGNORE)));
    CHECK(toolFailed(filter(rig, EVENT_NOTE_ON, true, ACTION_IGNORE)));

    const json event = filter(rig, "pitch_bend", 1, ACTION_IGNORE);
    CHECK(toolFailed(event));
    CHECK(hasText(toolText(event), "note_on, aftertouch, wheels, volume"));

    const json action = filter(rig, EVENT_NOTE_ON, 1, "mute");
    CHECK(toolFailed(action));
    CHECK(hasText(toolText(action), "allow, ignore"));

    CHECK(midiItemsSent(rig) == 0);
}

TEST_CASE("Given a missing, an unknown or a badly typed argument, When set_midi_filter is called, Then it is refused and nothing is sent [RQ-MCP-047]",
          "[mcp][midi]")
{
    ToolRig rig;
    CHECK(toolFailed(rig.call(TOOL_SET_FILTER)));
    CHECK(toolFailed(rig.call(TOOL_SET_FILTER, {{"event", EVENT_NOTE_ON}, {"channel", 1}})));
    CHECK(toolFailed(rig.call(TOOL_SET_FILTER, {{"event", EVENT_NOTE_ON}, {"action", ACTION_ALLOW}})));
    CHECK(toolFailed(rig.call(TOOL_SET_FILTER, {{"channel", 1}, {"action", ACTION_ALLOW}})));
    CHECK(toolFailed(rig.call(TOOL_SET_FILTER, {{"event", 3}, {"channel", 1}, {"action", ACTION_ALLOW}})));
    CHECK(toolFailed(rig.call(TOOL_SET_FILTER, {{"event", EVENT_NOTE_ON}, {"channel", 1}, {"action", ACTION_ALLOW}, {"extra", 1}})));
    CHECK(midiItemsSent(rig) == 0);
}
