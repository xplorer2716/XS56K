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

# The framework library (ported from XplorerEditor): the tests pin what `Logger` and `AbstractController` do before their code is
# reorganised for SonarCloud. Headless, on the in-memory MIDI backend, no thread started. [TASK-QLT-006, RQ-QLT-005,
# ADR-QLT-001 (DEC-QLT-004)]
add_executable(xs56k_framework_tests
    framework/AbstractControllerAutomationTests.cpp
    framework/LoggerTests.cpp)
target_link_libraries(xs56k_framework_tests PRIVATE
    xs56k_framework
    Catch2::Catch2WithMain
    xs56k::warnings)
catch_discover_tests(xs56k_framework_tests)

# The MCP server's library: its units are tested without a sampler and without a process, and its public headers
# hold no JUCE include. [RQ-MCP-011, RQ-MCP-012, ADR-MCP-001 (DEC-MCP-009)]
add_executable(xs56k_mcp_tests
    mcp/AuditionToolsTests.cpp
    mcp/BulkDeleteToolsTests.cpp
    mcp/ClearMemoryToolTests.cpp
    mcp/DiskBrowseTests.cpp
    mcp/DiskFileOpsTests.cpp
    mcp/DiskLoadTests.cpp
    mcp/DiskNamedListTests.cpp
    mcp/DiskSaveTests.cpp
    mcp/FxCatalogueTests.cpp
    mcp/FxToolsTests.cpp
    mcp/FrontPanelToolsTests.cpp
    mcp/KeygroupToolsTests.cpp
    mcp/McpServerTests.cpp
    mcp/ParameterCatalogueLot2Tests.cpp
    mcp/ParameterCatalogueLot3Tests.cpp
    mcp/MultiStructureToolsTests.cpp
    mcp/MultiToolsTests.cpp
    mcp/ParameterCatalogueTests.cpp
    mcp/SampleStructureToolsTests.cpp
    mcp/SampleToolsTests.cpp
    mcp/ProgramStructureTests.cpp
    mcp/SamplerGatewayTests.cpp
    mcp/ServerOptionsTests.cpp
    mcp/SystemInfoToolsTests.cpp
    mcp/SamplerSettingsToolsTests.cpp
    mcp/MidiSetupToolsTests.cpp
    mcp/NamedListDeleteToolsTests.cpp
    mcp/NamedListToolsTests.cpp
    mcp/ServerInstructionsTests.cpp
    mcp/ToolsTests.cpp
    mcp/ZoneParameterTests.cpp
    mcp/ZoneSampleToolsTests.cpp)
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
# The server wired to the simulated sampler (test tooling, never shipped): the same library, the same tools, three
# programs in the sampler. A scripted conversation piped into it must give the output in
# mcp/conversations/simulated_session.expected.jsonl; to regenerate it after a deliberate change, run
#   xs56k_mcp_server_simulated < mcp/conversations/simulated_session.jsonl > mcp/conversations/simulated_session.expected.jsonl
# and read the diff. [TASK-MCP-007, RQ-MCP-012, ADR-MCP-001 (DEC-MCP-009)]
add_executable(xs56k_mcp_server_simulated mcp/SimulatedServer.cpp)
target_include_directories(xs56k_mcp_server_simulated PRIVATE akm mcp)
target_link_libraries(xs56k_mcp_server_simulated PRIVATE
    xs56k_mcp
    xs56k_akm_test_support
    xs56k::warnings)
add_test(NAME mcp_simulated_server_scripted_conversation
    COMMAND ${CMAKE_COMMAND}
        -DSERVER=$<TARGET_FILE:xs56k_mcp_server_simulated>
        -DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/mcp/conversations/simulated_session.jsonl
        -DEXPECTED_FILE=${CMAKE_CURRENT_SOURCE_DIR}/mcp/conversations/simulated_session.expected.jsonl
        -DEXPECT_LINES=161
        -P ${CMAKE_CURRENT_SOURCE_DIR}/RunServerConversation.cmake)
