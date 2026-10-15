# PLAN-APP-005: Editor Application — the Sample Editor, First Editor of the Edit Panel

## Overview

Session APP (2026-10-15). The owner asked for the first tab of the edit panel: "Sample", editing the settable parameters of the sample selected in
the memory view (FTR-APP-010). The edit panel's shared parts are built with it (ADR-APP-004): the generated layouts, the edit controller, the
controls. Core first with tests on the simulated sampler, then the views, checked on the real S5000 through `uia.ps1`. 7 tasks.

| Step | Task | What |
|---|---|---|
| 1 | TASK-APP-034 | requirements, ADR-APP-004, this plan |
| 2 | TASK-APP-035 | `layouts/sample.json` and its generation into `EditorLayouts.generated.hpp` |
| 3 | TASK-APP-036 | `EditController`, read side: the tab and the information read on selection, and on request |
| 4 | TASK-APP-037 | `EditController`, write side: range, set and read back, one pending per parameter, the tab read again |
| 5 | TASK-APP-038 | the three controls (value box, choice, knob) drawn from the tokens |
| 6 | TASK-APP-039 | the edit panel view: header, tab row, groups, "Read again" |
| 7 | TASK-APP-040 | mockup of the sample editor, READMEs, run on the real S5000 |

## References
- **Requirements**: RQ-APP-086 to RQ-APP-089 (new); RQ-APP-048 (amended); RQ-APP-062 (superseded); RQ-APP-049 to RQ-APP-055; RQ-APP-001 to
  RQ-APP-005; RQ-APP-014; RQ-AKM-048, RQ-AKM-049; RQ-MCP-020
