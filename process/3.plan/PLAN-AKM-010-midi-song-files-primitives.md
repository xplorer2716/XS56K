# PLAN-AKM-010: MIDI Song File Primitives (Phase A, new lot, §16)

## Overview

Implements `FTR-AKM-010`: one tested primitive per command row of section `16` (12 commands, 6 REPLY formats —
18 rows), and a real-sampler check that reads what the sampler holds and puts back every name it changes.

Section `16` has the same shape as §0E: a sampler-wide "current song file" selection state (`&05`, `&06`, `&08`,
`&09`, `&13`, `&14`) and by-index Gets (`&10`, `&11`), plus a set list sub-group addressed by index only
(`&20`-`&23`). No new value format is needed: indexes are two `Byte` values like §0E's, names are `String`
(DEC-AKM-013). The one new shape is `&23` (an index, then a name), built by hand like `&2A` of §10.

**Safety note.** A song file or set list cannot be created through §16, so the real-sampler check works on what
the sampler holds: it never deletes, renames only to put back, and restores the selection it found
(`RQ-AKM-085`). The deletion primitives are tested on the simulated sampler only.

## References
- **Requirements**: RQ-AKM-082 to RQ-AKM-086 (`FTR-AKM-010`)
- **ADRs**: ADR-AKM-001 (Accepted): DEC-AKM-003, DEC-AKM-012, DEC-AKM-013. No new decision is expected; one is
  added to the existing file, as `DEC-AKM-012` to `019` were, if the work shows one is needed.

The plan has 6 tasks (TASK-AKM-083 to TASK-AKM-087, and TASK-AKM-106): 083 authors the artifacts; 084 delivers the song file items;
085 the set list items (after 084, which adds the section to the simulated sampler); 086 is the real-sampler check
(after 084, 085); 087 closes the coverage (after 084 to 086); 106 (added 2026-10-10, during TASK-MCP-058) makes the check put back by
name, the real S5000 having been seen sorting its song files when one is renamed.

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-AKM-083: Author FTR-AKM-010 and PLAN-AKM-010
- **Tier**: S
- **Status**: Done
- **Description**: Write the feature file and this plan for section `16`, from the spec's Tables 28 and 29.
- **Requirement refs**: RQ-AKM-082, RQ-AKM-083, RQ-AKM-084, RQ-AKM-085, RQ-AKM-086
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* section `16` of the spec, *When* the feature file is read, *Then* each of its 18 rows is the subject of a requirement.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: N/A (Tier S). Both files written this session; `agnos-index` re-run after them.
- **Assumptions**: The user's autonomy grant for §16 and §0C (session AKM, 2026-10-04) stands in for the DoR approval of each task of the plan.

---

### TASK-AKM-084: Song file selection, renaming, deletion and general information
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue and implement `&05`, `&06`, `&08`, `&09`, `&10`, `&11`, `&13`, `&14` as primitives
  in `SongPrimitives`, and model them in the simulated sampler (seeded by `setSongNames`).
- **Requirement refs**: RQ-AKM-082, RQ-AKM-083
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-082 and RQ-AKM-083.
- **Dependencies**: TASK-AKM-083
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean build (`/W4 /WX`), `ctest --test-dir juce/build -C Debug` 624/624
  passed after a full re-run in this session (617 before, 7 new in `SongPrimitivesTests.cpp`, written before the
  code, `ctest -R "RQ-AKM-08[23]"` 7/7): select by name then rename round-trips through `&14` with the frame bytes
  checked (`53 4F 4E 47 31 00`); a name or an index with no song file fails ERROR 04; select by index (frame
  `00 00`), delete, then the count is one less, no song is current and index 0 names what was `B`; delete, rename
  and the two current Gets fail ERROR 04 with none current; count and name by index read `A`, `B`, `C` in order,
  an index past the end fails ERROR 04, and reading by index leaves the selection alone; an empty memory counts 0
  on a REPLY; both name Gets are refused `ChecksumModeUnknown` with nothing sent. `generate_akm_items.py --check`:
  up to date (314 items); `--coverage`: `unaccounted: none`. `ItemCatalogueTests.cpp`'s total-item-count formula
  extended by the 8 new records, as every lot before it did. Not verified: the real sampler (TASK-AKM-086).
- **Assumptions**: The spec is silent on what §16 answers with no song file current or an unknown name or index:
  the simulated sampler answers ERROR `04` as §0E does (a modelling choice, `SimulatedSampler.cpp`
  `executeSongFiles`); the real sampler's answer is for TASK-AKM-086 to observe. Name bound `0-255` as §0E's.
  Section `16` was added to `items.json` with `complete: false` until TASK-AKM-087.

