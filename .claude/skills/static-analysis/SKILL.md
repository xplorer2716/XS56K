---
name: static-analysis
description: "Check the C++ and Python code a task changed before its commit: clang-tidy with the project's Sonar-matching checks (juce/tools/tidy.py), then the IDE's SonarQube findings, and the idioms of the Sonar rules clang-tidy cannot check. USE FOR: every task that changed code under juce/, before the commit; a whole-code check with --all. Not part of AGNOS."
argument-hint: "[files...] [--all] [--fix] [--tests] [--configure]"
---

# Static analysis (Claude Code entry point)

A thin pointer: read and follow the full procedure at `.github/skills/static-analysis/SKILL.md`. The tool is `juce/tools/tidy.py`, the checks are in
`juce/.clang-tidy`. If the two files ever disagree, the `.github` version is authoritative.