# The same server launched with --allow-disk, --allow-disk-refresh (the conversation sends a refresh) and --allow-front-panel, and a scripted conversation over the disk tools and the keys: browsing, loading, saving, what
# a save refuses and a read-only disk. To regenerate the expected output after a deliberate change, run
#   xs56k_mcp_server_simulated --allow-disk --allow-disk-refresh --allow-front-panel < mcp/conversations/simulated_disk_session.jsonl > mcp/conversations/simulated_disk_session.expected.jsonl
# [TASK-MCP-022, RQ-MCP-023, RQ-MCP-030, ADR-MCP-003 (DEC-MCP-015)]
add_test(NAME mcp_simulated_server_disk_conversation
    COMMAND ${CMAKE_COMMAND}
        -DSERVER=$<TARGET_FILE:xs56k_mcp_server_simulated>
        "-DSERVER_ARGS=--allow-disk;--allow-disk-refresh;--allow-front-panel"
        -DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/mcp/conversations/simulated_disk_session.jsonl
        -DEXPECTED_FILE=${CMAKE_CURRENT_SOURCE_DIR}/mcp/conversations/simulated_disk_session.expected.jsonl
        -DEXPECT_LINES=58
        -P ${CMAKE_CURRENT_SOURCE_DIR}/RunServerConversation.cmake)
# The shipped server must not link the simulated sampler or anything else of the test support: checked when CMake
# configures the tests. [RQ-MCP-012, ADR-MCP-001 (DEC-MCP-009)]
get_target_property(shipped_server_libraries xs56k_mcp_server LINK_LIBRARIES)
get_target_property(shipped_library_libraries xs56k_mcp LINK_LIBRARIES)
get_target_property(shipped_sampler_libraries xs56k_sampler LINK_LIBRARIES)
foreach(library IN LISTS shipped_server_libraries shipped_library_libraries shipped_sampler_libraries)
    if(library MATCHES "test_support")
        message(FATAL_ERROR "The shipped MCP server links the test support (${library}): it must not")
    endif()
endforeach()
# The gateway's library is shared with the editor application: it must not bring the MCP protocol or JSON with it.
# [RQ-APP-014, ADR-APP-001 (DEC-APP-002), TASK-APP-010]
foreach(library IN LISTS shipped_sampler_libraries)
    if(library MATCHES "nlohmann|xs56k_mcp")
        message(FATAL_ERROR "xs56k_sampler links ${library}: the gateway must depend on the AKM layer only")
    endif()
endforeach()
# The server never saves, loads, touches the disk or deletes everything, and only the gateway creates, renames or deletes
# a program, in memory. [RQ-MCP-008, RQ-MCP-014, ADR-MCP-001 (DEC-MCP-007), ADR-MCP-002 (DEC-MCP-010)]
add_test(NAME mcp_sources_call_no_destructive_primitive
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${PROJECT_SOURCE_DIR}/mcp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckNoDestructiveCalls.cmake)
# The same check must fail on a tool file that calls a delete primitive: the fixture holds one. [TASK-MCP-048, RQ-MCP-049, RQ-MCP-057]
add_test(NAME mcp_source_check_fails_on_a_delete_call_in_a_tool_file
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/mcp/forbidden_call_fixture
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckNoDestructiveCalls.cmake)
set_tests_properties(mcp_source_check_fails_on_a_delete_call_in_a_tool_file PROPERTIES
    PASS_REGULAR_EXPRESSION "A forbidden AKM call in the MCP server:.*akm::deleteSetList")
# ... and on a tool file that names a front-panel primitive. [TASK-MCP-053, RQ-MCP-054, RQ-MCP-057]
add_test(NAME mcp_source_check_fails_on_a_front_panel_call_in_a_tool_file
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/mcp/forbidden_key_fixture
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckNoDestructiveCalls.cmake)
set_tests_properties(mcp_source_check_fails_on_a_front_panel_call_in_a_tool_file PROPERTIES
    PASS_REGULAR_EXPRESSION "A forbidden AKM call in the MCP server:.*akm::holdKey")
