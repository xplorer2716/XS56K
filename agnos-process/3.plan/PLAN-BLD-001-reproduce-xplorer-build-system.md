# PLAN-BLD-001: Reproduce XplorerEditor's Build System in XS56K

## Overview

Documents, retroactively, the work to give this repository a build system equivalent to
[xplorer2716/XplorerEditor](https://github.com/xplorer2716/XplorerEditor)'s — the project
`juce/midi` and `juce/framework` were ported from. All eight tasks are now Done: the CMake+JUCE
build, canary/preprod CI, the AGPL-3.0 relicensing that build uncovered, this documentation itself,
a minimal placeholder GUI app (`juce/app`), the full deployment matrix, commit-derived versioning,
the cut-deployment workflow, and (by the owner, in GitHub's own settings) `main`'s branch
protection. What remains real-but-unrun: `cut-deployment.yml` has never actually fired (see
TASK-BLD-007's own verification), and the Windows/macOS legs of the matrix are unverified beyond
YAML syntax (see TASK-BLD-005's).

## References
- **Requirements**: RQ-BLD-001 through RQ-BLD-012 (`process/1.requirements/RQ-BLD-build-tooling.md`)
- **ADRs**: ADR-BLD-001, ADR-BLD-002, ADR-BLD-003 (all Accepted)

---

## Tasks

### TASK-BLD-001: Add the JUCE CMake build foundation
- **Tier**: L
- **Status**: Done
- **Description**: Add `juce/CMakeLists.txt` (pinned JUCE 8.0.15 via `FetchContent`, `xs56k::warnings`
  interface target, `BUILD_APP`/`BUILD_TESTS` options, `add_subdirectory(midi)` +
  `add_subdirectory(framework)`), adapted from XplorerEditor's own root build and trimmed to the
  layers this repository has.
- **Requirement refs**: RQ-BLD-001, RQ-BLD-002, RQ-BLD-003
- **ADR refs**: ADR-BLD-001
- **Acceptance Criteria** (Gherkin): *Given* `juce/CMakeLists.txt`, *When* `cmake -S juce -B juce/build -DCMAKE_BUILD_TYPE=Debug` then `cmake --build juce/build` are run, *Then* configure succeeds, JUCE 8.0.15 is fetched, and `libxs56k_midi.a`, `libxs56k_midi_juce.a`, `libxs56k_framework.a` are built with zero warnings. *Given* the same with `-DCMAKE_BUILD_TYPE=Release`, *When* built, *Then* it also succeeds.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Both configurations built successfully in this session (`ninja`, 25/25 targets each, no warnings). Confirmed `-Wall -Wextra -Wpedantic -Werror` present on project-code compile commands via `build.ninja` (grep on the `FLAGS` line for `AbstractParameter.cpp.o`). JUCE version confirmed fetched at tag `8.0.15` (configure log: `GIT_TAG 8.0.15`).
- **Assumptions**: `BUILD_TESTS` and `BUILD_APP` left at their `OFF` default rather than wired to non-existent directories — no `juce/tests` or `juce/app` exists yet (see TASK-BLD-005/006 backlog).

---

### TASK-BLD-002: Add canary/dev headless CI workflows
- **Tier**: M
- **Status**: Done
- **Description**: Add `.github/workflows/linux-headless-canary.yml` (push, any branch except
  `main`/`dev`) and `linux-headless-preprod.yml` (push + pull_request, `dev`), both building the
  headless libraries of TASK-BLD-001 on `ubuntu-22.04` with ALSA headers installed.
- **Requirement refs**: RQ-BLD-005, RQ-BLD-006, RQ-BLD-012
- **ADR refs**: ADR-BLD-002
- **Acceptance Criteria** (Gherkin): *Given* a push to a `feature/*` branch touching `juce/**`, *When* CI runs, *Then* `linux-headless-canary` builds successfully. *Given* a push or pull request targeting `dev` touching `juce/**`, *When* CI runs, *Then* `linux-headless-preprod` builds successfully. *Given* both workflow files, *When* read, *Then* each file's stem, `name:` and job key are identical. *Given* a push whose full diff touches only `documents/`, `process/` or top-level Markdown, *When* CI is checked, *Then* neither workflow ran for it.
- **Dependencies**: TASK-BLD-001
- **Assignee**: AI
- **Verification**: Both files pass `yaml.safe_load` (checked in-session). File stem = `name:` = job key verified by inspection for both (`linux-headless-canary`, `linux-headless-preprod`). Now also observed running on GitHub's runners: `mcp__github__actions_list` (`list_workflow_runs`) shows 2 completed, successful `linux-headless-canary` runs; `git diff --name-only` between each run's before/after commit confirms both pushes' full range genuinely touched `juce/**` (run #1's push carried `86fd171`, which added `juce/CMakeLists.txt`, alongside the displayed "relicense" commit; run #2's commit touched `juce/CMakeLists.txt` directly) — the `paths` filter (RQ-BLD-012) was not a false trigger either time.
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

## Unblocked, same session (owner decision: create a minimal placeholder app)

*The owner broke TASK-BLD-005/006/007's precondition directly: create a minimal, undesigned
placeholder GUI application target (`juce/app`) purely so the versioning/matrix/cut-deployment
plumbing has something real to build, rather than waiting for the actual editor UI to be
designed and ported. This is NOT TASK-005 of a future real app port — see each task's own
"placeholder" caveat.*

### TASK-BLD-006: Implement commit-derived versioning
- **Tier**: L
- **Status**: Done
- **Description**: Add the `resolve-version` composite action (numeric/display/full forms from the
  commit's UTC committer timestamp and `github.ref`) and wire it into CMake's configure line via
  `VERSION_NUMERIC`/`VERSION_FULL` (no inherited/project-specific prefix — owner decision, same as
  TASK-BLD-002's follow-up).
- **Requirement refs**: RQ-BLD-009
- **ADR refs**: ADR-BLD-003
- **Acceptance Criteria** (Gherkin): see RQ-BLD-009.
- **Dependencies**: None (implemented ahead of TASK-BLD-005 — the derivation itself needs no app target).
- **Assignee**: AI
- **Verification**: `resolve-version.sh` run directly (outside GitHub Actions, as XplorerEditor's own does) against five real refs — `refs/heads/main`, `refs/heads/dev`, `refs/heads/feature/BLD`, a tag, and a `refs/pull/1/merge` — producing stage `""`, `preprod`, `canary`, `""`, `canary` respectively, all correct. `juce/app` then configured and built with `-DVERSION_NUMERIC=2026.9.23.2214 -DVERSION_FULL=2026.09.23-2214-preprod` (the script's own real output, re-run after the `dev`→`preprod` naming correction below); `strings` on the resulting binary confirms `2026.09.23-2214-preprod` is embedded (`getApplicationVersion()` returns it).
- **Assumptions**: None.

### TASK-BLD-005: Implement the full platform/stream deployment matrix
- **Tier**: L
- **Status**: Done
- **Description**: Create the minimal placeholder GUI application target (`juce/app` — a bare
  `juce::DocumentWindow`, no real UI); generate the fifteen `<os>-<arch>-<config>-<stage>`
  deployment workflows (`juce/tools/generate_workflows.py`) plus the `build-app`/
  `package-deployment`/`publish-deployment` composite actions (`resolve-version` was TASK-BLD-006).
  Reduced from XplorerEditor's own equivalents: no SBOM, no icon, no AppImage, no build-provenance
  attestation, no macOS launch-screenshot — all explicitly out of scope (`ADR-BLD-003`) since the
  placeholder app has no icon, no embedded asset and nothing to disclose.
- **Requirement refs**: RQ-BLD-002 (superseded for the app-existence clause — a placeholder now exists), RQ-BLD-007, RQ-BLD-008, RQ-BLD-012
- **ADR refs**: ADR-BLD-003
- **Acceptance Criteria** (Gherkin): see RQ-BLD-007/RQ-BLD-008; each generated file's `paths` filter per RQ-BLD-012.
- **Dependencies**: TASK-BLD-006 (for `VERSION_NUMERIC`/`VERSION_FULL`, consumed by `build-app`).
- **Assignee**: AI
- **Verification**: `juce/app` configured (`-DBUILD_APP=ON`) and built clean in both Debug and Release on Linux, warnings-as-errors included; the produced binary launched under `xvfb-run` and ran its event loop until killed by `timeout` (not a crash — the X `BadAtom` lines are Xvfb's own known incomplete-EWMH-atom noise, not an application error). `juce/tools/generate_workflows.py` run, then `--check` immediately after: "15 generated workflows are up to date" (idempotent). All 18 workflow files and 4 composite-action files (`yaml.safe_load`) and `resolve-version.sh` (`bash -n`) parse without error. Windows and macOS legs of the generated matrix are unverified beyond syntax — no Windows/macOS runner available in this session; first real signal is their first CI run. **Defect found and fixed in the same task:** `.gitignore`'s inherited `build-*/` pattern (template init commit `a811402`) is unanchored and silently matched `.github/actions/build-app/` at any depth — `git status` showed it as untracked-and-about-to-be-added-looking but `git status <exact path>` actually reported "nothing to commit, working tree clean" for it, which is what surfaced the mismatch. Fixed by anchoring and scoping it to `/juce/build*/`, the only place local CMake build directories are actually created.
- **Assumptions**: The placeholder app links `juce::juce_gui_extra` + `juce::juce_audio_devices` and the existing `xs56k_midi_juce`/`xs56k_framework` libraries, PRODUCT_NAME `"XS56K"` (so the built executable is named `XS56K`, matching `build-app`'s locate-by-name logic). No composite action for the placeholder's own source beyond what's listed — a single `Main.cpp` needs none. No other directory in this repository's history matches the old unanchored `.gitignore` pattern (checked: `find . -iname 'build-*' -type d` finds only the one that exposed the bug).

### TASK-BLD-007: Implement the cut-deployment production workflow
- **Tier**: M
- **Status**: Done
- **Description**: Add `.github/workflows/cut-deployment.yml` (`workflow_dispatch` on `main`, derives
  the version, pushes it as a tag), adapted from XplorerEditor's own.
- **Requirement refs**: RQ-BLD-010
- **ADR refs**: ADR-BLD-003
- **Acceptance Criteria** (Gherkin): see RQ-BLD-010.
- **Dependencies**: TASK-BLD-006
- **Assignee**: AI
- **Verification**: `yaml.safe_load` passes. Not run on GitHub's runners — doing so would cut a real (placeholder) production deployment, which is a real, outward-facing effect (a public GitHub Release) this task does not have standing authorization to trigger; a run also has a real operational prerequisite that is not met yet (see the file's own OPERATIONAL PREREQUISITE comment: a `CUT_DEPLOYMENT` repository secret, a personal access token, must be added in Settings → Secrets and variables → Actions — the default `GITHUB_TOKEN` cannot trigger other workflows when it pushes, so without it the tag would push but the three `*-release-prod` workflows would never start).
- **Assumptions**: None.

### TASK-BLD-008: Configure explicit branch protection on `main`
- **Tier**: S
- **Status**: Done (by the human — repository-administration access this session does not have)
- **Description**: Add a GitHub branch protection rule covering `main`, now that it is no longer
  the repository's default branch and therefore not protected automatically. Settings recommended
  (Settings → Branches → Add branch protection rule, branch name pattern `main`) when this task was
  first drafted:
  - **Require a pull request before merging** — ON. No direct pushes to `main`; the only way
    content reaches it is a reviewed PR (routine dev→main promotions) or `cut-deployment`'s own
    tag push (which does not touch the branch itself, only pushes a tag from its tip).
  - **Required approvals** — 0. This is a single-maintainer project (`CODEOWNERS` was deliberately
    removed earlier this repository's history — "un seul owner"); requiring a second approver has
    no one to provide it.
  - **Require status checks to pass before merging** — deliberately left OFF. No generated workflow
    currently triggers on a pull request targeting `main` (only `dev`'s do — RQ-BLD-005); production
    correctness is instead gated at deployment time by the `*-release-prod` builds themselves
    (TASK-BLD-005), the same structural choice XplorerEditor's own `ADR-BLD-003` makes. Revisit if
    `main`-targeting PR checks are ever added.
  - Everything else (linear history, force-push/deletion restrictions, etc.) — owner's discretion;
    not required by any requirement in this plan.
- **Requirement refs**: RQ-BLD-011
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): see RQ-BLD-011.
- **Dependencies**: None functionally — done by the owner directly in GitHub's repository
  settings, outside this session's tool set.
- **Assignee**: Human
- **Verification**: Owner confirmed configuring it directly ("je l'ai fait dans le backend
  github"). Cross-checked, not just taken on trust: `mcp__github__list_branches` reports
  `main` with `"protected": true` (also `dev`, unaffected by this task). The specific rule
  contents (PR-required, approval count, which status checks if any) were not independently
  re-derived from a branch-protection-rules API this session's tool set does not expose —
  the boolean `protected` flag is the only signal available here.
- **Assumptions**: The recommended settings above (PR required, 0 approvals, no required status
  checks) were a suggestion for the owner to apply, not verified as the exact configuration
  chosen — the owner may have configured it differently.
