# AGENTS.md

Guidance for AI coding agents working in this repository.
`CLAUDE.md` imports this file, so keep project instructions here.

<!-- AI: TEMPLATE INITIALIZATION — delete this whole section (up to the next horizontal rule) once initialization is complete. -->
## ⚠️ Template initialization (pending)

This repository was created from a template and is **not initialized yet**.

Before doing any other task, check whether placeholders remain:

```bash
grep -rnE '\{\{[A-Z0-9_]+\}\}|<!-- (AI|OPTIONAL):|# AI:' --exclude-dir=.git --exclude-dir=.template .
```

If the command returns results, tell the user the project is not initialized and
offer to run the initialization by following [`.template/INIT.md`](.template/INIT.md):
infer what you can from the repository, **ask the user** the remaining questions in
small batches, never invent values, then fill in the files.

---

## Project overview

- **Name:** {{PROJECT_NAME}}
- **Purpose:** {{PROJECT_TAGLINE}}
- **Stack:** {{LANGUAGE_STACK}}
- **Status:** {{PROJECT_STATUS}}

<!-- AI: Describe the repository layout (main directories and what they contain) once code exists. Ask the user or read the tree; do not guess. -->

## Commands

| Task | Command |
|---|---|
| Install | `{{INSTALL_COMMAND}}` |
| Build | `{{BUILD_COMMAND}}` |
| Test | `{{TEST_COMMAND}}` |
| Lint / format | `{{LINT_COMMAND}}` |

Run the test and lint commands before proposing a commit.

## Conventions

- Default branch: `{{DEFAULT_BRANCH}}`
- Branch naming: {{BRANCH_NAMING}}
- Commit messages: {{COMMIT_CONVENTION}}
- Versioning: {{VERSIONING}} — record user-facing changes in `CHANGELOG.md` under `[Unreleased]`.

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

