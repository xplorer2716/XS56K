# Fails when a header under HEADER_DIR includes a JUCE header.
#
# Usage: cmake -DHEADER_DIR=<dir> -P CheckNoJuceIncludes.cmake
#
# Guards the independence of the AKM layer from the JUCE backend: no JUCE type may
# appear in its public headers, so the same scenario runs unchanged on the in-memory
# backend and on the real one. [RQ-AKM-019, ADR-AKM-001 (DEC-AKM-001)]
#
# Matches an #include whose path starts with "juce" (juce_core/..., juce/...,
# JuceHeader.h), in either quote style, with any whitespace around the '#'.

if(NOT DEFINED HEADER_DIR)
    message(FATAL_ERROR "HEADER_DIR is not set")
endif()
if(NOT IS_DIRECTORY "${HEADER_DIR}")
    message(FATAL_ERROR "HEADER_DIR is not a directory: ${HEADER_DIR}")
endif()

file(GLOB_RECURSE headers "${HEADER_DIR}/*.hpp" "${HEADER_DIR}/*.h" "${HEADER_DIR}/*.hxx")
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
    message(FATAL_ERROR "JUCE include in a public header of the AKM layer:\n  ${report}")
endif()

list(LENGTH headers header_count)
message(STATUS "${header_count} header(s) under ${HEADER_DIR}: no JUCE include")
