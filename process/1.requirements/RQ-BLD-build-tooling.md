# RQ-BLD — Build System & Tooling

## Overview

The build system for the JUCE (C++) port: how it is configured, compiled, linted and delivered.
Adapted from [xplorer2716/XplorerEditor](https://github.com/xplorer2716/XplorerEditor)'s own
`process/1.requirements/RQ-BLD-build-tooling.md` — the project `juce/midi` and `juce/framework`
were ported from (see `AGENTS.md`) — trimmed to what this repository actually has, and re-scoped
accordingly. `juce/app` is a **minimal, intentionally undesigned placeholder** (TASK-BLD-005), not
`model`/`controller`/`settings` or the real editor UI — RQ-BLD-007 through RQ-BLD-010 build,
version and package that placeholder, real mechanics against an unreal product. RQ-BLD-011 remains
genuinely blocked (repository-administration access this session does not have), kept here so a
future session does not have to re-derive the reasoning from scratch.

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: CI (GitHub Actions), contributors building the JUCE port locally

---

## Functional Requirements — implemented

### RQ-BLD-001: CMake build fetching a pinned JUCE
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The C++ implementation SHALL build with CMake (≥ 3.22) fetching JUCE 8.x as a pinned dependency via `FetchContent`, with no manually-copied binaries.
- **Rationale**: keeps the JUCE version explicit and reproducible, with nothing binary vendored into the repository.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a clean checkout of this repository, *When* `cmake -S juce -B juce/build` is run, *Then* CMake configures successfully and fetches JUCE at the pinned tag from its upstream repository, and *When* the repository tree is searched for a JUCE binary, *Then* there is no match.
- **Dependencies**: ADR-BLD-001

### RQ-BLD-002: Headless layered libraries, GUI application deferred
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The build SHALL compile the `midi` and `framework` layers as standalone static libraries, headless (no GUI system libraries required), on Linux; the GUI application target SHALL only build when `BUILD_APP` is explicitly enabled, and stays deferred until a `model`/`controller`/`settings`/`app` layer exists in this repository.
- **Rationale**: mirrors XplorerEditor's RQ-BLD-002/RQ-BLD-005 layering, sized to what has actually been ported so far (`midi` and `framework` only — see `AGENTS.md`).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the current repository state (no `app`/`model`/`controller`/`settings` directory), *When* `cmake --build juce/build` is run with default options, *Then* the `xpl_midi`, `xpl_midi_juce` and `xpl_framework` static libraries build successfully and no GUI application target is attempted.
- **Dependencies**: RQ-BLD-001; ADR-BLD-001

### RQ-BLD-003: Strict warnings for project code only
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The code SHALL use C++20 and compile warning-clean at high warning levels (`-Wall -Wextra -Wpedantic` / `/W4`) with warnings-as-errors, applied to project code only — never to JUCE's own module sources compiled into a target.
- **Rationale**: catches real defects early without being defeated by, or generating false positives against, a large vendored dependency this project does not control.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the `xpl_warnings` interface target, *When* a project source file (`xpl_midi`, `xpl_midi_juce`, `xpl_framework`) is compiled, *Then* `-Wall -Wextra -Wpedantic -Werror` (or `/W4 /WX` on MSVC) are present on its compile command. *Given* a full build, *When* it completes, *Then* it produces no warning.
- **Dependencies**: ADR-BLD-001

### RQ-BLD-004: AGPL-3.0 license header and project licensing
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: All source files SHALL carry the GNU Affero General Public License v3 header; the project LICENSE, README and CONTRIBUTING SHALL declare AGPL-3.0-or-later, consistent with JUCE 8's open-source tier (AGPLv3 only — no commercial JUCE license held).
- **Rationale**: JUCE 8.0.15's `LICENSE.md` offers no plain-GPLv3 open-source tier; combining GPLv3 project code with AGPL-licensed JUCE would require the combination to be declared AGPLv3 regardless (same finding as XplorerEditor's RQ-BLD-006 / `ADR-ABT-002`). Verified directly against the fetched `juce-src/LICENSE.md` rather than assumed.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the repository root, *When* `LICENSE` is read, *Then* it is the text of the GNU Affero General Public License, version 3. *Given* any `juce/` source file, *When* its header is read, *Then* it names the GNU Affero General Public License. *Given* `README.md` and `CONTRIBUTING.md`, *When* their License sections are read, *Then* both state AGPL-3.0-or-later.
- **Dependencies**: None

