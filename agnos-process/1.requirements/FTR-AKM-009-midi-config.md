# FTR-AKM-009: MIDI Configuration Primitives (§04)

## Overview

Phase A, new lot (session AKM, 2026-10-04). Section `04` changes the sampler's MIDI setup: "these options are the
same as those in the MIDISETUP section of the UTILITIES page" (spec p12, `documents/_index/sysex_spec.kb.md` map
line 29). This feature adds one tested primitive per command row — program change enable (`&01`), multi select
(`&02`), multi select channel (`&03`), external APM controller (`&04`), aftertouch (`&05`), enable a MIDI filter
(`&06`) and disable a MIDI filter (`&07`) — and an opt-in real-sampler check in which the owner supplies the
state to put back.

**In scope** (counts from `documents/_index/sysex_spec.items.tsv`, section `04`: 7 commands, 0 REPLY formats,
7 rows — every row): `&01` to `&07`. Every row carries the `RT` flag: it is designed to be sent while musical MIDI
is flowing.

**Out of scope.** Sections other than §04. The other parameters of the UTILITIES MIDI SETUP page that no §04 row
carries (the incoming MIDI clock tempo, the 32 level meters): they cannot be set or read over SysEx.

**Depends on** FTR-AKM-001 (transport) only: §04 items are sampler-wide, with no current program, keygroup,
zone or sample.

**What the section is not.** It has no Get and no REPLY, exactly like §20 (`FTR-AKM-008`): each command completes on
DONE, and the only proof a primitive worked is what the simulated sampler recorded receiving, or the owner's eyes
on the sampler's own MIDI SETUP and MIDI FILTER pages.

**What is new compared with §20: persistent state with no read-back.** A front-panel key leaves nothing behind
once released. A §04 setting is the owner's real, stored MIDI configuration, and the AKM layer can neither read it
nor restore it. Every other real-sampler check of this repository puts back what it changed by reading it first
(`RQ-AKM-018`, `RQ-AKM-058`); here the only source of the original values is the owner, hence `RQ-AKM-080`.

**Settings, as the spec and the operator's manual give them** (manual pp. 223-224, `documents/akai_s5000_s6000_user_manual.1.21.pdf.md`
lines 5422-5472):
- `&01` Program Change Enable: 0 OFF, 1 ON — remote selection of programs inside parts.
- `&02` Multi Select: 0 OFF, 1 PROG CHANGE, 2 BANK — remote selection of multis.
- `&03` Multi Select Channel: 0 to 31, 1A = 0 … 16B = 31. No effect while Multi Select is OFF; while it is on, that
  channel can no longer select programs in parts (manual p223, done by the sampler itself).
- `&04` External APM Controller: one of the 128 MIDI controllers, 0 to 127, used as a source in the APM matrix.
- `&05` Aftertouch: 0 channel, 1 polyphonic. A controller that only sends channel aftertouch produces no
  aftertouch at all while polyphonic is selected (manual p224).
- `&06` / `&07` Enable / disable a MIDI filter: `<Data1>` event type 0 NoteOn, 1 Aftertouch, 2 Wheels, 3 Volume;
  `<Data2>` channel 0 to 31 as for `&03`. `&06` allows the messages, `&07` ignores them; the item says "for
  Port A & B", the channel code carrying the port (A = 1A to 16A = 0 to 15, B = 16 to 31).

**Real-hardware risk.** Whatever is sent is stored: a filter that ignores NoteOn on a channel silences that channel
for the owner's keyboard; Multi Select ON takes a channel away from program selection; polyphonic aftertouch can
silence a keyboard that sends channel aftertouch. No setting touches a program, a multi or a sample, and none
stops SysEx from working, but the owner's configuration is changed until they put it back.

**Sources.** `documents/_index/sysex_spec.kb.md` (Table 4 line 76, map line 29), `documents/_index/sysex_spec.items.tsv`
(section `04`), `documents/akai_s5000_s6000_sysex_spec_2.10.pdf.md` printed p. 12 (Table 8, lines 547-570),
`documents/akai_s5000_s6000_user_manual.1.21.pdf.md` printed pp. 223-224.

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the Phase B workflows that configure the sampler's MIDI reception; CI (simulated sampler only —
  the owner-guided check never runs in CI, see `RQ-AKM-080`).

---

## Functional Requirements

### RQ-AKM-078: MIDI setup switches (Set)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller sets the program change enable (`&01`), the multi select mode (`&02`), the multi select channel (`&03`), the external APM controller (`&04`) or the aftertouch type (`&05`), the AKM layer SHALL send the matching item with the value encoded in its spec format as `<Data1>` and SHALL refuse any value outside the spec's range without sending.
- **Rationale**: five single-value Set items of the same shape; the ranges are the spec's own (Table 8).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler, *When* multi select is set to BANK, *Then* the frame carries `04 02 02` as section, item and data bytes, the command completes on DONE and the sampler records multi select 2. *Given* the multi select channel 16B, *When* sent, *Then* the data byte is `1F`. *Given* the external APM controller 127, *When* sent, *Then* the data byte is `7F`. *Given* multi select `3`, a channel `32`, a controller `128` or aftertouch `2`, *When* sent, *Then* nothing is sent and the refusal is `ArgumentOutOfRange`. *Given* a program change enable of `2` (the primitive takes a bool, so only a request built from the catalogue can carry it), *When* the request is built, *Then* the catalogue refuses it as `ArgumentOutOfRange`.
- **Dependencies**: RQ-AKM-002; FTR-AKM-001 (RQ-AKM-009, RQ-AKM-005)

