# PLAN-AKM-008: Front Panel Control Primitives (Phase A, new lot, §20)

## Overview

Implements `FTR-AKM-008`: one tested primitive per command row of section `20` (4 commands, no REPLY
format — 4 rows), a typed catalogue of the keycodes of Table 31, a paired "press" operation, the release of
a held key when a session closes (one new decision, `DEC-AKM-019`), and an interactive real-sampler check in
which the owner drives the sampler from the PC keyboard.

Like §02 and §10, §20 has no "current item" state. Unlike every section so far it has no Get and no REPLY:
each command completes on DONE, which only means "queued" (Table 30 note a), so the primitives are proven by
what the simulated sampler records having received, not by a read-back.

**Prerequisite check.** The catalogue already carries items completing on DONE with byte arguments (§02
`&10`, §0A selections); no new value format is needed. What is new: a command whose effect lasts until a
second command (Hold/Release), hence `TASK-AKM-072`, and a real-sampler check whose input is the owner's
keystrokes, hence a new owner-input seam in `RealSamplerSuite` next to `askOwner` (`TASK-AKM-073`).

**Safety note.** Front-panel keys act on the screen the sampler shows (`FTR-AKM-008`, real-hardware risk).
The only real-sampler check is opt-in (`--front-panel`), sends only what the owner presses, and starts only
after the owner has confirmed being on a screen of their choice (`RQ-AKM-076`). Every key held is released,
on every exit path (`RQ-AKM-075`, `RQ-AKM-076`).

## References
- **Requirements**: RQ-AKM-073 to RQ-AKM-077 (`FTR-AKM-008`)
- **ADRs**: ADR-AKM-001 (Accepted) — extended by one new decision, `DEC-AKM-019` (a session releases the keys
  it holds when it closes), added to the existing file like `DEC-AKM-012` to `018` before it, not a new
  ADR document.

The plan has 6 tasks (TASK-AKM-069 to TASK-AKM-074): 069 authors the artifacts; 070 and 071 deliver the
primitives (independent, both after 069); 072 adds the release at session close (after 070); 073 is the
real-sampler harness (after 070, 071, 072); 074 closes the coverage (after 070 to 073).

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-AKM-069: Author FTR-AKM-008 and PLAN-AKM-008
- **Tier**: M
- **Status**: Done
- **Description**: Write the feature file and this plan for the four items of section `20`, from the spec's
  own row counts and the owner's decisions of this session.
- **Requirement refs**: RQ-AKM-073, RQ-AKM-074, RQ-AKM-075, RQ-AKM-076, RQ-AKM-077
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* the 4 rows of section `20`, *When* the feature is read, *Then*
  each row falls under exactly one of RQ-AKM-073 and RQ-AKM-074. *Given* the owner's two decisions (release
  held keys at session close; a real, owner-driven check with a PC-keyboard mapping), *When* the feature is
  read, *Then* each is a requirement (RQ-AKM-075, RQ-AKM-076).
- **Dependencies**: None
- **Assignee**: AI, with the owner's approval (DoR, given 2026-10-03)
- **Verification**: `agnos-index` re-run in this session: 226 entries, 37 documents, exit 0, no duplicate
  ID; `FTR-AKM-008` (RQ-AKM-073 to 077) and `PLAN-AKM-008` (TASK-AKM-069 to 074) are indexed. Section `20`
  rows read from the spec text (`sysex_spec.clean.txt`, Table 30): 4 commands `&01`–`&04`, no REPLY row —
  `&01`/`&02` under RQ-AKM-073, `&03`/`&04` under RQ-AKM-074. Table 31 count re-done: 8 mode keys + 16
  function keys + 10 digits + 9 others = 43 keycodes of the 44 values of `&40`–`&6B`, `&66` unlisted.
  Both owner decisions are RQ-AKM-075 and RQ-AKM-076. No code changed; nothing sent to hardware.
- **Assumptions**: Tier M, not S: the task creates two new files (a Tier S task may not). The two decisions
  were given by the owner in answer to the plan presented at session start.

---

### TASK-AKM-070: Key Hold and Key Release, keycodes of Table 31
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue `&01` and `&02` (Set, byte argument, complete on DONE); add a typed
  `FrontPanelKey` enumeration with the 43 keycodes of Table 31 (an unlisted value is refused, `&66` included);
  expose `holdKey`, `releaseKey` and `pressKey` (Hold then Release, the Release sent whatever the Hold's
  outcome). Extend the simulated sampler to record keys down and up.
- **Requirement refs**: RQ-AKM-073
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-073 on the simulated sampler.
- **Dependencies**: TASK-AKM-069
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: build with no warning or error (`/W4 /WX`), `ctest` 556/556 after a
  re-run in this session. New `FrontPanelTests.cpp` (6 cases, `[akm][front-panel]`, written before the
  primitive, run standalone with `ctest -R RQ-AKM-073`): EXIT held then released puts `20 01 6A` then
  `20 02 6A` on the wire, both DONE, and the simulated sampler records the key down then up with nothing
  left down; each of the 43 keycodes of Table 31 (the table re-typed in the test from the spec, not read from
  the primitive) goes out as its own code; of the 128 data-byte values exactly those 43 convert to a key, and
  `&66`, `&3F` and `&6C` are refused `ArgumentOutOfRange` on both Hold and Release with nothing sent;
  `pressKey` sends Hold then Release and completes once with both results; with the Hold answered ERROR 3
  (`itemErrors`) the Release is still sent and both results are reported; an unlisted value pressed has both
  refused and nothing sent. Collateral: `ItemCatalogueTests.cpp`'s total count of catalogued items gained 2
  (an edit reflecting the new expected state, no assertion weakened). `generate_akm_items.py --check`: up to
  date (297 items); `--coverage`: section `20` 2 of 4 spec rows covered, `unaccounted: none`. Not verified:
  real sampler (TASK-AKM-073); mutation testing.
