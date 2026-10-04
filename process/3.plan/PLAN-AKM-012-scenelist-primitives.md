# PLAN-AKM-012: Scenelist Primitives (Phase A, new lot, §14)

## Overview

Implements `FTR-AKM-012`: one tested primitive per command row of section `14` (8 commands, 4 REPLY formats —
12 rows), and a real-sampler check that reads what the sampler holds and puts back every name it changes.

Section `14` has exactly the shape of the song file half of §16 (`PLAN-AKM-010`): a sampler-wide "current
scenelist" selection state (`&05`, `&06`, `&08`, `&09`, `&13`, `&14`) and by-index Gets (`&10`, `&11`). No new
value format and no hand-built request is needed: indexes are two `Byte` values, names are `String`
(DEC-AKM-013). The results are the song file ones (aliases), as the set lists' are.

**Safety note.** A scenelist cannot be created through §14, so the real-sampler check works on what the sampler
holds: it never deletes, renames only to put back, and restores the selection it found (`RQ-AKM-097`). The deletion
primitive is tested on the simulated sampler only.

## References
- **Requirements**: RQ-AKM-095 to RQ-AKM-098 (`FTR-AKM-012`)
- **ADRs**: ADR-AKM-001 (Accepted): DEC-AKM-003, DEC-AKM-012, DEC-AKM-013. No new decision is expected.

The plan has 4 tasks (TASK-AKM-096 to TASK-AKM-099): 096 authors the artifacts; 097 delivers the eight items and
their model in the simulated sampler; 098 is the real-sampler check (after 097); 099 closes the coverage (after
097, 098).

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-AKM-096: Author FTR-AKM-012 and PLAN-AKM-012
- **Tier**: S
- **Status**: Done
- **Description**: Write the feature file and this plan for section `14`, from the spec's Tables 26 and 27.
- **Requirement refs**: RQ-AKM-095, RQ-AKM-096, RQ-AKM-097, RQ-AKM-098
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* section `14` of the spec, *When* the feature file is read, *Then* each of its 12 rows is the subject of a requirement.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: N/A (Tier S). Both files written this session; `agnos-index` re-run after them.
- **Assumptions**: The owner's instruction to continue with §14 then §12 (session AKM, 2026-10-04) stands in for the DoR approval of each task of the plan, as the autonomy grant did for PLAN-AKM-010 and PLAN-AKM-011.

---

### TASK-AKM-097: Scenelist selection, renaming, deletion and general information
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue and implement `&05`, `&06`, `&08`, `&09`, `&10`, `&11`, `&13`, `&14` as primitives
  in `SceneListPrimitives`, and model them in the simulated sampler (seeded by `setSceneListNames`).
- **Requirement refs**: RQ-AKM-095, RQ-AKM-096
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-095 and RQ-AKM-096.
- **Dependencies**: TASK-AKM-096
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean build (`/W4 /WX`), `ctest --test-dir juce/build -C Debug` 682/682
  passed after a full re-run in this session (674 before, 8 new in `SceneListPrimitivesTests.cpp`, written before the
  code, `ctest -R "RQ-AKM-09[56]"` 8/8): select by name sends section `14`, item `05` and `53 43 45 4E 45 31 00`,
  then rename (`09`) round-trips through `&14`; a name or an index with no scenelist fails ERROR 04; select by index
  (`06`, frame `00 00`), delete (`08`), then the count is one less, no scenelist is current and index 0 names what
  was `B`; index 130 goes on the wire as `01 02`; delete, rename and the two current Gets fail ERROR 04 with none
  current; count and name by index read `A`, `B`, `C` in order, an index past the end fails ERROR 04, reading by
  index leaves the selection alone; an empty memory counts 0 on a REPLY; both name Gets are refused
  `ChecksumModeUnknown` with nothing sent. `generate_akm_items.py --check`: up to date (373 items).
  `ItemCatalogueTests.cpp`'s count formula extended by the 8 records. Not verified: the real sampler
  (TASK-AKM-098).
