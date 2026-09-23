# PLAN-BLD-001: Reproduce XplorerEditor's Build System in XS56K

## Overview

Documents, retroactively for what is already done and prospectively for what remains, the work to
give this repository a build system equivalent to
[xplorer2716/XplorerEditor](https://github.com/xplorer2716/XplorerEditor)'s — the project
`juce/midi` and `juce/framework` were ported from. Four tasks are already complete (a working
CMake+JUCE build, canary/dev CI, and the AGPL-3.0 relicensing that build uncovered, plus this
documentation itself); four are backlog, gated on a GUI application layer existing in this
repository or on access this session does not have.

## References
- **Requirements**: RQ-BLD-001 through RQ-BLD-011 (`process/1.requirements/RQ-BLD-build-tooling.md`)
- **ADRs**: ADR-BLD-001, ADR-BLD-002 (Accepted), ADR-BLD-003 (Proposed)

---

## Tasks

### TASK-BLD-001: Add the JUCE CMake build foundation
- **Tier**: L
- **Status**: Done
- **Description**: Add `juce/CMakeLists.txt` (pinned JUCE 8.0.15 via `FetchContent`, `xpl::warnings`
  interface target, `XS56K_BUILD_APP`/`XS56K_BUILD_TESTS` options, `add_subdirectory(midi)` +
  `add_subdirectory(framework)`), adapted from XplorerEditor's own root build and trimmed to the
  layers this repository has.
- **Requirement refs**: RQ-BLD-001, RQ-BLD-002, RQ-BLD-003
- **ADR refs**: ADR-BLD-001
- **Acceptance Criteria** (Gherkin): *Given* `juce/CMakeLists.txt`, *When* `cmake -S juce -B juce/build -DCMAKE_BUILD_TYPE=Debug` then `cmake --build juce/build` are run, *Then* configure succeeds, JUCE 8.0.15 is fetched, and `libxpl_midi.a`, `libxpl_midi_juce.a`, `libxpl_framework.a` are built with zero warnings. *Given* the same with `-DCMAKE_BUILD_TYPE=Release`, *When* built, *Then* it also succeeds.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Both configurations built successfully in this session (`ninja`, 25/25 targets each, no warnings). Confirmed `-Wall -Wextra -Wpedantic -Werror` present on project-code compile commands via `build.ninja` (grep on the `FLAGS` line for `AbstractParameter.cpp.o`). JUCE version confirmed fetched at tag `8.0.15` (configure log: `GIT_TAG 8.0.15`).
- **Assumptions**: `XS56K_BUILD_TESTS` and `XS56K_BUILD_APP` left at their `OFF` default rather than wired to non-existent directories — no `juce/tests` or `juce/app` exists yet (see TASK-BLD-005/006 backlog).

---

### TASK-BLD-002: Add canary/dev headless CI workflows
- **Tier**: M
- **Status**: Done
- **Description**: Add `.github/workflows/linux-headless-canary.yml` (push, any branch except
  `main`/`dev`) and `linux-headless-dev.yml` (push + pull_request, `dev`), both building the
  headless libraries of TASK-BLD-001 on `ubuntu-22.04` with ALSA headers installed.
- **Requirement refs**: RQ-BLD-005, RQ-BLD-006, RQ-BLD-012
- **ADR refs**: ADR-BLD-002
- **Acceptance Criteria** (Gherkin): *Given* a push to a `feature/*` branch touching `juce/**`, *When* CI runs, *Then* `linux-headless-canary` builds successfully. *Given* a push or pull request targeting `dev` touching `juce/**`, *When* CI runs, *Then* `linux-headless-dev` builds successfully. *Given* both workflow files, *When* read, *Then* each file's stem, `name:` and job key are identical. *Given* a push whose full diff touches only `documents/`, `process/` or top-level Markdown, *When* CI is checked, *Then* neither workflow ran for it.
- **Dependencies**: TASK-BLD-001
- **Assignee**: AI
- **Verification**: Both files pass `yaml.safe_load` (checked in-session). File stem = `name:` = job key verified by inspection for both (`linux-headless-canary`, `linux-headless-dev`). Now also observed running on GitHub's runners: `mcp__github__actions_list` (`list_workflow_runs`) shows 2 completed, successful `linux-headless-canary` runs; `git diff --name-only` between each run's before/after commit confirms both pushes' full range genuinely touched `juce/**` (run #1's push carried `86fd171`, which added `juce/CMakeLists.txt`, alongside the displayed "relicense" commit; run #2's commit touched `juce/CMakeLists.txt` directly) — the `paths` filter (RQ-BLD-012) was not a false trigger either time.
- **Assumptions**: No composite action introduced (DEC-BLD-007) — two call sites do not yet justify the indirection.

---

### TASK-BLD-003: Relicense the project as AGPL-3.0-or-later
- **Tier**: M
- **Status**: Done
- **Description**: Replace `LICENSE` with the official FSF AGPLv3 text, and update `README.md` /
  `CONTRIBUTING.md`'s License sections from GPL-3.0 to AGPL-3.0-or-later, after finding the ported
  `juce/` source headers already declared AGPL and JUCE 8.0.15 has no plain-GPLv3 open-source tier.
- **Requirement refs**: RQ-BLD-004
- **ADR refs**: None — a factual correction (JUCE 8's actual license terms), not an architecture trade-off.
- **Acceptance Criteria** (Gherkin): *Given* `LICENSE`, *When* read, *Then* it is the GNU Affero General Public License v3 text. *Given* `README.md`/`CONTRIBUTING.md`, *When* their License sections are read, *Then* both state AGPL-3.0-or-later. *Given* the repository, *When* searched for `GPL-3.0` outside `AGPL-3.0` matches, *Then* there is no match.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: `juce-src/LICENSE.md` (the fetched JUCE 8.0.15 tree, TASK-BLD-001) read directly in-session: "dual-licensed under the AGPLv3 and the commercial JUCE licence" — no GPLv3 tier offered. `LICENSE` replaced with XplorerEditor's own AGPLv3 text (itself the official FSF text, confirmed by its header/footer). Repository-wide grep for `GPL-3.0\b` outside `AGPL-3.0` returned no match after the edit. Change confirmed with the user (`AskUserQuestion`) before applying, since a license change is a decision only the project owner should confirm.
- **Assumptions**: None — every fact here (JUCE's license tier, the pre-existing AGPL headers, the resulting inconsistency) was verified by reading the actual files rather than inferred.

---

### TASK-BLD-004: Author this plan and its requirements/ADRs
- **Tier**: M
- **Status**: Done
- **Description**: Write `RQ-BLD-build-tooling.md`, `ADR-BLD-001`, `ADR-BLD-002`, `ADR-BLD-003` and
  this plan, documenting TASK-BLD-001 through 003 retroactively and the backlog needed to fully
  reproduce XplorerEditor's build system prospectively; add traceability comments referencing these
  IDs to the files TASK-BLD-001/002 created.
- **Requirement refs**: RQ-BLD-001 through RQ-BLD-011 (this task documents all of them)
- **ADR refs**: ADR-BLD-001, ADR-BLD-002, ADR-BLD-003
- **Acceptance Criteria** (Gherkin): *Given* `process/1.requirements/`, `process/2.architecture/` and `process/3.plan/`, *When* listed, *Then* `RQ-BLD-build-tooling.md`, the three `ADR-BLD-*` files and this plan are present. *Given* `juce/CMakeLists.txt` and the two workflow files, *When* read, *Then* each carries a comment citing the RQ-BLD/ADR-BLD IDs it satisfies.
- **Dependencies**: TASK-BLD-001, TASK-BLD-002, TASK-BLD-003
- **Assignee**: AI
- **Verification**: Files listed by `ls`/`Write` tool results in this session; traceability comments added by direct edit (see the same commit as this plan's addition).
- **Assumptions**: The user's request ("créer les exigences...correspondantes à ce que tu as fait / ce qu'il faut pour reproduire le système de builds de xplorer") was read as covering both the already-done work and a documented backlog — confirmed explicitly with the user before drafting (two rounds: scope of the backlog, then the platform-matrix/cut-deployment gap the user pointed out).

---

## Backlog — blocked

### TASK-BLD-005: Implement the full platform/stream deployment matrix
- **Tier**: L
- **Status**: Blocked
- **Description**: Generate the fifteen `<os>-<arch>-<config>-<stage>` deployment workflows (a
  `generate_workflows.py`-equivalent script) plus the `build-app`/`resolve-version` composite
  actions, once a GUI application target exists to build.
- **Requirement refs**: RQ-BLD-007, RQ-BLD-008, RQ-BLD-012
- **ADR refs**: ADR-BLD-003
- **Acceptance Criteria** (Gherkin): see RQ-BLD-007/RQ-BLD-008; each generated file's `paths` filter per RQ-BLD-012.
- **Dependencies**: A `model`/`controller`/`settings`/`app` layer must exist in this repository (none does yet).
- **Assignee**: AI
- **Verification**: N/A (Blocked, not started)
- **Assumptions**: None

### TASK-BLD-006: Implement commit-derived versioning
- **Tier**: L
- **Status**: Blocked
- **Description**: Add the `resolve-version` composite action (numeric/display/full forms from the
  commit's UTC committer timestamp and `github.ref`) and wire it into CMake's configure line.
- **Requirement refs**: RQ-BLD-009
- **ADR refs**: ADR-BLD-003
- **Acceptance Criteria** (Gherkin): see RQ-BLD-009.
- **Dependencies**: TASK-BLD-005
- **Assignee**: AI
- **Verification**: N/A (Blocked, not started)
- **Assumptions**: None

### TASK-BLD-007: Implement the cut-deployment production workflow
- **Tier**: M
- **Status**: Blocked
- **Description**: Add a `workflow_dispatch`-triggered action on `main` that derives the version,
  pushes it as a tag, and thereby fires the three `prod` deployment workflows.
- **Requirement refs**: RQ-BLD-010
- **ADR refs**: ADR-BLD-003
- **Acceptance Criteria** (Gherkin): see RQ-BLD-010.
- **Dependencies**: TASK-BLD-006
- **Assignee**: AI
- **Verification**: N/A (Blocked, not started)
- **Assumptions**: None

### TASK-BLD-008: Configure explicit branch protection on `main`
- **Tier**: S
- **Status**: Blocked
- **Description**: Add a GitHub branch protection rule covering `main`, now that it is no longer
  the repository's default branch and therefore not protected automatically.
- **Requirement refs**: RQ-BLD-011
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): see RQ-BLD-011.
- **Dependencies**: None functionally — blocked because this session's tool set has no
  repository-administration access, not because anything else must happen first.
- **Assignee**: Human
- **Verification**: N/A (Blocked, not started)
- **Assumptions**: None
