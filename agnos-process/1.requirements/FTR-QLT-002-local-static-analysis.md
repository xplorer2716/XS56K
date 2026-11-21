# FTR-QLT-002: Local Static Analysis Before the Commit

## Overview

Session APP (2026-10-16), after pull request 18. SonarCloud analysed the editor's code only at the pull request, after some twenty tasks: 196 issues, then 53, then 4, then 5 more
from the IDE's plugin, partly made by the fixes of the pass before (PLAN-QLT-003). The owner's SonarCloud plan is the free one: its quality profile and its exclusions cannot be
set the way the project would need. The owner asked for a local check, run by the AI in its cycle from now on, that finds the same kinds of issues before the commit.

**In scope.** A clang-tidy configuration whose checks match the SonarCloud rules met on this project; a tool that runs it on what changed; the production code made clean against it; the
procedure that makes every task run it (a skill, `AGENTS.md`); the rules clang-tidy has no check for, read from the IDE's SonarQube plugin.
**Out of scope.** Running it in CI (a later step, once its noise is known); test code (DEC-QLT-001); Python linting beyond what the IDE's plugin shows.

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the AI sessions that implement tasks; SonarCloud's analysis of the next pull requests.

---

## Functional Requirements

### RQ-QLT-009: A clang-tidy configuration of the Sonar rules met
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The project SHALL hold one clang-tidy configuration, `juce/.clang-tidy`, whose checks are those that match the SonarCloud rules raised on the project (each check naming its rule in a comment), plus a few bug-finding checks, with every finding an error, and that reports nothing on the code of JUCE or on generated files.
- **Rationale**: the same issues found before the commit rather than after the pull request.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* `juce/.clang-tidy`, *When* it is read, *Then* each check has a comment naming its Sonar rule or "bug finder". *Given* a file that includes a JUCE header with a finding of its own, *When* the tool runs on the file, *Then* only findings in the project's files are reported.
- **Dependencies**: ADR-QLT-002 (DEC-QLT-010)

### RQ-QLT-010: A tool that checks what changed
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN `juce/tools/tidy.py` runs, the tool SHALL check the production C++ files changed against the base branch (committed, staged, unstaged or new), a changed header through the sources that include it, or the files named, or with `--all` every production source; it SHALL print each finding once with its file and line, a count by check, and exit 0 when clean, 1 with findings, 2 when it cannot run.
- **Rationale**: the check of a task is fast when it reads what the task changed.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a change to `juce/app/core/src/Edit.cpp` only, *When* the tool runs with no argument, *Then* it checks that file and no other. *Given* a change to a header, *When* it runs, *Then* it checks the sources that include it. *Given* no compile database, *When* it runs, *Then* it configures `juce/build-tidy` first, or exits 2 saying how. *Given* a file of `juce/tests/`, *When* it is the only change, *Then* nothing is checked.
- **Dependencies**: ADR-QLT-002 (DEC-QLT-011, DEC-QLT-012)

### RQ-QLT-011: The production code is clean
- **Category**: Functional
- **EARS Type**: State-driven
- **Statement**: WHILE the configuration is in place, `tidy.py --all` SHALL report no finding on the production code; a finding not fixed SHALL carry a `NOLINT(check)` with its reason on the same line.
- **Rationale**: a check that starts clean can be required of every task.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the branch, *When* `tidy.py --all` runs, *Then* it exits 0, and every `NOLINT` of `juce/` but `juce/tests/` names its check and says why.
- **Dependencies**: RQ-QLT-009, RQ-QLT-010, RQ-QLT-004

### RQ-QLT-012: Every task runs it before its commit
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a task that changed C++ or Python code of `juce/` is about to be committed, the AI SHALL run the static-analysis skill (clang-tidy on the changed files, then the IDE's SonarQube findings on the changed files when the IDE is reachable) and fix or justify each finding first; `AGENTS.md` SHALL name the skill.
- **Rationale**: the owner's request: the check is part of the cycle, not an afterthought.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* `AGENTS.md`, *When* it is read, *Then* its commands name `tidy.py` and the skill. *Given* a task's Verification, *When* it changed C++ code, *Then* it cites the tool's result.
- **Dependencies**: RQ-QLT-010; ADR-QLT-002 (DEC-QLT-013, DEC-QLT-014)

## Non-Functional Requirements

### RQ-QLT-013: The tool is tested
- **Category**: Non-Functional
- **NFR Type**: Maintainability
- **EARS Type**: Ubiquitous
- **Statement**: The tool's selection of files, its filter of the findings and its exit status SHALL be covered by unit tests that do not need clang-tidy, run by `ctest`.
- **Metric**: a ctest entry, passing.
- **Measurement Method**: `ctest -R qlt_tidy`.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a replaced clang-tidy, *When* the tests run, *Then* they pass on every platform.
- **Dependencies**: RQ-QLT-010
