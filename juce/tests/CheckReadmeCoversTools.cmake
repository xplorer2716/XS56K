# Fails when the README of the MCP server leaves out a tool the server lists or an option of its usage text: the README is the
# reference for the person who installs and uses the server, so every tool and every launch option must be in it, written between
# backticks (`set_zone_sample`, `--allow-disk`).
#
# Usage: cmake -DSIMULATED_SERVER=<exe> -DREAL_SERVER=<exe> -DREADME=<path> -DINPUT_FILE=<jsonl with one tools/list request> -P CheckReadmeCoversTools.cmake
#
# The tools are read from the simulated server launched with every option that adds tools (--allow-disk, --allow-disk-refresh); the
# options are read from the usage text of the shipped server (--help).
# [RQ-MCP-043, ADR-MCP-004 (DEC-MCP-026)]

foreach(variable SIMULATED_SERVER REAL_SERVER README INPUT_FILE)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is not set")
    endif()
endforeach()

file(READ "${README}" readme)

execute_process(
    COMMAND "${SIMULATED_SERVER}" --allow-disk --allow-disk-refresh
    INPUT_FILE "${INPUT_FILE}"
    OUTPUT_VARIABLE listing
    RESULT_VARIABLE listing_status
    TIMEOUT 120)
if(NOT listing_status EQUAL 0)
    message(FATAL_ERROR "The simulated server exited with status ${listing_status}")
endif()
string(STRIP "${listing}" listing)
string(JSON tool_count LENGTH "${listing}" result tools)
if(tool_count LESS 1)
    message(FATAL_ERROR "The server listed no tool: the check would pass vacuously")
endif()

set(missing_tools "")
math(EXPR last_tool "${tool_count} - 1")
foreach(index RANGE 0 ${last_tool})
    string(JSON tool_name GET "${listing}" result tools ${index} name)
    string(FIND "${readme}" "`${tool_name}`" at)
    if(at EQUAL -1)
        list(APPEND missing_tools "${tool_name}")
    endif()
endforeach()

execute_process(
    COMMAND "${REAL_SERVER}" --help
    OUTPUT_VARIABLE usage
    RESULT_VARIABLE usage_status
    TIMEOUT 60)
if(NOT usage_status EQUAL 0)
    message(FATAL_ERROR "The server's --help exited with status ${usage_status}")
endif()
string(REGEX MATCHALL "--[a-z][a-z-]*" options "${usage}")
list(REMOVE_DUPLICATES options)
list(LENGTH options option_count)
if(option_count LESS 1)
    message(FATAL_ERROR "The usage text names no option: the check would pass vacuously")
endif()

set(missing_options "")
foreach(option IN LISTS options)
    string(FIND "${readme}" "`${option}" at)
    if(at EQUAL -1)
        list(APPEND missing_options "${option}")
    endif()
endforeach()

if(missing_tools OR missing_options)
    list(JOIN missing_tools ", " tools_text)
    list(JOIN missing_options ", " options_text)
    message(FATAL_ERROR "The README leaves out\n  tools: ${tools_text}\n  options: ${options_text}")
endif()
message(STATUS "The README names the ${tool_count} tools and the ${option_count} options")
