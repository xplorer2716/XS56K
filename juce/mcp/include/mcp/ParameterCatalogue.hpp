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

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "akm/ItemCatalogue.hpp"

namespace mcp
{
    // The parameters of a program in the musician's vocabulary: a table of rows, each naming a Set and a Get item
    // of the AKM catalogue, and the functions that turn what a person (or a model) says into what the sampler
    // takes and back. Adding a parameter is adding a row, not code. [RQ-MCP-004, RQ-MCP-005, RQ-MCP-006,
    // RQ-MCP-010, ADR-MCP-001 (DEC-MCP-005)]

    /// Where the sampler keeps the value: on the current keygroup (§08), on the current program (§0A) or on a zone of
    /// the current keygroup (§06: the zone number is the first argument of every item, 0 meaning all four zones).
    enum class ParameterScope
    {
        Keygroup,
        Program,
        Zone,
    };

    /// How a value is said: `Number` (a whole number in the sampler's own units, in steps of `step`), `Signed`
    /// (a whole number with a sign, which the sampler holds as a sign byte and a magnitude), `Choice` (one of the
    /// `labels`, named as the sampler's screen names them) or `Switch` (off or on).
    enum class ParameterKind
    {
        Number,
        Signed,
        Choice,
        Switch,
    };

    struct ParameterDefinition
    {
        std::string name;                   ///< in words, lower case: "filter cutoff"
        std::vector<std::string> aliases;   ///< other ways to say it: "cutoff"
        std::string group;                  ///< the primary name of its group: "filter"
        std::string description;
        ParameterScope scope = ParameterScope::Keygroup;
        ParameterKind kind = ParameterKind::Number;
        akm::ItemId setItem{};
        akm::ItemId getItem{};
        /// The arguments that come before the value in both items (the LFO number of an LFO parameter).
        std::vector<std::int64_t> leadingArguments;
        /// The range of the value as said, in its units. `Number`: `min` and `max` are multiples of `step`. `Signed`:
        /// `-max` to `max`. `Choice`: 0 to the number of labels less one, the code. `Switch`: 0 to 1.
        std::int64_t min = 0;
        std::int64_t max = 0;
        /// `Number` only: the value is the item's value times `step`, plus `offset` (the filter attenuation is a code 0-5
        /// for 0-30 dB; the keygroup level is a code 0-10 for -30 to 30 dB, `offset` -30).
        std::int64_t step = 1;
        std::int64_t offset = 0;
        /// `Signed` only: how many 7-bit bytes hold the magnitude after the sign byte, most significant first (the zone's
        /// velocity to start is two, +-9999).
        std::int64_t magnitudeBytes = 1;
        std::string unit;                   ///< "dB", or empty
        std::vector<std::string> labels;    ///< `Choice`: the label of code i; `Switch`: off, on
        /// `Choice` only: other ways to say a choice, each with its code ("pitch bend" for BEND).
        std::vector<std::pair<std::string, std::int64_t>> choiceAliases;
    };

    struct GroupDefinition
    {
        std::string name;                   ///< "amplitude envelope"
        std::vector<std::string> aliases;   ///< "amp envelope"; an alias may be shared by several groups ("lfo")
        std::string description;
    };

    struct NameResolution
    {
        const ParameterDefinition* parameter = nullptr;
        /// When `parameter` is null: up to three names that look like what was said, nearest first; empty when
        /// nothing does.
        std::vector<std::string> suggestions;
    };

    /// `value` is in the parameter's units; when it is empty, `problem` says in plain words what was wrong and
    /// what is accepted.
    struct ValueResolution
    {
        std::optional<std::int64_t> value;
        std::string problem;
    };

    class ParameterCatalogue
    {
    public:
        /// The catalogue of the program parameters the server edits. [RQ-MCP-004, RQ-MCP-010]
        [[nodiscard]] static const ParameterCatalogue& standard();

        ParameterCatalogue(std::vector<GroupDefinition> groups, std::vector<ParameterDefinition> parameters);

        [[nodiscard]] const std::vector<GroupDefinition>& groups() const { return _groups; }
        [[nodiscard]] const std::vector<ParameterDefinition>& parameters() const { return _parameters; }

        /// The groups a name or alias designates, tolerant as `resolveName` is: one, or several for an alias that
        /// is shared ("lfo" is both LFOs), or none.
        [[nodiscard]] std::vector<const GroupDefinition*> findGroups(std::string_view text) const;

        [[nodiscard]] std::vector<const ParameterDefinition*> parametersInGroup(const GroupDefinition& group) const;

        /// Case, spaces, hyphens and underscores do not matter ("Filter-Cutoff" is "filter cutoff"); aliases are
        /// accepted.
        [[nodiscard]] NameResolution resolveName(std::string_view text) const;

    private:
        std::vector<GroupDefinition> _groups;
        std::vector<ParameterDefinition> _parameters;
    };

    /// The form in which two spellings are compared: lower case; the words "plus" and "minus" as `+` and `-`,
    /// "poles" as "pole"; spaces, underscores and hyphens dropped, except a final `-` (SQUARE- is not SQUARE).
    [[nodiscard]] std::string normalizeText(std::string_view text);

    /// Checks a number said for a parameter: in range, and in steps. [RQ-MCP-006]
    [[nodiscard]] ValueResolution resolveValue(const ParameterDefinition& parameter, std::int64_t number);

    /// Reads what was said for a parameter: a label, a number (a code for a choice) or, for a switch, on, off, true,
    /// false, yes, no, enabled, disabled. [RQ-MCP-006]
    [[nodiscard]] ValueResolution resolveValue(const ParameterDefinition& parameter, std::string_view text);

    /// The values of the Set item that follow the leading arguments, for a value in the parameter's units: one
    /// number, or the sign and the magnitude of a signed one. The value must have been resolved. [RQ-MCP-006]
    [[nodiscard]] std::vector<std::int64_t> toItemValues(const ParameterDefinition& parameter, std::int64_t value);

    /// The value in the parameter's units from the REPLY of its Get item, or nothing when the REPLY does not have
    /// the number of values the item gives. [RQ-MCP-005]
    [[nodiscard]] std::optional<std::int64_t> fromItemValues(const ParameterDefinition& parameter,
                                                             std::span<const std::int64_t> reply);

    /// How a value reads in an answer: "80", "-40", "12 dB", "on", "2-POLE LP+ (code 2)". [RQ-MCP-005]
    [[nodiscard]] std::string describeValue(const ParameterDefinition& parameter, std::int64_t value);

    /// What a parameter accepts: "0 to 100", "-100 to 100", "0, 6, 12, 18, 24 or 30 dB", "on or off", or the
    /// labels of a choice. [RQ-MCP-004]
    [[nodiscard]] std::string describeRange(const ParameterDefinition& parameter);
}
