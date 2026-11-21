# PLAN-AKM-007: Disk Tools Primitives (Phase A, new lot, §10)

## Overview

Implements `FTR-AKM-007`: one tested primitive per command row of section `10` (35 commands, 16
REPLY formats, 51 rows — the largest single-section lot so far), two new item-catalogue value-format
decisions (`qword`, a second string argument in one item), destructive-command guards for Eject,
Delete Sub-Folder and Delete File, a dedicated real-sampler slow-operation guard for the six items the
spec itself calls potentially long-running, and the section's coverage/errata closure.

## References
- **Requirements**: RQ-AKM-060 to RQ-AKM-072 (`FTR-AKM-007`)
- **ADRs**: ADR-AKM-001 — amended in place as each new value-format decision is settled (`DEC-AKM-013`
  precedent for `string`), not a new ADR document.

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-AKM-056: Author FTR-AKM-007 and PLAN-AKM-007
- **Tier**: M
- **Status**: Done
- **Description**: Write the feature file for section `10` (Disk Tools) and this plan, grouping its
  51 spec rows into requirements and tasks, flagging the real-hardware slow-operation risk already
  observed for `&01` and the two new item-catalogue value-format needs (`qword`, a second string
  argument).
- **Requirement refs**: None (precedes the requirements it produces)
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* `documents/_index/sysex_spec.items.tsv` section `10`,
  *When* `FTR-AKM-007` is compared against it, *Then* every one of the 51 rows maps to exactly one RQ.
  *Given* the owner's confirmation on trigram and scope, *When* asked, *Then* it is recorded before
  drafting starts.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: `FTR-AKM-007-disk-tools.md` and this plan written; row-mapping cross-checked by
  hand against `sysex_spec.items.tsv` while drafting (35 commands + 16 REPLYs = 51, each assigned to
  exactly one of RQ-AKM-060 to RQ-AKM-071, confirmed in the RQ-by-RQ breakdown kept in session). Owner
  confirmed trigram `AKM` (continuing the existing SysEx-layer ID space, over a literal `ALM`) and
  confirmed including the known-risk items with a reinforced opt-in guard rather than deferring them.
- **Assumptions**: None.

---

