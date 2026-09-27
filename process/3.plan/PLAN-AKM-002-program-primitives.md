# PLAN-AKM-002: Program Primitives (Phase A, lot A2)

## Overview

Plan for FTR-AKM-002, section §0A (Program) of the S5000 control plan
(`process/1.requirements/DRAFT-s5000-midi-primitives-and-workflows.md`, lot A2): 95 commands and 46
REPLY formats — program lifecycle, structure/identity, general information (current program and all
programs in memory), and the five Set/Get parameter groups (Output, MIDI/Tune, Pitch Bend, LFOs,
Keygroup Modulation Sources) — each proven by a Set followed by a Get on the mock, then on the real
sampler with the dedicated test program of RQ-AKM-027.

FTR-AKM-002's outline deferred two things to when this lot starts: refining the per-primitive value
ranges against `documents/_index/sysex_spec.items.tsv`, and its "Open points" (primitive shape, Sync
LCD, program numbering). The per-primitive ranges are read from the TSV directly into each task's
acceptance criteria below rather than duplicated into the FTR file, following PLAN-AKM-001's practice
of verifying exact values at implementation time. The three open points: primitive shape was decided
by ADR-AKM-001 (no action here); Sync LCD is resolved in TASK-AKM-015 (the first program-selecting
primitive); program numbering is resolved in TASK-AKM-016, pinned down against the real sampler.

**A prerequisite the outline did not anticipate in full.** The catalogue (`ItemDescriptor.hpp`,
`generate_akm_items.py`) only knows the byte/word/dword/signed formats today; its own comments already
flag that strings are "added when the first item that needs one is catalogued" (DEC-AKM-003). §0A is
that first item: Create (`&02`/`&03`), Select by name (`&05`), Rename (`&09`) and Get Name (`&13`) all
carry an ASCII null-terminated name. `ByteWriter::appendString` and `ByteReader::readString` /
`readStringList` already exist at the codec level (added ahead of need) — only the catalogue's
`ValueFormat`, the generator script and `ItemRequest`'s generic encode/decode do not yet expose them.
TASK-AKM-014 closes that gap for a single string value. The all-programs replies (`&18`, `&19`) are a
second, separate gap — a REPLY that repeats a fixed-width record (or a string) an a priori unknown
number of times — which is new to the catalogue's one-value-per-`ValueSpec` decode contract and also
recurs in FTR-AKM-003 (RQ-AKM-031) and FTR-AKM-004 (RQ-AKM-036); TASK-AKM-017 designs it once, for all
three features to cite. Every other group (Output, MIDI/Tune, Pitch Bend, LFOs, Keygroup Modulation
Sources) fits the existing byte/signed-byte model without a new format: the "which LFO / which Amp Mod
/ which Pan Mod" selector Data1 bytes are ordinary range-constrained arguments, and the User Tune
Template (`&33`/`&3B`, 12 chained ±values) is 12 named `ValueSpec` entries, not a new shape.

## References
- **Requirements**: FTR-AKM-002 (RQ-AKM-021 to RQ-AKM-027)
- **ADRs**: ADR-AKM-001 (Accepted) — TASK-AKM-014 and TASK-AKM-017 each add a decision to it (the next
  free `DEC-AKM-*` ids per `process/INDEX.idx.md` are used in order); no new ADR file is opened, since
  both extend the existing catalogue/codec architecture rather than introduce a new one.

