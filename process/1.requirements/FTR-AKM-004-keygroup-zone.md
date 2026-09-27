# FTR-AKM-004: Keygroup Zone Primitives (§06)

## Overview

Phase A, lot A4 of the S5000 control plan
(`process/1.requirements/DRAFT-s5000-midi-primitives-and-workflows.md`): one tested primitive for each
item of section §06 (Keygroup Zone) — assign a sample to a zone by name, and get or set a zone's
level, pan/balance, output, filter, fine and semitone tune, keyboard track, playback mode,
velocity→start, velocity range, mute and solo — each proven by a Set followed by a Get on the mock
and on the real sampler. A keygroup has four zones; every §06 message names the zone in its first
data byte (1–4, or 0 for all four) and acts on the current keygroup of the current program.

**Outline status.** Same as FTR-AKM-002: requirements at the level of the spec's item groups,
refined into per-primitive detail when this lot starts.

**Decision recorded (session AKM, owner).** `Set Zone Sample` (`&01`) takes the sample's **name** as a
string. In Phase A the caller supplies that name (manual entry); no sample listing is part of this
lot. The read-only alternative (`§0E/&12`, "names of all samples in memory", for a picker) is not
adopted here and is left to a later lot or to Phase B.

**In scope** (counts from `documents/_index/sysex_spec.items.tsv`, section `06`: 28 commands, 14
REPLY formats): the 14 Set items `&01`–`&0E` (sample, level, pan/balance, output, filter, fine tune,
semitone tune, keyboard track, playback, velocity→start, high velocity, low velocity, mute, solo)
and their 14 Get counterparts and REPLY formats.

**Out of scope.** Listing, editing or loading samples (Sample §0E, Disk §10); sections §0A and §08
(FTR-AKM-002, FTR-AKM-003) and every other section; the blocked-request section §38 (its "never with
zone 0" caveat makes it a later optimisation); workflows (the plan's KEYGROUP ZONES and KEYGROUP
CROSSFADE pages, Phase B).

**Depends on** FTR-AKM-001 (transport), FTR-AKM-002 (a current program) and FTR-AKM-003 (a current
keygroup).

**Sources.** `documents/_index/sysex_spec.kb.md` (state model: §06), `documents/akai_s5000_s6000_sysex_spec_2.10.pdf.md`
pp. 12–14 (Tables 9 and 10).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: Phase B workflows KEYGROUP ZONES and KEYGROUP CROSSFADE; CI.

---

## Functional Requirements

### RQ-AKM-034: Zone parameters (Set and Get)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller sets or gets a parameter of a zone of the current keygroup — level, pan/balance (14–114 = L50 to R50, centre 64), output, filter, fine tune, semitone tune, keyboard track, playback mode (0–6), velocity→start, high velocity, low velocity, mute or solo — the AKM layer SHALL send the matching item with the zone number (1–4) as first data byte and the value encoded in its spec format and range (signed values as `sign, magnitude`), refuse an out-of-range value or a zone number outside 0–4 without sending, and decode the Get REPLY into a typed value; a Get right after a Set SHALL return the value set.
- **Rationale**: the plan's method — one item, one Set test, one Get test, verified by read-back — and its exit criterion for Phase A.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* each of the 13 non-sample Set items, *When* a value inside its range is set on zone 2 then read back on the simulated sampler, *Then* the value read equals the value set and zones 1, 3 and 4 are unchanged. *Given* a zone number 5, *When* set, *Then* it is refused without sending. *Given* the real sampler and the test program of FTR-AKM-002 (RQ-AKM-027), *When* the same test runs, *Then* it passes unchanged.
- **Dependencies**: FTR-AKM-001 (RQ-AKM-002, RQ-AKM-009); FTR-AKM-003 (RQ-AKM-028)

### RQ-AKM-035: Zone sample assignment by name
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller assigns a sample to a zone, the AKM layer SHALL send `Set Zone Sample` (`&01`) with the zone number and the caller-supplied name as a null-terminated ASCII string, and IF the sampler answers ERROR `04` (requested item not found), THEN it SHALL report it as "sample not found" with the name given; WHEN a caller reads a zone's sample (`Get Zone Sample`, `&21`), it SHALL return the name, or "no sample assigned" when the REPLY is the single byte `00` the spec defines for that case; the layer SHALL NOT offer a list of the sampler's samples.
- **Rationale**: owner decision recorded above — manual entry in Phase A keeps this lot independent of the Sample domain.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding a sample `KICK`, *When* it is assigned to zone 1, *Then* the frame carries section `06`, item `01`, data `01` then `4B 49 43 4B 00`, and the command completes on DONE and `Get Zone Sample` (`&21`) returns `KICK`. *Given* a zone with no sample and a REPLY of the single byte `00`, *When* `&21` is read, *Then* "no sample assigned" is returned. *Given* a name that does not exist, *When* assigned, *Then* "sample not found (NOPE)" is reported. *Given* a name containing a byte above `7F`, *When* assigned, *Then* it is refused without sending.
- **Dependencies**: RQ-AKM-034; FTR-AKM-001 (RQ-AKM-002, RQ-AKM-005)

### RQ-AKM-036: Replies covering several zones
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a Get is issued with zone 0 (all four zones), the AKM layer SHALL decode the REPLY into four value sets in zone order, and WHEN it is issued with zone 0 while keygroup 0 (all) is current, it SHALL decode one set per zone of every keygroup of the program (keygroup 1 first); IF the number of sets differs from what the program's shape implies, THEN it SHALL fail with a mismatch error instead of returning a partial result.
- **Rationale**: spec state model (§06): "Get zone 0 → 4 sets; zone 0 + keygroup 0 → sets for every zone of every keygroup (keygroup 1 first)"; the layer must not misassign a set to the wrong zone.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a keygroup and zone 0, *When* Get Zone Level runs, *Then* four values are returned in zone order. *Given* a 2-keygroup program, keygroup 0 current and zone 0, *When* Get Zone Level runs, *Then* eight values are returned, indexed by keygroup and zone. *Given* a REPLY holding three sets where four are implied, *When* decoded, *Then* it fails with a mismatch error.
- **Dependencies**: RQ-AKM-034; FTR-AKM-003 (RQ-AKM-031)

### RQ-AKM-037: Coverage of section §06
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Every §06 row of `documents/_index/sysex_spec.items.tsv` SHALL be either covered by a primitive of this lot or listed with a reason for its exclusion, and every spec inconsistency met (the errata list of `documents/_index/sysex_spec.kb.md`: the `&27` REPLY row labelled "Set Zone Semitone Tune" where the Get is meant) SHALL be resolved by observation on the real sampler and the resolution recorded.
- **Rationale**: 42 rows, and a REPLY table with a wrong label; completeness and correctness must be checkable.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the 28 command rows and 14 REPLY rows of section `06`, *When* compared with the catalogue of primitives and exclusions, *Then* no row is unaccounted for. *Given* the erratum listed, *When* the lot ends, *Then* its resolution is recorded with the observation that supports it.
- **Dependencies**: RQ-AKM-034 to RQ-AKM-036

### RQ-AKM-038: Real-sampler tests use only what they own
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: The real-sampler tests of this feature SHALL only set zones of the dedicated test program of FTR-AKM-002 (RQ-AKM-027), SHALL use for sample assignment only a sample name the operator names in the test configuration and SHALL NOT create, change or delete any sample, and IF no such name is configured, THEN the sample-assignment tests SHALL be reported as skipped, not as passed.
- **Rationale**: without the Disk lot no sample can be loaded by the software, so a sample already in the sampler's memory has to be chosen by hand; a silently passing test that assigned nothing would hide the gap.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* no sample name in the test configuration, *When* the real-sampler suite runs, *Then* the sample-assignment tests are reported as skipped with the reason and all other §06 tests run. *Given* a configured sample name, *When* the suite ends, *Then* the sampler holds the same samples as before and only the test program's zones were changed (then deleted with the test program).
- **Dependencies**: RQ-AKM-035; FTR-AKM-002 (RQ-AKM-027)

---

## Open points

- **Primitive shape**: as in FTR-AKM-002 (ADR-AKM-001).
- **Sample listing for a picker** (`§0E/&12`, read-only): not adopted in Phase A (owner decision above); revisit when the Phase B workflow "KEYGROUP ZONES" is designed.
