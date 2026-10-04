# PLAN-AKM-009: MIDI Configuration Primitives (Phase A, new lot, §04)

## Overview

Implements `FTR-AKM-009`: one tested primitive per command row of section `04` (7 commands, no REPLY format —
7 rows) and an opt-in real-sampler check guided by the owner.

Like §02, §10 and §20, §04 has no "current item" state, and like §20 it has no Get and no REPLY: each command
completes on DONE, so the primitives are proven by what the simulated sampler records having received, not by a
read-back. Unlike §20, what a §04 command sets is the owner's stored MIDI configuration, which the AKM layer can
neither read nor restore on its own (`FTR-AKM-009`, "What is new compared with §20"). The real-sampler check
therefore starts from values the owner declares and puts them back (`RQ-AKM-080`).

**Prerequisite check.** The catalogue already carries Set items completing on DONE with one or two byte arguments
(§02 `&10`, §20 `&03`); no new value format is needed. The suite already has `askOwner` (yes/no) and
`askOwnerChoice` (one of a list) as owner-input seams (`RealSamplerSuite.hpp`), which cover the declarations of the
check; no new seam is planned (to be confirmed in the DoR of `TASK-AKM-079`).

**Safety note.** Every §04 command changes stored configuration (`FTR-AKM-009`, real-hardware risk). The only
real-sampler check is opt-in (`--midi-config`), sends nothing before the owner has declared the values to restore,
and puts each setting back on every exit path (`RQ-AKM-080`).

## References
- **Requirements**: RQ-AKM-078 to RQ-AKM-081 (`FTR-AKM-009`)
- **ADRs**: ADR-AKM-001 (Accepted) — no new decision is planned; if `TASK-AKM-079` needs one (a restore policy that
  is cross-cutting), it is added to the existing file as `DEC-AKM-020`, like `DEC-AKM-012` to `019` before it, not
  as a new ADR document.

The plan has 5 tasks (TASK-AKM-076 to TASK-AKM-080): 076 authors the artifacts; 077 and 078 deliver the primitives
(independent, both after 076); 079 is the real-sampler harness (after 077 and 078); 080 closes the coverage (after
077 to 079).

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-AKM-076: Author FTR-AKM-009 and PLAN-AKM-009
- **Tier**: M
- **Status**: Done
- **Description**: Write the feature file and this plan for the seven items of section `04`, from the spec's own
  row counts and the owner's decisions of this session.
- **Requirement refs**: RQ-AKM-078, RQ-AKM-079, RQ-AKM-080, RQ-AKM-081
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* the 7 rows of section `04`, *When* the feature is read, *Then* each
  row falls under exactly one of RQ-AKM-078 (`&01` to `&05`) and RQ-AKM-079 (`&06`, `&07`). *Given* the owner's
  decision on the real-sampler check (an owner-guided opt-in that restores the values the owner declares), *When*
  the feature is read, *Then* it is a requirement (RQ-AKM-080).
- **Dependencies**: None
- **Assignee**: AI, with the owner's approval (DoR, given 2026-10-04)
- **Verification**: `agnos-index` re-run in this session: 239 entries, 40 documents, exit 0, no duplicate ID;
  `FTR-AKM-009` (RQ-AKM-078 to 081) and `PLAN-AKM-009` (TASK-AKM-076 to 080) are indexed, `#next` moved to
  `FTR-AKM-010 RQ-AKM-082 PLAN-AKM-010 TASK-AKM-081`. Section `04` rows re-read from the spec text (Table 8,
  `.pdf.md` lines 547-570) and `sysex_spec.items.tsv` (lines 36-42): 7 commands `&01`-`&07`, no REPLY row —
  `&01`-`&05` under RQ-AKM-078, `&06`/`&07` under RQ-AKM-079. Value ranges and the MIDI SETUP / MIDI FILTER
  descriptions re-read in the operator's manual (`.pdf.md` lines 5422-5472). The owner's decision on the
  real-sampler check is RQ-AKM-080. No code changed; nothing sent to hardware.
- **Assumptions**: Tier M, not S: the task creates two new files (a Tier S task may not). The owner's two
  decisions (the plan; the real-sampler check guided by the owner) were given in answer to the plan presented at
  session start. The first free IDs came from `process/INDEX.idx.md` (`#next`).

---

### TASK-AKM-077: MIDI setup switches
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue `&01` to `&05` (Set, one byte argument, complete on DONE) and expose one typed
  primitive each, refusing a value outside the spec's range without sending. Extend the simulated sampler to
  record the five settings.
