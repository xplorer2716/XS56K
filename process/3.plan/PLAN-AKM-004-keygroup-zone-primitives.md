# PLAN-AKM-004: Keygroup Zone Primitives (Phase A, lot A4)

## Overview

Plan for FTR-AKM-004, section §06 (Keygroup Zone) of the S5000 control plan
(`process/1.requirements/DRAFT-s5000-midi-primitives-and-workflows.md`, lot A4): 28 commands and 14
REPLY formats — assign a sample to a zone by name, and Set/Get a zone's level, pan/balance, output,
filter, fine and semitone tune, keyboard track, playback mode, velocity→start, high/low velocity,
mute and solo — each proven by a Set followed by a Get on the mock, then on the real sampler against
the dedicated test program of FTR-AKM-002 (RQ-AKM-027) and the keygroups TASK-AKM-033 added to it.

**Unlike §0A (Program) and §08 (Keygroup), §06 has no "current" selection state of its own.** Every
§06 message carries the zone number (1–4, or 0 for "all four") as its first data byte and acts on
the *current keygroup of the current program* (`documents/_index/sysex_spec.kb.md`, state model:
"§06: zone number is d1 of every message ... refers to current KG of current program"). So this lot
adds no `selectZone`/`getCurrentZone` primitive — there is none in the spec — and needs no new
"unlocking" task the way TASK-AKM-026 was for §08.

**Prerequisite already met.** Every non-sample §06 value is byte, signed-byte (`sign, magnitude`) or
one of the small enumerations already modelled by `ValueFormat` (`ADR-AKM-001`, DEC-AKM-003/012); the
sample item reuses `ValueFormat::String` and `makeStringRequest`/`decodeStringReply`
(DEC-AKM-013, first built for Program names); the "several zones" REPLY shape reuses
`decodeRepeatedReply` (DEC-AKM-015), already generalised "for any item," not only keygroups. The one
new question is whether the nested "zone 0 + keygroup 0" shape (RQ-AKM-036: sets for every zone of
every keygroup) needs its own decode helper or can be handled by chunking a flat `decodeRepeatedReply`
result into groups of 4 in the caller, the way `getForAllKeygroups` (`KeygroupPrimitives.cpp`) already
checks a flat count against an expected total rather than reshaping itself — decided when TASK-AKM-037
starts, following TASK-AKM-026's own precedent for that kind of call.

**Safety note carried from FTR-AKM-002/003.** A Set with zone 0 writes all four zones of the current
keygroup at once; combined with keygroup 0, it writes every zone of every keygroup of the *current
program*, not reversible on a real user's program (RQ-AKM-033's rule, extended here to zones). Every
real-sampler check of this lot runs inside FTR-AKM-002's `GuardedTestProgram` (`XS56K_SUITE_TEST`,
`RealSamplerSuite.cpp`), refusing before sending if the current program is not that one — the same
guard TASK-AKM-033 already relies on for keygroups.

## References
- **Requirements**: FTR-AKM-004 (RQ-AKM-034 to RQ-AKM-038)
- **ADRs**: ADR-AKM-001 (Accepted) — this lot is expected to extend it (item catalogue growth, and
  possibly a `DEC-AKM-*` for the nested zone×keygroup repeated-REPLY shape), not open a new ADR file,
  following TASK-AKM-014/017/026's precedent of extending the existing catalogue/codec architecture.

