# FTR-AKM-003: Keygroup Primitives (§08)

## Overview

Phase A, lot A3 of the S5000 control plan
(`process/1.requirements/DRAFT-s5000-midi-primitives-and-workflows.md`): one tested primitive for each
item of section §08 (Keygroup) — select the current keygroup, and get or set its general options
(keyspan low and high note, mute group, FX override, FX send level, zone crossfade), pitch/amp,
filter, and the filter, amplitude and aux envelopes — each proven by a Set followed by a Get on the
mock and on the real sampler.

**Outline status.** Same as FTR-AKM-002: requirements at the level of the spec's item groups,
refined into per-primitive detail when this lot starts.

**In scope** (counts from `documents/_index/sysex_spec.items.tsv`, section `08`: 80 commands, 40
REPLY formats):

| Group (spec wording) | Commands | REPLY formats |
|---|---|---|
| Keygroup selection (`&01` select, `&02` get current) | 2 | 1 |
| General Options — Set / Get (low/high note, mute group, FX override, FX send level, zone crossfade) | 6 + 6 | 6 |
| Keygroup Pitch/Amp — Set / Get | 5 + 5 | 5 |
| Filter — Set / Get | 6 + 6 | 6 |
| Filter Envelope — Set / Get | 9 + 9 | 9 |
| Amplitude Envelope — Set / Get | 8 + 8 | 8 |
| Aux Envelope — Set / Get | 5 + 5 | 5 |

**Out of scope.** Sections §0A and §06 (FTR-AKM-002, FTR-AKM-004) and every other section; the
blocked-request section §3A (its "never with keygroup 0" caveat, spec state model, makes it a later
optimisation); workflows, including the "voice editor" UI vision of the plan (Phase B).

**Depends on** FTR-AKM-001 (transport) and FTR-AKM-002 (a current program to act on).

**Sources.** `documents/_index/sysex_spec.kb.md` (state model: §08 acts on the current keygroup of
the current program), `documents/akai_s5000_s6000_sysex_spec_2.10.pdf.md` pp. 15–19 (Tables 11 and
12).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: FTR-AKM-004 (zones live inside a keygroup); Phase B workflows KEYSPAN and the
  voice editor (filter, envelopes, LFO); CI.

---

## Functional Requirements

### RQ-AKM-028: Current keygroup selection
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller selects a keygroup (`&01`, 1–99, or 0 for all keygroups) or asks which is current (`&02`), the AKM layer SHALL send the matching command and report the outcome; IF the sampler answers ERROR `181` (requested keygroup does not exist in the current program), THEN it SHALL report it as "keygroup not found" with the number requested.
- **Rationale**: every other §08 item and every §06 item acts on the current keygroup; selecting a keygroup that does not exist is the most likely failure of the lot.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a program with 3 keygroups, *When* keygroup 2 is selected then `&02` runs, *Then* it reports 2. *Given* keygroup 9, *When* selected, *Then* "keygroup not found (9)" is reported. *Given* keygroup 0, *When* selected then a Set runs, *Then* the Set is sent while keygroup 0 (all) is current, and the caller is told it applies to all keygroups.
- **Dependencies**: FTR-AKM-001 (RQ-AKM-005, RQ-AKM-009); FTR-AKM-002 (RQ-AKM-021)

### RQ-AKM-029: Keygroup general options (Set and Get)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller sets or gets a general option of the current keygroup — low note and high note (21–127 = A-1 to G8), mute group (0 = off, 1–32), FX override (0 = off, 1 = FX1, 2 = FX2, 3 = RV3, 4 = RV4), FX send level (0–100), zone crossfade (0 or 1) — the AKM layer SHALL send the matching item in its spec range, refuse an out-of-range value without sending, and decode the Get REPLY; a Get right after a Set SHALL return the value set.
- **Rationale**: the plan's KEYSPAN page composes exactly these primitives; Phase B must not need to invent behaviour.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a keygroup, *When* low note 36 and high note 60 are set then read back, *Then* the values are 36 and 60. *Given* a low note of 20, *When* set, *Then* it is refused without sending. *Given* the real sampler and the test program of FTR-AKM-002 (RQ-AKM-027), *When* the same test runs, *Then* it passes unchanged.
- **Dependencies**: RQ-AKM-028

### RQ-AKM-030: Keygroup voice parameter groups (Set and Get)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller sets or gets a parameter of the groups Pitch/Amp, Filter, Filter Envelope, Amplitude Envelope or Aux Envelope of the current keygroup, the AKM layer SHALL send the matching item with the value encoded in its spec format and range (signed values as `sign, magnitude`), refuse an out-of-range value without sending, and decode the Get REPLY into a typed value; a Get right after a Set SHALL return the value set.
- **Rationale**: these are the parameters of the "voice" the plan wants the UI to foreground (filter, envelopes, pitch/amp); 33 Set and 33 Get items share one shape.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* each Set item of the five groups, *When* a value inside its range is set then read back on the simulated sampler, *Then* the value read equals the value set. *Given* the filter mode values 0–25 and cutoff 0–100, resonance 0–15, attenuation 0–5, *When* each is set then read back, *Then* it round-trips. *Given* a value outside the range, *When* set, *Then* it is refused without sending. *Given* the real sampler, *When* the same test runs on the test program, *Then* it passes unchanged.
- **Dependencies**: RQ-AKM-028; FTR-AKM-001 (RQ-AKM-002)

### RQ-AKM-031: Replies covering all keygroups
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a Get is issued while keygroup 0 (all) is current, the AKM layer SHALL decode the REPLY into one value set per keygroup of the program, in keygroup order starting at 1, and IF the number of sets differs from the program's keygroup count, THEN it SHALL fail with a mismatch error instead of returning a partial result.
- **Rationale**: spec state model: a Get with keygroup 0 returns one data set per keygroup; the count is checkable against `&14` of FTR-AKM-002.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a program of 3 keygroups and keygroup 0 current, *When* Get Low Note runs, *Then* three values are returned in keygroup order. *Given* a REPLY holding two sets for a 3-keygroup program, *When* decoded, *Then* it fails with a mismatch error.
- **Dependencies**: RQ-AKM-028; FTR-AKM-002 (RQ-AKM-023)

### RQ-AKM-032: Coverage of section §08
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Every §08 row of `documents/_index/sysex_spec.items.tsv` SHALL be either covered by a primitive of this lot or listed with a reason for its exclusion, and every spec inconsistency met (the errata list of `documents/_index/sysex_spec.kb.md`: `&6C` numbered 107 instead of 108, `&64` labelled "Aux Env. Velocity→Rate (4 only)" where Off Velocity→Rate is likely meant, `&48` and `&5F` replies naming Off Velocity→Rate where their commands say Off Velocity→Release) SHALL be resolved by observation on the real sampler and the resolution recorded.
- **Rationale**: 120 rows, several with known errata; completeness and correctness must be checkable.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the 80 command rows and 40 REPLY rows of section `08`, *When* compared with the catalogue of primitives and exclusions, *Then* no row is unaccounted for. *Given* each erratum listed, *When* the lot ends, *Then* its resolution is recorded with the observation that supports it.
- **Dependencies**: RQ-AKM-028 to RQ-AKM-031

### RQ-AKM-033: Real-sampler tests stay inside the test program
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: The real-sampler tests of this feature SHALL only add, select and change keygroups of the dedicated test program of FTR-AKM-002 (RQ-AKM-027), SHALL NOT select keygroup 0 (all) before a Set on any other program, and SHALL leave the test program deleted by that requirement's cleanup.
- **Rationale**: a Set with keygroup 0 current overwrites the parameter on every keygroup of the current program; on a user's program that is not reversible.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a current program that is not the test program, *When* a test tries a Set or a select of keygroup 0, *Then* it is refused before sending. *Given* the suite ended, *When* the sampler is inspected, *Then* only its original programs remain, unchanged.
- **Dependencies**: RQ-AKM-028; FTR-AKM-002 (RQ-AKM-027)

---

## Open points

- **Primitive shape**: as in FTR-AKM-002 (ADR-AKM-001).
- **Errata resolution** (RQ-AKM-032): each may need a hardware test that reads a value back under both readings of the spec; cannot be settled from the documents alone.
