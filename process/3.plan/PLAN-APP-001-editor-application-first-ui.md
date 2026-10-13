# PLAN-APP-001: Editor Application — Requirements, Architecture and Design System of the First UI

## Overview

Session APP (2026-10-13). The owner starts the user interface of the editor application (`juce/app`): a file browser of the sampler's disks
at the top left, a memory view by type under it, an edit panel on the right with tabs at its bottom (the owner's sketch and description of
2026-10-13). This plan covers the work up to the first implementation task: the requirements (FTR-APP-001 to FTR-APP-009, done by
TASK-APP-001), the open decisions the owner has to take, the architecture of the application and its design system. The implementation tasks
are planned once the two ADRs are accepted, each feature of priority 1 in its turn, under the limit of 10 tasks per session.

**Priorities** (owner, 2026-10-13): priority 1, mandatory, written `Must`: FTR-APP-001 to FTR-APP-008 (and the `Must` requirements in them);
priority 2, nice to have, written `Could`: FTR-APP-009 and the `Could` requirements of the other features. Recording a sample is out of scope.

| Step | Task | What | Why here |
|---|---|---|---|
| 1 | TASK-APP-001 | the requirements FTR-APP-001 to FTR-APP-009 and this plan | the owner asked for a plan of functional requirements |
| 2 | TASK-APP-002 | the owner decides the open points: the tabs of the program editor | the owner was not sure on 2026-10-13 |
| 3 | TASK-APP-003 | ADR-APP-001: the application's architecture | before any code of `juce/app` |
| 4 | TASK-APP-004 | the design-system document and its ADR | the AGNOS process requires it before the first UI task is Ready |

## References
- **Requirements**: RQ-APP-001 to RQ-APP-072 (`FTR-APP-001` to `FTR-APP-009`); RQ-BLD-007 (the placeholder application)
- **ADRs**: None yet (ADR-APP-001 and the design-system ADR are written by TASK-APP-003 and TASK-APP-004)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-APP-001: Author FTR-APP-001 to FTR-APP-009 and PLAN-APP-001
- **Tier**: M
- **Status**: Done
- **Description**: Write the functional and non-functional requirements of the editor's first UI from the owner's sketch and description,
  the user manual, the SysEx specification and the AKM primitives already implemented, in EARS with Gherkin criteria, classified priority 1
  (`Must`) and priority 2 (`Could`); write this plan.
- **Requirement refs**: RQ-APP-001 to RQ-APP-072
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* the FTR-APP files written, *When* the `agnos-index` skill regenerates the index, *Then* it reports
  no duplicate ID; *given* any RQ-APP, *then* it has an EARS type, a priority `Must` or `Could`, a Gherkin criterion and its dependencies;
  *given* a requirement of a user interface, *then* its dependencies name the design system (RQ-APP-001).
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Structural check run in session (Python over `process/1.requirements/FTR-APP-*.md`): 72 requirements RQ-APP-001 to
  RQ-APP-072, no duplicate, no gap, no RQ-APP referenced that does not exist; each has its EARS type, statement, priority (`Must` or `Could`),
  Gherkin criterion and dependencies; every RQ of FTR-APP-002 to FTR-APP-009 names RQ-APP-001 in its dependencies except RQ-APP-014 (tests,
  not UI). The three lines the check printed (RQ-APP-003, RQ-APP-011, RQ-APP-055 "missing Metric") are its own false positive: the
  "Non-Functional" heading that follows each of them fell inside its block; each has its rationale. `agnos-index` (`build-index.sh`): exit 0
  (no duplicate ID), 583 entries, 89 documents, 72 `RQ-APP` rows, next free IDs `FTR-APP-010`, `RQ-APP-073`, `TASK-APP-005`.
- **Assumptions**: (1) The owner's answer "1)" of 2026-10-13 (categorise mandatory / nice to have) also classifies copy, paste and move in
  priority 2 (RQ-APP-064). (2) Priority 1 is `Must`, priority 2 is `Could`; `Should` is not used. (3) The requirements are written in English,
  like every FTR of the repository. (4) The priority-2 needs are one requirement each in one file (FTR-APP-009), detailed in a feature of their
  own when promoted. (5) Listing the six kinds in the memory view is priority 1, their editors and the changes to song files, set lists and
  scenelists priority 2. (6) A "pending" state, for a value sent and not yet read back, is added to the design system's states (RQ-APP-003).
  (7) The refresh of the list of disks stays priority 1, behind a confirmation (RQ-APP-034). (8) Saving is allowed to the formats MSDOS and
  FAT32 (RQ-APP-047), the owner's FAT32 disk having been saved to. (9) The program editor's tab list is provisional (RQ-APP-056).

### TASK-APP-002: The owner decides the tabs of the program editor
- **Tier**: S
- **Status**: Not Started
- **Description**: Review with the owner the provisional tab list of FTR-APP-008 against the pages of the user manual (EDIT PROGRAM,
  pp. 80-127), find the page that shows the Keygroup Modulation Sources, and write the decided list into FTR-APP-008.
- **Requirement refs**: RQ-APP-049, RQ-APP-056, RQ-APP-057
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* the provisional list, *When* the owner has decided, *Then* FTR-APP-008 holds the decided list and
  no longer says it is provisional.
- **Dependencies**: TASK-APP-001
- **Assignee**: Human
- **Verification**: N/A (Tier S)
- **Assumptions**: None

### TASK-APP-003: ADR-APP-001 — the application's architecture
- **Tier**: M
- **Status**: Not Started
- **Description**: Decide, in ADR-APP-001, how the application is built: its layers (the behaviour kept out of the JUCE components,
  RQ-APP-014), how a session's completions reach the UI thread without blocking it (RQ-APP-012), the model of the sampler's state the views
  share (memory lists, current items, current folder), the reading and writing of parameters (read-back, coalescing, RQ-APP-052 to
  RQ-APP-054), whether the MCP server's parameter catalogue is shared, where the settings are kept (RQ-APP-016), and how the application is
  tested against the simulated sampler.
- **Requirement refs**: RQ-APP-012, RQ-APP-013, RQ-APP-014, RQ-APP-016, RQ-APP-051 to RQ-APP-054
- **ADR refs**: ADR-APP-001 (to write); ADR-AKM-001; ADR-MCP-001 (DEC-MCP-005, DEC-MCP-006)
- **Acceptance Criteria** (Gherkin): *Given* ADR-APP-001, *When* it is read, *Then* it has the AGNOS structure with one DEC-APP per decision
  and a Mermaid diagram, references the RQ-APP it answers, and the owner has accepted it.
- **Dependencies**: TASK-APP-001
- **Assignee**: AI
- **Verification**: Not done yet.
- **Assumptions**: None

### TASK-APP-004: The design system and its ADR
- **Tier**: M
- **Status**: Not Started
- **Description**: Instantiate the design-system document from `process/1.requirements/design-system-template.md` with the values the
  owner chooses (palette, theme, fonts, spacing unit), and write the ADR that records the token structure, the token module and the check of
  RQ-APP-004.
- **Requirement refs**: RQ-APP-001 to RQ-APP-005
- **ADR refs**: the design-system ADR (to write); ADR-APP-001
- **Acceptance Criteria** (Gherkin): *Given* the design-system document, *When* it is read, *Then* no placeholder of the template is left and
  every state and status names its second cue; *given* the ADR, *then* the owner has accepted it.
- **Dependencies**: TASK-APP-003
- **Assignee**: AI
- **Verification**: Not done yet.
- **Assumptions**: None