# ... and on any file that ejects a disk, which the server never does. [TASK-MCP-054, RQ-MCP-055]
add_test(NAME mcp_source_check_fails_on_an_eject_call
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/mcp/forbidden_eject_fixture
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckNoDestructiveCalls.cmake)
set_tests_properties(mcp_source_check_fails_on_an_eject_call PROPERTIES
    PASS_REGULAR_EXPRESSION "A forbidden AKM call in the MCP server:.*akm::ejectDisk")

# The README of the MCP server names every tool the server lists and every option of its usage text: it is the reference for the person
# who uses the server. [TASK-MCP-036, RQ-MCP-043, ADR-MCP-004 (DEC-MCP-026)]
add_test(NAME mcp_readme_names_every_tool_and_option
    COMMAND ${CMAKE_COMMAND}
        -DSIMULATED_SERVER=$<TARGET_FILE:xs56k_mcp_server_simulated>
        -DREAL_SERVER=$<TARGET_FILE:xs56k_mcp_server>
        -DREADME=${PROJECT_SOURCE_DIR}/mcp/README.md
        -DROOT_README=${PROJECT_SOURCE_DIR}/../README.md
        -DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/mcp/conversations/list_tools.jsonl
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckReadmeCoversTools.cmake)

# The editor application's core: the controller and the model against the gateway over the simulated sampler, with a real worker
# thread and a UI dispatcher the test drains; no display, no JUCE. [RQ-APP-014, ADR-APP-001 (DEC-APP-010), TASK-APP-011]
add_executable(xs56k_app_tests
    app/BrowserCommandTests.cpp
    app/BrowserTests.cpp
    app/ConnectionTests.cpp
    app/CoreTests.cpp
    app/EditorLayoutTests.cpp
    app/EditTests.cpp
    app/MemoryCommandTests.cpp
    app/MemoryTests.cpp)
# The memory tests seed programs with the raw-frame helpers of the AKM and MCP tests (no MCP server). [TASK-APP-015]
target_include_directories(xs56k_app_tests PRIVATE app akm mcp)
target_link_libraries(xs56k_app_tests PRIVATE
    xs56k_app_core
    xs56k_akm_test_support
    Catch2::Catch2WithMain
    xs56k::warnings)
catch_discover_tests(xs56k_app_tests)
# No JUCE include anywhere in the core, its sources included; the check fails on a fixture that has one. [RQ-APP-014,
# ADR-APP-001 (DEC-APP-001, DEC-APP-010), TASK-APP-011]
add_test(NAME app_core_has_no_juce_include
    COMMAND ${CMAKE_COMMAND}
        -DHEADER_DIR=${PROJECT_SOURCE_DIR}/app/core
        -DWITH_SOURCES=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckNoJuceIncludes.cmake)
add_test(NAME app_core_check_fails_on_a_juce_include
    COMMAND ${CMAKE_COMMAND}
        -DHEADER_DIR=${CMAKE_CURRENT_SOURCE_DIR}/app/juce_include_fixture
        -DWITH_SOURCES=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckNoJuceIncludes.cmake)
set_tests_properties(app_core_check_fails_on_a_juce_include PROPERTIES
    PASS_REGULAR_EXPRESSION "juce_include_fixture/Fixture\\.cpp:")

# No raw visual literal in the editor application's sources: colours, font sizes, radii, strokes and spacings come from the token module;
# the check fails on a fixture that holds three. [RQ-APP-004, ADR-APP-002 (DEC-APP-014), TASK-APP-017]
add_test(NAME app_sources_have_no_visual_literal
    COMMAND ${CMAKE_COMMAND}
        "-DSOURCE_DIRS=${PROJECT_SOURCE_DIR}/app/src;${PROJECT_SOURCE_DIR}/app/core"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckNoVisualLiterals.cmake)
