# FTR-AKM-007: Disk Tools Primitives (§10)

## Overview

Phase A, new lot (session AKM, 2026-10-02). Section `10` is the sampler's disk/folder/file layer:
discover and select a disk, read its format/free space/name, navigate and manage folders, list and
manage files, load a multi/program/sample/scenelist/SMF from disk, save memory items to disk, and
audition a sample straight from disk without loading it. This feature adds one tested primitive per
command row, grouped by the spec's own "General Disk Functions" / "Folder Functions" / "File
Functions" headings (Table 20/21, `documents/_index/sysex_spec.kb.md` line 36).

**In scope** (counts from `documents/_index/sysex_spec.items.tsv`, section `10`: 35 commands, 16
REPLY formats, 51 rows — every row): `&01`–`&0B`, `&0D`, `&0E`, `&10`–`&18`, `&20`–`&24`, `&28`–`&31`.

**Out of scope.** Sections other than §10. Loading/saving Multi FX, Scenelist or MIDI Song Files as
*content* (their own sections, `&2C`'s Type values `4`/`5`/`6` aside, which this feature's Save items
pass through untyped — this feature does not interpret what it saves or loads, only sends the bytes
the spec defines). Any disk operation on the owner's own data outside a disposable test folder
created for the purpose (`RQ-AKM-071`).

**Depends on** FTR-AKM-001 (transport, Still Alive, timeout) only: §10 items are sampler-wide, like
§02, with no current program, keygroup, zone or sample.

**Known spec inconsistency.** Table 20 lists both `&0D` (Eject Disk) and `&0E` (Get the name of the
specified disk) with decimal `13` — `&0E` is `14` in hex, so one of the two is a transcription slip
(`documents/_index/sysex_spec.kb.md` line 178: "`&0E` is 14 (p32)"). Unlike the §02 Play Mode erratum
(`RQ-AKM-057`), this one needs no real-sampler observation to resolve: it is a decimal/hex column
mismatch within the printed table itself, the same kind already catalogued as `KNOWN_DEC_ERRATA` for
§08's `&6C` (`TASK-AKM-034`). `RQ-AKM-072` requires it recorded there.

**New value format.** Get Free Space (`&0B`) returns "8 bytes... as a Compound Quad Word" — the
`qword` format the codec already encodes and decodes (`ByteReader::readQword`,
`ByteWriter::appendQword`, `RQ-AKM-002`) but the item-catalogue schema does not yet accept (it is
listed as deferred in `generate_akm_items.py`, the same way `string` was before `DEC-AKM-013`). This
feature is the first to need it; the catalogue-schema decision is settled by whichever task
implements `RQ-AKM-062`, the same way `DEC-AKM-013` was settled by `TASK-AKM-014`.

**Known real-hardware risk.** Table 20's own footnote (`#FN 5 b`) warns that disk operations "may
require substantial time to execute" and relies on Still Alive's `F0 F7` to keep the host from
timing out. On the owner's S5000 (OS 2.14, 2026-09-27), `&01` (Update List of Disks) was observed to
answer an OK and then **nothing at all** — no DONE, no ERROR, no `F0 F7`, Still Alive on — and left
the sampler answering no SysEx of any kind until it was power-cycled by hand
(`process/2.architecture/OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`, F4–F7;
`ADR-AKM-001` DEC-AKM-006, DEC-AKM-008). This feature includes `&01` and the other items the same
footnote covers (`&15`, `&2A`, `&2B`, `&2C`, `&2D`) rather than deferring them, but gates every one of
them behind an explicit, non-default guard for any execution against the real sampler (`RQ-AKM-070`);
they are proven on the simulated sampler like every other item in this catalogue.

**Sources.** `documents/_index/sysex_spec.kb.md` (§10 in Table 4 line 36, disk workflow line 106-107,
disk-specific error numbers lines 69-71, value formats line 86, disk type/format codes line 143,
errata line 178), `documents/_index/sysex_spec.items.tsv` (section `10`),
`documents/akai_s5000_s6000_sysex_spec_2.10.pdf.md` printed pp. 32-34 (Tables 20 and 21),
`process/2.architecture/OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`.

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the Phase B workflows that browse, load and save sampler content; CI (simulated
  sampler only — no real-sampler test in this feature runs by default, see `RQ-AKM-070`).

