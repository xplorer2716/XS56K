# PLAN-QLT-003: SonarCloud Quality Gate of Pull Request 18

## Overview

Third plan of FTR-QLT-001 (ADR-QLT-001), written on 2026-10-16 after the owner pushed the editor work (PLAN-APP-005, PLAN-APP-006) in pull request 18 and its quality gate failed: reliability rating D and security rating C (maintainability, duplication and hotspots pass). Audit of the PR's 196 open issues (152 production, 44 test code), read with `sonar api get /api/issues/search?pullRequest=18` and facets, not file by file. The owner's order (2026-10-16): fix all the production code, in the order of the audit; push step 1, then do step 2.

| Step | Issues | Task |
|---|---|---|
| 1. The gate: 3 `cpp:S1048` bugs (1 production, 2 in test support) and 3 `pythonsecurity:S8707` in `new_source_file.py` | 6 | TASK-QLT-021 |
| 2. The other BLOCKER and CRITICAL of production code: `cpp:S3491` x2, `cpp:S1186` x4, `python:S3776` x1, `python:S1192` x2 | 9 | TASK-QLT-022 |
| 3. The MAJOR and MINOR of production code, by rule family (`cpp:S6197`, `S7172`, `S6177`, `S6004`, `S6185`...) | about 140 | TASK-QLT-023 |

Test-code issues are accepted by path rule, not fixed (DEC-QLT-001), except the two of step 1, which count in the gate. `sonar analyze --file` cannot report the rules on this connection: a fix is checked by the build and the tests, and SonarCloud's own analysis, which the owner refreshes, has the last word (DEC-QLT-005).

## References
- **Requirements**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007 (`process/1.requirements/FTR-QLT-001-sonar-blocker-critical.md`)
- **ADRs**: ADR-QLT-001 (DEC-QLT-002, DEC-QLT-005, DEC-QLT-008)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-QLT-021: The quality gate of PR 18 — three destructors and the path of `new_source_file.py`
- **Tier**: M
- **Status**: Done
- **Description**: The destructors of `ConfirmationContent` (`JuceAdapters.cpp`) and of the two fake ports of `AppTestSupport.hpp` no longer let an exception out (`cpp:S1048`). `new_source_file.py` writes and checks only a `.cpp`, `.hpp` or `.h` file under `juce/` (`pythonsecurity:S8707`), and is tested.
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-002)
- **Acceptance Criteria** (Gherkin): *Given* the destructors changed, *When* the suite runs, *Then* it passes unmodified. *Given* `new_source_file.py juce/../x.cpp`, *Then* it refuses, writes nothing, and exits 1. *Given* a path under `juce/`, *Then* the file is created as before.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Changed: `JuceAdapters.cpp` (`~ConfirmationContent`), `AppTestSupport.hpp` (`~Input`, `~Output`), `new_source_file.py` (`source_path`) and new `test_new_source_file.py` (7 tests, ctest entry `qlt_new_source_file_script_tests`). `cmake --build juce/build --config Debug --parallel`: warning-clean under `/WX`; `ctest -C Debug -j 8`: 1220/1220 passed (1219 before plus the new entry), no test modified. Not verified: that SonarCloud no longer reports `cpp:S1048` x3 and `pythonsecurity:S8707` x3 (the owner refreshes the analysis; the taint rule may not recognise `resolve()` plus `is_relative_to` as a sanitiser); GCC and Apple Clang (CI).
- **Follow-up (2026-10-16)**: after the owner's analysis of PR 18, SonarCloud kept one `pythonsecurity:S8707`: the `--body` file read at `new_source_file.py:62`; the target path was recognised as bounded, the body was not. `body_path` now accepts only a file of the repository or of the system's temporary directory (where a session's scratch files are), and a refused body is reported like a refused target instead of a traceback; 4 tests added (11 in the file, OK). The reliability condition of the gate passed (rating A).
- **Assumptions**: The tool is tightened rather than accepted: the change is short, and a path given by an assistant is the very case of the rule. Its target must be under `juce/` with a C++ suffix, which is every use in `.github/skills/new-source-file/SKILL.md`.

### TASK-QLT-022: The other BLOCKER and CRITICAL of production code
- **Tier**: M
- **Status**: Done
- **Description**: `&*` removed in `EditController.cpp` (`cpp:S3491` x2); a comment in the five empty methods of `Controller.cpp`, `FileBrowser.cpp` x2 and `MemoryController.cpp` (`cpp:S1186`, the fifth being test code); `generate_design.py`'s layout rendering cut into named steps (`python:S3776`, 46 to 15 or less) and the duplicated literals of `generate_design.py` and `generate_mockups.py` named (`python:S1192`).
- **Requirement refs**: RQ-QLT-001, RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-002, DEC-QLT-008)
- **Acceptance Criteria** (Gherkin): *Given* the changes, *When* `generate_design.py` and `generate_mockups.py` run, *Then* `git diff` of the generated header, the design document and the mockups is empty, and `--check` passes. *When* the suite runs, *Then* it passes unmodified.
- **Dependencies**: TASK-QLT-021
- **Assignee**: AI
- **Verification**: `&*` is `std::to_address(found)` in `EditController.cpp` (as TASK-QLT-004); the four empty lambdas of `Controller.cpp`, `FileBrowser.cpp` x2 and `MemoryController.cpp` carry a comment saying why. `generate_design.py`: `render_layouts` cut into `render_parameters`, `render_scope`, `render_group`, `render_tab`, `render_orders` and `render_editor`; the `"    }};"` literal is `CLOSE_TABLE`; `generate_mockups.py` names `ENVELOPE_HEIGHT`. `generate_design.py` and `generate_mockups.py` run: `git status` shows no change to the generated header, the design document or the mockups. Complexity by the `cognitive_complexity` package: highest function of `generate_design.py` is now 21 (`resolve_all`, not reported by SonarCloud), `render_envelope` 14, the new functions under 15 (`render_layouts` was 46 by SonarCloud). `test_generate_design.py` 21 tests and `test_generate_mockups.py` 18 tests: OK. Build warning-clean; `ctest -C Debug -j 8`: 1220/1220 passed. Not verified: that SonarCloud no longer reports these nine rules (the owner refreshes the analysis); the test-code `&*` (`EditorLayoutTests.cpp:35`) and empty method (`CoreTests.cpp:229`) are left to the path rule of DEC-QLT-001.
- **Assumptions**: A comment inside an empty lambda is what `cpp:S1186` asks for ("add a nested comment"), so no behaviour changed.

