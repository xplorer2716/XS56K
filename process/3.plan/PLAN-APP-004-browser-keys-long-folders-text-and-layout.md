# PLAN-APP-004: Editor Application — A Shorter Browser Menu, the Space Bar, Long Folder Names, Larger Text and Draggable Areas

## Overview

Session APP (2026-10-15, second session of the day). After using the editor on the real S5000, the owner asked for six changes:

1. remove "Refresh the list of disks" from the browser's menu (reading the disks never needs it, and it hung the S5000): RQ-APP-034 withdrawn,
   RQ-APP-027 amended;
2. remove "Open" from the menu, which repeats the double click: RQ-APP-027 amended;
3. start and stop the audition of the selected sample with the space bar, in the browser and the memory view: RQ-APP-083 (new);
4. allow a folder of more than 26 characters, which cannot be opened, to be renamed to a shorter name: RQ-MCP-068 and RQ-APP-031 amended;
5. make every text two points larger: RQ-APP-084 (new), ADR-APP-002 amended;
6. resize the three areas with the mouse by dragging the dark gaps between them: RQ-APP-008 amended (Could to Must), ADR-APP-002 DEC-APP-022.

Behaviour goes into the core or the gateway first, with tests against the simulated sampler (RQ-APP-014, DEC-APP-010); the views, which are not
unit-tested, are checked by running the application through `juce/tools/uia.ps1`. 6 tasks.

| Step | Task | What |
|---|---|---|
| 1 | TASK-APP-026 | the requirements, the ADR and this plan (done with the plan) |
| 2 | TASK-APP-027 | no "Open" and no refresh of the disks in the browser's menu |
| 3 | TASK-APP-028 | the space bar starts and stops the audition of the selected sample |
| 4 | TASK-APP-029 | a folder too long to be opened can be renamed to a name of 26 characters or fewer |
| 5 | TASK-APP-030 | the type scale two pixels larger |
| 6 | TASK-APP-031 | the gaps between the areas dragged with the mouse |
| 7 | TASK-APP-032 | (added) a command on a folder of the shown disk selects that disk again first |
| 8 | TASK-APP-033 | (added) a log file of the errors beside the executable |

## References
- **Requirements**: RQ-APP-083, RQ-APP-084 (new); RQ-APP-008, RQ-APP-027, RQ-APP-031, RQ-MCP-068 (amended); RQ-APP-034 (withdrawn); RQ-APP-033,
  RQ-APP-025, RQ-APP-079, RQ-APP-006, RQ-APP-009 (concerned); RQ-APP-001 to RQ-APP-005 (FTR-APP-001, design system); RQ-APP-014
- **ADRs**: ADR-APP-001 (DEC-APP-001, DEC-APP-002, DEC-APP-004, DEC-APP-010); ADR-APP-002 (DEC-APP-011 to DEC-APP-014, DEC-APP-022); ADR-MCP-003
  (DEC-MCP-020)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-APP-026: The requirements, the ADR and the plan of the six changes
- **Tier**: M
- **Status**: Done
- **Description**: RQ-APP-083 and RQ-APP-084 written; RQ-APP-008, RQ-APP-027, RQ-APP-031 and RQ-MCP-068 amended; RQ-APP-034 withdrawn; DEC-APP-022 and the
  type-scale amendment added to ADR-APP-002; this plan.
- **Requirement refs**: RQ-APP-008, RQ-APP-027, RQ-APP-031, RQ-APP-034, RQ-APP-083, RQ-APP-084, RQ-MCP-068
- **ADR refs**: ADR-APP-002 (DEC-APP-022)
- **Acceptance Criteria** (Gherkin): *Given* the process index rebuilt, *Then* it lists RQ-APP-083, RQ-APP-084, DEC-APP-022, PLAN-APP-004 and TASK-APP-026 to
  TASK-APP-031 with no duplicate ID.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: the `agnos-index` skill, 2026-10-15: exit 0, no duplicate; the IDs listed in the index.
- **Assumptions**: (1) "2 pt" read as two logical pixels on each text size, the unit of the tokens. (2) The heights of the rows, tabs and status bar stay as they
  are unless a text is cut (checked in TASK-APP-030). (3) The space bar acts in the browser and the memory view (owner, 2026-10-15), on samples only: song files
  have no play command in the SysEx. (4) Only the rename of a long folder is opened up; its open, load and deletion stay refused. (5) The sizes of the areas are
  not kept between runs (RQ-APP-009 stays Could).

### TASK-APP-027: No "Open" and no refresh of the disks in the browser's menu
- **Tier**: M
- **Status**: Done
- **Description**: In `xs56k_app_core`, `commandsFor` no longer gives `Open` for a folder nor `RefreshDisks` for a disk, and both commands leave
  `BrowserCommand`; `FileBrowser::refreshDisks` and its confirmation are removed, with their tests; the browser view loses the two menu items. The README's
  sentence on the refresh is updated. The MCP server's refresh (DEC-MCP-020) is untouched.