---

## Functional Requirements

### RQ-AKM-060: Disk discovery
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller requests the sampler to update its list of connected disks (`&01`),
  the number of disks connected (`&04`) or the list of all connected disks (`&05`, each entry
  decoding a handle, disk type, format, SCSI ID, writable flag and name), the AKM layer SHALL send
  the matching item and decode the REPLY into a typed value or list; the AKM layer SHALL NOT assume
  `&04` or `&05` reflect the sampler's actual disks until `&01` has completed at least once in the
  session (spec footnote `#FN 20 b`).
- **Rationale**: the entry point of every disk workflow (`sysex_spec.kb.md` line 106); the ordering
  constraint is the spec's own, not an AKM choice.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler with two disks, *When* `&01` then
  `&04` then `&05` are sent, *Then* the count is `2` and the list decodes both entries' handle, type,
  format, SCSI ID, writable flag and name. *Given* a session that has not sent `&01`, *When* `&04` or
  `&05` is requested, *Then* the AKM layer still sends it (the sampler, not the AKM layer, owns
  staleness) but documents the precondition in the primitive's own comment.
- **Dependencies**: RQ-AKM-002; FTR-AKM-001 (RQ-AKM-009, RQ-AKM-005); RQ-AKM-070 (real-sampler guard
  for `&01`)

### RQ-AKM-061: Disk selection and status
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller selects a disk by handle (`&02`), tests whether the currently selected
  disk is valid (`&03`, completing on DONE or ERROR per spec footnote `#FN 20 a`, never a REPLY), or
  reads the current disk's type (`&06`), a specified disk's type (`&07`), the current disk's handle
  (`&08`) or the current disk's path (`&09`, a string, empty at the root folder), the AKM layer SHALL
  send the matching item and decode the REPLY (where one exists) into a typed value.
- **Rationale**: disk handles are 14-bit (`<Data2> + 128×<Data1>`), the same composite-index pattern
  already used for program and sample selection by index (`RQ-AKM-022`, `RQ-AKM-045`); reused here,
  not reinvented.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler with a valid disk at handle `3`,
  *When* it is selected then tested, *Then* selection succeeds and the test completes DONE. *Given*
  the disk is then made invalid (ejected in the simulation), *When* tested again, *Then* it completes
  ERROR, not a REPLY. *Given* the current disk's type, specified-disk type, handle and path, *When*
  each is read, *Then* they decode to the simulated sampler's own values, the path being empty at the
  root folder.
- **Dependencies**: RQ-AKM-002; RQ-AKM-022 (composite index pattern)

### RQ-AKM-062: Disk format, free space and name
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller reads the current disk's format (`&0A`: other, MSDOS, FAT32, ISO9660,
  S1000, S3000, EMU or ROLAND), its free space in bytes (`&0B`, an 8-byte Compound Quad Word) or the
  name of a specified disk (`&0E`, a string), the AKM layer SHALL send the matching item and decode
  the REPLY into a typed value, the free space as a 64-bit unsigned value.
- **Rationale**: `&0B` is the catalogue's first item whose REPLY is a `qword`, not yet an accepted
  item-catalogue value format (see "New value format" above); introducing it here rather than
  stretching `Dword` the way `TASK-AKM-049` had to question for §02 keeps the codec's own type
  (`std::uint64_t`) all the way to the decoded value.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated disk formatted FAT32 with `4 294 967 296`
  bytes free, *When* its format and free space are read, *Then* they decode to `FAT32` and
  `4 294 967 296`. *Given* a specified disk's name, *When* read, *Then* it decodes as a string the
  same way a sampler or program name does.
- **Dependencies**: RQ-AKM-002; RQ-AKM-061

### RQ-AKM-063: Folder navigation, listing and management
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller reads the number of sub-folders in the current folder (`&10`), the name
  of a specified sub-folder (`&11`), the names of every sub-folder (`&12`), opens a named sub-folder
  or the root folder (`&13`, `<Data1> = 0` selects the root), closes the current folder (`&14`,
  completing ERROR if it is already the root), creates a sub-folder (`&16`) or renames one (`&18`,
  carrying the existing name then the new name as two consecutive null-terminated strings), the AKM
  layer SHALL send the matching item and decode the REPLY (where one exists) into a typed value or
  list.