### TASK-QLT-023: The MAJOR and MINOR of production code, by rule family
- **Tier**: M
- **Status**: Done
- **Description**: The rest of the 152 production issues, one rule family at a time, re-read with the triage tool first (the lines will have moved). A change that would alter an output stops and asks the owner (RQ-QLT-003).
- **Requirement refs**: RQ-QLT-004, RQ-QLT-007
- **ADR refs**: ADR-QLT-001 (DEC-QLT-002)
- **Acceptance Criteria** (Gherkin): *Given* a family fixed, *When* the build runs, *Then* it is warning-clean, and *When* the suite runs, *Then* it passes unmodified.
- **Dependencies**: TASK-QLT-022
- **Assignee**: AI
- **Verification**: Fixed, by rule: `cpp:S7172` x24 (`has_value()`), `S6197` x23 (`std::ranges::` algorithms, `ranges::mismatch(...).in1`), `S6177` x16 (`using enum` in 16 functions), `S6004` x13 (declaration moved into the `if`; three over-applied by the script were put back by hand after the compiler refused them), `S6003` x6 (`emplace_back`), `S6225` x5 (`std::to_address`), `S5945` x4 (`std::array`), `S1238` (`const&` for `MemoryController::read`, `Model::memoryChanged`, `Controller::queue`), `S1188` x2 (`EditController.cpp`: `showsScreen`, `withReading`, `withWritten`), `S6045` x2 (`std::less<>`), `S6009` x2, `S6171` x2 (`contains`), `S3574` x2, `S5827` x2, `S1103` x2 (also in the generator), `S5205`, `S6012`, `S5566`, `S5817`, `S6179`, and `python:S5713`, `python:S1871`. `cmake --build`: warning-clean under `/WX`; `ctest -C Debug -j 8`: 1220/1220 passed unmodified; `generate_design.py --check` and `generate_mockups.py --check`: up to date, generated files unchanged but for the header comment of `S1103`. Not fixed, with the reason: `cpp:S6185` x10 + 1 in `juce/mcp` (`std::format`: not usable on Apple Clang's libc++ of the macOS runner, not testable here), `cpp:S6168` (`std::jthread`: same), `cpp:S7035` x2 (`std::to_underlying` is C++23, the project is C++20), `cpp:S3642` x4 (the unscoped menu ids are `int`s given to JUCE's menus; an `enum class` would need a cast at every use), `cpp:S107` x4 (three are JUCE `LookAndFeel` overrides whose signature is fixed, one is `MainComponent`'s constructor that wires the application). Not verified: that SonarCloud no longer reports the fixed rules (the owner refreshes the analysis); GCC and Apple Clang (CI) for `using enum` and `emplace_back` of aggregates.
- **Follow-up (2026-10-16)**: the gate passed (reliability A, security A). The analysis then showed 53 new issues, 8 in production code and 45 in test code, all of rules the first pass had not met in code that changed since: fixed in production: `cpp:S1188` (`EditController::writeWaiting` out of the write lambda), `S1238` x2 (`FileBrowser::openFolder` and `MemoryController::select` take a `const&`), `S5415` (the `std::move(label)` that my `const&` of `Controller::queue` made useless), `S6004` (`finalName`), `S6177` (`MainComponent::modelChanged`), `S7172`, `S108` (a comment in the empty `catch` of `ConfirmationContent`, and the two of `AppTestSupport.hpp`). Build warning-clean; `ctest -C Debug -j 8`: 1220/1220. The 43 other test-code issues (the 2 `S108` of the test support being fixed) were accepted in SonarCloud under the standing rule DEC-QLT-001 (owner decision 2026-10-11) with its comment; read back: the 10 issues left open are exactly the ones fixed here, until the next analysis.
- **Second follow-up (2026-10-16)**: four issues remained. `cpp:S2486` x3: a destructor's `catch` that only holds a comment is not "handled", so `ConfirmationContent` logs the failed answer (`XS56K_LOG`, Error level) and the two fake ports of `AppTestSupport.hpp` write a line to `stderr`; `cpp:S7172`: `indexOf(...).has_value()` in `MemoryController::read`. Build warning-clean; `ctest -C Debug -j 8`: 1220/1220.
- **IDE findings (2026-10-16)**: the SonarQube plugin of the IDE (read through VS Code's Problems panel, which lists only the files that are open) showed five `python:S8786` (a regular expression with super-linear backtracking) in `generate_akm_items.py`, outside the PR's diff. The four splits and searches that start with `\s*` or `\s+` or hold `\s*=?\s*` now use `str.split(";", 1)`, a per-part `\d+` match, a one-character look-behind and `(?:=\s*)?`. Checked: 300,000 random pairs of old and new `parse_domain` and `spec_domains` give the same answers; `--coverage` on the real specification is byte-identical; `--check` up to date; `test_generate_akm_items.py` 23 tests OK; the IDE shows no finding left in the file.
- **Assumptions**: On 2026-10-16 the owner had the 21 issues left (`cpp:S6185` x10, `S107` x4, `S3642` x4, `S7035` x2, `S6168` x1) accepted in SonarCloud, each with the reason above as its comment (RQ-QLT-003); read back: 0 open for these rules on PR 18. A rule whose fix needs a library or standard the CI platforms may lack is left for the owner to decide (RQ-QLT-003), not guessed. `std::lerp` replaces `a + t * (b - a)` for the knob's angle: the same value within float rounding, drawn only.