- **Assumptions**: The spec is silent on what §14 answers with no scenelist current or an unknown name or index: the
  simulated sampler answers ERROR `04` as §16 does (a modelling choice). To model it without duplicating §16's
  code, `executeSongFiles`'s eight song file cases became `executeCurrentNamedList`, shared by §16 and §14 (the
  codes are the same; its constants were renamed `ITEM_LIST_*`), with no change of behaviour: the song file tests
  still pass unmodified. The result types are aliases of the song file ones (as the set lists' are), so
  `SceneListPrimitives.hpp` includes `SongPrimitives.hpp`. Name bound `0-255` as §16's. Section `14` was added to
  `items.json` with `complete: false` until TASK-AKM-099.

---

### TASK-AKM-098: Real-sampler check of the scenelists
- **Tier**: L
- **Status**: Done
- **Description**: Add `--scenelists` to `xs56k_akm_probe --suite`: a check that reads the count and names,
  round-trips selection by index and by name, renames the first scenelist and puts the name and the selection
  back, on every exit path; run it on the real sampler.
- **Requirement refs**: RQ-AKM-097
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008 precedent for the suite's opt-in flags)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-097, on the simulated sampler in `ctest` and
  on the real sampler.
- **Dependencies**: TASK-AKM-097
- **Assignee**: AI, running the real-sampler check under the owner's standing authorization
- **Verification**: Windows/MSVC Debug: clean build (`/W4 /WX`), `ctest -R "RQ-AKM-09[5-7]"` 13/13 (8 of TASK-AKM-097
  and 5 new in `RealSamplerSuiteTests.cpp`, written before the code): with scenelists seeded (the third current) both
  checks pass, the name and the selection are back and no `&08` is sent; with none, both are skipped, nothing is
  renamed or deleted and the empty-memory answers are logged (`select scenelist 0, none held`); the check made to
  fail after a rename has the name and the selection back; a sampler refusing `&10` as not supported fails the first
  check naming `could not read the number of scenelists (&10)` with the known state restored; without `--scenelists`
  no section 14 item is sent. Real sampler (S5000, OS 2.14), `xs56k_akm_probe --suite --scenelists --yes --in "MIDIIN2
  (ESI M8U eX)" --out "MIDIOUT15 (ESI M8U eX)"` (`akm-suite-20261004-165127.log`): 7 automatic checks passed, the
  sampler left in the known state; the two checks skipped because the sampler holds no scenelist; `&10` answered REPLY
  `00 00`, and `&13`, `&11`, `&06`, `&05` answered ERROR 4 (`process/2.architecture/OBSERVATIONS-RQ-AKM-097-scenelist.md`).
  Full `ctest` re-run in this session: 687/687 passed (682 + 5). Not verified on hardware: anything on a scenelist that exists.
- **Assumptions**: As for §16 (TASK-AKM-086): a real-sampler run holding nothing was accepted as the closure of the
  harness itself, and reading the count and the current index is not "nothing sent". The check renames only a scenelist
  whose name it has just read, reads at most 16 names (`SCENE_LIST_NAME_READ_LIMIT`), and cannot clear a selection: with
  no scenelist current before the check, the one it chose stays current, which the log says. The check was written
  beside the song file one rather than generalising it, the two having different types and an unproven second user
  being less valuable than leaving the shipped check untouched.

---

### TASK-AKM-099: Coverage of section §14
- **Tier**: M
- **Status**: Not Started
- **Description**: Run `generate_akm_items.py --coverage` for section `14`, list any exclusion with its reason,
  flip the section's `complete` flag in `items.json`, and update `SUMMARY-akm-sections-coverage.md`, `AGENTS.md`
  (the new probe option) and `CHANGELOG.md`.
- **Requirement refs**: RQ-AKM-098
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-098.
- **Dependencies**: TASK-AKM-097, TASK-AKM-098
- **Assignee**: AI
- **Verification**: (to fill at closure)
- **Assumptions**: (to fill at closure)