- **Requirement refs**: RQ-APP-027, RQ-APP-034 (withdrawn), RQ-APP-025, RQ-APP-079
- **ADR refs**: ADR-APP-001 (DEC-APP-001, DEC-APP-010); ADR-MCP-003 (DEC-MCP-020)
- **Acceptance Criteria** (Gherkin): *Given* the folder DRUMS, *When* its commands are asked, *Then* they are load the folder, create a folder, rename and delete;
  *Given* a disk, *Then* only create a folder; *Given* the application connected, *When* the context menu of a folder and of a disk is listed through the
  accessibility tree, *Then* neither "Open" nor "Refresh the list of disks…" is in it, and the double click still opens a folder.
- **Dependencies**: TASK-APP-026
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-15: the amended case of `BrowserTests.cpp` failed first (folder `{0,3,4,5,6}`, disk `{4,8}`), then passed;
  the refresh test removed with the command. `ctest -C Debug`: 100% passed, 0 failed out of 1192; warning-clean. The view (menu without the two items) is
  checked with the next task's run of the application, the owner's instance holding the executable.
- **Assumptions**: None

### TASK-APP-028: The space bar starts and stops the audition
- **Tier**: M
- **Status**: Done
- **Description**: `toggleAudition(item)` in `FileBrowser` and `MemoryController` (start, stop, or nothing if not a sample); both views call it on the space bar.
- **Requirement refs**: RQ-APP-083, RQ-APP-033, RQ-APP-041
- **ADR refs**: ADR-APP-001 (DEC-APP-001, DEC-APP-004, DEC-APP-010)
- **Acceptance Criteria** (Gherkin): see RQ-APP-083, in the core against the simulated sampler; *Given* the application connected to the real S5000 and a
  sample selected, *When* the space bar is sent twice through `uia.ps1`, *Then* the owner hears the sample start, then stop.
- **Dependencies**: TASK-APP-026
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-15: the two new cases (`BrowserCommandTests.cpp`, `MemoryCommandTests.cpp`) failed first to compile (no
  `toggleAudition`), then passed; `ctest -C Debug`: 100% passed, 0 failed out of 1194; warning-clean. On the real S5000 through `uia.ps1`: the space bar twice
  on `100_1 001 100.000 BPM.wav` in the browser, then on the same sample loaded in the memory view: no problem reported. The sound itself is the owner's to hear.
  TASK-APP-027's view checked in the same run: a folder's menu is load the folder, create a folder, rename, delete; a disk's is create a folder.
- **Assumptions**: (1) The space bar acts only while the list has the keyboard focus. (2) The test sample was loaded into the owner's empty memory.

### TASK-APP-029: A folder too long to be opened can be renamed shorter
- **Tier**: M
- **Status**: Done
- **Description**: In `xs56k_sampler`, the gateway's `renameFolder` checks the 26-character limit on the new name only; open, load, create and delete keep checking
  the name they send. The MCP README's sentence and the guard's message ("rename the folder to 26 characters or fewer") follow. The rename of the 29-character
  test folder of `S5K:\A B C` is tried on the real S5000 with the owner present.
- **Requirement refs**: RQ-MCP-068, RQ-APP-031
- **ADR refs**: ADR-APP-001 (DEC-APP-002)
- **Acceptance Criteria** (Gherkin): see RQ-MCP-068 (the amended case in `DiskFileOpsTests.cpp`: the 27-character folder renamed `SHORT` sends the rename; renamed
  to 27 characters, refused); *Given* the application on the real S5000, *When* `ABCDEFGHIJKLMNOPQRSTUVWXYZ123` is renamed to a short name, *Then* the
  sampler answers and the tree shows the new name, which opens.
- **Dependencies**: TASK-APP-026
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-15: the amended case of `DiskFileOpsTests.cpp` failed first (rename of the 27-character folder to SHORT
  refused, `DiskRenameFolder` 0), then passed; `ctest -C Debug`: 100% passed, 0 failed out of 1194; warning-clean. Real S5000: not tried, the 29-character test folder of `S5K:\A B C` is
  no longer on the disk (root listed 2026-10-15) and the application cannot create one (RQ-MCP-068). Closed without
  the real run at the owner's choice (2026-10-15).
- **Assumptions**: (1) Found while testing: `uia.ps1 invoke -Name XS56K` took the window, not its menu; the window is no longer a target (checked:
  `invoke XS56K`, `invoke Quit` quit the editor).

### TASK-APP-030: The type scale two pixels larger
- **Tier**: M
- **Status**: Done
- **Description**: In `juce/app/design/tokens.json`, the global text sizes become 13, 15, 17 and 20 (renamed after their values) and the semantic font tokens alias
  them as RQ-APP-084 says; the token module, the design-system document's tables and the mockups are regenerated; the main window is captured connected and
  compared with the capture before.
- **Requirement refs**: RQ-APP-084, RQ-APP-001, RQ-APP-002, RQ-APP-004, RQ-APP-074, RQ-APP-077
- **ADR refs**: ADR-APP-002 (DEC-APP-011, DEC-APP-012, DEC-APP-014); ADR-APP-003 (DEC-APP-020)
- **Acceptance Criteria** (Gherkin): see RQ-APP-084; *Given* `generate_design.py --check` and `generate_mockups.py --check`, *Then* they pass; the scripts'
  tests that name the old sizes are updated to the new expected values only.