---

### TASK-AKM-085: Set lists
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue and implement `&20`, `&21`, `&22`, `&23` (the index then the name, built by hand) as
  primitives, and model them in the simulated sampler (seeded by `setSetListNames`).
- **Requirement refs**: RQ-AKM-084
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-084.
- **Dependencies**: TASK-AKM-084
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean build (`/W4 /WX`), `ctest --test-dir juce/build -C Debug` 631/631
  passed after a full re-run in this session (624 before, 7 new in `SetListPrimitivesTests.cpp`, written before
  the code): count and names `SET1`, `SET2` in order with an index past the end failing ERROR 04; rename at index 1
  sends `00 01 53 45 54 33 00` (two index bytes, then `SET3` null-terminated), completes on DONE and reads back
  `SET3` with index 0 untouched; delete at index 0 sends `00 00`, leaves a count of 1 and index 0 naming what was
  at index 1; delete and rename of an index with no set list fail ERROR 04 and change nothing; an index of 16384
  or -1 is refused `ArgumentOutOfRange` and a non-ASCII name `NotEncodable`, with nothing sent; an empty memory
  counts 0 on a REPLY; the name Get is refused `ChecksumModeUnknown` with nothing sent. `generate_akm_items.py
  --check`: up to date (318 items); `--coverage`: `unaccounted: none` (`&23`'s args not compared, the spec row
  being variable-length). `ItemCatalogueTests.cpp`'s count formula extended by the 4 records. Not verified: the
  real sampler (TASK-AKM-086).
- **Assumptions**: `SetListCountResult` and `SetListNameResult` are aliases of the song file results, a count and
  a name being the same thing, rather than parallel types. `renameSetList` checks the index range itself (0-16383),
  the generic encoder not being used for `&23`. As for song files, ERROR `04` for an index with no set list is the
  simulated sampler's modelling choice, the spec being silent.

---

### TASK-AKM-086: Real-sampler check of the song files and set lists
- **Tier**: L
- **Status**: Done
- **Description**: Add `--song-files` to `xs56k_akm_probe --suite`: a check that reads the counts and names,
  round-trips selection by index and by name, renames the first song file and the first set list and puts every
  name and the selection back, on every exit path; run it on the real sampler.
- **Requirement refs**: RQ-AKM-085
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008 precedent for the suite's opt-in flags)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-085, on the simulated sampler in `ctest` and
  on the real sampler.
- **Dependencies**: TASK-AKM-084, TASK-AKM-085
- **Assignee**: AI, running the real-sampler check under the owner's standing authorization
- **Verification**: Windows/MSVC Debug: clean build (`/W4 /WX`), `ctest --test-dir juce/build -C Debug` 636/636
  passed after a full re-run in this session (631 before, 5 new in `RealSamplerSuiteTests.cpp`, `ctest -R
  RQ-AKM-085` 5/5): with song files and set lists seeded (one song file current) both checks pass, every name and the
  selection are back, and no `&08`/`&22` is sent; with none, both checks are skipped, nothing is renamed or deleted
  and the empty-memory answers are logged; the check made to fail after a rename has the name and the selection back;
  a set list alone is renamed and put back with the song file check skipped; without `--song-files` no section 16 item
  is sent. Real sampler (S5000, OS 2.14), `xs56k_akm_probe --suite --song-files --in "MIDIIN2 (ESI M8U eX)" --out
  "MIDIOUT15 (ESI M8U eX)"`, run twice this session (`akm-suite-20261004-130744.log`, `-130836.log`): 7 automatic checks
  passed, the sampler left in the known state; the two checks skipped because the sampler holds no song file and no set
  list; `&10` and `&20` answered REPLY `00 00`, and `&13`, `&11`, `&21`, `&06`, `&05` answered ERROR 4 with nothing in
  memory (`process/2.architecture/OBSERVATIONS-RQ-AKM-085-song-files.md`). Not verified on hardware: the REPLYs of the
  Gets for items that exist, selection, renaming and the restoration — the sampler held nothing, and §16 cannot create
  a song file; the owner is to run `--song-files` again once one is loaded. `AGENTS.md`, `CHANGELOG.md` for TASK-AKM-087.
- **Assumptions**: Reading the counts is not "nothing sent": a skipped check has read `&10`, `&20` and `&13` to know it is
  skipped, and (empty memory only) tried the four read and select items above for the observations; RQ-AKM-085's
  wording was adjusted accordingly. The check renames only a song file or set list whose name it has just read
  and puts it back; it reads at most 16 names of each kind (`SONG_FILES_NAME_READ_LIMIT`). A selection cannot be cleared:
  with no song file current before the check, the one the check chose stays current, which the log says. The two checks
  need nothing from the owner. A real-sampler run holding nothing was accepted as the closure of the harness itself,
  the same way `--front-panel` was closed on the simulated sampler before its owner run (TASK-AKM-073).

