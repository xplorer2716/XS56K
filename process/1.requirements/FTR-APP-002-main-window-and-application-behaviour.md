# FTR-APP-002: Main Window, Layout and Application-Wide Behaviour

## Overview

Session APP (2026-10-13). The owner's sketch of 2026-10-13 (a hand drawing, turned 90° to the left) gives the general look of the editor:
a **file browser** at the top left, to browse the sampler's disks and load from them; under it (not drawn) a **memory view** that shows by type
what is loaded in the sampler's memory; on the right an **edit panel** whose content depends on the item selected in the memory view and on the
tab selected in a row of tabs at the bottom of the panel (drawn F1 to F4). The working principle the owner gave: the person loads a file
from the browser, the item appears in the memory view, and selecting it there shows its editor.

This feature is the window itself and what every other feature shares: the menu bar, the behaviour while the sampler is busy, how errors are
reported, how destructive actions are confirmed and how the application is tested without the sampler.

**Priorities.** Priority 1 (mandatory) is written `Must`, priority 2 (nice to have) `Could` (owner, 2026-10-13).

**Depends on** FTR-APP-001 (every visual value), FTR-AKM-001 (session, timeouts, slow operations).

**Sources.** The owner's sketch and description (session APP, 2026-10-13); `juce/app/src/Main.cpp` (the placeholder window);
`process/1.requirements/FTR-AKM-001-transport-et-config-sysex.md`, `FTR-AKM-007-disk-tools.md` (RQ-AKM-070);
`process/2.architecture/OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`.

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the person editing the sampler; CI (simulated sampler only).

---

## Functional Requirements

### RQ-APP-006: Three areas in the main window
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The application's main window SHALL show three areas: the file browser (FTR-APP-004) at the top left, the memory view (FTR-APP-005) at the bottom left, under the browser, and the edit panel (FTR-APP-007) on the right, over the full height of the window, with its row of tabs at the bottom of the panel.
- **Rationale**: the owner's sketch and description of 2026-10-13.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the application started, *When* the main window is shown, *Then* the browser is at the top left, the memory view under it, and the edit panel on the right with its tabs along its bottom edge.
- **Dependencies**: RQ-APP-001; FTR-APP-004, FTR-APP-005, FTR-APP-007

### RQ-APP-007: A menu bar
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The main window SHALL have a menu bar that gives at least the application's MIDI setup (RQ-APP-015), connecting to and disconnecting from the sampler (RQ-APP-017, RQ-APP-021) and quitting the application.
- **Rationale**: the owner requires a menu for the MIDI setup of the application (2026-10-13).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the main window, *When* the person opens the menu bar, *Then* "MIDI setup", "Connect", "Disconnect" and "Quit" are offered, "Connect" only when disconnected and "Disconnect" only when connected.
- **Dependencies**: RQ-APP-001, RQ-APP-015, RQ-APP-017, RQ-APP-021

### RQ-APP-008: The areas can be resized
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person drags with the mouse the boundary between the left areas and the edit panel, or between the browser and the memory view (the dark gap of `space.edge` between them), the application SHALL resize the areas on both sides, down to a minimum size given by the design system (`layout.left.min`, `layout.area.min`, `layout.edit.min`); WHILE the pointer is over a boundary, it SHALL take the shape of a resize cursor.
- **Rationale**: a long file tree or a long list of samples needs room.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the main window, *When* the boundary between browser and memory view is dragged down, *Then* the browser grows and the memory view shrinks, never below its minimum size; *When* the boundary between the left column and the edit panel is dragged right, *Then* the left column widens and the edit panel narrows, never below its minimum width.
- **Dependencies**: RQ-APP-001, RQ-APP-006, ADR-APP-002 (DEC-APP-022)
- **Amended**: 2026-10-15, the owner asked for it (priority Could to Must) and named the dark gaps between the areas as the boundaries (TASK-APP-031). Keeping the sizes between runs stays RQ-APP-009 (Could).

### RQ-APP-009: The window comes back as it was left
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the application starts, it SHALL restore the size and position of the main window and the sizes of its areas as they were when it was last closed, and use the design system's default layout the first time.
- **Rationale**: the person does not rearrange the window at every start.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* the window resized and closed, *When* the application starts again, *Then* the window has the same size, position and area sizes.
- **Dependencies**: RQ-APP-001, RQ-APP-006, RQ-APP-008

### RQ-APP-010: Errors are reported, the state stays true
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a command to the sampler fails (an ERROR confirmation, a timeout, a lost MIDI port), THEN the application SHALL tell the person which action failed and why, in the sampler's own words when it gave an error number, and SHALL show the values it last read from the sampler, never the value it tried to set.
- **Rationale**: an editor that shows a value the sampler does not hold misleads the person.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler answering ERROR "parameter out of range" to a Set, *When* the person changes that parameter, *Then* the application says the change was refused and why, and the control shows the value read back from the sampler.
- **Dependencies**: RQ-APP-001, RQ-AKM-005, RQ-AKM-010

### RQ-APP-011: Destructive actions are confirmed
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF the person asks for an action that loses data or can leave the sampler unresponsive (deleting a file, a folder, a memory item or a keygroup; overwriting a file; refreshing the list of disks), THEN the application SHALL ask for a confirmation that names the object and what will be lost, and SHALL send nothing to the sampler unless the person confirms.
- **Rationale**: the sampler has no undo; the refresh of the disk list once left the owner's S5000 unresponsive until it was power-cycled (RQ-AKM-017).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a file KICK.WAV selected, *When* the person asks to delete it and cancels the confirmation, *Then* nothing is sent; *When* the person confirms, *Then* the delete is sent and the browser no longer lists KICK.WAV.
- **Dependencies**: RQ-APP-001, RQ-AKM-069, RQ-AKM-070

---

## Non-Functional Requirements

### RQ-APP-012: The window never freezes while the sampler works
- **Category**: Non-Functional
- **NFR Type**: Usability
- **EARS Type**: State-driven
- **Statement**: WHILE a command to the sampler is pending, the application SHALL keep redrawing and answering the person's input, and SHALL show that the sampler is busy and with what action.
- **Metric**: no call made on the UI thread waits for the sampler's answer.
- **Measurement Method**: a test against the simulated sampler holding its answer for longer than the test's deadline, checking that the UI thread's work completes before the answer is released.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler holding its answer to a Load File, *When* the person scrolls the memory view, *Then* it scrolls and the busy indicator names "Loading <file>".
- **Dependencies**: RQ-APP-001, RQ-AKM-020

### RQ-APP-013: Slow disk operations are shown as such
- **Category**: Non-Functional
- **NFR Type**: Reliability
- **EARS Type**: Complex
- **Statement**: WHILE a slow disk operation (load a file or a folder, save) runs, the application SHALL show it in progress and wait for it with the slow-operation timeout of RQ-AKM-070; IF the sampler stays silent past that timeout, THEN the application SHALL say that the sampler may have stopped answering and may need to be switched off and on.
- **Metric**: the timeout used for these commands is the slow-operation one, not the ordinary one.
- **Measurement Method**: a test against the simulated sampler that answers a load after the ordinary timeout and before the slow one (success), and one that never answers (the message above).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler answering a load after the ordinary timeout, *When* the person loads a file, *Then* the load succeeds; *Given* it never answers, *Then* the message says the sampler may need to be switched off and on.
- **Dependencies**: RQ-APP-001, RQ-AKM-070, RQ-AKM-017

