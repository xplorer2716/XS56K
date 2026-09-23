# XS56K - a realtime editor for the AKAI S5000/S6000 samplers
# Copyright (C) 2026 https://github.com/xplorer2716
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Root build for the JUCE (C++) port. [RQ-BLD-001, RQ-BLD-002, RQ-BLD-003, ADR-BLD-001]
#
# Structure, dependency pinning and the strict-warnings-for-project-code-only
# split are carried over from xplorer2716/XplorerEditor's own juce/CMakeLists.txt
# (see documents/INDEX.md / AGENTS.md for that repo's role as the source this
# framework and MIDI layer were ported from). Trimmed to what this repository
# actually has: only the midi and framework layers exist so far, no GUI
# application (model/controller/settings/app), so there is nothing to fetch a
# version for yet — that machinery is deferred until an app target exists
# (RQ-BLD-009, ADR-BLD-003, TASK-BLD-006).
cmake_minimum_required(VERSION 3.22)

project(XS56KJuce
    DESCRIPTION "XS56K JUCE port - realtime editor for the AKAI S5000/S6000 samplers"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# The GUI application target only builds once it exists; the non-UI layers and
# their tests always build, headless. [RQ-BLD-002]
option(XS56K_BUILD_APP "Build the XS56K JUCE GUI application" OFF)
option(XS56K_BUILD_TESTS "Build the test suites" OFF)

# --- Third-party dependencies (pinned) ---
include(FetchContent)

if(NOT XS56K_BUILD_APP)
    # Skip juceaide and extras: they require GUI system libraries not needed
    # for the headless layer/test builds.
    set(JUCE_MODULES_ONLY ON CACHE BOOL "" FORCE)
endif()
set(XS56K_JUCE_VERSION 8.0.15)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${XS56K_JUCE_VERSION}
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(XS56K_BUILD_TESTS)
    set(XS56K_CATCH2_VERSION 3.9.1)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v${XS56K_CATCH2_VERSION}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(CTest)
    include(Catch)
endif()

# --- Strict warnings for project code only (not JUCE module sources) [RQ-BLD-003] ---
add_library(xpl_warnings INTERFACE)
if(MSVC)
    target_compile_options(xpl_warnings INTERFACE /W4 /WX /permissive-)
else()
    target_compile_options(xpl_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        # See XplorerEditor's juce/CMakeLists.txt: GCC's -Wmaybe-uninitialized
        # false-positives on JUCE's vendored SheenBidi.c. Scoped to C only.
        target_compile_options(xpl_warnings INTERFACE
            $<$<COMPILE_LANGUAGE:C>:-Wno-error=maybe-uninitialized>)
    endif()
endif()
add_library(xpl::warnings ALIAS xpl_warnings)

# --- Layer libraries, bottom-up [RQ-BLD-002] ---
add_subdirectory(midi)
add_subdirectory(framework)

if(XS56K_BUILD_TESTS)
    add_subdirectory(tests)  # not yet ported — TASK-BLD-005 backlog
endif()

if(XS56K_BUILD_APP)
    add_subdirectory(app)  # not yet ported — TASK-BLD-005 backlog
endif()
