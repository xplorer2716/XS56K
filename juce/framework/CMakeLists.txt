# xs56k_framework layer library: port of MidiApp.MidiController core [RQ-FMW, ADR-JUC-002]
add_library(xs56k_framework STATIC
    src/LibraryInfo.cpp
    src/AbstractParameter.cpp
    src/OrderedParameterMap.cpp
    src/AbstractTone.cpp
    src/DualDictionary.cpp
    src/AbstractController.cpp
    src/AbstractControllerDevices.cpp
    src/AbstractControllerWorker.cpp
    src/Logger.cpp
    src/SingleInstanceGuard.cpp
    src/FileUtils.cpp)
target_include_directories(xs56k_framework PUBLIC include)
target_link_libraries(xs56k_framework PUBLIC xs56k_midi PRIVATE xs56k::warnings)
