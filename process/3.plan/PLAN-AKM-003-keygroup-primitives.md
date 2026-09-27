# PLAN-AKM-003: Keygroup Primitives (Phase A, lot A3)

## Overview

Plan for FTR-AKM-003, section §08 (Keygroup) of the S5000 control plan
(`process/1.requirements/DRAFT-s5000-midi-primitives-and-workflows.md`, lot A3): 80 commands and 40
REPLY formats — current-keygroup selection, general options (keyspan, mute group, FX override/send,
zone crossfade), and the four "voice" groups (Pitch/Amp, Filter, Filter Envelope, Amplitude Envelope,
Aux Envelope) — each proven by a Set followed by a Get on the mock, then on the real sampler against
the dedicated test program of FTR-AKM-002 (RQ-AKM-027).

**Prerequisite already met.** Unlike PLAN-AKM-002, this lot needs no new catalogue format: every §08
value is byte, signed-byte (`sign, magnitude`) or one of the small enumerations already modelled by
`ValueFormat`. The one design question is RQ-AKM-031 — a Get issued while keygroup 0 ("all") is
current answers one value set per keygroup, in keygroup order — which reuses the repeated-record REPLY
decode designed once for §0A's `&18`/`&19` (ADR-AKM-001 DEC-AKM-014), checked here against the
program's keygroup count from §0A `&14` rather than the programs-in-memory count. TASK-AKM-026 applies
that design to §08's shape; whether it needs its own `DEC-AKM-*` entry (an extension, or different
enough to warrant one) is decided when the task starts.

**Safety note carried from FTR-AKM-002.** A Set while keygroup 0 is current writes every keygroup of
the *current program* at once; on a real user's program that is not reversible (RQ-AKM-033). Every
real-sampler check of this lot therefore runs inside FTR-AKM-002's `GuardedTestProgram`
(`XS56K_SUITE_TEST`), the same way TASK-AKM-024's checks did, and refuses before sending if the current
program is not that one.

## References
- **Requirements**: FTR-AKM-003 (RQ-AKM-028 to RQ-AKM-033)
- **ADRs**: ADR-AKM-001 (Accepted) — TASK-AKM-026 may add a decision to it for the all-keygroups reply,
  following TASK-AKM-014/017's precedent of extending the existing catalogue/codec architecture rather
  than opening a new ADR file.

The plan has 9 tasks (TASK-AKM-026 to TASK-AKM-034), within one session's carry limit: TASK-AKM-026
(selection plus the shared all-keygroups reply design — unlocks everything else); TASK-AKM-027 to 032
(the six parameter groups: General Options, Pitch/Amp, Filter, Filter Envelope, Amplitude Envelope, Aux
Envelope — independent of each other once 026 is done); TASK-AKM-033 (real-sampler test harness,
RQ-AKM-033, reusing FTR-AKM-002's `GuardedTestProgram`); TASK-AKM-034 (coverage and errata
reconciliation, RQ-AKM-032).

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-AKM-026: Keygroup selection and the all-keygroups reply
- **Tier**: L
- **Status**: Done
- **Description**: Implement `selectKeygroup(number)` (1–99, or 0 for "all") and `getCurrentKeygroup()`
  (`&01`/`&02`), reporting ERROR `181` as "keygroup not found (N)"; extend the repeated-record REPLY
  decode of ADR-AKM-001 (DEC-AKM-014) to §08's shape — a Get answered while keygroup 0 is current
  returns one value set per keygroup, in keygroup order starting at 1, checked against the current
  program's keygroup count (§0A `&14`, `getProgramKeygroupCount`) for a mismatch.
- **Requirement refs**: RQ-AKM-028, RQ-AKM-031
- **ADR refs**: ADR-AKM-001 (DEC-AKM-014, extended here for §08; a new `DEC-AKM-*` is added only if the
  shape does not fit as an extension)
