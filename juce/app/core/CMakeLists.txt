# xs56k_app_core: the model and the controller of the editor application, without JUCE, so that the headless workflows build and test it
# whatever BUILD_APP says. It reaches the sampler only through xs56k_sampler (the gateway and the catalogues), never through xs56k_mcp.
# The views and the JUCE adapters of its interfaces are in XS56KApp (juce/app). [RQ-APP-014, ADR-APP-001 (DEC-APP-001, DEC-APP-002,
# DEC-APP-010), TASK-APP-011]
add_library(xs56k_app_core STATIC
    src/Browser.cpp
    src/Controller.cpp
    src/FileBrowser.cpp
    src/Memory.cpp
    src/MemoryController.cpp
    src/MidiSetup.cpp
    src/Model.cpp
    src/SamplerWorker.cpp)
target_include_directories(xs56k_app_core PUBLIC include)
target_link_libraries(xs56k_app_core PUBLIC xs56k_sampler PRIVATE xs56k::warnings)
