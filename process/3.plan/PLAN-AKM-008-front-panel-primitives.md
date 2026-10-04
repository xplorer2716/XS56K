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

The plan has 7 tasks (TASK-AKM-069 to TASK-AKM-075): 069 authors the artifacts; 070 and 071 deliver the
primitives (independent, both after 069); 072 adds the release at session close (after 070); 073 is the
real-sampler harness (after 070, 071, 072); 074 closes the coverage (after 070 to 073); 075, added in the
same session after an independent code review of the delivered lot, corrects what that review found (after 070 to 074).

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
- **Status**: Done
- **Description**: Make the session remember the keys held through `holdKey` and not yet released, and send a
  Release for each before `Session::close` completes, including when the close follows a failed command.
  Record the choice as `DEC-AKM-019` in `ADR-AKM-001` (a cross-cutting change to the session's closing
  sequence, `DEC-AKM-004`, `DEC-AKM-010`; `RQ-AKM-042`).
- **Requirement refs**: RQ-AKM-075
- **ADR refs**: ADR-AKM-001 (DEC-AKM-004, DEC-AKM-010, new DEC-AKM-019)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-075 on the simulated sampler.
- **Dependencies**: TASK-AKM-070
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean build with no warning or error (`/W4 /WX`), `ctest` 576/576 after
  a re-run in this session. New `FrontPanelCloseTests.cpp` (15 cases, `[akm][close][front-panel]`, written
  before the code, `ctest -R RQ-AKM-075`): a held EXIT is released before the close completes (frames Hold
  then Release `20 02 6A`, simulated sampler with no key down, `keysReleased == {6A}`); no key held, or held
  then released by the caller → no §20 frame at the close; two keys → released in ascending keycode order;
  the same key held twice → one Release; a Hold that timed out, or answered ERROR 3 → released all the same;
  a Hold answered ERROR 0 → forgotten, no frame, `restoredAll()`; a Release the sampler refuses at the close
  → `keysNotReleased`, the close finishes; a silent sampler → the close returns after one timeout (50 ms
  command timeout, elapsed within 10 ms of it) with the key reported not released; a press whose Hold is in
  flight at the close → both commands `Cancelled` and the key still released; a Hold still queued, never sent
  → no Release; the target rebound to another device since the Hold → nothing sent, key reported not released;
  a checksum-mode command in flight at the close → the Release carries a checksum and is accepted; a session
  destroyed without a close on a real thread (5 repeats) → the key is released. Mutation check done by hand:
  with the `cancelEverything` fix disabled, the checksum-mode case fails (3 assertions); restored, it passes.
  Independent review: `REVIEW-DEC-AKM-019-opus.md` (no blocking finding; S1 to S4 adopted, N1 to N5 followed
  or stated, see its disposition table); `DEC-AKM-019` amended accordingly in `ADR-AKM-001`. `CloseResult` gained
  `keysReleased`/`keysNotReleased` and `restoredAll()` now counts the keys; `CommandOptions` gained `holdsKey`/
  `releasesKey`; no existing close test changed. Not verified: real sampler (TASK-AKM-073); the real-sampler
  behaviour of a Release for a key that is not down, and whether the sampler counts Holds (spec silent).
- **Assumptions**: Keys are released before the §00 settings, in ascending keycode order (arbitrary; the
  review noted a human chord has no fixed order). A key is remembered per (DeviceID, keycode). The count went
  from 561 to 576: the 15 new cases, every earlier one re-run unchanged.
  `CloseResult` carries keycodes only, not each key's outcome (reviewer's (e), not adopted: nothing consumes
  it yet). The destructor's guarantee is bounded as DEC-AKM-004 bounds the settings (stated in the ADR).

---

### TASK-AKM-073: Real-sampler check driven from the PC keyboard
- **Tier**: L
- **Status**: Done
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
- **Verification**: Windows/MSVC Debug: clean build with no warning or error (`/W4 /WX`), `ctest` 596/596 after
  a re-run in this session (576 before, 20 new, `ctest -R RQ-AKM-076`, tests written before the code). New
  `FrontPanelRemoteTests.cpp`: the mapping function (`remoteAction`) — all 36 rows of normal mode the owner
  approved (F1–F8, digits, `-`, `+`/`=`, cursors, Enter, Escape, the eleven mode letters), letters read in either
  case, the wheel keys (arrows 1 click, pages 8, up = forwards), Space/Tab/`q`, unmapped keys give nothing, the
  text mode (every printable 32–126 is ASCII, Backspace/Enter are 8/13, Tab and Escape leave it, DEL and keys with
  no character give nothing), the printed mapping names every sampler key, and each of the 43 keys has a name; the
  check against the simulated sampler with a scripted source of PC keys — each key sends exactly the frames its row
  names and `q` ends it (a key scripted after `q` is never read), Space holds ENT/PLAY and `q` releases it, Space
  twice holds then releases it, the text mode sends ASCII and Escape in it sends no EXIT, unmapped keys send nothing
  and the owner is told, the owner's input ending with a key held releases it, a reader that throws after a key
  was held fails the check and the session's close releases the key with `knownStateRestored` true, a declined
  confirmation or no way to ask or read a key skips the check with no §20 frame sent, no `--front-panel` means no
  extra check, and all 36 rows pressed once reach the sampler as their own keycodes; the whole mapping is told to the owner as a
  block of its own before the confirmation is asked and before the first key is read (added at the owner's request,
  after the first closure: it had been inside the confirmation text only). By hand: `xs56k_akm_probe
  --help` shows the option and its warning; `--front-panel` without `--suite` is refused, exit 1. `GuardedSession::close`
  now logs each key released or not released and `closeAndVerify` names the keys not released. Not verified: the
  real sampler and the Windows console reader (`_getch`, scan-code translation), which only the owner can run —
  nothing here sent a frame to hardware. Real sampler (RQ-AKM-076, last Gherkin): the owner ran `--front-panel` on
  the S5000 and reported it good (2026-10-04, as stated by the owner; no log or per-key observation was kept, so
  nothing here is re-produced from tool output).
- **Assumptions**: The owner approved the mapping as proposed (session AKM, 2026-10-03): F9–F16 of the sampler
  are not mapped. The console reader is Windows only (`_getch`); elsewhere, or when stdin is not a console, no
  reader is given and the check is skipped — the owner works on Windows, and POSIX terminal code could not be
  run here. A key pressed in the text mode is not interpreted by the suite: whether the S5000 accepts Backspace
  as ASCII 8 or Enter as 13 is an observation for the owner's run. A failed command (an ERROR, a timeout of one
  key) is said to the owner and logged but does not fail the check; only a lost completion does. A held ENT/PLAY
  is remembered even if its Hold failed, as the session does (DEC-AKM-019).

---

### TASK-AKM-074: Coverage of section §20
- **Tier**: M
- **Status**: Done
- **Description**: Run `generate_akm_items.py --coverage` for section `20`, list any exclusion with its
  reason, flip the section's `complete` flag in `items.json` if all four rows are covered, and update
  `SUMMARY-akm-sections-coverage.md`, `AGENTS.md` (the new probe option) and `CHANGELOG.md`.
- **Requirement refs**: RQ-AKM-077
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-077.
- **Dependencies**: TASK-AKM-070 to TASK-AKM-073
- **Assignee**: AI
- **Verification**: `generate_akm_items.py --coverage` re-run this session, before and after the flag flip:
  `section 20: 4 of 4 spec rows covered (Front Panel, complete)`, `unaccounted: none`, exit 0 — no row excluded,
  nothing left to give a reason for. `items.json`'s section-20 `complete` flag flipped `false` → `true`, its note
  closed out; `generate_akm_items.py` (no args) regenerated `ItemTable.generated.hpp` with no diff (section
  metadata, not read by the generator), `--check`: up to date (299 items). Windows/MSVC Debug: clean build, no
  warning or error (`/W4 /WX`); `ctest --test-dir juce/build -C Debug` 596/596 passed, re-run after the flip. No
  erratum found in §20: Table 31 lists 43 of the 44 keycodes of `&40`-`&6B`, `&66` unlisted and refused by the
  primitives (TASK-AKM-070) — recorded as a note in `sysex_spec.kb.md`, not as an erratum, nothing there
  contradicting the spec. `AGENTS.md`: the `--front-panel` option and its warning documented; `CHANGELOG.md`:
  one `[Unreleased]` entry; `SUMMARY-akm-sections-coverage.md`: §20's bar to 100 %, the total to 445/560 (79 %),
  and a note that §20 has not run on a real sampler yet.
- **Assumptions**: `complete` is flipped on the strength of the catalogue matching the spec's rows, as for §02
  and §10 before it, not on a real-sampler run: §20 has no Get, and its hardware proof is the owner's
  `--front-panel` run, still to come. One `CHANGELOG.md` entry covers the whole lot rather than one per task.

---