- **Acceptance Criteria** (Gherkin): *Given* a program with 3 keygroups, *When* keygroup 2 is selected
  then `&02` runs, *Then* it reports 2. *Given* keygroup 9 in a 3-keygroup program, *When* selected,
  *Then* "keygroup not found (9)" is reported. *Given* keygroup 0, *When* selected then a Set runs,
  *Then* the Set is sent while keygroup 0 (all) is current, and the caller is told it applies to all
  keygroups. *Given* a 3-keygroup program with keygroup 0 current, *When* a representative Get (Low
  Note) runs, *Then* three values are returned in keygroup order. *Given* a REPLY holding two sets for
  a 3-keygroup program, *When* decoded, *Then* it fails with a mismatch error instead of a partial
  result.
- **Dependencies**: None (first task of the lot; builds on FTR-AKM-002's `ProgramPrimitives` for a
  current program and its keygroup count)
- **Assignee**: AI, with the owner running the real-sampler tests
- **Verification**: Windows/MSVC Debug: 0 warnings, `ctest` 402/402 (the pre-existing, unrelated
  `akm_item_catalogue_script_tests` Python failure excluded — reproduces identically on `main`, a
  `subprocess.run` capture issue under this machine's Python 3.14, not touched by this task).
  `KeygroupPrimitivesTests.cpp` (9 cases): selection reports back correctly, an unknown keygroup fails
  `KEYGROUP_NOT_IN_PROGRAM`, an out-of-range number is refused without sending, keygroup 0 selects and
  reports "all", a new program defaults to keygroup 1. `decodeRepeatedReply` proven directly (three
  one-byte records; a length not a whole multiple of a two-byte item's width fails). `getForAllKeygroups`
  proven end to end against `ProgramGetAllNumbers` as a stand-in repeatable item (two "keygroups" decode;
  a wrong expected count leaves `values` empty) — RQ-AKM-031's Gherkin against a *real* per-keygroup item
  (Low Note) and the mismatch AC on such an item are TASK-AKM-027's, once one is catalogued. Not
  verified: real sampler (no §08 item exists to select yet beyond `&01`/`&02` themselves, nothing to
  round-trip); mutation testing (blocked, as in prior tasks).
- **Assumptions**: A newly current program defaults to keygroup 1 (spec silent on §08/&02's answer before
  any `&01`); to be confirmed or corrected once TASK-AKM-033 runs on the real sampler. "ERROR 181" of
  RQ-AKM-028 is `error_number::KEYGROUP_NOT_IN_PROGRAM` (0x181 = 385 decimal), already defined for
  TASK-AKM-022's `deleteKeygroupFromProgram` — reused rather than duplicated, on the strength of both
  being "this keygroup does not exist in the current program". No bespoke "keygroup not found (N)"
  message wrapper was added: the caller already holds the number it requested and can compose that text
  itself from the raw `Error`, matching how `selectProgramByName`'s ERROR 04 is left undecorated.
  `getForAllKeygroups` takes the expected keygroup count as a parameter rather than fetching it itself
  (`getProgramKeygroupCount`), to avoid an implicit nested round trip; the caller is expected to already
  know it in the common case (having just read or set it). `KeygroupRecord` reuses `ProgramRecord`'s own
  generic `(item, selector) -> bytes` map shape rather than named fields, since every §08 group (General
  Options included, unlike a first glance at RQ-AKM-029 suggested) turns out to fit the same contiguous
  Set/Get-range pattern as §0A's five parameter groups — confirmed once the TSV rows were read in full for
  TASK-AKM-027 to 032's planning, not before.

---

### TASK-AKM-027: General Options group (Set and Get)
- **Tier**: M
- **Status**: Done
- **Description**: `setLowNote`/`getLowNote` (`&04`/`&0A`, 21–127), `setHighNote`/`getHighNote`
  (`&05`/`&0B`, 21–127), `setMuteGroup`/`getMuteGroup` (`&06`/`&0C`, 0 or 1–32),
  `setFxOverride`/`getFxOverride` (`&07`/`&0D`, 0=off/1=FX1/2=FX2/3=RV3/4=RV4),
  `setFxSendLevel`/`getFxSendLevel` (`&08`/`&0E`, 0–100), `setZoneCrossfade`/`getZoneCrossfade`
  (`&09`/`&0F`, 0 or 1) of the current keygroup.
- **Requirement refs**: RQ-AKM-029
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* a keygroup, *When* low note 36 and high note 60 are set
  then read back, *Then* the values are 36 and 60. *Given* a low note of 20, *When* set, *Then* it is
  refused without sending. *Given* each of the six items, *When* a value inside its range is set then
  read back on the simulated sampler, *Then* it round-trips. *Given* the real sampler and the test
  program of FTR-AKM-002, *When* the same test runs, *Then* it passes unchanged.
- **Dependencies**: TASK-AKM-026
- **Assignee**: AI, with the owner running the real-sampler tests
- **Verification**: Windows/MSVC Debug: 0 warnings, `ctest` 406/406 (same pre-existing Python failure
  excluded). `KeygroupGeneralOptionsTests.cpp` (4 cases): all six items round-trip; Low Note 20 is
  refused without sending; **RQ-AKM-031 proven end to end for the first time against a real per-keygroup
  item** (`&04`/`&0A`, Low Note) — a 3-keygroup program with keygroup 0 current, Set to 40, then
  `getForAllKeygroups` decodes three records, all 40; a wrong expected count (4, not 3) leaves `values`
  empty, closing TASK-AKM-026's deferred AC. Not verified: real sampler (no owner run yet — deferred to
  TASK-AKM-033, which round-trips every group at once); mutation testing (blocked, as in prior tasks).
- **Assumptions**: `KEYGROUP_PARAMETER_GROUP_RANGES` (`SimulatedSampler.cpp`, mirroring §0A's own
  `PARAMETER_GROUP_RANGES`) introduced here with General Options as its first row, grown one row per
  TASK-AKM-028 to 032 rather than declared all at once — matching TASK-AKM-018's own precedent for §0A.
  Mute Group's range is catalogued as a single `0-32` byte (the spec's "0=OFF, 1-32=value" is one
  contiguous range, not two), same choice as `KEYGROUP_NOT_IN_PROGRAM`-adjacent items elsewhere in this
  plan.

