# Contributing to XS56K

Thank you for taking the time to contribute! This document explains how to report
problems, propose changes and get your pull request merged.

By participating, you agree to follow our [Code of Conduct](CODE_OF_CONDUCT.md).

## Ways to contribute

- **Report a bug** — open a [bug report](https://github.com/xplorer2716/XS56K/issues/new?template=bug_report.yml).
- **Suggest a feature** — open a [feature request](https://github.com/xplorer2716/XS56K/issues/new?template=feature_request.yml).
- **Improve the documentation** — open a [documentation issue](https://github.com/xplorer2716/XS56K/issues/new?template=documentation.yml) or a pull request directly.
- **Write code** — pick an issue labelled `good first issue` or `help wanted`.
- **Ask a question** — see [SUPPORT.md](SUPPORT.md). Please do not use issues for questions.
- **Report a vulnerability** — follow [SECURITY.md](SECURITY.md), never a public issue.

Before opening an issue, search the [existing issues](https://github.com/xplorer2716/XS56K/issues)
to avoid duplicates.

## Development setup

### Prerequisites

- [JUCE](https://juce.com/) 8.0.15

### Get the code

```bash
git clone https://github.com/xplorer2716/XS56K.git
cd XS56K
```

### Build and test

Build, test and lint commands are not defined yet (the project has no code beyond
documentation). This section will be updated once they exist.

All required commands must pass before you open a pull request.

## Workflow

1. **Discuss first** for anything larger than a small fix: open or comment on an issue
   so that the approach can be agreed before you invest time.
2. **Fork** the repository and create a branch from `main`.
   Branch naming: `type/short-description`.
3. **Make focused changes.** One pull request = one logical change.
4. **Add or update tests** for any behavior change.
5. **Update the documentation** (README, code comments, `CHANGELOG.md` under
   `[Unreleased]`) when behavior visible to users changes.
6. **Commit** following the commit convention below.
7. **Open a pull request** and fill in every section of the template.

## Commit messages

[Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/)

```text
feat(parser): support multi-line strings

Explain *why* the change is needed, not only what it does.

Closes #123
```

## Pull request review

- A maintainer will review your pull request. Please be patient and responsive to feedback.
- CI must be green before merging.
- Maintainers may squash commits when merging.

## Using AI assistants

AI-assisted contributions are welcome. You remain responsible for the change: review
and test everything, and do not submit code you do not understand. Agents working on
this repository should read [`AGENTS.md`](AGENTS.md).

## License

By contributing, you agree that your contributions will be licensed under the
AGPL-3.0-or-later license of this project.
