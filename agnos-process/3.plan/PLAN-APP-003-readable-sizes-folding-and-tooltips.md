# PLAN-APP-003: Editor Application — Readable Sizes, Folding Folders and Tooltips of Cut Texts

## Overview

Session APP (2026-10-15). After trying the browser and the memory view of PLAN-APP-002, the owner asked for three changes the same day: sizes written
in kilobytes or megabytes as they read best (binary units, one decimal, the exact count of bytes in the tooltip), a double click on an open folder
that folds it, and a tooltip with the whole text wherever a text of the window is cut short, as the rows of the lists already do. The first is
an amendment of RQ-APP-026 and RQ-APP-042 through a new RQ-APP-081, the others are the new RQ-APP-079 and RQ-APP-080.

Each change is made core first where it has behaviour (the size text, the folded folders), with its tests against the simulated sampler
(RQ-APP-014, DEC-APP-010), then in the views. 3 tasks.

| Step | Task | What |
|---|---|---|
| 1 | TASK-APP-020 | sizes in bytes, KB or MB, in the browser and the memory view |
| 2 | TASK-APP-021 | a double click folds an open folder, and opens a folded one |
| 3 | TASK-APP-022 | a tooltip with the whole text on every text that is cut short |
| 4 | TASK-APP-023 | the advice of a refusal in the application's words, not the MCP server's tool names (added the same day) |
| 5 | TASK-APP-024 | no folder name over 26 characters sent to the sampler, which crashes on them (added the same day) |
| 6 | TASK-APP-025 | the UI Automation scripts of the session made a generic tool with its skill (added the same day) |

## References
- **Requirements**: RQ-APP-079, RQ-APP-080, RQ-APP-081, RQ-APP-082, RQ-MCP-068 (new); RQ-APP-024, RQ-APP-025, RQ-APP-026, RQ-APP-042 (amended or concerned); RQ-APP-001 to
  RQ-APP-005 (FTR-APP-001, design system); RQ-APP-014
- **ADRs**: ADR-APP-001 (DEC-APP-001, DEC-APP-004, DEC-APP-010); ADR-APP-002 (DEC-APP-011 to DEC-APP-016)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-APP-020: Sizes in bytes, kilobytes or megabytes
- **Tier**: M
- **Status**: Done
- **Description**: In `xs56k_app_core`, a function that writes a size as RQ-APP-081 says ("512 bytes", "4.0 KB", "114.6 KB", "151.2 MB"); the browser view uses it for
  the selected file and the free space of the disk, with the exact count of bytes (`bytesText`) in the tooltip, and the memory view for the free and total wave memory.
- **Requirement refs**: RQ-APP-081, RQ-APP-026, RQ-APP-042, RQ-APP-024, RQ-APP-001
- **ADR refs**: ADR-APP-001 (DEC-APP-001, DEC-APP-010); ADR-APP-002 (DEC-APP-014)
- **Acceptance Criteria** (Gherkin): *Given* 0, 512, 1 023, 1 024, 4 096, 117 398, 1 048 575, 1 048 576 and 158 548 694 bytes, *Then* "0 bytes", "512 bytes", "1023 bytes",
  "1.0 KB", "4.0 KB", "114.6 KB", "1.0 MB" (1 023.999 KB rounds to the next unit), "1.0 MB" and "151.2 MB"; *Given* a file of 117 398 bytes selected, *Then* the browser
  shows "114.6 KB" and its tooltip "117 398 bytes"; *Given* 158 281 898 bytes free of 158 548 694, *Then* the memory view shows "150.9 MB free of 151.2 MB".
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-15: the new case of `BrowserTests.cpp` (RQ-APP-081) failed first (link error, no `sizeText`), then, once written, failed
  on one value: 158 281 898 bytes is 150.95 MB, "150.9 MB", not the "151.0 MB" the test and this task's criterion said (an error of mine in the expected value,
  corrected in both); then passed: "0 bytes", "512 bytes", "1023 bytes", "1.0 KB", "4.0 KB", "114.6 KB", "1.0 MB" for 1 048 575 and 1 048 576, "151.2 MB",
  "not reported". `ctest -C Debug`: 100% passed, 0 failed out of 1189; build warning-clean. On the real S5000: the selected file "Half-Cutoff-sqr-bass-C.wav - sample
  - 114.6 KB" with the tooltip "Half-Cutoff-sqr-bass-C.wav - 117 398 bytes"; the memory view "150.2 MB free of 151.2 MB (99 %), MPKS 100 % free" with the tooltip
  "157 461 530 bytes free of 158 548 694 bytes (99 %), MPKS 100 % free" (read through the accessibility tree).