- **Requirement refs**: RQ-AKM-078
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-078 on the simulated sampler.
- **Dependencies**: TASK-AKM-076
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: build with no warning or error (`/W4 /WX`), `ctest` 607/607 after a
  re-run in this session (602 before, 5 new, `ctest -R RQ-AKM-078`). New `MidiConfigTests.cpp` (5 cases,
  `[akm][midi-config]`, written before the primitives): multi select BANK sends `04 02 02`, completes on DONE and
  the sampler records multi select 2 and one event; each switch at both ends of its range goes out as its own
  item and byte (program change 0/1, multi select 0/1/2, channel 0/31, controller 0/127, aftertouch 0/1) and
  the sampler ends holding the last values; multi select 3, channel 32 and -1, controller 128 and -1 and
  aftertouch 2 are refused `ArgumentOutOfRange` with nothing sent and no event; a program change enable of 2
  built through `makeRequest` is refused by the catalogue; with multi select answered ERROR the error is
  reported and the sampler keeps its value. `generate_akm_items.py` regenerated the table (304 items), `--check`
  up to date, `--coverage`: section `04` 5 of 7 spec rows covered (partial, as declared), `unaccounted: none`.
  Collateral edits reflecting the new expected state, no assertion weakened: `ItemCatalogueTests.cpp`'s total
  count gained 5, and `test_generate_akm_items.py`'s "undeclared section" example moved from `04` (now
  declared) to `12`. Not verified: real sampler (TASK-AKM-079); mutation testing. The tests were written first
  but not run red: the code they target did not compile until it existed.
- **Assumptions**: `setProgramChangeEnabled` takes a bool, like the §00 toggles, so the refusal of a 2 is the
  catalogue's (tested through `makeRequest`); the channel and controller are `int`, so that an out-of-range
  value is refusable rather than truncated, as for the wheel (TASK-AKM-071). The simulated sampler's defaults
  (program change on, multi select off on channel 1A, controller 0, channel aftertouch) are modelling choices —
  the spec gives none — and, like §02, the setup survives `powerCycle()`; it does not model the sampler taking a
  channel away from program selection when multi select is on (manual p223).

---

### TASK-AKM-078: MIDI filters
- **Tier**: M
- **Status**: Not Started
- **Description**: Catalogue `&06` and `&07` (Set, two byte arguments: event type 0-3, channel 0-31) and expose
  one primitive for each intent (allow, ignore), refusing an out-of-range event type or channel without
  sending. Extend the simulated sampler to record the filter state per event type and channel.
- **Requirement refs**: RQ-AKM-079
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-079 on the simulated sampler.
- **Dependencies**: TASK-AKM-076
- **Assignee**: AI
- **Verification**: To be filled at closure.
- **Assumptions**: None yet.

---

### TASK-AKM-079: Real-sampler check guided by the owner
- **Tier**: M
- **Status**: Not Started
- **Description**: Add `--midi-config` to `xs56k_akm_probe --suite`: tell the owner where the values are shown,
  ask for the current value of every setting the check changes (`askOwnerChoice`), change each to a different
  value with the owner's confirmation on the sampler's screen, then put each back to the declared value, on every
  exit path. The values to change to and the filter exercised are fixed with the owner before coding (DoR of this
  task).
- **Requirement refs**: RQ-AKM-080
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008 precedent for the suite's seams and opt-in flags)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-080 on the simulated sampler in `ctest`
  (scripted owner), and on the real sampler run by the owner.
- **Dependencies**: TASK-AKM-077, TASK-AKM-078
- **Assignee**: AI, with the owner running the real-sampler check
- **Verification**: To be filled at closure.
- **Assumptions**: None yet. Tier M assumes the existing owner seams are enough; it becomes L if a new public API
  or seam is needed.

---

### TASK-AKM-080: Coverage of section §04
- **Tier**: M
- **Status**: Not Started
- **Description**: Run `generate_akm_items.py --coverage` for section `04`, list any exclusion with its reason,
  flip the section's `complete` flag in `items.json` if all seven rows are covered, and update
  `SUMMARY-akm-sections-coverage.md`, `AGENTS.md` (the new probe option), `CHANGELOG.md` and
  `documents/_index/sysex_spec.kb.md`.
- **Requirement refs**: RQ-AKM-081
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-081.
- **Dependencies**: TASK-AKM-077 to TASK-AKM-079
- **Assignee**: AI
- **Verification**: To be filled at closure.
- **Assumptions**: None yet.