### RQ-BLD-005: Two long-lived branches, one deployment behaviour each
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The repository SHALL carry two long-lived branches — `main` (production, protected) and `dev` (integration, the repository's default branch) — plus short-lived `feature/*` branches (canary). CI SHALL build the headless libraries on every push to a `feature/*` branch (canary) and on every push or pull request targeting `dev` (dev); neither stream publishes anything, since there is no deployable artifact yet.
- **Rationale**: gives a place to integrate and validate a change before it reaches `dev`, and fast feedback on a feature branch without opening a pull request first — same reasoning as XplorerEditor's `ADR-BLD-003` (`DEC-BLD-013`, `DEC-BLD-024`), trimmed to drop everything deployment-related (no GUI app to deploy).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the repository settings, *When* the default branch is read, *Then* it is `dev`. *Given* a push to a `feature/*` branch touching `juce/**`, *When* CI runs, *Then* `linux-headless-canary` builds it and publishes no release. *Given* a push or pull request targeting `dev` and touching `juce/**`, *When* CI runs, *Then* `linux-headless-dev` builds it and publishes no release.
- **Dependencies**: ADR-BLD-002

### RQ-BLD-006: Self-describing CI workflow naming
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Each CI workflow SHALL cover exactly one stream, and its file name, workflow `name:` and job key SHALL be identical, so a pull-request status check names itself without cross-referencing.
- **Rationale**: same property XplorerEditor's `RQ-BLD-023` establishes for its full platform/config/stream matrix, applied here to the two workflows this repository currently has.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* `linux-headless-canary.yml` and `linux-headless-dev.yml`, *When* each is read, *Then* its file stem, its `name:` and its single job key are the same string.
- **Dependencies**: ADR-BLD-002

### RQ-BLD-012: Path-scoped CI triggers
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a push or pull request touches no file under `juce/**` (and does not itself edit the workflow file in question), THEN a build/compile CI workflow SHALL NOT run for it — every such workflow's trigger SHALL carry a `paths` filter scoped to `juce/**` plus its own workflow file.
- **Rationale**: a build workflow exists to answer "does the C++ port still compile", which a change confined to `documents/`, `process/` or the repository's own Markdown files cannot affect — running it anyway burns CI minutes and adds a status check with nothing to say about the change under review. Verified retroactively against this repository's own CI run history (`linux-headless-canary` runs #1–#2): both were triggered by a *push* whose full commit range genuinely touched `juce/**`, confirming the filter already in place works as intended, not merely as documented.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* `linux-headless-canary.yml` and `linux-headless-dev.yml`, *When* their `on.push.paths` (and, for the latter, `on.pull_request.paths`) are read, *Then* each lists `juce/**` and its own workflow file, and nothing else. *Given* a push whose full diff touches only `documents/`, `process/` or top-level Markdown files, *When* CI is checked, *Then* neither workflow ran for it.
- **Dependencies**: RQ-BLD-005, RQ-BLD-006

---

## Functional Requirements — implemented against a minimal placeholder application

*Originally drafted as backlog, deferred until a GUI application layer existed. The owner instead
created `juce/app` directly — an intentionally minimal, undesigned placeholder (a bare
`juce::DocumentWindow`), not the real editor UI (TASK-BLD-005) — so RQ-BLD-007 through RQ-BLD-010
below are now implemented against it. What they build, version and package is a placeholder; the
mechanics (the matrix, the generator, the derivation, the cut-deployment gate) are real.*

### RQ-BLD-007: Full platform/stream deployment matrix
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a GUI application target exists, CI SHALL build and package it for `windows-x64`, `macos-arm64` and `linux-x64`, each in Debug and Release configuration for the `canary` and `dev` (pre-production) streams, and Release-only for the `prod` stream — fifteen platform/configuration/stream combinations in total.
- **Rationale**: reproduces XplorerEditor's `RQ-BLD-002`/`RQ-BLD-011`/`RQ-BLD-012`/`RQ-BLD-019`/`RQ-BLD-023` coverage, the actual target this session's CI setup is a deliberately-reduced first step toward.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a GUI application target, *When* the CI workflows are listed, *Then* there are fifteen deployment workflows covering every (`windows`|`macos`|`linux`) × (`debug`|`release`, or `release` only for `prod`) × (`canary`|`dev`|`prod`) combination that is valid per this requirement.
- **Dependencies**: RQ-BLD-002, RQ-BLD-005; ADR-BLD-003

### RQ-BLD-008: Generated workflows over hand-duplicated files
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The fifteen deployment workflow files of RQ-BLD-007 SHALL be generated from one matrix source rather than hand-duplicated, with the shared build/package/publish logic living once, in composite actions under `.github/actions/`.
- **Rationale**: reproduces XplorerEditor's `RQ-BLD-023` (composite actions) and its `juce/tools/generate_workflows.py` generator — fifteen near-identical files are fifteen copies of one procedure waiting to drift apart.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* the fifteen deployment workflows, *When* their build steps are compared, *Then* the shared logic appears once, in composite actions. *Given* the generator script, *When* it is run with `--check` after a plain run with no matrix change, *Then* it reports zero stale files. *Given* any of the fifteen generated files, *When* its `on.push.paths` is read, *Then* it carries the same `juce/**`-scoped filter RQ-BLD-012 requires of today's two hand-written workflows — the generator SHALL NOT drop it.
- **Dependencies**: RQ-BLD-007, RQ-BLD-012; ADR-BLD-003

### RQ-BLD-009: Commit-derived version, single derivation
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The product version SHALL be produced by one derivation, from the commit being built (`YYYY.MM.DD-HHMM`, UTC committer timestamp, plus a stage suffix), in three forms (numeric, display, full) — never assigned by a counter or typed by a human. No file in the repository SHALL contain a literal product version.
- **Rationale**: reproduces XplorerEditor's `RQ-BLD-015`/`RQ-BLD-016`/`RQ-BLD-020` and the reasoning in its `ADR-BLD-003` (`DEC-BLD-014`) for why a counter is not viable across concurrent, independent CI workflows.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a commit, *When* the version is derived by all workflows building it, *Then* every workflow computes the identical three forms. *Given* the repository, *When* it is searched for a literal version string outside the derivation itself, *Then* there is no match.
- **Dependencies**: RQ-BLD-007; ADR-BLD-003

### RQ-BLD-010: Production cut by an explicit manual action
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a production deployment is wanted, it SHALL be cut by an explicit manual action on `main` which derives the version, pushes it as a tag, and thereby triggers the production workflows; a plain push to `main` SHALL NOT publish a deployment.
- **Rationale**: reproduces XplorerEditor's `RQ-BLD-028` — the version cannot be typed by a human before the commit it derives from exists, so computing and pushing the tag removes the possibility of disagreement instead of checking for it afterwards.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a push to `main` with no tag, *When* its workflows complete, *Then* no deployment was published. *Given* the cut-deployment action run on `main`, *When* it completes, *Then* a tag exists and every production workflow has run.
- **Dependencies**: RQ-BLD-009; ADR-BLD-003

---

## Non-Functional Requirements — backlog (Proposed, blocked)

*Not implementable from this session — see rationale below.*

### RQ-BLD-011: Explicit branch protection on `main`
- **Category**: Non-Functional
- **NFR Type**: Security
- **EARS Type**: Ubiquitous
- **Statement**: `main` SHALL be covered by an explicit GitHub branch protection rule, since it is no longer the repository's default branch (RQ-BLD-005) and therefore not protected automatically.
- **Rationale**: reproduces the protection clause of XplorerEditor's `RQ-BLD-019`. Unlike the other backlog items here, this does **not** depend on a GUI application existing — it is out of reach of this session for a different reason: configuring repository branch-protection rules requires GitHub repository-administration access this session's tool set does not have.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the repository's branch protection rules, *When* they are read, *Then* `main` is covered by one.
- **Dependencies**: RQ-BLD-005