### TASK-AKM-057: Disk discovery
- **Tier**: M
- **Status**: Done
- **Description**: Implement `&01` (Update List of Disks), `&04` (Get Number of Disks) and `&05` (Get
  List of All Connected Disks, decoding each entry's handle/type/format/SCSI ID/writable flag/name).
- **Requirement refs**: RQ-AKM-060
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-060, on the simulated sampler.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean rebuild, no warning or error (`/W4 /WX`); `ctest
  --test-dir juce/build -C Debug` 500/500 passed (re-run twice in this session). New
  `DiskPrimitivesTests.cpp` (6 cases, `[akm][disk]`, `ctest -R RQ-AKM-060`): no disks connected → count
  0, empty list (not refused); two simulated disks → count 2, both entries decode (handle, type,
  format, SCSI ID, writable, name) in order; a handle above 127 decodes correctly from its two data
  bytes; `&04`/`&05` still send and decode without `&01` having been sent first (the ordering the spec
  states is not enforced by this layer); `&01` completes DONE; `&05` is refused as
  `ChecksumModeUnknown` while the mode is unknown, nothing sent (mirrors `getAllProgramNames`,
  DEC-AKM-014). `generate_akm_items.py --coverage`: `DiskUpdateList`/`DiskGetCount`/`DiskGetList`
  covered (3 of 35 §10 command rows), `unaccounted: none` overall; `--check` up to date (263 items).
  Two pre-existing tests updated to reflect the catalogue's new, correct content rather than loosened:
  `ItemCatalogueTests.cpp`'s total-item-count check (`PROGRAM_ITEM_COUNT`, `+3`) and
  `RealSamplerSuiteTests.cpp`'s `--slow-operation` ERROR-path test, which relied on the simulated
  sampler having no §10 support at all — now forces `&01` to refuse via the existing `itemErrors`
  override (the same mechanism several other tests already use), so it still exercises the ERROR path
  it is named for, the DONE path being covered by `DiskPrimitivesTests.cpp` instead; re-run standalone
  (`ctest -R "sampler without a disk section"`) and through the full suite.
- **Assumptions**: `&05`'s REPLY is decoded with a custom `ByteReader` loop in `DiskPrimitives.cpp`,
  the same way `getAllProgramNumbers`/`getAllProgramNames` already are, rather than extending the
  generic `decodeRepeatedReply` to mixed fixed-field-plus-string records: no second item needs that
  shape yet, and the project avoids generalising for a single use (`AGENTS.md` anti-patterns). The
  simulated sampler does not model the spec's own precondition that `&04`/`&05` are unreliable before
  `&01` runs (Table 20, footnote b): `setDisks` always reflects what a test seeds, `&01` a no-op,
  since no acceptance criterion of `RQ-AKM-060` asks the model to simulate that staleness. A disk's
  name has no observed real-hardware bound, so its catalogue range is `0-255` (`STRING_MAX_LENGTH`),
  the same choice already made for Zone/Sample names with no observed bound.

### TASK-AKM-058: Disk selection and status
- **Tier**: M
- **Status**: Done
- **Description**: Implement `&02` (Select Disk), `&03` (Test Disk Valid), `&06`/`&07` (current/
  specified disk type), `&08` (index of current disk) and `&09` (current path).
- **Requirement refs**: RQ-AKM-061
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-061, on the simulated sampler.
- **Dependencies**: TASK-AKM-057
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean rebuild, no warning or error (`/W4 /WX`); `ctest
  --test-dir juce/build -C Debug` 504/504 passed. 4 new cases added to `DiskPrimitivesTests.cpp`
  (`ctest -R RQ-AKM-061`): a disk selected then tested completes Done both times; a handle naming no
  disk completes Error (not a Reply) for both Select and Test; the current type, the same disk's type
  by handle, its current handle and its current path all decode correctly once selected, the path
  empty at the root folder (no folder modelled yet, TASK-AKM-060); with nothing selected, the current
  type, handle and path each complete Error. `generate_akm_items.py --coverage`: the 6 new items
  covered (9 of 35 §10 rows now), `unaccounted: none`; `--check` up to date (269 items).
  `ItemCatalogueTests.cpp`'s total-item-count check updated (`PROGRAM_ITEM_COUNT`, `+6`).
- **Assumptions**: `&03` (Test Disk Valid) is modelled against the handle its own two data bytes name,
  not "the currently selected disk" as Table 20's footnote a reads in isolation — the item's own
  Data1/Data2 columns carry a handle exactly like `&02`'s, and the catalogue is built from an item's
  data columns over a footnote's looser wording (same precedence already used for every other erratum
  in this project). `&08` is named `getCurrentDiskHandle`, not `getCurrentDiskIndex`: its command title
  (Table 20) says "index" but its own REPLY description (Table 21) says "the handle... used with &02",
  a mismatch the spec's two tables show on their own, nothing to confirm on real hardware. `&06`/`&08`/
  `&09` answer ERROR 4 (not found) with no disk selected, mirroring how no current program/sample
  already behaves elsewhere; the spec does not say.

