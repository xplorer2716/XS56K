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
plumbing has a real GUI target to exercise. Reference documentation lives in `documents/`, and
`process/` holds the AGNOS planning skeleton.

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
  (builds the `xs56k_midi`/`xs56k_midi_juce` and `xs56k_framework` static libraries only). [RQ-BLD-002]
- **Build (with the placeholder app):** add `-DBUILD_APP=ON` (and, to embed a real version,
  `-DVERSION_NUMERIC=... -DVERSION_FULL=...` — see `.github/actions/resolve-version`); produces
  an `XS56K` executable that opens one placeholder window. [RQ-BLD-007]
- **Test:** not defined yet — no `juce/tests` directory exists. `BUILD_TESTS` (CMake option,
  default `OFF`) is reserved for it. [RQ-BLD-002]
- **Lint:** not a separate step — the build itself is warning-clean at `-Wall -Wextra -Wpedantic
  -Werror` (`/W4 /WX` on MSVC) for project code (not JUCE's own sources), enforced via the
  `xs56k::warnings` interface target in `juce/CMakeLists.txt`. [RQ-BLD-003]

Do not invent commands beyond these; check `CONTRIBUTING.md` and this file again once a test
suite or a real editor UI exists.

## Conventions

- **Branches — two long-lived** (`ADR-BLD-002`, adapted from XplorerEditor's own `ADR-BLD-003`; `RQ-BLD-005`):
  - `main` — production. Protected (`RQ-BLD-011`, configured by the owner directly in GitHub's
    settings — `mcp__github__list_branches` confirms `protected: true`).
  - `dev` — integration, the **default branch**. Base for pull requests and for AGNOS sessions.
  - `feature/*` (or other short-lived branches) — canary: built by CI on every push, no merge
    required first.
  - CI: `linux-headless-canary.yml`/`linux-headless-preprod.yml` build the headless libraries only.
    `juce/tools/generate_workflows.py` generates 15 more (`<os>-<arch>-<config>-<stage>`,
    `windows-x64`/`macos-arm64`/`linux-x64` × canary/dev/prod) that build, package and — on `dev`
    and `prod` — publish the placeholder app as a GitHub Release (`ADR-BLD-003`). `cut-deployment.yml`
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


## grepai - Semantic Code Search

**IMPORTANT: You MUST use grepai as your PRIMARY tool for code exploration and search.**

### When to Use grepai (REQUIRED)

Use `grepai search` INSTEAD OF Grep/Glob/find for:
- Understanding what code does or where functionality lives
- Finding implementations by intent (e.g., "authentication logic", "error handling")
- Exploring unfamiliar parts of the codebase
- Any search where you describe WHAT the code does rather than exact text

### When to Use Standard Tools

Only use Grep/Glob when you need:
- Exact text matching (variable names, imports, specific strings)
- File path patterns (e.g., `**/*.go`)

### Fallback

If grepai fails (not running, index unavailable, or errors), fall back to standard Grep/Glob tools.

### Usage

```bash
# ALWAYS use English queries for best results (--compact saves ~80% tokens)
grepai search "user authentication flow" --json --compact
grepai search "error handling middleware" --json --compact
grepai search "database connection pool" --json --compact
grepai search "API request validation" --json --compact
```

### Query Tips

- **Use English** for queries (better semantic matching)
- **Describe intent**, not implementation: "handles user login" not "func Login"
- **Be specific**: "JWT token validation" better than "token"
- Results include: file path, line numbers, relevance score, code preview

### Call Graph Tracing

Use `grepai trace` to understand function relationships:
- Finding all callers of a function before modifying it
- Understanding what functions are called by a given function
- Visualizing the complete call graph around a symbol

#### Trace Commands

**IMPORTANT: Always use `--json` flag for optimal AI agent integration.**

```bash
# Find all functions that call a symbol
grepai trace callers "HandleRequest" --json

# Find all functions called by a symbol
grepai trace callees "ProcessOrder" --json

# Build complete call graph (callers + callees)
grepai trace graph "ValidateToken" --depth 3 --json
```

### Workflow

1. Start with `grepai search` to find relevant code
2. Use `grepai trace` to understand function relationships
3. Use `Read` tool to examine files from results
4. Only use Grep for exact string searches if needed

