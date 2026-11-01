# PLAN-APP-007: Editor Application — Saving Memory Items to a Disk

## Overview

Session APP (2026-11-01). The owner asked for the save (FTR-APP-006), the last Must of the editor: without it, what is edited is lost when the
sampler is switched off. Gateway first, then the core with tests on the simulated sampler, then the views, checked by the owner on the real S5000
(this environment has no access to it). 7 tasks. The other editors (multi RQ-APP-063, settings and MIDI RQ-APP-070, songs, set lists and
scenelists RQ-APP-065) come after, in their own plan; the effects board (RQ-APP-069) is left out: the owner has no effects board to test it with.

| Step | Task | What |
|---|---|---|
| 1 | TASK-APP-055 | the gateway's `ExistingFile`: refuse, skip, overwrite (DEC-APP-038) |
| 2 | TASK-APP-056 | `SaveController`: the item proposed, alone or with children, the format refused |
| 3 | TASK-APP-057 | all of a kind and the whole memory |
| 4 | TASK-APP-058 | check names: the questions and the flags |
| 5 | TASK-APP-059 | the save checked: the folder shown again, the report |
| 6 | TASK-APP-060 | the save dialog, its mockup, the two menu entries |
| 7 | TASK-APP-061 | run on the real S5000 (owner) |

## References
- **Requirements**: RQ-APP-043 to RQ-APP-047; RQ-APP-001, RQ-APP-002, RQ-APP-011, RQ-APP-013, RQ-APP-024, RQ-APP-025, RQ-APP-037; RQ-AKM-062,
  RQ-AKM-065, RQ-AKM-067; RQ-MCP-026, RQ-MCP-027
- **ADRs**: ADR-APP-005 (DEC-APP-038 to DEC-APP-040); ADR-APP-001 (DEC-APP-001 to DEC-APP-003, DEC-APP-007); ADR-APP-002; ADR-APP-003;
  ADR-MCP-003 (DEC-MCP-018, DEC-MCP-022)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-APP-055: The gateway saves with refuse, skip or overwrite
- **Tier**: L
- **Status**: Done
- **Description**: `saveMemoryItem` and `saveAllMemoryItems` take an `ExistingFile` (`Refuse`, `Skip`, `Overwrite`) instead of `overwrite`;
  `Skip` sends the "skip if the file exists" flag; the MCP tools pass `Refuse` or `Overwrite`, unchanged for the client.
- **Requirement refs**: RQ-APP-045, RQ-MCP-026, RQ-MCP-027, RQ-AKM-067
- **ADR refs**: ADR-APP-005 (DEC-APP-038), ADR-MCP-003
- **Acceptance Criteria**: *Given* PAD.AKP in the current folder, *When* PAD is saved with `Skip`, *Then* the save is sent with the skip flag and
  the outcome lists PAD.AKP as already there; *When* with `Refuse`, *Then* nothing is sent; *When* with `Overwrite`, *Then* the save is sent with
  the overwrite flag; *Given* the MCP tests, *Then* they pass unchanged; the SysEx coverage table stays up to date.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: `juce/tests/mcp/GatewaySaveTests.cpp` (6 cases, written before the code; not run red, the build had both): `xs56k_mcp_tests
  "[save]"` 25 cases / 540 assertions pass; whole `xs56k_mcp_tests` 378 cases / 11191 assertions pass, MCP tests unchanged; `ctest -R
  "coverage|destructive"` 20/20 (SysEx coverage table up to date); `tidy.py --base HEAD` 0 findings (Clang, Linux).
- **Assumptions**: one item with `Skip` and its file there: nothing is sent (same result as the flag, no unknown answer of the sampler);
  the simulated sampler's error on the skip flag with a file there is kept (an AKM test relies on it), the real answer is left to TASK-APP-061.

### TASK-APP-056: The save controller proposes the item and saves it alone or with its children
- **Tier**: M
- **Status**: Done
- **Description**: `SaveController` in `xs56k_app_core`: the item proposed (selection, else the current item of its kind), the current disk and
  folder as destination, alone or with children, one job with the slow-operation timeout; a disk not MSDOS or FAT32 refused with nothing sent.
