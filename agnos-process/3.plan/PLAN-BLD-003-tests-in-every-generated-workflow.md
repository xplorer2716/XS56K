# PLAN-BLD-003: Every Generated Workflow Runs the Test Suites

## Overview

Makes the fifteen generated deployment workflows run the test suites (RQ-BLD-014, ADR-BLD-005): the
generator passes the `run-tests` input that the `build-app` action already has. Done on the AKM branch,
right after the suite grew to 150 tests, so that the threaded tests and the codec are run on every
platform and configuration from now on.

## References
- **Requirements**: RQ-BLD-014 (`process/1.requirements/RQ-BLD-build-tooling.md`); RQ-BLD-007, RQ-BLD-008 (the matrix and its generator); RQ-AKM-016 (the suite)
- **ADRs**: ADR-BLD-005 (DEC-BLD-028); ADR-BLD-003 (the matrix it completes)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-BLD-011: Pass `run-tests: true` from the generator to every workflow
- **Tier**: M
- **Status**: Done
- **Description**: Add `run-tests: true` to the `build-app` step of the generator, regenerate the fifteen workflows, and align the generator's docstring and `AGENTS.md` with it.
- **Requirement refs**: RQ-BLD-014, RQ-BLD-007, RQ-BLD-008
- **ADR refs**: ADR-BLD-005 (DEC-BLD-028)
- **Acceptance Criteria** (Gherkin): *Given* the generator, *When* it is run and then run with `--check`, *Then* it reports fifteen workflows up to date, and the parsed YAML of each has a `build-app` step with `run-tests: true`. *Given* the canary workflows of `windows-x64`, `macos-arm64` and `linux-x64` in Debug and Release, *When* the branch is pushed, *Then* each log shows `ctest` running the suite and each workflow succeeds. *Given* the steps of a generated workflow, *When* read, *Then* packaging and publishing come after the step that runs the tests, in the same job.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Generator run then `--check`: 15 workflows up to date; parsed YAML: 15/15 `build-app` steps carry `run-tests: true` and package/publish steps come after it. Local `BUILD_APP=ON` + `BUILD_TESTS=ON` Release build on MSVC: 0 warnings, `ctest` 150/150. CI on `38e5c39`: the six canaries and `linux-headless-canary` succeeded, each log showing the whole suite passing (150/150) on GCC 11.4 (linux debug/release), MSVC 19.44 (windows debug/release) and AppleClang 21 (macos debug/release). Not verified: the preprod and prod workflows (they run on `dev` and on a tag; same generated step); that a failing test stops packaging (follows from one job with consecutive steps, not exercised).
- **Assumptions**: The headless workflows are left as they are (ADR-BLD-005). The action `build-app` is not changed.