The plan has 12 tasks (TASK-AKM-014 to TASK-AKM-025), more than one session should carry
(context-management rule of at most 10 per session): a natural split is TASK-AKM-014, 015, 016
(catalogue prerequisite, lifecycle, structure/identity — unlocks everything else); TASK-AKM-017
(general information, including the shared list-reply design); TASK-AKM-018 to 022 (the five parameter
groups, independent of each other once 015 is done — could run across two sessions, e.g. 018-019-020
then 021-022, LFOs being the largest single group at 32 commands); TASK-AKM-023 (destructive guard);
TASK-AKM-024 (real-sampler test harness, RQ-AKM-027); TASK-AKM-025 (coverage reconciliation, RQ-AKM-026
— its errata resolution needs the owner's real-sampler run, like RQ-AKM-017/018 before it).

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-AKM-014: Item catalogue support for a single string value
- **Tier**: L
- **Status**: Done
- **Description**: Add `ValueFormat::String` to `ItemDescriptor.hpp` and the generator script's format
  table and schema validation, and add dedicated `makeStringRequest` / `decodeStringReply` functions
  (`ItemRequest.hpp`/`.cpp`) built on the codec's existing `ByteWriter::appendString` /
  `ByteReader::readString` — kept separate from the generic `makeRequest`/`decodeReply`, which stay
  `int64_t`-only, so no existing call site or numeric item is touched. A string argument or reply has
  no fixed width, so `fixedReplyLength()` needed a rule for it (a REPLY carrying one is refused as
  `ChecksumModeUnknown` while the port's mode is unknown, exactly as an uncatalogued REPLY already is —
  no codec change needed). Recorded as DEC-AKM-013 in ADR-AKM-001.
- **Requirement refs**: RQ-AKM-002
- **ADR refs**: ADR-AKM-001 (new decision DEC-AKM-013, this task)
- **Acceptance Criteria** (Gherkin): *Given* an item descriptor with one `String` argument, *When*
  `makeStringRequest` is called with an ASCII name inside its declared character-count range, *Then*
  the encoded frame carries the name followed by a single `00`. *Given* a name outside that range, or
  containing a non-ASCII or `00` character, *When* requested, *Then* it is refused without sending
  (`ArgumentOutOfRange`, respectively `NotEncodable`). *Given* an item without exactly one `String`
  argument or reply, *When* `makeStringRequest` / `decodeStringReply` is called on it, *Then* it is
  refused / returns nothing. *Given* a `String` reply, *When* `decodeStringReply` reads a REPLY ending
  in a null-terminated name, *Then* it returns the name as text; *given* data that is not exactly one
  such string, *then* nothing is returned. *Given* the generator's `--check` and `--coverage`, *When*
  run on a data file using `"string"` as a format, *Then* they accept it like any other format.
- **Dependencies**: None
- **Assignee**: AI, with the owner's approval (DoR)
- **Verification**: Windows/MSVC (Visual Studio 18 2026), Debug, re-run this session: `cmake --build
  juce/build --config Debug` warning-free (`-W4 /WX`), `ctest --test-dir juce/build -C Debug` 359/359
  (9 new Catch2 cases in `ItemCatalogueTests.cpp`, one per Gherkin criterion above, plus the String
  width and `fixedReplyLength` facts). One pre-existing Python test broke and was fixed, not worked
  around: `test_given_a_format_the_schema_does_not_support_when_read_then_it_is_refused_naming_the_record`
  used `"string"` itself as its example of an unsupported format; since this task makes it supported,
  the example was changed to `"qword"` (still deferred, per DEC-AKM-003), which correctly reflects the
  new behaviour rather than forcing the old assertion to pass. `akm_item_table_is_up_to_date` and
  `akm_item_catalogue_matches_the_spec` stayed green unchanged, since no real item was catalogued as
  `items.json` is untouched by this task (the mechanism only; TASK-AKM-015 is its first real consumer).
  Not verified: mutation testing of the new guards (`makeStringRequest`'s length check,
  `fixedReplyLength`'s String early-return, `singleStringSpec`'s format check) — a deliberate,
  temporary, revert-planned mutation edit was blocked by the session's auto-mode security classifier
  ("Security Test Removal"); per its instructions the same outcome was not pursued through another
  tool, so test sensitivity for these three guards rests on inspection (each guarded branch is exercised
  by name in the 9 new tests) rather than an executed red/green cycle. Not verified either: the real
  S5000 (the 20-character Program-name bound is the owner's own observation, 2026-09-27, recorded in
  the KB; other name fields are unconfirmed).
- **Assumptions**: The spec gives no maximum length or extra character-set rule for any name field
  (checked p. 8-9's string-format prose and the Disk/File/Folder section, `documents/_index/sysex_spec.kb.md`
  "Common value codes"); the schema's own `STRING_MAX_LENGTH` (255) is a generous structural ceiling,
  not a spec or hardware limit, so a future name field with a different real bound needs no schema
  change, only its own item range. The 20-character Program-name bound came from the owner testing the
  real sampler mid-task (2026-09-27), not from the spec, and is recorded in the KB rather than
  `FTR-AKM-002` itself, which states no requirement about it. `RefusalReason` gained no new enumerator:
  `NotEncodable` (invalid text) and `ArgumentOutOfRange` (wrong length) already fit, reused rather than
  added to. `ValueSpec.min`/`max` are reinterpreted as a character count for `String` — a comment-level
  distinction, not a new field, to avoid widening the struct for one format. Tests and code were written
  in close succession rather than strictly tests-then-code (the design changed mid-task on the owner's
  real-hardware input); no red run was captured before the code existed, unlike some acceptance
  criteria of this lot's sibling tasks in FTR-AKM-001's plan, which did capture one — disclosed here
  rather than left implicit. The two-item list-reply design that FTR-AKM-002's general program
  information (`&18`/`&19`) will need is explicitly out of this task's scope (see DEC-AKM-013's last
  point) and belongs to TASK-AKM-017.

