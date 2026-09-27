# FTR-AKM-002: Program Primitives (§0A)

## Overview

Phase A, lot A2 of the S5000 control plan
(`process/1.requirements/DRAFT-s5000-midi-primitives-and-workflows.md`): one tested primitive for each
item of section §0A (Program) that the plan puts in scope — select, create, rename and delete a
program, read its general information, and get or set its parameters (output, MIDI/tune, pitch bend,
LFOs, keygroup modulation sources) — each proven by a Set followed by a Get on the mock and on the
real sampler.

**Outline status.** The requirements below are at the level of the item groups of the spec's own
tables. They are refined into per-primitive detail, and the value ranges checked against
`documents/_index/sysex_spec.items.tsv`, when this lot starts (after FTR-AKM-001 is implemented, so
that the primitive mechanism decided by its ADR is known).

**In scope** (counts from `documents/_index/sysex_spec.items.tsv`, section `0A`: 95 commands, 46
REPLY formats):

| Group (spec wording) | Commands | REPLY formats |
|---|---|---|
| Program Selection, Creation and Deletion (`&02`, `&03`, `&05`–`&0D`) | 11 | — |
| General Program Information, current program (`&10`–`&15`) | 6 | 6 |
| General Program Information, all programs in memory | 2 | 2 |
| Output — Set / Get | 6 + 6 | 6 |
| MIDI/Tune — Set / Get | 5 + 5 | 5 |
| Pitch Bend — Set / Get | 8 + 8 | 8 |
| LFOs — Set / Get (`<Data1>` = LFO 1 or 2) | 16 + 16 | 16 |
| Keygroup Modulation Sources — Set / Get | 3 + 3 | 3 |

**Out of scope.** Sections §08, §06 (FTR-AKM-003, FTR-AKM-004), Multi, Sample, Disk and every other
section; loading a program from a disk file (needs Disk, so only programs already in the sampler's
memory can be worked on); the alternative-by-index and blocked-request sections (§2A, §3A, §3C),
which are a later optimisation of what this lot does with the normal section; workflows.

**Depends on** FTR-AKM-001 (transport).

**Sources.** `documents/_index/sysex_spec.kb.md` (state model: §0A acts on the *current* program),
`documents/akai_s5000_s6000_sysex_spec_2.10.pdf.md` pp. 20–25 (Tables 13 and 14) and p. 25
(Table 15, modulation sources).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: FTR-AKM-003 and FTR-AKM-004 (they act on the current program this lot selects); the
  Phase B workflow "open a program"; CI.

---

## Functional Requirements

### RQ-AKM-021: Program lifecycle primitives
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller creates a program (`&02`, or `&03` with a keygroup count), selects one by name (`&05`) or by index (`&06`), renames the current one (`&09`) or deletes the current one (`&08`), the AKM layer SHALL send the matching §0A command and report its outcome, and IF the sampler answers ERROR `04` (not found) or `05` (could not create), THEN it SHALL report that error unchanged.
- **Rationale**: these are the entry points every later primitive depends on: the sampler acts on the *current* program, and creating one also makes it current (spec state model).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler, *When* a program named `TESTPRG` is created then selected by name, *Then* the frames carry the ASCII name null-terminated and the commands complete on DONE. *Given* a name that does not exist, *When* it is selected, *Then* the ERROR `04` is reported. *Given* the real sampler and the test program of RQ-AKM-027, *When* create, select by index, rename and delete are run, *Then* each is followed by a Get (`&13` name, `&10` count) confirming its effect.
- **Dependencies**: FTR-AKM-001 (RQ-AKM-009, RQ-AKM-005)

### RQ-AKM-022: Program structure and identity primitives
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller sets the "Program Number" (`&0A`), adds keygroups to the current program (`&0B`), deletes one of its keygroups (`&0C`) or sets or gets keygroup crossfade (`&0D`, `&15`), the AKM layer SHALL send the matching command with the values in their spec ranges (keygroups to add 1–98, keygroup to delete zero-based) and refuse an out-of-range value without sending.
- **Rationale**: these commands change the shape of the program that FTR-AKM-003 and FTR-AKM-004 then address.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a program with 1 keygroup, *When* 3 are added, *Then* Get number of keygroups (`&14`) reports 4. *Given* the value 0 for keygroups to add, *When* requested, *Then* it is refused without sending. *Given* crossfade set to ON, *When* Get crossfade (`&15`) runs, *Then* it reports ON.
- **Dependencies**: RQ-AKM-021