### TASK-AKM-059: Disk format, free space and name
- **Tier**: L
- **Status**: Done
- **Description**: Implement `&0A` (Get Format), `&0B` (Get Free Space) and `&0E` (Get Disk Name).
  `&0B` is the catalogue's first `qword`-formatted item: extend `generate_akm_items.py`'s schema to
  accept `format: "qword"` (mirroring `DEC-AKM-013`'s addition of `string`), record the new decision
  as `DEC-AKM-017` under `ADR-AKM-001`, and decode it to `std::uint64_t` end to end.
- **Requirement refs**: RQ-AKM-062
- **ADR refs**: ADR-AKM-001 (new DEC-AKM-017)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-062, on the simulated sampler;
  `generate_akm_items.py --check` up to date after the schema change.
- **Dependencies**: TASK-AKM-057
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean rebuild, no warning or error (`/W4 /WX`); `ctest
  --test-dir juce/build -C Debug` 507/507 passed. `DEC-AKM-017` (ADR-AKM-001): `ValueFormat::Qword`
  added next to `Dword` (`ItemDescriptor.hpp`), `valueWidth` returns `QWORD_WIDTH` (8) for it, and the
  generic `appendValue`/`readValue` of `ItemRequest.cpp` grew one case each calling the codec's own
  `ByteWriter::appendQword`/`ByteReader::readQword` (already existed, `RQ-AKM-002`, unused until now):
  no dedicated pair of functions was needed the way `String` required, its 56-bit range fitting
  `std::int64_t` directly. `generate_akm_items.py`'s `FORMATS` dict accepts `"qword"`
  (`QWORD_MAX = 128**8 - 1`); its own test for a format the schema refuses now uses a fabricated
  `"nibble"` instead of `"qword"` (which would no longer be refused). `ItemCatalogueTests.cpp`'s
  `EVERY_FORMAT`/`EVERY_FORMAT_ITEM` grew from 6 to 7 values to cover `Qword` in the generic round
  trip (encode and decode, value 128, `fixedReplyLength` sum `+8`), alongside its own direct
  `valueWidth(ValueFormat::Qword) == 8` check. 3 new cases in `DiskPrimitivesTests.cpp`
  (`ctest -R RQ-AKM-062`): a disk formatted FAT32 with 4 GiB free decodes both fields correctly, freeing
  the free-space value through `std::uint64_t` end to end; a specified disk's name decodes like a
  sampler or program name; with nothing selected, format and free space each complete Error.
  `generate_akm_items.py --coverage`: the 3 new items covered (12 of 35 §10 rows now),
  `unaccounted: none`, exit 0 — the `&0D`/`&0E` decimal erratum this task's `&0E` triggers is noted,
  not flagged, now that `KNOWN_DEC_ERRATA` excepts it; `--check` up to date (272 items); the 22 Python
  script tests re-run (`python juce/tests/tools/test_generate_akm_items.py`), still 22/22.
- **Assumptions**: The `&0D`/`&0E` decimal-column erratum (`FTR-AKM-007`'s "Known spec inconsistency",
  also `sysex_spec.kb.md` line 178) is resolved now, by this task, rather than deferred to
  `TASK-AKM-068`'s coverage closure: leaving it unresolved would fail `--coverage`'s exit code (and the
  `akm_item_catalogue_matches_the_spec` ctest entry) the moment `&0E` is catalogued, the same way
  `TASK-AKM-032` resolved `08 &6C` immediately rather than waiting for `TASK-AKM-034`'s own coverage
  task — `TASK-AKM-068` only needs to confirm it, not fix it. `&0A`/`&0B` act on the currently selected
  disk (the spec's own "of current disk" wording), answering `ERROR 4` with none selected, consistent
  with `TASK-AKM-058`'s choice for `&06`/`&08`/`&09`. A disk's free space has no spec-given default; the
  model's own default (`0`) is as arbitrary as `SystemSetupState::waveTotalBytes`'s.

### TASK-AKM-060: Folder navigation, listing and management
- **Tier**: L
- **Status**: Done
- **Description**: Implement `&10`-`&14` (sub-folder count/name/all-names, open, close) and `&16`/
  `&18` (create, rename). `&18` is the catalogue's first item carrying two consecutive
  null-terminated strings: extend the item-catalogue schema to express a second string argument,
  recording the new decision under `ADR-AKM-001`.
- **Requirement refs**: RQ-AKM-063
- **ADR refs**: ADR-AKM-001 (new DEC-AKM-018)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-063, on the simulated sampler;
  `generate_akm_items.py --check` up to date after the schema change.
- **Dependencies**: TASK-AKM-058
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean rebuild, no warning or error (`/W4 /WX`); `ctest
  --test-dir juce/build -C Debug` 512/512 passed. `DEC-AKM-018` (ADR-AKM-001): `makeTwoStringRequest`
  added next to `makeStringRequest` in `ItemRequest.hpp`/`.cpp` — the catalogue schema already allowed
  two `String` values (`validate_values` checks each independently), so only the C++ encode path needed
  the new function; no decode counterpart exists yet since every two-String item so far is a `Set`.
  `SimulatedSampler` gained a minimal folder model (`FolderRecord`: name plus sub-folders,
  `DiskRecord::rootFolder`, `_currentFolderPath` as a chain of indices from the root, reset on
  `setDisks` and on `&02`). 5 new cases in `DiskPrimitivesTests.cpp` (`ctest -R RQ-AKM-063`): a folder
  with two sub-folders reports count 2, the second one's name, and both names in order; closing the
  root completes Error; opening a sub-folder, creating a folder inside it, renaming it and reading its
  name back all round-trip correctly; opening a name that does not exist completes Error; with no disk
  selected, folder count and open both complete Error. `generate_akm_items.py --coverage`: the 7 new
  items covered (19 of 35 §10 rows now), `unaccounted: none`, exit 0 (no new decimal erratum: `&16`/
  `&18`'s own decimal columns already match their hex); `--check` up to date (279 items); the 22 Python
  script tests re-run, still 22/22.
- **Assumptions**: `&13` (Open Folder) treats an empty name as the spec's own `<Data1> = 0` root
  convention; `decodeStringReply`'s existing empty-string handling (a lone terminator byte) already
  covers it with no special case. `&03` ("Test Disk Valid")-style ambiguity does not recur here: every
  item's own data columns and text agree on this group. The current folder resets when the current disk
  changes (`&02`) or `setDisks` reseeds the list — the spec says nothing of what survives a disk
  change, and a stale path into a different disk's tree would be meaningless. Folder creation does not
  check for an existing sub-folder of the same name (the spec does not say what should happen); no
  acceptance criterion exercises that case, so nothing was added for it (no scope creep).

### TASK-AKM-061: Load Folder
- **Tier**: M
- **Status**: Done
- **Description**: Implement `&15` (Load Folder), completing on DONE or ERROR; no real-sampler call
  without the guard of TASK-AKM-067.
- **Requirement refs**: RQ-AKM-064
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-064, on the simulated sampler.
- **Dependencies**: TASK-AKM-060
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean rebuild, no warning or error (`/W4 /WX`); `ctest
  --test-dir juce/build -C Debug` 514/514 passed. `FolderRecord` gained `programFiles`/`sampleFiles`
  (names only — no AKAI file format is modelled, only the SysEx protocol, so a "file" is just the name
  loading it would materialize); `SimulatedSampler::executeDisk` gained `programs`/`samples` parameters
  so `&15` can append to them. 2 new cases in `DiskPrimitivesTests.cpp` (`ctest -R RQ-AKM-064`): a
  folder containing a program directly and a sample in its own sub-folder, when loaded, adds both to
  memory (read back via `getAllProgramNames`/`getAllSampleNames`) and completes Done; a name that does
  not exist completes Error. `generate_akm_items.py --coverage`: the 1 new item covered (20 of 35 §10
  rows now), `unaccounted: none`; `--check` up to date (280 items).
  `ItemCatalogueTests.cpp`'s total-item-count check updated (`PROGRAM_ITEM_COUNT`, `+1`).
- **Assumptions**: Loading recurses into every sub-folder of the named folder, collecting every
  program and sample name found anywhere in that sub-tree — the spec's own wording ("the selected
  folder, and all its contents including sub-folders, are loaded") supports this directly. No
  real-sampler test exercises `&15` in this task: that is `TASK-AKM-067`'s guard, per `RQ-AKM-070`.

### TASK-AKM-062: File listing, info and rename
- **Tier**: M
- **Status**: Done
- **Description**: Implement `&20`-`&24` (file count/name/all-names/size/index-by-name) and `&28`
  (rename), reusing the two-string item shape added by TASK-AKM-060.
- **Requirement refs**: RQ-AKM-065
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-065, on the simulated sampler.
- **Dependencies**: TASK-AKM-060
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean rebuild, no warning or error (`/W4 /WX`); `ctest
  --test-dir juce/build -C Debug` 518/518 passed. `&23`'s Compound Double Word is catalogued as four
  separate `Byte` values (`sizeMsb`/`Sb2`/`Sb1`/`Lsb`), not a single `Dword`: the first attempt (one
  `Dword`) failed `--coverage` outright (`DiskGetFileSize: reply has 1 values, the spec row describes
  4`) because this item's own TSV row, unlike §02's Wave memory, decomposes cleanly into four
  comparable byte columns rather than being shortened by an ellipsis — so it is catalogued the way
  §0E's position/loop items are, verified byte-for-byte instead of skipped. `FileRecord` (name, a size
  in bytes) added to `FolderRecord` as `files`, separate from `programFiles`/`sampleFiles`: listing and
  loading are different items, and nothing ties a listed file to a loadable one. 5 new cases in
  `DiskPrimitivesTests.cpp` (`ctest -R RQ-AKM-065`): a folder with two files (one of them empty) reports
  count 2, the first name, both names in order, one size (2048) and the empty one's size (0), and the
  empty file's index by name (1); renaming to a name with an extension already in it sends exactly that
  name, read back unchanged; a file name that does not exist completes Error for both the index lookup
  and a rename; with no disk selected, file count and index-by-name both complete Error.
  `generate_akm_items.py --coverage`: the 6 new items covered (26 of 35 §10 rows now),
  `unaccounted: none`; `--check` up to date (286 items). `ItemCatalogueTests.cpp`'s total-item-count
  check updated (`PROGRAM_ITEM_COUNT`, `+6`).
- **Assumptions**: `&24`'s REPLY is read through the generic `decodeReply` (two Bytes, like every other
  index REPLY): the spec's "returned if found" wording is read as "else an ERROR", the same assumption
  already made for `&03`/other find-by-something items, not a new one. `&28`'s rename does not strip or
  validate an extension the caller repeats in the new name (spec footnote c): per `RQ-AKM-065`'s own
  statement, that is a caller concern.

### TASK-AKM-063: Load File, with and without dependent children
- **Tier**: M
- **Status**: Done
- **Description**: Implement `&2A` (Load File) and `&2B` (Load File including dependents); no
  real-sampler call without the guard of TASK-AKM-067.
- **Requirement refs**: RQ-AKM-066
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-066, on the simulated sampler.
- **Dependencies**: TASK-AKM-062
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean rebuild, no warning or error (`/W4 /WX`); `ctest
  --test-dir juce/build -C Debug` 522/522 passed. `&2A`'s shape (a String, then a sample-load-option
  Byte) fits neither `makeStringRequest` (exactly one String) nor the generic `int64_t` path (no
  String), so `loadFile` builds its `CommandRequest` by hand, the same way
  `ProgramPrimitives::setProgramNumber` already does for its own conditional shape; `&2B` has no such
  byte and uses `makeStringRequest` directly. `FileRecord` gained `loadsProgramNamed`/
  `loadsSampleNamed`/`dependsOnFiles` so a file can say what loading it materializes, independent of
  `FolderRecord::programFiles`/`sampleFiles` (which stay Load Folder's own, recursive, concern). 4 new
  cases in `DiskPrimitivesTests.cpp` (`ctest -R RQ-AKM-066`): a program file whose sample is a separate,
  dependent file loads only the program with `&2A`; the same file loads both with `&2B`; a `Virtual`
  sample-load option is confirmed on the wire as the last accepted byte, `2`; a file name that does not
  exist completes Error for both items. `generate_akm_items.py --coverage`: the 2 new items covered (28
  of 35 §10 rows now), `unaccounted: none`; `--check` up to date (288 items).
  `ItemCatalogueTests.cpp`'s total-item-count check updated (`PROGRAM_ITEM_COUNT`, `+2`).
- **Assumptions**: `&2A`'s `sampleLoadOption` only affects `SampleRecord::type` (`Virtual` → 1, anything
  else → 0, `RAM` included) when the loaded file materializes a sample; it is ignored for a program.
  `&2B`'s dependents are resolved one level only (a dependency's own `dependsOnFiles`, if any, is not
  followed) and use the `Normal` load option, since the spec gives `&2B` no load-option byte of its own
  and no acceptance criterion needs more than one level. A dependency name `&2B` cannot find in the
  current folder is skipped rather than failing the whole command — the spec does not say what should
  happen, and failing the file that does exist for one that does not seemed the worse default.

