# Fails when a source or header under SOURCE_DIR names an AKM primitive or item that creates, renames, deletes, saves,
# loads, clears, ejects or formats something, except in the units that may:
#   - SamplerGateway.cpp: the program structure primitives (create a program with keygroups, rename the current
#     program, delete the current program, delete a keygroup, rename or delete the current sample, create, rename or delete a multi, remove a part's program, delete ALL programs, samples or multis and clear the sampler's memory, with their typed confirmations), all in memory (ADR-MCP-002 DEC-MCP-010,
#     ADR-MCP-004 DEC-MCP-023, ADR-MCP-005 DEC-MCP-029);
#   - SamplerGatewayDisk.cpp: the disk primitives of loading and saving, of creating a folder, of renaming and deleting a file or a
#     folder (the deletions with their typed confirmations), and their types (ADR-MCP-003 DEC-MCP-019, DEC-MCP-021, ADR-MCP-004
#     DEC-MCP-023);
#   - SamplerGatewayLists.cpp: the rename and delete primitives of the current song file, of a set list and of the current scenelist
#     (ADR-MCP-005 DEC-MCP-031, ADR-MCP-004 DEC-MCP-023);
#   - SamplerGatewayKeys.cpp: the front-panel primitives of section 20 (hold, release and press a key, the data wheel, an ASCII key), which carry
#     no destructive verb but can answer "ENT" to a delete or save screen; no other file may name them (ADR-MCP-005 DEC-MCP-032).
#
# Usage: cmake -DSOURCE_DIR=<dir> -P CheckNoDestructiveCalls.cmake
#
# The words are looked for as the names of the AKM layer's own functions (`akm::createProgram`, `deleteAllPrograms`,
# `deleteFile`, ...) and of its items (`ItemId::ProgramCreate`, `ItemId::DiskSaveMemoryItem`, ...). Delete ALL is allowed in the
# gateway's memory unit for programs, samples and multis only, and so is Clear Sampler Memory; the eject and format primitives of the disk are
# never allowed anywhere.
# [RQ-MCP-008, RQ-MCP-014, RQ-MCP-028, ADR-MCP-001 (DEC-MCP-007), ADR-MCP-002 (DEC-MCP-010), ADR-MCP-003 (DEC-MCP-019)]

# A script run with -P sets no policy of its own: before CMake 4 the operator IN_LIST (used below) is then read as a plain
# string on the CI runners that still have CMake 3 (CMP0057), and the check fails with an error.
cmake_policy(SET CMP0057 NEW)

if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is not set")
endif()
if(NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "SOURCE_DIR is not a directory: ${SOURCE_DIR}")
endif()

file(GLOB_RECURSE sources "${SOURCE_DIR}/*.hpp" "${SOURCE_DIR}/*.cpp")
if(NOT sources)
    message(FATAL_ERROR "No source found under ${SOURCE_DIR}: the check would pass vacuously")
endif()

# An ItemId or a function of namespace akm whose name carries one of the verbs.
set(verbs "[Cc]reate|[Dd]elete|[Rr]ename|[Ss]ave|[Ll]oad|[Cc]lear|[Ee]ject|[Ff]ormat")
set(pattern "(ItemId::|akm::)[A-Za-z]*(${verbs})")
# What each allowed file may call. A file that is not listed may call none of the names that carry a verb.
set(allowed_files "SamplerGateway.cpp" "SamplerGatewayDisk.cpp" "SamplerGatewayLists.cpp")
set(allowed_calls_SamplerGateway.cpp "akm::createProgramWithKeygroups|akm::renameCurrentProgram|akm::deleteCurrentProgram|akm::deleteKeygroupFromProgram|akm::renameCurrentSample|akm::deleteCurrentSample|akm::createMulti|akm::renameCurrentMulti|akm::deleteCurrentMulti|akm::deleteMultiPart|akm::deleteAllPrograms|akm::deleteAllSamples|akm::deleteAllMultis|akm::ConfirmDeleteAllPrograms|akm::ConfirmDeleteAllSamples|akm::ConfirmDeleteAllMultis|akm::clearSamplerMemory|akm::ConfirmClearSamplerMemory")
set(allowed_calls_SamplerGatewayDisk.cpp "akm::renameFile|akm::renameFolder|akm::deleteFile|akm::deleteSubFolder|akm::ConfirmDeleteFile|akm::ConfirmDeleteSubFolder|akm::createFolder|akm::loadFileWithDependents|akm::loadFile|akm::loadFolder|akm::saveMemoryItem|akm::saveAllMemoryItems|akm::SaveableMemoryType|akm::SampleLoadOption")
set(allowed_calls_SamplerGatewayLists.cpp "akm::renameCurrentSong|akm::renameSetList|akm::renameCurrentSceneList|akm::deleteCurrentSong|akm::deleteSetList|akm::deleteCurrentSceneList")
set(offenders "")
# The front-panel primitives (section 20) and their types: only the keys unit may name them.
set(key_pattern "(ItemId::FrontPanel|akm::(holdKey|releaseKey|pressKey|moveDataWheel|sendAsciiKey|FrontPanelKey|DataWheelDirection|frontPanelKeyFromCode|KeyPress))")
set(key_files "SamplerGatewayKeys.cpp")
foreach(source IN LISTS sources)
    get_filename_component(source_name "${source}" NAME)
    file(STRINGS "${source}" key_lines REGEX "${key_pattern}")
    if(key_lines AND NOT source_name IN_LIST key_files)
        list(APPEND offenders "${source}: ${key_lines}")
    endif()
    file(STRINGS "${source}" lines REGEX "${pattern}")
    if(source_name IN_LIST allowed_files)
        set(remaining "")
        foreach(line IN LISTS lines)
            string(REGEX REPLACE "(${allowed_calls_${source_name}})" "" stripped "${line}")
            if(stripped MATCHES "${pattern}")
                list(APPEND remaining "${line}")
            endif()
        endforeach()
        set(lines "${remaining}")
    endif()
    if(lines)
        list(APPEND offenders "${source}: ${lines}")
    endif()
endforeach()

if(offenders)
    list(JOIN offenders "\n  " report)
    message(FATAL_ERROR "A forbidden AKM call in the MCP server:\n  ${report}")
endif()

list(LENGTH sources source_count)
message(STATUS "${source_count} file(s) under ${SOURCE_DIR}: no create, delete, rename, save, load, clear, eject, format or front-panel call beyond the primitives of the gateway's units")
