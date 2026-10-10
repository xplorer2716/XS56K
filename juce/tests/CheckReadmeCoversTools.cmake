# Fails when the README of the MCP server leaves out a tool the server lists or an option of its usage text: the README is the
# reference for the person who installs and uses the server, so every tool and every launch option must be in it, written between
# backticks (`set_zone_sample`, `--allow-disk`).
#
# Usage: cmake -DSIMULATED_SERVER=<exe> -DREAL_SERVER=<exe> -DREADME=<path> -DROOT_README=<path> -DINPUT_FILE=<jsonl with one tools/list request> -P CheckReadmeCoversTools.cmake
#
# The tools are read from the simulated server launched with every option that adds tools (--allow-disk, --allow-disk-refresh, --allow-front-panel); the
# options are read from the usage text of the shipped server (--help). The numbers of tools the two READMEs state (without any option, with
# --allow-disk, with --allow-front-panel, with both) are the ones the server reports. [TASK-MCP-054, RQ-MCP-043]
# [RQ-MCP-043, ADR-MCP-004 (DEC-MCP-026)]

foreach(variable SIMULATED_SERVER REAL_SERVER README ROOT_README INPUT_FILE)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} is not set")
    endif()
endforeach()

file(READ "${README}" readme)

execute_process(
    COMMAND "${SIMULATED_SERVER}" --allow-disk --allow-disk-refresh --allow-front-panel
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

# The counts: how many tools the server lists with no option, with --allow-disk, with --allow-front-panel and with both, against the numbers the READMEs state.
file(READ "${ROOT_README}" root_readme)
function(count_tools result)
    execute_process(
        COMMAND "${SIMULATED_SERVER}" ${ARGN}
        INPUT_FILE "${INPUT_FILE}"
        OUTPUT_VARIABLE counted_listing
        RESULT_VARIABLE counted_status
        TIMEOUT 120)
    if(NOT counted_status EQUAL 0)
        message(FATAL_ERROR "The simulated server exited with status ${counted_status}")
    endif()
    string(STRIP "${counted_listing}" counted_listing)
    string(JSON counted LENGTH "${counted_listing}" result tools)
    set(${result} ${counted} PARENT_SCOPE)
endfunction()
count_tools(plain_count)
count_tools(disk_count --allow-disk)
count_tools(panel_count --allow-front-panel)
count_tools(both_count --allow-disk --allow-front-panel)

set(wrong_counts "")
set(mcp_sentence "${plain_count} tools without any option, ${disk_count} with `--allow-disk`, ${panel_count} with `--allow-front-panel` and ${both_count} with both")
string(FIND "${readme}" "${mcp_sentence}" at)
if(at EQUAL -1)
    list(APPEND wrong_counts "juce/mcp/README.md should say: ${mcp_sentence}")
endif()
set(root_sentence "${plain_count} tools, ${disk_count} with `--allow-disk`, ${panel_count} with `--allow-front-panel`, ${both_count} with both")
string(FIND "${root_readme}" "${root_sentence}" at)
if(at EQUAL -1)
    list(APPEND wrong_counts "README.md should say: ${root_sentence}")
endif()
if(wrong_counts)
    list(JOIN wrong_counts "\n  " counts_text)
    message(FATAL_ERROR "The tool counts of the READMEs are not the server's:\n  ${counts_text}")
endif()

if(missing_tools OR missing_options)
    list(JOIN missing_tools ", " tools_text)
    list(JOIN missing_options ", " options_text)
    message(FATAL_ERROR "The README leaves out\n  tools: ${tools_text}\n  options: ${options_text}")
endif()
message(STATUS "The README names the ${tool_count} tools and the ${option_count} options, and the READMEs state the counts the server reports")
