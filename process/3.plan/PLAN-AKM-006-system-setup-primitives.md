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
- **Status**: Done
- **Description**: Catalogue `&04`, `&30`, `&31`, `&33`, `&34` and expose typed Gets: the model as an
  enumeration (a byte other than `0`/`1` is malformed), the two percentages, and the two byte counts decoded
  from the compound double word. Settle `Dword` against the four-`Byte` split of §0E.
- **Requirement refs**: RQ-AKM-053
- **ADR refs**: ADR-AKM-001 (DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-053 on the simulated sampler.
- **Dependencies**: TASK-AKM-047
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: build with no warning or error (`/W4 /WX`), `ctest` 464/464 after
  a re-run in this session. 5 new cases in `SystemSetupTests.cpp` (`[akm][system]`, `ctest -R RQ-AKM-053`):
  an S6000 with 64 MiB of Wave memory, 16 MiB free and 40 % MPKS free reads back S6000, 25 %, 40 %, 67108864
  and 16777216 (the byte counts need all four data bytes); the factory sampler is an S5000; a model byte `2`
  or an MPKS percentage `101` yields no value while the outcome stays the REPLY; a sampler with no Wave
  memory reads 0 and 0; with the checksum mode unknown all of them are answered, their REPLYs having a
  fixed length. `ItemCatalogueTests.cpp`: the five records added to its `CATALOGUE` table (REPLY length 1,
  1, 1, 4, 4). `generate_akm_items.py --check`: up to date (253 items); `--coverage`: section `02` 9 of 16
  command rows covered, `unaccounted: none`. Not verified: real sampler (TASK-AKM-053); mutation testing.
- **Assumptions**: `ValueFormat::Dword` fits and is used (one value, four data bytes, up to 268435455 —
  256 MiB less one byte, ample for the 32 to 256 MiB of sample memory these samplers take, and what the
  spec's own encoding can express); the §0E split into four `Byte` values was not repeated. The spec
  writes the REPLY rows of `&33`/`&34` with two columns, "MSB" and "LSB" (the compound *word* notation),
  while its text calls them compound *double* words — an inconsistency of the same family as the
  Play Mode range (`sysex_spec.kb.md`, errata): four bytes are assumed, the total of a memory in bytes not
  fitting 14 bits, and `decodeReply` refuses a REPLY of another length, so the real sampler will show it
  at TASK-AKM-053. The coverage check does not compare these two rows (the ellipsis makes them
  free text), so no exception was needed in `generate_akm_items.py`. The Wave percentage of the simulated
  sampler is derived from its byte counts, rounded down, and 0 for a sampler with no memory; the MPKS
  percentage is stored as given — modelling choices. A percentage above 100 or a model byte outside
  0/1 is reported as no value rather than clamped (RQ-AKM-053's own Gherkin: "reported as malformed,
  not guessed"); `SamplerModelResult`/`MemoryPercentResult` keep the outcome so the caller still sees
  the REPLY.

---

### TASK-AKM-050: Clock and date (Set and Get)
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue `&05` and `&06` and expose a typed `ClockDate` with field-range refusal
  before sending (year 1980–2079, month 1–12, day 1–31, weekday 1–7, hours 0–23, minutes and seconds
  0–59).
- **Requirement refs**: RQ-AKM-054
- **ADR refs**: ADR-AKM-001 (DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-054 on the simulated sampler.
- **Dependencies**: TASK-AKM-047
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: build with no warning or error (`/W4 /WX`), `ctest` 468/468 after
  a re-run in this session. 4 new cases in `SystemSetupTests.cpp` (`[akm][system]`, `ctest -R RQ-AKM-054`):
  Thursday 2026-10-01 14:30:15 set with frame data bytes `0F 6A 0A 01 05 0E 1E 0F` and read back equal;
  1980-01-01 and 2079-12-31 23:59:59 round-trip; 13 clocks, each with one field just outside its range
  (year 1979/2080, month 0/13, day 0/32, day of week 0/8, hours -1/24, minutes 60, seconds -1/60), are
  refused `ArgumentOutOfRange` with nothing sent and `invalidClockField`/`clockFieldName` naming the field,
  while the three valid clocks name none; with the checksum mode unknown the Get is answered (fixed
  8-byte REPLY). `ItemCatalogueTests.cpp`: the two records added to its `CATALOGUE` table (REPLY length 8
  for the Get). `generate_akm_items.py --check`: up to date (255 items); `--coverage`: section `02` 11 of 16
  command rows covered, `unaccounted: none`. Collateral: `SimulatedSamplerTests.cpp`'s "ERROR 0 for an
  item of section 02 other than the version ones" sent `&05`, now modelled; it sends `&07`, which the spec
  does not have, and says so in its title — a change reflecting the new expected state. One bug of my own
  found by the new test on its first run (a reference taken on the by-value `results().back()` dangled),
  fixed in the test. Not verified: real sampler (TASK-AKM-053); mutation testing.
- **Assumptions**: The year is read as the whole year, 1980-2079, in a compound word (MSB, LSB): 2079 is
  MSB 16, exactly the largest the spec's "0–16 (MSB year)" column allows, where an offset from 1980 would
  never exceed MSB 0 — a strong argument, not an observation; TASK-AKM-053 confirms it on the real
  sampler. It is catalogued as one `Word` (1980-2079) rather than two `Byte` values: the spec row carries
  an ellipsis, so the coverage check does not compare it, and the generic encoder then gives the year
  range check for free. The day of week is the caller's to give (the spec makes it a field of its own) and
  is not checked against the date. RQ-AKM-054 first said "an error names the field"; `Refused` carries a
  reason only, and widening it for one item would be a cross-cutting change, so the field is named by
  `invalidClockField`/`clockFieldName` instead and the refusal reason stays `ArgumentOutOfRange`. The
  owner agreed (session AKM, 2026-10-01) and RQ-AKM-054's statement and Gherkin were reworded to say so. The simulated sampler refuses a Set whose fields are outside the same ranges with
  ERROR `OUT_OF_RANGE`, keeps the clock across `powerCycle()`, and does not tick: its clock only changes
  by a Set — so the restoration "advanced by the elapsed time" of RQ-AKM-058 is for TASK-AKM-053 to
  model and test.

---

### TASK-AKM-051: Play Mode and front-panel lock (Set and Get)
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue `&10`/`&20` and `&11`/`&21`, expose typed Set/Get, refuse out-of-range
  values before sending; Play Mode accepts `0`–`3` as the item text defines (`3 = Muted`), the data range
  column's "0, 1, 2" being the known erratum, pending the real sampler (TASK-AKM-054).
- **Requirement refs**: RQ-AKM-055
- **ADR refs**: ADR-AKM-001 (DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-055 on the simulated sampler.
- **Dependencies**: TASK-AKM-047
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug, after a clean rebuild (`--clean-first`): no warning or error
  (`/W4 /WX`), `ctest` 473/473 re-run in this session. 5 new cases in `SystemSetupTests.cpp`
  (`[akm][system]`, `ctest -R RQ-AKM-055`): each of the four Play Modes is set (frame data byte 0, 1, 2, 3)
  and read back; the lock set to locked then normal (frame bytes 1 then 0) reads back each time and is
  normal at the start; a Play Mode cast from 4 and a lock cast from 2 are refused `ArgumentOutOfRange` with
  nothing sent; a REPLY of Play Mode 4 or lock 2 yields no value while the outcome stays the REPLY; with the
  checksum mode unknown both Gets are answered. `ItemCatalogueTests.cpp`: the four records added to its
  `CATALOGUE` table. `generate_akm_items.py --check`: up to date (259 items); `--coverage`: section `02`
  15 of 16 command rows covered (only `&32`, TASK-AKM-052's), `unaccounted: none`. Two new cases in
  `test_generate_akm_items.py` (run inside `akm_item_catalogue_script_tests`): the Play Mode catalogued 0-3
  passes with a note on the erratum; widened to 0-4 it is reported against 0..3. Not verified: real
  sampler (the Play Mode 3 and the whole of the two Set/Get pairs, TASK-AKM-053/054); mutation testing.
  Process note: a first build after a half-applied edit left three zone tests (`RQ-AKM-036`) failing against
  a catalogue table that two translation units saw differently; they passed again after a rebuild, and a
  clean rebuild gave 473/473 — recorded here, no source change involved.
- **Assumptions**: The Play Mode is accepted over 0-3, the range the item's text defines, as the owner
  decided (session AKM, 2026-10-01); the spec's data column "0, 1, 2" is an erratum, resolved on the real
  sampler by TASK-AKM-054. The coverage checker compares the catalogue with that 0..3 range instead of the
  column (`KNOWN_RANGE_ERRATA`, a new exception of the same family as `KNOWN_DEC_ERRATA`), so a catalogue
  range that drifts from the text is still reported; it covers `&10` and `&20` of section `02` only. The
  Play Mode and the lock are typed enumerations; the refusal of an out-of-range value is testable by casting
  an integer into one. The simulated sampler starts in Program mode with the panel normal (the spec gives no
  default), refuses a Set outside 0-3 / 0-1 with `OUT_OF_RANGE`, and keeps both across `powerCycle()` — a
  modelling choice.

---

### TASK-AKM-052: Destructive command guard for "Clear Sampler Memory"
- **Tier**: M
- **Status**: Done
- **Description**: Implement `&32` so that it is sent only with an explicit confirmation argument no
  default supplies (a dedicated type, mirroring `TASK-AKM-023`/`TASK-AKM-041`) and audit the real-sampler
  test sources for any call.
- **Requirement refs**: RQ-AKM-056
- **ADR refs**: ADR-AKM-001 (DEC-AKM-011)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-056.
- **Dependencies**: TASK-AKM-047
- **Assignee**: AI, with the owner's approval (DoR)
- **Verification**: Windows/MSVC Debug: no warning or error (`/W4 /WX`), `ctest` 476/476 re-run in this
  session. New `SystemClearMemoryGuardTests.cpp` (3 cases, `[akm][system][delete-all-guard]`,
  `ctest -R RQ-AKM-056`): without the confirmation nothing is sent, `NotConfirmed` is reported and the
  simulated sampler accepted no command; with it, item 32 of section 02 is sent with no data (checked on
  the sampler's accepted command) and, after two programs, two samples and one multi were seeded, the program
  count is 0, selecting sample 0 fails ERROR 04 and no multi remains; and a sampler with 16 of 64 MiB of Wave
  memory free and 40 % of its MPKS memory free reads 100 % free of both afterwards. `ItemCatalogueTests.cpp`:
  the record added to its `CATALOGUE` table. `generate_akm_items.py --check`: up to date (260 items);
  `--coverage`: section `02` 16 of 16 command rows covered, `unaccounted: none`. Audit:
  `grep -rn "clearSamplerMemory\|ConfirmClearSamplerMemory\|SystemClearMemory" juce/tests/support
  juce/tests/probe` — no match (exit 1), so no real-sampler test, first-contact probe or session test calls
  the primitive (the simulator's own executor is named `executeClearMemory` so the audit stays clean). Not
  verified: real sampler — by design and by RQ-AKM-056 it never will be; mutation testing.
- **Assumptions**: Tier M rather than S: a new public type and file (`TASK-AKM-041` was S because it only
  specialised an existing shape). Per the owner (session AKM, 2026-10-01) the multis stay among what the
  command deletes: the confirmation enumerator is named for programs, multis and samples, and the simulated
  sampler holds multis by name (`setMultiNames`, `multiCount`) though no §0C item exists, so that §02/&32
  deletes all three kinds there as the spec says. After a clear the simulated sampler's Wave memory and MPKS
  memory read entirely free — the spec says nothing of it, a modelling choice. How long the real sampler takes
  to answer `&32` is unknown (`F0 F7`/Still Alive handling for slow operations is itself unsettled,
  RQ-AKM-011), so the default timeout applies and is documented as such in `SystemSetup.hpp`. The
  catalogue's section `02` stays `complete: false` until TASK-AKM-054. A test name must not contain a
  non-ASCII character such as "§": ctest passes it to the binary in another encoding and the test is then
  never found (the first run of the new test failed for that reason alone).

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
