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

// The instructions the server gives at `initialize` say what it offers in each launch: they name a tool of every family that `tools/list`
// offers, name no tool that it does not offer, and contain none of the sentences that were true before PLAN-MCP-002 to PLAN-MCP-005. The
// table of the families holds every tool the server can offer, so that a new tool fails the test until it is placed in a family, and
// therefore until the instructions are looked at. [TASK-MCP-065, RQ-MCP-043, RQ-MCP-001, ADR-MCP-004 (DEC-MCP-026)]
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "akm/RealScheduler.hpp"
#include "akm/harness/SimulatedMidiBackend.hpp"
#include "mcp/McpServer.hpp"
#include "mcp/ParameterCatalogue.hpp"
#include "mcp/SamplerGateway.hpp"
#include "mcp/Tools.hpp"

using json = nlohmann::json;

namespace
{
    constexpr const char* LEGACY_REVISION = "2025-11-25";
    constexpr const char* MODERN_REVISION = "2026-07-28";
    constexpr std::size_t TOOLS_WITH_NO_OPTION = 56;
    constexpr std::size_t TOOLS_WITH_DISK = 72;
    constexpr std::size_t TOOLS_WITH_FRONT_PANEL = 61;
    constexpr std::size_t TOOLS_WITH_BOTH = 77;

    /// Which launch option a family of tools needs.
    enum class Needs
    {
        Nothing,
        Disk,
        FrontPanel
    };

    struct Family
    {
        const char* name;
        Needs needs;
        std::vector<std::string> tools;
    };

    /// Every tool the server can offer, by family.
    const std::vector<Family>& families()
    {
        static const std::vector<Family> table = {
            {"the sampler", Needs::Nothing, {"get_status", "get_system_info"}},
            {"programs", Needs::Nothing, {"list_programs", "select_program", "create_program", "rename_program", "delete_program", "delete_all_programs"}},
            {"parameters", Needs::Nothing, {"list_parameters", "get_parameters", "set_parameter"}},
            {"keygroups and zones", Needs::Nothing, {"add_keygroups", "delete_keygroup", "get_zone_samples", "set_zone_sample"}},
            {"samples", Needs::Nothing,
             {"list_samples", "select_sample", "get_sample_parameters", "set_sample_parameter", "rename_sample", "delete_sample", "delete_all_samples",
              "audition_sample"}},
            {"multis", Needs::Nothing,
             {"list_multis", "select_multi", "create_multi", "rename_multi", "delete_multi", "delete_all_multis", "get_multi_parameters",
              "set_multi_parameter", "set_part_program", "get_part_programs", "clear_part", "set_multi_program_number"}},
            {"settings", Needs::Nothing, {"get_sampler_settings", "set_sampler_setting"}},
            {"MIDI setup", Needs::Nothing, {"set_midi_setting", "set_midi_filter"}},
            {"song files", Needs::Nothing, {"list_song_files", "select_song_file", "rename_song_file", "delete_song_file"}},
            {"set lists", Needs::Nothing, {"list_set_lists", "rename_set_list", "delete_set_list"}},
            {"scenelists", Needs::Nothing, {"list_scenelists", "select_scenelist", "rename_scenelist", "delete_scenelist"}},
            {"effects board", Needs::Nothing, {"get_fx_board", "set_fx_channel_mute", "set_fx_module", "get_fx_parameter", "set_fx_parameter"}},
            {"clearing the memory", Needs::Nothing, {"clear_sampler_memory"}},
            {"disks", Needs::Disk, {"list_disks", "select_disk", "list_disk_contents", "open_folder", "close_folder", "get_disk_space"}},
            {"files and folders", Needs::Disk, {"create_folder", "rename_folder", "delete_folder", "rename_file", "delete_file"}},
            {"loading", Needs::Disk, {"load_file", "load_folder"}},
            {"saving", Needs::Disk, {"save_memory_item", "save_all_memory_items"}},
            {"auditioning a file", Needs::Disk, {"audition_file"}},
            {"front-panel keys", Needs::FrontPanel, {"press_key", "hold_key", "release_key", "turn_data_wheel", "send_ascii_key"}},
        };
        return table;
    }

