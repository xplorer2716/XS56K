# FTR-APP-007: The Edit Panel — Content by Item, Tabs by Group, Values Read from the Sampler

## Overview

Session APP (2026-10-13). The right area of the main window (RQ-APP-006) is the edit panel: its content depends on the item selected in the
memory view and on the tab selected in a row of tabs at the bottom of the panel (drawn F1 to F4 in the owner's sketch). This feature is what
every editor shares; the program editor, the only one of priority 1, is FTR-APP-008.

**Tabs.** Each editor shows its parameters in groups, one tab per group. The groups are the owner's to decide, editor by editor, in the
editor's own feature. The owner (2026-10-13) has not decided them yet and asked that no one-to-one match be assumed: a tab is not
necessarily a page or a function of the sampler, a section of the SysEx specification or a group of the AKM layer (TASK-APP-005).

**Values.** The editor shows what the sampler holds: a value is read from the sampler before it is shown, every change is sent and read back
(the rule of the MCP server, DEC-MCP-006), and the sampler's front panel can change a value without the application knowing, so the person can
read again. The item catalogue gives each parameter its format and range (`juce/akm/data/items.json`, `ItemDescriptor`); the names a
person knows are those of the sampler's screen (the MCP server's parameter catalogue already holds them, DEC-MCP-005).

**Priorities.** Priority 1 (mandatory) is written `Must`, priority 2 (nice to have) `Could` (owner, 2026-10-13).

**Depends on** FTR-APP-001, FTR-APP-002, FTR-APP-005, FTR-AKM-001 (RQ-AKM-008, one command at a time per port).

**Sources.** The owner's sketch and description; `documents/akai_s5000_s6000_user_manual.1.21.pdf.md` (EDIT PROGRAM pp. 80-140, EDIT SAMPLE
pp. 141-177, MULTI pp. 37-79); `juce/akm/include/akm/ItemDescriptor.hpp`; `process/2.architecture/ADR-MCP-001` (DEC-MCP-005, DEC-MCP-006);
`process/1.requirements/DRAFT-s5000-midi-primitives-and-workflows.md` (UI sketch: the keygroup selector always visible on the FILTER page).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the person editing the sampler; the editors of FTR-APP-008 and FTR-APP-009.

---

## Functional Requirements

### RQ-APP-048: The panel shows the editor of the selected item's kind
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN an item is selected in the memory view, the edit panel SHALL show the editor of its kind with that editor's tabs; WHILE a kind has no editor, the panel SHALL show the item's name and kind and say that its editor does not exist yet.
- **Rationale**: the owner's working principle; in priority 1 only programs have an editor.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the program PAD, *When* it is selected, *Then* the program editor shows PAD with its tabs; *Given* the sample KICK, *When* it is selected, *Then* the panel shows "KICK, sample" and says its editor does not exist yet.
- **Dependencies**: RQ-APP-001, RQ-APP-037

### RQ-APP-049: Tabs at the bottom of the panel, grouping as the owner decides
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: Each editor SHALL show its parameters in groups, one tab per group, the tabs in a row at the bottom of the panel, the groups being those the owner decides in the editor's own feature, which need not follow the sampler's pages, the manual's functions or the SysEx sections; WHEN the person selects a tab, the panel SHALL show the parameters of that group.
- **Rationale**: the owner's sketch (tabs F1 to F4 at the bottom); the owner's remark of 2026-10-13 that the groups are not decided and are not a one-to-one copy of what the sampler exposes.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* an editor whose groups are defined, *When* the person selects a tab, *Then* the panel shows the parameters of that group and no other; *Given* the editor shown, *Then* its tab row is along the bottom edge of the panel.
- **Dependencies**: RQ-APP-001, RQ-APP-048
  - *Amended 2026-10-13 (owner, TASK-APP-005):* the tabs are groups the owner decides; they are not assumed to follow the sampler's pages, the manual's functions or the SysEx sections one to one.