---

### TASK-AKM-087: Coverage of section §16
- **Tier**: M
- **Status**: Done
- **Description**: Run `generate_akm_items.py --coverage` for section `16`, list any exclusion with its reason,
  flip the section's `complete` flag in `items.json`, and update `SUMMARY-akm-sections-coverage.md`, `AGENTS.md`
  (the new probe option) and `CHANGELOG.md`.
- **Requirement refs**: RQ-AKM-086
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-086.
- **Dependencies**: TASK-AKM-084 to TASK-AKM-086
- **Assignee**: AI
- **Verification**: `generate_akm_items.py --coverage` re-run this session after the flag flip: `section 16: 12 of 12
  spec rows covered (MIDI Song Files, complete)`, `unaccounted: none`, exit 0 — no row excluded, nothing left to give a
  reason for; `--check`: up to date (318 items); the three catalogue `ctest` entries (`akm_item_*`,
  `akm_public_headers_have_no_juce_include`) pass after the flip; the full `ctest` was 636/636 just before it, with the
  flag the only change of code since. `items.json`'s section-16 `complete` flipped `false` → `true`, its note closed
  out. No erratum beyond the two cosmetic ones already in `sysex_spec.kb.md` (lines 192-193: Table 29's title says
  `§&14{20}`, Table 28's sub-group header is a leftover): both recorded, neither affects the wire.
  `SUMMARY-akm-sections-coverage.md`: §16's bar to 100 %, the total to 470/560 (84 %) and a note of what the real
  sampler did and did not show; `AGENTS.md`: `--song-files` documented; `CHANGELOG.md`: one `[Unreleased]` entry for
  the whole lot.
- **Assumptions**: `complete` is flipped on the strength of the catalogue matching the spec's rows, as for §20 and §04
  before it, not on a real-sampler run that exercised everything: the sampler held no song file or set list (see
  TASK-AKM-086).

---

### TASK-AKM-106: The song files check puts back by name, the sampler sorting its song files
- **Tier**: M
- **Status**: Done
- **Description**: The real S5000 (OS 2.14) sorts its song files by name when one is renamed, without regard to case, and answered
  its first song file as current after it (TASK-MCP-058, 2026-10-10). `GuardedSongFiles` puts a name back by the position the song
  file had, and `songFileRoundTrips` reads the new name at the current song file and at position 0: on a sampler holding song files,
  the check would rename another song file than the one it changed. The simulated sampler sorts its song files on a renaming as the
  real one does (and makes the first current); the guard finds the renamed song file and the renamed set list by the test name, puts
  their names back and selects again, by name, the song file that was current; the round trip looks for the new name in the list.
- **Requirement refs**: RQ-AKM-085, RQ-AKM-082, RQ-AKM-084, RQ-AKM-016
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008)
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding song files sorted by name, the second current, and two set
  lists, *When* the suite runs with `--song-files`, *Then* both checks pass, every name and the current song file are what they were,
  and nothing is deleted; *Given* the song files of the real run ("a_woman_in_love", "AIMER", "alors_regarde", "assassine", "AIMER"
  current), *When* the song files checks run, *Then* the song file they renamed has its name back, every song file is at its place, and "AIMER" is
  current again.
- **Dependencies**: TASK-AKM-086
- **Assignee**: AI
- **Verification**: Test first: with the simulated sampler sorting on a renaming, `xs56k_akm_tests "[song]"` 3 of 13 cases failed (11
  assertions; the old guard left "CHORUS", "XS56K TEST", "XS56K TEST": it put the name back on another song file). After the guard and
  the round trip find the renamed item by the test name: 13 of 13 cases, 627 assertions, the real-run case included ("AIMER" current
  again, every name at its place, no deletion); full `ctest` on Windows (MSVC, Debug) 1085 of 1085, no warning. The guard renames back
  only the song file it finds by the test name, and logs that nothing was renamed back when none bears it.
- **Assumptions**: (1) The real sampler always holds its song files sorted, so the seeds of the probe's tests are sorted
  ({"VERSE", "CHORUS", "BRIDGE"} becomes {"BRIDGE", "CHORUS", "VERSE"}): a test change that follows the observed behaviour.
  (2) Whether the set lists and the scenelists are sorted too is not known (one set list on the sampler, the scenelists not yet
  renamed): the simulated sampler keeps them as they are, the guard of the set lists finds its set list by name all the same, and the
  scenelists check is left for TASK-MCP-058 to show.
