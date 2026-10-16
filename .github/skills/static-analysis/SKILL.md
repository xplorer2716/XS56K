---
name: static-analysis
description: "Check the C++ and Python code a task changed before its commit: clang-tidy with the project's Sonar-matching checks (juce/tools/tidy.py), then the IDE's SonarQube findings, and the idioms of the Sonar rules clang-tidy cannot check. USE FOR: every task that changed code under juce/, before the commit; a whole-code check with --all. Not part of AGNOS."
argument-hint: "[files...] [--all] [--fix] [--tests] [--configure]"
---

# Static analysis before the commit

Canonical procedure, shared by GitHub Copilot (`.github/skills/static-analysis/`) and Claude Code (`.claude/skills/static-analysis/`, which points here).
[RQ-QLT-009 to RQ-QLT-013, ADR-QLT-002 (DEC-QLT-010 to DEC-QLT-014)]

SonarCloud (free plan, not tunable) sees the code only at the pull request. This finds the same kinds of issues on the task's own diff, before the commit.

## When

Before the commit of every task that changed C++ or Python code under `juce/`. Cite the result in the task's `Verification` ("tidy: N sources, 0 findings").

## Steps

1. **clang-tidy on what changed**: `python juce/tools/tidy.py` (`python3` on Linux and macOS). It checks the production files changed against the merge base
   with `origin/dev` (a header through the sources that include it), with the checks of `juce/.clang-tidy`. Exit 0: clean; 1: findings; 2: the tool could not run
   (it says why). The first run configures `juce/build-tidy` (a Ninja compile database, a minute or two; `--configure` redoes it after a change of CMake files).
2. **Fix each finding**, then run again until it exits 0:
   - `--fix` applies the fixes clang-tidy offers (ranges, `emplace_back`, `contains`, `auto`, `const&`, a useless `std::move`); review the diff, build, run the suite.
   - The others are fixed by hand. A finding kept on purpose takes `// NOLINT(<check>): <reason>` on its line; never a bare `NOLINT`.
   - Never weaken `juce/.clang-tidy` to make a finding go: a change of the checks is the owner's decision.
3. **The IDE's SonarQube findings** (Claude Code in VS Code): open the changed files (`code -r <files>`), then read the Problems panel with the
   `getDiagnostics` tool of the IDE; keep the entries whose `source` is `sonarqube`. The plugin applies the real Sonar rules, Python included, but only to open files.
   Fix them as in step 2. Skip this step when no IDE is connected, and say so.
4. **Build and the whole suite** pass after the fixes, as for any change.

Test code (`juce/tests/`) is not checked by default (DEC-QLT-001); `--tests` includes it, for the automatic fixes only, on the owner's request.

## Idioms of the Sonar rules clang-tidy cannot check

Write them right the first time; step 3 is the only check of these:

| Rule | Write |
|---|---|
| `cpp:S7172` | `x.has_value()` to test an `std::optional`, never `if (x)` or `x && ...` (a function returning an optional too) |
| `cpp:S6004` | a variable used only by an `if` is declared in its init-statement: `if (const auto x = f(); x.ok())` |
| `cpp:S6177` | `using enum E;` at the top of a function that names `E::` several times (a `switch`) |
| `cpp:S6225`, `cpp:S3491` | `std::to_address(it)`, never `&*it` |
| `cpp:S1188` | a lambda of 20 lines at most: move its body into a named function |
| `cpp:S6045` | `std::map<std::string, T, std::less<>>`, `std::set<std::string, std::less<>>` |
| `cpp:S3574` | no `-> T` on a lambda whose returns already say `T` |
| `cpp:S108`, `cpp:S2486` | a `catch` handles the exception: log it (in a destructor, through something that cannot throw, e.g. `std::fputs` to `stderr`) |
| `python:S3776` | a Python function of cognitive complexity 15 at most: named steps |
| `python:S1192` | a literal used three times or more is a named constant |
| `python:S8786` | no regular expression with adjacent quantifiers over the same characters (`\s*;` in `re.split`: use `str.split` then `strip()`) |
| `pythonsecurity:S2083`, `S8707` | a path from the command line is resolved and checked to be inside the folder it may touch |

Not used on purpose: `std::format` (the Linux CI's GCC 11 has no `<format>`; `midiapp::service::formatStr` is the portable form) and `std::to_underlying` (C++23).
