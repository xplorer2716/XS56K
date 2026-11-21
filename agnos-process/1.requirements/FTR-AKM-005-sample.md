# FTR-AKM-005: Sample Primitives (§0E)

## Overview

Phase A, new lot (owner decision, session AKM, 2026-09-29): one tested primitive for each item of
section §0E (Sample) — select the current sample by name or by index, delete it, rename it, start
or stop auditioning it; read the sampler's general information about the samples it holds (count,
name by index, all names, current index, current name); set or get the current sample's start and
end position, original pitch, semitone and fine tune, playback mode and loop start/end; and read
its type, channel count, length and rate — each proven by a Set followed by a Get (or a Get alone
for the read-only items) on the mock and on the real sampler.

**Decision recorded (session AKM, owner, 2026-09-29).** `FTR-AKM-004`'s "Out of scope" line deferred
all of §0E to "a later lot or to Phase B" — this feature lifts that deferral. The boundary that
still holds is Disk (§10): every primitive here acts on a sample **already in the sampler's
memory** (loaded via front panel); loading a sample from a file stays out of scope and unplanned.
Lifting this deferral also supplies the picker data `FTR-AKM-004`'s open point named
(`§0E/&12`, "names of all samples in memory") — whether `RQ-AKM-035`'s manual name entry is
actually replaced by a picker in the UI stays a Phase B decision, unaffected by this feature.

**In scope** (counts from `documents/_index/sysex_spec.items.tsv`, section `0E`: 34 commands, 19
REPLY formats, 53 rows total; `documents/_index/sysex_spec.kb.md` line 35: Tables 18/19, printed
pp. 29–31): sample selection and lifecycle (`&05`, `&06`, `&08`, `&09`, `&0A`, `&0B`), the
destructive `&07` (Delete ALL samples), general information (`&10`–`&14`), the eight settable
parameter items and their Get counterparts (`&20`–`&24`, `&28`–`&2A`, `&40`–`&44`, `&48`–`&4A`),
the four read-only items (`&30`–`&33`) and the two grouped-REPLY items (`&34`, `&4B`).

**Out of scope.** Loading, browsing or otherwise reaching into Disk (§10) — no primitive here can
put a new sample into the sampler's memory or take one out to a file. Sections §0A, §08, §06
(`FTR-AKM-002`–`FTR-AKM-004`) and every other section. The blocked-request section §38. Whether or
how the UI actually offers a sample picker (Phase B).

**Depends on** FTR-AKM-001 (transport) only — unlike `FTR-AKM-002`–`FTR-AKM-004`, §0E is not scoped
to the current program: Sample is its own flat namespace, addressed by the sampler-wide "current
sample" that `&05`/`&06` select (`sysex_spec.kb.md` line 91: "§0A/§0C/§0E/§14/§16 act on the
*current* item: select by name (`&05`) or by index (`&06`, zero-based word)" — the same pattern
already used for Program in `RQ-AKM-021`). No dependency on a current program, keygroup or zone.

**Outline status.** Same as FTR-AKM-002/003/004: requirements at the level of the spec's item
groups, refined into per-primitive detail when this lot's plan starts.