- **ADRs**: ADR-APP-004 (DEC-APP-024 to DEC-APP-027); ADR-APP-001 (DEC-APP-004 to DEC-APP-006, DEC-APP-010); ADR-APP-002; ADR-APP-003 (DEC-APP-018)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-APP-034: Requirements, ADR and plan of the sample editor
- **Tier**: M
- **Status**: Done
- **Description**: FTR-APP-010 (RQ-APP-086 to RQ-APP-089), ADR-APP-004, this plan; RQ-APP-062 superseded, RQ-APP-048 amended.
- **Requirement refs**: RQ-APP-086 to RQ-APP-089, RQ-APP-048, RQ-APP-062
- **ADR refs**: ADR-APP-004
- **Acceptance Criteria** (Gherkin): *Given* the index rebuilt, *Then* the new IDs are listed with no duplicate.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: the `agnos-index` skill, 2026-10-15: exit 0, no duplicate.
- **Assumptions**: (1) The three groups of the tab are the assistant's proposal. (2) The read-only information is in the header, not in the tab. (3) The audition
  in the editor is the space bar (RQ-APP-083, amended at the owner's request), no button.

### TASK-APP-035: The sample layout and its generation
- **Tier**: M
- **Status**: Not Started
- **Description**: `juce/app/design/layouts/sample.json`; `generate_design.py` writes `EditorLayouts.generated.hpp` from `layouts/` with `--check`;
  Python tests first; a C++ test checks the names against `ParameterCatalogue::samples()`.
- **Requirement refs**: RQ-APP-086, RQ-APP-049
- **ADR refs**: ADR-APP-004 (DEC-APP-024); ADR-APP-003 (DEC-APP-018)
- **Acceptance Criteria** (Gherkin): *Given* `sample.json`, *When* the generator runs, *Then* the header lists the tab, its three groups and eight
  parameters; *Given* an unknown catalogue name, *Then* the C++ test fails; `--check` fails on a stale header.
- **Dependencies**: TASK-APP-034
- **Assignee**: AI
- **Verification**: —
- **Assumptions**: —

### TASK-APP-036: The edit controller, read side
- **Tier**: L
- **Status**: Not Started
- **Description**: `EditController` and `EditState` (`ModelPart::Edit`): on a sample selected, read the tab's parameters and the four read-only rows;
  a kind with no layout shows "no editor yet"; read again on request.
- **Requirement refs**: RQ-APP-048, RQ-APP-050, RQ-APP-053, RQ-APP-086, RQ-APP-087
- **ADR refs**: ADR-APP-004 (DEC-APP-025); ADR-APP-001 (DEC-APP-004, DEC-APP-005)
- **Acceptance Criteria** (Gherkin): see RQ-APP-086 (read part), RQ-APP-087, RQ-APP-048 (a program: no editor yet), on the simulated sampler.
- **Dependencies**: TASK-APP-035
- **Assignee**: AI
- **Verification**: —
- **Assumptions**: —

### TASK-APP-037: The edit controller, write side
- **Tier**: L
- **Status**: Not Started
- **Description**: A value given: refused out of range (positions bounded by the length), else set and read back, pending meanwhile, one write per
  parameter in flight with the latest waiting, then the whole tab read again; the sample made current first if it is not.
- **Requirement refs**: RQ-APP-051, RQ-APP-052, RQ-APP-054, RQ-APP-088, RQ-APP-089
- **ADR refs**: ADR-APP-004 (DEC-APP-025, DEC-APP-026); ADR-APP-001 (DEC-APP-005)
- **Acceptance Criteria** (Gherkin): see RQ-APP-052, RQ-APP-054, RQ-APP-088, RQ-APP-089 on the simulated sampler.
- **Dependencies**: TASK-APP-036
- **Assignee**: AI
- **Verification**: —
- **Assumptions**: —

### TASK-APP-038: The three controls
- **Tier**: M
- **Status**: Not Started
- **Description**: Value box, choice and knob in `XS56KApp`, drawn by the look-and-feel from the component tokens, with the pending state; the
  note-name text of the original pitch in the core (tested).
- **Requirement refs**: RQ-APP-051, RQ-APP-086, RQ-APP-003, RQ-APP-005, RQ-APP-001, RQ-APP-004
- **ADR refs**: ADR-APP-004 (DEC-APP-027); ADR-APP-002 (DEC-APP-013, DEC-APP-014)
- **Acceptance Criteria** (Gherkin): *Given* 60, *Then* the note text is "C3" (core test); the literal and token checks pass.
- **Dependencies**: TASK-APP-034
- **Assignee**: AI
- **Verification**: —
- **Assumptions**: —

### TASK-APP-039: The edit panel view
- **Tier**: M
- **Status**: Not Started
- **Description**: The edit panel draws the `EditState`: header (name and information), the tab row at the bottom, the groups with their controls, a
  "Read again" button, the message of a kind without editor; the space bar starts and stops the audition of the edited sample, except while a value
  is being typed.
- **Requirement refs**: RQ-APP-048 to RQ-APP-053, RQ-APP-083, RQ-APP-086, RQ-APP-087
- **ADR refs**: ADR-APP-004 (DEC-APP-025, DEC-APP-027); ADR-APP-001 (DEC-APP-001)
- **Acceptance Criteria** (Gherkin): *Given* the application on the real S5000 and a sample selected, *Then* the tab shows the eight values read and the
  header the information (through `uia.ps1` and a capture).
- **Dependencies**: TASK-APP-036, TASK-APP-037, TASK-APP-038
- **Assignee**: AI
- **Verification**: —
- **Assumptions**: —

### TASK-APP-040: Mockup, documentation and the real run
- **Tier**: M
- **Status**: In Progress
- **Description**: A screen of the sample editor and its mockup; the READMEs updated; on the real S5000, a test sample's fine tune and loop end changed
  and put back.
- **Requirement refs**: RQ-APP-074, RQ-APP-086, RQ-APP-089
- **ADR refs**: ADR-APP-003 (DEC-APP-019); ADR-APP-004
- **Acceptance Criteria** (Gherkin): `generate_mockups.py --check` passes; on the real S5000 the values read back are those set, and are put back.
- **Dependencies**: TASK-APP-039
- **Assignee**: AI
- **Verification**: (mockup part, done first at the owner's request, 2026-10-15) two new tests of `test_generate_mockups.py` (a value box alone, no knob;
  an information line instead of the keygroup) failed first, then passed (15 tests OK); `layouts/sample.json` and `screens/sample-editor.json` written;
  `sample-editor-connected.svg` and `sample-editor-writing.svg` generated and looked at as images.
- **Assumptions**: —
