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
- **Status**: Not Started
- **Description**: Catalogue and implement `&05`, `&06`, `&08`, `&09`, `&10`, `&11`, `&13`, `&14` as primitives
  in `SceneListPrimitives`, and model them in the simulated sampler (seeded by `setSceneListNames`).
- **Requirement refs**: RQ-AKM-095, RQ-AKM-096
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-095 and RQ-AKM-096.
- **Dependencies**: TASK-AKM-096
- **Assignee**: AI
- **Verification**: (to fill at closure)
- **Assumptions**: (to fill at closure)

---

### TASK-AKM-098: Real-sampler check of the scenelists
- **Tier**: L
- **Status**: Not Started
- **Description**: Add `--scenelists` to `xs56k_akm_probe --suite`: a check that reads the count and names,
  round-trips selection by index and by name, renames the first scenelist and puts the name and the selection
  back, on every exit path; run it on the real sampler.
- **Requirement refs**: RQ-AKM-097
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008 precedent for the suite's opt-in flags)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-097, on the simulated sampler in `ctest` and
  on the real sampler.
- **Dependencies**: TASK-AKM-097
- **Assignee**: AI, running the real-sampler check under the owner's standing authorization
- **Verification**: (to fill at closure)
- **Assumptions**: (to fill at closure)

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
