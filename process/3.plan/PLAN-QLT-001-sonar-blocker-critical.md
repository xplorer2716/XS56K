# PLAN-QLT-001: SonarCloud BLOCKER and CRITICAL Issues

## Overview

Gets the project to no open BLOCKER or CRITICAL issue outside the test code (FTR-QLT-001, ADR-QLT-001). SonarCloud lists 427 on 2026-10-12: 110 on test code and 317 on production code.
A tool classifies them and accepts the 110 on test code in SonarCloud; the production ones are fixed, each family in its own task, with no change of behaviour.

**Two plans, because of the limit of ten tasks per session.** This plan holds the first ten tasks: the tool and the acceptance (TASK-QLT-001, 002), the small fixes (003 to 005), the first test target of
`juce/framework` and the two fixes that need it (006 to 008), and the two Python generators (009, 010). It fixes 15 production issues (S5404, S3491 x2, S1186, S3624 x2, S5421 x4, S134 in `AbstractController`,
S3776 x4 in two Python files). `PLAN-QLT-002` holds the next ten (the `module` names, the 241 lambda issues, the large functions of `juce/mcp`, `generate_sysex_coverage.py`, the path traversal),
and starts only when this one is Done or the owner reorders them.

**Read before starting.** SonarCloud's analysis (2026-10-12) is of `dev` at `937b503`, and `feature/QLT` differs from it only by documents and `.gitignore`, so the lines SonarCloud gives are those of the files;
they move as soon as a fix is made, so an issue is found again by its rule and its function. Before changing a file, run `sonar analyze --file <file>`: if the rule is no longer reported, the task says so and
changes nothing. Every `sonar` call goes through `rtk`. Raw issue data is never read whole: the tool prints counts. Every task on `juce/akm` or `juce/mcp` also runs the three source checks
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
- **Status**: Not Started
- **Description**: Run `sonar_triage.py --apply` on the BLOCKER and CRITICAL issues, after the owner has seen the dry run's count, so the 110 test-code issues are `ACCEPTED` with the comment of DEC-QLT-001. This changes SonarCloud's state, so it runs only on the owner's go for this call.
- **Requirement refs**: RQ-QLT-002, RQ-QLT-008
- **ADR refs**: ADR-QLT-001 (DEC-QLT-001)
- **Acceptance Criteria** (Gherkin): *Given* the dry run of TASK-QLT-001 showing 110, *When* the owner agrees and the script runs with `--apply`, *Then* the listing of `ACCEPTED` issues of severity BLOCKER and CRITICAL holds the 110, each with the comment naming RQ-QLT-002. *Given* the script run a second time, *Then* it sends nothing.
- **Dependencies**: TASK-QLT-001
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**: None

### TASK-QLT-003: Empty body and copy operations (`cpp:S1186`, `cpp:S3624`)
- **Tier**: S
- **Status**: Not Started
- **Description**: A comment in the empty `NullDiagnosticSink::report` of `DiagnosticSink.hpp`; `MockInputPort` and `MockOutputPort` of `MockMidiBackend.cpp` delete their copy constructor and copy assignment. Three issues, at most five lines.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-002)
- **Acceptance Criteria** (Gherkin): *Given* the two files changed, *When* the project builds with the tests on, *Then* it builds warning-clean and `ctest` passes. *Given* `sonar analyze --file` on each, *Then* neither `cpp:S1186` nor `cpp:S3624` is reported there.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**: `MockMidiBackend.cpp` is production code (it is a source of the `xs56k_midi` library): it is fixed, not accepted.

### TASK-QLT-004: Two useless `&*` (`cpp:S3491`, BLOCKER)
- **Tier**: S
- **Status**: Not Started
- **Description**: In `ItemCatalogue.cpp`, `findItem` and `findReplyItem` return `std::to_address(found)` in place of `&*found` (SonarCloud flags both, lines 31 and 39). Two lines.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-002)
- **Acceptance Criteria** (Gherkin): *Given* the change, *When* the project builds on MSVC with the tests on, *Then* it builds warning-clean and the item lookups of `xs56k_akm_tests` pass. *Given* `sonar analyze --file juce/akm/src/ItemCatalogue.cpp`, *Then* `cpp:S3491` is not reported.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**: None