### RQ-AKM-023: General program information
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller reads the general information of the current program (number of programs, its "Program Number", its index, its name, its number of keygroups, its crossfade) or of all programs in memory, the AKM layer SHALL send the matching Get and decode the REPLY into typed values, including the concatenated null-terminated names of the all-programs reply.
- **Rationale**: the Phase B workflow "open a program" starts with this information; the string-list format is the first place RQ-AKM-002's string decoding is exercised on real data.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding programs `A`, `B`, `C`, *When* the names of all programs are read, *Then* the result is the ordered list `A`, `B`, `C`. *Given* the current program, *When* its index, name and keygroup count are read, *Then* they equal what was created in RQ-AKM-021 and RQ-AKM-022.
- **Dependencies**: RQ-AKM-002; RQ-AKM-021

### RQ-AKM-024: Program parameter groups (Set and Get)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller sets or gets a parameter of the groups Output, MIDI/Tune, Pitch Bend, LFOs (LFO 1 or 2 given by `<Data1>`) or Keygroup Modulation Sources, the AKM layer SHALL send the matching item with the value encoded in its spec format and range, refuse an out-of-range value without sending, and decode the Get REPLY into a typed value; a Get immediately after a Set SHALL return the value set.
- **Rationale**: the plan's method — one item, one Set test, one Get test, verified by read-back — and the plan's exit criterion for Phase A.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* each Set item of the five groups, *When* a value inside its range is set then read back on the simulated sampler, *Then* the value read equals the value set. *Given* a value outside the range, *When* set, *Then* it is refused without sending. *Given* the real sampler, the test program of RQ-AKM-027 and the same values, *When* the same test runs, *Then* it passes unchanged.
- **Dependencies**: RQ-AKM-021; RQ-AKM-002

### RQ-AKM-025: Destructive command guard
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a caller requests "Delete ALL programs from memory" (`&07`), THEN the AKM layer SHALL send it only when the caller passes an explicit confirmation argument that no default supplies, and no real-sampler test of any feature SHALL call it.
- **Rationale**: `&07` empties the sampler's program memory and is not reversible without a saved backup; a primitive that is one mistyped call away from that must not be usable by accident.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a request for `&07` without the confirmation argument, *When* made, *Then* nothing is sent and an error explains why. *Given* the source of the real-sampler tests, *When* searched for the `&07` primitive, *Then* there is no call.
- **Dependencies**: RQ-AKM-021

### RQ-AKM-026: Coverage of section §0A
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Every §0A row of `documents/_index/sysex_spec.items.tsv` SHALL be either covered by a primitive of this lot or listed with a reason for its exclusion, and any spec inconsistency met while doing so (the errata list of `documents/_index/sysex_spec.kb.md`) SHALL be resolved by observation on the real sampler and the resolution recorded.
- **Rationale**: 141 spec rows are too many to track by eye; the completeness of the lot must be checkable, and the spec is known to contain slips (for example `&2C`/`&2D` labelled Amp Pan where Pan Mod is meant).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the 95 command rows and 46 REPLY rows of section `0A`, *When* compared with the catalogue of primitives and exclusions, *Then* no row is unaccounted for. *Given* an erratum of the KB touching §0A, *When* the lot ends, *Then* its resolution is recorded with the observation that supports it.
- **Dependencies**: RQ-AKM-021 to RQ-AKM-025

### RQ-AKM-027: Real-sampler tests use a dedicated test program
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: The real-sampler tests of this feature SHALL only create, select, change and delete a program they created themselves under a reserved test name, SHALL delete it before returning even when a test fails or is interrupted, and SHALL restore the sampler's current-program selection to the one found.
- **Rationale**: the plan's methodological constraint — a test against the real sampler changes real memory and is not idempotent like the mock; renaming or deleting a program that was there before is irreversible.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a real sampler holding programs `KEEP1` and `KEEP2` and `KEEP1` selected, *When* the suite runs and one test fails halfway, *Then* afterwards the sampler holds exactly `KEEP1` and `KEEP2`, both unchanged, and `KEEP1` is current. *Given* a test that would act on a program not created by the suite, *When* it runs, *Then* it is refused before sending.
- **Dependencies**: RQ-AKM-021; FTR-AKM-001 (RQ-AKM-018)

---

## Open points

- **Primitive shape** (one function per item, or a table-driven descriptor with generic Get/Set): decided by ADR-AKM-001 at the end of FTR-AKM-001; this feature's per-primitive requirements are written with that answer known.
- **Sync LCD** (FTR-AKM-001 open point): whether program-selecting primitives switch it off is decided here, when the first program-selecting primitive exists.
- **Program numbering across the spec** (front-panel 1–128 sent as 0–127, prefixed by an OFF/ON byte for `&0A`): to be pinned down against the real sampler while refining RQ-AKM-022.