---

### TASK-AKM-028: Keygroup Pitch/Amp group (Set and Get)
- **Tier**: M
- **Status**: Done
- **Description**: `setSemitoneTune`/`getSemitoneTune` (`&10`/`&18`, sign + 0–36),
  `setFineTune`/`getFineTune` (`&11`/`&19`, sign + 0–50), `setKeygroupLevel`/`getKeygroupLevel`
  (`&12`/`&1A`, 0–10, −30 dB to +30 dB steps), `setPitchModValue`/`getPitchModValue` (`&13`/`&1B`,
  Pitch Mod 1 or 2, sign + 0–100), `setAmpModValue`/`getAmpModValue` (`&14`/`&1C`, Amp Mod 1 only,
  sign + 0–100) of the current keygroup.
- **Requirement refs**: RQ-AKM-030
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* each of the five items, *When* a value inside its range is
  set then read back on the simulated sampler, *Then* it round-trips. *Given* a value outside the
  range, *When* set, *Then* it is refused without sending. *Given* the real sampler, *When* the same
  test runs on the test program, *Then* it passes unchanged.
- **Dependencies**: TASK-AKM-026
- **Assignee**: AI, with the owner running the real-sampler tests
- **Verification**: Windows/MSVC Debug: 0 warnings, `ctest` 408/408 (same pre-existing Python failure
  excluded). `KeygroupPitchAmpTests.cpp` (2 cases): all five items round-trip, including both Pitch Mod
  instances (1 and 2); a Pitch Mod selector of 3 is refused without sending. Not verified: real sampler
  (deferred to TASK-AKM-033); mutation testing (blocked, as in prior tasks).
- **Assumptions**: `KEYGROUP_PARAMETER_GROUP_RANGES` grown by one row (`{0x10, 0x14, 0x08}`), no other
  change to the engine TASK-AKM-027 introduced.

---

### TASK-AKM-029: Filter group (Set and Get)
- **Tier**: M
- **Status**: Not Started
- **Description**: `setFilterMode`/`getFilterMode` (`&20`/`&28`, 0–25),
  `setFilterCutoff`/`getFilterCutoff` (`&21`/`&29`, 0–100),
  `setFilterResonance`/`getFilterResonance` (`&22`/`&2A`, 0–15),
  `setFilterKeyboardTrack`/`getFilterKeyboardTrack` (`&23`/`&2B`, sign + 0–36),
  `setFilterModInputValue`/`getFilterModInputValue` (`&24`/`&2C`, Mod Input 1, 2 or 3, sign + 0–100),
  `setFilterAttenuation`/`getFilterAttenuation` (`&25`/`&2D`, 0–5, 0 dB to 30 dB steps) of the current
  keygroup.
