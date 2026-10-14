# FTR-APP-008: Program Editor

## Overview

Session APP (2026-10-13). The program editor is the only editor of priority 1 (the owner agreed on 2026-10-13 to a first lot of browser,
memory view and program editor). It edits the current program of the sampler (RQ-APP-037) through the parameters the AKM layer already
sends and reads: the program's groups (§0A: Output, MIDI/Tune, Pitch Bend, LFO 1 and 2, Keygroup Modulation Sources, RQ-AKM-024), the
current keygroup's (§08: general options — key span, mute group, FX override and send, zone crossfade — then Pitch/Amp, Filter, Filter
Envelope, Amp Envelope, Aux Envelope, RQ-AKM-029, RQ-AKM-030) and its four zones' (§06, RQ-AKM-034, RQ-AKM-035).

**The tabs** are the groups the owner decides (RQ-APP-049, TASK-APP-002). They are not decided yet, and the owner asked on 2026-10-13
that they not be assumed to follow the sampler's pages, the manual's functions or the SysEx sections one to one (TASK-APP-005).

**Inventory of what the editor must reach** — what the AKM layer sets and gets, listed by where it lives on the sampler. This is a list of
parameters to place, **not a list of tabs**:

| Scope | AKM groups | Requirements |
|---|---|---|
| program (§0A) | Output, MIDI/Tune, Pitch Bend, LFO 1 and 2, Keygroup Modulation Sources; program number; keygroup crossfade switch | RQ-AKM-022, RQ-AKM-024 |
| current keygroup (§08) | general options (low and high note, mute group, FX override, FX send level, zone crossfade), Pitch/Amp, Filter, Filter Envelope, Amp Envelope, Aux Envelope | RQ-AKM-029, RQ-AKM-030 |
| zones 1 to 4 of the current keygroup (§06) | sample, level, pan/balance, output, filter, fine and semitone tune, keyboard track, playback mode, velocity to start, low and high velocity, mute, solo | RQ-AKM-034, RQ-AKM-035 |

The pages of the sampler's EDIT PROGRAM mode (user manual pp. 80-127) are a source for naming the parameters, not a model the tabs must copy.

**Priorities.** Priority 1 (mandatory) is written `Must`, priority 2 (nice to have) `Could` (owner, 2026-10-13).

**Depends on** FTR-APP-007, FTR-AKM-002, FTR-AKM-003, FTR-AKM-004.

**Sources.** `documents/akai_s5000_s6000_user_manual.1.21.pdf.md` (EDIT PROGRAM, pp. 80-140; contents pp. v-vi), `process/1.requirements/FTR-AKM-002-program.md`,
`FTR-AKM-003-keygroup.md`, `FTR-AKM-004-keygroup-zone.md`, `juce/akm/data/items.json`,
`process/1.requirements/DRAFT-s5000-midi-primitives-and-workflows.md` (UI sketch: the voice editor at the heart of the UI).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the person editing a program.

---

## Functional Requirements

### RQ-APP-056: The tabs of the program editor
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The program editor SHALL have the tabs the owner defines (TASK-APP-002), and each parameter SHALL say whether it belongs to the program, to the current keygroup or to a zone of it, a tab being free to mix the three.
- **Rationale**: RQ-APP-049; a person must know whether a change applies to the whole program, to one keygroup or to one zone.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the program PAD open, *When* the person looks at the tab row, *Then* it shows the tabs the owner defined; *Given* a tab holding a program parameter and a keygroup parameter, *When* it is shown, *Then* each says which it belongs to.
- **Dependencies**: RQ-APP-001, RQ-APP-049, RQ-APP-050
  - *Amended 2026-10-13 (owner, TASK-APP-005):* the tabs are groups the owner decides; they are not assumed to follow the sampler's pages, the manual's functions or the SysEx sections one to one.

