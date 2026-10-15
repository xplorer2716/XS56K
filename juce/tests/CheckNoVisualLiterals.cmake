# Fails when a source of the editor application holds a raw visual literal: a colour written as a number or taken from juce::Colours, or a
# number written in a call that sets a font size, a corner radius, a stroke width, a spacing or a size. Only the generated token module
# (DesignTokens.generated.hpp) may hold such values. Names the file and the line.
#
# Usage: cmake -DSOURCE_DIRS=<dir>[;<dir>...] -P CheckNoVisualLiterals.cmake
#
# A number starting with 0 is let through (`reduced (0)`, `Colour (0x…)` is caught by its own pattern), so that "nothing" stays writable.
# [RQ-APP-004, ADR-APP-002 (DEC-APP-014), TASK-APP-017]

# A script run with -P sets no policy of its own: before CMake 4, the list commands drop the empty elements (CMP0007), so the blank lines
# would vanish from the lines of a file and the line numbers given would be wrong; the CI runners still have CMake 3.
cmake_policy(SET CMP0007 NEW)

if(NOT DEFINED SOURCE_DIRS)
    message(FATAL_ERROR "SOURCE_DIRS is not set")
endif()

set(sources "")
foreach(dir IN LISTS SOURCE_DIRS)
    if(NOT IS_DIRECTORY "${dir}")
        message(FATAL_ERROR "Not a directory: ${dir}")
    endif()
    file(GLOB_RECURSE found "${dir}/*.cpp" "${dir}/*.hpp" "${dir}/*.h")
    list(APPEND sources ${found})
endforeach()
list(FILTER sources EXCLUDE REGEX "\\.generated\\.hpp$")
if(NOT sources)
    message(FATAL_ERROR "No source found under ${SOURCE_DIRS}: the check would pass vacuously")
endif()

set(ws "[ \t]*")
set(number "[1-9][0-9.]*f?")
set(patterns
    # colours
    "Colour${ws}\\(${ws}[0-9]"
    "Colour::from(RGB|RGBA|FloatRGBA|HSV|HSL|String)"
    "Colours::"
    # font sizes, spacing, sizes and stroke widths given as a number
    "(withHeight|setHeight|withPointHeight|FontOptions|reduced|expanded|removeFromTop|removeFromBottom|removeFromLeft|removeFromRight|withTrimmedTop|withTrimmedBottom|withTrimmedLeft|withTrimmedRight|withSizeKeepingCentre|PathStrokeType|setSize|centreWithSize|setMinimumSize|setResizeLimits)${ws}\\(${ws}${number}"
    # a corner radius or a line thickness as the last argument
    "(RoundedRectangle|drawRect|drawLine|drawEllipse)${ws}\\(.*,${ws}${number}${ws}\\)")

set(offenders "")
foreach(source IN LISTS sources)
    # One list element per line, blank lines kept so that the line numbers are right: the characters a CMake list treats specially
    # (";" splits, "[" and "]" group) are replaced first, none of them being part of a pattern.
    file(READ "${source}" content)
    string(REPLACE ";" "," content "${content}")
    string(REPLACE "[" "(" content "${content}")
    string(REPLACE "]" ")" content "${content}")
    string(REPLACE "\r" "" content "${content}")
    string(REPLACE "\n" ";" lines "${content}")
    list(LENGTH lines line_count)
    if(line_count EQUAL 0)
        continue()
    endif()
    math(EXPR last "${line_count} - 1")
    foreach(index RANGE ${last})
        list(GET lines ${index} line)
        foreach(pattern IN LISTS patterns)
            if(line MATCHES "${pattern}")
                math(EXPR line_number "${index} + 1")
                string(STRIP "${line}" shown)
                list(APPEND offenders "${source}:${line_number}: ${shown}")
                break()
            endif()
        endforeach()
    endforeach()
endforeach()

if(offenders)
    list(JOIN offenders "\n  " report)
    message(FATAL_ERROR "Raw visual literal outside the token module:\n  ${report}")
endif()

list(LENGTH sources source_count)
message(STATUS "${source_count} source(s) under ${SOURCE_DIRS}: no raw visual literal")
