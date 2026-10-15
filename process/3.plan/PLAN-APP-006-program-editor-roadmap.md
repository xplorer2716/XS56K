# PLAN-APP-006: Editor Application — Roadmap of the Program Editor

## Overview

Session APP (2026-10-15). The reference path to a program editor that edits a whole program (FTR-APP-008), one tab at a time, each tab usable on
the real S5000 when it is done. Two orders, independent of each other:

- **The order of implementation** (the tasks below): each tab brings at most one new piece of the editor, so that each step is small and tested.
  The program's own parameters first (nothing to choose), then the keygroup selector, then the four zones, then the most complex control.
- **The order of the tabs on screen** (for the musician): the order in which a program is built. Since a layout places its tabs as it likes
  (DEC-APP-024), a tab built fourth can be shown first.

| Implementation | Tab | Scope | New piece |
|---|---|---|---|
| 1 | Program | program | the program editor itself: the program chosen by name, the program-scope parameters of `ParameterCatalogue::standard()` (DEC-APP-030) |
| 2 | Amp | keygroup | the keygroup selector (RQ-APP-058); each parameter says its scope (RQ-APP-056) |
| 3 | Filter | keygroup | none: a layout |
| 4 | Keygroup | keygroup and zones | the parameters of the four zones; a sample chosen among those in memory (RQ-APP-060); orders note and velocity (RQ-APP-092) |
| 5 | (in Keygroup) | program | adding and deleting keygroups (RQ-APP-061) |
| 6 | Mod sources | program and keygroup | none: LFO 1 and LFO 2 (program), the Aux envelope (keygroup) |
| 7 | Modulation | keygroup | the routings of the modulation sources, source to destination |
| 8 | (selector) | keygroup | "all keygroups": a state "the values differ" in the controls (RQ-APP-059, Could) |