---

### TASK-AKM-015: Program lifecycle primitives
- **Tier**: M
- **Status**: Done
- **Description**: Implement Create Program (`&02`, name only), Create Program with keygroups (`&03`,
  1–99 keygroups + name), Select by name (`&05`), Select by index (`&06`, zero-based word), Delete the
  current program (`&08`) and Rename the current program (`&09`), each completing on DONE, with ERROR
  `04` (not found) and `05` (could not create) reported unchanged. Resolve the Sync LCD open point of
  FTR-AKM-002 here: whether a program-selecting primitive switches it off before acting, given the
  session already defaults it off (ADR-AKM-001, DEC-AKM-007) and the spec's own recommendation to
  leave it off except when needed.
- **Requirement refs**: RQ-AKM-021
- **ADR refs**: ADR-AKM-001 (DEC-AKM-007, DEC-AKM-011)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-021 — *Given* a simulated sampler,
  *When* a program named `TESTPRG` is created then selected by name, *Then* the frames carry the ASCII
  name null-terminated and the commands complete on DONE. *Given* a name that does not exist, *When*
  it is selected, *Then* ERROR `04` is reported. *Given* the real sampler and the test program of
  RQ-AKM-027, *When* create, select by index, rename and delete are run, *Then* each is followed by a
  Get (`&13` name, `&10` count) confirming its effect.
