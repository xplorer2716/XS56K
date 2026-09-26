# xs56k_midi: backend-agnostic MIDI abstraction [RQ-MID-040, ADR-JUC-004]
# xs56k_midi_juce: JUCE adapter, kept separate so strict warnings never apply
# to JUCE module sources compiled into the target. [RQ-BLD-003, ADR-JUC-004]

add_library(xs56k_midi STATIC
    src/LibraryInfo.cpp
    src/MidiMessage.cpp
    src/MockMidiBackend.cpp
    src/SysexStreamIterator.cpp)
target_include_directories(xs56k_midi PUBLIC include)
target_link_libraries(xs56k_midi PRIVATE xs56k::warnings)

add_library(xs56k_midi_juce STATIC
    src/juce/JuceBackendInfo.cpp
    src/juce/JuceMidiBackend.cpp)
target_compile_definitions(xs56k_midi_juce PRIVATE
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0)
target_link_libraries(xs56k_midi_juce
    PUBLIC xs56k_midi
    PRIVATE juce::juce_audio_devices)
