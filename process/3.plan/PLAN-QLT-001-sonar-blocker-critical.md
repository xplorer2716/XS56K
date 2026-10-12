# PLAN-QLT-001: SonarCloud BLOCKER and CRITICAL Issues

## Overview

Gets the project to no open BLOCKER or CRITICAL issue outside the test code (FTR-QLT-001, ADR-QLT-001). SonarCloud lists 427 on 2026-10-12: 110 on test code and 317 on production code.
A tool classifies them and accepts the 110 on test code in SonarCloud; the production ones are fixed, each family in its own task, with no change of behaviour.

**Two plans, because of the limit of ten tasks per session.** This plan holds the first ten tasks: the tool and the acceptance (TASK-QLT-001, 002), the small fixes (003 to 005), the first test target of
`juce/framework` and the two fixes that need it (006 to 008), and the two Python generators (009, 010). It fixes 15 production issues (S5404, S3491 x2, S1186, S3624 x2, S5421 x4, S134 in `AbstractController`,
S3776 x4 in two Python files). `PLAN-QLT-002` holds the next ten (the `module` names, the 241 lambda issues, the large functions of `juce/mcp`, `generate_sysex_coverage.py`, the path traversal),
and starts only when this one is Done or the owner reorders them.

**Read before starting.** SonarCloud's analysis (2026-10-12) is of `dev` at `937b503`, and `feature/QLT` differs from it only by documents and `.gitignore`, so the lines SonarCloud gives are those of the files;
they move as soon as a fix is made, so an issue is found again by its rule and its function. `sonar analyze --file` cannot report the rules on this connection (it only looks for secrets), so a fix is checked by the build and the tests (DEC-QLT-005) and a task says "not checked by Sonar". Every `sonar` call goes through `rtk`. Raw issue data is never read whole: the tool prints counts. Every task on `juce/akm` or `juce/mcp` also runs the three source checks
(`CheckNoDestructiveCalls`, `CheckReadmeCoversTools`, `generate_sysex_coverage.py --check`), which `ctest` runs.

**Order.** TASK-QLT-001 then TASK-QLT-002 (the tool, then the acceptance). TASK-QLT-003, 004, 005, 009 and 010 are independent of each other. TASK-QLT-006 then 007 and 008 (the test target, then the
two fixes that need it). The last step of the plans, outside the tasks, is the owner's refresh of the SonarCloud analysis and a re-read with the tool (RQ-QLT-007).

