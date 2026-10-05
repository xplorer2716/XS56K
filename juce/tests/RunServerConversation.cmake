# Runs an MCP server executable, with a scripted conversation on its standard input when there is one, and checks
# what it does.
#
# Usage: cmake -DSERVER=<exe> [-DSERVER_ARGS=<;-list>] [-DINPUT_FILE=<jsonl>] [-DEXPECT_STATUS=<n, default 0>]
#              [-DEXPECT_LINES=<n>] [-DPATTERNS_FILE=<one regex per line>] [-DEXPECTED_FILE=<the exact output>]
#              [-DOUTPUT_PATTERN=<regex>] [-DERROR_PATTERN=<regex>] -P RunServerConversation.cmake
#
# The checks, each of which fails the test: the process exits with EXPECT_STATUS (0 when its input ends); when
# EXPECT_LINES is given, its standard output is exactly that many lines, each valid JSON and none empty (nothing but
# the protocol on it, and no carriage return); every regex of PATTERNS_FILE matches somewhere in it; EXPECTED_FILE, when
# given, is the exact output; OUTPUT_PATTERN and ERROR_PATTERN match its standard output and its standard error, when
# given. Lines are read one at a time rather than as a CMake list: JSON holds semicolons and brackets.
# [RQ-MCP-001, RQ-MCP-002, RQ-MCP-012, ADR-MCP-001 (DEC-MCP-009), TASK-MCP-006]

if(NOT DEFINED SERVER)
    message(FATAL_ERROR "SERVER is not set")
endif()
if(NOT DEFINED EXPECT_STATUS)
    set(EXPECT_STATUS 0)
endif()

if(DEFINED INPUT_FILE)
    execute_process(
        COMMAND ${SERVER} ${SERVER_ARGS}
        INPUT_FILE ${INPUT_FILE}
        OUTPUT_VARIABLE output
        ERROR_VARIABLE diagnostics
        RESULT_VARIABLE status
        TIMEOUT 120)
else()
    execute_process(
        COMMAND ${SERVER} ${SERVER_ARGS}
        OUTPUT_VARIABLE output
        ERROR_VARIABLE diagnostics
        RESULT_VARIABLE status
        TIMEOUT 120)
endif()

if(NOT status EQUAL ${EXPECT_STATUS})
    message(FATAL_ERROR "The server exited with status ${status}, expected ${EXPECT_STATUS}.\nstdout:\n${output}\nstderr:\n${diagnostics}")
endif()

# A CR in the output would mean a text-mode stream: the protocol's separator is a bare newline.
string(FIND "${output}" "\r" carriage_return)
if(NOT carriage_return EQUAL -1)
    message(FATAL_ERROR "The server wrote a carriage return on standard output.\nstdout:\n${output}")
endif()

# Each line of the output, in turn: valid JSON, not empty.
set(remaining "")
if(DEFINED EXPECT_LINES)
    set(remaining "${output}")
endif()
set(line_count 0)
while(NOT remaining STREQUAL "")
    string(FIND "${remaining}" "\n" end_of_line)
    if(end_of_line EQUAL -1)
        message(FATAL_ERROR "The last line of standard output has no newline.\nstdout:\n${output}")
    endif()
    string(SUBSTRING "${remaining}" 0 ${end_of_line} line)
    math(EXPR next "${end_of_line} + 1")
    string(SUBSTRING "${remaining}" ${next} -1 remaining)
    if(line STREQUAL "")
        message(FATAL_ERROR "An empty line on standard output.\nstdout:\n${output}")
    endif()
    string(JSON kind ERROR_VARIABLE parse_error TYPE "${line}")
    if(NOT parse_error STREQUAL "NOTFOUND")
        message(FATAL_ERROR "A line of standard output is not JSON (${parse_error}):\n${line}")
    endif()
    math(EXPR line_count "${line_count} + 1")
endwhile()
if(DEFINED EXPECT_LINES AND NOT line_count EQUAL EXPECT_LINES)
    message(FATAL_ERROR "Expected ${EXPECT_LINES} line(s) on standard output, got ${line_count}.\nstdout:\n${output}\nstderr:\n${diagnostics}")
endif()

if(DEFINED PATTERNS_FILE)
    file(READ "${PATTERNS_FILE}" remaining)
    while(NOT remaining STREQUAL "")
        string(FIND "${remaining}" "\n" end_of_line)
        if(end_of_line EQUAL -1)
            set(pattern "${remaining}")
            set(remaining "")
        else()
            string(SUBSTRING "${remaining}" 0 ${end_of_line} pattern)
            math(EXPR next "${end_of_line} + 1")
            string(SUBSTRING "${remaining}" ${next} -1 remaining)
        endif()
        if(NOT pattern STREQUAL "" AND NOT output MATCHES "${pattern}")
            message(FATAL_ERROR "Standard output does not match /${pattern}/.\nstdout:\n${output}")
        endif()
    endwhile()
endif()

if(DEFINED EXPECTED_FILE)
    file(READ "${EXPECTED_FILE}" expected)
    if(NOT output STREQUAL expected)
        message(FATAL_ERROR "Standard output differs from ${EXPECTED_FILE}.\nactual:\n${output}\nexpected:\n${expected}")
    endif()
endif()

if(DEFINED OUTPUT_PATTERN AND NOT output MATCHES "${OUTPUT_PATTERN}")
    message(FATAL_ERROR "Standard output does not match /${OUTPUT_PATTERN}/.\nstdout:\n${output}")
endif()

if(DEFINED ERROR_PATTERN AND NOT diagnostics MATCHES "${ERROR_PATTERN}")
    message(FATAL_ERROR "Standard error does not match /${ERROR_PATTERN}/.\nstderr:\n${diagnostics}")
endif()

message(STATUS "The server exited with status ${status}; ${line_count} line(s) checked as JSON")