### TASK-AKM-075: Corrections found by the code review of the front panel lot
- **Tier**: L
- **Status**: Done
- **Description**: Correct what a review of the delivered lot found and the author verified against the code: (1) the
  owner-driven check's held-key bookkeeping — a short press of the key currently held must clear it, and a key is
  "held" only once its Hold succeeded; (2) the session forgets a key whose Hold the sampler answered with an ERROR
  of any number, since Table 30 note a says an ERROR means the data was not queued (DEC-AKM-019 amended); (3) a
  closing Release that times out no longer abandons the §00 settings: the keys not yet tried are reported, the
  settings are still tried, and the first setting that times out ends the restoring (DEC-AKM-019 amended); (4) the
  console reader of `--front-panel` reads the keys through `ReadConsoleInputW`, which separates a character from a
  prefix of an extended key and reports the number row as the digits printed on it whatever the layout — an
  AZERTY keyboard's unshifted number row included (the reader is told whether the check is in the text mode, where
  it must report the characters typed); (5) smaller points: the `close()` documentation says keys then settings, the
  key names come from one table, the wheel and ASCII primitives say where their range refusal comes from.
- **Requirement refs**: RQ-AKM-075, RQ-AKM-076
- **ADR refs**: ADR-AKM-001 (DEC-AKM-004, DEC-AKM-019)
- **Acceptance Criteria** (Gherkin): *Given* Space then Enter then Space, *When* the check runs, *Then* the second Space
  holds again (the Enter released the key) and the end of the check releases it once. *Given* a Hold the sampler
  answers with ERROR, *When* Space is pressed twice, *Then* two Holds are sent, no Release, and none at the end.
  *Given* a Hold answered ERROR 3, *When* the session is closed, *Then* no Release is sent and nothing is reported not
  released. *Given* a Release at the close that times out and settings the session changed, *When* the session
  is closed, *Then* the key is reported not released and the settings are still put back. *Given* a reader, *When* the
  check is in the text mode, *Then* it is told so.
- **Dependencies**: TASK-AKM-070 to TASK-AKM-074
- **Assignee**: AI, at the owner's request (2026-10-03)
- **Verification**: Windows/MSVC Debug: clean build with no warning or error (`/W4 /WX`), `ctest` 602/602 after a
  re-run in this session (596 before, 6 new cases; one existing case rewritten, see below). New or changed cases,
  written before the code: `FrontPanelCloseTests.cpp` — a Hold answered ERROR 3 or ERROR 2 is forgotten, nothing
  released and nothing reported (this case replaces "Hold answered ERROR → released all the same": its old
  expectation was the behaviour being corrected, Table 30 note a saying an ERROR means the data was not queued,
  an edit reflecting the corrected expected behaviour, no assertion weakened to pass); a Release timing out at the
  close with settings changed → the key reported not released, the three settings put back, one timeout elapsed;
  two keys with the first Release timing out → the second reported without being sent, settings put back; a
  sampler answering nothing → the close ends after two timeouts (the key's, the first setting's), everything
  reported not done. `FrontPanelRemoteTests.cpp` — Space, Enter, Space, `q` → hold, hold, release, hold, release
  (the Enter cleared the key held); a Hold refused with ERROR, Space twice → two Holds, no Release, none at the
  end; the reader is told false, true, true, false for Tab, `H`, Escape, `q`. Mutation check by hand: with the
  two session changes reverted to the old behaviour the four close cases above fail (4 failed of 22 run);
  restored, all pass. The simulated sampler gained `SamplerBehaviour::silentItems` (an item executed and answered
  with nothing) as the seam for a Release that alone does not answer. DEC-AKM-019 amended in `ADR-AKM-001`; the
  `close()` comment in `Session.hpp` now reads keys first, then the settings; `remoteKeyName` reads one table;
  `FrontPanel.cpp` says where the wheel's and ASCII's range refusal comes from. The review's remaining point, a
  missing test for the checksum-mode cancel in `cancelEverything`, was already covered: `FrontPanelCloseTests.cpp`
  has it (TASK-AKM-072), and that test fails with the fix disabled. Not verified: the new console reader of
  `--front-panel` (`ReadConsoleInputW`, the number row read by position on an AZERTY keyboard, `à` no longer taken for
  a key prefix), which compiles and links but only the owner can run on a console — nothing here sent a frame to
  hardware.
- **Assumptions**: The number row is read by position (scan codes `0x02`–`0x0B`) in the normal mode, so that the
  digits work on any layout; a key of the numeric pad, which types its digit, is read by its character. In the
  text mode the character typed is what is sent. The reader's own layout independence is by the scan codes
  Windows gives the keys, assumed to be those of a PC keyboard in the order QWERTY prints them. A Hold that
  merely times out is still remembered by the session (it may have been carried out) while the check's own
  Space toggle does not count it as held: the close releases it. Both reviews of this session's lot were run by
  the owner (`/code-review`); only the findings verified against the code were acted on.
