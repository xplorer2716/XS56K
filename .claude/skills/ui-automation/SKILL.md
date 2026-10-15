---
name: ui-automation
description: "Drive and check the XS56K editor (or any desktop application on Windows) without the mouse, through Windows UI Automation: list the window's elements with their texts and tooltips, invoke a menu item or a row, type keys, capture the window. USE FOR: verifying a UI task on the running application, reading the status bar, reproducing what the owner did. Windows only. Not part of AGNOS."
argument-hint: "list | invoke <name> | type <name|edit index> <keys> | screenshot <file>"
---

# UI Automation (Claude Code entry point)

A thin pointer: read and follow the full procedure at `.github/skills/ui-automation/SKILL.md`. The tool is `juce/tools/uia.ps1`. If the
two files ever disagree, the `.github` version is authoritative.
