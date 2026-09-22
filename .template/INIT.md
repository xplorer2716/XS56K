# Template initialization protocol (for AI agents)

This repository is a **template**. Its community and documentation files contain
placeholders that must be filled in for the actual project. This document tells an
AI agent (Claude Code, Codex, Copilot agent, …) exactly how to do it.

> Delete the whole `.template/` directory once initialization is complete.

---

## 1. Conventions used in the template files

| Marker | Meaning | What to do |
|---|---|---|
| `{{UPPER_SNAKE_CASE}}` | A value to replace | Replace it with the value obtained in step 3. |
| `<!-- AI: … -->` (Markdown) | Instruction for the agent | Follow it, then **delete the comment**. |
| `# AI: …` (YAML) | Instruction for the agent | Follow it, then **delete the comment**. |
| `<!-- OPTIONAL: … -->` | Section that may not apply | Keep it only if the user confirms it applies; otherwise delete the section. |

Some placeholders are local to one section (e.g. `{{FEATURE_1}}`, `{{OPTION_NAME}}`,
`{{ROADMAP}}`, `{{CODE_STYLE}}` in `README.md` / `CONTRIBUTING.md`): the `AI:` or
`OPTIONAL:` comment next to them says which question to ask.

Plain HTML comments that do **not** start with `AI:` or `OPTIONAL:` are guidance for
human contributors (e.g. in the PR template). **Keep them.**

Detect what remains to be done:

```bash
grep -rnE '\{\{[A-Z0-9_]+\}\}|<!-- (AI|OPTIONAL):|# AI:' --exclude-dir=.git --exclude-dir=.template .
```

The repository is initialized when this command returns nothing.

---

## 2. Ground rules

1. **Never invent a value.** Every placeholder is either (a) inferred from facts in the
   repository and *confirmed* by the user, or (b) given by the user. If the user does not
   know, remove the sentence/section, or leave an explicit `TODO:` agreed with the user.
2. **Infer before asking.** Read `git remote -v`, the default branch, `LICENSE`,
   `.gitignore`, and any manifest (`package.json`, `pyproject.toml`, `Cargo.toml`,
   `go.mod`, `CMakeLists.txt`, `*.csproj`, `pom.xml`, …). Propose these values as
   defaults instead of asking open questions.
3. **Ask in small batches** (3–4 questions max per batch, grouped by theme below).
   Offer choices with a recommended default whenever possible.
4. **Do not overwrite work already done.** If a file no longer contains placeholders,
   it is already initialized: leave it alone unless the user asks.
5. **Keep the Code of Conduct verbatim** (Contributor Covenant 2.1). Only
   `{{CONDUCT_CONTACT}}` may change.
6. Before writing, **show the user a summary** of all collected values and get a
   final confirmation.

---

## 3. Questionnaire

### Batch A — Identity

| Placeholder | Question to ask | Inference source |
|---|---|---|
| `{{PROJECT_NAME}}` | What is the project's name? | repo name, manifest |
| `{{PROJECT_TAGLINE}}` | In one sentence, what does the project do and for whom? | manifest `description` |
| `{{PROJECT_DESCRIPTION}}` | Describe the problem it solves and why it exists (2–5 sentences). | — |
| `{{GITHUB_OWNER}}` / `{{GITHUB_REPO}}` | Confirm the GitHub owner and repository name. | `git remote -v` |
| `{{DEFAULT_BRANCH}}` | Confirm the default branch. | `git symbolic-ref refs/remotes/origin/HEAD` |
| `{{PROJECT_STATUS}}` | Maturity: experimental / alpha / beta / stable / maintenance-only? | — |

### Batch B — Technical stack & commands

| Placeholder | Question to ask | Inference source |
|---|---|---|
| `{{LANGUAGE_STACK}}` | Main language(s), frameworks and runtime versions? | manifests, `.gitignore` |
| `{{PREREQUISITES}}` | What must be installed before building (tools + versions)? | manifests, CI files |
| `{{INSTALL_COMMAND}}` | How do users install the project? | manifests |
| `{{BUILD_COMMAND}}` | How do developers build it? | scripts, Makefile |
| `{{TEST_COMMAND}}` | How are tests run? | scripts, CI |
| `{{LINT_COMMAND}}` | How are lint/format checks run? (none is a valid answer) | config files |
| `{{USAGE_EXAMPLE}}` | Show a minimal usage example (command or code). | — |