### TASK-AKM-064: Save Memory Item(s) to disk
- **Tier**: M
- **Status**: Done
- **Description**: Implement `&2C` (Save Memory Item) and `&2D` (Save All Memory Items), with no
  default for the overwrite flag; no real-sampler call without the guard of TASK-AKM-067.
- **Requirement refs**: RQ-AKM-067
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-067, on the simulated sampler.
- **Dependencies**: TASK-AKM-062
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean rebuild, no warning or error (`/W4 /WX`); `ctest
  --test-dir juce/build -C Debug` 525/525 passed. Both items fit the generic `makeRequest` path
  directly (all-Byte args, no String) — no hand-built request needed, unlike `&2A`. `overwriteExisting`
  and `saveChildren` are plain `bool` parameters with no default value in either primitive's signature,
  satisfying RQ-AKM-067's "no silent default" acceptance criterion by construction (a caller must always
  write something at that argument position; there is nothing to verify at runtime beyond this). The
  simulated sampler saves a Program or a Sample (by index for `&2C`, every one in memory for `&2D`) as a
  new or replaced `FileRecord` named `<itemName>.AKP`/`.WAV` in the current folder, loadable the same
  way `&2A` already materializes one; the other four memory types are not modelled (no Multi/SMF/
  Setlist/Scenelist section exists yet) and are accepted as a no-op `Done`, same choice as Load
  Folder's own scope. 3 new cases in `DiskPrimitivesTests.cpp` (`ctest -R RQ-AKM-067`): an existing file
  at the target name, saved with overwrite `false`, is left unchanged (its original size still reads
  back) and the command completes Error; saved with overwrite `true`, the file is replaced (new size 0,
  still one file) and it completes Done; two programs saved with `saveAllMemoryItems` produce
  `A.AKP`/`B.AKP` in order. `generate_akm_items.py --coverage`: the 2 new items covered (30 of 35 §10
  rows now), `unaccounted: none`; `--check` up to date (290 items). `ItemCatalogueTests.cpp`'s
  total-item-count check updated (`PROGRAM_ITEM_COUNT`, `+2`).
