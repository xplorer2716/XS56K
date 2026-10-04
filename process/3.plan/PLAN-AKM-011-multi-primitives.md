# PLAN-AKM-011: Multi Primitives (Phase A, new lot, §0C)

## Overview

Implements `FTR-AKM-011`: one tested primitive per command row of section `0C` (49 commands, 11 REPLY formats — 60
rows), and an opt-in real-sampler check on a disposable test multi.

Section `0C` has the shape of §0A: a sampler-wide "current multi" selection state (set by creation or selection) and
the items acting on it. Its twelve part parameters take a part number first, like §06's zone, so they go through the
generic `makeRequest`/`decodeReply` path with a table-driven test (`ZoneParameterCases` is the model). What is new:
the flag-then-number REPLY of `&41`/`&50`, streams of names with a single `00` for "no part" (`&46`, `&51`, `&45`),
three more values-per-part or per-multi REPLYs (`&47`, `&48`, `&52`), and three hand-built shapes for the Sets `&31`,
`&32`, `&33`. The simulated sampler's multi list moves from names (`setMultiNames`, used by Clear Sampler Memory,
`RQ-AKM-056`) to full records, keeping its signature.

**Safety note.** The real-sampler check creates one multi and one program under reserved names and deletes both on
every exit path, puts back the multi that was current, and sends neither `&07` nor `&01` (`RQ-AKM-093`).

## References
- **Requirements**: RQ-AKM-087 to RQ-AKM-094 (`FTR-AKM-011`)
- **ADRs**: ADR-AKM-001 (Accepted): DEC-AKM-003, DEC-AKM-011 (the guard), DEC-AKM-012, DEC-AKM-013, DEC-AKM-014,
  DEC-AKM-015. No new decision is expected; one is added to the existing file, as before, if the work shows one is needed.

The plan has 8 tasks (TASK-AKM-088 to TASK-AKM-095): 088 authors the artifacts; 089 the lifecycle (which adds the
section to the simulated sampler); 090 the guarded "Delete ALL"; 091 the part parameters; 092 the Gets of general
information; 093 renaming, program number and part assignment; 094 the real-sampler check; 095 the coverage.

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-AKM-088: Author FTR-AKM-011 and PLAN-AKM-011
- **Tier**: S
- **Status**: Done
- **Description**: Write the feature file and this plan for section `0C`, from the spec's Tables 16 and 17.
- **Requirement refs**: RQ-AKM-087 to RQ-AKM-094
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* section `0C` of the spec, *When* the feature file is read, *Then* each of its 60 rows is the subject of a requirement.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: N/A (Tier S). Both files written this session; `agnos-index` re-run after them.
- **Assumptions**: The user's autonomy grant for §16 and §0C (session AKM, 2026-10-04) stands in for the DoR approval of each task of the plan.

---

### TASK-AKM-089: Multi creation, selection, deletion and the current multi's name and index
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue and implement `&01`, `&02`, `&05`, `&06`, `&08` and, to verify them, `&42` and `&43` as
  primitives in `MultiPrimitives`; move the simulated sampler's multis to records (`setMultiNames` unchanged for its
  callers, §0C items modelled).
- **Requirement refs**: RQ-AKM-087
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-087.
- **Dependencies**: TASK-AKM-088
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean build (`/W4 /WX`), `ctest --test-dir juce/build -C Debug` 645/645 passed
  after a full re-run in this session (636 before, 9 new in `MultiPrimitivesTests.cpp`, written before the code,
  `ctest -R RQ-AKM-087` 9/9): create `MIX1` (frame `4D 49 58 31 00`) makes it current, index 1 after a seeded `OLD`,
  count 2; `&01` carries `01` for 64 parts and the multis created after it have 32, 64 and 128 parts (the default 32
  before any `&01`); an unknown name or an index past the end fails ERROR 04; a duplicate name fails ERROR 05 and adds
  nothing; selection by name and by index (frame `00 02`) is followed by `&42`/`&43`; deleting the current multi leaves
  the others in order with none current; delete, `&42` and `&43` fail ERROR 04 with none current; an index of 16384 is
  refused `ArgumentOutOfRange` and a non-ASCII name `NotEncodable`, nothing sent; the name Get is refused
  `ChecksumModeUnknown`. `generate_akm_items.py --check`: up to date (325 items); `--coverage`: `unaccounted: none`.
  Clear Sampler Memory tests (`RQ-AKM-056`) still pass on the multi records. `ItemCatalogueTests.cpp`'s count formula
  extended by the 7 records. Not verified: the real sampler (TASK-AKM-094).
- **Assumptions**: The simulated sampler answers ERROR 05 for a duplicate multi name (as for a program) and ERROR 04
  for a name, an index or a "current" item with no multi (as §0A and §0E do): modelling choices, the spec being silent.
  `MultiPartCount` is an enumerator rather than a bare 0-2 so that an out-of-range code cannot be built. Section `0C`
  was added to `items.json` with `complete: false` until TASK-AKM-095. `setMultiNames` keeps its signature; its
  multis now have 32 parts.

---

### TASK-AKM-090: Destructive command guard for "Delete ALL Multis"
- **Tier**: S
- **Status**: Done
- **Description**: Implement `&07` so that it is sent only with an explicit confirmation argument that no default
  supplies, mirroring `ConfirmDeleteAllSamples`, and audit the real-sampler test sources to confirm none calls it.