### RQ-APP-050: The context stays in sight
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The edit panel SHALL always show, above the tab's parameters, the name of the item being edited and, for an editor whose parameters belong to a part of the item (a program's keygroup), the selector of that part (RQ-APP-058).
- **Rationale**: the sampler shows the current keygroup on every page of EDIT PROGRAM; a value is meaningless without knowing what it belongs to.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the program PAD open on keygroup 2, *When* the person goes from tab to tab, *Then* "PAD" and keygroup 2 stay shown.
- **Dependencies**: RQ-APP-001, RQ-APP-048

### RQ-APP-051: A parameter is shown as the sampler shows it
- **Category**: Functional
- **EARS Type**: Complex
- **Statement**: Each parameter SHALL be shown with the name the sampler's screen gives it and its value in the sampler's terms (a name for a value that has one, such as an LFO waveform; a number with its unit otherwise), and its control SHALL accept only the range of the item catalogue; IF the person enters a value out of that range, THEN the application SHALL refuse it with nothing sent and show the range.
- **Rationale**: the person knows the sampler's names; a value out of range would be answered by an ERROR.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the LFO 1 waveform, *When* it is shown, *Then* it reads a waveform name such as "TRIANGLE"; *Given* a parameter of range 0 to 100, *When* the person types 120, *Then* nothing is sent and 0 to 100 is shown.
- **Dependencies**: RQ-APP-001, RQ-APP-049, RQ-AKM-001, RQ-AKM-002

### RQ-APP-052: A change is sent, then read back
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person changes a parameter, the application SHALL send the new value to the sampler, show the parameter as pending (RQ-APP-003) until the sampler has answered, then read the value back and show the value read.
- **Rationale**: the sampler may adjust or refuse a value; the editor shows what it holds (DEC-MCP-006, RQ-APP-010).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the filter cutoff at 50, *When* the person sets 70, *Then* the control is pending, the Set is sent, then the Get, and the control shows the value the Get returned.
- **Dependencies**: RQ-APP-001, RQ-APP-003, RQ-APP-010, RQ-APP-051

### RQ-APP-053: The values are read when shown, and on request
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a tab is shown, or the item or the part of the item being edited changes, the application SHALL read from the sampler the parameters that tab shows before it shows their values; WHEN the person asks to read again, it SHALL read again every parameter of the tab shown.
- **Rationale**: the sampler's front panel can change a value without the application knowing.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the filter tab shown, *When* the cutoff is changed on the sampler's front panel and the person asks to read again, *Then* the new cutoff is shown.
- **Dependencies**: RQ-APP-001, RQ-APP-049, RQ-APP-050

### RQ-APP-055: Each kind remembers its tab
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person selects another item of a kind already edited in this run, the edit panel SHALL show the tab last selected for that kind.
- **Rationale**: going from program to program, the person usually works on the same function.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* PAD edited on the filter tab, *When* the person selects BASS, *Then* the filter tab of BASS is shown.
- **Dependencies**: RQ-APP-001, RQ-APP-049

---

## Non-Functional Requirements

### RQ-APP-054: A control moved continuously does not flood the sampler
- **Category**: Non-Functional
- **NFR Type**: Performance
- **EARS Type**: State-driven
- **Statement**: WHILE the person moves a control continuously, the application SHALL have at most one change of that parameter waiting for the sampler's answer, send only the latest value when that answer arrives, and end with the sampler holding the last value the person gave.
- **Metric**: at most one pending Set per parameter; the last value given equals the value read back at the end.
- **Measurement Method**: a test against the simulated sampler moving a control through 100 values while the sampler holds its answers.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler answering slowly, *When* the control goes through 100 values, *Then* never more than one Set of that parameter is pending, and the last value is the one read back at the end.
- **Dependencies**: RQ-APP-001, RQ-APP-052, RQ-AKM-008
