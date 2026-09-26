# XS56K JUCE application (GUI). Built only with -DBUILD_APP=ON.
#
# MINIMAL PLACEHOLDER, not the XS56K editor UI: it exists so BUILD_APP=ON
# configures and links a real, runnable executable, unblocking the
# versioning/deployment-matrix/cut-deployment plumbing of TASK-BLD-005/006/007
# — none of which can be exercised against an application target that does not
# exist. The actual editor UI (model/controller/settings layers, ported from
# xplorer2716/XplorerEditor per AGENTS.md, or designed fresh) is a separate,
# undesigned effort — see ADR-BLD-003's "Proposed" status.
# [RQ-BLD-002, RQ-BLD-007, ADR-BLD-003, TASK-BLD-005]
juce_add_gui_app(XS56KApp
    PRODUCT_NAME "XS56K"
    COMPANY_NAME "XS56K"
    # NUMERIC form only: this feeds Windows FILEVERSION (four 16-bit fields)
    # and CFBundleVersion, neither of which can carry a "-canary"/"-preprod"
    # suffix. The FULL form (with the suffix) is what the application itself
    # reports — see VERSION_FULL_STRING below. [RQ-BLD-009, RQ-BLD-020]
    VERSION "${VERSION_NUMERIC}")

target_sources(XS56KApp PRIVATE src/Main.cpp)

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
    xs56k_midi_juce
    xs56k_framework
    juce::juce_gui_extra
    juce::juce_audio_devices
    xs56k::warnings)
