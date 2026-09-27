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
    akm/CommandEncodingTests.cpp
    akm/CommandResultTests.cpp
    akm/ConfirmationDecodingTests.cpp
    akm/ExecutorTests.cpp
    akm/FirstContactProbeTests.cpp
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
    akm/MidiTuneParametersTests.cpp
    akm/OutputParametersTests.cpp
    akm/PitchBendParametersTests.cpp
    akm/ProgramDeleteAllGuardTests.cpp
    akm/ProgramGeneralInfoTests.cpp
    akm/ProgramPrimitivesTests.cpp
    akm/ProgramStructureTests.cpp
    akm/RealSamplerSuiteTests.cpp
    akm/SamplerErrorTests.cpp
    akm/ScenarioTests.cpp
    akm/SchedulerTests.cpp
    akm/SessionCloseTests.cpp
    akm/SessionOpenTests.cpp
    akm/SessionSmokeTests.cpp
    akm/SessionTests.cpp
    akm/SessionThreadingTests.cpp
    akm/SimulatedSamplerTests.cpp
    akm/SysExConfigTests.cpp
    akm/SystemVersionTests.cpp
    akm/ValueFormatTests.cpp
    akm/WireLogTests.cpp)
target_link_libraries(xs56k_akm_tests PRIVATE
    xs56k_akm
    xs56k_akm_test_support
    Catch2::Catch2WithMain
    xs56k::warnings)
catch_discover_tests(xs56k_akm_tests)

# No JUCE include in the public headers of the AKM layer. [RQ-AKM-019]
add_test(NAME akm_public_headers_have_no_juce_include
    COMMAND ${CMAKE_COMMAND}
        -DHEADER_DIR=${PROJECT_SOURCE_DIR}/akm/include
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckNoJuceIncludes.cmake)

# The item catalogue: the checked-in table matches its data file, the data file matches the spec's item list,
# and the script that says so is itself tested. Python only runs here, in the test suite, never during the
# build; without it the checks are skipped and said to be. [TASK-AKM-008, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_FOUND)
    add_test(NAME akm_item_table_is_up_to_date
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/generate_akm_items.py --check)
    add_test(NAME akm_item_catalogue_matches_the_spec
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/generate_akm_items.py --coverage)
    add_test(NAME akm_item_catalogue_script_tests
        COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tools -p "test_*.py")
else()
    message(STATUS "Python 3 not found: the item catalogue checks of juce/tests/tools are not registered")
endif()
