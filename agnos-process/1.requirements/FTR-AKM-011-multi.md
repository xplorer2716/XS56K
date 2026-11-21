# FTR-AKM-011: Multi Primitives (§0C)

## Overview

Phase A, new lot (session AKM, 2026-10-04). Section `0C` is the sampler's Multi section: "control of all of the
parameters of a Multi in real-time" (a mixer map of part levels, pans and mutes) "extended to provide control over the
multis, including creation, deletion and selection of a part's program" (spec p. 26, Tables 16 and 17). This feature
adds one tested primitive per command row: creation, selection and deletion of multis (`&01`, `&02`, `&05`, `&06`,
`&07`, `&08`), the twelve Set/Get pairs of part parameters (`&10`-`&1B`, `&20`-`&2B`), the renaming, the program
number and the part assignment (`&30`-`&34`), and every Get of general information (`&40`-`&48`, `&50`-`&52`) — plus a
real-sampler check on a disposable test multi.

**In scope** (counts from `documents/_index/sysex_spec.items.tsv`, section `0C`: 49 commands and 11 REPLY formats,
60 rows — every row): `&01 &02 &05 &06 &07 &08`, `&10`-`&1B`, `&20`-`&2B`, `&30`-`&34`, `&40`-`&48`, `&50`-`&52`.

**Out of scope.** Sections other than §0C. Editing a part's program: "modification of a Multi part's program must be
done through the Program Section" (spec p. 26): the part's program is found by `&45` (the name of the part) and then
selected in §0A. Multi FX (§12), a different section acting on the current multi.

**Depends on** FTR-AKM-001 (transport) only. Same shape as §0A (FTR-AKM-002): a sampler-wide "current multi" selection
state set by creation or selection, and the items acting on it; part parameters take a part number (0-127) as their
first value, like §06's zone number, so they go through the generic encoder and decoder.

**Shapes new to this section.** `&41` and `&50` carry a flag byte then a number (like §0A's program number, `&31` is
its Set, conditional: the number is only sent when the flag is 1, built by hand); `&46` and `&51` are streams of names
with a single `00` for "no part"; `&47` is twelve bytes at once; `&48` and `&52` carry one byte per part or per multi,
their count depending on the multi; `&32` is a part number then a 14-bit index and `&33` a part number then a name,
both built by hand like §06's `&01`.

**Real-hardware risk.** Creating a multi makes it current and changes the sampler's current multi; deleting the current
multi (`&08`) removes it; "Delete ALL Multis" (`&07`) removes every multi and is guarded like the program and sample
equivalents (`RQ-AKM-088`). The number of parts for *new* multis (`&01`) is a stored setting that no item reads back: the
real-sampler check never sends it (`RQ-AKM-093`).

**Sources.** `documents/_index/sysex_spec.kb.md` (§0C in Table 4 line 77, notes lines 97, 137, 139, 151),
`documents/_index/sysex_spec.items.tsv` (section `0C`), `documents/akai_s5000_s6000_sysex_spec_2.10.pdf.md` printed
pp. 26-28 (Tables 16 and 17).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the Phase B mixer-map and multi-management workflows; CI (simulated sampler only).

---

## Functional Requirements

### RQ-AKM-087: Multi creation, selection and deletion
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller sets the number of parts of new multis (`&01`: 0, 1 or 2 for 32, 64 or 128), creates a multi by name (`&02`), selects a multi by name (`&05`) or by zero-based index (`&06`, two 7-bit data bytes) or deletes the current multi (`&08`), the AKM layer SHALL send the matching item and SHALL report the sampler's ERROR `04` unchanged when no multi has that name or index or none is current, and ERROR `05` unchanged when a multi of that name already exists.
- **Rationale**: the lifecycle of the section, the same shape as the program lifecycle of `RQ-AKM-021`.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler, *When* a multi `MIX1` is created, *Then* it is current (`&43` returns `MIX1`, `&42` its index), the frame carries the ASCII name null-terminated and the number of multis is one more. *Given* parts set to 64 before the creation, *When* the number of parts is read, *Then* it is 64. *Given* a name or an index no multi has, *When* selected, *Then* ERROR `04` is reported; *Given* a name that exists, *When* created, *Then* the sampler's error is reported unchanged. *Given* the current multi deleted, *When* the count is read, *Then* it is one less and none is current.
- **Dependencies**: RQ-AKM-002; FTR-AKM-001 (RQ-AKM-009, RQ-AKM-005)

### RQ-AKM-088: Destructive command guard for "Delete ALL Multis"
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF "Delete ALL Multis" (`&07`) is requested without an explicit confirmation argument that no default supplies, THEN the AKM layer SHALL NOT send it and SHALL complete the request as `NotConfirmed`; no real-sampler test of any feature SHALL call it.
- **Rationale**: it removes every multi from memory, irreversibly without a saved backup (same guard as `RQ-AKM-025` and `RQ-AKM-046`).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* no confirmation, *When* the primitive is called, *Then* nothing is sent and the outcome is `NotConfirmed`. *Given* the confirmation enumerator, *When* called on a simulated sampler holding multis, *Then* `&07` is sent and no multi remains. *Given* the real-sampler test sources, *When* searched for the primitive, *Then* there is no call.
- **Dependencies**: RQ-AKM-087

### RQ-AKM-089: Multi part parameters (Set and Get)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller sets (`&10`-`&1B`) or gets (`&20`-`&2B`) one of the twelve part parameters — MIDI channel, mute, solo, level, output, pan/balance, effects channel, FX send level, fine tune, transpose, low note, high note — of a part (0-127) of the current multi, the AKM layer SHALL send the item with the part number first and the value in its documented range, SHALL refuse a part or a value out of range without sending, and SHALL decode the REPLY of a Get as the single value byte.
- **Rationale**: the real-time mixer-map items, the reason the section exists; same Set/Get shape as `RQ-AKM-034`.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler with a current multi, *When* each of the twelve parameters is set on part 3 then read back, *Then* the value read equals the value set. *Given* a value set on part 3, *When* part 4 is read, *Then* it is unchanged. *Given* a part number above 127 or a value out of its range (a level of 101, a pan of 13), *When* set, *Then* it is refused without sending.
- **Dependencies**: RQ-AKM-002; RQ-AKM-087

### RQ-AKM-090: All the parameters of a part, and the mute and solo state of all parts
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller asks for all the parameters of a part (`&47`: twelve bytes, in the order of items `&20`-`&2B`) or for the mute and solo status of all the parts of the current multi (`&48`: one byte per part, 0 = none, 1 = mute, 2 = solo), the AKM layer SHALL send the item and decode the REPLY, report an empty result when the REPLY does not decode, and refuse the two Gets as `ChecksumModeUnknown` while the port's checksum mode is unknown (`&48`'s length depending on the multi).
- **Rationale**: the two Gets that return many values at once; `&47` must agree with the twelve single Gets.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a part whose twelve parameters were set, *When* `&47` is read, *Then* the twelve bytes equal the twelve single Gets in order. *Given* parts 1 muted and 2 soloed in a 32-part multi, *When* `&48` is read, *Then* 32 values come back with 1 at index 1, 2 at index 2 and 0 elsewhere.
- **Dependencies**: RQ-AKM-089; RQ-AKM-041

### RQ-AKM-091: General information about the current multi and about all the multis
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller asks for the number of multis (`&40`), the current multi's program number (`&41`), index (`&42`) or name (`&43`), its number of parts (`&44`: the REPLY byte plus one), the name of a part (`&45`: empty when none is assigned), the names of all its parts (`&46`), or, for every multi in memory, the program numbers (`&50`), the names (`&51`) or the numbers of parts (`&52`), the AKM layer SHALL send the item, decode the REPLY of Table 17 and refuse the name-carrying and variable-length Gets as `ChecksumModeUnknown` while the port's checksum mode is unknown.
- **Rationale**: the Gets of the section; a REPLY for all the multis repeats a record once per multi (DEC-AKM-014, DEC-AKM-015).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding `A` (32 parts) and `B` (64 parts), *When* the count, the names of all multis and the numbers of parts of all multis are read, *Then* they are 2, `A`, `B` and 32, 64 in memory order. *Given* a multi with a program number set, *When* `&41` and `&50` are read, *Then* the flag and the number agree. *Given* a part with no program assigned, *When* its name is read, *Then* it is empty, not a failure. *Given* the checksum mode unknown, *When* a name Get is requested, *Then* nothing is sent.
- **Dependencies**: RQ-AKM-087; RQ-AKM-041

### RQ-AKM-092: Multi renaming, program number and part assignment
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller renames the current multi (`&30`), sets or clears its program number (`&31`: off, or on with a front-panel number 1-128 sent as 0-127), assigns a program to a part by index (`&32`: the part, then a 14-bit index) or by name (`&33`: the part, then the name) or deletes the program of a part (`&34`), the AKM layer SHALL send the matching item, SHALL refuse an out-of-range part, number or index and a name that is not 7-bit ASCII without sending, and SHALL report the sampler's ERROR `04` unchanged for a program that does not exist.
- **Rationale**: the general information the section lets a caller set; `&31`, `&32` and `&33` have shapes the generic encoder cannot build.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a current multi, *When* it is renamed `MIX2`, *Then* `&43` returns `MIX2`. *Given* the program number set to 5 then cleared, *When* `&41` is read, *Then* it reads on with 4, then off. *Given* a program `LEAD` in memory, *When* assigned to part 2 by name, *Then* `&45` of part 2 returns `LEAD`; *When* the part is deleted, *Then* it returns empty. *Given* a name no program has, *When* assigned, *Then* ERROR `04` is reported. *Given* a front-panel number of 129 or a part of 128, *When* set, *Then* it is refused without sending.
- **Dependencies**: RQ-AKM-087; RQ-AKM-091

### RQ-AKM-093: Real-sampler tests use a dedicated test multi
- **Category**: Functional
- **EARS Type**: State-driven
- **Statement**: WHILE the real-sampler suite exercises section 0C (opt-in `--multi-lifecycle`), the suite SHALL create one multi under a reserved test name, act only on it, assign to its parts only a program it created under a reserved test name, put back the multi that was current, and delete both on every exit path; it SHALL never send `&07` and SHALL never send `&01`, which no item reads back.
- **Rationale**: same rule as `RQ-AKM-027` and `RQ-AKM-033`: the only multi and program the check may delete are its own.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding the owner's multis and programs, *When* the check runs, *Then* the test multi and test program exist only for the length of the check, the owner's items and the current multi are as they were, and no `&07` or `&01` is sent. *Given* a check made to fail half way, *When* it ends, *Then* the test items are deleted. *Given* a test multi name that already exists, *When* the check starts, *Then* it stops without touching it. *Given* the real sampler, *When* the owner runs the check, *Then* the log lists each step.
- **Dependencies**: RQ-AKM-017; RQ-AKM-018; RQ-AKM-027; RQ-AKM-087 to RQ-AKM-092

### RQ-AKM-094: Coverage of section §0C
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Every §0C row of `documents/_index/sysex_spec.items.tsv` SHALL be either covered by a primitive or listed with a reason for its exclusion, and any spec inconsistency met while doing so SHALL be resolved or recorded.
- **Rationale**: same bookkeeping as `RQ-AKM-086`, for the section's sixty rows.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the 49 command rows of section `0C`, *When* compared with the catalogue of primitives and exclusions, *Then* no row is unaccounted for. *Given* any erratum found, *When* the lot ends, *Then* its resolution is recorded with the observation that supports it.
- **Dependencies**: RQ-AKM-087 to RQ-AKM-093

---

## Open points

- **Part count of new multis.** `&01` is stored configuration with no Get; the real-sampler check leaves it alone and reads the part count of the test multi with `&44` (observation: 32, 64 or 128 according to the owner's setting).
- **Unlisted ranges.** Part MIDI channel is `0-31`; the output item's range `0-23`; the spec gives the pan centre as 64 and the fine-tune centre as 50. The catalogue uses the spec's ranges; the real sampler's refusals are observations.
- **What `&48`, `&52` and `&50`/`&51` return with no multi current or no multi in memory** is not stated; the model answers ERROR `04` for the current-multi Gets and ERROR `04` (read as an empty list by the primitives) for the three all-multis Gets, as the real S5000 does for §0A's `&18`/`&19`, to be observed for multis.
- **Section 0C and Clear Sampler Memory.** `&32` of §02 deletes the multis too (`RQ-AKM-056`); the simulated sampler's multi list moves from names to full records, `setMultiNames` keeping its signature.
