# xs56k_akm_test_support: the simulated sampler (a MidiBackend modelled on the spec), the ScenarioDriver
# and the scenarios shared by the CI suite and the real-sampler suite. Test code, built only with
# BUILD_TESTS. [RQ-AKM-016, RQ-AKM-019, ADR-AKM-001 (DEC-AKM-008)]

add_library(xs56k_akm_test_support STATIC
    src/EchoScenario.cpp
    src/ScenarioDriver.cpp
    src/SimulatedMidiBackend.cpp
    src/SimulatedSampler.cpp)
target_include_directories(xs56k_akm_test_support PUBLIC include)
target_link_libraries(xs56k_akm_test_support PUBLIC xs56k_akm PRIVATE xs56k::warnings)
