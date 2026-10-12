# PLAN-QLT-002: SonarCloud BLOCKER and CRITICAL Issues — the Production Code of `juce/mcp` and the Generators

## Overview

Second plan of FTR-QLT-001 (ADR-QLT-001), written in session QLT on 2026-10-12 because AGNOS plans at most ten tasks per session: PLAN-QLT-001 holds the first ten, this plan the next ten. Together they cover
the 317 production issues of severity BLOCKER and CRITICAL that SonarCloud lists on 2026-10-12 (analysis of `dev` at `937b503`). This plan covers the 302 that PLAN-QLT-001 does not:

| Family | Issues | Tasks |
|---|---|---|
| `cpp:S1669` BLOCKER, the name `module` | 30 | TASK-QLT-011 |
| `cpp:S3608` + `cpp:S5019`, lambda captures (about 121 lambdas) | 241 | TASK-QLT-012, 013, 014 |
| `cpp:S1048`, a destructor that can throw | 1 | TASK-QLT-015 |
| `cpp:S3776` x16 and `cpp:S134` x2 in `juce/mcp` | 18 | TASK-QLT-016, 017, 018 |
| `python:S3776` in `generate_sysex_coverage.py` | 10 | TASK-QLT-019 |
| `pythonsecurity:S2083` BLOCKER | 2 | TASK-QLT-020 |

**Handoff from PLAN-QLT-001.** This plan starts when PLAN-QLT-001 is Done, or when the owner reorders it; none of its tasks depends on one of PLAN-QLT-001's, except that TASK-QLT-020 needs the owner's answer to DEC-QLT-009.
Start the session with the START SESSION steps, read this plan and ADR-QLT-001, and re-read the open issues with the triage tool of TASK-QLT-001 (the lines and the counts will have moved).

**Read before starting each task.** `sonar analyze --file` cannot report the rules on this connection (it only looks for secrets), so a fix is checked by the build and the tests (DEC-QLT-005) and a task says "not checked by Sonar"; re-read the open issues with the triage tool first, and change nothing for a rule that is no longer reported. Every task on `juce/akm` or `juce/mcp` runs `ctest` on the whole suite, which includes the
source checks that read the sources (`CheckNoDestructiveCalls`, `CheckReadmeCoversTools`, `generate_sysex_coverage.py --check`, `SYSEX-COVERAGE.md` unchanged). A task whose change would alter an output, a tool's
reply or a public name stops and asks the owner (RQ-QLT-003).

## References
- **Requirements**: RQ-QLT-001, RQ-QLT-003, RQ-QLT-004, RQ-QLT-005, RQ-QLT-007 (`process/1.requirements/FTR-QLT-001-sonar-blocker-critical.md`); RQ-MCP-011 (layering and maintainability of the server); RQ-AKM-016 (the suite)
- **ADRs**: ADR-QLT-001 (DEC-QLT-002, DEC-QLT-006, DEC-QLT-007, DEC-QLT-008, DEC-QLT-009)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-QLT-011: The name `module` (`cpp:S1669`, 30 BLOCKER)
- **Tier**: M
- **Status**: Done
- **Description**: Rename the parameters and locals called `module` to `moduleIndex` in `juce/akm/include/akm/MultiFxPrimitives.hpp` and `juce/akm/src/MultiFxPrimitives.cpp` (6 + 6), `juce/mcp/include/mcp/SamplerGateway.hpp` (4), `FxCatalogue.hpp` and `FxCatalogue.cpp` (1 + 1), `juce/mcp/src/SamplerGatewayFx.cpp` (5) and `juce/mcp/src/Tools.cpp` (7), with the comments that name them. The names of functions, tools and JSON fields are not touched: a JSON field called `module` that a tool exposes stays as it is, and the task says which ones it found.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-007)
- **Acceptance Criteria** (Gherkin): *Given* the seven files changed, *When* `git diff` is read, *Then* the only differences in the two public headers are parameter names and the comments that name them. *Given* the project, *When* it builds with the tests on, *Then* it builds warning-clean and `ctest` passes with the same number of tests, `SYSEX-COVERAGE.md` unchanged. *Given* `sonar analyze --file` on each of the seven, *Then* `cpp:S1669` is not reported.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: `git diff --stat`: 7 files, 84 insertions and 84 deletions, nothing else changed. Renamed with a small lexer (`rename_module.py`) that touches only the C++ identifier and the backticked `module` of comments: 81 identifier uses and 8 comment mentions; `left_module.py` finds 0 identifier `module` left in the seven files. The string literals of the changed lines are identical before and after (35 and 35, `literals_same.py`): the message texts and the JSON argument name `module` (the constant `ARGUMENT_MODULE = "module"` of `Tools.cpp`) are unchanged. The two public headers (`MultiFxPrimitives.hpp`, `SamplerGateway.hpp`) and `FxCatalogue.hpp` differ only in parameter names and the comments that name them. `cmake --build juce/build --config Debug --parallel`: every target builds warning-clean under `/WX`; `ctest -C Debug -j 8`: 1135/1135 passed (the mutation tool's tests included), with `sysex_coverage_table_is_up_to_date` and `sysex_coverage_tools_match_the_simulated_sampler`; `documents/SYSEX-COVERAGE.md` untouched. Not verified: that SonarCloud no longer reports `cpp:S1669` on the 30 production lines (the analyser cannot report rules on this connection, and the PR analysis covers new code only); GCC and Apple Clang (CI).
- **Assumptions**: The 81 uses of the identifier are the 30 flagged declarations and their uses in the same functions: renaming every use of a flagged name in a file keeps each function consistent. The 11 test-code occurrences of `module` (accepted in SonarCloud, RQ-QLT-002) were left as they are. The JSON argument `module` that the tools expose is a string literal and keeps its name. Checked by the build and the tests, not by Sonar.

### TASK-QLT-012: Lambda captures in `SamplerGateway.cpp` (`cpp:S3608` x65, `cpp:S5019` x64)
- **Tier**: M
- **Status**: Not Started
- **Description**: Every lambda of `juce/mcp/src/SamplerGateway.cpp` that captures with a default (`[&]`, `[=]`) gets the explicit list of what its body uses, by reference unless it outlives the scope, with `this` named when a member is used (DEC-QLT-006). The compiler finds the list: a missing name is a build error. No lambda changes what it does.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-006)
- **Acceptance Criteria** (Gherkin): *Given* the file changed, *When* the project builds with the tests on, *Then* it builds warning-clean on MSVC (locally) and on GCC and Apple Clang (CI) and `ctest` passes, including the scripted conversations of the simulated server and `xs56k_mcp_tests`. *Given* `git diff`, *Then* it shows only capture lists. *Given* `sonar analyze --file juce/mcp/src/SamplerGateway.cpp`, *Then* neither `cpp:S3608` nor `cpp:S5019` is reported.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: To be filled at closure.
- **Assumptions**: None