- **Assumptions**: `saveChildren` is read (for wire-format validation) but has no modelled effect — no
  dependent file is created or saved alongside the target, since no acceptance criterion exercises it
  and the model has no "child of a save" concept beyond `&2B`'s own, unrelated, `dependsOnFiles`. The
  file extension a save gives its target (`.AKP`/`.WAV`: a program is `.AKP` and a sample `.WAV`, corrected on 2026-10-06; `.AKS` had been an invention of the assistant, in no spec) is an internal modelling choice with no wire
  significance; a disk whose free space or format the save might plausibly affect is left untouched,
  since the spec gives no rule for how a save changes them and no criterion asks for it. `&2D`'s save
  stops at the first item that fails to save (mirrors `DEC-AKM-010`'s command-sequence-abort-on-failure
  precedent) rather than attempting every item and reporting a partial result.

### TASK-AKM-065: Sample audition from disk
- **Tier**: M
- **Status**: Done
- **Description**: Implement `&30` (Start Audition) and `&31` (Stop Audition).
- **Requirement refs**: RQ-AKM-068
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-068, on the simulated sampler.
- **Dependencies**: TASK-AKM-057
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean rebuild, no warning or error (`/W4 /WX`); `ctest
  --test-dir juce/build -C Debug` 527/527 passed. Named `startFileAudition`/`stopFileAudition`, not
  `startSampleAudition`/`stopSampleAudition`: those names are already `SamplePrimitives.hpp`'s own
  §0E items, which audition a sample already in memory, a different operation from auditioning a file
  straight off disk. 2 new cases in `DiskPrimitivesTests.cpp` (`ctest -R RQ-AKM-068`): a file at index 0
  starts then stops audition, both Done; an index naming no file completes Error on start. The model
  does not track audition state at all — `&31` always completes Done, the "stop when nothing plays"
  case RQ-AKM-068 itself leaves unconstrained. `generate_akm_items.py --coverage`: the 2 new items
  covered (32 of 35 §10 rows now), `unaccounted: none`; `--check` up to date (292 items).
  `ItemCatalogueTests.cpp`'s total-item-count check updated (`PROGRAM_ITEM_COUNT`, `+2`).
