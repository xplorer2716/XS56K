<!-- AI: Initialize this file by following .template/INIT.md. Adapt the workflow to the answers given by the user; delete steps that do not apply (e.g. no linter). -->

# Contributing to {{PROJECT_NAME}}

Thank you for taking the time to contribute! This document explains how to report
problems, propose changes and get your pull request merged.

By participating, you agree to follow our [Code of Conduct](CODE_OF_CONDUCT.md).

## Ways to contribute

- **Report a bug** — open a [bug report](https://github.com/{{GITHUB_OWNER}}/{{GITHUB_REPO}}/issues/new?template=bug_report.yml).
- **Suggest a feature** — open a [feature request](https://github.com/{{GITHUB_OWNER}}/{{GITHUB_REPO}}/issues/new?template=feature_request.yml).
- **Improve the documentation** — open a [documentation issue](https://github.com/{{GITHUB_OWNER}}/{{GITHUB_REPO}}/issues/new?template=documentation.yml) or a pull request directly.
- **Write code** — pick an issue labelled `good first issue` or `help wanted`.
- **Ask a question** — see [SUPPORT.md](SUPPORT.md). Please do not use issues for questions.
- **Report a vulnerability** — follow [SECURITY.md](SECURITY.md), never a public issue.

Before opening an issue, search the [existing issues](https://github.com/{{GITHUB_OWNER}}/{{GITHUB_REPO}}/issues)
to avoid duplicates.

## Development setup

### Prerequisites

{{PREREQUISITES}}

### Get the code

```bash
git clone https://github.com/{{GITHUB_OWNER}}/{{GITHUB_REPO}}.git
cd {{GITHUB_REPO}}
{{INSTALL_COMMAND}}
```

### Build and test

```bash
{{BUILD_COMMAND}}
{{TEST_COMMAND}}
{{LINT_COMMAND}}
```

All three commands must pass before you open a pull request.

## Workflow

1. **Discuss first** for anything larger than a small fix: open or comment on an issue
   so that the approach can be agreed before you invest time.
2. **Fork** the repository and create a branch from `{{DEFAULT_BRANCH}}`.
   Branch naming: {{BRANCH_NAMING}}.
3. **Make focused changes.** One pull request = one logical change.
4. **Add or update tests** for any behavior change.
5. **Update the documentation** (README, code comments, `CHANGELOG.md` under
   `[Unreleased]`) when behavior visible to users changes.
6. **Commit** following the commit convention below.
7. **Open a pull request** and fill in every section of the template.

## Commit messages

{{COMMIT_CONVENTION}}

<!-- AI: If the user chose Conventional Commits, keep the example below; otherwise replace it with an example of their convention or delete it. -->

```text
feat(parser): support multi-line strings

Explain *why* the change is needed, not only what it does.

Closes #123
```

## Code style

<!-- AI: Ask: "Is there a style guide, formatter or linter configuration contributors must follow?" Link the tool/config. Remove this section if none. -->

{{CODE_STYLE}}

## Pull request review

- A maintainer will review your pull request. Please be patient and responsive to feedback.
- CI must be green before merging.
- Maintainers may squash commits when merging.

## Using AI assistants

AI-assisted contributions are welcome. You remain responsible for the change: review
and test everything, and do not submit code you do not understand. Agents working on
this repository should read [`AGENTS.md`](AGENTS.md).

<!-- AI: Ask the user whether they want to keep this "Using AI assistants" policy, change it, or delete it. -->

## License

By contributing, you agree that your contributions will be licensed under the
{{LICENSE_NAME}} license of this project.
