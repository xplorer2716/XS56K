# PLAN-BLD-004: Mutation checks that leave the working tree alone

## Overview

A mutation check alters the code on purpose and looks for a test that fails (RQ-BLD-015). Run in place, as it was
during PLAN-AKM-001, it edits the files and the build directory the developer works in, rebuilds and runs the whole
suite for each alteration, and is stopped by any test that hangs. This plan makes it a tool that works on isolated
copies, in parallel, on the tests concerned, with a time limit. Scheduled by the owner on 2026-09-27 to be done
after PLAN-AKM-001, which has to be finished first.

## References
- **Requirements**: RQ-BLD-015 (`process/1.requirements/RQ-BLD-build-tooling.md`); RQ-BLD-002 (the build); RQ-AKM-016 (the suites it checks)
- **ADRs**: None

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-BLD-012: Mutation check tool on isolated copies
- **Tier**: M
- **Status**: Not Started
- **Description**: A script in `juce/tools` (Python, like the other generators, so that it runs on the three platforms) that reads a list of alterations (file, text to find, replacement, a name), copies the tree without its build directories into a scratch folder per worker, configures each copy with the sources of the dependencies the working tree already fetched (no new download), applies one alteration at a time, builds the test target, runs the tests selected for that alteration (by name or tag) with `ctest --timeout`, reports caught or missed per alteration, and checks that the working tree is byte for byte unchanged. The alterations of TASK-AKM-006, 008, 013 and 009 (kept in the scratchpad of those sessions) are the first list to run.
- **Requirement refs**: RQ-BLD-015
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the four criteria of RQ-BLD-015, each with a test of the script on a small fixture project (a tree of a few files and one test), plus: *Given* the alterations of TASK-AKM-009, *When* the tool runs with four workers, *Then* it reports the same caught and missed results as the in-place runs, in a fraction of their time.
- **Dependencies**: None (to be done after PLAN-AKM-001)
- **Assignee**: AI
- **Verification**: Not yet run.
- **Assumptions**: None yet.