- **Assumptions**: (1) A value that rounds to 1024.0 in its unit is written in the next unit ("1.0 MB", not "1024.0 KB"). (2) The decimal mark is the point, as in the
  rest of the English interface. (3) The free space of a disk keeps "not reported" for 0 (RQ-APP-024). (4) Gigabytes added beyond the owner's "kilo ou mega" for the
  free space of a large disk, RQ-APP-081 amended to say so. (5) The free space of a disk is written by `sizeText` too, so the existing case of RQ-APP-024 now expects
  "976.6 KB" for 1 000 000 bytes (a behaviour the amended requirement changed, not a test made to pass). (6) The path, the details and the free memory became labels
  (`juce::Label`) so that they carry a tooltip.

### TASK-APP-021: Folding an open folder
- **Tier**: M
- **Status**: Done
- **Description**: In `xs56k_app_core`, the browser's state records the folded folders, and `FileBrowser` folds a folder (nothing sent) or opens a folded one (as
  `openFolder`, which unfolds it); the browser view hides what a folded folder holds and calls one or the other on a double click of a folder, by its state.
- **Requirement refs**: RQ-APP-079, RQ-APP-025
- **ADR refs**: ADR-APP-001 (DEC-APP-001, DEC-APP-004, DEC-APP-010)
- **Acceptance Criteria** (Gherkin): *Given* DRUMS opened, *When* it is folded, *Then* the model says it is folded and nothing is sent; *When* it is opened again, *Then*
  it is opened on the sampler, listed and no longer folded; *Given* DRUMS folded, *When* a sub-folder of it is opened from elsewhere (a load in it), *Then* DRUMS is
  unfolded; the application shows DRUMS without its files once folded.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-15: the two new cases of `BrowserTests.cpp` (RQ-APP-079) failed first to compile (no `fold`, no `isFolded`), then
  passed: DRUMS folded, nothing sent, its listing kept; opened again, sent, listed, no longer folded and current; DRUMS folded then DRUMS\LOOPS opened: DRUMS
  unfolded. `ctest -C Debug`: 100% passed, 0 failed out of 1191; build warning-clean. On the real S5000, the session opening in MoogBass: a first double click
  (through the accessibility tree: select, then Return) on "MoogBass (current)" left it with its 7 files shown, a bug of the tree's branch for a path not yet
  listed, which did not look at the folding; fixed; then the double click folded MoogBass (the rows after it are the memory view's) and a second one opened it
  again on the sampler with its files.
- **Assumptions**: (1) Folding is a state of the view kept in the model (DEC-APP-004), lost on disconnection like the rest of the browser's state. (2) Opening a folder
  unfolds it and every folder on its path, so the current folder is always shown. (3) A double click on a disk keeps its present meaning (open its root). (4) A folder
  "shows what it holds" when it has been listed and is not folded; a folder never listed is opened by the double click.

### TASK-APP-022: Tooltips of the texts cut short
- **Tier**: M
- **Status**: Done
- **Description**: In `XS56KApp`, a text component that gives its whole text as its tooltip when, and only when, it does not fit its bounds, used for the texts drawn
  by the views (the browser's path and details, the memory view's free memory, the status bar's connection, job and problem, the areas' titles and hints, the
  dialogs' labels); the rows keep theirs. The delay before a tooltip appears becomes a design-system token (`motion.tooltip`).
- **Requirement refs**: RQ-APP-080, RQ-APP-001, RQ-APP-002, RQ-APP-004, RQ-APP-005
- **ADR refs**: ADR-APP-002 (DEC-APP-011, DEC-APP-013, DEC-APP-014); ADR-APP-001 (DEC-APP-001)
- **Acceptance Criteria** (Gherkin): *Given* the window at its minimum size and the path "S5K:\\Soul - Funky - Acid jazz", *When* the pointer rests on the path, *Then*
  its whole text is shown in a tooltip; *Given* "Disks", which fits, *Then* no tooltip; *Given* the token check and the visual-literal check, *Then* they pass.
- **Dependencies**: TASK-APP-020
- **Assignee**: AI
- **Verification**: (partial, 2026-10-15) Windows/MSVC, Debug: build warning-clean (after renaming a local `font` that hid `Label::font`, C4458); `ctest -C Debug`:
  100% passed, 0 failed out of 1191, among them `app_sources_have_no_visual_literal`, `app_design_tokens_are_up_to_date` (with `motion.tooltip`) and
  `app_mockups_are_up_to_date`. On the real S5000, the tooltips read through the accessibility tree: disconnected, "Disks", "Memory", "Editor", the three hints and
  "Disconnected" have none (they fit); connected, the path "S5K:\MoogBass", "S5K - hard disk - FAT32 - free not reported", the connection text and a refusal on two
  lines ("Creating the program XS56K TOOLTIP TEST 1 failed: A program named ... is already in memory.") have none, the file's details and the free memory keep theirs
  (the exact bytes). No text of that session was long enough to be cut, so the tooltip of a cut text is still to be seen, by the owner: a long folder path or a long
  file name in the browser, the pointer resting on it. A first measure of the cut (the text's width against the label's lines) would have missed texts broken
  between words; it measures the broken text now. Seen later the same day on the real S5000: the status bar's problem "Opening the folder A B C\ABCDEF...\
  ABCDEFGHIJKLMNOPQRSTUVWXYZ123 failed: The sampler did not answer ..." cut short in its two lines had its whole text as its tooltip (read through the
  accessibility tree), while the shorter problems before it had none.
- **Assumptions**: (1) The views are not unit-tested (DEC-APP-010): checked by a run of the application, with the tooltip shown through the accessibility tree or a capture.
  (2) `motion.tooltip` is a new global motion token of 700 ms, JUCE's default, the mockups having no tooltip. (3) Texts already given whole (the rows' names) keep their
  tooltip as it is. (4) A label whose tooltip says more than its text (the exact count of bytes) keeps that tooltip whether or not it is cut. (5) The areas' titles and
  hints became labels, drawn as before, so that they can carry the tooltip. (6) The test program XS56K TOOLTIP TEST 1 was created and deleted on the owner's sampler.

