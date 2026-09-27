# xs56k_akm_probe: the first-contact probe, a console program the owner runs against the real S5000. Test
# tooling, built only with BUILD_TESTS and never shipped; its logic is in xs56k_akm_test_support, where CI
# runs it against the simulated sampler. [TASK-AKM-012, RQ-AKM-017, RQ-AKM-044]

add_executable(xs56k_akm_probe main.cpp)
target_link_libraries(xs56k_akm_probe PRIVATE
    xs56k_akm_test_support
    xs56k_midi_juce
    xs56k::warnings)