### RQ-APP-057: Every program parameter has its place
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Every parameter item that the AKM layer sets and gets in the program groups (RQ-AKM-024), the program number and the keygroup crossfade (RQ-AKM-022), the keygroup groups (RQ-AKM-029, RQ-AKM-030) and the zone parameters (RQ-AKM-034) SHALL appear in at least one tab of the program editor.
- **Rationale**: no parameter of the sampler is left unreachable; whether one may be shown in several tabs is left to the owner's grouping.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the item catalogue and the editor's tab definitions, *When* a test compares them, *Then* each such item is in at least one tab, and the test names any item missing.
- **Dependencies**: RQ-APP-001, RQ-APP-056, RQ-AKM-022, RQ-AKM-024, RQ-AKM-029, RQ-AKM-030, RQ-AKM-034
  - *Amended 2026-10-13 (owner, TASK-APP-005):* "exactly one tab" became "at least one tab", so as not to decide for the owner whether a parameter may appear in two groups.

### RQ-APP-058: Choosing the keygroup
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: The program editor SHALL show a keygroup selector from 1 to the program's keygroup count (RQ-AKM-023); WHEN the person chooses a keygroup, the application SHALL make it the sampler's current keygroup (RQ-AKM-028) and read the shown tab again (RQ-APP-053).
- **Rationale**: the keygroup's parameters are those of the sampler's current keygroup.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the program PAD with 3 keygroups, *When* the person chooses keygroup 2, *Then* keygroup 2 is selected on the sampler and the Filter tab shows keygroup 2's filter.
- **Dependencies**: RQ-APP-001, RQ-APP-050, RQ-APP-053, RQ-AKM-023, RQ-AKM-028

### RQ-APP-059: Editing all keygroups at once
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person chooses "all keygroups" in the keygroup selector, the application SHALL select keygroup 0 on the sampler (RQ-AKM-028), show for each keygroup parameter its value when all keygroups share it or that the values differ (RQ-AKM-031), and send a change to all keygroups.
- **Rationale**: the sampler allows it (§08 `&01` with 0); setting a filter on every keygroup is common.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* 3 keygroups with cutoffs 50, 50 and 70, *When* the person chooses "all keygroups", *Then* the cutoff shows that the values differ; *When* the person sets 60, *Then* the three keygroups read 60.
- **Dependencies**: RQ-APP-001, RQ-APP-058, RQ-AKM-028, RQ-AKM-031

### RQ-APP-060: The samples of the zones
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: The Keygroup tab SHALL show the sample assigned to each of the four zones of the current keygroup, or that a zone has none; WHEN the person assigns to a zone a sample chosen among the samples in memory, or no sample, the application SHALL send the assignment by name (RQ-AKM-035) and read it back.
- **Rationale**: without it a program cannot play a loaded sample.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* keygroup 1 with no sample and the sample KICK in memory, *When* the person assigns KICK to zone 1, *Then* zone 1 reads KICK and zones 2 to 4 read no sample.
- **Dependencies**: RQ-APP-001, RQ-APP-052, RQ-APP-058, RQ-AKM-035

### RQ-APP-061: Adding and deleting keygroups
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person adds a number of keygroups to the program, the application SHALL add them (RQ-AKM-022) and update the keygroup selector; WHEN the person deletes the current keygroup, it SHALL ask for the confirmation of RQ-APP-011 naming the keygroup and the program, then delete it; IF it is the program's last keygroup, THEN it SHALL refuse with nothing sent.
- **Rationale**: the keygroup count is otherwise fixed when the program is created; a deleted keygroup loses its settings and zones.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* PAD with 2 keygroups, *When* the person adds 2, *Then* the selector goes from 1 to 4; *When* the person deletes keygroup 3 and confirms, *Then* it goes from 1 to 3; *Given* a program with 1 keygroup, *When* the person deletes it, *Then* nothing is sent.
- **Dependencies**: RQ-APP-001, RQ-APP-011, RQ-APP-058, RQ-AKM-022
