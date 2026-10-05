# Fails when a source or header under SOURCE_DIR names an AKM primitive or item that creates, renames, deletes, saves,
# loads, clears or ejects something.
#
# Usage: cmake -DSOURCE_DIR=<dir> -P CheckNoDestructiveCalls.cmake
#
# The MCP server edits the program in memory and nothing else: it holds no call to the primitives that change what the
# sampler stores (programs, keygroups, multis, samples, disks, the whole memory). The words are looked for as the
# names of the AKM layer's own functions (`akm::createProgram`, `deleteAllPrograms`, ...) and of its items
# (`ItemId::ProgramCreate`, `ItemId::DiskSaveMemoryItem`, ...). [RQ-MCP-008, ADR-MCP-001 (DEC-MCP-007)]

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
set(offenders "")
foreach(source IN LISTS sources)
    file(STRINGS "${source}" lines REGEX "${pattern}")
    if(lines)
        list(APPEND offenders "${source}: ${lines}")
    endif()
endforeach()

if(offenders)
    list(JOIN offenders "\n  " report)
    message(FATAL_ERROR "A destructive AKM call in the MCP server:\n  ${report}")
endif()

list(LENGTH sources source_count)
message(STATUS "${source_count} file(s) under ${SOURCE_DIR}: no create, delete, rename, save, load, clear, eject or format call")
