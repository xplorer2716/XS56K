# xs56k_akm: AKAI S5000/S6000 SysEx layer (codec, session, items). Depends on the
# abstract MIDI interface (xs56k_midi) only and exposes no JUCE type in its public
# headers, so the same scenario runs on the in-memory and on the real backend.
# [RQ-AKM-019, RQ-BLD-003, ADR-AKM-001 (DEC-AKM-001)]

find_package(Threads REQUIRED)  # the real scheduler and executor use std::thread

add_library(xs56k_akm STATIC
    src/ByteReader.cpp
    src/ByteWriter.cpp
    src/Checksum.cpp
    src/Command.cpp
    src/Confirmation.cpp
    src/LibraryInfo.cpp
    src/ManualExecutor.cpp
    src/ManualScheduler.cpp
    src/RealScheduler.cpp
    src/SamplerError.cpp
    src/ThreadExecutor.cpp
    src/TimerQueue.cpp)
target_include_directories(xs56k_akm PUBLIC include)
target_link_libraries(xs56k_akm PUBLIC xs56k_midi Threads::Threads PRIVATE xs56k::warnings)
