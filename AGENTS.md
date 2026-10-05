# AGENTS.md

Guidance for AI coding agents working in this repository.
`CLAUDE.md` imports this file, so keep project instructions here.

## Project overview

- **Name:** XS56K
- **Purpose:** Éditeur pour les sampleurs AKAI S5000 et S6000 — contrôle bidirectionnel
  avec une interface moderne.
- **Stack:** C++ / [JUCE](https://juce.com/) 8.0.15
- **Status:** experimental

`juce/midi` and `juce/framework` are ported from
[xplorer2716/XplorerEditor](https://github.com/xplorer2716/XplorerEditor) (a real-time editor for
the Oberheim Xpander/Matrix-12, itself a JUCE C++ port of a .NET application) — this repository's
CI setup and `juce/CMakeLists.txt` are likewise adapted from that project's. `juce/app` is a
**minimal, intentionally undesigned placeholder** (a bare `juce::DocumentWindow`) — not `model`,
`controller`, `settings`, or any real editor UI — that exists solely so the build/version/deploy
plumbing has a real GUI target to exercise. `juce/akm` is the S5000 SysEx layer (namespace `akm`,
library `xs56k_akm`), written for this repository, not ported: it depends on `xs56k_midi` only and
exposes no JUCE type in its public headers (`ADR-AKM-001`, `FTR-AKM-001`). `juce/mcp` is an MCP (Model Context
Protocol) server that exposes the AKM layer to an MCP client, to edit programs, zones, samples and multis in the sampler's
memory and, behind a launch flag, to load and save them through the sampler's own disks (library `xs56k_mcp`, executable
`xs56k_mcp_server`; `ADR-MCP-001` to `ADR-MCP-003`, `FTR-MCP-001` to `FTR-MCP-003`). Reference documentation
lives in `documents/`, and `process/` holds the AGNOS planning skeleton.

Reference documents are listed in `documents/INDEX.md`. For SysEx questions, start with
`documents/_index/sysex_spec.kb.md` (it explains how to query `sysex_spec.items.tsv`).

The build system itself is a traceable AGNOS artifact: `process/1.requirements/RQ-BLD-build-tooling.md`,
`process/2.architecture/ADR-BLD-001` through `ADR-BLD-003`, and
`process/3.plan/PLAN-BLD-001-reproduce-xplorer-build-system.md` — the last of these lists what is
already done and what remains (blocked) to fully reproduce XplorerEditor's build system here.

## Commands

- **Install:** none beyond a C++20 compiler, CMake ≥ 3.22 and (on Linux) `libasound2-dev`
  (ALSA headers, needed by `juce_audio_devices`); the GUI target (`BUILD_APP=ON`) additionally
  needs `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libxext-dev
  libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev` on Linux. JUCE itself is fetched by CMake
  (`FetchContent`, pinned in `juce/CMakeLists.txt`), not installed separately. [RQ-BLD-001]
- **Build (libraries only):** `cmake -S juce -B juce/build -DCMAKE_BUILD_TYPE=Debug && cmake --build juce/build -j"$(nproc)"`
  (builds the `xs56k_midi`/`xs56k_midi_juce`, `xs56k_framework` and `xs56k_akm` static libraries only). [RQ-BLD-002, RQ-AKM-019]
- **Build (with the placeholder app):** add `-DBUILD_APP=ON` (and, to embed a real version,
  `-DVERSION_NUMERIC=... -DVERSION_FULL=...` — see `.github/actions/resolve-version`); produces
  an `XS56K` executable that opens one placeholder window. [RQ-BLD-007]
- **Test:** `cmake -S juce -B juce/build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON && cmake --build juce/build -j"$(nproc)" && ctest --test-dir juce/build --output-on-failure`
  (`BUILD_TESTS` defaults to `OFF`; Catch2 is fetched by CMake, pinned in `juce/CMakeLists.txt`). With
  a multi-configuration generator (Visual Studio, Xcode) add `--config <cfg>` to the build and
  `-C <cfg>` to `ctest`. Test sources live under `juce/tests/`, mirroring the library they exercise.
  The linux-headless canary and preprod workflows run exactly this, and every generated workflow runs
  the suite in its own configuration. [RQ-AKM-016, RQ-BLD-014, TASK-AKM-003]
- **Probe and real-sampler suite** (`xs56k_akm_probe`, needs the sampler, run by the owner, never by CI against hardware): see
  `juce/tests/probe/README.md` (modes, options, rules, observations). Never run a slow section 10 command on the sampler without the
  owner present, and never send `&32` (Clear Sampler Memory). [RQ-AKM-016 to RQ-AKM-018, RQ-AKM-044]
- **Item catalogue:** the SysEx items are data (`juce/akm/data/items.json`); `python3 juce/tools/generate_akm_items.py`
  (`python` on Windows) regenerates `juce/akm/include/akm/ItemTable.generated.hpp` from it, `--check` fails if that
  table is out of date, `--coverage` compares the data file with the spec's item list
  (`documents/_index/sysex_spec.items.tsv`). Never edit the generated header by hand, and no script runs during the
  build; the three checks are also `ctest` entries when CMake finds Python 3. [RQ-AKM-001, TASK-AKM-008,
  ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]
- **MCP server** (`juce/mcp`, `xs56k_mcp_server`, run by an MCP client with the MIDI ports as its arguments): see
  `juce/mcp/README.md` (launch, tools, safety rules, tests). The disk tools exist only with `--allow-disk` (browsing, loads and the save of a program run on the sampler; loading with dependents checked on one program; bulk save and load of a folder not yet);
  the disk refresh needs `--allow-disk-refresh` too and **hung the sampler**, never send it without asking the owner; no tool deletes or renames a file or folder, formats or ejects a disk, or clears the sampler's memory.
  [RQ-MCP-001 to RQ-MCP-030, ADR-MCP-001 to ADR-MCP-003]
- **Lint:** not a separate step — the build itself is warning-clean at `-Wall -Wextra -Wpedantic
  -Werror` (`/W4 /WX` on MSVC) for project code (not JUCE's own sources), enforced via the
  `xs56k::warnings` interface target in `juce/CMakeLists.txt`. [RQ-BLD-003]

Do not invent commands beyond these; check `CONTRIBUTING.md` (which still has none) and this file
again once a real editor UI exists.

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
    `ADR-BLD-005`), package and — on `dev` and `prod` — publish the placeholder app as a GitHub
    Release (`ADR-BLD-003`). `cut-deployment.yml`
    (`workflow_dispatch` on `main`) triggers the three `prod` ones by pushing a version tag —
    **not yet usable**: it needs a `CUT_DEPLOYMENT` repository secret (a PAT) that has not been
    added (`GITHUB_TOKEN` can't trigger other workflows when it pushes). No SBOM/icon/AppImage/
    code-signing exists — explicitly out of scope until the placeholder becomes a real UI.
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