add_test(NAME app_visual_literal_check_fails_on_a_fixture
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIRS=${CMAKE_CURRENT_SOURCE_DIR}/app/visual_literal_fixture
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckNoVisualLiterals.cmake)
set_tests_properties(app_visual_literal_check_fails_on_a_fixture PROPERTIES
    PASS_REGULAR_EXPRESSION "Fixture\\.cpp:6:.*Fixture\\.cpp:8:.*Fixture\\.cpp:9:")

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
    # The table of the spec's messages with the AKM function and the MCP tool of each (documents/SYSEX-COVERAGE.md): it matches what the
    # sources say, and the script that writes it is itself tested. A new tool, function or item that the script cannot place fails the first.
    # [TASK-MCP-073, RQ-MCP-062, RQ-MCP-063, RQ-MCP-065, ADR-MCP-006 (DEC-MCP-039)]
    add_test(NAME sysex_coverage_table_is_up_to_date
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/generate_sysex_coverage.py --check)
    add_test(NAME sysex_coverage_script_tests
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tools -p "test_generate_sysex_coverage.py")
    # What each tool really sends, from the simulated server run on the scripted conversations (--trace-messages), against the table: a
    # tool that sends a message neither its row nor the methods it calls give fails. [TASK-MCP-075, RQ-MCP-066, ADR-MCP-006 (DEC-MCP-040)]
    add_test(NAME sysex_coverage_tools_match_the_simulated_sampler
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/generate_sysex_coverage.py --check-tools
            --server $<TARGET_FILE:xs56k_mcp_server_simulated>)
    # The design system's token module and the value tables of its document match the token file, and the generator is tested.
    # [TASK-APP-007, RQ-APP-002, RQ-APP-005, ADR-APP-002 (DEC-APP-011, DEC-APP-012)]
    add_test(NAME app_design_tokens_are_up_to_date
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/generate_design.py --check)
    add_test(NAME app_design_tokens_script_tests
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tools -p "test_generate_design.py")
    # The committed SVG mockups match the tokens, the layouts and the screens, and the generator is tested.
    # [TASK-APP-008, RQ-APP-077, RQ-APP-078, ADR-APP-003 (DEC-APP-020)]
    add_test(NAME app_mockups_are_up_to_date
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/generate_mockups.py --check)
    add_test(NAME app_mockups_script_tests
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tools -p "test_generate_mockups.py")
    # The mutation check tool (RQ-BLD-015): configures and builds the small fixture project of
    # tools/mutate_fixture/ several times over, so it is markedly slower than the catalogue's own
    # tests above. [TASK-BLD-012]
    add_test(NAME bld_mutate_tool_script_tests
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tools -p "test_mutate.py")
    # The SonarCloud triage tool (it sorts the issues into test code and production code and accepts the first): tested against a
    # replaced `sonar` CLI, never against SonarCloud. [TASK-QLT-001, RQ-QLT-006, RQ-QLT-008, ADR-QLT-001 (DEC-QLT-003)]
    add_test(NAME qlt_sonar_triage_script_tests
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tools -p "test_sonar_triage.py")
    # The new-source-file tool writes only a C++ file under juce/. [TASK-QLT-021, RQ-QLT-001]
    add_test(NAME qlt_new_source_file_script_tests
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tools -p "test_new_source_file.py")
    # The clang-tidy runner: its selection of files and its filter of the findings, against a replaced clang-tidy. [TASK-QLT-024, RQ-QLT-013]
    add_test(NAME qlt_tidy_script_tests
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tools -p "test_tidy.py")
else()
    message(STATUS "Python 3 not found: the item catalogue, coverage table, design tokens, mockups and mutation-tool checks of juce/tests/tools are not registered")
endif()
