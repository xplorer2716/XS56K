# AGENTS.md

Guidance for AI coding agents working in this repository.
`CLAUDE.md` imports this file, so keep project instructions here.

## Project overview

- **Name:** XS56K
- **Purpose:** Drive an AKAI S5000 or S6000 sampler from a computer: a SysEx library, an MCP server
  for AI assistants, and an editor.
- **Stack:** C++ / [JUCE](https://juce.com/) 8.0.15
- **Status:** experimental. The editor is partly built: the sample editor exists, the program editor does not.

XS56K is three things that build on each other (see `README.md`):

1. **A SysEx library** (`juce/akm`) that aims to cover every MIDI SysEx message of the S5000 and S6000.
2. **An MCP server** (`juce/mcp`) that lets an AI assistant work on the sampler. The goal is every function of the
   samplers that the SysEx specification offers.
3. **A program editor** (`juce/app`): main window, menus, MIDI setup, disk browser, memory view and the sample editor
   (FTR-APP-010); the save and the program editor (FTR-APP-006, FTR-APP-008) are not built. Light MVC (`ADR-APP-001`): `juce/app/core` (`xs56k_app_core`, model and
   controllers, no JUCE, tested on the simulated sampler) over `xs56k_sampler` (the gateway shared with the MCP server);
   `juce/app/src` holds the thin JUCE views, every visual value from the design tokens (`ADR-APP-002`).

`juce/midi` and `juce/framework` are ported from
[xplorer2716/XplorerEditor](https://github.com/xplorer2716/XplorerEditor) (a real-time editor for
the Oberheim Xpander/Matrix-12, itself a JUCE C++ port of a .NET application) — this repository's
CI setup and `juce/CMakeLists.txt` are likewise adapted from that project's. `juce/akm` is the S5000 SysEx layer (namespace `akm`,
library `xs56k_akm`), written for this repository, not ported: it depends on `xs56k_midi` only and
exposes no JUCE type in its public headers (`ADR-AKM-001`, `FTR-AKM-001`). `juce/mcp` is an MCP (Model Context
Protocol) server that exposes the AKM layer to an MCP client, to edit programs, zones, samples and multis in the sampler's
memory and, behind a launch flag, to load and save them through the sampler's own disks (library `xs56k_mcp`, executable
`xs56k_mcp_server`; `ADR-MCP-001` to `ADR-MCP-003`, `FTR-MCP-001` to `FTR-MCP-003`). Reference documentation
lives in `documents/`, and `process/` holds the AGNOS planning skeleton.

**The AGNOS process is in `.github/instructions/agnos-sw-eng.instructions.md`: read it in full, every line, before any session that plans,
implements or commits a task** (its START SESSION step 1 requires it; a partial read has already cost a session). Its skills are
`agnos-git-workflow` (branch and commit format) and `agnos-index` (the index of `process/`); `process/_sessionstate/session.yaml` must say
the session's real platform and `unit_tests` before the first task.

Reference documents are listed in `documents/INDEX.md`. For SysEx questions, start with
`documents/_index/sysex_spec.kb.md` (it explains how to query `sysex_spec.items.tsv`).

The build system itself is a traceable AGNOS artifact: `process/1.requirements/RQ-BLD-build-tooling.md`,
`process/2.architecture/ADR-BLD-001` through `ADR-BLD-003`, and
`process/3.plan/PLAN-BLD-001-reproduce-xplorer-build-system.md` — the last of these lists what is
already done and what remains (blocked) to fully reproduce XplorerEditor's build system here.

## Commands

- **Shell calls:** prefix every command-line call with `rtk` (RTK, "Rust Token Killer", a token-saving CLI proxy that
  condenses command output): `rtk git status`, `rtk cmake --build juce/build`, `rtk ctest --test-dir juce/build
  --output-on-failure`, `rtk gh run view <id>`. Write the prefix yourself, the automatic rewriting hook is not relied
  upon. If a condensed result is unusable (empty when output was clearly expected, contradicting its exit code, or
  garbled), re-run it as `rtk proxy <cmd>` to get the raw output; `rtk gain` shows the savings.
