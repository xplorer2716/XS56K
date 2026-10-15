# XS56K JUCE application (GUI). Built only with -DBUILD_APP=ON.
#
# The editor's views and the JUCE adapters of its core: the model and the controller are in xs56k_app_core (juce/app/core, no JUCE),
# built and tested with the libraries; this target draws them (ADR-APP-001 DEC-APP-001, DEC-APP-010). It reaches the sampler through the
# core and xs56k_sampler, never through xs56k_mcp. Every visual value comes from the design tokens (ADR-APP-002).
# [RQ-BLD-002, RQ-BLD-007, RQ-APP-001, RQ-APP-006, ADR-BLD-003, TASK-BLD-005, TASK-APP-017]
juce_add_gui_app(XS56KApp
    PRODUCT_NAME "XS56K"
    COMPANY_NAME "XS56K"
    # NUMERIC form only: this feeds Windows FILEVERSION (four 16-bit fields)
    # and CFBundleVersion, neither of which can carry a "-canary"/"-preprod"
    # suffix. The FULL form (with the suffix) is what the application itself
    # reports — see VERSION_FULL_STRING below. [RQ-BLD-009, RQ-BLD-020]
    VERSION "${VERSION_NUMERIC}")

target_sources(XS56KApp PRIVATE
    src/BrowserView.cpp
    src/CutLabel.cpp
    src/EditView.cpp
    src/EnvelopeGraph.cpp
    src/ItemList.cpp
    src/JuceAdapters.cpp
    src/LookAndFeel.cpp
    src/Main.cpp
    src/MainComponent.cpp
    src/MemoryView.cpp
    src/MidiSetupDialog.cpp
    src/ParameterCell.cpp
    src/StatusBar.cpp
    src/TextPrompt.cpp)

# Inter, regular and medium, embedded; its licence (SIL OFL 1.1) is copied next to the executable. [RQ-APP-001, ADR-APP-002 (DEC-APP-015)]
set(INTER_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts/LICENSE.txt)
juce_add_binary_data(XS56KAppFonts
    NAMESPACE BinaryData
    SOURCES
        resources/fonts/Inter-Regular.woff2
        resources/fonts/Inter-Medium.woff2)
add_custom_command(TARGET XS56KApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${INTER_LICENSE} $<TARGET_FILE_DIR:XS56KApp>/Inter-LICENSE.txt)

target_compile_definitions(XS56KApp PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:XS56KApp,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:XS56KApp,JUCE_VERSION>"
    # What the application itself reports (Main.cpp's getApplicationVersion()):
    # the FULL form, so a running instance — or a bug report quoting it —
    # states which stream it came from, which the numeric form above cannot.
    # [RQ-BLD-009]
    VERSION_FULL_STRING="${VERSION_FULL}")

target_link_libraries(XS56KApp PRIVATE
    xs56k_app_core
    xs56k_midi_juce
    XS56KAppFonts
    juce::juce_gui_extra
    juce::juce_audio_devices
    xs56k::warnings)
