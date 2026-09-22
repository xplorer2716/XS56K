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