## References
- **Requirements**: RQ-QLT-001 to RQ-QLT-008 (PLAN-QLT-002 continues them) (`process/1.requirements/FTR-QLT-001-sonar-blocker-critical.md`); RQ-BLD-003 (the build stays warning-clean); RQ-AKM-016 (the suite)
- **ADRs**: ADR-QLT-001 (DEC-QLT-001 to DEC-QLT-005)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-QLT-001: The triage tool `juce/tools/sonar_triage.py`
- **Tier**: M
- **Status**: Done
- **Description**: Write the script of DEC-QLT-003 and its `unittest` tests (`juce/tests/tools/test_sonar_triage.py`, a `ctest` entry beside the other tools'): list the open issues of given severities through `sonar api`, classify by path, print counts and the production issues, accept the test-code ones with `--apply`.
- **Requirement refs**: RQ-QLT-002, RQ-QLT-006, RQ-QLT-008
- **ADR refs**: ADR-QLT-001 (DEC-QLT-001, DEC-QLT-003)
- **Acceptance Criteria** (Gherkin): *Given* a recorded reply with issues on `juce/tests/`, on `.github/skills/*/tests/` and elsewhere, *When* the script classifies it, *Then* each is counted on the right side, with `juce/midi/src/MockMidiBackend.cpp` counted as production. *Given* no `--apply`, *When* the script runs, *Then* no `do_transition` request is made and it prints the number it would accept. *Given* `--apply` and a reply in which every test issue is `ACCEPTED`, *When* the script runs, *Then* it sends nothing and exits 0. *Given* more than one page of issues, *When* the script runs, *Then* it reads every page. *Given* a failing `sonar` call, *When* the script runs, *Then* it exits non-zero with the CLI's message and writes no cache file.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Tests written first: `unittest discover -p test_sonar_triage.py` ran 1 test and errored (the script did not exist); after the script, 28 tests OK (classification of the two test trees, the mock backend and `juce/tests_old/` as production; paging over 3 pages; the API's 10 000 limit refused; dry run posts nothing; `--apply` posts one `accept` with a comment naming RQ-QLT-002 per pending test issue and none for production; ACCEPTED skipped, second run posts nothing; failing call exits 2 with the CLI's message and no cache file; banner before the JSON ignored). `cmake -S juce -B juce/build` registered `qlt_sonar_triage_script_tests` (#997); `ctest -C Debug -R "qlt_sonar_triage|akm_item_catalogue_script|sysex_coverage_script"`: 3/3 passed, the new one in 0.22 s. Real dry run against SonarCloud (`sonar_triage.py --limit 8`, exit 0): 427 issues, 110 test code, 317 production code, "would accept 110 test-code issues (0 already handled); nothing sent"; the raw reply went to a file in the scratchpad, 8 production issues listed. Not verified: an `--apply` against SonarCloud (TASK-QLT-002), so the transition's body (the `comment` inside `do_transition`, taken from the CLI's own help example) has not been exercised; the whole suite was not run (the change adds a Python script, a test file and one CMake entry).
- **Assumptions**: An issue is accepted from the statuses OPEN, CONFIRMED and REOPENED; the search asks for `resolved=false`, so an accepted issue is not even returned. The list of production issues is capped (30 by default, `--limit`), so as not to print 317 lines. TASK-QLT-002 tries the transition on one issue before running it on the others.

### TASK-QLT-002: Accept the 110 test-code issues in SonarCloud
- **Tier**: S
- **Status**: Done
- **Description**: Run `sonar_triage.py --apply` on the BLOCKER and CRITICAL issues, after the owner has seen the dry run's count, so the 110 test-code issues are `ACCEPTED` with the comment of DEC-QLT-001. This changes SonarCloud's state, so it runs only on the owner's go for this call.
- **Requirement refs**: RQ-QLT-002, RQ-QLT-008
- **ADR refs**: ADR-QLT-001 (DEC-QLT-001)
- **Acceptance Criteria** (Gherkin): *Given* the dry run of TASK-QLT-001 showing 110, *When* the owner agrees and the script runs with `--apply`, *Then* the listing of `ACCEPTED` issues of severity BLOCKER and CRITICAL holds the 110, each with the comment naming RQ-QLT-002. *Given* the script run a second time, *Then* it sends nothing.
- **Dependencies**: TASK-QLT-001
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**: None (the transition was tried on one issue, `AaExyeQbfxqBnb3I71lo`, then run on the 109 others, as planned; 110 accepted on 2026-10-12 on the owner's go for this call)

### TASK-QLT-003: Empty body and copy operations (`cpp:S1186`, `cpp:S3624`)
- **Tier**: S
- **Status**: Done
- **Description**: A comment in the empty `NullDiagnosticSink::report` of `DiagnosticSink.hpp`; `MockInputPort` and `MockOutputPort` of `MockMidiBackend.cpp` delete their copy constructor and copy assignment. Three issues, at most five lines.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-002)
- **Acceptance Criteria** (Gherkin): *Given* the two files changed, *When* the project builds with the tests on, *Then* it builds warning-clean and `ctest` passes. *Given* `sonar analyze --file` on each, *Then* neither `cpp:S1186` nor `cpp:S3624` is reported there.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**: Checked by the build and the tests, not by Sonar: `sonar analyze --file` cannot report the rules on this connection. `MockMidiBackend.cpp` is production code (it is a source of the `xs56k_midi` library): it is fixed, not accepted.

### TASK-QLT-004: Two useless `&*` (`cpp:S3491`, BLOCKER)
- **Tier**: S
- **Status**: Done
- **Description**: In `ItemCatalogue.cpp`, `findItem` and `findReplyItem` return `std::to_address(found)` in place of `&*found` (SonarCloud flags both, lines 31 and 39). Two lines.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-002)
- **Acceptance Criteria** (Gherkin): *Given* the change, *When* the project builds on MSVC with the tests on, *Then* it builds warning-clean and the item lookups of `xs56k_akm_tests` pass. *Given* `sonar analyze --file juce/akm/src/ItemCatalogue.cpp`, *Then* `cpp:S3491` is not reported.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**: Checked by the build and the tests, not by Sonar (the analyser cannot report the rules here). `<memory>` is included for `std::to_address`.

### TASK-QLT-005: The scheduler's wait has a predicate (`cpp:S5404`, BLOCKER bug)
- **Tier**: M
- **Status**: Done
- **Description**: In `RealScheduler::run`, the call `_wake.wait(lock)` (the wait on an empty queue, the one SonarCloud flags) takes a predicate that rechecks what the loop reads: `_stopping`, or a task now queued (`_queue.nextDeadline().has_value()`), so that a wake-up that finds neither goes back to sleep. The timed `wait_until` is not changed: SonarCloud does not flag it and it already returns to the loop, which recomputes the deadline.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-002)
- **Acceptance Criteria** (Gherkin): *Given* a scheduler with an empty queue and a worker waiting, *When* a task is scheduled, *Then* it runs. *Given* a worker waiting for a deadline a minute away, *When* a task due sooner is scheduled, *Then* it runs at its time and not at the first deadline. *Given* a worker waiting, *When* the scheduler is stopped, *Then* `run` returns and the thread joins. *Given* `SchedulerTests` and `ExecutorTests` run 50 times in a row, *Then* none fails. *Given* `sonar analyze --file juce/akm/src/RealScheduler.cpp`, *Then* `cpp:S5404` is not reported.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Tests first, in `SchedulerTests.cpp`: idle scheduler then a task scheduled runs; a task due in 50 ms scheduled while the thread waits for one an hour ahead runs at its time and the late one does not; an idle scheduler destroyed returns. `xs56k_akm_tests "[scheduler]"` on the unfixed code: all passed (37 assertions, 22 test cases). After the change: `cmake --build juce/build --config Debug --parallel` clean under `/WX`; `ctest -C Debug -j 8 -E bld_mutate_tool_script_tests`: 1106/1106 passed, including the source checks (`sysex_coverage_table_is_up_to_date`, `sysex_coverage_tools_match_the_simulated_sampler`); `ctest -R "the real scheduler" --repeat until-fail:50`: 13 tests, 50 runs each, all passed. Not verified: that SonarCloud no longer reports `cpp:S5404` (`sonar analyze --file` only looks for secrets on this connection); the mutation tool was not run on the change.
- **Assumptions**: Only the `wait(lock)` call is changed, the one SonarCloud flags; `wait_until` is left as it is. The three new tests pin behaviour that the unfixed code already had, as the plan expects of a behaviour-preserving change.

