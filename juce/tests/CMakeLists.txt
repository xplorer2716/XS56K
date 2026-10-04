# Test suites, built only with BUILD_TESTS. [RQ-BLD-002, RQ-AKM-016, ADR-AKM-001 (DEC-AKM-008)]
#
# Catch2 is fetched and CTest/Catch are included by the root juce/CMakeLists.txt.
# Test sources mirror the tree of the library they exercise (akm/ for xs56k_akm).

add_subdirectory(support)
add_subdirectory(compile_checks)
add_subdirectory(probe)

add_executable(xs56k_akm_tests
    akm/CapturedFramesTests.cpp
    akm/ChecksumTests.cpp
    akm/ClockArithmeticTests.cpp
    akm/CommandEncodingTests.cpp
    akm/CommandResultTests.cpp
    akm/ConfirmationDecodingTests.cpp
    akm/DiskPrimitivesTests.cpp
    akm/ExecutorTests.cpp
    akm/FirstContactProbeTests.cpp
    akm/FrontPanelCloseTests.cpp
    akm/FrontPanelRemoteTests.cpp
    akm/FrontPanelTests.cpp
    akm/ItemCatalogueTests.cpp
    akm/KeygroupAmplitudeEnvelopeTests.cpp
    akm/KeygroupAuxEnvelopeTests.cpp
    akm/KeygroupFilterEnvelopeTests.cpp
    akm/KeygroupFilterTests.cpp
    akm/KeygroupGeneralOptionsTests.cpp
    akm/KeygroupModSourcesParametersTests.cpp
    akm/KeygroupPitchAmpTests.cpp
    akm/KeygroupPrimitivesTests.cpp
    akm/LfoParametersTests.cpp
    akm/LibraryInfoTests.cpp
    akm/MidiConfigTests.cpp
    akm/MultiDeleteAllGuardTests.cpp
    akm/MultiEditingTests.cpp
    akm/MultiFxConfigurationTests.cpp
    akm/MultiFxDiscoveryTests.cpp
    akm/MultiFxParametersTests.cpp
    akm/MultiInformationTests.cpp
    akm/MultiPartParametersTests.cpp
    akm/MultiPrimitivesTests.cpp
    akm/MidiTuneParametersTests.cpp
    akm/OutputParametersTests.cpp
    akm/PitchBendParametersTests.cpp
    akm/ProgramDeleteAllGuardTests.cpp
    akm/ProgramGeneralInfoTests.cpp
    akm/ProgramPrimitivesTests.cpp
    akm/ProgramStructureTests.cpp
    akm/RealSamplerSuiteTests.cpp
    akm/ReplySectionTests.cpp
    akm/SamplerErrorTests.cpp
    akm/SampleDeleteAllGuardTests.cpp
    akm/SampleParametersTests.cpp
    akm/SamplePrimitivesTests.cpp
    akm/SampleReadOnlyParametersTests.cpp
    akm/SceneListPrimitivesTests.cpp
    akm/ScenarioTests.cpp
    akm/SchedulerTests.cpp
    akm/SessionCloseTests.cpp
    akm/SessionOpenTests.cpp
    akm/SessionSmokeTests.cpp
    akm/SessionTests.cpp
    akm/SetListPrimitivesTests.cpp
    akm/SessionThreadingTests.cpp
    akm/SimulatedSamplerTests.cpp
    akm/SongPrimitivesTests.cpp
    akm/SysExConfigTests.cpp
    akm/SystemClearMemoryGuardTests.cpp
    akm/SystemSetupTests.cpp
    akm/SystemVersionTests.cpp
    akm/ValueFormatTests.cpp
    akm/WireLogTests.cpp
    akm/ZoneAllZonesTests.cpp
    akm/ZoneParametersTests.cpp
    akm/ZoneSampleTests.cpp)
target_link_libraries(xs56k_akm_tests PRIVATE
    xs56k_akm
    xs56k_akm_test_support
    Catch2::Catch2WithMain
    xs56k::warnings)
catch_discover_tests(xs56k_akm_tests)

# The MCP server's library: its units are tested without a sampler and without a process, and its public headers
# hold no JUCE include. [RQ-MCP-011, RQ-MCP-012, ADR-MCP-001 (DEC-MCP-009)]
add_executable(xs56k_mcp_tests
    mcp/McpServerTests.cpp
    mcp/ParameterCatalogueTests.cpp
    mcp/SamplerGatewayTests.cpp
    mcp/ServerOptionsTests.cpp
    mcp/ToolsTests.cpp)
