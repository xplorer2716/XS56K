# PLAN-AKM-006: System Setup Primitives (Phase A, residual §02)

## Overview

Plan for FTR-AKM-006, the 14 items of section §02 (System setup) that `PLAN-AKM-001` did not cover
(`&00`/`&01`, the operating-system version, are done under `RQ-AKM-044`): sampler name `&02`/`&03`,
model `&04`, clock and date `&05`/`&06`, Play Mode `&10`/`&20`, front-panel lock `&11`/`&21`, Wave
memory `&30`/`&33`/`&34`, MPKS memory `&31` and the destructive Clear Sampler Memory `&32` — each
proven by a Set followed by a Get (or a Get alone) on the simulated sampler, then on the real sampler.

Like §0E and unlike §0A/§08/§06, §02 has no "current item" state: every item is sampler-wide.

**Prerequisite check.** `ValueFormat::String` (`DEC-AKM-013`) covers the name; `Byte` the model,
Play Mode, lock and memory percentages; the clock needs an eight-byte request and REPLY, decoded through
the existing generic multi-field path (`PLAN-AKM-005` did the same for `&34`/`&4B`); `&33`/`&34` return a
compound double word (`ValueFormat::Dword`, declared but unused so far — `TASK-AKM-049` decides whether
it fits or whether the §0E split into `Byte` values is repeated). **No new ADR file is opened for this
lot**; revisited only if the clock or the double word do not fit the catalogue.

**Safety note.** `&32` empties the sampler (`RQ-AKM-056`): it gets the `RQ-AKM-025`/`RQ-AKM-046` guard
and is audited out of every real-sampler test. `&02`, `&06`, `&10` and `&11` change settings the owner
sees (name, time, sound routing, panel) — every real-sampler check restores them, and a locked front
panel is never left behind (`RQ-AKM-058`).

## References
- **Requirements**: FTR-AKM-006 (RQ-AKM-052 to RQ-AKM-058); RQ-AKM-044 (already delivered)
- **ADRs**: ADR-AKM-001 (Accepted) — extended by catalogue growth only (`DEC-AKM-003`, `DEC-AKM-011`,
  `DEC-AKM-012`, `DEC-AKM-013`); no new ADR file, no new `DEC-AKM-*` expected.

The plan has 8 tasks (TASK-AKM-047 to TASK-AKM-054): 047 authors the artifacts; 048 to 052 deliver the
primitives, one per requirement (each independent of the others, all after 047); 053 is the
real-sampler harness (depends on 048, 050, 051, 052); 054 closes the coverage (depends on 048 to 053).

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-AKM-047: Author FTR-AKM-006 and PLAN-AKM-006
- **Tier**: S
- **Status**: Done
- **Description**: Write the feature file and this plan for the 14 residual §02 items, from the spec's
  own row counts.
- **Requirement refs**: RQ-AKM-052, RQ-AKM-053, RQ-AKM-054, RQ-AKM-055, RQ-AKM-056, RQ-AKM-057,
  RQ-AKM-058
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* the 27 rows of section `02`, *When* the feature is read,
  *Then* each of the 23 rows not yet covered falls under exactly one requirement.
- **Dependencies**: None
- **Assignee**: AI, with the owner's approval (DoR, given 2026-10-01)
- **Verification**: N/A (Tier S)
- **Assumptions**: None.

---

### TASK-AKM-048: Sampler name (Set and Get)
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue `&02` (Set, `String`, completes on DONE) and `&03` (Get, REPLY `String`) and
  expose a typed primitive reusing `makeStringRequest`; extend the simulated sampler with a name.
- **Requirement refs**: RQ-AKM-052
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-052 on the simulated sampler —
  frame bytes `53 54 55 44 49 4F 00` for `STUDIO`, Get returns what was set.
- **Dependencies**: TASK-AKM-047
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: build with no warning or error (`/W4 /WX`), `ctest` 459/459 after
  a re-run in this session. New `SystemSetupTests.cpp` (3 cases, `[akm][system]`, run standalone with
  `ctest -R RQ-AKM-052`): the factory name `AKAI S5000` is read, `STUDIO` is set with frame data bytes
  `53 54 55 44 49 4F 00` and read back; a 21-character name is refused `ArgumentOutOfRange` and a non-ASCII
  one `NotEncodable`, nothing sent; with the checksum mode unknown the Get is refused `ChecksumModeUnknown`,
  nothing sent. Collateral: `ItemCatalogueTests.cpp`'s `CATALOGUE` table gained the two new items, and its
  lookup test, which used `&02` of §02 as its example of an uncatalogued item, now uses `&07` (no such item
  in the spec) and `&0A` — an edit reflecting the new expected state, no failing assertion weakened.
  `generate_akm_items.py --check`: up to date (248 items); `--coverage`: section `02` 4 of 16 command rows
  covered, `unaccounted: none`. Not verified: real sampler (TASK-AKM-053); mutation testing (RQ-BLD-015 /
  TASK-BLD-012 tooling exists but was not run on this change).