- **Requirement refs**: RQ-AKM-030
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* the filter mode values 0–25 and cutoff 0–100, resonance
  0–15, attenuation 0–5, *When* each is set then read back, *Then* it round-trips. *Given* a value
  outside the range, *When* set, *Then* it is refused without sending. *Given* the real sampler, *When*
  the same test runs on the test program, *Then* it passes unchanged.
- **Dependencies**: TASK-AKM-026
- **Assignee**: AI, with the owner running the real-sampler tests
- **Verification**: N/A — not started
- **Assumptions**: None

---

### TASK-AKM-030: Filter Envelope group (Set and Get)
- **Tier**: M
- **Status**: Not Started
- **Description**: `&30`–`&38` set / `&40`–`&48` get, nine pairs of the current keygroup's filter
  envelope: Attack (0–100), Velocity→Attack (sign + 0–100), Decay (0–100), Sustain (0–100), Release
  (0–100), On Velocity→Release (sign + 0–100), Keyscale (sign + 0–100), Depth (sign + 0–100), Off
  Velocity→Release (sign + 0–100). Resolves the KB erratum: `&48`'s REPLY row names "Off
  Velocity→Rate" while its Set (`&38`) and every other reading name "Off Velocity→Release" — decided
  by observation on the real sampler and recorded (part of RQ-AKM-032).
- **Requirement refs**: RQ-AKM-030, RQ-AKM-032 (the `&48` erratum only)
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* each of the nine items, *When* a value inside its range is
  set then read back on the simulated sampler, *Then* it round-trips. *Given* the real sampler, *When*
  `&38` (Off Velocity→Release) is set then `&48` is read, *Then* the value set is read back, resolving
  whether `&48` is the same parameter as `&38` regardless of its REPLY table label.
- **Dependencies**: TASK-AKM-026
- **Assignee**: AI, with the owner running the real-sampler tests
- **Verification**: N/A — not started
- **Assumptions**: None

---

### TASK-AKM-031: Amplitude Envelope group (Set and Get)
- **Tier**: M
- **Status**: Not Started
- **Description**: `&50`–`&57` set / `&58`–`&5F` get, eight pairs of the current keygroup's amplitude
  envelope: Attack (0–100), Velocity→Attack (sign + 0–100), Decay (0–100), Sustain (0–100), Release
  (0–100), On Velocity→Release (sign + 0–100), Keyscale (sign + 0–100), Off Velocity→Release (sign +
  0–100). Resolves the analogous KB erratum: `&5F`'s REPLY row names "Off Velocity→Rate" while its Set
  (`&57`) names "Off Velocity→Release" (part of RQ-AKM-032).
- **Requirement refs**: RQ-AKM-030, RQ-AKM-032 (the `&5F` erratum only)
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* each of the eight items, *When* a value inside its range
  is set then read back on the simulated sampler, *Then* it round-trips. *Given* the real sampler,
  *When* `&57` is set then `&5F` is read, *Then* the value set is read back, resolving the erratum the
  same way as TASK-AKM-030's `&48`.
- **Dependencies**: TASK-AKM-026
- **Assignee**: AI, with the owner running the real-sampler tests
- **Verification**: N/A — not started
- **Assumptions**: None

---

