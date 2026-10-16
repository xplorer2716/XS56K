---
name: ui-automation
description: "Drive and check the XS56K editor (or any desktop application on Windows) without the mouse, through Windows UI Automation: list the window's elements with their texts and tooltips, invoke a menu item or a row, type keys, capture the window. USE FOR: verifying a UI task on the running application (a real or simulated sampler), reading the status bar, reproducing what the owner did. Windows only. Not part of AGNOS."
argument-hint: "list | invoke <name> | type <name|edit index> <keys> | screenshot <file>"
---

# UI Automation of the editor

Canonical procedure, shared by GitHub Copilot (`.github/skills/ui-automation/`) and Claude Code (`.claude/skills/ui-automation/`, which
points here). The tool is `juce/tools/uia.ps1`; its header gives the usage and the exit codes.

## Why this and not the mouse

Simulated mouse clicks land on whatever window is in front: on 2026-10-14 two of them hit another program of the owner's desktop.
UI Automation addresses the application's own elements (JUCE exposes its components to it), and the tool sends keys only when the
application's window is in the foreground (exit code 2 otherwise). **Never simulate mouse clicks.**

## Steps

1. Build and start the application (`juce/build/app/XS56KApp_artefacts/<config>/XS56K.exe`, see AGENTS.md); leave it 4 s to open.
2. `powershell -NoProfile -File juce/tools/uia.ps1 -Action list [-Filter <regex>]`: every element of every window of the process, as
   `[Type] 'name' tip='tooltip'`. Read it before acting: names are exact (`"S5K - FAT32 (current)"`, `"Create a folder…"` with its ellipsis).
3. `-Action invoke -Name <name>`: a menu of the menu bar opens its popup, a popup item runs, a button is pressed, a row of a list is
   **selected and then given Return** (JUCE's press of a row): in the browser, Return on a folder opens or folds it.
4. `-Action type -Name <name> -Keys <keys>` or `-Action type -Edit <n> -Keys <keys>`: focuses the element (or the n-th text field, from 0,
   in the windows' order) and sends `SendKeys` text: `+{F10}` (Shift+F10, the context menu of the selected row), `{UP}`, `{DOWN 3}`,
   `{ESC}`, `^a` then text to replace a field. Escape `+ ^ % ~ ( ) { } [ ]` as `{+}` etc.
5. Give the application time to answer the sampler (2 to 10 s), then `list` again and read the status bar's texts.
6. `-Action screenshot -Out <file.png>` captures the main window even behind others; read the image to compare it with
   `documents/mockups/`.
7. Quit through the menu (`invoke XS56K`, then `invoke Quit`): the session with the sampler is closed properly. Never kill the process
   while it is connected.

## What the editor's accessibility does and does not do (JUCE 8)

- While a popup menu is open, the menu bar's items leave the tree: act on the popup's items (`type -Name <item> -Keys {ESC}` closes it).
- A list's rows outside its visible part are not in the tree, and a list cannot be scrolled through UI Automation: select a visible row,
  then move with `{UP}` / `{DOWN}` (each move selects the row: in the browser, selecting a disk that is not current selects it on the
  sampler, so never move onto "No disk").
- A text field takes no value through UI Automation: type into it (`-Edit <n> -Keys ...`).
- A row's tooltip is its accessible name; a label's tooltip is its help text (`tip=` in the listing).

## Safety with the owner's sampler

Every rule of AGENTS.md and of the memory applies to what the application sends: never the disk refresh, never a folder name of more
than 26 characters (the gateway refuses it, RQ-MCP-068), only test items created, renamed or deleted, and the owner present for anything
that can crash the sampler.
