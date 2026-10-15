---
name: new-source-file
description: "Create a new C++ source or header file of XS56K with the project's licence header copied from juce/LICENSE_HEADER.txt, never typed or regenerated. USE FOR: any new .cpp/.hpp under juce/. Not part of AGNOS."
argument-hint: "<path> [--body FILE]"
---

# New source file with the licence header

Canonical procedure, shared by GitHub Copilot (`.github/skills/new-source-file/`) and Claude Code (`.claude/skills/new-source-file/`, which
points here; the subagent `.claude/agents/license-header.md` follows it).

The licence header (AGPL-3.0, 17 lines) lives once in `juce/LICENSE_HEADER.txt`. Copy it, never retype it.

## Steps

1. Write the file's content **without** the header to a temporary file (the scratchpad), or nothing for an empty file.
2. `python juce/tools/new_source_file.py <path> --body <temporary file>` (`python3` on Linux and macOS): it writes the header, then the body,
   with LF line endings, and refuses to overwrite an existing file (exit 1).
3. Check files when in doubt: `python juce/tools/new_source_file.py --check <path> ...` names those that do not start with the header.
4. Add the new file to its `CMakeLists.txt` as usual.

Python scripts (`juce/tools/*.py`) and files that are not C++ do not take this header.
