# Fails when a header under HEADER_DIR includes a JUCE header; with WITH_SOURCES=ON, a .cpp
# under it too.
#
# Usage: cmake -DHEADER_DIR=<dir> [-DWITH_SOURCES=ON] -P CheckNoJuceIncludes.cmake
#
# Guards the independence of the AKM layer from the JUCE backend: no JUCE type may
# appear in its public headers, so the same scenario runs unchanged on the in-memory
# backend and on the real one. [RQ-AKM-019, ADR-AKM-001 (DEC-AKM-001)]
# The editor application's core is checked whole, sources included: it is built and
# tested without JUCE. [RQ-APP-014, ADR-APP-001 (DEC-APP-010), TASK-APP-011]
#
# Matches an #include whose path starts with "juce" (juce_core/..., juce/...,
# JuceHeader.h), in either quote style, with any whitespace around the '#'.

if(NOT DEFINED HEADER_DIR)
    message(FATAL_ERROR "HEADER_DIR is not set")
endif()
if(NOT IS_DIRECTORY "${HEADER_DIR}")
    message(FATAL_ERROR "HEADER_DIR is not a directory: ${HEADER_DIR}")
endif()

set(patterns "${HEADER_DIR}/*.hpp" "${HEADER_DIR}/*.h" "${HEADER_DIR}/*.hxx")
if(WITH_SOURCES)
    list(APPEND patterns "${HEADER_DIR}/*.cpp")
endif()
file(GLOB_RECURSE headers ${patterns})
if(NOT headers)
    message(FATAL_ERROR "No header found under ${HEADER_DIR}: the check would pass vacuously")
endif()

set(juce_include_pattern "^[ \t]*#[ \t]*include[ \t]*[<\"][ \t]*[Jj][Uu][Cc][Ee]")
set(offenders "")
foreach(header IN LISTS headers)
    file(STRINGS "${header}" include_lines REGEX "${juce_include_pattern}")
    if(include_lines)
        list(APPEND offenders "${header}: ${include_lines}")
    endif()
endforeach()

if(offenders)
    list(JOIN offenders "\n  " report)
    message(FATAL_ERROR "JUCE include in ${report}")
endif()

list(LENGTH headers header_count)
message(STATUS "${header_count} header(s) under ${HEADER_DIR}: no JUCE include")
