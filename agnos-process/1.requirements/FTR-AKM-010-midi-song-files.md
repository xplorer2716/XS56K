# FTR-AKM-010: MIDI Song File Primitives (§16)

## Overview

Phase A, new lot (session AKM, 2026-10-04). Section `16` is the sampler's MIDI song file tools: "a simple means
to determine which song files are currently loaded. Control of playback of these files should be done by standard
MIDI messages, not by SysEx" (spec p. 40, Table 28). This feature adds one tested primitive per command row: the
selection of the current song file by name (`&05`) or by index (`&06`), its deletion (`&08`) and renaming (`&09`),
the general information about the song files in memory (`&10` count, `&11` name by index, `&13` current index,
`&14` current name), and the four set list items (`&20` count, `&21` name by index, `&22` delete by index, `&23`
rename by index) — plus a real-sampler check that reads and round-trips what the sampler holds.

**In scope** (counts from `documents/_index/sysex_spec.items.tsv`, section `16`: 12 commands and 6 REPLY
formats, 18 rows — every row): `&05`, `&06`, `&08`, `&09`, `&10`, `&11`, `&13`, `&14`, `&20`, `&21`, `&22`, `&23`.

**Out of scope.** Sections other than §16. Playing a song file: the spec itself says it is done by standard MIDI
messages. Loading a song file from disk (a §10 item, `RQ-AKM-066`).

**Depends on** FTR-AKM-001 (transport) only. Same shape as §0E (FTR-AKM-005): a sampler-wide "current song file"
selection state, like the current sample, plus a set list sub-group addressed by index only (no current set list).

**Shapes new to this section.** Rename Set List by index (`&23`) takes a 14-bit index (two data bytes) *then* a
name, a shape that fits neither `makeStringRequest` (one `String`) nor the generic path (no `String`): it is built
by hand, like `&2A` of §10 and Set Program Number of §0A (DEC-AKM-013). There is no "all names" item for song
files or set lists: a caller reads the count then each name by index.

**Real-hardware risk.** Deleting a song file or a set list removes it from the sampler's memory. The lifecycle
primitives are as destructive as their §0A and §0E counterparts, which the spec and the earlier lots leave unguarded
(only "Delete ALL" is guarded); the real-sampler check never deletes anything, and puts back every name it changes
(`RQ-AKM-085`).

**Sources.** `documents/_index/sysex_spec.kb.md` (§16 in Table 4, line 80; errata lines 192-193),
`documents/_index/sysex_spec.items.tsv` (section `16`), `documents/akai_s5000_s6000_sysex_spec_2.10.pdf.md`
printed p. 40 (Tables 28 and 29).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the Phase B workflows that list and select the sampler's song files and set lists; CI
  (simulated sampler only).

---

## Functional Requirements

### RQ-AKM-082: Song file selection, deletion and renaming
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller selects a song file by name (`&05`) or by zero-based index (`&06`, carried as two 7-bit data bytes, most significant first), deletes the current song file (`&08`) or renames it (`&09`), the AKM layer SHALL send the matching item and SHALL report the sampler's ERROR `04` unchanged when no song file has that name or index, or none is current.
- **Rationale**: the lifecycle of the section, the same shape as the sample lifecycle of `RQ-AKM-045`.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding the song file `SONG1`, *When* it is selected by name then renamed `SONG2`, *Then* the frames carry the ASCII name null-terminated, both complete on DONE and `&14` returns `SONG2`. *Given* a name or an index that no song file has, *When* selected, *Then* ERROR `04` is reported. *Given* a song file selected by index and then deleted, *When* the count is read, *Then* it is one less, and deleting or renaming with no song file current fails with ERROR `04`.
- **Dependencies**: RQ-AKM-002; FTR-AKM-001 (RQ-AKM-009, RQ-AKM-005)

### RQ-AKM-083: General information about the song files in memory
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller asks for the number of song files (`&10`), the name of a song file by index (`&11`), the current song file's index (`&13`) or its name (`&14`), the AKM layer SHALL send the item and decode its REPLY (Table 29), SHALL refuse the two name Gets as `ChecksumModeUnknown` while the port's checksum mode is unknown, and SHALL report an empty result, not a value, when the REPLY does not decode.
- **Rationale**: the Gets of the section; a name is a `String` REPLY with no fixed length to delimit it by (DEC-AKM-013).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding `A`, `B`, `C`, *When* the count and the name at each index are read, *Then* the count is 3 and the names are `A`, `B`, `C` in order. *Given* a song file selected by index, *When* its current index and current name are read, *Then* they equal what was selected. *Given* the checksum mode unknown, *When* a name is asked for, *Then* nothing is sent.
- **Dependencies**: RQ-AKM-082; RQ-AKM-041