### TASK-AKM-032: Aux Envelope group (Set and Get)
- **Tier**: M
- **Status**: Not Started
- **Description**: `setAuxEnvRate`/`getAuxEnvRate` (`&60`/`&68`, Aux Rate 1–4, 0–100),
  `setAuxEnvVelocityToRate`/`getAuxEnvVelocityToRate` (`&61`/`&69`, Aux Rate 1 or 4, sign + 0–100),
  `setAuxEnvKeyboardToR2R4`/`getAuxEnvKeyboardToR2R4` (`&62`/`&6A`, sign + 0–100),
  `setAuxEnvLevel`/`getAuxEnvLevel` (`&63`/`&6B`, Aux Level 1–4, 0–100), and the fifth pair
  `&64`/`&6C` — the spec's own `&64` command row restricts it to "Aux Rate 4 only" and its REPLY `&6C`
  reads "Off Velocity→Rate (Aux Rate 4 only)"; the two KB erratum items ( `&6C` numbered 107, a
  duplicate of `&6B`'s number instead of 108, and whether `&64`/`&6C` name a distinct parameter from
  `&61`/`&69` or the same one under a second, Rate-4-only alias) are decided by observation on the real
  sampler (part of RQ-AKM-032).
- **Requirement refs**: RQ-AKM-030, RQ-AKM-032 (the `&6C` numbering and `&64`/`&6C` naming only)
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* each of the five items, *When* a value inside its range is
  set then read back on the simulated sampler, *Then* it round-trips. *Given* the real sampler, *When*
  `&61` (Aux Rate 4, Velocity→Rate) is set and then `&64`/`&6C` (Aux Rate 4) is set/read, *Then* whether
  they alias the same stored value or are distinct is observed and recorded.
- **Dependencies**: TASK-AKM-026
- **Assignee**: AI, with the owner running the real-sampler tests
- **Verification**: N/A — not started
- **Assumptions**: None

---

### TASK-AKM-033: Real-sampler test harness — keygroups of the dedicated test program
- **Tier**: L
- **Status**: Not Started
- **Description**: Extend `xs56k_akm_probe --suite` with the §08 checks this lot unlocks, reusing
  FTR-AKM-002's `GuardedTestProgram` (`XS56K_SUITE_TEST`): add keygroups to it, round-trip every Set/Get
  pair of TASK-AKM-027 to 032 on its keygroups, and exercise the keygroup-0 ("all") Get/Set shape of
  TASK-AKM-026 on it. Refuse before sending if a check would select keygroup 0 for a Set, or select any
  keygroup at all, while the current program is not the test program.
- **Requirement refs**: RQ-AKM-033
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* a current program that is not the test program, *When* a
  check tries a Set or a select of keygroup 0, *Then* it is refused before sending. *Given* the suite
  ended, *When* the sampler is inspected, *Then* only its original programs remain, unchanged. *Given*
  the real sampler and the test program, *When* the extended check runs, *Then* every TASK-AKM-027 to
  032 item round-trips on real hardware.
- **Dependencies**: TASK-AKM-026, TASK-AKM-027, TASK-AKM-028, TASK-AKM-029, TASK-AKM-030, TASK-AKM-031,
  TASK-AKM-032; FTR-AKM-002 (TASK-AKM-024, `GuardedTestProgram`)
- **Assignee**: AI, with the owner running the real-sampler suite
- **Verification**: N/A — not started
- **Assumptions**: None

---

### TASK-AKM-034: Coverage of section §08 and remaining errata resolution
- **Tier**: M
- **Status**: Not Started
- **Description**: Add the 80 command rows and 40 REPLY rows of section `08` to `items.json` coverage
  (or list an explicit exclusion reason for any row this lot does not implement) and run
  `generate_akm_items.py --coverage` to confirm none is unaccounted for. Resolve the KB erratum not
  already settled by TASK-AKM-030/031/032 in passing — `&6C` numbered 107 instead of 108 (a
  documentation-only slip, confirmable from the TSV alone, no hardware needed) — and record every
  §08 erratum's resolution in the KB in one place.
- **Requirement refs**: RQ-AKM-032
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* the 80 command rows and 40 REPLY rows of section `08`,
  *When* compared with the catalogue of primitives and exclusions, *Then* no row is unaccounted for.
  *Given* each erratum listed in RQ-AKM-032, *When* the lot ends, *Then* its resolution is recorded
  with the observation that supports it.
- **Dependencies**: TASK-AKM-027, TASK-AKM-028, TASK-AKM-029, TASK-AKM-030, TASK-AKM-031, TASK-AKM-032,
  TASK-AKM-033
- **Assignee**: AI, with the owner running the real-sampler observation
- **Verification**: N/A — not started
- **Assumptions**: None