**Sources.** `documents/_index/sysex_spec.kb.md` (state model line 91; errata list — none currently
listed against §0E), `documents/akai_s5000_s6000_sysex_spec_2.10.pdf.md` printed pp. 29–31
(Tables 18 and 19).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: `FTR-AKM-004` (`RQ-AKM-035`'s deferred picker, if adopted in Phase B); the Phase B
  workflow(s) covering the manual's sample-editing pages; CI.

---

## Functional Requirements

### RQ-AKM-045: Sample lifecycle primitives
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller selects a sample by name (`&05`) or by index (`&06`, zero-based word), deletes the currently selected sample (`&08`), renames it (`&09`) or starts or stops auditioning it (`&0A`/`&0B`), the AKM layer SHALL send the matching §0E command and report its outcome, and IF the sampler answers ERROR `04` (not found), THEN it SHALL report that error unchanged.
- **Rationale**: the entry point every later §0E primitive depends on — items act on the sampler-wide *current sample*, the same pattern `RQ-AKM-021` already established for Program.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding a sample `SNARE`, *When* it is selected by name then renamed to `SNARE2`, *Then* the frames carry the ASCII name null-terminated, the commands complete on DONE and `Get Current Sample's Name` (`&14`) returns `SNARE2`. *Given* a name that does not exist, *When* selected, *Then* the ERROR `04` is reported. *Given* the real sampler and the dedicated test sample of `RQ-AKM-051`, *When* select by index, select by name, audition start and audition stop are run, *Then* each completes as the spec defines and `Get Current Sample's Index`/`Name` (`&13`/`&14`) confirm the selection.
- **Dependencies**: FTR-AKM-001 (RQ-AKM-009, RQ-AKM-005)

### RQ-AKM-046: Destructive command guard
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a caller requests "Delete ALL samples from memory" (`&07`), THEN the AKM layer SHALL send it only when the caller passes an explicit confirmation argument that no default supplies, and no real-sampler test of any feature SHALL call it.
- **Rationale**: mirrors `RQ-AKM-025` exactly — `&07` empties the sampler's sample memory and is not reversible without a saved backup; a primitive one mistyped call away from that must not be usable by accident.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a request for `&07` without the confirmation argument, *When* made, *Then* nothing is sent and an error explains why. *Given* the source of the real-sampler tests, *When* searched for the `&07` primitive, *Then* there is no call.
- **Dependencies**: RQ-AKM-045

### RQ-AKM-047: General information about samples in memory
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller reads the number of samples in memory (`&10`), a sample's name by index (`&11`), the names of all samples in memory (`&12`), the current sample's index (`&13`) or its name (`&14`), the AKM layer SHALL send the matching Get and decode the REPLY into typed values, including the concatenated null-terminated names of the all-samples reply.
- **Rationale**: this is the picker data `FTR-AKM-004`'s open point named, and the information the dedicated test sample of `RQ-AKM-051` is located and confirmed by.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding samples `KICK`, `SNARE`, `HAT`, *When* the names of all samples are read, *Then* the result is the ordered list `KICK`, `SNARE`, `HAT` and its length equals the count from `&10`. *Given* a sample selected by index (`RQ-AKM-045`), *When* its current index and current name are read, *Then* they equal what was selected.
- **Dependencies**: RQ-AKM-002; RQ-AKM-045

### RQ-AKM-048: Settable sample parameters (Set and Get)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller sets or gets a parameter of the current sample — start position, end position, original pitch, semitone tune, fine tune, playback mode (`0`–`5`, no `AS SAMPLE`: `sysex_spec.kb.md` line 125), loop start or loop end — the AKM layer SHALL send the matching item with the value encoded in its spec format and range (positions as compound double words, signed values as `sign, magnitude`), refuse an out-of-range value without sending, and decode the Get REPLY into a typed value; a Get right after a Set SHALL return the value set.
- **Rationale**: the plan's method — one item, one Set test, one Get test, verified by read-back — applied to the eight settable §0E parameters (`&20`–`&24`, `&28`–`&2A`, mirrored by `&40`–`&44`, `&48`–`&4A`).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* each of the 8 Set items, *When* a value inside its range is set on the current sample then read back on the simulated sampler, *Then* the value read equals the value set. *Given* playback mode `6`, *When* set, *Then* it is refused without sending. *Given* the real sampler and the dedicated test sample of `RQ-AKM-051`, *When* the same test runs, *Then* it passes unchanged.
- **Dependencies**: RQ-AKM-045; RQ-AKM-002

### RQ-AKM-049: Read-only sample parameters and their grouped replies
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller reads the current sample's type (`&30`, RAM/VIRTUAL), number of channels (`&31`, mono/stereo), length (`&32`) or rate (`&33`), individually or all at once (`&34`), the AKM layer SHALL send the matching Get and decode the REPLY into typed values; WHEN a caller reads the eight settable parameters of `RQ-AKM-048` together (`&4B`), it SHALL decode all eight values from the single REPLY in the order the spec defines.
- **Rationale**: `&30`–`&33` have no Set counterpart (derived from the sample's audio data, not user-settable) and need their own requirement; `&34` and `&4B` are the grouped-REPLY convenience items the spec provides for both families.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated sample with a known length and rate, *When* `&34` is read, *Then* it decodes to the same four values as reading `&30`–`&33` individually. *Given* the same sample after `RQ-AKM-048`'s Set tests, *When* `&4B` is read, *Then* it decodes to the same eight values as reading `&40`–`&4A` individually.
- **Dependencies**: RQ-AKM-048; RQ-AKM-002

### RQ-AKM-050: Coverage of section §0E
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Every §0E row of `documents/_index/sysex_spec.items.tsv` SHALL be either covered by a primitive of this lot or listed with a reason for its exclusion, and any spec inconsistency met while doing so SHALL be resolved by observation on the real sampler and the resolution recorded.
- **Rationale**: 53 rows (34 commands, 19 REPLY) are too many to track by eye; the errata list carries no §0E entry today, but the other lots each found at least one, so this lot must still check.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the 34 command rows and 19 REPLY rows of section `0E`, *When* compared with the catalogue of primitives and exclusions, *Then* no row is unaccounted for. *Given* any erratum found while doing so, *When* the lot ends, *Then* its resolution is recorded with the observation that supports it.
- **Dependencies**: RQ-AKM-045 to RQ-AKM-049

### RQ-AKM-051: Real-sampler tests use a dedicated test sample
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: The real-sampler tests of this feature SHALL only select, rename, audition and set parameters of a sample the operator names as the dedicated test sample in the test configuration, SHALL restore its name and every parameter this lot can set to what they were before the suite ran even when a test fails or is interrupted, SHALL restore the sampler's current-sample selection to the one found, and SHALL NOT call `&07` or `&08` against it or any other sample; IF no such name is configured, THEN the §0E tests SHALL be reported as skipped, not as passed.
- **Rationale**: mirrors `RQ-AKM-027` (dedicated test program) and `RQ-AKM-038`'s skip pattern — without Disk (§10) no sample can be loaded by the software, and unlike a program the operator cannot conjure a throwaway sample from nothing; reusing the same sample `RQ-AKM-035`'s real-sampler test already needs (`RQ-AKM-038`) avoids asking the operator to prepare a second one.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* no test sample name configured, *When* the real-sampler suite runs, *Then* the §0E tests are reported as skipped with the reason and all other tests run. *Given* a configured name, *When* the suite ends, *Then* the sample under that name exists with the same parameters and name it had before, no `&07`/`&08` was sent, and the sampler's current-sample selection is what it was when the suite started.
- **Dependencies**: RQ-AKM-045; RQ-AKM-046; RQ-AKM-048

---

## Open points

- **Primitive shape**: as in FTR-AKM-002 (ADR-AKM-001) — no new architecture expected, the item
  catalogue grows the same way it did for `§06`/`§08`/`§0A`.
- **Test-sample reuse across features**: `RQ-AKM-051` and `RQ-AKM-038` (`FTR-AKM-004`) both want an
  operator-configured sample name; whether the test configuration exposes one shared setting or two
  is a detail for this lot's plan, not a requirement-level decision.
- **Picker adoption**: whether `RQ-AKM-035`'s manual name entry is replaced by a picker built on
  `RQ-AKM-047`'s `&12` is left to Phase B, as `FTR-AKM-004` already recorded.