- **Assumptions**: None.

### TASK-AKM-066: Destructive command guards for Eject, Delete Sub-Folder and Delete File
- **Tier**: M
- **Status**: Done
- **Description**: Guard `&0D` (Eject Disk, discard option), `&17` (Delete Sub-Folder) and `&29`
  (Delete File) behind an explicit confirmation argument, mirroring `RQ-AKM-025`/`RQ-AKM-046`/
  `RQ-AKM-056`.
- **Requirement refs**: RQ-AKM-069
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-069, on the simulated sampler.
- **Dependencies**: TASK-AKM-060, TASK-AKM-062
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean rebuild, no warning or error (`/W4 /WX`); `ctest
  --test-dir juce/build -C Debug` 530/530 passed. `&0D` is split into two primitives rather than one
  with an ignorable parameter: `ejectDisk` (always sends `<Data3> = 0`, no guard, cannot discard
  anything) and `ejectDiskDiscardingVirtualSamples` (sends `<Data3> = 1`, gated the same way as
  `deleteAllPrograms`); `deleteSubFolder`/`deleteFile` follow the same gated-request pattern, each with
  its own confirmation enumerator. These three items complete §10's 35 command rows — the section is
  now fully catalogued. 4 new cases in `DiskPrimitivesTests.cpp` (`ctest -R RQ-AKM-069`): all three
  guarded calls refuse as `NotConfirmed` with nothing sent when given no confirmation; `ejectDisk`
  (no discard) sends and removes the disk without any confirmation; with confirmation, eject-with-
  discard, delete sub-folder and delete file each remove their target and complete Done.
  `grep -rn "ejectDiskDiscardingVirtualSamples\|deleteSubFolder\|deleteFile\b" juce/tests/support/src/RealSamplerSuite.cpp juce/tests/probe/main.cpp`:
  no match — satisfies "no real-sampler test calls this" trivially, since §10's real-sampler harness
  does not exist yet (`TASK-AKM-067`). `generate_akm_items.py --coverage`: `section 10: 35 of 35 spec
  rows covered`, `unaccounted: none`; `--check` up to date (295 items). `ItemCatalogueTests.cpp`'s
  total-item-count check updated (`PROGRAM_ITEM_COUNT`, `+3`).
