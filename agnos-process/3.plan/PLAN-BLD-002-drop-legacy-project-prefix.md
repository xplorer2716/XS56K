# PLAN-BLD-002: Drop the Legacy Project Prefix

## Overview

Removes the project prefix that was copied from XplorerEditor along with the ported `juce/midi` and
`juce/framework` code (named in RQ-BLD-013), replacing it by `xs56k` where a project-wide name is
needed (ADR-BLD-004).
Split in two so the build-affecting change (targets, alias, macro, identifiers, CI comment) is
verified by a real clean build before the descriptive artifacts (process documents, `AGENTS.md`)
are aligned to it. Done before the AKM layer (FTR-AKM-*) adds targets of its own.

## References
- **Requirements**: RQ-BLD-013 (`process/1.requirements/RQ-BLD-build-tooling.md`); RQ-BLD-002, RQ-BLD-003 (their acceptance criteria name the renamed targets)
- **ADRs**: ADR-BLD-004 (Proposed); amends the target-name statement of ADR-BLD-001 `DEC-BLD-003`

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-BLD-009: Rename CMake targets, alias, macro and source identifiers
- **Tier**: L
- **Status**: Done
- **Description**: Apply DEC-BLD-025 and DEC-BLD-026: rename the four targets and the alias in `juce/CMakeLists.txt`, `juce/midi/CMakeLists.txt`, `juce/framework/CMakeLists.txt` and `juce/app/CMakeLists.txt`; the two `libraryName()` strings; the log macro in `Logger.hpp`; the two local identifiers in `JuceMidiBackend.cpp`; and every comment naming the legacy prefix under `juce/` and `.github/`.
- **Requirement refs**: RQ-BLD-013, RQ-BLD-002, RQ-BLD-003
- **ADR refs**: ADR-BLD-004 (DEC-BLD-025, DEC-BLD-026)
- **Acceptance Criteria** (Gherkin): *Given* `juce/` and `.github/`, *When* searched outside build directories with both patterns defined in RQ-BLD-013's acceptance criteria, *Then* there is no match. *Given* a fresh build directory, *When* `cmake -S juce -B <dir> -DBUILD_APP=ON` and a full build are run, *Then* configure succeeds, `xs56k_midi`, `xs56k_midi_juce`, `xs56k_framework` and `XS56K` are built with zero warnings, and no target with the legacy prefix exists in the generated project.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Re-run in this session. (1) Both RQ-BLD-013 patterns, case-sensitive, over `juce/` and `.github/`: no match. (2) Fresh directory `juce/build-verify-rename`, `cmake -S juce -B <dir> -A x64 -DBUILD_APP=ON -DBUILD_TESTS=OFF` (JUCE source reused from `build-win-local/_deps/juce-src` through `FETCHCONTENT_SOURCE_DIR_JUCE`, no download): configure exit 0; `cmake --build <dir> --config Release --parallel`: exit 0, zero `warning`/`error` lines outside JUCE sources in the log, outputs `xs56k_midi.lib`, `xs56k_midi_juce.lib`, `xs56k_framework.lib` and `XS56K.exe`. (3) Generated `*.vcxproj`, solution and `CMakeCache.txt` searched for the prefixed pattern: 0 match. Scratch directory deleted afterwards. Not verified: Linux/GCC and macOS legs (CI only); the Debug configuration.
- **Assumptions**: Unit tests are out of scope (`session.unit_tests = false`; no `juce/tests` exists) — the clean build with `/WX` is the verification. Existing build directories are not reused (ADR-BLD-004 Consequences). The comment naming a test target from XplorerEditor's virtual-cable scenarios (`MockMidiBackend.cpp`) is reworded to describe the scenario without a target name, since no such target exists here. Three ported comments (`EnumUtils.hpp`, `Logger.cpp`, `PortableFormat.hpp`) cite XplorerEditor's own `ADR-BLD-004`, which the new ADR-BLD-004 of this repository would have shadowed: they now say "XplorerEditor" explicitly (`RQ-BLD-025` in two of them, same reason).

---

### TASK-BLD-010: Align process artifacts and `AGENTS.md` to the new names
- **Tier**: M
- **Status**: Done
- **Description**: Apply DEC-BLD-027: rewrite every live-state mention of the legacy names in `AGENTS.md`, `RQ-BLD-build-tooling.md` (RQ-BLD-002/003 and other criteria), `ADR-BLD-001`/`002`/`003`, `PLAN-BLD-001` (including its acceptance criteria naming library files) and the S5000 DRAFT plan (its planned Akai codec library is described as a dedicated AKM library, target name to be decided in its own ADR); reword the `DEC-BLD-003` passage about keeping the ported names to point at ADR-BLD-004; set ADR-BLD-004 to Accepted once TASK-BLD-009 is verified.
- **Requirement refs**: RQ-BLD-013
- **ADR refs**: ADR-BLD-004 (DEC-BLD-027)
- **Acceptance Criteria** (Gherkin): *Given* the repository, *When* searched outside build directories, `documents/`, RQ-BLD-013 and ADR-BLD-004 with both patterns defined in RQ-BLD-013's acceptance criteria, *Then* there is no match. *Given* the references to XplorerEditor as origin (name, URLs) in the same files, *When* compared with the state before this task, *Then* they are unchanged.
- **Dependencies**: TASK-BLD-009
- **Assignee**: AI
- **Verification**: Re-run in this session: both RQ-BLD-013 patterns, case-sensitive, over every tracked or untracked non-ignored file outside `documents/` (`git ls-files -co --exclude-standard`): matches only in RQ-BLD-013, ADR-BLD-004, and `.claude/settings.json` (local permission rules recorded from this session's approved commands, not a project artifact and not committed). The regenerated `process/INDEX.idx.md` has no match. Provenance references (`XplorerEditor`, `xplorer2716`) were not edited: neither pattern matches them.
- **Assumptions**: Provenance references (`XplorerEditor`, `xplorer2716`) are not identifiers and stay, per RQ-BLD-013's statement.