### TASK-QLT-006: The test target `xs56k_framework_tests`, with `Logger` and `AbstractController` pinned
- **Tier**: M
- **Status**: Done
- **Description**: Add `juce/tests/framework/` (Catch2, linked to `xs56k_framework`, `xs56k_midi` and its mock backend) and register it in `juce/tests/CMakeLists.txt` with `catch_discover_tests`. Gherkin-named tests that pin, on the unfixed code: `Logger` (a call below the level or under a disabled domain writes nothing; a call above writes once with the level and domain names; the sink is replaced and removed) and `AbstractController::automationInputDeviceChannelMessageReceived` (an automated CC sets and notifies the parameter by the ratio formula, including the two-state mid-range case; the disabled CC is dropped; any other message is forwarded on the tone's channel).
- **Requirement refs**: RQ-QLT-005, RQ-QLT-004
- **ADR refs**: ADR-QLT-001 (DEC-QLT-004)
- **Acceptance Criteria** (Gherkin): *Given* the unfixed `Logger` and `AbstractController`, *When* the new tests run, *Then* they pass. *Given* the tests of the CMake build, *When* `ctest` runs, *Then* the new executable's tests are discovered and the earlier ones are unchanged. *Given* a mutation of the ratio formula or of the level check (`juce/tools/mutate.py`), *Then* at least one new test fails.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Tests first: 28 Gherkin-named tests in the new target `xs56k_framework_tests` (`juce/tests/framework/LoggerTests.cpp`, `AbstractControllerAutomationTests.cpp`), run on the unchanged `Logger` and `AbstractController` code: all passed (494 assertions in 28 test cases). `cmake -S juce -B juce/build` then a build warning-clean under `/WX`; `ctest -C Debug -R "RQ-FMW|ADR-FMW|RQ-NFR-008"`: the 28 are discovered and pass. Mutation check (`juce/tools/mutate.py` on a copy of `juce/`, `-DBUILD_TESTS=ON`, `--reuse-deps-from juce/build`), 6 alterations, all caught by failing tests: the CC value halved before scaling, the two-state threshold 63 moved to 100, the disabled-CC check removed, the forwarded message left on the incoming channel, the level comparison `>` made `>=`, the domain gate removed. Not verified: the real-time paths of `AbstractController` (worker thread, transmit delay) and `Logger::configure(path)` on a read-only folder; the new tests start no thread.
- **Assumptions**: The tests pin today's behaviour of the paths that TASK-QLT-007 and TASK-QLT-008 touch, not the whole of the two classes. A control change mapped to a name that is not a parameter would dereference a null pointer in `AbstractController`; it is not tested and not changed (out of scope). The ported sources cite RQ-QLT-011, RQ-QLT-012 and RQ-QLT-013, which are XplorerEditor's own requirements: they are not defined in this repository, and this repository's RQ-QLT numbering will meet them at RQ-QLT-011 (not changed). The first mutation runs were invalid ("build failed": the copy was configured without `BUILD_TESTS`, so the target did not exist) and were repeated with `--cmake-arg=-DBUILD_TESTS=ON`; two alterations whose configure step failed under three workers were repeated with one.

### TASK-QLT-007: `Logger`'s state in one object (`cpp:S5421` x4)
- **Tier**: M
- **Status**: Done
- **Description**: In `Logger.cpp`, the file-local mutable `g_mutex`, `g_sink`, `g_level` and `g_domainEnabled` become the members of one struct reached through one function that returns a reference to a function-local object; the public functions use it. Same locking rule, same atomics, no change of the public header.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-005, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-002, DEC-QLT-004)
- **Acceptance Criteria** (Gherkin): *Given* the tests of TASK-QLT-006, *When* `Logger.cpp` is changed, *Then* they pass unmodified. *Given* the library, *When* it builds, *Then* it builds warning-clean on MSVC and GCC (CI) and the public header is unchanged. *Given* `sonar analyze --file juce/framework/src/Logger.cpp`, *Then* `cpp:S5421` is not reported.
- **Dependencies**: TASK-QLT-006
- **Assignee**: AI
- **Verification**: `git diff --stat`: only `juce/framework/src/Logger.cpp` (33 insertions, 21 deletions); the public header `Logger.hpp` is unchanged and no `g_` variable is left (`grep g_ Logger.cpp`: none). The tests of TASK-QLT-006 run on the changed file, unmodified: `xs56k_framework_tests.exe`: all passed (494 assertions in 28 test cases), the same count as before the change. Build of `xs56k_framework` and `xs56k_framework_tests` warning-clean under `/WX` (MSVC, Debug). Not verified: that SonarCloud no longer reports `cpp:S5421` (the analyser cannot report rules on this connection); GCC and Apple Clang (CI); the static destruction order at process exit, which is that of one function-local static in place of four namespace-scope objects.
- **Assumptions**: The state is one struct with a function-local static, built on first use and destroyed at exit like the four variables it replaces; the comment above it explains why it stays file-local. Checked by the build and the tests, not by Sonar.

