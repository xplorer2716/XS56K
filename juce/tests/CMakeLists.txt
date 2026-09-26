# Test suites, built only with BUILD_TESTS. [RQ-BLD-002, RQ-AKM-016, ADR-AKM-001 (DEC-AKM-008)]
#
# Catch2 is fetched and CTest/Catch are included by the root juce/CMakeLists.txt.
# Test sources mirror the tree of the library they exercise (akm/ for xs56k_akm).

add_subdirectory(support)
add_subdirectory(compile_checks)

add_executable(xs56k_akm_tests
    akm/ChecksumTests.cpp
    akm/CommandEncodingTests.cpp
    akm/ConfirmationDecodingTests.cpp
    akm/ExecutorTests.cpp
    akm/LibraryInfoTests.cpp
    akm/SamplerErrorTests.cpp
    akm/ScenarioTests.cpp
    akm/SchedulerTests.cpp
    akm/SimulatedSamplerTests.cpp
    akm/ValueFormatTests.cpp)
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
