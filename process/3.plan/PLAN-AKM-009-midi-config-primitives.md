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
- **Status**: Done
- **Description**: Catalogue `&06` and `&07` (Set, two byte arguments: event type 0-3, channel 0-31) and expose
  one primitive for each intent (allow, ignore), refusing an out-of-range event type or channel without
  sending. Extend the simulated sampler to record the filter state per event type and channel.
- **Requirement refs**: RQ-AKM-079
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-079 on the simulated sampler.
- **Dependencies**: TASK-AKM-076
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: build with no warning or error (`/W4 /WX`), `ctest` 610/610 after a
  re-run in this session (607 before, 3 new, `ctest -R RQ-AKM-079`). Three new cases in `MidiConfigTests.cpp`
  (`[akm][midi-config]`, written before the primitives): the Wheels filter on channel 3A ignored then allowed
  sends `04 07 02 02` then `04 06 02 02`, both DONE, exactly one of the 128 filters is recorded ignoring in
  between, and the two events are recorded in order; each of the 4 event types on channel 0 and on channel 31
  (1A and 16B) is ignored then allowed, each going out as its own item and bytes, with the filter state
  following; event type 4, channel 32 and channel -1 are refused `ArgumentOutOfRange` through both
  primitives with nothing sent and no event. `generate_akm_items.py` regenerated the table (306 items),
  `--check` up to date, `--coverage`: section `04` 7 of 7 spec rows covered, `unaccounted: none` (`complete`
  stays `false` until TASK-AKM-080). Collateral: `ItemCatalogueTests.cpp`'s total count gained 2 (an edit
  reflecting the new expected state, no assertion weakened). Not verified: real sampler (TASK-AKM-079);
  mutation testing. As in TASK-AKM-077 the tests were written first but not run red.
- **Assumptions**: The primitives are named for their effect on the messages (`allowMidiEvents`,
  `ignoreMidiEvents`), not for the item's "enable/disable filter" wording, which reads the opposite way round
  (enabling the filter allows the messages). The channel is the spec's 0-31 code carrying the port; the
  simulated sampler keeps one filter per (event type, channel) and does not model the port a frame arrives
  on — whether the sampler cares is the open point on Port B in `FTR-AKM-009`, for the real sampler.

---

### TASK-AKM-079: Real-sampler check guided by the owner
- **Tier**: L (re-tiered from M, see Assumptions)
- **Status**: Done
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
- **Verification**: Windows/MSVC Debug: build with no warning or error (`/W4 /WX`), `ctest` 615/615 after a
  re-run in this session (610 before, 5 new, `ctest -R RQ-AKM-080`, written before the code). New cases in
  `RealSamplerSuiteTests.cpp` (`[akm][suite][midi-config]`): an owner declaring the values the sampler was
  seeded with (program change off, multi select BANK, channel 6A, controller 74, polyphonic aftertouch, the
  Wheels filter of channel 2B ignoring) — the sampler is sent exactly 14 §04 items in this order: six changes to
  another value (program change on, multi select OFF, channel 7A, controller 75, channel aftertouch, the filter
  allowed), the six restores in the opposite order, then the failed check's change and restore; it ends in the
  seeded state, all 9 checks pass, `knownStateRestored` true, and the sampler had been sent no §04 item when the
  owner was first asked anything; an owner who declines the declaration, or no way to ask for a number: both
  checks Skipped, no §04 item sent; an owner who sees that nothing changed: the first check Failed ("NOT MET"
  in the log) and the sampler back to the seeded state all the same; default options: no §04 item sent. By hand,
  on the built probe: `xs56k_akm_probe --help` shows `--midi-config` and its warning; `--midi-config` without
  `--suite` is refused, exit 1. Not verified: the real sampler and the probe's console prompts — which only the
  owner can run; nothing here sent a frame to hardware.
- **Assumptions**: Re-tiered from M to L as the plan itself foresaw: the check needs a new owner seam,
  `RealSuiteOptions::askOwnerNumber`, for the external APM controller (0-127, too long a list for
  `askOwnerChoice`); no ADR (like `readOwnerKey` in TASK-AKM-073, test-support API only, DEC-AKM-008 precedent).
  The owner declares one filter (its event type, its channel, whether it allows or ignores) rather than all 128.
  The value each setting is changed to is the next one after the declared (multi select, channel, controller:
  +1 modulo the range; the two-valued ones toggled; the filter reversed). The declaration is asked once per run
  and shared by the two checks. A "No" on a screen confirmation fails the check, a decline skips it, and either
  way the guard puts back what was changed. The probe's choice prompt now says "your choice" instead of "the
  disk" since it serves both. The owner's declaration is not verified (stated in FTR-AKM-009).

---