- **Dependencies**: TASK-APP-026
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-15: the new case of `CoreTests.cpp` (RQ-APP-084) failed first (5 of 5), then passed once the tokens were
  13/13/15/17/20 and regenerated; `ctest -C Debug`: 100% passed, 0 failed out of 1195 (token, mockup and literal checks among them); warning-clean. Captured
  connected to the real S5000: the connection text ("... DeviceID 0") and the free memory were broken or cut at the new size; fixed (the connection 4/9 of the
  status line instead of 3/9, the free memory on two lines over the column's width above Refresh); captured again, both whole.
- **Assumptions**: (1) The knob-label width note of DESIGN-SYSTEM-APP.md §4 rewritten for 13 px, to be measured with the edit panel. (2) Names cut at the
  column's minimum width (long file names, the path) were cut before too and keep their tooltip.

### TASK-APP-031: The gaps between the areas dragged with the mouse
- **Tier**: M
- **Status**: Done
- **Description**: In `XS56KApp`, `MainComponent` lays the left column and the edit panel, and the browser and the memory view, out with a
  `juce::StretchableLayoutManager` each and a `juce::StretchableLayoutResizerBar` in each gap, the minimums from the layout tokens; the look-and-feel draws the
  bars as DEC-APP-022 says, with the new component token `resizer.bar.active`.
- **Requirement refs**: RQ-APP-008, RQ-APP-006, RQ-APP-005, RQ-APP-001, RQ-APP-004
- **ADR refs**: ADR-APP-002 (DEC-APP-013, DEC-APP-014, DEC-APP-022); ADR-APP-001 (DEC-APP-001)
- **Acceptance Criteria** (Gherkin): see RQ-APP-008; *Given* the application started, *Then* the window looks as before (the first layout of RQ-APP-006); *Given*
  the token and literal checks, *Then* they pass. The drag itself is done by the owner's mouse (the assistant never simulates mouse clicks).
- **Dependencies**: TASK-APP-030
- **Assignee**: AI and Human (the drag)
- **Verification**: Windows/MSVC, Debug, 2026-10-15: the new case of `CoreTests.cpp` (the two resizer tokens) failed first to compile, then passed;
  `ctest -C Debug`: 100% passed, 0 failed out of 1196 (literal and token checks among them); warning-clean. The window captured at start: the same layout
  as before. The drag checked by the owner the same day: OK.
- **Assumptions**: (1) The accent is a line of `stroke.focus` across the middle of the gap, not the whole gap. (2) A new component token `resizer.bar`
  (the gap's colour) beside `resizer.bar.active`.

### TASK-APP-032: The shown disk selected again before a command on its folders
- **Tier**: M
- **Status**: Done
- **Description**: In `FileBrowser`, every command on a folder or a file (open, load, create, rename, delete, audition) passes the shown disk to `moveTo`, which
  selects it again when the sampler is on another one.
- **Requirement refs**: RQ-APP-025, RQ-APP-024, RQ-APP-030
- **ADR refs**: ADR-APP-001 (DEC-APP-001, DEC-APP-010)
- **Acceptance Criteria** (Gherkin): *Given* the browser on HD1 and the sampler on another disk, *When* a folder is created in DRUMS, *Then* HD1 is selected
  again and the folder is created, with no problem.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-15: the new case of `BrowserCommandTests.cpp` failed first (a problem, no NEW in DRUMS), then passed
  (one more Select Disk, NEW created); `ctest -C Debug`: 100% passed, 0 failed out of 1197; libraries warning-clean (the executable not relinked: the
  owner's instance was running). On the real S5000: the owner's case (No disk, then a folder created in S5K)
  replayed by the owner: OK.
- **Assumptions**: (1) One more Get Disk List per command on a folder, the price of the check.

### TASK-APP-033: A log file of the errors
- **Tier**: M
- **Status**: Done
- **Description**: `xs56k_app_core` links `xs56k_framework`; `Model` logs problems and connection changes; `XS56KApp` installs a `juce::FileLogger` sink on
  `log/XS56K.log` beside the executable (fallback: application data) and logs the start with the version.
- **Requirement refs**: RQ-APP-085
- **ADR refs**: ADR-APP-001 (DEC-APP-023)
- **Acceptance Criteria** (Gherkin): see RQ-APP-085, in the core with a test sink; the file written by the application on the real S5000.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-15: the new case of `CoreTests.cpp` failed first (no line), then passed; `ctest -C Debug`: 100% passed,
  0 failed out of 1198; warning-clean. A run on the real S5000 wrote `juce/build/app/XS56KApp_artefacts/Debug/log/XS56K.log`: started with the version,
  Connecting, Connected, Disconnected, stopped.
- **Assumptions**: (1) Info level, one domain (CTL); no settings for them yet. (2) The sink sits with the JUCE adapters (`JuceAdapters.cpp`). (3) The
  timestamps are UTC, as the framework's `Logger` writes them.