- **Platforms:** the commands below run on Windows, Linux and macOS, so each is one plain call with no shell operator:
  run the steps of a sequence one after the other, never chained with `&&` (Windows PowerShell 5.1 has no `&&`), and
  take the number of build jobs from `--parallel` (not from `$(nproc)`, which only bash has). Write the shell syntax
  of the session's `platform` (`process/_sessionstate/session.yaml`): PowerShell on `windows`, bash or zsh on `linux`
  and `macos`. The `rtk` prefix and the paths with `/` are the same everywhere.
- **Install:** none beyond a C++20 compiler, CMake ≥ 3.22 and (on Linux) `libasound2-dev`
  (ALSA headers, needed by `juce_audio_devices`); the GUI target (`BUILD_APP=ON`) additionally
  needs `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libxext-dev
  libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev` on Linux. JUCE itself is fetched by CMake
  (`FetchContent`, pinned in `juce/CMakeLists.txt`), not installed separately. [RQ-BLD-001]
- **Build (libraries only):** `cmake -S juce -B juce/build -DCMAKE_BUILD_TYPE=Debug`, then `cmake --build juce/build --parallel`
  (builds the `xs56k_midi`/`xs56k_midi_juce`, `xs56k_framework` and `xs56k_akm` static libraries only). [RQ-BLD-002, RQ-AKM-019]
- **Build (with the editor):** add `-DBUILD_APP=ON` (and, to embed a real version,
  `-DVERSION_NUMERIC=... -DVERSION_FULL=...` — see `.github/actions/resolve-version`); produces
  the `XS56K` editor (`juce/build/app/XS56KApp_artefacts/<config>/`). [RQ-BLD-007]
- **Test:** `cmake -S juce -B juce/build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON`, then `cmake --build juce/build --parallel`,
  then `ctest --test-dir juce/build --output-on-failure`
  (`BUILD_TESTS` defaults to `OFF`; Catch2 is fetched by CMake, pinned in `juce/CMakeLists.txt`). With
  a multi-configuration generator (Visual Studio, Xcode) add `--config <cfg>` to the build and
  `-C <cfg>` to `ctest`. Test sources live under `juce/tests/`, mirroring the library they exercise.
  The linux-headless canary and preprod workflows run exactly this, and every generated workflow runs
  the suite in its own configuration. [RQ-AKM-016, RQ-BLD-014, TASK-AKM-003]
- **Probe and real-sampler suite** (`xs56k_akm_probe`, needs the sampler, run by the owner, never by CI against hardware): see
  `juce/tests/probe/README.md` (modes, options, rules, observations). Never run a slow section 10 command on the sampler without the
  owner present, and never send `&32` (Clear Sampler Memory). [RQ-AKM-016 to RQ-AKM-018, RQ-AKM-044]
- **Item catalogue:** the SysEx items are data (`juce/akm/data/items.json`); `python3 juce/tools/generate_akm_items.py`
  (`python` on Windows, as for every script below) regenerates `juce/akm/include/akm/ItemTable.generated.hpp` from it, `--check` fails if that
  table is out of date, `--coverage` compares the data file with the spec's item list
  (`documents/_index/sysex_spec.items.tsv`). Never edit the generated header by hand, and no script runs during the
  build; the three checks are also `ctest` entries when CMake finds Python 3. [RQ-AKM-001, TASK-AKM-008,
  ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]
- **Coverage table:** `documents/SYSEX-COVERAGE.md` gives, for each message of the SysEx spec, the AKM function and the MCP tool that send it;
  `python3 juce/tools/generate_sysex_coverage.py` (`python` on Windows) writes it and `--check` fails if it is out of date or if an item,
  function or tool appears that the script cannot place (a new tool needs its gateway methods in `juce/tools/sysex_coverage_declared.json`).
  Never edit the file by hand; the checks, one of which runs the simulated server on the scripted conversations to see which tool sends what,
  are `ctest` entries when CMake finds Python 3. [RQ-MCP-058 to RQ-MCP-066, ADR-MCP-006]