- **Dependencies**: TASK-AKM-014
- **Assignee**: AI, with the owner's approval (DoR)
- **Verification**: Windows/MSVC Debug: 0 warnings, `ctest` 369/369 (10 new in `ProgramPrimitivesTests.cpp`;
  2 pre-existing `ItemCatalogueTests.cpp` assertions corrected for the 8 new catalogue entries).
  `--coverage`: section 0A 8/95 rows, no problems. Not verified: real sampler (TASK-AKM-024); mutation
  testing (blocked by the session's security classifier, as in TASK-AKM-014).
- **Assumptions**: Plain Create defaults to 1 keygroup; a duplicate name on Create is ERROR `05` (spec
  silent on both). `&06`/`&10` use two Byte values (msb/lsb), not Word, matching `SystemOsVersion`'s
  precedent, so `--coverage` stays comparable; combined by hand in the typed helpers. `&03`'s mixed
  byte+string args bypass `makeRequest`/`makeStringRequest` (hand-built with `ByteWriter`), reusing the
  catalogue's own ranges. `getCurrentProgramName` sets `ExpectedReply::NeedsKnownChecksumMode` (a String
  reply has no fixed length). The mock's §0A handlers ignore a trailing byte after their data (a
  checksum the session appends whenever its mode is not known to be Off), matching §00's own convention.
  Program numbering (FTR-AKM-002's other open point) is not resolved here; still TASK-AKM-016's.

---

### TASK-AKM-016: Program structure and identity primitives
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement Set "Program Number" (`&0A`, an OFF/ON byte plus, only when ON, a
  0–127 number), Add Keygroups (`&0B`, 1–98), Delete Keygroup (`&0C`, zero-based 0–98) and Set/Get
  Keygroup Crossfade (`&0D`/`&15`), refusing an out-of-range value without sending. Resolve the
  "program numbering across the spec" open point of FTR-AKM-002 (front-panel 1–128 sent as 0–127,
  prefixed by the OFF/ON byte) against the real sampler.
- **Requirement refs**: RQ-AKM-022
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-011)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-022 — *Given* a program with 1
  keygroup, *When* 3 are added, *Then* Get number of keygroups (`&14`) reports 4. *Given* the value 0
  for keygroups to add, *When* requested, *Then* it is refused without sending. *Given* crossfade set
  to ON, *When* Get crossfade (`&15`) runs, *Then* it reports ON. *Given* the real sampler, *When* the
  Program Number is set ON with a front-panel value, *Then* Get Program Number reports the value this
  task decided it should (0-based or 1-based), recorded as this task's resolution of the open point.
- **Dependencies**: TASK-AKM-015
- **Assignee**: AI, with the owner's approval (DoR)
- **Verification**: N/A — not started
- **Assumptions**: None yet

---

### TASK-AKM-017: General program information, including all programs in memory
- **Tier**: L
- **Status**: Not Started
- **Description**: Implement Get Number of Programs (`&10`), Get Program Number (`&11`), Get Program
  Index (`&12`), Get Program Name (`&13`), Get Number of Keygroups (`&14`) and reuse Get Crossfade
  (`&15`) for the current program; Get the Program Numbers of all programs (`&18`) and Get the names
  of all programs (`&19`) for every program in memory. `&18`/`&19` are this lot's first REPLY that
  repeats a record (an on/off-plus-number pair, or a name) an a priori unknown number of times — decode
  it locally (bespoke, like the OS-version primitive of TASK-AKM-008, not through the single-value
  generic `decodeReply`), and record the approach as a new `DEC-AKM-*` in ADR-AKM-001 so FTR-AKM-003
  (RQ-AKM-031) and FTR-AKM-004 (RQ-AKM-036) can cite it instead of re-deciding it.