- **Assumptions**: Ejecting the current disk resets the current disk and folder selection; ejecting any
  other disk shifts the current-disk index down by one if it came after the ejected one in the list —
  neither case is asked for by an acceptance criterion, but leaving the index dangling or pointing at
  the wrong disk seemed the worse default. The discard flag itself has no modelled effect beyond
  removing the disk: no virtual sample is tracked in this model, so there is nothing to discard or to
  refuse over "drive in use".

### TASK-AKM-067: Real-sampler harness — disposable test folder and slow-operation guard
- **Tier**: L
- **Status**: Done
- **Description**: Add a `--disk-tools` check to `xs56k_akm_probe --suite` that creates its own
  disposable sub-folder, exercises the safe §10 primitives inside it, deletes it through TASK-AKM-066's
  guard when done, never touches anything that existed before, and gates `&01`/`&15`/`&2A`/`&2B`/`&2C`/
  `&2D` behind a further, separate opt-in flag (`--disk-tools-slow OP`) that documents the observed hang
  risk in its own help text.
- **Requirement refs**: RQ-AKM-070, RQ-AKM-071
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008 precedent for `--slow-operation`)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-070 and RQ-AKM-071, on the
  simulated sampler in `ctest`, and on the real sampler run by the owner (safe path only, unless the
  owner explicitly also passes the slow-operation guard).