- **Requirement refs**: RQ-APP-043, RQ-APP-047, RQ-APP-013
- **ADR refs**: ADR-APP-005 (DEC-APP-039), ADR-APP-001 (DEC-APP-003)
- **Acceptance Criteria**: *Given* the program PAD selected and the folder KIT current, *When* it is saved with its children, *Then* PAD is saved
  into KIT with the "save children" flag; *Given* nothing selected and BASS the current program, *Then* BASS is proposed; *Given* the current
  disk formatted S3000, *Then* nothing is sent and the reason says the sampler saves only to an MS-DOS formatted disk; *Given* FAT32, *Then* the
  save proceeds.
- **Dependencies**: TASK-APP-055
- **Assignee**: AI
- **Verification**: `juce/tests/app/SaveTests.cpp` [save] 11 cases / 105 assertions pass (Clang, Linux): PAD saved into KIT with the children
  flag and listed in KIT; BASS proposed with nothing selected, children flag off; S3000 refused (`refusal()` and problem name MS-DOS, no
  &2C sent), FAT32 sends one &2C. Full `ctest` 1261/1261; `tidy.py --base HEAD` 0 findings; `XS56K` builds.
- **Assumptions**: the save first puts the sampler in the browser's folder (`moveToFolder`, the browser's own `moveTo` now shared); the
  proposed item without a selection is the current program (the kind of RQ-APP-043's example).

### TASK-APP-057: All the items of a kind, and the whole memory
- **Tier**: M
- **Status**: Done
- **Description**: The controller saves all the items of the proposed item's kind, with or without children (`&2D`), and the whole memory as one
  `&2D` per kind holding items, in one job, stopping at the first failure.
- **Requirement refs**: RQ-APP-044
- **ADR refs**: ADR-APP-005 (DEC-APP-040)
- **Acceptance Criteria**: *Given* three samples in memory, *When* all samples are saved, *Then* one "save all" of kind sample is sent; *Given*
  programs and samples only, *When* the whole memory is saved, *Then* one "save all" is sent for each kind holding items, in order; *Given* the
  second one fails, *Then* nothing more is sent and the report names the kinds saved.
- **Dependencies**: TASK-APP-056
- **Assignee**: AI
- **Verification**: same run: all samples → one &2D of type sample, no &2C; whole memory with programs and samples → two &2D, programs
  then samples, `kindsSaved` = program, sample; disk taken away → no &2D sent, problem shown, report with no kind saved.
- **Assumptions**: the whole memory is saved without the children flag (every kind is saved anyway); the failure test fails the first kind
  (the simulated sampler cannot fail the second one alone).

### TASK-APP-058: Check names
- **Tier**: M
- **Status**: Done
- **Description**: A `UserPrompts` question for an existing file (replace, skip, replace all, cancel; replace, skip, cancel when asked once for a
  whole save); "check names" on by default, remembered in `AppSettings`; off: overwrite with no question.
- **Requirement refs**: RQ-APP-045, RQ-APP-011
- **ADR refs**: ADR-APP-005 (DEC-APP-040), ADR-APP-001 (DEC-APP-007)
- **Acceptance Criteria**: *Given* PAD.AKP in the folder and "check names" on, *When* PAD is saved alone and the answer is "skip", *Then* the
  save is sent with `Skip`; *When* the answer is cancel, *Then* nothing is sent; *Given* all programs saved and one file existing, *Then* one
  question is asked for the whole save; *Given* "check names" off, *Then* the save is sent with `Overwrite` and no question is asked.
- **Dependencies**: TASK-APP-056, TASK-APP-057
- **Assignee**: AI
- **Verification**: same run: PAD.AKP there, "skip" → question names PAD.AKP, nothing sent, report skipped PAD.AKP; cancel → nothing sent;
  replace → &2C with overwrite 1; all programs with PAD.AKP there → one question, &2D with flag 0; "check names" off → no question,
  overwrite 1, and a new controller reads it off from the settings.
- **Assumptions**: "replace all" is not offered for one file (it is "replace"); with no file known to be there, `Skip` is sent.

### TASK-APP-059: The save is checked on the disk
- **Tier**: M
- **Status**: Done
- **Description**: After the save, the browser shows the current folder from the outcome's listing, and the controller reports the files saved,
  skipped (there before, `Skip`) and missing.
- **Requirement refs**: RQ-APP-046, RQ-APP-025
- **ADR refs**: ADR-APP-005 (DEC-APP-038, DEC-APP-039), ADR-MCP-003 (DEC-MCP-022)
- **Acceptance Criteria**: *Given* PAD saved alone into KIT, *When* the save is done, *Then* the browser lists PAD.AKP in KIT and the report says
  PAD.AKP was saved; *Given* PAD.AKP there before and `Skip`, *Then* the report says it was skipped; *Given* the file absent after the save,
  *Then* the report says it is missing.
- **Dependencies**: TASK-APP-056
- **Assignee**: AI
- **Verification**: same run: PAD saved into KIT → browser lists PAD.AKP in KIT, report saved = PAD.AKP, nothing skipped or missing, text
  names it; `reportOf` with an empty listing → PAD.AKP missing, text says "missing".
- **Assumptions**: the browser shows the folder from the save's own listing (no new listing command); children are the files added that
  bear no item's name.

### TASK-APP-060: The save dialog and its two entries
- **Tier**: M
- **Status**: Done
- **Description**: A JUCE save dialog (item, scope, destination shown, "check names") opened by "File > Save…" (Ctrl+S) and by "Save…" in the
  memory view's context menu; its mockup generated first; every visual value from the design tokens.
- **Requirement refs**: RQ-APP-043 to RQ-APP-045, RQ-APP-002
- **ADR refs**: ADR-APP-005 (DEC-APP-039), ADR-APP-002, ADR-APP-003
- **Acceptance Criteria**: *Given* a program selected, *When* "File > Save…" or the context menu's "Save…" is chosen, *Then* the dialog proposes
  it with the current folder; *Given* no disk selected or disconnected, *Then* "File > Save…" is disabled and Ctrl+S shows why; *Given* the mockup generator, *Then*
  `--check` passes; the build, tests and tidy pass.
- **Dependencies**: TASK-APP-056 to TASK-APP-059
- **Assignee**: AI
- **Verification**: `XS56K` builds with Clang (`-Werror`); full `ctest` 1261/1261; `tidy.py --base HEAD~2` 25 sources, 0 findings;
  `generate_mockups.py --check`: 22 mockups up to date, `main-window-save.svg` and `main-window-save-existing.svg` new, rendered to PNG with
  Chromium and looked at (three buttons fit). The entries' enabled state follows `SaveController::refusal()` (tested in [save]); the
  dialog itself is not driven by a test (no UI automation on Linux): to check on the owner's run (TASK-APP-061).
- **Assumptions**: a "File" menu is added (there was none) holding "Save…" with Ctrl+S (Cmd+S on macOS); the report is shown in an
  information box; the dialog also lets the person choose another item of memory (the SAVE page does, FTR-APP-006); "check names" is a
  check box of the dialog, remembered.

### TASK-APP-061: Run on the real S5000
- **Tier**: S
- **Status**: Not Started
- **Description**: The owner saves on the S5000 a program alone, with children, all samples, the whole memory, and answers each check-names
  question, following a checklist in `process/3.plan/checklists/`.
- **Requirement refs**: RQ-APP-043 to RQ-APP-047
- **ADR refs**: ADR-APP-005
- **Acceptance Criteria**: *Given* the checklist, *When* the owner runs it, *Then* each line is OK or its finding is planned.
- **Dependencies**: TASK-APP-060
- **Assignee**: Human
- **Verification**: N/A (Tier S)
- **Assumptions**:
