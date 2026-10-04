# PLAN-AKM-013: Multi FX Primitives (Phase A, new lot, §12)

## Overview

Implements `FTR-AKM-013`: one tested primitive per command row of section `12` (11 commands, 7 REPLY formats — 18
rows), and a real-sampler check that creates a test multi, reads what the sampler answers about its FX board and,
when a board is installed, round-trips the items and puts every value back.

Section `12` acts on the current multi (§0C). Its shapes need nothing new in the codec: channel, module, parameter
index and flags are `Byte` values, a REPLY is one data byte (three for `&51`), and a parameter value is the
`signed_word` format, already in the catalogue's value formats and the codec, used here by an item for the first
time. The module type takes the codes of Table 24, named by an enumeration as the number of parts of new multis is.

**Safety note.** The owner's sampler has no FX board, so the Sets can only be tested on the simulated sampler; the
real-sampler check sends none when `&01` says "none". With a board it changes only the FX of a test multi the check
creates and deletes, and puts each value back (`RQ-AKM-102`).

## References
- **Requirements**: RQ-AKM-099 to RQ-AKM-103 (`FTR-AKM-013`)
- **ADRs**: ADR-AKM-001 (Accepted): DEC-AKM-003, DEC-AKM-012, DEC-AKM-013. No new decision is expected.

The plan has 6 tasks (TASK-AKM-100 to TASK-AKM-105): 100 authors the artifacts; 101 delivers the discovery Gets and
the model of the section in the simulated sampler; 102 the channel and module items (after 101); 103 the parameter
values (after 101); 104 is the real-sampler check (after 101 to 103); 105 closes the coverage (after 101 to 104).

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-AKM-100: Author FTR-AKM-013 and PLAN-AKM-013
- **Tier**: S
- **Status**: Done
- **Description**: Write the feature file and this plan for section `12`, from the spec's Figure 2 and Tables 22 to 25.
- **Requirement refs**: RQ-AKM-099, RQ-AKM-100, RQ-AKM-101, RQ-AKM-102, RQ-AKM-103
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* section `12` of the spec, *When* the feature file is read, *Then* each of its 18 rows is the subject of a requirement.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: N/A (Tier S). Both files written this session; `agnos-index` re-run after them.
- **Assumptions**: The owner's instruction to continue with §14 then §12 (session AKM, 2026-10-04) stands in for the DoR approval of each task of the plan. The owner said the sampler holds no EB20 card, so no Set of the section can be tested on hardware.

---

### TASK-AKM-101: FX board and layout discovery, and the section in the simulated sampler
- **Tier**: M
- **Status**: Not Started
- **Description**: Catalogue and implement `&01`, `&10`, `&11` as primitives in `MultiFxPrimitives`, and model section
  `12` in the simulated sampler: the card, the layout of channels and modules (seeded by `setFxLayout`, an EB20 laid out
  as Figure 2 by default of the tests that want one) and the answers of a sampler with no board.
- **Requirement refs**: RQ-AKM-099
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-099.
- **Dependencies**: TASK-AKM-100
- **Assignee**: AI
- **Verification**: (to fill at closure)
- **Assumptions**: (to fill at closure)

---

### TASK-AKM-102: Channel mute, module type and module state
- **Tier**: M
- **Status**: Not Started
- **Description**: Catalogue and implement `&20`, `&21`, `&30`, `&31`, `&40`, `&41` and the `FxModuleType` enumeration
  of Table 24, and model them in the simulated sampler.
- **Requirement refs**: RQ-AKM-100
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-100.
- **Dependencies**: TASK-AKM-101
- **Assignee**: AI
- **Verification**: (to fill at closure)
- **Assumptions**: (to fill at closure)

---

### TASK-AKM-103: FX parameter values
- **Tier**: M
- **Status**: Not Started
- **Description**: Catalogue and implement `&50`, `&51` (the first items to use the `signed_word` value format) and
  model them in the simulated sampler.
- **Requirement refs**: RQ-AKM-101
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-101.
- **Dependencies**: TASK-AKM-101
- **Assignee**: AI
- **Verification**: (to fill at closure)
- **Assumptions**: (to fill at closure)

---

### TASK-AKM-104: Real-sampler check of the Multi FX
- **Tier**: L
- **Status**: Not Started
- **Description**: Add `--multi-fx` to `xs56k_akm_probe --suite`: a check that creates a test multi, reads the board
  and, with none, logs what the other Gets answer and is skipped; with one, round-trips the mute of a channel, the
  state of a module, the type of a changeable module and a parameter, putting each back; on every exit path the test
  multi is deleted and the current multi selected again. Run it on the real sampler (no board).
- **Requirement refs**: RQ-AKM-102
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008 precedent for the suite's opt-in flags)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-102, on the simulated sampler in `ctest` and on the
  real sampler (the empty-board answers only).
- **Dependencies**: TASK-AKM-101, TASK-AKM-102, TASK-AKM-103
- **Assignee**: AI, running the real-sampler check under the owner's standing authorization
- **Verification**: (to fill at closure)
- **Assumptions**: (to fill at closure)

---

### TASK-AKM-105: Coverage of section §12
- **Tier**: M
- **Status**: Not Started
- **Description**: Run `generate_akm_items.py --coverage` for section `12`, list any exclusion with its reason,
  flip the section's `complete` flag in `items.json`, and update `SUMMARY-akm-sections-coverage.md`, `AGENTS.md`
  (the new probe option) and `CHANGELOG.md`.
- **Requirement refs**: RQ-AKM-103
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-103.
- **Dependencies**: TASK-AKM-101 to TASK-AKM-104
- **Assignee**: AI
- **Verification**: (to fill at closure)
- **Assumptions**: (to fill at closure)