- **Dependencies**: TASK-AKM-057 to TASK-AKM-066
- **Assignee**: AI, with the owner running the real-sampler suite
- **Verification**: Windows/MSVC Debug: clean rebuild, no warning or error (`/W4 /WX`); `ctest
  --test-dir juce/build -C Debug` 538/538 passed. 8 new cases in `RealSamplerSuiteTests.cpp`
  (`ctest -R "Disk Tools"`, `[akm][suite][disk-tools]`): the safe check on a sampler with a current disk
  passes, creates the folder and its inner sub-folder (at least two `&16`) and deletes the test folder
  with exactly one `&17`; with no current disk the safe check fails before creating anything; each of the six
  guarded items passes on the simulated sampler inside the folder — `update-list` sends exactly one
  `&01`; `save-memory-item` sends one `&2C`; `load-file` sends one `&2C` then one `&2A`;
  `load-file-with-dependents` one `&2C` then one `&2B`; `load-folder` one `&15` on an empty child folder;
  `save-all-memory-items` one `&2D`. `--disk-tools-slow` refusals checked by hand: without `--suite`,
  without `--disk-tools`, and with an unknown or missing OP, each refused before any port is opened.
  Owner confirmation added after the owner's request: each save-making item asks the owner, through
  `askOwner`, to confirm the file on the sampler (after the save, and before the load for the load items);
  declined → Skipped; no way to ask → Skipped before any save is sent. 3 more cases, 11 disk-tools cases in
  total; `ctest` 540/540 after the change.
  `AGENTS.md` updated (the two flags, the one-per-run rule, the hang citation and the owner confirmation). `setCurrentDisk`
  added to `SimulatedSampler` as the test seam the safe check needs (the suite never selects a disk).
  The real-sampler run is the owner's to make: nothing here sent a frame to hardware.
- **Assumptions**: Three choices made here, to be confirmed by the owner before a real run:
  (1) `--disk-tools-slow` sends **one** long-running item per run, as chosen, but `load-file` and
  `load-file-with-dependents` each send a save (`&2C`) before their load (`&2A`/`&2B`): no file can be
  made inside the sub-folder without saving one, so the two-command sequence is forced by the spec and
  not a choice; the owner is told this in the help text and in the pre-run printout. (2) The safe check
  cannot list real files — there is no safe way to create one — so its file items are only read on the
  empty folder. (3) `--slow-operation` (the older flag, sending one raw `&01`) is left unchanged; it does
  not go through `DiskPrimitives` and is not gated behind `--disk-tools`.

### TASK-AKM-068: Coverage of section §10 and errata resolution
- **Tier**: M
- **Status**: Done
- **Description**: Run `generate_akm_items.py --coverage` for section `10`, list any exclusion with
  its reason, record the `&0D`/`&0E` decimal-column erratum in `KNOWN_DEC_ERRATA` and in
  `sysex_spec.kb.md`, and update `SUMMARY-akm-sections-coverage.md` and `AGENTS.md` if the suite's
  options changed.
- **Requirement refs**: RQ-AKM-072
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-072.
- **Dependencies**: TASK-AKM-057 to TASK-AKM-067
- **Assignee**: AI
- **Verification**: The `&0D`/`&0E` decimal-column erratum was already resolved by `TASK-AKM-059`
  (`KNOWN_DEC_ERRATA`, `sysex_spec.kb.md` line 178) before this task started, the same pattern
  `TASK-AKM-054` found for §02's own erratum. `generate_akm_items.py --coverage` re-run this session:
  `section 10: 35 of 35 spec rows covered (Disk, complete)`, `unaccounted: none`, exit 0 — no row
  excluded, nothing left to list a reason for. `items.json`'s section-10 `complete` flag flipped
  `false` → `true` (its note closed out, citing this task); `generate_akm_items.py` (no args)
  regenerated `ItemTable.generated.hpp` with no diff (section metadata, not read by the generator), then
  `--check` reported up to date (295 items, unchanged). Windows/MSVC Debug: clean rebuild, no warning or
  error (`/W4 /WX`); `ctest --test-dir juce/build -C Debug` 530/530 passed, re-run after the flag flip.
  `AGENTS.md`: no change — no probe option changed in this task (the real-sampler harness extension is
  `TASK-AKM-067`, not yet done). `SUMMARY-akm-sections-coverage.md` updated: §10's bar to 100%, the
  overall total to 441/560 (79%), and the closing note from anticipatory to confirmed.
- **Assumptions**: None.
- **Assumptions**: None.