### RQ-AKM-079: MIDI filters (Set)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller enables (`&06`) or disables (`&07`) a MIDI filter for an event type (NoteOn, Aftertouch, Wheels, Volume) on a channel (1A to 16B), the AKM layer SHALL send the matching item with the event type as `<Data1>` and the channel as `<Data2>`, and SHALL refuse an event type outside 0 to 3 or a channel outside 0 to 31 without sending.
- **Rationale**: the two filter items share one two-byte shape; enabling allows the messages and disabling ignores them, so a caller states the intent, not the item code.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler, *When* the Wheels filter on channel 3A is disabled, *Then* the frame carries `04 07 02 02` as section, item and data bytes, it completes on DONE and the sampler records the Wheels filter of channel 3A as ignoring. *Given* the same filter enabled afterwards, *Then* the frame carries `04 06 02 02` and the sampler records it as allowing. *Given* an event type `4` or a channel `32`, *When* sent through either item, *Then* nothing is sent and the refusal is `ArgumentOutOfRange`.
- **Dependencies**: RQ-AKM-002; FTR-AKM-001 (RQ-AKM-009, RQ-AKM-005)

### RQ-AKM-080: Real-sampler check guided by the owner
- **Category**: Functional
- **EARS Type**: Complex
- **Statement**: WHEN the owner runs the real-sampler suite with the opt-in `--midi-config` option, the suite SHALL (a) tell the owner to open the sampler's MIDI SETUP and MIDI FILTER pages and note the current values, (b) ask the owner for the current value of every setting the check will change, and skip the check without sending anything if there is no way to ask or the owner declines, (c) change each of those settings to a value different from the one declared, one at a time, asking the owner to confirm on the sampler's own screen that it shows the new value, and (d) put each setting back to the value the owner declared; IF the check fails or is interrupted, THEN the suite SHALL still put back every setting it had changed.
- **Rationale**: §04 has no Get, so the owner is the only source of the original configuration and the only reader of the new one; the suite sends nothing before the owner has declared what to restore, and the restore is owed on every exit path like `RQ-AKM-018`.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler and a scripted owner who declares the sampler's real values, *When* the check runs, *Then* each setting was changed then set back and the sampler ends in the declared state. *Given* no owner or a declined confirmation, *When* the check runs, *Then* it is skipped and no §04 frame is sent. *Given* a check made to throw after a setting was changed, *When* it ends, *Then* that setting was put back to the declared value. *Given* the real sampler, *When* the owner runs the check, *Then* the log lists each change, the owner's confirmation or denial of it and the restore, and the owner confirms the original screen is back.
- **Dependencies**: RQ-AKM-017; RQ-AKM-018; RQ-AKM-078; RQ-AKM-079

### RQ-AKM-081: Coverage of section §04
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Every §04 row of `documents/_index/sysex_spec.items.tsv` SHALL be either covered by a primitive or listed with a reason for its exclusion, and any spec inconsistency met while doing so SHALL be resolved or recorded.
- **Rationale**: same bookkeeping as `RQ-AKM-077`, for the section's seven rows.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the 7 command rows of section `04`, *When* compared with the catalogue of primitives and exclusions, *Then* no row is unaccounted for. *Given* any erratum found, *When* the lot ends, *Then* its resolution is recorded with the observation that supports it.
- **Dependencies**: RQ-AKM-078 to RQ-AKM-080

---

## Open points

- **Which filter the check exercises.** The 128 filter switches (4 event types × 32 channels) cannot all be declared by the owner. The check of `RQ-AKM-080` exercises one event type on one channel, chosen by the owner at run time through the suite's choice prompt; the choice is settled in the DoR of the real-sampler task.
- **The owner's declared values are not verified.** The suite cannot detect a wrong declaration: it restores what it was told. The log records the declaration, so a mistake is traceable after the run.
- **Which values the check changes to.** "A value different from the declared one" leaves a choice per setting (for example, Multi Select has two others). The values, and the order that keeps Multi Select Channel meaningful (it has no effect while Multi Select is OFF), are fixed in the task's DoR.
- **Port B and the channel code.** `&06`/`&07` say "for Port A & B": whether the sampler also accepts the channel codes of the other port from the port a frame arrives on is not stated and is observed, not assumed.
- **A setting may depend on another, or the screen may not follow (observed 2026-10-04, settled).** In the first two real runs MULTI SELECT, its channel, the external APM controller and aftertouch were not seen on the S5000's screen; with §00/&05 (automatic screen updating) switched on, in the third run, all seven items were seen. The sampler obeyed them all; its MIDI SETUP and MIDI FILTER pages are only redrawn by a SysEx message while §00/&05 is on, and there is no dependence of `&02` on `&01` (`process/2.architecture/OBSERVATIONS-RQ-AKM-080-midi-config.md`). The check tests each setting alone, the others at the owner's declared values, and switches §00/&05 on for its session.