### TASK-APP-023: The advice of a refusal in the application's words
- **Tier**: M
- **Status**: Done
- **Description**: In `xs56k_sampler`, `GatewayConfig` gains the advice given when nothing of a kind is selected, as a function of the kind; empty, the gateway keeps
  the MCP server's "Use select_<kind> first."; the five places that wrote it use it. In `xs56k_app_core`, the controller sets the application's advice ("Select a
  <kind> first.").
- **Requirement refs**: RQ-APP-082, RQ-APP-010, RQ-MCP-044
- **ADR refs**: ADR-APP-001 (DEC-APP-002, the gateway shared with the MCP server); ADR-MCP-001
- **Acceptance Criteria** (Gherkin): *Given* a session of the application with no disk selected, *When* the root is opened, *Then* the problem says "Select a disk
  first." and has no "select_"; *Given* the MCP server's tests, *Then* they pass unchanged.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-15: the new case of `BrowserTests.cpp` (RQ-APP-082) failed first (the problem held "select_" at 126 and no "Select
  a disk first."), then passed once the gateway took the advice from its configuration and the controller gave the application's; `ctest -C Debug`: 100% passed, 0
  failed out of 1192, the MCP server's tests and scripted conversations among them, unchanged ("Use select_disk first." kept by default). Libraries and tests
  warning-clean; the application's executable was not relinked (the owner's instance was running).
- **Assumptions**: (1) Asked by the owner on 2026-10-15 ("ok pour le reste") after the message "Use select_disk first." in the editor. (2) The cause of that refusal
  right after the sampler's restart (the root opened while the sampler had no disk selected) is looked at separately, at the next restart.

