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

#include <vector>

#include "mcp/ParameterCatalogue.hpp"

namespace mcp
{
    // The rows of the standard catalogue: the data, apart from the code that reads it. [RQ-MCP-004, RQ-MCP-010,
    // ADR-MCP-001 (DEC-MCP-005)]
    [[nodiscard]] std::vector<GroupDefinition> standardGroups();
    [[nodiscard]] std::vector<ParameterDefinition> standardParameters();

    // The rows of the sample catalogue (section 0E). [RQ-MCP-020, ADR-MCP-002 (DEC-MCP-012)]
    [[nodiscard]] std::vector<GroupDefinition> sampleGroups();
    [[nodiscard]] std::vector<ParameterDefinition> sampleParameters();

    // The rows of the multi catalogue (the parameters of a part, section 0C). [RQ-MCP-021, ADR-MCP-002 (DEC-MCP-012)]
    [[nodiscard]] std::vector<GroupDefinition> multiGroups();
    [[nodiscard]] std::vector<ParameterDefinition> multiParameters();
}
