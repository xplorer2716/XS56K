# PLAN-QLT-005: Function Traceability in Definitions

## Overview

Implements FTR-QLT-003 (ADR-QLT-003): the tool that copies the IDs of a header's function declaration onto its definition, then its run on the production code and on the test code.
Written on 2026-11-22 on the owner's request (session QLT). Baseline: performance center `functions-traced` 1.3/10, 1129 untraced production definitions of `.cpp` files.

## References
- **Requirements**: RQ-QLT-014 to RQ-QLT-016 (`agnos-process/1.requirements/FTR-QLT-003-function-traceability-in-definitions.md`)
- **ADRs**: ADR-QLT-003 (DEC-QLT-015 to DEC-QLT-017)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-QLT-028: Write the header ID tracing tool
- **Tier**: L
- **Status**: Done
- **Description**: `juce/tools/trace_header_ids.py` (DEC-QLT-015 to DEC-QLT-017), its tests `juce/tests/tools/test_trace_header_ids.py` written first, and their ctest entry `qlt_trace_ids_script_tests`.
- **Requirement refs**: RQ-QLT-014, RQ-QLT-015, RQ-QLT-016
- **ADR refs**: ADR-QLT-003 (DEC-QLT-015, DEC-QLT-016, DEC-QLT-017)
- **Acceptance Criteria** (Gherkin): see RQ-QLT-014, RQ-QLT-015 and RQ-QLT-016.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: `test_trace_header_ids.py` written first, 11 tests: 10 passed at the first run, 1 failed (two overloads and a third definition counted ambiguous, not unmatched), code fixed, then 11 OK, 0 skipped. `--check --exclude juce/tests juce` on the real code in 0.9 s: missing 384, traced 18, unmatched 49 (all in headers of `juce/tests/`), ambiguous 0; of 837 production function declarations, 409 carry IDs in their own comment, the 98 commented without ID cite none. clang-tidy: no C++ changed. IDE SonarQube: `python:S1481` (unused `end`) fixed; test file clean. ctest entry `qlt_trace_ids_script_tests` added (SKIP_RETURN_CODE 77), its command run directly (exit 0); no ctest run, the worktree has no build.
- **Assumptions**: The name-only fallback of DEC-QLT-016 (added to the DEC during the task) pairs a type spelt differently in header and source. TASK IDs of a declaration's comment are copied with the others.

### TASK-QLT-029: Trace production definitions
- **Tier**: M
- **Status**: Done
- **Description**: Run the tool on the production code of `juce/` (all but `juce/tests/`), review the unmatched and ambiguous ones, build, run the tests, rerun the performance center.
- **Requirement refs**: RQ-QLT-014, RQ-QLT-015
- **ADR refs**: ADR-QLT-003 (DEC-QLT-017)
- **Acceptance Criteria** (Gherkin): *Given* the production code, *When* the tool runs with `--check` after the run, *Then* it reports no definition missing an ID. *Given* the change, *When* the build and ctest run, *Then* they pass. *Given* the performance center with `--call-graph`, *When* it runs, *Then* `functions-traced` is above 1.3.
- **Dependencies**: TASK-QLT-028
- **Assignee**: AI
- **Verification**: tool on `juce` with `--exclude juce/tests`: copied 384, traced 18, unmatched 49 (headers of `juce/tests/`), ambiguous 0; then `--check`: missing 0, traced 402, exit 0. Diff: 63 files, 384 insertions, 0 deletions, LF kept; each added line follows a blank line (375), a `{` (6) or a comment (3), none after a macro continuation. Build (MSVC, Debug, tests on): fails only on `juce/tests/app/SaveTests.cpp(79)` C4244 as error, a file not changed by the task (pre-existing). ctest: 1153/1155 passed; not passing: `xs56k_app_tests` (not built, the error above) and `bld_publish_deployment_script_tests` (bash cannot find its replaced `gh`, environment; no C++ involved). Performance center `--call-graph`: `functions-traced` 1.3 -> 2.5, quality B 8.9 -> A 9.1.
- **Assumptions**: The two failures are pre-existing: the task only adds comment lines to production `.cpp` files (0 test files touched).

### TASK-QLT-030: Trace test definitions
- **Tier**: M
- **Status**: Done
- **Description**: Same as TASK-QLT-029 on `juce/tests/`, once TASK-QLT-031 and TASK-QLT-032 make the build and ctest pass.
- **Requirement refs**: RQ-QLT-014, RQ-QLT-015
- **ADR refs**: ADR-QLT-003 (DEC-QLT-017)
- **Acceptance Criteria** (Gherkin): *Given* `juce/tests/`, *When* the tool runs with `--check` after the run, *Then* it reports no definition missing an ID. *Given* the change, *When* the build and ctest run, *Then* they pass.
- **Dependencies**: TASK-QLT-029, TASK-QLT-031, TASK-QLT-032
- **Assignee**: AI
- **Verification**: tool on `juce/tests`: copied 49, unmatched 402 (production declarations, defined outside `juce/tests`), ambiguous 0; then `--check`: missing 0, traced 49, exit 0. Diff: 12 files, 49 insertions, 0 deletions; each added line follows a blank line (44) or a `{` (5). Build (MSVC Debug, tests on) exit 0; ctest 1271/1271 passed, 0 skipped by ctest. Performance center `--call-graph`: `functions-traced` 2.5 -> 2.7, findings 2572 -> 2513.
- **Assumptions**: None

### TASK-QLT-031: Fix disk format type in save tests
- **Tier**: S
- **Status**: Done
- **Description**: Found by TASK-QLT-029's build: `juce/tests/app/SaveTests.cpp(79)` gives an `int` to `DiskRecord::format` (`std::uint8_t`), C4244 as an error under MSVC, so `xs56k_app_tests` is not built. The format constants and parameters become `std::uint8_t`.
- **Requirement refs**: RQ-APP-043, RQ-APP-047 (the tests built again)
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* the MSVC Debug build with tests, *When* it runs, *Then* `xs56k_app_tests` builds and its tests pass.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**: None

### TASK-QLT-032: Fix tool path of publish tests
- **Tier**: S
- **Status**: Done
- **Description**: Found by TASK-QLT-029's ctest: `juce/tests/tools/test_publish_deployment.py` runs Git's bash with the Windows PATH, which lacks Git's `usr/bin`: `publish.sh` finds no `sed`, and the stand-ins' `#!/usr/bin/env bash` resolves to the WSL launcher of `WindowsApps`, which cannot see the script. On Windows the test's PATH now holds the folder of Git's bash right after the stand-ins.
- **Requirement refs**: RQ-BLD-016
- **ADR refs**: ADR-BLD-006
- **Acceptance Criteria** (Gherkin): *Given* Windows with the WSL launcher on the PATH, *When* `bld_publish_deployment_script_tests` runs, *Then* it passes.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**: None