    bool offered(const Family& family, const mcp::ToolOptions& options)
    {
        switch (family.needs)
        {
            case Needs::Nothing:
                return true;
            case Needs::Disk:
                return options.allowDisk;
            case Needs::FrontPanel:
                return options.allowFrontPanel;
        }
        return false;
    }

    /// The sentences the server's instructions held when they were written for the program editing of PLAN-MCP-001.
    const std::vector<std::string>& staleSentences()
    {
        static const std::vector<std::string> sentences = {"no tool creates, deletes or loads a sample", "no tool creates, deletes or renames a multi",
                                                           "nothing is saved by this server"};
        return sentences;
    }

    /// A server launched as `main.cpp` launches it, over a simulated MIDI bus: what it says at `initialize` and what it lists.
    struct Launch
    {
        explicit Launch(mcp::ToolOptions launchOptions) : options(launchOptions)
        {
            backend.addSampler();
            mcp::ServerIdentity identity;
            identity.name = "xs56k-mcp";
            identity.title = "XS56K";
            identity.version = "0.0.1";
            identity.instructions = mcp::serverInstructions(options);
            server = std::make_unique<mcp::McpServer>(identity, mcp::makeAllTools(gateway, mcp::ParameterCatalogue::standard(), options));
        }

        static mcp::GatewayConfig configFor(const akm::harness::SimulatedMidiBackend& bus)
        {
            mcp::GatewayConfig config;
            config.inputPort = bus.inputName();
            config.outputPort = bus.outputName();
            return config;
        }