# The gateway tests seed the simulated sampler with the raw-frame helpers of the AKM tests.
target_include_directories(xs56k_mcp_tests PRIVATE akm)
target_link_libraries(xs56k_mcp_tests PRIVATE
    xs56k_mcp
    xs56k_akm_test_support
    Catch2::Catch2WithMain
    xs56k::warnings)
catch_discover_tests(xs56k_mcp_tests)
add_test(NAME mcp_public_headers_have_no_juce_include
    COMMAND ${CMAKE_COMMAND}
        -DHEADER_DIR=${PROJECT_SOURCE_DIR}/mcp/include
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckNoJuceIncludes.cmake)
# The built executable, run as an MCP client runs it: a scripted conversation on its standard input, with no
# sampler (the ports it is given do not exist). [RQ-MCP-001, RQ-MCP-003, ADR-MCP-001 (DEC-MCP-009), TASK-MCP-006]
add_test(NAME mcp_server_executable_answers_without_a_sampler
    COMMAND ${CMAKE_COMMAND}
        -DSERVER=$<TARGET_FILE:xs56k_mcp_server>
        "-DSERVER_ARGS=--in;XS56K No Such Input;--out;XS56K No Such Output"
        -DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/mcp/conversations/no_sampler.jsonl
        -DPATTERNS_FILE=${CMAKE_CURRENT_SOURCE_DIR}/mcp/conversations/no_sampler.patterns
        -DEXPECT_LINES=4
        "-DERROR_PATTERN=input closed"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/RunServerConversation.cmake)
# --list-ports prints the ports and exits 0; a missing port is a usage error: status 2, the reason on standard error.
add_test(NAME mcp_server_executable_lists_ports
    COMMAND ${CMAKE_COMMAND}
        -DSERVER=$<TARGET_FILE:xs56k_mcp_server>
        -DSERVER_ARGS=--list-ports
        "-DOUTPUT_PATTERN=MIDI input ports.*MIDI output ports"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/RunServerConversation.cmake)
add_test(NAME mcp_server_executable_refuses_a_missing_port
    COMMAND ${CMAKE_COMMAND}
        -DSERVER=$<TARGET_FILE:xs56k_mcp_server>
        "-DSERVER_ARGS=--in;Somewhere"
        -DEXPECT_STATUS=2
        "-DERROR_PATTERN=--out is required"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/RunServerConversation.cmake)
# The server edits and never creates, renames, deletes or saves. [RQ-MCP-008, ADR-MCP-001 (DEC-MCP-007)]
add_test(NAME mcp_sources_call_no_destructive_primitive
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${PROJECT_SOURCE_DIR}/mcp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckNoDestructiveCalls.cmake)

# No JUCE include in the public headers of the AKM layer. [RQ-AKM-019]
add_test(NAME akm_public_headers_have_no_juce_include
    COMMAND ${CMAKE_COMMAND}
        -DHEADER_DIR=${PROJECT_SOURCE_DIR}/akm/include
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckNoJuceIncludes.cmake)

# The item catalogue: the checked-in table matches its data file, the data file matches the spec's item list,
# and the script that says so is itself tested. Python only runs here, in the test suite, never during the
# build; without it the checks are skipped and said to be. [TASK-AKM-008, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]
# Each Python script under tools/ gets its own discover call, scoped to its own test_*.py file: a single
# combined discovery would fold the mutation tool's own (much slower) tests into this one's name.
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME akm_item_table_is_up_to_date
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/generate_akm_items.py --check)
    add_test(NAME akm_item_catalogue_matches_the_spec
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/generate_akm_items.py --coverage)
    add_test(NAME akm_item_catalogue_script_tests
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tools -p "test_generate_akm_items.py")
    # The mutation check tool (RQ-BLD-015): configures and builds the small fixture project of
    # tools/mutate_fixture/ several times over, so it is markedly slower than the catalogue's own
    # tests above. [TASK-BLD-012]
    add_test(NAME bld_mutate_tool_script_tests
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tools -p "test_mutate.py")
else()
    message(STATUS "Python 3 not found: the item catalogue and mutation-tool checks of juce/tests/tools are not registered")
endif()
