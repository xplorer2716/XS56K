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
- **Assumptions**: The owner's verdict of 2026-09-27, to be taken as the design brief: rebuilding and re-running the whole suite for one alteration is overengineering and a heavy design flaw, not a cost to accept, and no further waiting of that kind is to be imposed on the execution of PLAN-AKM-001. So the tool SHALL NOT rerun everything: it rebuilds only the target that contains the altered file (an incremental build in the copy), runs only the tests selected for that alteration — by a tag or name filter written beside the alteration — with a per-test time limit, and reports as soon as one test fails; its cost per alteration is to be measured against the in-place runs (about a minute each), and if compile and link alone are still too slow, the alternative to study is building the alterations once and selecting one at run time, which puts test switches in the code under test and needs the owner's decision. Until this task is done, PLAN-AKM-001 runs no mutation batch: its remaining tasks say in their Verification that mutations were not replayed and are deferred here.