- **Design tokens and mockups of the editor:** the design system's values are data (`juce/app/design/tokens.json`); `python3
  juce/tools/generate_design.py` (`python` on Windows) writes `juce/app/core/include/app/DesignTokens.generated.hpp` and the value tables of
  `process/1.requirements/DESIGN-SYSTEM-APP.md`, and the editor layouts of `juce/app/design/layouts/` into `EditorLayouts.generated.hpp`;
  `python3 juce/tools/generate_mockups.py` writes the static SVG mockups of
  `documents/mockups/` from the tokens, the layouts and the screens of `juce/app/design/`. Never edit the generated files by hand; `--check`
  fails when they are out of date, and both checks and the scripts' tests are `ctest` entries when CMake finds Python 3. [RQ-APP-002,
  RQ-APP-074 to RQ-APP-078, ADR-APP-002, ADR-APP-003]
- **MCP server** (`juce/mcp`, `xs56k_mcp_server`, run by an MCP client with the MIDI ports as its arguments): see
  `juce/mcp/README.md` (options, tools, safety rules, tests). Every destructive tool asks for a `confirm`; never send the disk
  refresh (`--allow-disk-refresh`) to the owner's sampler without asking him first, and never call `clear_sampler_memory` or a `delete_all_*`
  tool on it unless he says so for that call: a general "do it all without me" is not enough, ask each time. [FTR-MCP-*, ADR-MCP-*]
- **Driving the editor without the mouse** (Windows): `juce/tools/uia.ps1` through the `ui-automation` skill
  (`.github/skills/ui-automation/SKILL.md`): list, invoke, type, capture. Never simulate mouse clicks.
- **New C++ file:** the licence header is copied from `juce/LICENSE_HEADER.txt` by `juce/tools/new_source_file.py`, never typed: skill
  `new-source-file` (`.github/skills/new-source-file/SKILL.md`), subagent `license-header` (`.claude/agents/license-header.md`).
- **Reusable tools:** a script written for a session and used more than once becomes a generic tool in `juce/tools/`, with a skill or
  an instructions file linked here. AGNOS files are not edited for that.
- **Lint:** not a separate step — the build itself is warning-clean at `-Wall -Wextra -Wpedantic
  -Werror` (`/W4 /WX` on MSVC) for project code (not JUCE's own sources), enforced via the
  `xs56k::warnings` interface target in `juce/CMakeLists.txt`. [RQ-BLD-003]

Do not invent commands beyond these; `CONTRIBUTING.md` still has none.

## Conventions

- **Branches — two long-lived** (`ADR-BLD-002`, adapted from XplorerEditor's own `ADR-BLD-003`; `RQ-BLD-005`):
  - `main` — production. Protected (`RQ-BLD-011`, configured by the owner directly in GitHub's
    settings — `mcp__github__list_branches` confirms `protected: true`).
  - `dev` — integration, the **default branch**. Base for pull requests and for AGNOS sessions.
  - `feature/*` (or other short-lived branches) — canary: built by CI on every push, no merge
    required first.
  - CI: `linux-headless-canary.yml`/`linux-headless-preprod.yml` build the headless libraries only.
    `juce/tools/generate_workflows.py` generates 15 more (`<os>-<arch>-<config>-<stage>`,
    `windows-x64`/`macos-arm64`/`linux-x64` × canary/dev/prod) that build, run the test suites
    (`ctest`, before anything is packaged: a failing test stops the job — `RQ-BLD-014`,
    `ADR-BLD-005`), package and — on `dev` and `prod` — publish the editor app as a GitHub
    Release (`ADR-BLD-003`). `cut-deployment.yml`
    (`workflow_dispatch` on `main`) triggers the three `prod` ones by pushing a version tag —
    **not yet usable**: it needs a `CUT_DEPLOYMENT` repository secret (a PAT) that has not been
    added (`GITHUB_TOKEN` can't trigger other workflows when it pushes). No SBOM/icon/AppImage/
    code-signing exists — out of scope until the editor is usable.
- Branch naming: `type/short-description`
- Commit messages: [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/)
- Versioning: [SemVer](https://semver.org/spec/v2.0.0.html) — record user-facing changes in `CHANGELOG.md` under `[Unreleased]`.

## Working with issues and pull requests

The forms in `.github/ISSUE_TEMPLATE/` and `.github/PULL_REQUEST_TEMPLATE.md` define
the information the project needs. When a user asks you to open an issue or a PR:

1. Pick the matching form (bug, feature, documentation, PR).
2. Fill in what you can from the conversation and the code (diff, logs, commands run).
3. **Ask the user** for every required field you cannot answer from facts — for example
   expected vs. actual behavior, reproduction steps, motivation, or how the change was
   tested. Do not fabricate answers.
4. Keep the section headings of the template in the final text.

## Security

Never commit secrets. Report vulnerabilities as described in `SECURITY.md`, never in a
public issue.
