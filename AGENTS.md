# AGENTS.md

Guidance for AI coding agents working in this repository.
`CLAUDE.md` imports this file, so keep project instructions here.

## Project overview

- **Name:** XS56K
- **Purpose:** Éditeur pour les sampleurs AKAI S5000 et S6000 — contrôle bidirectionnel
  avec une interface moderne.
- **Stack:** C++ / [JUCE](https://juce.com/) 8.0.15
- **Status:** experimental

No source code exists yet beyond reference documentation (`documents/`) and the
`process/` planning skeleton. Update this section once the repository layout is real.

Reference documents are listed in `documents/INDEX.md`. For SysEx questions, start with
`documents/_index/sysex_spec.kb.md` (it explains how to query `sysex_spec.items.tsv`).

## Commands

Install, build, test and lint commands are not defined yet — there is no buildable
code in the repository at this stage. Do not invent commands; check `CONTRIBUTING.md`
and this file again once they exist.

## Conventions

- Default branch: `main`
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

