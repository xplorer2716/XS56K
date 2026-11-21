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
- **Status**: Not Started
- **Description**: Run the tool on the production code of `juce/` (all but `juce/tests/`), review the unmatched and ambiguous ones, build, run the tests, rerun the performance center.
- **Requirement refs**: RQ-QLT-014, RQ-QLT-015
- **ADR refs**: ADR-QLT-003 (DEC-QLT-017)
- **Acceptance Criteria** (Gherkin): *Given* the production code, *When* the tool runs with `--check` after the run, *Then* it reports no definition missing an ID. *Given* the change, *When* the build and ctest run, *Then* they pass. *Given* the performance center with `--call-graph`, *When* it runs, *Then* `functions-traced` is above 1.3.
- **Dependencies**: TASK-QLT-028
- **Assignee**: AI
- **Verification**:
- **Assumptions**:

### TASK-QLT-030: Trace test definitions
- **Tier**: M
- **Status**: Not Started
- **Description**: Same as TASK-QLT-029 on `juce/tests/`.
- **Requirement refs**: RQ-QLT-014, RQ-QLT-015
- **ADR refs**: ADR-QLT-003 (DEC-QLT-017)
- **Acceptance Criteria** (Gherkin): *Given* `juce/tests/`, *When* the tool runs with `--check` after the run, *Then* it reports no definition missing an ID. *Given* the change, *When* the build and ctest run, *Then* they pass.
- **Dependencies**: TASK-QLT-029
- **Assignee**: AI
- **Verification**:
- **Assumptions**:
