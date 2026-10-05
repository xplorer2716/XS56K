# Fails when a source or header under SOURCE_DIR names an AKM primitive or item that creates, renames, deletes, saves,
# loads, clears or ejects something, except the three program structure primitives that the sampler gateway alone may
# call (create a program with keygroups, rename the current program, delete the current program), all in memory.
#
# Usage: cmake -DSOURCE_DIR=<dir> -P CheckNoDestructiveCalls.cmake
#
# The words are looked for as the names of the AKM layer's own functions (`akm::createProgram`, `deleteAllPrograms`,
# ...) and of its items (`ItemId::ProgramCreate`, `ItemId::DiskSaveMemoryItem`, ...). The disk, Delete ALL and Clear
# Sampler Memory are never allowed anywhere; the three allowed primitives are allowed in the gateway's one source file
# and nowhere else. [RQ-MCP-008, RQ-MCP-014, ADR-MCP-001 (DEC-MCP-007), ADR-MCP-002 (DEC-MCP-010)]

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
# What the one allowed file may call: the program structure primitives of DEC-MCP-010.
set(allowed_file "SamplerGateway.cpp")
set(allowed_calls "akm::createProgramWithKeygroups|akm::renameCurrentProgram|akm::deleteCurrentProgram")
set(offenders "")
foreach(source IN LISTS sources)
    file(STRINGS "${source}" lines REGEX "${pattern}")
    get_filename_component(source_name "${source}" NAME)
    if(source_name STREQUAL allowed_file)
        set(remaining "")
        foreach(line IN LISTS lines)
            string(REGEX REPLACE "(${allowed_calls})" "" stripped "${line}")
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
    message(FATAL_ERROR "A destructive AKM call in the MCP server:\n  ${report}")
endif()

list(LENGTH sources source_count)
message(STATUS "${source_count} file(s) under ${SOURCE_DIR}: no create, delete, rename, save, load, clear, eject or format call beyond the three program structure primitives of the gateway")