- **Rationale**: the folder hierarchy is sampler-wide state, like the current disk; `&18`'s two
  strings in one item are new to the catalogue (every renaming item so far, `RQ-AKM-052`, carries
  exactly one), so the item-catalogue schema decision for a second string argument is settled by
  whichever task implements it, the same deferral this feature already makes for `qword`
  (`RQ-AKM-062`).
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated folder with two sub-folders, *When* their
  count, one name and all names are read, *Then* they decode to `2`, the requested name and both
  names in order. *Given* the root folder, *When* closed, *Then* it completes ERROR. *Given* a
  sub-folder opened then a new one created and renamed, *When* its name is read back, *Then* it is
  the new name.
- **Dependencies**: RQ-AKM-002; RQ-AKM-052 (string value format); RQ-AKM-069 (guard for `&17`, not
  part of this requirement)

### RQ-AKM-064: Load Folder
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller loads a named folder and everything it contains, including
  sub-folders, into memory (`&15`), the AKM layer SHALL send the item and complete on DONE or ERROR,
  SHALL NOT execute it against the real sampler except through the guard of `RQ-AKM-070`, and SHALL
  document that it is one of the operations the spec itself calls potentially long-running.
- **Rationale**: the broadest single load operation in the section (an entire folder tree at once);
  grouped apart from the other load items because its blast radius is the largest.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated folder containing a program and a sample in
  a sub-folder, *When* loaded, *Then* the simulated sampler's memory gains both, and the command
  completes DONE. *Given* a name that does not exist, *When* loaded, *Then* it completes ERROR.
- **Dependencies**: RQ-AKM-002; RQ-AKM-070

### RQ-AKM-065: File listing, info and rename
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller reads the number of files in the current folder (`&20`), the name of a
  specified file (`&21`), the names of every file (`&22`), a specified file's size in bytes (`&23`,
  a Compound Double Word), the index of a named file (`&24`), or renames a file (`&28`, carrying the
  existing name then the new name, its extension never repeated in the new name per spec footnote
  `#FN 20 c`), the AKM layer SHALL send the matching item and decode the REPLY (where one exists)
  into a typed value or list.
- **Rationale**: mirrors `RQ-AKM-063`'s folder-listing shape one level down, for files.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated folder with two files, *When* their count,
  one name, all names, one size and one index-by-name are read, *Then* they decode correctly,
  including a size of `0` for an empty file. *Given* a file renamed with an extension accidentally
  included in the new name, *When* sent, *Then* the primitive still sends exactly what it was given —
  stripping it is a caller concern, not this layer's, and is noted as such.
- **Dependencies**: RQ-AKM-002; RQ-AKM-052 (string value format); RQ-AKM-069 (guard for `&29`, not
  part of this requirement)

### RQ-AKM-066: Load File, with and without dependent children
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller loads a named file (`&2A`, its extension deciding whether it is a
  multi, program, sample or scenelist, with an option for samples: normal, as RAM or as VIRTUAL) or
  loads a named file together with every file it depends on (`&2B`), the AKM layer SHALL send the
  matching item and complete on DONE or ERROR, and SHALL NOT execute either against the real sampler
  except through the guard of `RQ-AKM-070`.