- **Assumptions**: The catalogue's range for the keycode is Table 30's own 64-107, and the exclusion of
  `&66` is the primitive's (`frontPanelKeyFromCode`), not the catalogue's: a range cannot express a hole.
  `pressKey` queues the Release behind the Hold with two `submit` calls rather than `submitSequence`, which
  would cancel the Release when the Hold fails (`DEC-AKM-010`); another command from another thread may
  therefore interleave between the two, which is harmless. The simulated sampler accepts a Release of a key
  that is not down (the spec is silent) and, like its §02 setup, keeps the front panel across `powerCycle()`
  — modelling choices, not proven on hardware. The primitives live in a new `FrontPanel.hpp`/`.cpp` that
  TASK-AKM-071 and 072 extend.

---

### TASK-AKM-071: Data wheel and ASCII keyboard
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue `&03` (direction 0/1, clicks 1–8) and `&04` (0–127); expose `moveDataWheel` and
  `sendAsciiKey`, refusing out-of-range values without sending. Extend the simulated sampler to record them.
- **Requirement refs**: RQ-AKM-074
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-074 on the simulated sampler.
- **Dependencies**: TASK-AKM-069
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: build with no warning or error (`/W4 /WX`), `ctest` 561/561 after a
  re-run in this session. 5 new cases in `FrontPanelTests.cpp` (`[akm][front-panel]`, written before the
  code, `ctest -R RQ-AKM-074`): the wheel moved backwards by 3 clicks sends item `03` with data `01 03` and the
  simulated sampler records one wheel movement (direction 1, 3 clicks); both directions at 1 and at 8 clicks
  go out as their own bytes (4 DONE, 4 events); 0 or 9 clicks, or a direction cast to 2, are refused
  `ArgumentOutOfRange` with nothing sent; ASCII 65 sends item `04` with data `41`, 0 and 127 go out as `00`
  and `7F`; 128 and -1 are refused `ArgumentOutOfRange` with nothing sent. Collateral: `ItemCatalogueTests.cpp`'s
  total count gained 2 (an edit reflecting the new expected state, no assertion weakened).
  `generate_akm_items.py --check`: up to date (299 items); `--coverage`: section `20` 4 of 4 spec rows
  covered, `unaccounted: none` (`complete` is flipped by TASK-AKM-074). Not verified: real sampler
  (TASK-AKM-073, which exercises the keys only, as decided); mutation testing.
- **Assumptions**: The ASCII primitive takes an `int` so that 128 and negative values are refusable by the
  catalogue's range check, rather than a `std::uint8_t` that could not hold them; the same for the wheel's
  click count. The simulated sampler records an ASCII character without interpreting it (no text field is
  modelled) and, like the keys, keeps the record across `powerCycle()` — modelling choices, not proven on
  hardware.

---

### TASK-AKM-072: Release held keys when the session closes
- **Tier**: L
- **Status**: Not Started
- **Description**: Make the session remember the keys held through `holdKey` and not yet released, and send a
  Release for each before `Session::close` completes, including when the close follows a failed command.
  Record the choice as `DEC-AKM-019` in `ADR-AKM-001` (a cross-cutting change to the session's closing
  sequence, `DEC-AKM-004`, `DEC-AKM-010`; `RQ-AKM-042`).
- **Requirement refs**: RQ-AKM-075
- **ADR refs**: ADR-AKM-001 (DEC-AKM-004, DEC-AKM-010, new DEC-AKM-019)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-075 on the simulated sampler.
- **Dependencies**: TASK-AKM-070
- **Assignee**: AI
- **Verification**: To be filled at closure.
- **Assumptions**: None yet.

---

### TASK-AKM-073: Real-sampler check driven from the PC keyboard
- **Tier**: L
- **Status**: Not Started
- **Description**: Add `--front-panel` to `xs56k_akm_probe --suite`: print a named mapping table of PC keys to
  sampler keys, ask the owner (`askOwner`) to confirm the sampler shows a screen of their choice, then read
  PC keys through a new owner-input seam in `RealSamplerSuite` (one key at a time, no Enter) and send the
  mapped §20 item for each until the end key; release every key still held on every exit path. The mapping
  is fixed with the owner before coding (DoR of this task).
- **Requirement refs**: RQ-AKM-076, RQ-AKM-075
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008 precedent for the suite's seams and opt-in flags)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-076 on the simulated sampler in `ctest`
  (scripted PC keys), and on the real sampler run by the owner.
- **Dependencies**: TASK-AKM-070, TASK-AKM-071, TASK-AKM-072
- **Assignee**: AI, with the owner running the real-sampler check
- **Verification**: To be filled at closure.
- **Assumptions**: None yet.

---

### TASK-AKM-074: Coverage of section §20
- **Tier**: M
- **Status**: Not Started
- **Description**: Run `generate_akm_items.py --coverage` for section `20`, list any exclusion with its
  reason, flip the section's `complete` flag in `items.json` if all four rows are covered, and update
  `SUMMARY-akm-sections-coverage.md`, `AGENTS.md` (the new probe option) and `CHANGELOG.md`.
- **Requirement refs**: RQ-AKM-077
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-077.
- **Dependencies**: TASK-AKM-070 to TASK-AKM-073
- **Assignee**: AI
- **Verification**: To be filled at closure.
- **Assumptions**: None yet.
