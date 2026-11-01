# PLAN-APP-008: Editor Application — the Multi Editor

## Overview

Session APP (2026-11-01), after PLAN-APP-007. The owner asked for the multi editor (FTR-APP-011) and to go on in this session past the save's 7
tasks ("Continue sur multi", in answer to the 10-task question). Core first with tests on the simulated sampler, then the views, checked by the
owner on the real S5000. 6 tasks.

| Step | Task | What |
|---|---|---|
| 1 | TASK-APP-062 | the layout `multi.json`, scopes multi and part, the control program; the gateway reads the program number |
| 2 | TASK-APP-063 | the multi binding: the part chosen or all parts, the part parameters |
| 3 | TASK-APP-064 | the program of a part and the multi's program number |
| 4 | TASK-APP-065 | a multi created from the memory view |
| 5 | TASK-APP-066 | the views: the part selector, the program choice, "New multi…", the mockup |
| 6 | TASK-APP-067 | run on the real S5000 (owner) |

## References
- **Requirements**: RQ-APP-100 to RQ-APP-104; RQ-APP-048 to RQ-APP-054, RQ-APP-058, RQ-APP-059, RQ-APP-092; RQ-AKM-087 to RQ-AKM-092;
  RQ-MCP-021, RQ-MCP-037, RQ-MCP-038
- **ADRs**: ADR-APP-006 (DEC-APP-041 to DEC-APP-043); ADR-APP-004 (DEC-APP-024, DEC-APP-025, DEC-APP-030, DEC-APP-033); ADR-APP-002; ADR-APP-003

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-APP-062: The multi layout and the program number read alone
- **Tier**: M
- **Status**: Not Started
- **Description**: `juce/app/design/layouts/multi.json` (tab "Parts": Multi, Program, Mix, Setup); the generator knows the scopes `multi`,
  `part` and the control `program`; `SamplerGateway::readMultiProgramNumber` (`&41`).
- **Requirement refs**: RQ-APP-100, RQ-APP-103
- **ADR refs**: ADR-APP-006 (DEC-APP-041, DEC-APP-043), ADR-APP-004 (DEC-APP-024)
- **Acceptance Criteria**: *Given* the layouts, *When* the generator runs, *Then* the multi layout is generated and `--check` passes; *Given* a
  multi with program number 5, *When* it is read alone, *Then* 5 comes back, and empty when it is off; the SysEx coverage table stays up to date.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**:
- **Assumptions**:

### TASK-APP-063: The multi binding
- **Tier**: M
- **Status**: Not Started
- **Description**: The `EditController` edits a multi: made current by name with its part count, its part parameters read and written for the
  part chosen or all parts, "VARIES" when the parts differ, the low note bounded by the high note.
- **Requirement refs**: RQ-APP-100, RQ-APP-101
- **ADR refs**: ADR-APP-006 (DEC-APP-041)
- **Acceptance Criteria**: *Given* LIVE of 32 parts selected, *Then* the editor shows part 1's twelve parameters read from the sampler and 32
  parts; *When* part 2 is chosen, *Then* its values are read; *When* the person sets the level to 70, *Then* it is sent, read back and shown;
  *When* all parts are chosen and the person mutes, *Then* every part is muted; *Given* a high note of 60, *When* the low note is set to 72,
  *Then* nothing is sent.
- **Dependencies**: TASK-APP-062
- **Assignee**: AI
- **Verification**:
- **Assumptions**:

### TASK-APP-064: The program of a part and the program number
- **Tier**: M
- **Status**: Not Started
- **Description**: The values "part program" (a choice among the programs in memory, 0 none) and "multi program number" (OFF, 1 to 128); the
  programs of the parts in the edit state for the selector.
- **Requirement refs**: RQ-APP-101, RQ-APP-102, RQ-APP-103
- **ADR refs**: ADR-APP-006 (DEC-APP-042, DEC-APP-043)
- **Acceptance Criteria**: *Given* part 2 playing nothing, *When* PAD is chosen, *Then* it is assigned by name and part 2 reads PAD; *When* none
  is chosen, *Then* the part is cleared and PAD stays in memory; *Given* all parts, *Then* a program change is refused with nothing sent;
  *Given* no program number, *Then* it reads OFF; *When* 5 is set, *Then* it is sent and reads 5.
- **Dependencies**: TASK-APP-063
- **Assignee**: AI
- **Verification**:
- **Assumptions**:

### TASK-APP-065: A multi created from the memory view
- **Tier**: S
- **Status**: Not Started
- **Description**: `MemoryController::createMulti`: a name taken refused with nothing sent; the multi created, listed and selected.
- **Requirement refs**: RQ-APP-104
- **ADR refs**: ADR-APP-006 (DEC-APP-043)
- **Acceptance Criteria**: *Given* M1, *When* LIVE is created, *Then* it is listed and selected; *When* m-1 is created, *Then* nothing is sent.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**:

### TASK-APP-066: The views of the multi editor
- **Tier**: M
- **Status**: Not Started
- **Description**: The edit panel's part selector ("Part n of N", the program, "All parts"), no add or delete button for a multi, the program
  choice of a part, the scopes' names; "New multi…" in the memory view's context menu; the mockup `multi-editor` generated first.
- **Requirement refs**: RQ-APP-100 to RQ-APP-102, RQ-APP-104, RQ-APP-002
- **ADR refs**: ADR-APP-006, ADR-APP-002, ADR-APP-003
- **Acceptance Criteria**: *Given* the mockup generator, *Then* `--check` passes and the multi editor's mockup exists; the editor builds with
  `-Werror`, the tests and tidy pass.
- **Dependencies**: TASK-APP-063 to TASK-APP-065
- **Assignee**: AI
- **Verification**:
- **Assumptions**:

### TASK-APP-067: Run on the real S5000
- **Tier**: S
- **Status**: Not Started
- **Description**: The owner edits a multi on the S5000 following `process/3.plan/checklists/real-sampler-run-APP-multi.md`.
- **Requirement refs**: RQ-APP-100 to RQ-APP-104
- **ADR refs**: ADR-APP-006
- **Acceptance Criteria**: *Given* the checklist, *When* the owner runs it, *Then* each line is OK or its finding is planned.
- **Dependencies**: TASK-APP-066
- **Assignee**: Human
- **Verification**: N/A (Tier S)
- **Assumptions**:
