# FTR-AKM-012: Scenelist Primitives (§14)

## Overview

Phase A, new lot (session AKM, 2026-10-04). Section `14` is the sampler's scenelist manipulation: "the protocol
provided for scenelist management only permits the manipulation of existing scenelists; it does not currently
allow the building and adjustment of scenelists" (spec p. 39, Table 26). This feature adds one tested primitive
per command row: the selection of the current scenelist by name (`&05`) or by index (`&06`), its deletion (`&08`)
and renaming (`&09`), and the general information about the scenelists in memory (`&10` count, `&11` name by
index, `&13` current index, `&14` current name) — plus a real-sampler check that reads and round-trips what the
sampler holds.

**In scope** (counts from `documents/_index/sysex_spec.items.tsv`, section `14`: 8 commands and 4 REPLY formats,
12 rows — every row): `&05`, `&06`, `&08`, `&09`, `&10`, `&11`, `&13`, `&14`.

**Out of scope.** Sections other than §14. Building or editing a scenelist: the spec itself says no item does it.
Loading a scenelist from disk (a §10 item, `RQ-AKM-066`, memory item type 6).

**Depends on** FTR-AKM-001 (transport) only. The same shape as the song file half of §16 (FTR-AKM-010): a
sampler-wide "current scenelist" selection state like the current sample (§0E) or the current song file, an index
carried as two 7-bit data bytes and a name as a `String`; no new value format and no hand-built request.

**Real-hardware risk.** Deleting a scenelist removes it from the sampler's memory. The deletion is as destructive
as its §0A, §0E and §16 counterparts, which the spec and the earlier lots leave unguarded (only "Delete ALL" is
guarded); the real-sampler check never deletes anything, and puts back every name it changes (`RQ-AKM-097`).

**Sources.** `documents/_index/sysex_spec.kb.md` (§14 in Table 4, line 39 and line 78),
`documents/_index/sysex_spec.items.tsv` (section `14`), `documents/akai_s5000_s6000_sysex_spec_2.10.pdf.md`
printed p. 39 (Tables 26 and 27).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the Phase B workflows that list and select the sampler's scenelists; CI (simulated sampler only).

---

## Functional Requirements

### RQ-AKM-095: Scenelist selection, deletion and renaming
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller selects a scenelist by name (`&05`) or by zero-based index (`&06`, carried as two 7-bit data bytes, most significant first), deletes the current scenelist (`&08`) or renames it (`&09`), the AKM layer SHALL send the matching item and SHALL report the sampler's ERROR `04` unchanged when no scenelist has that name or index, or none is current.
- **Rationale**: the lifecycle of the section, the same shape as the song file lifecycle of `RQ-AKM-082`.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding the scenelist `SCENE1`, *When* it is selected by name then renamed `SCENE2`, *Then* the frames carry the ASCII name null-terminated, both complete on DONE and `&14` returns `SCENE2`. *Given* a name or an index that no scenelist has, *When* selected, *Then* ERROR `04` is reported. *Given* a scenelist selected by index and then deleted, *When* the count is read, *Then* it is one less, and deleting or renaming with no scenelist current fails with ERROR `04`.
- **Dependencies**: RQ-AKM-002; FTR-AKM-001 (RQ-AKM-009, RQ-AKM-005)

### RQ-AKM-096: General information about the scenelists in memory
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller asks for the number of scenelists (`&10`), the name of a scenelist by index (`&11`), the current scenelist's index (`&13`) or its name (`&14`), the AKM layer SHALL send the item and decode its REPLY (Table 27), SHALL refuse the two name Gets as `ChecksumModeUnknown` while the port's checksum mode is unknown, and SHALL report an empty result, not a value, when the REPLY does not decode.
- **Rationale**: the Gets of the section; a name is a `String` REPLY with no fixed length to delimit it by (DEC-AKM-013).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding `A`, `B`, `C`, *When* the count and the name at each index are read, *Then* the count is 3 and the names are `A`, `B`, `C` in order. *Given* a scenelist selected by index, *When* its current index and current name are read, *Then* they equal what was selected. *Given* the checksum mode unknown, *When* a name is asked for, *Then* nothing is sent.
- **Dependencies**: RQ-AKM-095; RQ-AKM-041

### RQ-AKM-097: Real-sampler check puts back what it changes
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the owner runs the real-sampler suite with the opt-in `--scenelists` option, the suite SHALL read the number of scenelists and the name of each, select each scenelist by index and by name and read back its index and name, rename the first scenelist and read the new name back, and put back every name it changed and the selection it found, even when a step fails; the check SHALL never delete anything and SHALL be skipped, after reading the count and trying the read and select items on an empty memory for the observations, when the sampler holds no scenelist.
- **Rationale**: a scenelist cannot be created by §14 (it is loaded from disk), so the check works on what the sampler holds and changes nothing it does not put back (`RQ-AKM-018`).
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding scenelists, *When* the check runs, *Then* every read and selection agrees, the names are put back, the selection found is restored, and no deletion is sent. *Given* a sampler holding none, *When* the check runs, *Then* it is skipped and nothing was renamed or deleted. *Given* a check made to fail after a rename, *When* it ends, *Then* the name was put back. *Given* the real sampler, *When* the owner runs the check, *Then* the log lists each step and what was read.
- **Dependencies**: RQ-AKM-017; RQ-AKM-018; RQ-AKM-095; RQ-AKM-096

### RQ-AKM-098: Coverage of section §14
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Every §14 row of `documents/_index/sysex_spec.items.tsv` SHALL be either covered by a primitive or listed with a reason for its exclusion, and any spec inconsistency met while doing so SHALL be resolved or recorded.
- **Rationale**: same bookkeeping as `RQ-AKM-086`, for the section's twelve rows.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the 8 command rows of section `14`, *When* compared with the catalogue of primitives and exclusions, *Then* no row is unaccounted for. *Given* any erratum found, *When* the lot ends, *Then* its resolution is recorded with the observation that supports it.
- **Dependencies**: RQ-AKM-095 to RQ-AKM-097

---

## Open points

- **What an empty memory answers.** As for §16, the spec does not say whether `&13`/`&14` answer ERROR `04` with no scenelist current, nor whether `&11` of an out-of-range index is ERROR `04`; the model follows §16 (ERROR `04`) and the real sampler's answer is an observation, not an erratum.
- **Whether selection by name is case-sensitive** and the maximum name length are not stated; the catalogue uses the structural ceiling of §16's names (`0-255`), the real bound being an observation.
- **Whether a scenelist exists on an S5000 at all.** The spec lists the section for the S5000/S6000 pair; a sampler that holds none, or answers ERROR `0` (not supported), is an observation to record, not a failure of the check.
