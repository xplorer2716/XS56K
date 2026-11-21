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
- **Status**: Done
- **Description**: Implement `&40`, `&41`, `&44`, `&45`, `&46`, `&47`, `&48` and `&50`, `&51`, `&52` as primitives
  (`&42`/`&43` are TASK-AKM-089's) and model them in the simulated sampler.
- **Requirement refs**: RQ-AKM-090, RQ-AKM-091
- **ADR refs**: ADR-AKM-001 (DEC-AKM-013, DEC-AKM-014, DEC-AKM-015)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-090 and RQ-AKM-091.
- **Dependencies**: TASK-AKM-089, TASK-AKM-091
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean build (`/W4 /WX`, after one conversion warning in the first build was
  fixed at its cause), `ctest --test-dir juce/build -C Debug` 661/661 passed after a full re-run in this session (652
  before, 9 new in `MultiInformationTests.cpp`, written before the code, `ctest -R "RQ-AKM-09[01]"` 9/9): with `A` (32
  parts) and `B` (64 parts, created after `&01`) the count is 2, the names `A`, `B`, the numbers of parts 32 and 64
  and the current multi's 64; a program number of 4 reads as front-panel 5 on `&41` and `&50` (the other multi's
  empty, the flag off), and off again when cleared; a part with no program reads an empty name on `&45`, `LEAD` on the
  part given it, and `&46` gives 64 names in part order; `&47` on part 3 gives the twelve values the twelve Sets of the
  shared table wrote, in the order of `&20`-`&2B`; with part 1 muted and part 2 soloed `&48` gives 32 values, 1 at
  index 1, 2 at index 2, 0 elsewhere; with no multi current the six current-multi Gets fail ERROR 04; with no multi in
  memory the count is 0 and the three all-multis Gets give empty lists; with the checksum mode unknown the six
  variable-length Gets are refused `ChecksumModeUnknown` with nothing sent; a part of 128 is refused
  `ArgumentOutOfRange`. `generate_akm_items.py --check`: up to date (360 items); `--coverage`: `unaccounted: none`
  (section `0C` 42 of 47 rows covered so far). `ItemCatalogueTests.cpp`'s count formula extended by the 10 records.
  Not verified: the real sampler (TASK-AKM-094).
- **Assumptions**: The three all-multis Gets read ERROR 04 as an empty list and the simulated sampler answers it with
  no multi in memory, mirroring §0A's `&18`/`&19` as observed on the real S5000 for programs (FTR-AKM-011 open
  point amended): a modelling choice for multis until TASK-AKM-094. The part-count REPLY range is 31-127, the spec's
  "31, 63, 127" (the coverage check compares it). A part beyond the multi's size reads as one with no program and zero
  parameters; a part with both mute and solo reads as muted on `&48` (the spec says nothing of both). The `&47`
  values come back as the twelve parameters in the order of `&20`-`&2B`, the spec's "12 data bytes".

---

### TASK-AKM-093: Multi renaming, program number and part assignment
- **Tier**: M
- **Status**: Done
- **Description**: Implement `&30`, `&31`, `&32`, `&33`, `&34` as primitives (`&31`, `&32`, `&33` built by hand) and
  model them in the simulated sampler.
- **Requirement refs**: RQ-AKM-092
- **ADR refs**: ADR-AKM-001 (DEC-AKM-013)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-092.
- **Dependencies**: TASK-AKM-089, TASK-AKM-092
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean build (`/W4 /WX`), `ctest --test-dir juce/build -C Debug` 669/669 passed
  after a full re-run in this session (661 before, 8 new in `MultiEditingTests.cpp`, written before the code,
  `ctest -R RQ-AKM-092` 8/8): rename to `MIX2` sends `4D 49 58 32 00` and `&43` reads `MIX2`; the program number 5 sends
  `01 04` and reads 5 on `&41`, cleared it sends `00` and reads off; 0 and 129 are refused `ArgumentOutOfRange`,
  nothing sent; assigning `LEAD` to part 2 by name sends `02 4C 45 41 44 00` and `&45` reads `LEAD`, `&34` on part 2
  (`02`) leaves it empty; assigning program index 1 to part 5 sends `05 00 01` and `&45` reads `PAD`; a name or an
  index no program has fails ERROR 04 and the part stays empty; with no multi current all five Sets fail ERROR 04;
  part 128, program index 16384 and a non-ASCII name are refused (`ArgumentOutOfRange`, `ArgumentOutOfRange`,
  `NotEncodable`) with nothing sent. `generate_akm_items.py --check`: up to date (365 items); `--coverage`: section
  `0C` 47 of 47 rows covered, `unaccounted: none`. `ItemCatalogueTests.cpp`'s count formula extended by the 5 records.
  Not verified: the real sampler (TASK-AKM-094).
- **Assumptions**: The simulated sampler assigns a program only if the sampler's program memory holds it (ERROR 04
  otherwise) and refuses a part beyond the multi's size with ERROR 02 (a modelling choice, the spec says nothing of
  either). `&31` with the flag off sends the flag alone, as `setProgramNumber` does for §0A. `&32`'s program index is
  the position in the sampler's program memory, as §0A's selection by index.