The plan has 5 tasks (TASK-AKM-035 to TASK-AKM-039), within one session's carry limit: TASK-AKM-035
(the 13 non-sample zone parameters — one task, since unlike §08 the spec's own §06 tables are not
subdivided into named groups); TASK-AKM-036 (sample assignment by name, independent of 035);
TASK-AKM-037 (the "several zones" REPLY shapes, depends on 035 for a real item to decode and on
FTR-AKM-003's `getForAllKeygroups` for the keygroup dimension); TASK-AKM-038 (real-sampler test
harness, RQ-AKM-038, depends on 035–037); TASK-AKM-039 (coverage and the `&27` erratum resolution,
RQ-AKM-037, depends on 038 for the real-sampler observation the erratum's resolution needs).

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-AKM-035: Zone parameters (Set and Get)
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue the 13 non-sample §06 items — Level (`&02`/`&22`, sign + 0–100),
  Pan/Balance (`&03`/`&23`, 14–114, centre 64), Output (`&04`/`&24`, 0–24), Filter (`&05`/`&25`, sign
  + 0–100), Fine Tune (`&06`/`&26`, sign + 0–50), Semitone Tune (`&07`/`&27`, sign + 0–36), Keyboard
  Track (`&08`/`&28`, 0/1), Playback (`&09`/`&29`, 0–6), Velocity→Start (`&0A`/`&2A`, sign + 0–9999,
  MSB/LSB), High Velocity (`&0B`/`&2B`, 0–127), Low Velocity (`&0C`/`&2C`, 0–127), Mute (`&0D`/`&2D`,
  0/1), Solo (`&0E`/`&2E`, 0/1) — of the current keygroup, addressed by zone number (1–4) as first
  data byte. Catalogues `&27`'s REPLY as the Get despite its command-table label reading "Set Zone
  Semitone Tune" (the `&27`/erratum, part of RQ-AKM-037), recording the reading in the KB.
- **Requirement refs**: RQ-AKM-034, RQ-AKM-037 (the `&27` label only)
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012 — catalogue growth only)
- **Acceptance Criteria** (Gherkin): *Given* each of the 13 items, *When* a value inside its range is
  set on zone 2 of the current keygroup then read back on the simulated sampler, *Then* it equals the
  value set and zones 1, 3 and 4 are unchanged. *Given* a zone number 5, *When* set, *Then* it is
  refused without sending. *Given* the real sampler and the test program of FTR-AKM-002, *When* the
  same test runs, *Then* it passes unchanged.
- **Dependencies**: None
- **Assignee**: AI, with the owner running the real-sampler tests
- **Verification**: Windows/MSVC Debug: 0 warnings, `ctest` 421/421 (the pre-existing, unrelated
  `akm_item_catalogue_script_tests` Python failure excluded — a `subprocess.run` capture issue under
  this machine's Python 3.14, same as TASK-AKM-026's own note, not touched by this task).
  `ZoneParametersTests.cpp` (3 cases): all 13 items round-trip on zone 2; zones 1, 3 and 4 read back
  unchanged (0, 0) after a zone-2 Set; zone 5 is refused without sending. `--coverage` reports section
  `06` 26/28 rows covered (partial, as declared: `&01`/`&21` sample assignment is TASK-AKM-036's).
  `&27`'s REPLY is catalogued as the Get (RQ-AKM-037's erratum) despite its own command-table label;
  not yet confirmed by real-sampler observation (TASK-AKM-039's). Not verified: real sampler (deferred
  to TASK-AKM-038); mutation testing (blocked, as in prior tasks, RQ-BLD-015/TASK-BLD-012 not done).
