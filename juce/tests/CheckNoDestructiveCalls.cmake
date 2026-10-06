# Fails when a source or header under SOURCE_DIR names an AKM primitive or item that creates, renames, deletes, saves,
# loads, clears, ejects or formats something, except in the two units that may:
#   - SamplerGateway.cpp: the program structure primitives (create a program with keygroups, rename the current
#     program, delete the current program, delete a keygroup, rename or delete the current sample), all in memory (ADR-MCP-002 DEC-MCP-010, ADR-MCP-004 DEC-MCP-023);
#   - SamplerGatewayDisk.cpp: the disk primitives of loading and saving, of creating a folder, and their types (ADR-MCP-003
#     DEC-MCP-019, DEC-MCP-021).
#
# Usage: cmake -DSOURCE_DIR=<dir> -P CheckNoDestructiveCalls.cmake
#
# The words are looked for as the names of the AKM layer's own functions (`akm::createProgram`, `deleteAllPrograms`,
# `deleteFile`, ...) and of its items (`ItemId::ProgramCreate`, `ItemId::DiskSaveMemoryItem`, ...). Delete ALL, Clear Sampler
# Memory and the delete, rename, eject and format primitives of the disk are never allowed anywhere.
# [RQ-MCP-008, RQ-MCP-014, RQ-MCP-028, ADR-MCP-001 (DEC-MCP-007), ADR-MCP-002 (DEC-MCP-010), ADR-MCP-003 (DEC-MCP-019)]

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
set(allowed_files "SamplerGateway.cpp" "SamplerGatewayDisk.cpp")
set(allowed_calls_SamplerGateway.cpp "akm::createProgramWithKeygroups|akm::renameCurrentProgram|akm::deleteCurrentProgram|akm::deleteKeygroupFromProgram|akm::renameCurrentSample|akm::deleteCurrentSample")
set(allowed_calls_SamplerGatewayDisk.cpp "akm::createFolder|akm::loadFileWithDependents|akm::loadFile|akm::loadFolder|akm::saveMemoryItem|akm::saveAllMemoryItems|akm::SaveableMemoryType|akm::SampleLoadOption")
set(offenders "")
foreach(source IN LISTS sources)
    file(STRINGS "${source}" lines REGEX "${pattern}")
    get_filename_component(source_name "${source}" NAME)
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
message(STATUS "${source_count} file(s) under ${SOURCE_DIR}: no create, delete, rename, save, load, clear, eject or format call beyond the program structure primitives of the gateway and the load and save primitives of its disk unit")
