# FTR-AKM-006: System Setup Primitives (§02)

## Overview

Phase A, residual items of section §02 (owner decision, session AKM, 2026-10-01): `FTR-AKM-001`
(`RQ-AKM-044`) delivered only `&00`/`&01` (operating-system version) of §02. This feature adds one
tested primitive for each of the remaining items — 14 commands and 9 REPLY formats (23 of the
section's 27 rows): read or set the sampler's name (`&02`/`&03`), read its model (`&04`), read or set
its clock and date (`&05`/`&06`), set or read its Play Mode (`&10`/`&20`) and its front-panel lock
(`&11`/`&21`), read the Wave memory (`&30`, `&33`, `&34`) and MPKS memory (`&31`) available, and clear
the sampler's memory (`&32`, guarded) — each proven by a Set followed by a Get (or a Get alone for the
read-only items) on the simulated sampler, then on the real sampler.

**In scope** (counts from `documents/_index/sysex_spec.items.tsv`, section `02`: 16 commands and 11
REPLY formats, 27 rows; `&00`/`&01` and their two REPLY formats are already covered by `RQ-AKM-044`):
`&02`, `&03`, `&04`, `&05`, `&06`, `&10`, `&11`, `&20`, `&21`, `&30`, `&31`, `&32`, `&33`, `&34`.

**Out of scope.** Sections other than §02, including every other §00 toggle (`FTR-AKM-001`). Any
real-sampler test of `&32` — it is delivered guarded and proven on the simulated sampler only.

**Depends on** FTR-AKM-001 (transport) only: §02 items are sampler-wide, with no current program,
keygroup, zone or sample.

**Known spec inconsistency.** `&10` (Set Play Mode) lists `<Data1>` as "0, 1, 2" while its text defines
`3 = Muted` (`documents/_index/sysex_spec.kb.md`, errata list; p11). `RQ-AKM-057` requires its
resolution by observation.

**Sources.** `documents/_index/sysex_spec.kb.md` (§02 in Table 4, Play Mode line 131, errata line 181;
data formats lines 85–87), `documents/_index/sysex_spec.items.tsv` (section `02`),
`documents/akai_s5000_s6000_sysex_spec_2.10.pdf.md` printed pp. 14–15 and 11 (Tables 6 and 7).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the Phase B workflows that show the sampler's identity, memory and mode; CI.

---

## Functional Requirements

### RQ-AKM-052: Sampler name (Set and Get)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller sets the sampler's name (`&02`) or gets it (`&03`), the AKM layer SHALL send the matching item with the name as a null-terminated string (`String` value format, `DEC-AKM-013`) and decode the REPLY into a typed name; a Get right after a Set SHALL return the name set.
- **Rationale**: the first §02 items that carry a string; the name identifies the sampler to the user.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler named `S5000`, *When* its name is set to `STUDIO` then read, *Then* the frame carries `53 54 55 44 49 4F 00` and the Get returns `STUDIO`. *Given* the real sampler, *When* the same round trip runs and the original name is restored, *Then* it passes and the name is what it was.
- **Dependencies**: RQ-AKM-002; FTR-AKM-001 (RQ-AKM-009, RQ-AKM-005)

### RQ-AKM-053: Sampler model and available memory (Get)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller reads the sampler's model (`&04`: `0` = S5000, `1` = S6000), the percentage of free Wave memory (`&30`) or of free MPKS memory (`&31`), or the total (`&33`) or free (`&34`) bytes of Wave memory, the AKM layer SHALL send the matching Get and decode the REPLY into a typed value, the byte counts as compound double words (`sysex_spec.kb.md` line 85).
- **Rationale**: lets an application tell the two models apart and check that a sample fits before it is loaded.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler of model S6000 with 64 MiB of Wave memory of which 16 MiB are free, *When* the model and the four memory values are read, *Then* they decode to S6000, 25 %, 64 MiB and 16 MiB. *Given* a REPLY whose model byte is neither `0` nor `1`, *When* decoded, *Then* it is reported as malformed, not guessed.
- **Dependencies**: RQ-AKM-002

### RQ-AKM-054: Clock and date (Set and Get)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller reads the clock and date (`&05`) or sets them (`&06`: year 1980–2079, month, day of month, day of week 1–7 with 1 = Sunday, hours, minutes, seconds), the AKM layer SHALL encode and decode all eight data bytes per the spec, and SHALL refuse any out-of-range field without sending, the first offending field being identifiable by a function of the AKM layer that applies the same ranges.
- **Rationale**: the sampler stamps what it saves with this clock; wrong or unreadable dates are otherwise only fixable from the front panel.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler, *When* the clock is set to 2026-10-01 (Thursday) 14:30:15 then read, *Then* the value read equals the value set. *Given* month `13`, *When* set, *Then* nothing is sent, the refusal is `ArgumentOutOfRange`, and the function that finds the first out-of-range field names the month.
- **Dependencies**: RQ-AKM-002