- **Rationale**: spec footnotes `#FN 20 d`/`#FN 20 e` distinguish the two explicitly: `&2A` alone
  never follows a dependency (e.g. a program's samples); `&2B` does.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated program file whose sample is a separate
  file, *When* loaded with `&2A`, *Then* only the program appears in memory. *Given* the same file
  loaded with `&2B`, *Then* both the program and its sample appear. *Given* a sample load option of
  VIRTUAL, *When* sent, *Then* the option byte is `2`.
- **Dependencies**: RQ-AKM-002; RQ-AKM-070

### RQ-AKM-067: Save Memory Item(s) to disk
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller saves one memory item by index (`&2C`: type multi/program/sample/
  SMF/setlist/scenelist, overwrite flag, save-children flag) or every memory item at once (`&2D`:
  type, overwrite flag, save-children flag), the AKM layer SHALL send the matching item and complete
  on DONE or ERROR, SHALL NOT execute either against the real sampler except through the guard of
  `RQ-AKM-070`, and SHALL require the caller to pass the overwrite flag explicitly rather than
  defaulting it to `1` (overwrite).
- **Rationale**: the only items in this section that write sampler-owned files rather than only
  reading them or loading into memory; defaulting overwrite to `0` (skip if the file exists) is the
  safer failure mode for anything this feature cannot undo.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated disk with an existing file at the target
  name, *When* saved with overwrite `0`, *Then* the file is unchanged and the command completes
  ERROR; *When* saved with overwrite `1`, *Then* the file is replaced and it completes DONE. *Given*
  a caller that does not pass the overwrite flag, *When* compiled, *Then* there is no default
  argument supplying it silently.
- **Dependencies**: RQ-AKM-002; RQ-AKM-070

### RQ-AKM-068: Sample audition from disk
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller starts auditioning a sample file by index without loading it into
  memory (`&30`) or stops the current audition (`&31`), the AKM layer SHALL send the matching item
  and complete on DONE or ERROR.
- **Rationale**: the only two items in this section that are neither informational nor a lasting
  change to memory or disk; grouped on their own for that reason.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* a simulated file at index `0`, *When* audition is
  started then stopped, *Then* both complete DONE. *Given* audition stopped when none is playing,
  *When* sent, *Then* the simulated sampler's own behaviour decides DONE or ERROR, unconstrained by
  this requirement.
- **Dependencies**: RQ-AKM-002

### RQ-AKM-069: Destructive command guards for Eject, Delete Sub-Folder and Delete File
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a caller requests Eject Disk with the discard-virtual-samples option (`&0D`,
  `<Data3> = 1`), Delete Sub-Folder (`&17`) or Delete File (`&29`), THEN the AKM layer SHALL send it
  only when the caller passes an explicit confirmation argument that no default supplies, and no
  real-sampler test of any feature SHALL call any of the three with that confirmation.
- **Rationale**: mirrors `RQ-AKM-025`, `RQ-AKM-046` and `RQ-AKM-056`, extended to the three ways this
  section can discard data the AKM layer cannot restore (a loaded virtual sample, a folder's
  contents, a file). Eject with `<Data3> = 0` (fail if the drive is in use) needs no guard — it
  already refuses to discard anything.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a request for `&0D` with discard, `&17` or `&29` without
  the confirmation argument, *When* made, *Then* nothing is sent and an error explains why. *Given*
  the source of the real-sampler tests, *When* searched for any of the three primitives with
  confirmation given, *Then* there is no call. *Given* `&0D` with `<Data3> = 0`, *When* requested,
  *Then* it is sent without needing confirmation.
- **Dependencies**: RQ-AKM-025; RQ-AKM-046; RQ-AKM-056

### RQ-AKM-070: Slow-operation handling for long-running §10 commands
- **Category**: Non-Functional
- **NFR Type**: Reliability
- **EARS Type**: Unwanted-behavior
- **Statement**: WHILE a command among `&01`, `&15`, `&2A`, `&2B`, `&2C` or `&2D` is pending, the AKM
  layer SHALL apply the session's normal timeout and Still Alive handling (`RQ-AKM-009`,
  `RQ-AKM-011`) with no special case in its own logic; IF any of the six is requested against the
  real sampler (not the simulated one), THEN the request SHALL be refused unless the caller passes an
  explicit, non-default guard argument, and the guard's own documentation SHALL cite the hang
  observed on the owner's S5000 (`process/2.architecture/OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`,
  F4–F7) so a future caller does not re-discover it by losing a session the same way.
