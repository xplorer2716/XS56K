# Build-only checks: a scenario written against `MidiBackend&` must compile against the real backend as
# well as against the simulated one. The library is never linked into anything and needs no hardware.
# [RQ-AKM-016, RQ-AKM-019, ADR-AKM-001 (DEC-AKM-008)]

add_library(xs56k_akm_scenarios_on_juce STATIC
    EchoScenarioOnJuce.cpp)
target_link_libraries(xs56k_akm_scenarios_on_juce PRIVATE
    xs56k_akm_test_support
    xs56k_midi_juce
    xs56k::warnings)