---

### TASK-AKM-094: Real-sampler check on a dedicated test multi
- **Tier**: L
- **Status**: Done
- **Description**: Add `--multi-lifecycle` to `xs56k_akm_probe --suite`: create a test multi and a test program under
  reserved names, round-trip every item of the section on them, put back the current multi and delete both on every
  exit path; run it on the real sampler.
- **Requirement refs**: RQ-AKM-093
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008 precedent for the suite's opt-in flags)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-093, on the simulated sampler in `ctest` and on
  the real sampler.
- **Dependencies**: TASK-AKM-089 to TASK-AKM-093
- **Assignee**: AI, running the real-sampler check under the owner's standing authorization
- **Verification**: Windows/MSVC Debug: clean build (`/W4 /WX`), `ctest --test-dir juce/build -C Debug` 674/674 passed
  after a full re-run in this session (661 before TASK-AKM-093, 8 of its own, 5 new here in `RealSamplerSuiteTests.cpp`,
  `ctest -R RQ-AKM-093` 6/6 with the shared table's test): with two owner's multis (the second current) both checks pass,
  the test multi and the test program are gone, the multis and the selection are as they were, `&02` was sent and
  neither `&07` nor `&01`; with no multi both pass; a multi already bearing the reserved name makes the first check
  fail ("already exists … stops without touching it") with no `&02` sent and the multi untouched; the check made to
  fail leaves the sampler as found; without `--multi-lifecycle` no section 0C item is sent. Real sampler (S5000, OS 2.14),
  `xs56k_akm_probe --suite --multi-lifecycle --in "MIDIIN2 (ESI M8U eX)" --out "MIDIOUT15 (ESI M8U eX)"`, three runs this
  session (`akm-suite-20261004-161129.log`, `-161416.log`, `-161454.log`): the first two failed on two things the spec
  leaves open (setting a part's solo clears its mute; `&31` off needs its number byte, `00 00`), each time with both test
  items deleted by the guards and the sampler as found; the third passed 9 checks of 9, every §0C item of the plan
  sent and read back, the sampler left in the known state (`process/2.architecture/OBSERVATIONS-RQ-AKM-093-multi.md`).
  Not verified on hardware: `&01`, `&07` (never sent, by design), mute set after solo, parts beyond the multi's
  size, selection of an existing multi of the owner's (the sampler held none).
- **Assumptions**: The suite's own program (`GuardedTestProgram`, `XS56K_SUITE_TEST`) is reused for the part assignment.
  The test multi lands last in memory, so its index is the number of multis before it, and the guard deletes it only
  after reading its name (one of the two reserved ones). The multi created takes the number of parts the sampler's
  setting gives it (32 here), which no item reads before the creation. The simulated sampler now clears a part's mute
  when its solo is set, and `setMultiProgramNumber` off sends `00 00`, both from the hardware observations (TASK-AKM-092's
  and TASK-AKM-093's tests were adjusted to them: the `&47` test reads its expectation from the twelve Gets, the off
  frame is `00 00`; no test was changed to force a pass: each states the observed behaviour). The reverse
  (solo cleared by mute) is not modelled, not being known.

---

### TASK-AKM-095: Coverage of section §0C
- **Tier**: M
- **Status**: Done
- **Description**: Run `generate_akm_items.py --coverage` for section `0C`, list any exclusion with its reason, flip
  the section's `complete` flag in `items.json`, and update `SUMMARY-akm-sections-coverage.md`, `AGENTS.md` (the new
  probe option) and `CHANGELOG.md`.
- **Requirement refs**: RQ-AKM-094
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-094.
- **Dependencies**: TASK-AKM-089 to TASK-AKM-094
- **Assignee**: AI
- **Verification**: `generate_akm_items.py --coverage` re-run this session after the flag flip: `section 0C: 47 of 47
  spec rows covered (Multi, complete)`, `unaccounted: none`, exit 0 — no row excluded, nothing left to give a reason for;
  `--check`: up to date (365 items); the four catalogue `ctest` entries pass after the flip; the full `ctest` was
  674/674 just before it, with `items.json`'s flag the only change of code since. `items.json`'s section-0C `complete`
  flipped `false` → `true`, its note closed out. No erratum beyond the two behaviours the spec leaves open and the real
  S5000 settled (TASK-AKM-094: solo clears mute; `&31` off needs `00 00`), recorded in
  `OBSERVATIONS-RQ-AKM-093-multi.md`. `SUMMARY-akm-sections-coverage.md`: §0C's bar to 100 %, the total to 530/560
  (95 %); `AGENTS.md`: `--multi-lifecycle` documented; `CHANGELOG.md`: one `[Unreleased]` entry for the whole lot.
- **Assumptions**: `complete` is flipped on the strength of the catalogue matching the spec's rows, as for §20, §04 and
  §16 before it; `&01` and `&07` were never sent to the real sampler, by design.
