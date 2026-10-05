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
    // that cannot be carried out answers a problem in plain words rather than throwing. It sends no command that
    // creates, renames, deletes or saves anything (RQ-MCP-008). [RQ-MCP-003, RQ-MCP-005, RQ-MCP-006, RQ-MCP-007,
    // RQ-MCP-008, RQ-MCP-009, ADR-MCP-001 (DEC-MCP-003, DEC-MCP-004, DEC-MCP-006, DEC-MCP-007)]

    struct GatewayConfig
    {
        std::string inputPort;   ///< what the sampler sends on
        std::string outputPort;  ///< what the sampler receives on
        std::uint32_t deviceId = 0;
        std::chrono::milliseconds commandTimeout = std::chrono::duration_cast<std::chrono::milliseconds>(akm::DEFAULT_COMMAND_TIMEOUT);
        /// Whether the session switches Sync LCD off and Auto screen update on, so that the sampler's screen follows
        /// the edits (both are put back at the close); false leaves both alone.
        bool touchLcdSettings = true;
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

    /// Which keygroup an edit or a reading is about: one (1 to the program's count), or all of them.
    struct KeygroupSelection
    {
        std::optional<int> keygroup;  ///< empty: all keygroups

        [[nodiscard]] static KeygroupSelection all() { return {}; }
        [[nodiscard]] static KeygroupSelection of(int keygroup) { return KeygroupSelection{keygroup}; }
    };

    /// One value of a parameter: of a keygroup (1-based) for a keygroup parameter, or of the program.
    struct ParameterValue
    {
        std::optional<int> keygroup;
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

        /// The value of a parameter of the current program, for one keygroup or for each. A keygroup beyond the
        /// program's is a problem that gives the count, and nothing is sent to the sampler but the questions that
        /// say so. A program parameter has no keygroup: its one value carries none. [RQ-MCP-005, RQ-MCP-007]
        [[nodiscard]] Outcome<std::vector<ParameterValue>> readParameter(const ParameterDefinition& parameter,
                                                                         KeygroupSelection selection);

        /// Sets a parameter of the current program (the value must have been resolved by the catalogue) and reads it
        /// back; the answer is what the sampler reports. A reading that differs from the value set is a problem that
        /// says so. The keygroup the sampler has selected is left as the edit set it. [RQ-MCP-006]
        [[nodiscard]] Outcome<std::vector<ParameterValue>> writeParameter(const ParameterDefinition& parameter,
                                                                          std::int64_t value, KeygroupSelection selection);

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
                                                                                  bool needsCurrentProgram);
        [[nodiscard]] Outcome<int> keygroupCount();
        [[nodiscard]] Outcome<ProgramInfo> currentProgramInfo();
        [[nodiscard]] Outcome<std::vector<ParameterValue>> editParameter(const ParameterDefinition& parameter,
                                                                         std::optional<std::int64_t> valueToSet,
                                                                         KeygroupSelection selection);

        GatewayConfig _config;
        common::midi::MidiBackend& _backend;
        std::unique_ptr<Connection> _connection;
    };
}