- **Rationale**: the spec's own footnote claims Still Alive keeps the host from timing out during a
  long disk operation (`#FN 5 b`); the owner's observation contradicts that claim for at least one of
  these six items on at least one sampler (no `F0 F7` arrived, and the sampler stopped answering any
  SysEx at all). The AKM layer cannot tell in advance which of the six would do the same, so the guard
  covers all six rather than only `&01`.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* any of the six primitives invoked against a simulated
  sampler, *When* called without any extra argument, *Then* it is sent exactly like any other item.
  *Given* any of the six invoked against the real-sampler test harness without the guard argument,
  *When* called, *Then* nothing is sent and the refusal names the primitive and points to the
  observation above. *Given* the guard argument supplied, *When* the command then hangs as observed,
  *Then* the harness reports the timeout and does not claim the sampler was left in a known state
  (mirrors `RQ-AKM-018`'s honesty requirement for what it could not confirm).
- **Dependencies**: RQ-AKM-009; RQ-AKM-011; RQ-AKM-018; RQ-AKM-060; RQ-AKM-064; RQ-AKM-066; RQ-AKM-067

### RQ-AKM-071: Real-sampler tests use a dedicated, disposable test folder and file
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: The real-sampler tests of this feature SHALL create their own sub-folder under the
  currently open folder before changing anything else, SHALL perform every create, rename, load and
  save they exercise inside that sub-folder only, SHALL delete it (through the guard of `RQ-AKM-069`)
  when the suite ends successfully, SHALL NEVER select a different disk, rename or delete anything
  that existed before the suite ran, and SHALL NOT exercise `&01`, `&15`, `&2A`, `&2B`, `&2C` or
  `&2D` at all unless the owner passes the guard of `RQ-AKM-070` in addition to this feature's own
  real-sampler flag.
- **Rationale**: mirrors `RQ-AKM-027`, `RQ-AKM-033`, `RQ-AKM-038` and `RQ-AKM-051`'s "dedicated test
  X" pattern, extended here to disk/folder/file because nothing in this section is reproducible from
  the software if damaged — there is no "restore" for a deleted file the way `RQ-AKM-058` restores a
  clock or a lock.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a real-sampler run of this feature's suite, *When* it
  ends successfully, *Then* the disposable sub-folder it created no longer exists and nothing else on
  the disk changed. *Given* the suite run without the slow-operation guard, *When* it reaches a step
  that would need `&01`, `&15`, `&2A`, `&2B`, `&2C` or `&2D`, *Then* that step is skipped and reported
  as such, not silently passed.
- **Dependencies**: RQ-AKM-018; RQ-AKM-027; RQ-AKM-069; RQ-AKM-070

### RQ-AKM-072: Coverage of section §10 and errata resolution
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Every §10 row of `documents/_index/sysex_spec.items.tsv` SHALL be either covered by
  a primitive or listed with a reason for its exclusion, and the `&0D`/`&0E` decimal-column erratum
  noted above SHALL be resolved by inspection and recorded, the same way the §08 `&6C` erratum was.
- **Rationale**: 51 rows, the largest single-section lot so far; the errata list already carries one
  §10 entry.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the 35 command rows and 16 REPLY rows of section `10`,
  *When* compared with the catalogue of primitives and exclusions, *Then* no row is unaccounted for.
  *Given* the `&0D`/`&0E` decimal erratum, *When* the lot ends, *Then* it is recorded in
  `KNOWN_DEC_ERRATA` and in `sysex_spec.kb.md`'s errata list.
- **Dependencies**: RQ-AKM-060 to RQ-AKM-071

---

## Open points

- **`qword` as an item-catalogue value format.** The codec already has `appendQword`/`readQword`
  (`RQ-AKM-002`); `generate_akm_items.py` still refuses `format: "qword"` (deferred, like `string`
  was before `DEC-AKM-013`). Settled by the task implementing `RQ-AKM-062`.
- **A second string argument in one item.** `&18` (Rename Folder) and `&28` (Rename File) carry two
  consecutive null-terminated strings; every string item so far (`RQ-AKM-052` and its precedents)
  carries exactly one. Settled by the task implementing `RQ-AKM-063`/`RQ-AKM-065`.
- **Disk-specific error numbers.** `sysex_spec.kb.md` lines 69-71 list `&101`–`&10E`-range codes
  specific to disk operations (e.g. "`10B` name not unique", "`10C` invalid disk handle"). The error
  decoding of `RQ-AKM-005` already reads a 16-bit error number (`Confirmation::errorNumber`), so no
  catalogue or codec change is needed; describing what a given number means is left to whatever layer
  eventually surfaces errors to a user, out of this feature's scope.
- **The maximum total wait of `DEC-AKM-006` stays unset.** The owner's one real observation of a
  slow-operation hang did not establish how long `&01` would eventually have taken, if ever
  (`OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`, "Not observed"). `RQ-AKM-070`'s guard does not
  depend on knowing that value; a future session may narrow it once more real-hardware runs exist.