### TASK-QLT-013: Lambda captures in `SamplerGatewayDisk.cpp` (`cpp:S3608` x30, `cpp:S5019` x30)
- **Tier**: M
- **Status**: Not Started
- **Description**: The same change as TASK-QLT-012, in `juce/mcp/src/SamplerGatewayDisk.cpp`. The disk tools are the slow, opt-in ones: a lambda that outlives its scope (a callback of a slow command) is checked by hand against the code that stores it.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-006)
- **Acceptance Criteria** (Gherkin): *Given* the file changed, *When* the project builds with the tests on, *Then* it builds warning-clean and `ctest` passes, including the disk tools' tests and conversations. *Given* `git diff`, *Then* it shows only capture lists. *Given* `sonar analyze --file juce/mcp/src/SamplerGatewayDisk.cpp`, *Then* neither rule is reported.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: To be filled at closure.
- **Assumptions**: None

### TASK-QLT-014: Lambda captures in `SamplerGatewayFx.cpp`, `SamplerGatewayLists.cpp` and `SamplerGatewayKeys.cpp` (26 + 26)
- **Tier**: M
- **Status**: Not Started
- **Description**: The same change as TASK-QLT-012, in the three smaller files: `cpp:S3608` x12, x9, x5 and `cpp:S5019` x12, x9, x5.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-006)
- **Acceptance Criteria** (Gherkin): *Given* the three files changed, *When* the project builds with the tests on, *Then* it builds warning-clean and `ctest` passes. *Given* `git diff`, *Then* it shows only capture lists. *Given* `sonar analyze --file` on each, *Then* neither rule is reported.
- **Dependencies**: TASK-QLT-011 (it edits `SamplerGatewayFx.cpp` too: do it first, so the two diffs do not collide)
- **Assignee**: AI
- **Verification**: To be filled at closure.
- **Assumptions**: None

### TASK-QLT-015: The destructor of `SamplerGateway` does not throw (`cpp:S1048`, bug)
- **Tier**: S
- **Status**: Not Started
- **Description**: `~SamplerGateway` calls `close()`, which waits on the session and can throw (`std::system_error`, `std::bad_alloc`). The call is wrapped in `try { ... } catch (...) {}` with a comment that says a destructor must not throw and that the close has no one to report to. Five lines.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-002)
- **Acceptance Criteria** (Gherkin): *Given* a connected gateway on the simulated sampler, *When* it is destroyed, *Then* the session is closed as before and `ctest` passes. *Given* `sonar analyze --file juce/mcp/src/SamplerGateway.cpp`, *Then* `cpp:S1048` is not reported.
- **Dependencies**: None (if TASK-QLT-012 is not done yet, the two edit the same file in different places)
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**: None