Also ask: **Is `.gitignore` suitable for this stack?** (The template ships a C/C++
`.gitignore`.) If not, replace it with the matching one from
<https://github.com/github/gitignore>.

### Batch C — People & contacts

| Placeholder | Question to ask | Notes |
|---|---|---|
| `{{MAINTAINER_NAME}}` | Who maintains the project (name or GitHub handle)? | |
| `{{CONDUCT_CONTACT}}` | Where should Code of Conduct violations be reported (email preferred)? | Must be private. |
| `{{SECURITY_CONTACT}}` | How should vulnerabilities be reported privately? | Recommend GitHub private vulnerability reporting: `https://github.com/{{GITHUB_OWNER}}/{{GITHUB_REPO}}/security/advisories/new` (must be enabled in *Settings → Code security*). An email is an alternative. |
| `{{SUPPORT_CHANNEL}}` | Where should users ask questions? (GitHub Discussions, chat, forum, none) | If Discussions: must be enabled in repo settings. |
| `{{CODEOWNERS}}` | Who must review which parts of the code? | Used in `.github/CODEOWNERS`; can be deleted if single maintainer. |

### Batch D — Policies

| Placeholder | Question to ask | Default to propose |
|---|---|---|
| `{{LICENSE_NAME}}` | Keep the current license? | Read `LICENSE` (currently GPL-3.0). If changed, replace `LICENSE` with the official text from <https://choosealicense.com>. |
| `{{SUPPORTED_VERSIONS}}` | Which versions receive security fixes? | "latest release only" |
| `{{SECURITY_RESPONSE_TIME}}` | Within how long do you acknowledge a security report? | Ask; do not promise a delay the maintainer did not choose. |
| `{{COMMIT_CONVENTION}}` | Commit message convention? | Conventional Commits <https://www.conventionalcommits.org/en/v1.0.0/> or "none" |
| `{{BRANCH_NAMING}}` | Branch naming convention? | `type/short-description` |
| `{{VERSIONING}}` | Versioning scheme? | SemVer <https://semver.org/spec/v2.0.0.html> |

### Batch E — Issue & PR forms

Ask:

1. Which **components/areas** exist (for the "Component" dropdowns in issue forms)?
   → replace `{{COMPONENTS}}` list in `.github/ISSUE_TEMPLATE/*.yml`.
2. Which **environments** matter for bug reports (OS, browser, runtime version, …)?
   → adapt the environment fields in `bug_report.yml`.
3. Should blank issues be allowed? (default: no) → `.github/ISSUE_TEMPLATE/config.yml`.

---

## 4. Files to initialize

| File | Purpose |
|---|---|
| `README.md` | Project presentation |
| `CONTRIBUTING.md` | How to contribute |
| `CODE_OF_CONDUCT.md` | Contributor Covenant 2.1 (only the contact) |
| `SECURITY.md` | Vulnerability reporting policy |
| `SUPPORT.md` | Where to get help |
| `CHANGELOG.md` | Keep a Changelog format |
| `AGENTS.md` | Project context for AI agents (`CLAUDE.md` imports it) |
| `.github/PULL_REQUEST_TEMPLATE.md` | PR form |
| `.github/ISSUE_TEMPLATE/*.yml` | Issue forms + `config.yml` |
| `.github/CODEOWNERS` | Review ownership (optional) |
| `LICENSE`, `.gitignore` | Confirm or replace (see batches B and D) |

---

## 5. Completion checklist

- [ ] The detection command of section 1 returns nothing.
- [ ] YAML files are valid (e.g. `python3 -c "import yaml,sys;[yaml.safe_load(open(f)) for f in sys.argv[1:]]" .github/ISSUE_TEMPLATE/*.yml`).
- [ ] Links point to the real owner/repository.
- [ ] The "Template initialization" section of `AGENTS.md` has been removed.
- [ ] The `.template/` directory has been deleted.
- [ ] Changes are committed (suggested message: `docs: initialize project files from template`).
- [ ] The user has been reminded of the GitHub settings to enable (private vulnerability
      reporting, Discussions, branch protection) — the agent cannot assume they are on.