### RQ-AKM-084: Set lists
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller asks for the number of set lists (`&20`) or the name of a set list by index (`&21`), deletes a set list by index (`&22`) or renames it (`&23`: the index, then the new name), the AKM layer SHALL send the matching item with the index as two 7-bit data bytes, most significant first, decode the REPLYs of Table 29, and report ERROR `04` unchanged for an index that names no set list.
- **Rationale**: the second sub-group of the section; unlike song files, a set list has no "current" selection: every item takes its index.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding the set lists `SET1`, `SET2`, *When* the count and each name are read, *Then* the count is 2 and the names are `SET1`, `SET2`. *Given* the set list at index 1 renamed `SET3`, *When* its name is read, *Then* it is `SET3`, and the rename frame carries the two index bytes, then the name null-terminated. *Given* the set list at index 0 deleted, *When* the count is read, *Then* it is 1 and the name at index 0 is the one that was at index 1. *Given* an index with no set list, *When* a name, a deletion or a rename is asked for, *Then* ERROR `04` is reported.
- **Dependencies**: RQ-AKM-002; RQ-AKM-041; FTR-AKM-001 (RQ-AKM-009, RQ-AKM-005)

### RQ-AKM-085: Real-sampler check puts back what it changes
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the owner runs the real-sampler suite with the opt-in `--song-files` option, the suite SHALL read the number of song files and set lists and the name of each, select each song file by index and by name and read back its index and name, rename the first song file and the first set list and read the new name back, and put back every name it changed and the selection it found, even when a step fails; the check SHALL never delete anything and SHALL be skipped, after reading the two counts and sending nothing else, when the sampler holds no song file and no set list.
- **Rationale**: song files and set lists cannot be created by §16 (a song file is loaded from disk), so the check works on what the sampler holds and changes nothing it does not put back (`RQ-AKM-018`).
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding song files and set lists, *When* the check runs, *Then* every read and selection agrees, the names are put back, the selection found is restored, and no deletion is sent. *Given* a sampler holding none, *When* the check runs, *Then* it is skipped and nothing but the counts was read. *Given* a check made to fail after a rename, *When* it ends, *Then* the name was put back. *Given* the real sampler, *When* the owner runs the check, *Then* the log lists each step and what was read.
- **Dependencies**: RQ-AKM-017; RQ-AKM-018; RQ-AKM-082; RQ-AKM-083; RQ-AKM-084

### RQ-AKM-086: Coverage of section §16
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Every §16 row of `documents/_index/sysex_spec.items.tsv` SHALL be either covered by a primitive or listed with a reason for its exclusion, and any spec inconsistency met while doing so SHALL be resolved or recorded.
- **Rationale**: same bookkeeping as `RQ-AKM-077`, for the section's eighteen rows.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the 12 command rows of section `16`, *When* compared with the catalogue of primitives and exclusions, *Then* no row is unaccounted for. *Given* any erratum found, *When* the lot ends, *Then* its resolution is recorded with the observation that supports it.
- **Dependencies**: RQ-AKM-082 to RQ-AKM-085

---

## Open points

- **Errata noted in the kb.** Table 29's title says `§&14{20}` where it means `§&16{22}`, and Table 28's sub-group header "Scenelist Songfile" is a copy/paste leftover (`sysex_spec.kb.md` lines 192-193): cosmetic, no effect on the wire.
- **What an empty memory answers.** The spec does not say whether `&13`/`&14` answer ERROR `04` or something else with no song file current, nor whether `&11` of an out-of-range index is ERROR `04`; the model follows §0E (ERROR `04`) and the real sampler's answer is an observation, not an erratum. Observed on the real S5000 (OS 2.14, nothing in memory): ERROR `04` for `&13`, `&11`, `&21`, `&06` and `&05` (`process/2.architecture/OBSERVATIONS-RQ-AKM-085-song-files.md`); the answer with something in memory is still to observe.
- **Whether selection by name is case-sensitive** and the maximum name length are not stated; the catalogue uses the structural ceiling of §0E's names (`0-255`), the real bound being an observation.