### TASK-QLT-016: The function of `Tools.cpp` that holds the nesting (`cpp:S3776` at 149, `cpp:S134` x2)
- **Tier**: M
- **Status**: Not Started
- **Description**: In `juce/mcp/src/Tools.cpp`, the function at cognitive complexity 149 (lines 2917 to about 3000, the one that holds the two `cpp:S134`) is cut into named steps (DEC-QLT-008), each under the 25 the rule allows, with the same replies, the same order of gateway calls and the same messages.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-008)
- **Acceptance Criteria** (Gherkin): *Given* the function cut, *When* `xs56k_mcp_tests` and the scripted conversations run, *Then* they pass unmodified. *Given* the source checks, *Then* `generate_sysex_coverage.py --check` and `CheckNoDestructiveCalls` pass and `SYSEX-COVERAGE.md` is unchanged. *Given* `sonar analyze --file juce/mcp/src/Tools.cpp`, *Then* the function is not reported for `cpp:S3776` and `cpp:S134`.
- **Dependencies**: TASK-QLT-011 (it edits `Tools.cpp` too)
- **Assignee**: AI
- **Verification**: To be filled at closure.
- **Assumptions**: None

### TASK-QLT-017: The other functions of `Tools.cpp` (`cpp:S3776` x11)
- **Tier**: M
- **Status**: Not Started
- **Description**: The eleven other functions of `juce/mcp/src/Tools.cpp` over the limit of 25 (complexities from 31 to 236, four of them above 140) are cut into named steps (DEC-QLT-008). Done in groups of a few functions, with `ctest` after each group.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-008)
- **Acceptance Criteria** (Gherkin): *Given* each group cut, *When* `ctest` runs, *Then* it passes unmodified and `SYSEX-COVERAGE.md` is unchanged. *Given* `sonar analyze --file juce/mcp/src/Tools.cpp`, *Then* `cpp:S3776` is not reported.
- **Dependencies**: TASK-QLT-016
- **Assignee**: AI
- **Verification**: To be filled at closure.
- **Assumptions**: If the file grows past what a reviewer can read in one diff, the group is committed in parts, each with the ID of this task.

### TASK-QLT-018: `SamplerGateway.cpp` and `ServerOptions.cpp` (`cpp:S3776` x3 and x1)
- **Tier**: M
- **Status**: Not Started
- **Description**: Three functions of `juce/mcp/src/SamplerGateway.cpp` and one of `juce/mcp/src/ServerOptions.cpp` over the limit of 25 are cut into named steps (DEC-QLT-008). The option parser keeps its messages word for word: the README and the `--help` text depend on them.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-008)
- **Acceptance Criteria** (Gherkin): *Given* the four functions cut, *When* `ctest` runs, *Then* it passes unmodified, including `CheckReadmeCoversTools`. *Given* a bad option on the command line, *When* the server starts, *Then* its message is the same as before. *Given* `sonar analyze --file` on the two files, *Then* `cpp:S3776` is not reported.
- **Dependencies**: TASK-QLT-012, TASK-QLT-015 (same file)
- **Assignee**: AI
- **Verification**: To be filled at closure.
- **Assumptions**: None

### TASK-QLT-019: Cognitive complexity of `generate_sysex_coverage.py` (`python:S3776` x10)
- **Tier**: M
- **Status**: Not Started
- **Description**: The ten functions of `juce/tools/generate_sysex_coverage.py` over the limit of 15 (one at 57) are cut into named steps (DEC-QLT-008). The table, `SYSEX-COVERAGE.md`, and the output of `--check` do not change.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-008)
- **Acceptance Criteria** (Gherkin): *Given* the script changed, *When* it is run to write the table, *Then* `git diff documents/SYSEX-COVERAGE.md` is empty, and `--check` passes. *Given* `test_generate_sysex_coverage.py`, *When* it runs, *Then* it passes unmodified. *Given* `sonar analyze --file juce/tools/generate_sysex_coverage.py`, *Then* `python:S3776` is not reported.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: To be filled at closure.
- **Assumptions**: None

### TASK-QLT-020: Path traversal of the two generators (`pythonsecurity:S2083`, 2 BLOCKER)
- **Tier**: S
- **Status**: Not Started
- **Description**: Ask the owner the question of DEC-QLT-009 and do what he answers: accept the two issues in SonarCloud with his reason (recommended: developer tools run at a shell, which write where they are told, and whose tests write to a temporary directory), or add a check that `--output` is inside the repository or the system temporary directory, in `generate_sysex_coverage.py` and `generate_akm_items.py`. Accepting is one call of the triage tool or `sonar api`, and changes SonarCloud's state: only on his go.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-003
- **ADR refs**: ADR-QLT-001 (DEC-QLT-009)
- **Acceptance Criteria** (Gherkin): *Given* the owner's answer "accept", *When* the two issues are read in SonarCloud, *Then* they are `ACCEPTED` with his reason in the comment. *Given* the answer "check the path", *When* `generate_*.py --output /elsewhere/x` runs, *Then* it refuses, `ctest` passes, and `sonar analyze --file` on the two files reports no `pythonsecurity:S2083`.
- **Dependencies**: None
- **Assignee**: Human (the decision), AI (the change)
- **Verification**: N/A (Tier S)
- **Assumptions**: None