### TASK-QLT-005: The scheduler's wait has a predicate (`cpp:S5404`, BLOCKER bug)
- **Tier**: M
- **Status**: Not Started
- **Description**: In `RealScheduler::run`, the two calls `_wake.wait_until(lock, *next)` and `_wake.wait(lock)` take a predicate that rechecks what the loop reads (`_stopping`, and a deadline earlier than the one waited for), so that a spurious or useless wake-up goes back to waiting and a task scheduled for an earlier time still wakes the worker. The analyser flags the `wait(lock)`; the timed wait is changed with it only if the analyser reports it or the two cannot be told apart.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-002)
- **Acceptance Criteria** (Gherkin): *Given* a scheduler with an empty queue and a worker waiting, *When* a task is scheduled, *Then* it runs. *Given* a worker waiting for a deadline a minute away, *When* a task due sooner is scheduled, *Then* it runs at its time and not at the first deadline. *Given* a worker waiting, *When* the scheduler is stopped, *Then* `run` returns and the thread joins. *Given* `SchedulerTests` and `ExecutorTests` run 50 times in a row, *Then* none fails. *Given* `sonar analyze --file juce/akm/src/RealScheduler.cpp`, *Then* `cpp:S5404` is not reported.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: To be filled at closure (the tests written first and shown to pass on the unfixed code, then after the change; the 50 runs; the analyser's output).
- **Assumptions**: None

### TASK-QLT-006: The test target `xs56k_framework_tests`, with `Logger` and `AbstractController` pinned
- **Tier**: M
- **Status**: Not Started
- **Description**: Add `juce/tests/framework/` (Catch2, linked to `xs56k_framework`, `xs56k_midi` and its mock backend) and register it in `juce/tests/CMakeLists.txt` with `catch_discover_tests`. Gherkin-named tests that pin, on the unfixed code: `Logger` (a call below the level or under a disabled domain writes nothing; a call above writes once with the level and domain names; the sink is replaced and removed) and `AbstractController::automationInputDeviceChannelMessageReceived` (an automated CC sets and notifies the parameter by the ratio formula, including the two-state mid-range case; the disabled CC is dropped; any other message is forwarded on the tone's channel).
- **Requirement refs**: RQ-QLT-005, RQ-QLT-004
- **ADR refs**: ADR-QLT-001 (DEC-QLT-004)
- **Acceptance Criteria** (Gherkin): *Given* the unfixed `Logger` and `AbstractController`, *When* the new tests run, *Then* they pass. *Given* the tests of the CMake build, *When* `ctest` runs, *Then* the new executable's tests are discovered and the earlier ones are unchanged. *Given* a mutation of the ratio formula or of the level check (`juce/tools/mutate.py`), *Then* at least one new test fails.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: To be filled at closure (`ctest` counts before and after; the mutation run).
- **Assumptions**: The tests pin today's behaviour; one that cannot be written without a device or a thread the target cannot own is recorded here, with what was pinned instead.

### TASK-QLT-007: `Logger`'s state in one object (`cpp:S5421` x4)
- **Tier**: M
- **Status**: Not Started
- **Description**: In `Logger.cpp`, the file-local mutable `g_mutex`, `g_sink`, `g_level` and `g_domainEnabled` become the members of one struct reached through one function that returns a reference to a function-local object; the public functions use it. Same locking rule, same atomics, no change of the public header.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-005, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-002, DEC-QLT-004)
- **Acceptance Criteria** (Gherkin): *Given* the tests of TASK-QLT-006, *When* `Logger.cpp` is changed, *Then* they pass unmodified. *Given* the library, *When* it builds, *Then* it builds warning-clean on MSVC and GCC (CI) and the public header is unchanged. *Given* `sonar analyze --file juce/framework/src/Logger.cpp`, *Then* `cpp:S5421` is not reported.
- **Dependencies**: TASK-QLT-006
- **Assignee**: AI
- **Verification**: To be filled at closure.
- **Assumptions**: None

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