        json ask(int id, const std::string& method, json params)
        {
            const json message{{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", std::move(params)}};
            const auto answer = server->handleLine(message.dump());
            REQUIRE(answer.has_value());
            return json::parse(*answer);
        }

        std::string instructions()
        {
            const json answer = ask(1, "initialize",
                                    json{{"protocolVersion", LEGACY_REVISION},
                                         {"capabilities", json::object()},
                                         {"clientInfo", {{"name", "test"}, {"version", "1"}}}});
            REQUIRE(answer.contains("result"));
            REQUIRE(answer["result"].contains("instructions"));
            return answer["result"]["instructions"].get<std::string>();
        }

        std::set<std::string> toolNames()
        {
            const json answer = ask(2, "tools/list",
                                    json{{"_meta",
                                          {{"io.modelcontextprotocol/protocolVersion", MODERN_REVISION},
                                           {"io.modelcontextprotocol/clientCapabilities", json::object()}}}});
            REQUIRE(answer.contains("result"));
            std::set<std::string> names;
            for (const json& tool : answer["result"]["tools"])
                names.insert(tool["name"].get<std::string>());
            return names;
        }

        mcp::ToolOptions options;
        akm::RealScheduler scheduler;
        akm::harness::SimulatedMidiBackend backend{scheduler};
        mcp::SamplerGateway gateway{backend, configFor(backend)};
        std::unique_ptr<mcp::McpServer> server;
    };

    /// The words of `text` that look like a tool name: lower-case words joined by underscores.
    std::set<std::string> toolLikeWords(const std::string& text)
    {
        static const std::regex word("[a-z]+(?:_[a-z]+)+");
        std::set<std::string> found;
        for (std::sregex_iterator it(text.begin(), text.end(), word), end; it != end; ++it)
            found.insert(it->str());
        return found;
    }

    mcp::ToolOptions optionsOf(bool disk, bool refresh, bool frontPanel)
    {
        mcp::ToolOptions options;
        options.allowDisk = disk;
        options.allowDiskRefresh = refresh;
        options.allowFrontPanel = frontPanel;
        return options;
    }

    bool contains(const std::string& text, const std::string& part)
    {
        return text.find(part) != std::string::npos;
    }
}

TEST_CASE("Given the server launched with no option, with --allow-disk, with --allow-front-panel and with both, When tools/list is asked, Then it offers 56, 72, 61 and 77 tools and each belongs to a family of the table [TASK-MCP-065, RQ-MCP-043]",
          "[mcp][instructions]")
{
    struct Case
    {
        mcp::ToolOptions options;
        std::size_t count;
    };
    for (const Case& launch : {Case{optionsOf(false, false, false), TOOLS_WITH_NO_OPTION}, Case{optionsOf(true, false, false), TOOLS_WITH_DISK},
                               Case{optionsOf(false, false, true), TOOLS_WITH_FRONT_PANEL}, Case{optionsOf(true, true, true), TOOLS_WITH_BOTH}})
    {
        Launch server(launch.options);
        const std::set<std::string> listed = server.toolNames();
        CHECK(listed.size() == launch.count);

        std::set<std::string> expected;
        for (const Family& family : families())
        {
            if (offered(family, launch.options))
                expected.insert(family.tools.begin(), family.tools.end());
        }
        CHECK(listed == expected);
    }
}

TEST_CASE("Given each launch, When initialize is answered, Then the instructions name a tool of every family that tools/list offers [TASK-MCP-065, RQ-MCP-043]",
          "[mcp][instructions]")
{
    for (const mcp::ToolOptions& options : {optionsOf(false, false, false), optionsOf(true, false, false), optionsOf(true, true, false),
                                            optionsOf(false, false, true), optionsOf(true, true, true)})
    {
        Launch server(options);
        const std::set<std::string> named = toolLikeWords(server.instructions());
        for (const Family& family : families())
        {
            if (!offered(family, options))
                continue;
            const bool namesOne = std::any_of(family.tools.begin(), family.tools.end(), [&](const std::string& tool) { return named.count(tool) > 0; });
            INFO("family: " << family.name << ", disk " << options.allowDisk << ", refresh " << options.allowDiskRefresh << ", front panel "
                            << options.allowFrontPanel);
            CHECK(namesOne);
        }
    }
}

TEST_CASE("Given each launch, When initialize is answered, Then the instructions name no tool that tools/list does not offer, whatever the option [TASK-MCP-065, RQ-MCP-043]",
          "[mcp][instructions]")
{
    for (const mcp::ToolOptions& options : {optionsOf(false, false, false), optionsOf(true, false, false), optionsOf(true, true, false),
                                            optionsOf(false, false, true), optionsOf(true, true, true)})
    {
        Launch server(options);
        const std::set<std::string> listed = server.toolNames();
        for (const std::string& word : toolLikeWords(server.instructions()))
        {
            INFO("word: " << word << ", disk " << options.allowDisk << ", front panel " << options.allowFrontPanel);
            CHECK(listed.count(word) == 1);
        }
    }
}

TEST_CASE("Given each launch, When initialize is answered, Then the instructions contain none of the sentences that the tools of PLAN-MCP-002 to PLAN-MCP-005 made false [TASK-MCP-065, RQ-MCP-043]",
          "[mcp][instructions]")
{
    for (const mcp::ToolOptions& options : {optionsOf(false, false, false), optionsOf(true, false, false), optionsOf(false, false, true),
                                            optionsOf(true, true, true)})
    {
        Launch server(options);
        const std::string text = server.instructions();
        for (const std::string& stale : staleSentences())
        {
            INFO(stale);
            CHECK_FALSE(contains(text, stale));
        }
    }
}

TEST_CASE("Given the launch with no option, When initialize is answered, Then the instructions say that every deletion asks for a confirm, that the delete-all tools and the clearing of the memory ask for a count, and that nothing reaches a disk; the disk and key paragraphs come only with their option [TASK-MCP-065, RQ-MCP-043, RQ-MCP-042]",
          "[mcp][instructions]")
{
    const std::string none = Launch(optionsOf(false, false, false)).instructions();
    CHECK(contains(none, "confirm"));
    CHECK(contains(none, "exact name"));
    CHECK(contains(none, "number"));
    CHECK(contains(none, "memory"));
    CHECK_FALSE(contains(none, "disk tools"));
    CHECK_FALSE(contains(none, "front-panel key tools"));

    const std::string disk = Launch(optionsOf(true, false, false)).instructions();
    CHECK(contains(disk, "disk tools"));
    CHECK(contains(disk, "never asks the sampler to refresh"));
    CHECK(contains(disk, "overwrite"));
    CHECK_FALSE(contains(disk, "front-panel key tools"));

    const std::string refresh = Launch(optionsOf(true, true, false)).instructions();
    CHECK(contains(refresh, "refresh true"));

    const std::string panel = Launch(optionsOf(false, false, true)).instructions();
    CHECK(contains(panel, "front-panel key tools"));
    CHECK_FALSE(contains(panel, "disk tools"));
}