### TASK-AKM-080: Coverage of section §04
- **Tier**: M
- **Status**: Done
- **Description**: Run `generate_akm_items.py --coverage` for section `04`, list any exclusion with its reason,
  flip the section's `complete` flag in `items.json` if all seven rows are covered, and update
  `SUMMARY-akm-sections-coverage.md`, `AGENTS.md` (the new probe option), `CHANGELOG.md` and
  `documents/_index/sysex_spec.kb.md`.
- **Requirement refs**: RQ-AKM-081
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-081.
- **Dependencies**: TASK-AKM-077 to TASK-AKM-079
- **Assignee**: AI
- **Verification**: `generate_akm_items.py --coverage` re-run this session, after the flag flip:
  `section 04: 7 of 7 spec rows covered (MIDI Configuration, complete)`, `unaccounted: none`, exit 0 — no row
  excluded, nothing left to give a reason for. `items.json`'s section-04 `complete` flag flipped `false` → `true`,
  its note closed out; `generate_akm_items.py` (no args) regenerated `ItemTable.generated.hpp` with no diff,
  `--check`: up to date (306 items). Windows/MSVC Debug: clean build, no warning or error (`/W4 /WX`); `ctest
  --test-dir juce/build -C Debug` 615/615 passed, re-run after the flip. No erratum found in §04: the seven rows
  of Table 8 (spec lines 547-570) match the catalogue's ranges; the "Port A & B" wording of `&06`/`&07` is
  recorded as an open observation (FTR-AKM-009, `sysex_spec.kb.md`), not as an erratum, nothing there
  contradicting the spec. `AGENTS.md`: the `--midi-config` option and its warnings documented; `CHANGELOG.md`:
  one `[Unreleased]` entry; `SUMMARY-akm-sections-coverage.md`: §04's bar to 100 %, the total to 452/560
  (81 %), and a note that §04 has not run on a real sampler yet; `sysex_spec.kb.md`: a §04 state-model line.
- **Assumptions**: `complete` is flipped on the strength of the catalogue matching the spec's rows, as for §02,
  §10 and §20 before it, not on a real-sampler run: §04 has no Get, and its hardware proof is the owner's
  `--midi-config` run, still to come. One `CHANGELOG.md` entry covers the whole lot rather than one per task.

---

### TASK-AKM-081: Correct the MIDI configuration check after its first real-sampler run
- **Tier**: M
- **Status**: Done
- **Description**: The owner's first run of `--midi-config` on the S5000 (`OBSERVATIONS-RQ-AKM-080-midi-config.md`)
  stopped at the first setting the owner did not see (MULTI SELECT) and had tested it while PROGRAM CHANGE was still
  changed. Make the check change and put back each setting before the next is touched, note a "no" instead of
  stopping, try every setting, and fail at the end naming those not seen.
- **Requirement refs**: RQ-AKM-080
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008 precedent for the suite's seams)
- **Acceptance Criteria** (Gherkin): *Given* a scripted owner who declares the sampler's real values, *When* the check
  runs, *Then* the §04 items reach the sampler as one change then its restore per setting, in turn, and the sampler
  ends in the declared state. *Given* an owner who sees nothing change, *When* the check runs, *Then* all six settings
  are still changed and put back, the check fails and names them, and the sampler ends in the declared state.
- **Dependencies**: TASK-AKM-079
- **Assignee**: AI, with the owner's second real-sampler run
- **Verification**: Windows/MSVC Debug: build with no warning or error (`/W4 /WX`), `ctest` 615/615 after a re-run in
  this session. In `RealSamplerSuiteTests.cpp` the two cases of TASK-AKM-079 changed to the new expected behaviour:
  the first now expects the §04 items as pairs (program change 1 then 0, multi select 0 then 2, channel 6 then 5,
  controller 75 then 74, aftertouch 0 then 1, filter allowed then ignored) followed by the failed check's pair, the
  sampler ending in the seeded state; the second (owner answering "no" everywhere) expects the same 14 items, the first
  check Failed with its detail naming MULTI SELECT and MIDI FILTER, and the seeded state back. The first run's own
  facts are in `OBSERVATIONS-RQ-AKM-080-midi-config.md`. Not verified: the second run on the real sampler, which only
  the owner can make; nothing here sent a frame to hardware. After the owner's comment that the screen had not changed
  where "no" was answered, the "no" answer was split in two ("nothing changed on the screen" / "the screen shows another
  value"), the answer being named in the log and in the report; `ctest` 615/615 re-run after that edit.
- **Assumptions**: The two edited tests describe a corrected expectation (a check that stopped at the first "no" and
  stacked its changes), not a failing test forced to pass. The final confirmation (the original screens are back) is
  noted like the others and listed with them. Whether MULTI SELECT depends on PROGRAM CHANGE is not decided here:
  the owner's hypothesis is recorded as such, to be settled by the next run.