### TASK-APP-024: No folder name the sampler cannot take
- **Tier**: M
- **Status**: Done
- **Description**: In `xs56k_sampler`, the gateway's `openFolder`, `loadFolder`, `createFolder`, `renameFolder` and `deleteFolder` refuse a folder name, or a new
  name, longer than 26 characters before anything is sent, with a message that says why and how to rename the folder from the sampler; the editor and the MCP server
  both get it. The observation goes into `OBSERVATIONS-RQ-MCP-012-real-sampler.md` and `documents/_index/sysex_spec.kb.md`.
- **Requirement refs**: RQ-MCP-068, RQ-MCP-024, RQ-MCP-025, RQ-MCP-032, RQ-APP-025
- **ADR refs**: ADR-MCP-003 (DEC-MCP-019); ADR-APP-001 (DEC-APP-002)
- **Acceptance Criteria** (Gherkin): see RQ-MCP-068.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-15: the new case of `DiskFileOpsTests.cpp` (RQ-MCP-068) failed first, then passed: open, load, rename (old and new
  name), delete and create of a 27-character folder each refused with "26 characters" in the answer and none of `&13`, `&15`, `&16`, `&17`, rename sent; the
  26-character folder opened (`&13` once). `ctest -C Debug`: 100% passed, 0 failed out of 1193, the README checks among them; build warning-clean. On the real
  S5000, the application rebuilt: in `S5K:\A B C\ABCDEFGHIJKLMNOPQRSTUVWXYZ`, the double click on `ABCDEFGHIJKLMNOPQRSTUVWXYZ123` gave "The folder name ... has 29
  characters: the S5000 stops answering on a folder name of more than 26 characters, so nothing was sent. ..." and the sampler then answered a disconnection and a
  new connection. The observations (steps 1 to 6 with the two crashes) are in `OBSERVATIONS-RQ-MCP-012-real-sampler.md` and `sysex_spec.kb.md`.
- **Assumptions**: (1) The owner chose 26 (2026-10-15, "on garde 26"), the longest name seen opening, 27 and 28 untried. (2) The name is counted in characters
  as the person gives it, the sampler's names being plain ASCII. (3) The deletion, the load and the rename of a folder already named longer are refused too: whether
  those commands crash on such a name is not known, and the front panel renames it.

### TASK-APP-025: Driving the editor without the mouse, as a tool and a skill
- **Tier**: M
- **Status**: Done
- **Description**: The scripts written in the session's scratchpad to check the editor on the real S5000 (list the window's elements, invoke one, type keys
  only when the editor is in front, capture the window) become one generic tool, `juce/tools/uia.ps1`, with the skill `ui-automation`
  (`.github/skills/ui-automation/SKILL.md`, pointed to by `.claude/skills/ui-automation/SKILL.md`) and two lines of AGENTS.md: the tool, and the rule
  that a script used more than once becomes such a tool. AGNOS is not changed.
- **Requirement refs**: RQ-APP-014 (the behaviour checked without the owner's hands where it can be), RQ-APP-080 (the tooltips read through it)
- **ADR refs**: ADR-APP-001 (DEC-APP-010, the views checked by running the application)
- **Acceptance Criteria** (Gherkin): *Given* the editor running, *When* the tool lists, captures, invokes a menu and types Escape into its popup, *Then* each is
  done; *Given* an element or a process that does not exist, *Then* the tool says so and ends with 1; *Given* another program in front, *Then* no key is sent (2).
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Windows 11, PowerShell 5.1, 2026-10-15, the editor connected to the real S5000: `list -Filter` gave the menu bar, the texts and the tooltips
  (the free memory's exact bytes); `screenshot` saved the window; `invoke -Name Sampler` opened the menu ("Disconnect" listed in its popup); `type -Name Disconnect
  -Keys {ESC}` closed it; `invoke -Name "Nothing here"` and `-Process NoSuchApp` said "not found" and ended with 1. The foreground check is the one that refused no
  key in the session's earlier scripts (keys.ps1). Found while testing: while a JUCE popup is open, the menu bar leaves the accessibility tree (in the skill).
- **Assumptions**: (1) Asked by the owner on 2026-10-15 ("capitalise ces outils ... ne modifie pas agnos, ajoute cela à côté"). (2) One script with four actions
  rather than three scripts; PowerShell because UI Automation is a Windows interface. (3) The canonical skill sits in `.github/skills` with a pointer in
  `.claude/skills`, as the AGNOS skills do, so that Copilot and Claude Code both find it.
