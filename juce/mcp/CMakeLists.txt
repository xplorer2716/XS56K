# xs56k_sampler: the sampler gateway and the parameter catalogues, shared by the MCP server and the editor application.
# It depends on the AKM layer only (no nlohmann/json, no MCP protocol) and exposes no JUCE type in its public headers.
# The files stay in juce/mcp and the namespace stays mcp, so the coverage generator and the source check of destructive
# calls keep their paths. [RQ-APP-014, RQ-MCP-057, ADR-APP-001 (DEC-APP-002), TASK-APP-010]
add_library(xs56k_sampler STATIC
    src/FxCatalogue.cpp
    src/ParameterCatalogue.cpp
    src/SamplerGateway.cpp
    src/SamplerGatewayDisk.cpp
    src/SamplerGatewayFx.cpp
    src/SamplerGatewayKeys.cpp
    src/SamplerGatewayLists.cpp
    src/StandardParameters.cpp)
target_include_directories(xs56k_sampler PUBLIC include)
target_link_libraries(xs56k_sampler PUBLIC xs56k_akm PRIVATE xs56k::warnings)

# xs56k_mcp: the MCP server's logic (protocol, tools, launch arguments) over xs56k_sampler. It depends on the AKM layer
# and on nlohmann/json only and exposes no JUCE type in its public headers.
# xs56k_mcp_server is the executable: the only place that knows the JUCE MIDI backend. [RQ-MCP-011,
# ADR-MCP-001 (DEC-MCP-001), ADR-APP-001 (DEC-APP-002)]

add_library(xs56k_mcp STATIC
    src/McpServer.cpp
    src/ServerOptions.cpp
    src/Tools.cpp)
target_include_directories(xs56k_mcp PUBLIC include)
target_link_libraries(xs56k_mcp PUBLIC xs56k_sampler nlohmann_json::nlohmann_json PRIVATE xs56k::warnings)

# The executable an MCP client launches: MIDI ports from its arguments, the protocol on standard input and output.
# [TASK-MCP-006, RQ-MCP-001, RQ-MCP-002, ADR-MCP-001 (DEC-MCP-001, DEC-MCP-008)]
add_executable(xs56k_mcp_server server/main.cpp)
target_compile_definitions(xs56k_mcp_server PRIVATE XS56K_MCP_VERSION="${VERSION_FULL}")
target_link_libraries(xs56k_mcp_server PRIVATE
    xs56k_mcp
    xs56k_midi_juce
    xs56k::warnings)