### RQ-APP-014: Tested without the sampler
- **Category**: Non-Functional
- **NFR Type**: Maintainability
- **EARS Type**: Ubiquitous
- **Statement**: Every priority-1 behaviour of the FTR-APP features SHALL be exercised by automated tests that run against the simulated sampler (RQ-AKM-016) without a display, the behaviour being kept out of the JUCE components that draw it.
- **Metric**: each priority-1 RQ-APP of a functional kind is named by at least one test case.
- **Measurement Method**: `ctest` on the CI's headless runners; a grep of the RQ IDs in `juce/tests/`.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the test suite, *When* `ctest` runs on a runner with no display, *Then* the FTR-APP tests run and pass; *Given* a priority-1 functional RQ-APP, *When* its ID is searched in `juce/tests/`, *Then* at least one test case names it.
- **Dependencies**: RQ-AKM-016, RQ-BLD-014

### RQ-APP-080: A text cut short is shown whole in a tooltip
- **Category**: Non-Functional
- **NFR Type**: Usability
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a text of the window (a name, a label, a path, a status or a detail) does not fit the space it is drawn in, THEN the application SHALL show it whole in a tooltip once the pointer has rested on it for the design system's tooltip delay.
- **Metric**: every text component of the window that can be cut short gives its whole text as its tooltip when, and only when, it is cut.
- **Measurement Method**: a run of the application at its minimum size with long names (a capture of the tooltip), and a review of the components that draw text.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the path "S5K:\\Soul - Funky - Acid jazz" wider than the browser, *When* the pointer rests on it, *Then* the whole path is shown in a tooltip; *Given* the title "Disks", which fits, *Then* it has no tooltip.
- **Dependencies**: RQ-APP-001, RQ-APP-006

### RQ-APP-081: Sizes in the unit that reads best
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The application SHALL write a size in bytes under 1 KB ("512 bytes"), and otherwise in kilobytes, megabytes or gigabytes with one decimal (1 KB = 1 024 bytes, 1 MB = 1 024 KB, 1 GB = 1 024 MB: "114.6 KB", "151.2 MB", "29.8 GB" for the free space of a large disk), the unit chosen so that the number is below 1 024.
- **Rationale**: the owner's request of 2026-10-15: "117 398 bytes" or "158 281 898 bytes" cannot be read at a glance.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* 512 bytes, *Then* "512 bytes"; *Given* 4 096 bytes, *Then* "4.0 KB"; *Given* 117 398 bytes, *Then* "114.6 KB"; *Given* 158 548 694 bytes, *Then* "151.2 MB".
- **Dependencies**: RQ-APP-024, RQ-APP-026, RQ-APP-042

### RQ-APP-082: Messages in the application's words
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF the sampler refuses a command because nothing of the kind is selected (no disk, no sample, no program, no multi), THEN the application SHALL say so with the action to take in the application ("Select a disk first."), never with the name of an MCP server's tool ("Use select_disk first.").
- **Rationale**: the owner saw "Use select_disk first." in the editor's status bar on 2026-10-15: a tool name the editor has no use for.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a session with no disk selected, *When* the root of the current disk is opened, *Then* the problem says "Select a disk first." and holds no "select_"; *Given* the MCP server in the same case, *Then* its answer still says "Use select_disk first.".
- **Dependencies**: RQ-APP-010, RQ-MCP-044

### RQ-APP-085: A log file of the errors
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the application reports a problem (RQ-APP-010) or the connection changes (RQ-APP-019), it SHALL append a timestamped line with the level, the action and the reason to `log/XS56K.log` in the folder of the executable; IF that folder cannot be written, THEN it SHALL write to `XS56K/log` in the person's application-data folder. The start of each run SHALL be written with the version.
- **Rationale**: the owner's request of 2026-10-15: errors kept in a text file, as XplorerEditor does.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a log sink, *When* a problem "Creating the folder X" / "disk error" is set, *Then* one error line holds both; *When* the session connects, *Then* an info line says so.
- **Dependencies**: RQ-APP-010, RQ-APP-019, ADR-APP-001 (DEC-APP-023)
