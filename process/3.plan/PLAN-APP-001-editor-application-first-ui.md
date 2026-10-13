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
| 6 | TASK-APP-006 | requirements and ADR-APP-003 of the SVG mockup generator | the owner's proposal of 2026-10-13 |
| 7 | TASK-APP-007 | the token file and `generate_design.py` (tokens, editor layouts, the document's tables) | the mockups read them |
| 8 | TASK-APP-008 | `generate_mockups.py` and its `--check` | prototype before code |
| 9 | TASK-APP-009 | first mockups: main window connected and disconnected, a confirmation dialog | first exchange with the owner |

## References
- **Requirements**: RQ-APP-001 to RQ-APP-078 (`FTR-APP-001` to `FTR-APP-009`); RQ-BLD-007 (the placeholder application)
- **ADRs**: ADR-APP-001 (Accepted, DEC-APP-001 to DEC-APP-010); ADR-APP-002 (design system, DEC-APP-011 to DEC-APP-016); ADR-APP-003 (SVG mockups, DEC-APP-017 to DEC-APP-021)

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
- **Status**: In Progress
- **Description**: Instantiate the design-system document from `process/1.requirements/design-system-template.md` with the values the
  owner chooses (palette, theme, fonts, spacing unit), and write the ADR that records the token structure, the token module and the check of
  RQ-APP-004.
- **Requirement refs**: RQ-APP-001 to RQ-APP-005
- **ADR refs**: the design-system ADR (to write); ADR-APP-001
- **Acceptance Criteria** (Gherkin): *Given* the design-system document, *When* it is read, *Then* no placeholder of the template is left and
  every state and status names its second cue; *given* the ADR, *then* the owner has accepted it.
- **Dependencies**: TASK-APP-003
- **Assignee**: AI
- **Verification**: `process/1.requirements/DESIGN-SYSTEM-APP.md` written from the template with every section filled (a search for
  the template's `[...]`, `[Principle` and `[rule]` placeholders finds none); contrast of every text pair computed in session with the WCAG
  formula (all text pairs >= 4.5 : 1, `status.error` on `surface.raised` 4.52 after `red.300` was lightened to `#F47A74`); Inter's licence read
  in `LICENSE.txt` of github.com/rsms/inter (SIL OFL 1.1); ADR-APP-002 drafted (DEC-APP-011 to DEC-APP-016), awaiting the owner's acceptance.
- **Assumptions**: (1) The theme is dark, from the reference image (the owner answered the theme question by giving the image).
  (2) The colours of §2.1 are approximations of the reference's style, not measured from it, and are this project's own. (3) The roles are
  sample (violet), modulation (teal), filter (amber), general (grey); output, MIDI/tune and pitch bend are "general". (4) Inter is the free font
  chosen (the reference's own font is not known with certainty). (5) The curves editable with the mouse are a new priority-2 requirement,
  RQ-APP-073. (6) The minimum window and area sizes of §2.3 are proposals.

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

### TASK-APP-006: Requirements and ADR of the SVG mockup generator
- **Tier**: M
- **Status**: In Progress
- **Description**: After the owner's proposal of 2026-10-13, add the requirements of a generator of static SVG mockups to FTR-APP-001
  (RQ-APP-074 to RQ-APP-078), write ADR-APP-003, and amend ADR-APP-002 and the design-system document (§7.1, §7.4, §8) to name it.
- **Requirement refs**: RQ-APP-074, RQ-APP-075, RQ-APP-076, RQ-APP-077, RQ-APP-078
- **ADR refs**: ADR-APP-003 (to accept), ADR-APP-002
- **Acceptance Criteria** (Gherkin): *Given* FTR-APP-001, *When* the index is regenerated, *Then* RQ-APP-074 to RQ-APP-078 are indexed with no
  duplicate; *given* ADR-APP-003, *then* it has the AGNOS sections and a Mermaid diagram and the owner has accepted it; *given* the design-system
  document, *then* its §7.4 names the generator.
- **Dependencies**: TASK-APP-004
- **Assignee**: AI
- **Verification**: Drafted; awaiting the owner's acceptance of ADR-APP-002 and ADR-APP-003.
- **Assumptions**: (1) The editor layouts (shared with the application) and the screens (mockups only) are separate files (DEC-APP-018).
  (2) The design generator of ADR-APP-002 is renamed `generate_design.py` because it also writes the layouts. (3) The mockups are committed
  under `documents/mockups/`.

### TASK-APP-007: The token file and `generate_design.py`
- **Tier**: M
- **Status**: Not Started
- **Description**: Write `juce/app/design/tokens.json` with the values of the design-system document and `juce/tools/generate_design.py`
  (checks of the aliases and the contrasts, `DesignTokens.generated.hpp`, the document's §2 tables between markers, `--check` as a `ctest`
  entry), tests first; the editor layouts part is added when the first layout exists (TASK-APP-002).
- **Requirement refs**: RQ-APP-002, RQ-APP-005, RQ-APP-075
- **ADR refs**: ADR-APP-002 (DEC-APP-011, DEC-APP-012), ADR-APP-003 (DEC-APP-018)
- **Acceptance Criteria** (Gherkin): *Given* the token file, *When* the generator runs, *Then* the header and the tables are written and every
  component token resolves to a global value; *given* a cycle or a missing alias, *then* it fails and names it; *given* the header edited,
  *then* `--check` fails.
- **Dependencies**: TASK-APP-004
- **Assignee**: AI
- **Verification**: Not done yet.
- **Assumptions**: None

### TASK-APP-008: `generate_mockups.py` and its `--check`
- **Tier**: M
- **Status**: Not Started
- **Description**: Write the mockup generator of ADR-APP-003 (screens, layouts, tokens to static SVG with Inter embedded, deterministic output,
  `--check` as a `ctest` entry), tests first.
- **Requirement refs**: RQ-APP-074, RQ-APP-076, RQ-APP-077, RQ-APP-078
- **ADR refs**: ADR-APP-003 (DEC-APP-017 to DEC-APP-021)
- **Acceptance Criteria** (Gherkin): the criteria of RQ-APP-074, RQ-APP-076, RQ-APP-077 and RQ-APP-078.
- **Dependencies**: TASK-APP-007
- **Assignee**: AI
- **Verification**: Not done yet.
- **Assumptions**: None

### TASK-APP-009: First mockups — the main window and a confirmation
- **Tier**: M
- **Status**: Not Started
- **Description**: Write the screen descriptions and generate the mockups of the main window connected and disconnected (browser, memory view,
  an edit panel with a placeholder editor) and of a confirmation dialog; render them to PNG to check them; send them to the owner.
- **Requirement refs**: RQ-APP-006, RQ-APP-011, RQ-APP-019, RQ-APP-074
- **ADR refs**: ADR-APP-003
- **Acceptance Criteria** (Gherkin): *Given* the generated mockups, *When* the owner reads them, *Then* they show the three areas, the connection
  state and the confirmation naming the object; *given* `ctest`, *then* the mockup check passes.
- **Dependencies**: TASK-APP-008
- **Assignee**: AI
- **Verification**: Not done yet.
- **Assumptions**: None
