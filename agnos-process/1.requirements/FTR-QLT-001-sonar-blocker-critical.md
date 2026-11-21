# FTR-QLT-001: SonarCloud BLOCKER and CRITICAL Issues

## Overview

Session QLT (2026-10-11, updated 2026-10-12). SonarCloud holds 1 797 open issues on the project `xplorer2716_XS56K`, among them 46 BLOCKER and 381 CRITICAL
(counts read with the `sonar` CLI, `sonar api get /api/issues/search`; the other 1 370 are MAJOR or MINOR and are not part of this feature).
This feature gets the project to no open BLOCKER or CRITICAL issue, in two ways decided by the owner:

- **Test code is not fixed, it is accepted** in SonarCloud with a comment that says why: the test code is edited by automated
  changes, and its style rules are not worth that churn. 110 of the 427 issues are on test code.
- **Production code is fixed**, without a change of behaviour. 317 of the 427 issues, 273 of them in `juce/mcp/src`:
  - 241 about lambdas (`cpp:S3608` x121, `cpp:S5019` x120: the same 121 or so lambdas of `juce/mcp/src/SamplerGateway*.cpp`, flagged by two rules);
  - 30 BLOCKER `cpp:S1669` (a parameter named `module`, a C++20 contextual keyword), in `juce/akm` and `juce/mcp`;
  - 30 on complexity and nesting (`cpp:S3776` x16 and `cpp:S134` x3 in C++, `python:S3776` x14);
  - 2 BLOCKER `pythonsecurity:S2083` (path traversal through `--output` of two generators);
  - 14 isolated ones: `cpp:S5404` (BLOCKER, a bug: `RealScheduler`), `cpp:S1048` (a bug: `SamplerGateway`'s destructor), `cpp:S3491` x2 (BLOCKER), `cpp:S5421` x4,
    `cpp:S3624` x2, `cpp:S1186`.

**The analysis is current, for now.** The project was deleted and created again on SonarCloud on 2026-10-12; its analysis of that day is of revision `937b503`, the head of `dev`
(the branch `feature/QLT` differs from it only by documents and `.gitignore`), so the lines SonarCloud gives are those of the files. It will go stale as `dev` moves, and how SonarCloud
is fed is still not known (no `sonar-project.properties`, no workflow names it). An issue is nevertheless located by its rule and its function, not by its line, since a fix moves the lines.

**In scope.** The 427 issues of severity BLOCKER and CRITICAL listed by SonarCloud on 2026-10-12, and a tool that classifies and accepts the ones on test code.
**Out of scope.** The 1 370 MAJOR and MINOR issues, among them the 4 `pythonsecurity:S8707` and the `cpp:S2245` vulnerabilities (MAJOR); making SonarCloud analyse `dev` again.

**Size.** The work does not fit in one session: AGNOS plans at most 10 tasks per session. `PLAN-QLT-001` holds the first ten (the tool, the acceptance, and the small and framework fixes),
`PLAN-QLT-002` the next ten (the `module` names, the lambdas, the large functions, the path traversal).

**Test code** means every file under `juce/tests/` and under `.github/skills/<skill>/tests/`. The mock backend `juce/midi/src/MockMidiBackend.cpp`
is a file of the `xs56k_midi` library, so it counts as production code (assumption, TASK-QLT-003).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: SonarCloud's quality view of the project; whoever reads its issue list; the AGNOS sessions that fix further issues.

---

## Functional Requirements

### RQ-QLT-001: No open BLOCKER or CRITICAL issue outside the test code
- **Category**: Functional
- **EARS Type**: State-driven
- **Statement**: WHILE the project is analysed by SonarCloud, the project SHALL have no open issue of severity BLOCKER or CRITICAL on a file that is not test code.
- **Rationale**: the owner's goal for this feature; a BLOCKER is a bug or a defect that is certain to bite, a CRITICAL one that is likely to.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a SonarCloud analysis of the branch that holds the fixes, *When* the open issues of severity BLOCKER and CRITICAL are listed with the triage tool, *Then* the production-code count is 0 (317 on 2026-10-12). *Given* the analysis that has not run since the fixes, *When* only `sonar analyze --file` is available, *Then* the criterion is reported "checked locally, not by SonarCloud" and the requirement is not closed.
- **Dependencies**: RQ-QLT-004, RQ-QLT-007; ADR-QLT-001 (DEC-QLT-002, DEC-QLT-005)

### RQ-QLT-002: Test-code issues are accepted, with a reason
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN an open issue of severity BLOCKER or CRITICAL is on test code, the project SHALL accept it in SonarCloud with a comment that names RQ-QLT-002 and says that test code is not fixed for style rules, and SHALL NOT change the test code for it.
- **Rationale**: owner decision of 2026-10-11: the test code is the subject of many automated changes.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the 110 open BLOCKER and CRITICAL issues on files under `juce/tests/` or `.github/skills/*/tests/`, *When* the acceptance has run, *Then* each has the status `ACCEPTED`, a comment that names RQ-QLT-002, and `git diff` shows no change to a test file for it. *Given* a production-code issue, *When* the acceptance runs, *Then* it is not touched.
- **Dependencies**: RQ-QLT-006; ADR-QLT-001 (DEC-QLT-001)

### RQ-QLT-003: A production issue left open is accepted only by the owner
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a production-code issue cannot be fixed without a change of behaviour, or its fix cannot be verified, THEN the project SHALL leave the decision to the owner and, when he decides to accept it, SHALL accept it in SonarCloud with a comment that gives his reason, and the task's Assumptions SHALL name the issue and the decision.
- **Rationale**: an unexplained acceptance hides a defect; a fix that cannot be verified may add one.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a production issue the owner has decided to accept, *When* it is read in SonarCloud, *Then* it is `ACCEPTED` with his reason in the comment. *Given* a production issue with no such decision, *When* the plan is closed, *Then* it is fixed.
- **Dependencies**: RQ-QLT-001; ADR-QLT-001 (DEC-QLT-002)

### RQ-QLT-004: A fix keeps the behaviour
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN production code is changed for an issue, the project SHALL keep its behaviour: the test suite of the CMake build passes with no test modified, and a generated output of the changed generator (`ItemTable.generated.hpp`, `SYSEX-COVERAGE.md`, `sysex_spec.items.tsv`, `sysex_spec.clean.txt`) is byte-identical to the committed one, and the source checks of the build (`CheckNoDestructiveCalls`, `CheckReadmeCoversTools`, `generate_sysex_coverage.py --check`), which read the sources, still pass.
- **Rationale**: the issues are about the shape of the code, not about what it does.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a fix, *When* `ctest` runs on the whole suite, *Then* it passes with the same number of tests or more, and `git diff` shows no deletion in a test. *Given* a change of a generator, *When* it is run and the output compared with the committed file, *Then* they are identical.
- **Dependencies**: RQ-QLT-005; RQ-AKM-016

### RQ-QLT-005: Code with no test gets a characterization test first
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a fix changes code that no test of the repository exercises (today `juce/framework`, `juce/midi` and `documents/_index`), the project SHALL write, before the fix, a test that pins the behaviour the fix must keep, and that test SHALL pass before and after the fix.
- **Rationale**: AGNOS requires a test for a Tier M change (test first); a behaviour-preserving refactor with no test would only be a hope.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* `Logger` and `AbstractController`, which have no test, *When* a fix is planned, *Then* a Gherkin-named test exists in a target built by the CMake tests and it passes on the unfixed code. *Given* `documents/_index/build_sysex_index.py`, which needs `pymupdf` and the PDF, *When* no unit test is possible, *Then* the byte-identical output of RQ-QLT-004 is the test, and the task says so.
- **Dependencies**: RQ-QLT-004; ADR-QLT-001 (DEC-QLT-004)

### RQ-QLT-006: A tool classifies and accepts, and prints counts, not data
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The project SHALL have a script in `juce/tools` that reads the open issues of a given severity from SonarCloud through the `sonar` CLI, classifies each as test code or production code by its path, prints counts by rule and by file and the list of production issues, and, only when asked, accepts the test-code ones; it SHALL write the raw data to a file outside the repository and never print it whole.
- **Rationale**: the owner asked for a tool rather than the data in an assistant's context; the acceptance must be repeatable, since a later analysis can add test-code issues.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a recorded SonarCloud reply with issues on `juce/tests/`, on `.github/skills/*/tests/`, and elsewhere, *When* the script classifies it, *Then* each is counted on the right side, and the real reply gives 110 test and 317 production. *Given* no `--apply`, *When* the script runs, *Then* it sends no transition and says what it would send. *Given* `--apply`, *When* an issue is already accepted, *Then* it is skipped.
- **Dependencies**: RQ-QLT-002; ADR-QLT-001 (DEC-QLT-003)

### RQ-QLT-007: A fix is checked by the analyser
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a fix is delivered, the project SHALL check it by the build and the whole test suite and SHALL say that the rule of the issue has not been checked by Sonar, until `sonar analyze --file` can report the rules on the connection (on 2026-10-12 it only looks for secrets); and WHEN SonarCloud has analysed the branch, the project SHALL re-read the open issues with the tool of RQ-QLT-006 before RQ-QLT-001 is closed.
- **Rationale**: the SonarCloud analysis is of the day the project was recreated and is not known to follow pushes; the local analysis is the only immediate signal.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a fixed file, *When* the build and `ctest` run, *Then* they pass, and the task says whether `sonar analyze --file <it>` could report the rule (not on 2026-10-12). *Given* a SonarCloud analysis dated after the fixes, *When* the tool lists the BLOCKER and CRITICAL issues, *Then* the production count is 0.
- **Dependencies**: RQ-QLT-001; ADR-QLT-001 (DEC-QLT-005)

---

## Non-Functional Requirements

### RQ-QLT-008: The acceptance is repeatable
- **Category**: Non-Functional
- **NFR Type**: Reliability
- **EARS Type**: Ubiquitous
- **Statement**: The acceptance of RQ-QLT-002 SHALL be idempotent: running it again on the same SonarCloud state sends no request that changes an issue.
- **Metric**: number of transitions sent by a second run = 0.
- **Measurement Method**: run the tool with `--apply` twice; the second run reports 0 issues to accept (unit test with a recorded reply in which the issues are `ACCEPTED`, and the real run).
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a state in which every test-code issue is `ACCEPTED`, *When* the tool runs with `--apply`, *Then* it sends no transition and exits 0.
- **Dependencies**: RQ-QLT-002, RQ-QLT-006
