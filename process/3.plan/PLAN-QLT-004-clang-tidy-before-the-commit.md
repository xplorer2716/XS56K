# PLAN-QLT-004: clang-tidy Before the Commit

## Overview

Implements FTR-QLT-002 (ADR-QLT-002): the configuration, the tool and its tests, the production code made clean, and the skill that makes every task use it. Written on 2026-10-16
on the owner's request ("spécifie et implémente la solution clang tidy, rends-la disponible pour l'IA dans le cycle").

## References
- **Requirements**: RQ-QLT-009 to RQ-QLT-013 (`process/1.requirements/FTR-QLT-002-local-static-analysis.md`); RQ-QLT-004 (a fix keeps the behaviour)
- **ADRs**: ADR-QLT-002 (DEC-QLT-010 to DEC-QLT-014); ADR-QLT-001 (DEC-QLT-001)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-QLT-024: The configuration, the tool and its tests
- **Tier**: M
- **Status**: Done
- **Description**: `juce/.clang-tidy` (DEC-QLT-010) and `juce/tools/tidy.py` (DEC-QLT-011, DEC-QLT-012), with `juce/tests/tools/test_tidy.py` and its ctest entry.
- **Requirement refs**: RQ-QLT-009, RQ-QLT-010, RQ-QLT-013
- **ADR refs**: ADR-QLT-002 (DEC-QLT-010, DEC-QLT-011, DEC-QLT-012)
- **Acceptance Criteria** (Gherkin): see RQ-QLT-009, RQ-QLT-010 and RQ-QLT-013.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: `juce/.clang-tidy` (14 checks, each with its Sonar rule; exception-escape limited to destructors and `noexcept` functions), `juce/tools/tidy.py` (`--all`, `--tests`, `--fix`, `--configure`), `test_tidy.py` 14 tests OK, ctest entry `qlt_tidy_script_tests`. Configured `juce/build-tidy` with Ninja inside `vcvars64` (Visual Studio 18's clang-tidy, LLVM 22.1.3) and the `XS56KAppFonts` target; `tidy.py --all`: 85 production sources in about 27 s. Findings of JUCE's own headers are dropped (a test checks it). The IDE's SonarQube plugin reports nothing on `tidy.py` and `test_tidy.py`.
- **Assumptions**: The tool was written before its tests (the AGNOS test-first rule was not followed for this task; the tests were then written against the scenarios of RQ-QLT-010). The compile commands carry no `/EHsc`: the tool adds it for an MSVC command rather than changing the project's flags.

### TASK-QLT-025: The production code clean against it
- **Tier**: M
- **Status**: Done
- **Description**: `tidy.py --all` run on the production code; each finding fixed without a change of behaviour, or kept with a `NOLINT(check)` and its reason.
- **Requirement refs**: RQ-QLT-011, RQ-QLT-004
- **ADR refs**: ADR-QLT-002 (DEC-QLT-010)
- **Acceptance Criteria** (Gherkin): see RQ-QLT-011; *and* the suite passes unmodified, the build is warning-clean.
- **Dependencies**: TASK-QLT-024
- **Assignee**: AI
- **Verification**: `tidy.py --all` first gave 72 findings once `exception-escape` was limited (109 before): `--fix` corrected 45 in 17 files (`ranges` x30, `emplace_back` x5, `const&` x5, a useless `std::move` x3, `contains` x2...), and 27 were fixed by hand: 20 C arrays to `std::to_array` (and the three templates of `Tools.cpp` that took them), the destructors of `Session`, `ConfirmationContent`, `LogSession`, `MockInputPort` and `MockOutputPort` that could let an exception out (each failure now written to the log or to `stderr`), the empty `catch` of `~SamplerGateway`, and `std::stop_token` by `const&`. No `NOLINT` was needed. Also `SamplerWorker` holds a `std::jthread` (Sonar `cpp:S6168`, accepted on PR 18 for a wrong reason: the framework already uses `std::jthread` on every CI platform). `cmake --build juce/build --config Debug`: warning-clean under `/WX`; `ctest -C Debug -j 8`: 1221/1221 passed, no test modified; `tidy.py --all`: 85 sources, 0 findings. Not verified: GCC and Apple Clang (CI).
- **Assumptions**: The fixes keep the behaviour: the automatic ones are clang-tidy's equivalences, the manual ones change no value or order (a `std::to_array` is the same table; a destructor that caught nothing before now catches and reports).

### TASK-QLT-026: The skill and `AGENTS.md`
- **Tier**: S
- **Status**: Done
- **Description**: `.github/skills/static-analysis/SKILL.md` (the procedure, the IDE step, the idioms of the rules clang-tidy lacks), its pointer `.claude/skills/static-analysis/SKILL.md`, one line in `AGENTS.md`'s commands.
- **Requirement refs**: RQ-QLT-012
- **ADR refs**: ADR-QLT-002 (DEC-QLT-013, DEC-QLT-014)
- **Acceptance Criteria** (Gherkin): see RQ-QLT-012.
- **Dependencies**: TASK-QLT-024
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**: None
