# xs56k_mcp: the MCP server's logic (protocol, parameter catalogue, sampler gateway, tools). It depends on the
# AKM layer and on nlohmann/json only and exposes no JUCE type in its public headers; the executable that
# knows the JUCE MIDI backend is added with TASK-MCP-006. [RQ-MCP-011, ADR-MCP-001 (DEC-MCP-001)]

add_library(xs56k_mcp STATIC
    src/McpServer.cpp
    src/ParameterCatalogue.cpp
    src/StandardParameters.cpp)
target_include_directories(xs56k_mcp PUBLIC include)
target_link_libraries(xs56k_mcp PUBLIC xs56k_akm nlohmann_json::nlohmann_json PRIVATE xs56k::warnings)
