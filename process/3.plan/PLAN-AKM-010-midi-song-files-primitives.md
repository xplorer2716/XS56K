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

The plan has 5 tasks (TASK-AKM-083 to TASK-AKM-087): 083 authors the artifacts; 084 delivers the song file items;
085 the set list items (after 084, which adds the section to the simulated sampler); 086 is the real-sampler check
(after 084, 085); 087 closes the coverage (after 084 to 086).

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
- **Status**: Not Started
- **Description**: Catalogue and implement `&05`, `&06`, `&08`, `&09`, `&10`, `&11`, `&13`, `&14` as primitives
  in `SongPrimitives`, and model them in the simulated sampler (seeded by `setSongNames`).
- **Requirement refs**: RQ-AKM-082, RQ-AKM-083
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-082 and RQ-AKM-083.
- **Dependencies**: TASK-AKM-083
- **Assignee**: AI
- **Verification**: (filled at closure)
- **Assumptions**: (filled at closure)

---

### TASK-AKM-085: Set lists
- **Tier**: M
- **Status**: Not Started
- **Description**: Catalogue and implement `&20`, `&21`, `&22`, `&23` (the index then the name, built by hand) as
  primitives, and model them in the simulated sampler (seeded by `setSetListNames`).
- **Requirement refs**: RQ-AKM-084
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-084.
- **Dependencies**: TASK-AKM-084
- **Assignee**: AI
- **Verification**: (filled at closure)
- **Assumptions**: (filled at closure)

---

### TASK-AKM-086: Real-sampler check of the song files and set lists
- **Tier**: L
- **Status**: Not Started
- **Description**: Add `--song-files` to `xs56k_akm_probe --suite`: a check that reads the counts and names,
  round-trips selection by index and by name, renames the first song file and the first set list and puts every
  name and the selection back, on every exit path; run it on the real sampler.
- **Requirement refs**: RQ-AKM-085
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008 precedent for the suite's opt-in flags)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-085, on the simulated sampler in `ctest` and
  on the real sampler.
- **Dependencies**: TASK-AKM-084, TASK-AKM-085
- **Assignee**: AI, running the real-sampler check under the owner's standing authorization
- **Verification**: (filled at closure)
- **Assumptions**: (filled at closure)

---

### TASK-AKM-087: Coverage of section §16
- **Tier**: M
- **Status**: Not Started
- **Description**: Run `generate_akm_items.py --coverage` for section `16`, list any exclusion with its reason,
  flip the section's `complete` flag in `items.json`, and update `SUMMARY-akm-sections-coverage.md`, `AGENTS.md`
  (the new probe option) and `CHANGELOG.md`.
- **Requirement refs**: RQ-AKM-086
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-086.
- **Dependencies**: TASK-AKM-084 to TASK-AKM-086
- **Assignee**: AI
- **Verification**: (filled at closure)
- **Assumptions**: (filled at closure)
