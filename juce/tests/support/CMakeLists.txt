# xs56k_akm_test_support: the simulated sampler (a MidiBackend modelled on the spec), the ScenarioDriver
# and the scenarios shared by the CI suite and the real-sampler suite. Test code, built only with
# BUILD_TESTS. [RQ-AKM-016, RQ-AKM-019, ADR-AKM-001 (DEC-AKM-008)]

add_library(xs56k_akm_test_support STATIC
    src/ClockArithmetic.cpp
    src/EchoScenario.cpp
    src/FirstContactProbe.cpp
    src/FrontPanelRemote.cpp
    src/KeygroupParameterCases.cpp
    src/MultiPartParameterCases.cpp
    src/ProgramParameterCases.cpp
    src/RealSamplerSuite.cpp
    src/SampleParameterCases.cpp
    src/ScenarioDriver.cpp
    src/SessionSmokeTest.cpp
    src/SimulatedMidiBackend.cpp
    src/SimulatedSampler.cpp
    src/WireFormat.cpp
    src/WireLog.cpp
    src/ZoneParameterCases.cpp)
# RealSamplerSuite.cpp holds every opt-in check of the real-sampler suite with the lambdas and templates they use: its
# object file passes MSVC's default limit of 65535 sections (C1128), which /bigobj lifts. [RQ-AKM-102]
if(MSVC)
    set_source_files_properties(src/RealSamplerSuite.cpp PROPERTIES COMPILE_OPTIONS /bigobj)
endif()
target_include_directories(xs56k_akm_test_support PUBLIC include)
target_link_libraries(xs56k_akm_test_support PUBLIC xs56k_akm PRIVATE xs56k::warnings)