**The tabs on screen** (the assistant's proposal; the owner decides, TASK-APP-002):

| Screen | Tab | Holds |
|---|---|---|
| 1 | Keygroup | the key span, the four zones: sample, level, pan, tune, velocity span, playback, mute and solo |
| 2 | Amp | the keygroup's level and velocity, the Amp envelope |
| 3 | Filter | mode, cutoff, resonance, keyboard track, the Filter envelope |
| 4 | Mod sources | LFO 1, LFO 2, the Aux envelope |
| 5 | Modulation | the routings of the sources |
| 6 | Program | output, tuning, pitch bend, portamento: set once |

The three envelopes (Amp, Filter, Aux) are knobs first; an envelope graph could come later as a fifth control of the library (DEC-APP-027),
changing nothing else. A zone's sample could open the sample editor.

## References
- **Requirements**: RQ-APP-056 to RQ-APP-061 (FTR-APP-008); RQ-APP-048 to RQ-APP-055, RQ-APP-091, RQ-APP-092 (FTR-APP-007); RQ-AKM-022 to
  RQ-AKM-035
- **ADRs**: ADR-APP-004 (DEC-APP-024 to DEC-APP-029); ADR-APP-001

This plan implements the tasks in the format specified below.
---

## Tasks

Each task starts with its requirements and, where a new piece needs one, a decision in ADR-APP-004 or a new ADR; its layout is the assistant's
proposal until the owner decides (TASK-APP-002). Every task: core tests on the simulated sampler first, then a run on the real S5000.

### TASK-APP-043: The Program tab
- **Tier**: M
- **Status**: In Progress
- **Description**: The program editor opens on a program selected in the memory view, chosen on the sampler by name; its first tab holds the
  program-scope parameters: Output, Tuning, Pitch bend, Portamento; the header gives the keygroup count. The S5000's SysEx has no MIDI
  setting of a program (§0A), so none is shown.
- **Requirement refs**: RQ-APP-093 (new), RQ-APP-048 (amended), RQ-APP-056, RQ-APP-057
- **ADR refs**: ADR-APP-004 (DEC-APP-030)
- **Acceptance Criteria** (Gherkin): *Given* the program PAD selected, *Then* the Program tab shows its values read from the sampler; *When* the
  person changes one, *Then* it is sent and read back.
- **Dependencies**: TASK-APP-042
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-15: `layouts/program.json`, `screens/program-editor.json`, the mockups
  `program-editor-connected.svg` and `-writing.svg` generated and looked at (a long template name overflows its choice in the mockup). Two new
  cases of `EditTests.cpp` (PAD's fourteen values, "2 keygroups", legato "ON"; loudness 70 and legato typed "off" sent and read back) failed
  first to compile, then passed; the "no editor" case moved to a multi; 71 app cases passed; `ctest -C Debug`: 1215 passed; the editor built
  warning-clean. Real run by the owner (2026-10-15): the label Legato renamed "Mono legato", as the sampler's screen says. Open: a change of
  the tune template or of the key changes nothing on the sampler's screen, and "Read again" gives back the value the editor set, not the
  screen's (the MCP real run of PLAN-MCP-002 read back every value set). Diagnosed with the owner (2026-10-16): the S5000 applies the
  template and the key sent by SysEx (the per-key graphic of MIDI/TUNE shows the new tuning, and it is heard), but its fields TEMPLATE and KEY
  keep showing USER and C, even after leaving the page; the Get gives back the value sent. A firmware behaviour, nothing changed in the editor.
- **Assumptions**: (1) The edit controller generalises from the sample catalogue to a catalogue chosen by the layout.

### TASK-APP-044: The Amp tab and the keygroup selector
- **Tier**: M
- **Status**: Not Started
- **Description**: The keygroup selector (1 to the keygroup count); the Amp tab: the keygroup's level and velocity, the Amp envelope; each parameter
  shows its scope.
- **Requirement refs**: RQ-APP-056, RQ-APP-058
- **ADR refs**: ADR-APP-004
- **Acceptance Criteria** (Gherkin): RQ-APP-058's.
- **Dependencies**: TASK-APP-043
- **Assignee**: AI
- **Verification**: —
- **Assumptions**: —

### TASK-APP-045: The Filter tab
- **Tier**: S
- **Status**: Not Started
- **Description**: Mode, cutoff, resonance, keyboard track, the Filter envelope: a layout, no code expected.
- **Requirement refs**: RQ-APP-057
- **ADR refs**: ADR-APP-004 (DEC-APP-024)
- **Acceptance Criteria** (Gherkin): *Given* keygroup 2 chosen, *Then* the Filter tab shows keygroup 2's filter.
- **Dependencies**: TASK-APP-044
- **Assignee**: AI
- **Verification**: —
- **Assumptions**: —

### TASK-APP-046: The Keygroup tab and its four zones
- **Tier**: M
- **Status**: Not Started
- **Description**: The key span and, for each of the four zones, sample, level, pan, tune, velocity span, playback, mute and solo; the sample chosen
  among the samples in memory; low note before high note, low velocity before high velocity (orders).
- **Requirement refs**: RQ-APP-057, RQ-APP-060, RQ-APP-092
- **ADR refs**: ADR-APP-004
- **Acceptance Criteria** (Gherkin): RQ-APP-060's.
- **Dependencies**: TASK-APP-044
- **Assignee**: AI
- **Verification**: —
- **Assumptions**: (1) A zone is a column of the tab, the zone number a leading argument of its parameters.

### TASK-APP-047: Adding and deleting keygroups
- **Tier**: S
- **Status**: Not Started
- **Description**: In the Keygroup tab, add keygroups and delete the current one after confirmation; the last one cannot be deleted.
- **Requirement refs**: RQ-APP-061
- **ADR refs**: ADR-APP-001
- **Acceptance Criteria** (Gherkin): RQ-APP-061's.
- **Dependencies**: TASK-APP-046
- **Assignee**: AI
- **Verification**: —
- **Assumptions**: —

### TASK-APP-048: The Mod sources tab
- **Tier**: S
- **Status**: Not Started
- **Description**: LFO 1 and LFO 2 (program) and the Aux envelope (keygroup), in one tab mixing the two scopes.
- **Requirement refs**: RQ-APP-056, RQ-APP-057
- **ADR refs**: ADR-APP-004 (DEC-APP-024)
- **Acceptance Criteria** (Gherkin): *Given* the tab shown, *Then* each parameter says whether it belongs to the program or to the keygroup.
- **Dependencies**: TASK-APP-044
- **Assignee**: AI
- **Verification**: —
- **Assumptions**: —

### TASK-APP-049: The Modulation tab
- **Tier**: M
- **Status**: Not Started
- **Description**: The routings of the keygroup modulation sources: each source, its destination and its amount.
- **Requirement refs**: RQ-APP-057
- **ADR refs**: a new decision (the control of a routing)
- **Acceptance Criteria** (Gherkin): *Given* the Aux envelope routed to the filter cutoff, *Then* the tab shows the routing; *When* the person changes
  its amount, *Then* it is sent and read back.
- **Dependencies**: TASK-APP-048
- **Assignee**: AI
- **Verification**: —
- **Assumptions**: —

### TASK-APP-050: All keygroups at once
- **Tier**: M
- **Status**: Not Started
- **Description**: "All keygroups" in the selector: a value shared by all keygroups is shown, otherwise "the values differ"; a change goes to all.
- **Requirement refs**: RQ-APP-059
- **ADR refs**: ADR-APP-004
- **Acceptance Criteria** (Gherkin): RQ-APP-059's.
- **Dependencies**: TASK-APP-045
- **Assignee**: AI
- **Verification**: —
- **Assumptions**: (1) Priority Could: after the program can be edited whole.