- **Assumptions**: A name is catalogued with the 20-character maximum of the other name fields — the spec
  states none (`sysex_spec.kb.md`, "Common value codes") and the real limit of this field is observed by
  TASK-AKM-053. The simulated sampler truncates to 20 on store, as it models the S5000's program-name
  limit, and keeps the name across `powerCycle()` (a stored setting, unlike the §00 flags) — modelling
  choices, not proven on hardware. The primitives live in a new `SystemSetup.hpp`/`.cpp` that the later
  tasks of this plan extend, rather than in `SystemVersion.hpp`, which is the OS version's alone.

---

### TASK-AKM-049: Sampler model and available memory (Get)
- **Tier**: M
- **Status**: Not Started
- **Description**: Catalogue `&04`, `&30`, `&31`, `&33`, `&34` and expose typed Gets: the model as an
  enumeration (a byte other than `0`/`1` is malformed), the two percentages, and the two byte counts decoded
  from the compound double word. Settle `Dword` against the four-`Byte` split of §0E.
- **Requirement refs**: RQ-AKM-053
- **ADR refs**: ADR-AKM-001 (DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-053 on the simulated sampler.
- **Dependencies**: TASK-AKM-047
- **Assignee**: AI
- **Verification**: to be filled at closure.
- **Assumptions**: None yet.

---

### TASK-AKM-050: Clock and date (Set and Get)
- **Tier**: M
- **Status**: Not Started
- **Description**: Catalogue `&05` and `&06` and expose a typed `ClockDate` with field-range refusal
  before sending (year 1980–2079, month 1–12, day 1–31, weekday 1–7, hours 0–23, minutes and seconds
  0–59).
- **Requirement refs**: RQ-AKM-054
- **ADR refs**: ADR-AKM-001 (DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-054 on the simulated sampler.
- **Dependencies**: TASK-AKM-047
- **Assignee**: AI
- **Verification**: to be filled at closure.
- **Assumptions**: None yet.

---

### TASK-AKM-051: Play Mode and front-panel lock (Set and Get)
- **Tier**: M
- **Status**: Not Started
- **Description**: Catalogue `&10`/`&20` and `&11`/`&21`, expose typed Set/Get, refuse out-of-range
  values before sending; Play Mode accepts `0`–`3` as the item text defines (`3 = Muted`), the data range
  column's "0, 1, 2" being the known erratum, pending the real sampler (TASK-AKM-054).
- **Requirement refs**: RQ-AKM-055
- **ADR refs**: ADR-AKM-001 (DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-055 on the simulated sampler.
- **Dependencies**: TASK-AKM-047
- **Assignee**: AI
- **Verification**: to be filled at closure.
- **Assumptions**: None yet.

---

### TASK-AKM-052: Destructive command guard for "Clear Sampler Memory"
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement `&32` so that it is sent only with an explicit confirmation argument no
  default supplies (a dedicated type, mirroring `TASK-AKM-023`/`TASK-AKM-041`) and audit the real-sampler
  test sources for any call.
- **Requirement refs**: RQ-AKM-056
- **ADR refs**: ADR-AKM-001 (DEC-AKM-011)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-056.
- **Dependencies**: TASK-AKM-047
- **Assignee**: AI, with the owner's approval (DoR)
- **Verification**: to be filled at closure.
- **Assumptions**: Tier M rather than S: a new public type and file (`TASK-AKM-041` was S because it only
  specialised an existing shape).

---

### TASK-AKM-053: Real-sampler harness — system setup restored
- **Tier**: L
- **Status**: Not Started
- **Description**: Add a check to `xs56k_akm_probe --suite` that round-trips the name, Play Mode, front-panel
  lock and clock on the real sampler under a guard that restores each value (clock advanced by the elapsed
  time) even when a check throws, never leaves the panel locked, never calls `&32`, and is opt-in through its
  own flag like `--sample-lifecycle`.
- **Requirement refs**: RQ-AKM-058
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-058, on the simulated sampler in
  `ctest` and on the real sampler run by the owner.
- **Dependencies**: TASK-AKM-048, TASK-AKM-050, TASK-AKM-051, TASK-AKM-052
- **Assignee**: AI, with the owner running the real-sampler suite
- **Verification**: to be filled at closure.
- **Assumptions**: None yet.

---

### TASK-AKM-054: Coverage of section §02 and errata resolution
- **Tier**: M
- **Status**: Not Started
- **Description**: Run `generate_akm_items.py --coverage` for section `02`, list any exclusion with its
  reason, resolve the `&10` Play Mode range erratum on the real sampler, and update
  `SUMMARY-akm-sections-coverage.md` and `AGENTS.md` if the suite's options changed.
- **Requirement refs**: RQ-AKM-057
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-057.
- **Dependencies**: TASK-AKM-048, TASK-AKM-049, TASK-AKM-050, TASK-AKM-051, TASK-AKM-052, TASK-AKM-053
- **Assignee**: AI, with the owner running the real-sampler observation
- **Verification**: to be filled at closure.
- **Assumptions**: None yet.