- **Assumptions**: Velocity→Start (`&0A`/`&2A`) is catalogued as four separate byte args/reply values
  (zone, sign, magnitudeMsb, magnitudeLsb) rather than one combined `signed_word` value, even though
  the latter would enforce the spec's stated ±9999 ceiling exactly and the codec already supports it
  (`ValueFormat::SignedWord`, unused elsewhere in the catalogue) — `generate_akm_items.py --coverage`
  parses the spec's own range columns into one domain per wire byte (`spec_domains`), and every existing
  multi-byte item in the catalogue (e.g. `ProgramGetCount`'s count MSB/LSB) already follows the
  one-arg-per-wire-byte convention; matching it keeps the coverage check meaningful and avoids being the
  first exception. Consequence accepted: `magnitudeMsb`/`magnitudeLsb` are independently range-checked
  at 0-127 each (the per-byte maximum), not against the combined ±9999 bound — a combination like
  MSB=127, LSB=127 (16383) is not refused client-side, the same limitation every other MSB/LSB pair in
  this catalogue already has (no existing item enforces a combined bound tighter than its widest byte).
  Fixed a genuine bug in `spec_domains` (`generate_akm_items.py`), surfaced by this task: a REPLY row
  whose second range column holds only embedded `<DataN>` references (no field of its own before the
  first one, e.g. `&2A`'s REPLY) was double-counted as an extra domain, wrongly flagging a correctly
  3-field reply as "differs from the spec" (4 domains expected instead of 3). Verified the fix changes
  nothing for any previously-covered row: no section-00/0A/08 spec row has this shape (checked by
  grepping the TSV), and `--coverage` still reports 00/0A/08 fully covered, identically, after the fix.
  `SimulatedSampler`'s zone storage does not yet fan out a Set/Get with zone 0 to all four zones (RQ-
  AKM-034's own ACs only exercise zone 2 and zone 5's refusal) — left to TASK-AKM-037, which needs that
  shape anyway for RQ-AKM-036's repeated-REPLY decode. Zone parameters are stored in a new
  `KeygroupRecord::zoneParameters` map, separate from `parameters`, because §06 and §08 item codes
  overlap (both have a `&04`, for instance) and a shared map would silently collide the two sections'
  values.

---

### TASK-AKM-036: Zone sample assignment by name
- **Tier**: M
- **Status**: Done
- **Description**: `Set Zone Sample` (`&01`, zone number + name as `ValueFormat::String`, reusing
  `makeStringRequest`/DEC-AKM-013) and `Get Zone Sample` (`&21`, `decodeStringReply`), reporting
  ERROR `04` as "sample not found (<name>)" and the REPLY's single byte `00` case as "no sample
  assigned" rather than an empty string.
- **Requirement refs**: RQ-AKM-035
- **ADR refs**: ADR-AKM-001 (DEC-AKM-013)
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding a sample `KICK`, *When* it
  is assigned to zone 1, *Then* the frame carries section `06`, item `01`, data `01` then
  `4B 49 43 4B 00`, and the command completes on DONE and `&21` returns `KICK`. *Given* a zone with
  no sample and a REPLY of the single byte `00`, *When* `&21` is read, *Then* "no sample assigned" is
  returned. *Given* a name that does not exist, *When* assigned, *Then* "sample not found" is
  reported. *Given* a name containing a byte above `7F`, *When* assigned, *Then* it is refused
  without sending.
- **Dependencies**: None
- **Assignee**: AI, with the owner running the real-sampler tests
- **Verification**: Windows/MSVC Debug: 0 warnings, `ctest` 426/426 (the same pre-existing
  `akm_item_catalogue_script_tests` Python failure excluded). `ZoneSampleTests.cpp` (5 cases): a
  configured sample (`KICK`) assigns and reads back, the frame's data checked byte for byte (`01 4B 49
  43 4B 00`); an unset zone's Get returns an empty (present, not absent) name — the single-`00`-byte
  REPLY; an unconfigured name fails ERROR 04; a byte above `7F` is refused without sending
  (`RefusalReason::NotEncodable`); a zone outside 0-4 is refused without sending
  (`ArgumentOutOfRange`). `--coverage` now reports section `06` 28/28 (complete). Two other tests needed
  updating for reasons unrelated to this task's own logic but caused by it: `ItemCatalogueTests.cpp`'s
  total-item-count assertion (+2, one already anticipated in TASK-AKM-035's own commit) and its
  "an item of the spec that is not catalogued" negative example, which had used section 06 item `01` —
  now catalogued — swapped for section 02 item `02` (still partial, only its two version items are
  catalogued). Not verified: real sampler (deferred to TASK-AKM-038); mutation testing (blocked, as in
  prior tasks).
- **Assumptions**: The sample name's length bound (`0-255`, `STRING_MAX_LENGTH`) is the generator's
  generic structural ceiling, not a real hardware limit — unlike Program names (0-20), no owner
  observation exists yet for Sample names (DEC-AKM-013 says each name field keeps its own bound "when
  their lot catalogues them"); TASK-AKM-038/039's real-sampler run is where this gets confirmed or
  tightened, the same way Program's 20-character bound was found. `setZoneSample`/`getZoneSample` are
  hand-written wrappers (`ZonePrimitives.hpp/.cpp`, a new file pair, registered in
  `juce/akm/CMakeLists.txt`) rather than going through `makeRequest`/`makeStringRequest`, because
  neither fits the byte+String mixed shape — the same reason `createProgramWithKeygroups` (§0A/&03)
  already bypasses them; `RQ-AKM-035`'s own "IF ERROR 04 THEN report it as 'sample not found' with the
  name given" is left undecorated at this layer, matching `selectProgramByName`'s own ERROR 04
  precedent (TASK-AKM-026's assumption): the caller already holds the name and composes that text
  itself from the raw `Error`. `SimulatedSampler` gained a `setSampleNames` knob (empty by default,
  mirroring `setBehaviour`) and a dedicated `executeZone` dispatch for `&01`/`&21` ahead of
  `executeZoneParameterGroup`, storing the encoded name (with its `00` terminator) in the existing
  `zoneParameters` map under the *Set* item's own code — an unset zone's default (`Bytes{0}`, a lone
  terminator) already decodes as "no sample assigned" with no separate code path needed. Keygroup 0
  ("all") fan-out is deliberately not modelled for this item (`executeZone` fails `NOT_FOUND` instead):
  RQ-AKM-035's own acceptance criteria never exercise it, unlike the 13 numeric items, which get it for
  free from the shared `executeZoneParameterGroup` code.

---

### TASK-AKM-037: Replies covering several zones
- **Description context**: extends `decodeRepeatedReply` usage (already generalised "for any item,"
  DEC-AKM-015) to §06: a Get with zone 0 decodes into 4 value sets (zone order 1–4); a Get with zone 0
  while keygroup 0 is current decodes into one set per zone of every keygroup (keygroup 1 first,
  zones 1–4 within each). A mismatched count fails with a mismatch error instead of a partial result,
  the same contract `getForAllKeygroups` already gives (RQ-AKM-031, TASK-AKM-026).
- **Tier**: M
- **Status**: Not Started
- **Requirement refs**: RQ-AKM-036
- **ADR refs**: ADR-AKM-001 (DEC-AKM-014, DEC-AKM-015 — extended for the zone and zone×keygroup
  shapes; a new `DEC-AKM-*` is added only if the nested shape does not fit as an extension, decided
  when this task starts)
- **Acceptance Criteria** (Gherkin): *Given* a keygroup and zone 0, *When* Get Zone Level runs,
  *Then* four values are returned in zone order. *Given* a 2-keygroup program, keygroup 0 current and
  zone 0, *When* Get Zone Level runs, *Then* eight values are returned, indexed by keygroup and zone.
  *Given* a REPLY holding three sets where four are implied, *When* decoded, *Then* it fails with a
  mismatch error.
- **Dependencies**: TASK-AKM-035 (needs a catalogued §06 item to decode against); FTR-AKM-003
  (`getForAllKeygroups`, TASK-AKM-026) for the keygroup dimension
- **Assignee**: AI, with the owner running the real-sampler tests
- **Verification**: Not yet run.
- **Assumptions**: None yet — recorded when the task starts.

---

### TASK-AKM-038: Real-sampler test harness — zones of the dedicated test program
- **Tier**: L
- **Status**: Not Started
- **Description**: Extend `xs56k_akm_probe --suite` (`RealSamplerSuite.cpp`) with the §06 checks this
  lot unlocks, reusing `GuardedTestProgram` and the keygroups TASK-AKM-033 already adds to it: round-
  trip every item of TASK-AKM-035 on a zone of one of those keygroups, exercise the zone-0 ("all four")
  and zone-0+keygroup-0 shapes of TASK-AKM-037, and implement RQ-AKM-038's rule for sample assignment
  — run only when the operator configures a sample name already in the sampler's memory, reported as
  skipped (not passed) otherwise, and never creating, changing or deleting a sample.
- **Requirement refs**: RQ-AKM-038
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* a current program that is not the test program, *When* a
  check tries a Set with zone 0, *Then* it is refused before sending. *Given* no sample name in the
  test configuration, *When* the suite runs, *Then* the sample-assignment tests are reported as
  skipped with the reason and all other §06 tests run. *Given* a configured sample name, *When* the
  suite ends, *Then* the sampler holds the same samples as before and only the test program's zones
  were changed. *Given* the real sampler, *When* the extended check runs, *Then* every TASK-AKM-035 to
  037 item round-trips on real hardware.
- **Dependencies**: TASK-AKM-035, TASK-AKM-036, TASK-AKM-037; FTR-AKM-003 (TASK-AKM-033,
  `keygroupsOnTestProgram`)
- **Assignee**: AI, with the owner running the real-sampler suite
- **Verification**: Not yet run.
- **Assumptions**: None yet — recorded when the task starts.

---

### TASK-AKM-039: Coverage of section §06 and remaining errata resolution
- **Tier**: M
- **Status**: Not Started
- **Description**: Confirm `generate_akm_items.py --coverage` reports section `06` complete (28
  command rows, 14 REPLY rows, no row unaccounted for). Resolve the `&27` erratum (REPLY labelled
  "Set Zone Semitone Tune" where the Get is meant, `documents/_index/sysex_spec.kb.md` errata list) by
  observation on the real sampler — set Semitone Tune (`&07`) then read `&27`, confirming it returns
  the value set — and record the resolution in the KB.
- **Requirement refs**: RQ-AKM-037
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* the 28 command rows and 14 REPLY rows of section `06`,
  *When* compared with the catalogue of primitives and exclusions, *Then* no row is unaccounted for.
  *Given* the `&27` erratum, *When* the lot ends, *Then* its resolution is recorded with the
  observation that supports it.
- **Dependencies**: TASK-AKM-035, TASK-AKM-036, TASK-AKM-037, TASK-AKM-038
- **Assignee**: AI, with the owner running the real-sampler observation
- **Verification**: Not yet run.
- **Assumptions**: None yet — recorded when the task starts.
