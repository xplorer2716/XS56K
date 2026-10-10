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

#include <string>
#include <string_view>
#include <vector>

namespace mcp
{
    // What the specification says of the effects modules (Table 24, the module codes) and of their parameters (Table 25, with their ranges), as
    // data for the effects tools: the sampler judges a parameter's own range, so the tools check it first and send nothing outside it. The values
    // are the raw numbers the sampler takes (a rate of 15 is 1.5 where the table says 0 to 99 = 0.0 to 9.9). Verified on the simulated sampler
    // only: the owner has no effects board. [RQ-MCP-053, ADR-MCP-005 (DEC-MCP-033)]

    /// One parameter of a module kind: its index on the wire, its name, its range as the sampler takes it and what the range means.
    struct FxParameter
    {
        int index;
        const char* name;  ///< snake case, as a client gives it
        int minimum;
        int maximum;
        const char* meaning;  ///< the table's own words for the range, such as "0 to 99 = 0.0 to 9.9", or an empty text
    };

    /// One kind of module (Table 24): its code, its name and its parameters.
    struct FxModuleKind
    {
        int code;
        const char* name;  ///< snake case, as a client gives it
        std::vector<FxParameter> parameters;
    };

    /// Every module kind of Table 24, in code order, "none" included.
    [[nodiscard]] const std::vector<FxModuleKind>& fxModuleKinds();

    /// The kind with that code, or null for a code Table 24 does not name (the sampler may use others: they are read as they are).
    [[nodiscard]] const FxModuleKind* fxKindByCode(int code);

    /// The kind with that name, in other letters or with a space or a hyphen for the underscore; null when there is none.
    [[nodiscard]] const FxModuleKind* fxKindByName(std::string_view name);

    /// The parameter of a kind with that name (in other letters, with a space or a hyphen for the underscore) or with that index in digits; null
    /// when there is none.
    [[nodiscard]] const FxParameter* fxParameterOf(const FxModuleKind& kind, std::string_view nameOrIndex);

    /// The kinds the module `module` of channel `channel` of an EB20 may be set to, by code; empty when that module cannot change its type: only
    /// modules 2 (modulation) and 3 (delay) of channels 0 and 1 can (the specification, p. 35 and Figure 2).
    [[nodiscard]] std::vector<int> eb20KindsFor(int channel, int module);

    /// The names of some kinds or parameters, as "a, b, c".
    [[nodiscard]] std::string fxKindNames(const std::vector<int>& codes);
    [[nodiscard]] std::string fxParameterNames(const FxModuleKind& kind);

    /// A parameter's range as a sentence: "0 to 99 (0 to 99 = 0.0 to 9.9)" or "-50 to 50".
    [[nodiscard]] std::string fxRangeText(const FxParameter& parameter);
}