### TASK-QLT-008: Nesting in `AbstractController` (`cpp:S134`)
- **Tier**: M
- **Status**: Not Started
- **Description**: In `AbstractControllerDevices.cpp`, the loop body of `automationInputDeviceChannelMessageReceived` that scales a CC value and sets and notifies one parameter moves into a private member; the nesting falls to three. The member is declared in `AbstractController.hpp`; the formula and the order of the calls are unchanged. [RQ-FMW-050]
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-005, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-002, DEC-QLT-004)
- **Acceptance Criteria** (Gherkin): *Given* the tests of TASK-QLT-006, *When* the function is changed, *Then* they pass unmodified. *Given* `sonar analyze --file juce/framework/src/AbstractControllerDevices.cpp`, *Then* `cpp:S134` is not reported there.
- **Dependencies**: TASK-QLT-006
- **Assignee**: AI
- **Verification**: To be filled at closure.
- **Assumptions**: None

### TASK-QLT-009: Cognitive complexity of `generate_akm_items.py` (`python:S3776` x2)
- **Tier**: M
- **Status**: Not Started
- **Description**: `validate` (complexity 46 at the analysis) and `coverage` (22) are cut into named steps, each under the 15 the rule allows. The data file, the generated header and the output of `--check` and `--coverage` do not change. Located by function: the file changed after the analysis.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-002)
- **Acceptance Criteria** (Gherkin): *Given* the script changed, *When* `python juce/tools/generate_akm_items.py --check` and `--coverage` run, *Then* they report what they reported before, and `ItemTable.generated.hpp` is unchanged (`git diff` empty). *Given* `test_generate_akm_items.py`, *When* it runs, *Then* it passes unmodified. *Given* `sonar analyze --file juce/tools/generate_akm_items.py`, *Then* `python:S3776` is not reported.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: To be filled at closure.
- **Assumptions**: None

### TASK-QLT-010: Cognitive complexity of `build_sysex_index.py` (`python:S3776` x2)
- **Tier**: M
- **Status**: Not Started
- **Description**: The two functions (complexity 17 and 137 at the analysis) are cut into named steps. The script has no unit test and needs `pymupdf` and the PDF: its test is the byte-identical regeneration of `sysex_spec.clean.txt` and `sysex_spec.items.tsv` (RQ-QLT-005). The riskiest task of the plan: if `pymupdf` cannot be installed, or the output differs and the difference is not understood after one fix, the task stops and the owner decides (RQ-QLT-003: refactor, or accept with his reason).
- **Requirement refs**: RQ-QLT-001, RQ-QLT-003, RQ-QLT-004, RQ-QLT-005, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-002, DEC-QLT-004)
- **Acceptance Criteria** (Gherkin): *Given* the committed `sysex_spec.clean.txt` and `sysex_spec.items.tsv`, *When* the changed script regenerates them, *Then* `git diff` on both is empty. *Given* `sonar analyze --file documents/_index/build_sysex_index.py`, *Then* `python:S3776` is not reported.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: To be filled at closure.
- **Assumptions**: None