- **Requirement refs**: RQ-AKM-088
- **ADR refs**: ADR-AKM-001 (DEC-AKM-011)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-088.
- **Dependencies**: TASK-AKM-089
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean build (`/W4 /WX`), `ctest --test-dir juce/build -C Debug` 647/647 passed
  after a full re-run in this session (645 before, 2 new in `MultiDeleteAllGuardTests.cpp`, mirroring
  `SampleDeleteAllGuardTests.cpp`): `std::nullopt` refuses as `NotConfirmed` with nothing sent and both multis still
  held; the enumerator sends `&07`, completes on DONE, leaves no multi and a following select-by-index fails ERROR 04.
  `generate_akm_items.py --check`: up to date (326 items); `--coverage`: `unaccounted: none`. Audit: a search of
  `juce/tests/support/src/*.cpp` and `juce/tests/probe/main.cpp` for `deleteAllMultis|MultiDeleteAll` returns no match,
  confirming no real-sampler test calls it.
- **Assumptions**: None.

---

### TASK-AKM-091: Multi part parameters (Set and Get)
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue the twelve Set/Get pairs `&10`-`&1B` / `&20`-`&2B` (part number first, one value) and
  model them in the simulated sampler; a table of cases shared with the real-sampler check.
- **Requirement refs**: RQ-AKM-089
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-089.
- **Dependencies**: TASK-AKM-089
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean build (`/W4 /WX`), `ctest --test-dir juce/build -C Debug` 652/652 passed
  after a full re-run in this session (647 before, 5 new in `MultiPartParametersTests.cpp`, written before the code,
  `ctest -R RQ-AKM-089` 5/5): every one of the twelve items is set on part 2 then read back and equals the value set
  (one table, values chosen across each range); the shared table `MultiPartParameterCases` (12 rows, part 3, values
  independent of the first table's) does the same; a Level set on part 2 leaves parts 1, 3 and 127 at 0; set and get
  with no current multi fail ERROR 04; part 128, a level of 101 and a pan of 13 are refused `ArgumentOutOfRange`
  with only the Create Multi sent. `generate_akm_items.py --check`: up to date (350 items); `--coverage`:
  `unaccounted: none`, all 24 items covered. `ItemCatalogueTests.cpp`'s count formula extended by the 24 records.
  Not verified: the real sampler (TASK-AKM-094).
- **Assumptions**: The simulated sampler accepts any part number 0-127 whatever the multi's number of parts (the spec
  says nothing of a part beyond the multi's size) and does not range-check values (the client does, as for §06).
  Value ranges are the spec's own: output 0-23, pan 14-114, fine tune 0-100, transpose 0-72, notes 21-127, MIDI
  channel 0-31; the real sampler's refusals are observations for TASK-AKM-094.

---

### TASK-AKM-092: General information about the current multi and about all the multis
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement `&40`, `&41`, `&44`, `&45`, `&46`, `&47`, `&48` and `&50`, `&51`, `&52` as primitives
  (`&42`/`&43` are TASK-AKM-089's) and model them in the simulated sampler.
- **Requirement refs**: RQ-AKM-090, RQ-AKM-091
- **ADR refs**: ADR-AKM-001 (DEC-AKM-013, DEC-AKM-014, DEC-AKM-015)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-090 and RQ-AKM-091.
- **Dependencies**: TASK-AKM-089, TASK-AKM-091
- **Assignee**: AI
- **Verification**: (filled at closure)
- **Assumptions**: (filled at closure)

---

### TASK-AKM-093: Multi renaming, program number and part assignment
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement `&30`, `&31`, `&32`, `&33`, `&34` as primitives (`&31`, `&32`, `&33` built by hand) and
  model them in the simulated sampler.
- **Requirement refs**: RQ-AKM-092
- **ADR refs**: ADR-AKM-001 (DEC-AKM-013)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-092.
- **Dependencies**: TASK-AKM-089, TASK-AKM-092
- **Assignee**: AI
- **Verification**: (filled at closure)
- **Assumptions**: (filled at closure)

---

### TASK-AKM-094: Real-sampler check on a dedicated test multi
- **Tier**: L
- **Status**: Not Started
- **Description**: Add `--multi-lifecycle` to `xs56k_akm_probe --suite`: create a test multi and a test program under
  reserved names, round-trip every item of the section on them, put back the current multi and delete both on every
  exit path; run it on the real sampler.
- **Requirement refs**: RQ-AKM-093
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008 precedent for the suite's opt-in flags)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-093, on the simulated sampler in `ctest` and on
  the real sampler.
- **Dependencies**: TASK-AKM-089 to TASK-AKM-093
- **Assignee**: AI, running the real-sampler check under the owner's standing authorization
- **Verification**: (filled at closure)
- **Assumptions**: (filled at closure)

---

### TASK-AKM-095: Coverage of section §0C
- **Tier**: M
- **Status**: Not Started
- **Description**: Run `generate_akm_items.py --coverage` for section `0C`, list any exclusion with its reason, flip
  the section's `complete` flag in `items.json`, and update `SUMMARY-akm-sections-coverage.md`, `AGENTS.md` (the new
  probe option) and `CHANGELOG.md`.
- **Requirement refs**: RQ-AKM-094
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-094.
- **Dependencies**: TASK-AKM-089 to TASK-AKM-094
- **Assignee**: AI
- **Verification**: (filled at closure)
- **Assumptions**: (filled at closure)
