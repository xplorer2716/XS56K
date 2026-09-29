# PLAN-AKM-005: Sample Primitives (Phase A, new lot)

## Overview

Plan for FTR-AKM-005, section §0E (Sample) of the S5000 control plan
(`process/1.requirements/DRAFT-s5000-midi-primitives-and-workflows.md`): 34 commands and 19 REPLY
formats (53 rows) — select, delete, rename and audition the current sample; read the sampler's
general information about the samples it holds; Set/Get its start/end position, original pitch,
semitone/fine tune, playback mode and loop start/end; and read its type, channel count, length and
rate — each proven by a Set followed by a Get (or a Get alone for the four read-only items) on the
mock, then on the real sampler against a dedicated test sample the operator loads via front panel
(no Disk lot exists yet to do it from software).

**Unlike §06 (Keygroup Zone), but like §0A (Program), §0E has its own "current" selection state.**
`&05`/`&06` (select by name / by index) set the sampler-wide *current sample* every later item in
this lot acts on — the same pattern `TASK-AKM-015` already built for Program, not the zone-number-
as-first-data-byte pattern of `§06`. Unlike Program, Keygroup and Zone, §0E does **not** depend on a
current program, keygroup or zone at all (`FTR-AKM-005`'s "Depends on" line) — it is the sampler's
own flat sample list.

**Prerequisite check (architecture step of this lot, recorded in FTR-AKM-005).** Every value this
lot needs already has a `ValueFormat` (`ADR-AKM-001`, `ItemDescriptor.hpp`): `Byte`/`SignedByte` for
pitch and tune, `String` for names (`DEC-AKM-013`, already exercised by Program's `&05`/`&09`/`&11`).
`Dword` was tried for the four position/loop items (`&20`/`&21`/`&29`/`&2A` and their Gets) and
dropped by `TASK-AKM-043`: `generate_akm_items.py --coverage` parses the spec's own columns into one
domain per wire byte, so a single combined value was flagged "1 value, the spec row describes 4" —
the exact gap this paragraph predicted, resolved the same way `TASK-AKM-035` resolved it for `§06`,
by splitting into 4 separate `Byte` args/reply values rather than changing the checker. `Dword`
remains declared but unused in the catalogue. The grouped replies `&34`/`&4B` ("all params in a single
message") are a fixed-field-count record, not the variable-count repeated-record shape of
`DEC-AKM-014`/`DEC-AKM-015` — they decode through the existing generic multi-field `decodeReply`,
needing no new decode helper. **No new ADR file is opened for this lot.**

**Safety note, mirroring FTR-AKM-002/004.** `&07` (Delete ALL samples) and `&08` (Delete the current
sample) are irreversible on the sampler's real sample memory. `&07` gets `RQ-AKM-025`'s guard
pattern (explicit confirmation argument, no default, audited out of every real-sampler test); every
real-sampler check of this lot runs against one sample the operator names in the test
configuration (`RQ-AKM-051`), never `&07`, and restores that sample's name and parameters before
returning even when a check fails — the same `GuardedTestProgram`/`GuardedSession` RAII shape
`TASK-AKM-024`/`TASK-AKM-009` already established, specialised here as `GuardedTestSample`.

## References
- **Requirements**: FTR-AKM-005 (RQ-AKM-045 to RQ-AKM-051)
- **ADRs**: ADR-AKM-001 (Accepted) — this lot extends it by catalogue growth only (`DEC-AKM-003`,
  `DEC-AKM-012`, `DEC-AKM-013`), the first real use of `ValueFormat::Dword`; no new ADR file, no new
  `DEC-AKM-*` expected (confirmed above; revisited only if `Dword` or the grouped replies turn out
  not to fit as cleanly as this analysis expects).

The plan has 7 tasks (TASK-AKM-040 to TASK-AKM-046), grouped the way `PLAN-AKM-002` (Program) split
its own RQs rather than `PLAN-AKM-004`'s single "13 items, one task" (§0E's item groups are smaller
and less uniform than §06's): TASK-AKM-040 (sample lifecycle, `RQ-AKM-045`); TASK-AKM-041
(destructive guard, `RQ-AKM-046`, depends on 040 for the selection it guards); TASK-AKM-042 (general
information, `RQ-AKM-047`, depends on 040 for a sample to select and verify against); TASK-AKM-043
(settable parameters, `RQ-AKM-048`, depends on 040); TASK-AKM-044 (read-only parameters and grouped
replies, `RQ-AKM-049`, depends on 043 for the values `&4B` combines); TASK-AKM-045 (real-sampler
harness, `RQ-AKM-051`, depends on 040–044); TASK-AKM-046 (coverage and errata resolution,
`RQ-AKM-050`, depends on 040–045).

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-AKM-040: Sample lifecycle primitives
- **Tier**: M
- **Status**: Done
- **Description**: Implement Select Sample by name (`&05`, `ValueFormat::String`, reusing
  `makeStringRequest`), Select Sample by index (`&06`, zero-based word), Delete the currently
  selected Sample (`&08`), Rename the currently selected Sample (`&09`, `String`), Start auditioning
  (`&0A`) and Stop auditioning (`&0B`) — all zero- or single-argument, completed on DONE, reporting
  ERROR `04` unchanged on a name or index not found.
- **Requirement refs**: RQ-AKM-045
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012, DEC-AKM-013)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-045 — *Given* a simulated sampler
  holding a sample `SNARE`, *When* it is selected by name then renamed to `SNARE2`, *Then* the frames
  carry the ASCII name null-terminated, the commands complete on DONE and `&14` returns `SNARE2`.
  *Given* a name that does not exist, *When* selected, *Then* the ERROR `04` is reported.
- **Dependencies**: None
- **Assignee**: AI, with the owner running the real-sampler tests
- **Verification**: Windows/MSVC Debug: 0 warnings (`/W4 /WX`), `ctest` 441/441 (one pre-existing
  failure along the way — `ItemCatalogueTests.cpp`'s total-item-count assertion, off by the 8 new §0E
  records — fixed by extending its own formula, the same collateral update TASK-AKM-036 made for §06).
  New `SamplePrimitivesTests.cpp` (7 cases, all under `[akm][sample]`, all passing standalone via
  `ctest -R "RQ-AKM-045|RQ-AKM-047"`): select by name then rename round-trips through `&14`, frame
  bytes checked (`53 4E 41 52 45 00`); a name that does not exist fails ERROR 04; select by index then
  delete then reselect by index confirms both the deletion (`&13` empty) and the index shifting onto
  the remaining sample; an out-of-range index fails ERROR 04; rename, delete, start- and
  stop-audition all fail ERROR 04 with no sample current; audition start then stop complete DONE;
  `&14` is refused `ChecksumModeUnknown` while the mode is unknown, mirroring `ProgramPrimitivesTests`'s
  own case for `&13` (DEC-AKM-013). `--coverage`: section `0E` 8/34, partial as declared. Re-ran
  `RQ-AKM-035`/`RQ-AKM-038` (§06 zone-sample tests, 7 cases) to confirm the `SimulatedSampler`
  `_sampleNames` → `_samples` refactor changed no behaviour: all still pass. Not verified: real sampler
  (deferred to TASK-AKM-045, RQ-AKM-051's dedicated test sample not built yet); mutation testing
  (blocked, as in prior tasks, RQ-BLD-015/TASK-BLD-012 not done).
- **Assumptions**: `&13` (Get Current Sample's Index) and `&14` (Get Current Sample's Name) — formally
  `RQ-AKM-047`'s, TASK-AKM-042's — were catalogued here too, the same way TASK-AKM-015 pulled in
  Program's `&10`/`&13`: RQ-AKM-045's own acceptance criteria need a Get to verify a Set against, and
  the plan's Overview already flagged this as the expected shape. `items.json`'s new "0E" section note
  and `ItemCatalogueTests.cpp`'s count record the split; TASK-AKM-042 will find both already done and
  add the rest (`&10`-`&12`). Audition (`&0A`/`&0B`) is assumed to need a current sample, like every
  other "act on the current one" item of this section — the spec is silent either way, so this is a
  modelling choice, documented in `SimulatedSampler.cpp` next to `executeSample`, not a proven fact.
  `selectSampleByIndex` sends the index unchecked (no client-side range refusal), matching what
  `selectProgramByIndex` actually does (not what its own header comment claims) rather than
  introducing a check Program itself does not have; `SamplePrimitives.hpp`'s comment states only what
  the code does. `SimulatedSampler::setSampleNames` keeps its exact signature and its existing callers
  (`ZoneSampleTests.cpp`, `RealSamplerSuiteTests.cpp`) untouched, but now seeds the same `_samples`
  list §0E's own lifecycle acts on — one source of truth instead of two, and the reuse `RQ-AKM-051`
  already plans for (the same sample serving both `RQ-AKM-035`'s zone-assignment tests and this lot's).

---

### TASK-AKM-041: Destructive command guard for "Delete ALL samples"
- **Tier**: S
- **Status**: Done
- **Description**: Implement `&07` (Delete ALL samples from memory) so that it is sent only when the
  caller passes an explicit confirmation argument that no default supplies, mirroring
  `TASK-AKM-023`'s `ConfirmDeleteAllPrograms` shape (a dedicated type, not a bare `bool`), and audit
  the real-sampler test sources to confirm none calls it.
- **Requirement refs**: RQ-AKM-046
- **ADR refs**: ADR-AKM-001 (DEC-AKM-011)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-046 — *Given* a request for `&07`
  without the confirmation argument, *When* made, *Then* nothing is sent and an error explains why.
  *Given* the source of the real-sampler tests, *When* searched for the `&07` primitive, *Then* there
  is no call.
- **Dependencies**: TASK-AKM-040
- **Assignee**: AI, with the owner's approval (DoR)
- **Verification**: Windows/MSVC Debug: 0 warnings, `ctest` 443/443 (2 new in
  `SampleDeleteAllGuardTests.cpp`, mirroring `ProgramDeleteAllGuardTests.cpp`: `std::nullopt` refuses as
  `NotConfirmed` with nothing sent; the enumerator sends `&07`, completes on DONE, and a following
  select-by-index confirms the sampler's samples are gone). `--coverage`: section `0E` now 9/34, still
  partial as declared. Audit: `grep -rn "deleteAllSamples\|SampleDeleteAll" juce/tests/support
  juce/tests/probe` — no match (exit 1), confirming no real-sampler test calls it.
- **Assumptions**: None.

---

### TASK-AKM-042: General information about samples in memory
- **Tier**: M
- **Status**: Done
- **Description**: Implement Get Number of Samples (`&10`), Get name of sample by index (`&11`),
  Get the names of all samples in memory (`&12`, a repeated `String` REPLY — reuse
  `ByteReader::readStringList()`/`DEC-AKM-014`'s precedent, not `decodeRepeatedReply`, since the
  record is a bare string like Program's `&19`), Get Current Sample's Index (`&13`) and Get Current
  Sample's Name (`&14`).
- **Requirement refs**: RQ-AKM-047
- **ADR refs**: ADR-AKM-001 (DEC-AKM-013, DEC-AKM-014)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-047 — *Given* a simulated sampler
  holding samples `KICK`, `SNARE`, `HAT`, *When* the names of all samples are read, *Then* the result
  is the ordered list `KICK`, `SNARE`, `HAT` and its length equals the count from `&10`. *Given* a
  sample selected by index, *When* its current index and current name are read, *Then* they equal
  what was selected.
- **Dependencies**: TASK-AKM-040
- **Assignee**: AI, with the owner running the real-sampler tests
- **Verification**: Windows/MSVC Debug: 0 warnings, `ctest` 447/447 (`&13`/`&14` already proven by
  TASK-AKM-040; this task adds `&10`-`&12`). 5 new cases in `SamplePrimitivesTests.cpp`, all under
  `[akm][sample]`: the `KICK`/`SNARE`/`HAT` AC verbatim (`getCount` and `getAllSampleNames` agree);
  zero samples in memory answers an empty list from `&12`, not a failure (see Assumptions — no
  ERROR-4-on-empty quirk assumed for §0E, unlike Program's `&18`/`&19`); a sample selected by index has
  its index and name confirmed, then a *different* sample read by `&11` without selecting it leaves the
  current selection unchanged; `&11` and `&12` are both refused `ChecksumModeUnknown` with nothing sent
  while the mode is unknown. `--coverage`: section `0E` now 12/34, still partial as declared.
- **Assumptions**: `&12` (Get the names of all samples) is modelled to answer a normal 0-byte REPLY
  when the sampler holds no sample, decoding to an empty list — the spec-literal behaviour, not
  Program's own `&18`/`&19` real-sampler quirk (ERROR 4 instead of an empty REPLY,
  `answersEmptyMemory` in `ProgramPrimitives.cpp`), since that quirk was observed on real hardware for
  §0A specifically and no equivalent observation exists yet for §0E. If TASK-AKM-045's real-sampler
  run finds the S5000 answers `&12` the same way it answers `&18`/`&19`, `getAllSampleNames` gets the
  same normalisation then, not here. `getSampleNameByIndex` reuses `SampleNameResult`/
  `SampleNameCompletion` from TASK-AKM-040 rather than a dedicated result type: same shape (an
  optional name plus an outcome), and a second identical struct would only rename fields nothing reads
  differently.

---

### TASK-AKM-043: Settable sample parameters (Set and Get)
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue the 8 settable §0E items and their Get counterparts — Start Position
  (`&20`/`&40`), End Position (`&21`/`&41`), Original Pitch (`&22`/`&42`, byte, range `21–127`),
  Semitone Tune (`&23`/`&43`, sign + `0–36`), Fine Tune (`&24`/`&44`, sign + `0–50`), Playback Mode
  (`&28`/`&48`, `0–5`, no `AS SAMPLE`), Loop Start (`&29`/`&49`), Loop End (`&2A`/`&4A`) — of the
  current sample, each proven by a Set then a Get on the mock, no per-item wrapper function (matching
  Program's Output/MIDI-Tune/Pitch-Bend/LFO groups and §06's own 13 numeric items: tested directly
  against the catalogue through `ProgramParameterRoundTrip.hpp`'s generic `ParameterCase`, ADR-AKM-001
  DEC-AKM-003).
- **Requirement refs**: RQ-AKM-048
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-048 — *Given* each of the 8 Set
  items, *When* a value inside its range is set on the current sample then read back on the
  simulated sampler, *Then* the value read equals the value set. *Given* playback mode `6`, *When*
  set, *Then* it is refused without sending.
- **Dependencies**: TASK-AKM-040
- **Assignee**: AI, with the owner running the real-sampler tests
- **Verification**: Windows/MSVC Debug: 0 warnings, `ctest` 451/451. New `SampleParametersTests.cpp`
  (4 cases, `[akm][sample]`, all passing standalone via `ctest -R "RQ-AKM-048"`): all 8 items
  round-trip on the current sample via the shared `ParameterCase`/`checkParameterRoundTrips` helper;
  playback mode `6` and original pitch `20` (below the spec's `21-127` floor) are both refused
  `ArgumentOutOfRange` without sending; setting a parameter with no sample current fails ERROR 04.
  `--coverage`: section `0E` now 28/34, still partial as declared, `unaccounted: none`, no "differs
  from the spec" problem. `generate_akm_items.py`'s own unit tests: `python -m unittest discover -s
  juce/tests/tools -p "test_*.py"` — 16/16 pass after the `spec_domains` fix (see Assumptions).
- **Assumptions**: `Dword` was dropped for the four position/loop items after `--coverage` flagged
  each as "1 value, the spec row describes 4": the spec's own Data1-4 columns are what the checker
  compares against, so — following `TASK-AKM-035`'s own precedent exactly, for the same reason — each
  is split into 4 separate `Byte` args/reply values (`positionMsb`/`Sb2`/`Sb1`/`Lsb`) instead. Same
  accepted consequence as `TASK-AKM-035`'s `Velocity→Start`: each byte is independently range-checked
  0-127, not the combined value against the spec's `0-268435455` ceiling (`Dword`'s own declared
  range, now unused). Separately, `--coverage` also flagged Original Pitch (`&22`/`&42`) as "1 value,
  the spec row describes 2": its second spec column reads `N/A ; {21–127}` (a clarifying note on an
  otherwise-N/A column, spec pp. 33/35), which `spec_domains` counted as a real second domain because
  it checked the *whole* column against the literal string `"N/A"` rather than its leading segment.
  Fixed in `generate_akm_items.py` (compare `second`'s own head before any `;` instead) — a real
  generator gap, the same class of fix `TASK-AKM-035` made for `&2A`'s REPLY, not a catalogue mistake.
  Verified narrow: only 3 rows in the whole spec TSV match `N/A ;` in their second column (grep), 2 of
  them these two Original Pitch rows and the third (`§0C/&20-&2B`, Multi) not catalogued at all, so no
  previously-covered row's result changed — confirmed by `--coverage` still reporting `00`/`0A`/`08`/
  `06` unchanged after the fix. `SampleRecord` gained a `parameters` map (mirroring
  `ProgramRecord::parameters`) and `SimulatedSampler.cpp` a `SAMPLE_PARAMETER_GROUP_RANGES` +
  `executeSampleParameterGroup`, a close mirror of Program's own `executeParameterGroup` (two
  contiguous ranges, `&20-&24`→`&40-&44` and `&28-&2A`→`&48-&4A`, both offset `0x20`) rather than one
  generic cross-section function: every existing section (`0A`, `08`, `06`) already hand-duplicates
  its own version rather than sharing one, so this follows that precedent instead of being the first
  exception. Not verified: real sampler (deferred to TASK-AKM-045); mutation testing (blocked, as in
  prior tasks, RQ-BLD-015/TASK-BLD-012 not done).

---

### TASK-AKM-044: Read-only sample parameters and grouped replies
- **Tier**: M
- **Status**: Not Started
- **Description**: Catalogue Get Sample Type (`&30`, RAM/VIRTUAL enum), Get Number of Channels
  (`&31`, mono/stereo enum), Get Sample Length (`&32`, `Dword`), Get Sample Rate (`&33`, `Dword`),
  Get all of `&30`–`&33` in one message (`&34`, a fixed 4-field REPLY through the existing generic
  multi-field decode, no new helper), and Get all of `&40`–`&4A` in one message (`&4B`, the 8
  settable-parameter Gets of TASK-AKM-043, same fixed-field decode).
- **Requirement refs**: RQ-AKM-049
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-049 — *Given* a simulated sample
  with a known length and rate, *When* `&34` is read, *Then* it decodes to the same four values as
  reading `&30`–`&33` individually. *Given* the same sample after TASK-AKM-043's Set tests, *When*
  `&4B` is read, *Then* it decodes to the same eight values as reading `&40`–`&4A` individually.
- **Dependencies**: TASK-AKM-043
- **Assignee**: AI, with the owner running the real-sampler tests
- **Verification**: Not yet run.
- **Assumptions**: None yet.

---

### TASK-AKM-045: Real-sampler test harness — dedicated test sample
- **Tier**: L
- **Status**: Not Started
- **Description**: Implement the RQ-AKM-051 guard for every real-sampler test of this feature
  (`GuardedTestSample`, mirroring `GuardedTestProgram`): act only on a sample the operator names in
  the test configuration, restore its name and every settable parameter before returning even when a
  check fails or is interrupted, restore the sampler's original current-sample selection, refuse
  before sending if a check would act on a different sample, and never call `&07`/`&08`. Report the
  §0E checks as skipped, not passed, when no test sample name is configured — mirroring `RQ-AKM-038`'s
  skip pattern for §06 sample assignment, and reusing the same `--sample-name` option
  `TASK-AKM-038` already added to `xs56k_akm_probe --suite` if practical, rather than adding a second
  one. Extend the suite with the checks this lot unlocks.
- **Requirement refs**: RQ-AKM-051
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-051 — *Given* no test sample name
  configured, *When* the real-sampler suite runs, *Then* the §0E tests are reported as skipped with
  the reason and all other tests run. *Given* a configured name, *When* the suite ends, *Then* the
  sample under that name exists with the same parameters and name it had before, no `&07`/`&08` was
  sent, and the sampler's current-sample selection is what it was when the suite started.
- **Dependencies**: TASK-AKM-040, TASK-AKM-041, TASK-AKM-042, TASK-AKM-043, TASK-AKM-044
- **Assignee**: AI, with the owner running the real-sampler suite
- **Verification**: Not yet run.
- **Assumptions**: None yet.

---

### TASK-AKM-046: Coverage of section §0E and errata resolution
- **Tier**: M
- **Status**: Not Started
- **Description**: Add the 34 command rows and 19 REPLY rows of section `0E` to `items.json`
  coverage (or list an explicit exclusion reason for any row this lot does not implement) and run
  `generate_akm_items.py --coverage` to confirm none is unaccounted for. The errata list carries no
  §0E entry today (`documents/_index/sysex_spec.kb.md`); resolve on the real sampler and record in
  the KB any inconsistency this lot's own tasks turn up along the way.
- **Requirement refs**: RQ-AKM-050
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-050 — *Given* the 34 command rows
  and 19 REPLY rows of section `0E`, *When* compared with the catalogue of primitives and exclusions,
  *Then* no row is unaccounted for. *Given* any erratum found while doing so, *When* the lot ends,
  *Then* its resolution is recorded with the observation that supports it.
- **Dependencies**: TASK-AKM-040, TASK-AKM-041, TASK-AKM-042, TASK-AKM-043, TASK-AKM-044,
  TASK-AKM-045
- **Assignee**: AI, with the owner running any real-sampler observation an erratum needs
- **Verification**: Not yet run.
- **Assumptions**: None yet.