- **Requirement refs**: RQ-AKM-023, RQ-AKM-002
- **ADR refs**: ADR-AKM-001 (new decision, this task; DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-023 — *Given* a simulated sampler
  holding programs `A`, `B`, `C`, *When* the names of all programs are read, *Then* the result is the
  ordered list `A`, `B`, `C`. *Given* the current program, *When* its index, name and keygroup count
  are read, *Then* they equal what was created in RQ-AKM-021 and RQ-AKM-022. *Given* zero programs in
  memory (if reachable) or exactly one, *When* the all-programs Gets run, *Then* the list decodes
  correctly at that boundary too.
- **Dependencies**: TASK-AKM-014, TASK-AKM-015, TASK-AKM-016
- **Assignee**: AI, with the owner's approval (DoR)
- **Verification**: N/A — not started
- **Assumptions**: None yet

---

### TASK-AKM-018: Output parameter group (Set and Get)
- **Tier**: M
- **Status**: Done
- **Description**: Implement Set/Get Loudness (`&20`/`&28`), Velocity Sensitivity (`&21`/`&29`), Amp
  Mod Source/Value for Amp Mod 1 and 2 (`&22`/`&2A`, `&23`/`&2B`) and Pan Mod Source/Value for Pan Mod
  1, 2 and 3 (`&24`/`&2C`, `&25`/`&2D`) — 6 Set items, 6 Get items, 6 REPLY formats — each with a Set
  test followed by a Get test verified by read-back. Catalogued only (no per-item typed helper): every
  value fits the existing byte/no-selector-or-selector shape, so the generic `makeRequest`/`decodeReply`
  already serves them (DEC-AKM-003). Extends the simulated sampler (DEC-AKM-008) with a single generic
  §0A parameter-group mechanism (byte-width-driven from the catalogue itself) that also serves
  TASK-AKM-019 to 022 unchanged.
- **Requirement refs**: RQ-AKM-024
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-008, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-024 — *Given* each Set item of this
  group, *When* a value inside its range is set then read back on the simulated sampler, *Then* the
  value read equals the value set. *Given* a value outside the range (e.g. Amp Mod Source above 14, or
  Pan Mod index above 3), *When* set, *Then* it is refused without sending. *Given* the real sampler
  and the test program of RQ-AKM-027, *When* the same values are set and read back, *Then* the test
  passes unchanged.
- **Dependencies**: TASK-AKM-015
- **Assignee**: AI, with the owner's approval (DoR)
- **Verification**: Windows/MSVC Debug: 0 warnings, `ctest` 371/371 (2 new in `OutputParametersTests.cpp`:
  all 6 items round-tripped in one table-driven test, plus a range-refusal spot check). `--coverage`
  clean (also fixed a pre-existing regex bug it exposed: `spec_domains` required `=` after `<DataN>`,
  but &23/&25's own column has none). Not verified: real sampler (TASK-AKM-024); mutation testing
  (blocked by the session's security classifier, as in TASK-AKM-014/015).
- **Assumptions**: Every item of this group fits `byte`, one value per spec column (no `SignedByte`/
  `Word`: they would total fewer values than the spec's own columns, breaking `--coverage`'s comparison,
  as `&06`/`&10` already showed in TASK-AKM-015). `items.json` was authored for all five parameter
  groups (76 items) in this task's own commit, one data-file edit being far cheaper to review and
  coverage-check than five; TASK-AKM-019 to 022 add only their own round-trip test file, no further
  catalogue or mock change. The generic mock mechanism (`ProgramRecord::parameters`,
  keyed by (Set item, selector bytes)) reads each pair's widths from the catalogue itself, so it needs no
  per-item code; an item outside its five known ranges still answers `NOT_SUPPORTED` regardless of
  whether a program is current (a pre-existing test assumed exactly this for section 0A generally, which
  caught an ordering bug in an earlier version of this change).

---

### TASK-AKM-019: MIDI/Tune parameter group (Set and Get)
- **Tier**: M
- **Status**: Done
- **Description**: Implement Set/Get Semitone Tune (`&30`/`&38`), Fine Tune (`&31`/`&39`), Tune
  Template (`&32`/`&3A`), User Tune Template (`&33`/`&3B`, 12 chained ± values, one per semitone
  starting at C) and Key (`&34`/`&3C`) — 5 Set items, 5 Get items, 5 REPLY formats.
- **Requirement refs**: RQ-AKM-024
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-024, as TASK-AKM-018, applied to
  this group's 5 items — in particular the User Tune Template's 12 values are set and read back in
  order (C first), and a 13th value or one outside ±50 is refused without sending.
- **Dependencies**: TASK-AKM-015
- **Assignee**: AI, with the owner's approval (DoR)
- **Verification**: Windows/MSVC Debug: 0 warnings, `ctest` 377/377 (`MidiTuneParametersTests.cpp`, 1
  table-driven test, all 5 items incl. the 24-value User Tune Template). Wrong-count/out-of-range
  refusal not re-tested per item: already generic (`ItemCatalogueTests.cpp`). Not verified: real sampler
  (TASK-AKM-024); mutation testing (blocked, as in prior tasks).
- **Assumptions**: `items.json` for this group was authored in TASK-AKM-018's commit; this task adds
  only its test file.

---

### TASK-AKM-020: Pitch Bend parameter group (Set and Get)
- **Tier**: M
- **Status**: Done
- **Description**: Implement Set/Get Pitch Bend Up (`&40`/`&48`), Pitch Bend Down (`&41`/`&49`), Bend
  Mode (`&42`/`&4A`), Aftertouch Value (`&43`/`&4B`), Legato (`&44`/`&4C`), Portamento Enable
  (`&45`/`&4D`), Portamento Mode (`&46`/`&4E`) and Portamento Time (`&47`/`&4F`) — 8 Set items, 8 Get
  items, 8 REPLY formats.
- **Requirement refs**: RQ-AKM-024
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-024, as TASK-AKM-018, applied to
  this group's 8 items.
- **Dependencies**: TASK-AKM-015
- **Assignee**: AI, with the owner's approval (DoR)
- **Verification**: Windows/MSVC Debug: 0 warnings, `ctest` 377/377 (`PitchBendParametersTests.cpp`, 1
  table-driven test, all 8 items). Not verified: real sampler (TASK-AKM-024); mutation testing (blocked,
  as in prior tasks).
- **Assumptions**: `items.json` for this group was authored in TASK-AKM-018's commit; this task adds
  only its test file.

---

### TASK-AKM-021: LFO parameter groups (Set and Get, LFO 1 and 2)
- **Tier**: M
- **Status**: Done
- **Description**: Implement Set/Get Rate, Delay, Depth, Waveform, Sync (LFO1 only)/Re-trigger (LFO2
  only), Rate/Delay/Depth Mod Source and Value, Modwheel (LFO1 only), Aftertouch (LFO1 only) and MIDI
  Clock Sync Enable/Division (LFO2 only) (`&50`–`&5F` Set, `&60`–`&6F` Get) — `<Data1>` selects LFO 1
  or 2 as an ordinary range-constrained argument, not a new shape; the LFO1-only/LFO2-only items are
  still single items, just refused for the other LFO's index. 16 Set items, 16 Get items, 16 REPLY
  formats — the largest single group in this lot.
- **Requirement refs**: RQ-AKM-024
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-024, as TASK-AKM-018, applied to
  this group's 16 items, plus: *Given* LFO Sync (LFO1-only) or Re-trigger (LFO2-only), *When* requested
  for the other LFO's index, *Then* it is refused without sending.
- **Dependencies**: TASK-AKM-015
- **Assignee**: AI, with the owner's approval (DoR)
- **Verification**: Windows/MSVC Debug: 0 warnings, `ctest` 377/377 (`LfoParametersTests.cpp`, 1
  table-driven test round-tripping all 32 items across both LFOs where both apply, plus the dedicated
  Sync/Re-trigger cross-LFO refusal test). Not verified: real sampler (TASK-AKM-024); mutation testing
  (blocked, as in prior tasks).
- **Assumptions**: `items.json` for this group was authored in TASK-AKM-018's commit; this task adds
  only its test file. LFO1-only/LFO2-only items use a degenerate `min==max` selector range, refused
  generically like any other out-of-range argument — no special-case code.

---

### TASK-AKM-022: Keygroup Modulation Sources parameter group (Set and Get)
- **Tier**: S
- **Status**: Done
- **Description**: Implement Set/Get Pitch Mod Source (Pitch Mod 1 or 2, `&70`/`&74`), Amp Mod Source
  (Amp Mod 1 only, `&71`/`&75`) and Filter Mod Input Source (Mod Input 1, 2 or 3, `&72`/`&76`) — 3 Set
  items, 3 Get items, 3 REPLY formats, each a modulation-source value 0–14 (Table 15).
- **Requirement refs**: RQ-AKM-024
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-024, as TASK-AKM-018, applied to
  this group's 3 items; a modulation source above 14 is refused without sending.
- **Dependencies**: TASK-AKM-015
- **Assignee**: AI, with the owner's approval (DoR)
- **Verification**: Windows/MSVC Debug: 0 warnings, `ctest` 377/377 (`KeygroupModSourcesParametersTests.cpp`,
  2 tests: round trip of all 3 items, and the out-of-range refusal). Not verified: real sampler
  (TASK-AKM-024); mutation testing (blocked, as in prior tasks).
- **Assumptions**: `items.json` for this group was authored in TASK-AKM-018's commit; this task adds
  only its test file. This closes the 5 parameter-group tasks (018-022): all 76 items of RQ-AKM-024 are
  now catalogued and round-trip-proved on the simulated sampler.

---

### TASK-AKM-023: Destructive command guard for "Delete ALL programs"
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement `&07` (Delete ALL programs from memory) so that it is sent only when the
  caller passes an explicit confirmation argument that no default supplies (e.g. a dedicated type, not
  a bare `bool`, so a call site cannot satisfy it by accident), and audit the real-sampler test sources
  to confirm none calls it.
- **Requirement refs**: RQ-AKM-025
- **ADR refs**: ADR-AKM-001 (DEC-AKM-011)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-025 — *Given* a request for `&07`
  without the confirmation argument, *When* made, *Then* nothing is sent and an error explains why.
  *Given* the source of the real-sampler tests, *When* searched for the `&07` primitive, *Then* there
  is no call.
- **Dependencies**: TASK-AKM-015
- **Assignee**: AI, with the owner's approval (DoR)
- **Verification**: N/A — not started
- **Assumptions**: None yet

---

### TASK-AKM-024: Real-sampler test harness — dedicated test program
- **Tier**: L
- **Status**: Not Started
- **Description**: Implement the RQ-AKM-027 guard for every real-sampler test of this feature: create,
  select, change and delete only a program under a reserved test name; delete it before returning even
  when a test fails or is interrupted (an RAII-style guard, mirroring how RQ-AKM-018's closing put
  section 00 settings back); restore the sampler's original current-program selection; and refuse
  before sending if a test would act on a program it did not create. Extend `xs56k_akm_probe --suite`
  with the Program-lot checks this unlocks (mirroring TASK-AKM-010's structure), for the owner to run.
- **Requirement refs**: RQ-AKM-027
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-027 — *Given* a real sampler
  holding programs `KEEP1` and `KEEP2` and `KEEP1` selected, *When* the suite runs and one test fails
  halfway, *Then* afterwards the sampler holds exactly `KEEP1` and `KEEP2`, both unchanged, and `KEEP1`
  is current. *Given* a test that would act on a program not created by the suite, *When* it runs,
  *Then* it is refused before sending.
- **Dependencies**: TASK-AKM-015, TASK-AKM-016, TASK-AKM-017
- **Assignee**: AI, with the owner running the real-sampler suite
- **Verification**: N/A — not started
- **Assumptions**: None yet

---

### TASK-AKM-025: Coverage of section §0A and errata resolution
- **Tier**: M
- **Status**: Not Started
- **Description**: Add the 95 command rows and 46 REPLY rows of section `0A` to `items.json` coverage
  (or list an explicit exclusion reason for any row this lot does not implement) and run
  `generate_akm_items.py --coverage` to confirm none is unaccounted for. Resolve the KB errata that
  touch §0A (`documents/_index/sysex_spec.kb.md`: `&2C`/`&2D` labelled "Amp Pan Source/Value", read as
  Pan Mod Source/Value) by observation on the real sampler, and record the resolution in the KB.
- **Requirement refs**: RQ-AKM-026
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-026 — *Given* the 95 command rows
  and 46 REPLY rows of section `0A`, *When* compared with the catalogue of primitives and exclusions,
  *Then* no row is unaccounted for. *Given* the `&2C`/`&2D` erratum, *When* observed on the real
  sampler, *Then* its resolution is recorded with the observation that supports it (owner-run, like
  RQ-AKM-017/018).
- **Dependencies**: TASK-AKM-018, TASK-AKM-019, TASK-AKM-020, TASK-AKM-021, TASK-AKM-022, TASK-AKM-023
- **Assignee**: AI, with the owner running the real-sampler observation
- **Verification**: N/A — not started
- **Assumptions**: None yet
