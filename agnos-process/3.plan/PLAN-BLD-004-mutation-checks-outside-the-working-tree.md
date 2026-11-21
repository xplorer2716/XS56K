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
- **Status**: Done
- **Description**: A script in `juce/tools` (Python, like the other generators, so that it runs on the three platforms) that reads a list of alterations (file, text to find, replacement, a name), copies the tree without its build directories into a scratch folder per worker, configures each copy with the sources of the dependencies the working tree already fetched (no new download), applies one alteration at a time, builds the test target, runs the tests selected for that alteration (by name or tag) with `ctest --timeout`, reports caught or missed per alteration, and checks that the working tree is byte for byte unchanged. The alterations of TASK-AKM-006, 008, 013 and 009 (kept in the scratchpad of those sessions) are the first list to run.
- **Requirement refs**: RQ-BLD-015
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the four criteria of RQ-BLD-015, each with a test of the script on a small fixture project (a tree of a few files and one test), plus: *Given* the alterations of TASK-AKM-009, *When* the tool runs with four workers, *Then* it reports the same caught and missed results as the in-place runs, in a fraction of their time.
- **Dependencies**: None (to be done after PLAN-AKM-001)
- **Assignee**: AI
- **Verification**: Windows/MSVC, `juce/tools/mutate.py` (new) + a fixture project
  `juce/tests/tools/mutate_fixture/` (new: a library of three tiny functions, one test per function —
  a normal one, an unused one, and one that hangs unless stopped) + `juce/tests/tools/test_mutate.py`
  (new, 6 cases, registered as the new ctest entry `bld_mutate_tool_script_tests`, split out from
  `akm_item_catalogue_script_tests` so the two Python scripts' tests stay independently named and
  timed). `ctest` 351/351 on a fresh `build-win-bld` (Debug, configured from `dev`), the two Python
  entries included (`akm_item_catalogue_script_tests` 1.9 s, `bld_mutate_tool_script_tests` 38.5 s).
  Each of RQ-BLD-015's five Gherkin clauses has its own fixture-based test, all passing on a real run
  (not just reasoned about): a caught alteration (`break-add`, arithmetic sign flipped) and a missed
  one (`touch-multiply-unwatched`, a change no selected test exercises) in the same run, the working
  tree's own hash unchanged before and after; two `hang` alterations under two workers finish in under
  75% of the time two under one worker take (a generous margin against machine-load flakiness); a hang
  is reported `caught` by timeout, not left running; text found zero times or twice is `skipped`,
  never applied, with the count named; a `break-add` run only builds the `fixture_add` target (checked
  by inspecting the actual linked executables in the scratch build directory, past the project-file
  scaffolding a multi-config generator writes for every target regardless of what is built) and its
  log never mentions the other targets' tests. Beyond the fixture: **run once against the real
  project** — `akm/src/Session.cpp`'s `config.targetDeviceId > DEVICE_ID_MAX` replaced with `false`
  (the same fault as TASK-AKM-009's own recorded "a DeviceID above 31 accepted" mutation), target
  `xs56k_akm_tests`, tests `DeviceID of 32`, `--reuse-deps-from build-win-bld` (no JUCE/Catch2
  re-download) and `--cmake-arg=-DBUILD_TESTS=ON`: reported `caught` in 1 m 47 s for the one alteration
  (Catch2 and the whole `xs56k_akm`/`xs56k_akm_tests` built from scratch in the scratch copy — no
  compiled artefact is reused, only fetched sources), the real `SessionOpenTests.cpp` case failing as
  expected, and `juce/`'s own tree hash unchanged before and after. TASK-BLD-012's own extra
  acceptance criterion — replaying TASK-AKM-009's eighteen alterations with four workers and comparing
  to its in-place results — could not be run as literally written: those alterations were kept in that
  session's own scratchpad, not in this repository, and are not recoverable now; the single real-project
  run above is offered as the closest available substitute (the tool working end to end against the
  actual codebase, not only the synthetic fixture), not a replacement for the missing AC. Mutation
  testing of this task's own code is not applicable (RQ-BLD-015/TASK-BLD-012 is the mutation tool
  itself).
- **Assumptions**: The owner's verdict of 2026-09-27, taken as the design brief: rebuilding and
  re-running the whole suite for one alteration is overengineering and a heavy design flaw, not a cost
  to accept, and no further waiting of that kind is to be imposed on the execution of PLAN-AKM-001. So
  the tool rebuilds only the CMake target an alteration names (an incremental build in the copy — CMake's
  own dependency graph pulls in the library the altered file belongs to), runs only the tests selected
  for that alteration via a `ctest -R` regex written beside it, and stops at the first failing one
  (`ctest --stop-on-failure`); compile and link were not found too slow to make the alternative studied
  (building every alteration once, selecting one at run time via test switches in the code under test).
  Each alteration names its own CMake target and `ctest -R` regex rather than the tool inferring them,
  matching the brief's "a tag or name filter written beside the alteration" — inference (which target
  owns a given source file, which tests exercise it) is a much larger problem the brief did not ask for.
  "Caught" does not distinguish a failed test from a build failure or a timeout, matching RQ-BLD-015's
  own wording ("a test failed, timed out, or the build broke") — the log kept alongside each result is
  where that distinction is read from when it matters. The per-test time limit (`--timeout`, `ctest
  --timeout`) is the only one RQ-BLD-015 asks for; the tool places none on the configure or build steps
  themselves, so a build that hangs (as opposed to a test that does) is not caught by a timeout — not
  observed in this task's own runs, left for a later task if it proves a real problem.
  `--reuse-deps-from <dir>` passes `-DFETCHCONTENT_BASE_DIR=<dir>/_deps` when that folder exists, reusing
  fetched *sources* only; every copy still compiles Catch2 (and, for the real project, JUCE) from
  scratch, since no compiled artefact crosses from the original build directory to a scratch one — the
  1 m 47 s single-alteration run above is that cost, not a defect to fix here. `--cmake-arg` exists so a
  project whose test targets are behind a flag (`BUILD_TESTS=ON` here) can still be checked; an
  argparse value that itself starts with `-` needs the `--cmake-arg=-DX=Y` form, not a following bare
  token, a gotcha hit and recorded while proving the real-project run. The working-tree-unchanged
  check (RQ-BLD-015's first clause) hashes every file under `--project-root` except a fixed list of
  build-directory names (`.git`, `build`, `build-win-*`) before and after the whole run, raising a tool
  error (exit 2) if they differ — this is independent of, and in addition to, the fact that no scratch
  copy ever writes back to the original tree by construction. `--workers` defaults to 1; nothing in this
  task chose a default worker count for the real project's own eighteen-alteration batch, left to
  whoever runs it next. Feature branch for this task is `feature/BLD`, created from `dev` (not from
  `feature/AKM`, whose own FTR-AKM-004 work is unrelated and not yet merged) — the owner confirmed no
  need to merge `feature/AKM` first, since the alterations this task's own AC names (TASK-AKM-006, 008,
  009, 013) belong to PLAN-AKM-001, already on `dev`.