### RQ-AKM-055: Play Mode and front-panel lock (Set and Get)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller sets or gets the Play Mode (`&10`/`&20`: Multi, Program, Sample, Muted) or the front-panel lock-out state (`&11`/`&21`: normal, locked), the AKM layer SHALL send the matching item with the value encoded in its spec format, refuse an out-of-range value without sending, and decode the Get REPLY into a typed value; a Get right after a Set SHALL return the value set.
- **Rationale**: the plan's method — one item, one Set test, one Get test, verified by read-back.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler, *When* each Play Mode is set then read, *Then* the value read equals the value set. *Given* the lock set to `1` then `0`, *When* read after each, *Then* it reads `1` then `0`. *Given* Play Mode `4` or lock `2`, *When* set, *Then* it is refused without sending.
- **Dependencies**: RQ-AKM-002

### RQ-AKM-056: Destructive command guard for "Clear Sampler Memory"
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a caller requests "Clear Sampler Memory" (`&32`), THEN the AKM layer SHALL send it only when the caller passes an explicit confirmation argument that no default supplies, and no real-sampler test of any feature SHALL call it.
- **Rationale**: mirrors `RQ-AKM-025` and `RQ-AKM-046`, for the one item that deletes every program, multi and sample at once.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a request for `&32` without the confirmation argument, *When* made, *Then* nothing is sent and an error explains why. *Given* the source of the real-sampler tests, *When* searched for the `&32` primitive, *Then* there is no call.
- **Dependencies**: RQ-AKM-025; RQ-AKM-046

### RQ-AKM-057: Coverage of section §02
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Every §02 row of `documents/_index/sysex_spec.items.tsv` SHALL be either covered by a primitive or listed with a reason for its exclusion, and any spec inconsistency met while doing so — in particular the Play Mode range of `&10` — SHALL be resolved by observation on the real sampler and the resolution recorded.
- **Rationale**: 27 rows are too many to track by eye; the errata list already carries one §02 entry.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the 16 command rows and 11 REPLY rows of section `02`, *When* compared with the catalogue of primitives and exclusions, *Then* no row is unaccounted for. *Given* any erratum found, *When* the lot ends, *Then* its resolution is recorded with the observation that supports it.
- **Dependencies**: RQ-AKM-052 to RQ-AKM-056; RQ-AKM-044

### RQ-AKM-058: Real-sampler tests restore the system setup they change
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: The real-sampler tests of this feature SHALL read the sampler's name, clock, Play Mode and front-panel lock before changing any of them and SHALL put each back — the clock advanced by the time elapsed — even when a test fails or is interrupted, SHALL never leave the front panel locked by a test (it is left as it was found, so locked only if it was locked when the suite began), and SHALL NOT call `&32`; IF the restoration of any value fails, THEN the suite SHALL report it as failed and say which value was left changed.
- **Rationale**: mirrors `RQ-AKM-018` (known state) and `RQ-AKM-027`; these four values are the owner's own settings and are not reproducible from the software.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a real-sampler run, *When* the suite ends, *Then* the name, Play Mode and lock equal what they were, the clock is within a few seconds of what it was advanced by the time elapsed, and no `&32` was sent. *Given* a check made to throw after the lock was set to locked, *When* the suite ends, *Then* the lock reads `0`.
- **Dependencies**: RQ-AKM-018; RQ-AKM-052; RQ-AKM-054; RQ-AKM-055; RQ-AKM-056

### RQ-AKM-059: The REPLY of Get Clock Time and Date carries another section than its command
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF the sampler answers Get Clock Time & Date (`&05`) with a REPLY whose section byte is not `02` (the command's own) but `0B`, THEN the AKM layer SHALL still read it as that command's REPLY, and SHALL NOT accept any other item's REPLY, nor any confirmation other than a REPLY, under a section other than its command's.
- **Rationale**: observed on a real S5000 (OS 2.14) twice, independently — once through the real-sampler suite (TASK-AKM-053, `akm-suite-20261001-215601.log`) and once by hand with a raw SysEx frame sent and read in MIDI-OX (session AKM, 2026-10-02, owner) — the sampler's own text for item `05` names no such behaviour; every other item observed so far answers under its own section.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the REPLY the S5000 sent for `&05` (section `0B`, item `05`, 8 data bytes), *When* decoded, *Then* its data is read and the command it answers completes. *Given* a REPLY under section `0B` for an item that does not declare it, *When* received, *Then* it completes nothing and is reported as unsolicited (or the command times out). *Given* a sampler that answers `&05` under section `02` as the spec's own text says, *When* received, *Then* it is read all the same.
- **Dependencies**: RQ-AKM-007; RQ-AKM-041; RQ-AKM-054

---

## Open points

- **Compound double word.** `ValueFormat::Dword` is declared but unused (`PLAN-AKM-005`: the coverage checker flagged one combined value against four wire bytes when it was tried for §0E). `&33`/`&34` are the first replies where a decoded 28-bit count is the natural result; whether they use `Dword` or four `Byte` values like §0E is settled by `TASK-AKM-049`.
- **Year encoding.** The spec gives Data1 as 0–16 (MSB) and Data2 as 0–127 (LSB) for years 1980–2079; whether that word is the full year or an offset from 1980 is settled on the simulated spec reading in `TASK-AKM-050` and confirmed on the real sampler.
- **Which items answer.** Only `&03`, `&04`, `&05`, `&20`, `&21`, `&30`, `&31`, `&33`, `&34` have a REPLY row; `&02`, `&06`, `&10`, `&11`, `&32` complete on DONE.
