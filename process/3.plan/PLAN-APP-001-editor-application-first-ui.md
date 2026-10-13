# PLAN-APP-001: Editor Application — Requirements, Architecture and Design System of the First UI

## Overview

Session APP (2026-10-13). The owner starts the user interface of the editor application (`juce/app`): a file browser of the sampler's disks
at the top left, a memory view by type under it, an edit panel on the right with tabs at its bottom (the owner's sketch and description of
2026-10-13). This plan covers the work up to the first implementation task: the requirements (FTR-APP-001 to FTR-APP-009, done by
TASK-APP-001), the open decisions the owner has to take, the architecture of the application and its design system. The implementation tasks
are planned once the two ADRs are accepted and, as the owner asked on 2026-10-13, once the owner's branch that fixes the code's
quality issues is merged, so that the tasks are based on the code as it will then be, each feature of priority 1 in its turn, under the limit of 10 tasks per session.

**Priorities** (owner, 2026-10-13): priority 1, mandatory, written `Must`: FTR-APP-001 to FTR-APP-008 (and the `Must` requirements in them);
priority 2, nice to have, written `Could`: FTR-APP-009 and the `Could` requirements of the other features. Recording a sample is out of scope.

| Step | Task | What | Why here |
|---|---|---|---|
| 1 | TASK-APP-001 | the requirements FTR-APP-001 to FTR-APP-009 and this plan | the owner asked for a plan of functional requirements |
| 2 | TASK-APP-005 | remove the assumption that a tab is a function of the sampler (RQ-APP-049, RQ-APP-056, RQ-APP-057) | the owner's remark of 2026-10-13 |
| 3 | TASK-APP-002 | the owner defines the groups (tabs) of the program editor | the owner was not sure on 2026-10-13 |
| 4 | TASK-APP-003 | ADR-APP-001: the application's architecture | before any code of `juce/app` |
| 5 | TASK-APP-004 | the design-system document and its ADR | the AGNOS process requires it before the first UI task is Ready |

## References
- **Requirements**: RQ-APP-001 to RQ-APP-072 (`FTR-APP-001` to `FTR-APP-009`); RQ-BLD-007 (the placeholder application)
- **ADRs**: ADR-APP-001 (Accepted, DEC-APP-001 to DEC-APP-010); the design-system ADR is written by TASK-APP-004

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
  FAT32 (RQ-APP-047), the owner's FAT32 disk having been saved to. (9) The program editor's tab list is provisional (RQ-APP-056); superseded by TASK-APP-005, which removed that list.

### TASK-APP-002: The owner decides the tabs of the program editor
- **Tier**: S
- **Status**: Not Started
- **Description**: The owner defines the groups (tabs) of the program editor and which parameters of the inventory of FTR-APP-008 each
  holds, with no one-to-one match assumed with the sampler's pages, the manual's functions or the SysEx sections; the decided groups are
  written into FTR-APP-008.
- **Requirement refs**: RQ-APP-049, RQ-APP-056, RQ-APP-057
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* the inventory of FTR-APP-008, *When* the owner has decided, *Then* FTR-APP-008 holds the groups
  and every parameter of the inventory is in at least one of them (RQ-APP-057).
- **Dependencies**: TASK-APP-005
- **Assignee**: Human
- **Verification**: N/A (Tier S)
- **Assumptions**: None

### TASK-APP-003: ADR-APP-001 — the application's architecture
- **Tier**: M
- **Status**: Done
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
- **Verification**: ADR-APP-001 re-read in session: the AGNOS sections (Status, Context, Decision, Consequences, Alternatives
  Considered, Diagram) are present, ten `### DEC-APP-` headings (DEC-APP-001 to DEC-APP-010, indexed by `agnos-index`), two Mermaid
  diagrams, the RQ-APP answered cited in each decision; accepted by the owner on 2026-10-13 (Status: Accepted).
- **Assumptions**: (1) The gateway and the catalogues are split from `xs56k_mcp` without moving their files or renaming their namespace
  (DEC-APP-002). (2) `xs56k_app_core` is built with the libraries whatever `BUILD_APP` says, so the headless workflows test it (DEC-APP-010).

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

### TASK-APP-005: Remove the assumption that a tab is a function of the sampler
- **Tier**: M
- **Status**: Done
- **Description**: After the owner's remark of 2026-10-13 ("il ne faut pas assumer que par exemple il y aura un onglet par section de
  sysex"; LFO is not known to be a big function), rewrite RQ-APP-049 so that the tabs are groups the owner decides, replace the provisional
  tab list of FTR-APP-008 by an inventory of the parameters to place, let a tab mix program, keygroup and zone parameters (RQ-APP-056), and
  require every parameter in at least one tab rather than exactly one (RQ-APP-057).
- **Requirement refs**: RQ-APP-049, RQ-APP-056, RQ-APP-057
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* FTR-APP-007 and FTR-APP-008, *When* they are searched for a tab defined by a function, a page or
  a section of the sampler, *Then* none is found; *given* RQ-APP-049, RQ-APP-056 and RQ-APP-057, *then* each carries an amendment note of
  2026-10-13; *given* the index regenerated, *then* it reports no duplicate ID.
- **Dependencies**: TASK-APP-001
- **Assignee**: AI
- **Verification**: Re-run in session: a search of FTR-APP-007 and FTR-APP-008 for "big function", "provisional tab", "one tab per big",
  "function's parameters", "LFO tab" and "exactly one tab" finds only the amendment note of RQ-APP-057, which quotes the old wording;
  the note "Amended 2026-10-13 (owner, TASK-APP-005)" is found once in FTR-APP-007 (RQ-APP-049) and twice in FTR-APP-008 (RQ-APP-056,
  RQ-APP-057); `validate-ids.sh -Type TASK -Value TASK-APP-005` exit 0; `build-index.sh` exit 0 (no duplicate ID), 584 entries, 89 documents.
- **Assumptions**: (1) "Exactly one tab" became "at least one tab" (RQ-APP-057), so as not to decide whether a parameter may appear in two
  groups. (2) The inventory of FTR-APP-008 is listed by scope (program, keygroup, zones) because that is where the parameters live on the
  sampler, not as a proposal of tabs.
