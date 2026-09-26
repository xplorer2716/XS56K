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
# actually has: only the midi, framework and a MINIMAL PLACEHOLDER app layer
# (juce/app — see its own CMakeLists.txt) exist so far — no model/controller/
# settings layer, so there is no real editor UI, only enough of a GUI target
# to exercise the versioning/deployment-matrix/cut-deployment plumbing below.
cmake_minimum_required(VERSION 3.22)

# --- Product version: DERIVED, never declared [RQ-BLD-009, ADR-BLD-003 (DEC-BLD-011)] ---
#
# CI passes these on the configure line; .github/actions/resolve-version is the
# one place either value is computed, from the commit being built — adapted
# from XplorerEditor's own root CMakeLists.txt (AGENTS.md), same reasoning,
# renamed without its inherited project prefix (owner decision, TASK-BLD-002
# follow-up; ADR-BLD-001's amended DEC-BLD-003) and without a project-specific
# one either.
#
# The default is what a local build reports, chosen to be unmistakably not a
# deployment: 0.0.0.0-local sorts below everything and cannot be confused with
# a published build in a bug report.
set(VERSION_NUMERIC "0.0.0.0" CACHE STRING
    "Numeric product version (YYYY.M.D.HHMM). CI supplies it; a local build gets 0.0.0.0.")
set(VERSION_FULL "" CACHE STRING
    "Full product version with stage suffix. Defaults to <numeric>-local.")
if(VERSION_FULL STREQUAL "")
    set(VERSION_FULL "${VERSION_NUMERIC}-local")
endif()
set(VERSION_TIMESTAMP "1970-01-01T00:00:00Z" CACHE STRING
    "Commit timestamp (ISO 8601 UTC), from resolve-version. Not yet consumed (no SBOM exists — ADR-BLD-003's explicit out-of-scope list).")
message(STATUS "XS56K product version: ${VERSION_FULL} (numeric ${VERSION_NUMERIC})")

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
option(BUILD_APP "Build the XS56K JUCE GUI application" OFF)
option(BUILD_TESTS "Build the test suites" OFF)

# --- Third-party dependencies (pinned) ---
include(FetchContent)

if(NOT BUILD_APP)
    # Skip juceaide and extras: they require GUI system libraries not needed
    # for the headless layer/test builds.
    set(JUCE_MODULES_ONLY ON CACHE BOOL "" FORCE)
endif()
set(JUCE_VERSION 8.0.15)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${JUCE_VERSION}
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(BUILD_TESTS)
    set(CATCH2_VERSION 3.9.1)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v${CATCH2_VERSION}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(CTest)
    include(Catch)
endif()

# --- Strict warnings for project code only (not JUCE module sources) [RQ-BLD-003] ---
add_library(xs56k_warnings INTERFACE)
if(MSVC)
    target_compile_options(xs56k_warnings INTERFACE /W4 /WX /permissive-)
else()
    target_compile_options(xs56k_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        # See XplorerEditor's juce/CMakeLists.txt: GCC's -Wmaybe-uninitialized
        # false-positives on JUCE's vendored SheenBidi.c. Scoped to C only.
        target_compile_options(xs56k_warnings INTERFACE
            $<$<COMPILE_LANGUAGE:C>:-Wno-error=maybe-uninitialized>)
    endif()
endif()
add_library(xs56k::warnings ALIAS xs56k_warnings)

# --- Layer libraries, bottom-up [RQ-BLD-002] ---
add_subdirectory(midi)
add_subdirectory(framework)

if(BUILD_TESTS)
    add_subdirectory(tests)  # not yet ported — TASK-BLD-005 backlog
endif()

if(BUILD_APP)
    add_subdirectory(app)  # minimal placeholder — TASK-BLD-005
endif()
