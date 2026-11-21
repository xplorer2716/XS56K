# FTR-AKM-008: Front Panel Control Primitives (§20)

## Overview

Phase A, new lot (session AKM, 2026-10-03). Section `20` is the sampler's remote-control layer: it lets a
host press and release the front-panel keys, turn the data wheel and type ASCII characters, "to facilitate
a Remote Control facility" (Table 30, `documents/_index/sysex_spec.kb.md` line 169). This feature adds one
tested primitive per command row — hold a key (`&01`), release a key (`&02`), move the data wheel (`&03`),
send an ASCII character (`&04`) — plus a typed catalogue of the keycodes of Table 31, a "press" helper that
always pairs a Hold with its Release, and a real-sampler check that lets the owner drive the sampler from the
PC keyboard.

**In scope** (counts from `documents/_index/sysex_spec.items.tsv`, section `20`: 4 commands, 0 REPLY
formats, 4 rows — every row): `&01`, `&02`, `&03`, `&04`.

**Out of scope.** Sections other than §20. Interpreting what a key does: the effect of a key depends on the
screen the sampler is showing, which no §20 item reads back (there is no Get and no REPLY in this section).

**Depends on** FTR-AKM-001 (transport) only: §20 items are sampler-wide, with no current program, keygroup,
zone or sample.

**What DONE means here.** Table 30 note a: these items only *queue* the data in the sampler. DONE confirms
the data was queued, not processed; an ERROR is returned only if it could not be queued. No primitive of this
feature may claim a key was *obeyed*, only that it was accepted.

**Held keys.** A Key Hold "must eventually be followed by a Key Release" (p41). The sampler keeps the key
down until the release arrives, however long. This is the one place in the protocol where a command leaves
state behind that only a second command clears, hence `RQ-AKM-075`.

**Keycodes.** Table 31 lists 43 keycodes in `&40`–`&6B` (64–107); of the 44 values of that range only `&66`
(102) is not listed. Table 30 note b: "Only those keycodes defined in Table 31 should be used. Use of other
values may lead to undefined behaviour." The AKM layer therefore accepts only the listed codes, not the
range.

**Real-hardware risk.** Whatever a key does on the front panel it does for real: from some screens, ENT/PLAY,
SAVE or the data wheel can change or delete the owner's data. The real-sampler check of this feature is
therefore interactive and owner-driven (`RQ-AKM-076`): the owner chooses the screen, and the check sends
nothing the owner did not press.

**Sources.** `documents/_index/sysex_spec.kb.md` (§20 in Table 4 line 79, keycodes line 169-172, map line 41),
`documents/_index/sysex_spec.items.tsv` (section `20`), `documents/akai_s5000_s6000_sysex_spec_2.10.pdf.md`
printed p. 41 (Tables 30 and 31).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the Phase B workflows that remote-control the sampler's screens; CI (simulated sampler
  only — the interactive check never runs in CI, see `RQ-AKM-076`).

---

## Functional Requirements

### RQ-AKM-073: Key Hold and Key Release (Set)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller holds (`&01`) or releases (`&02`) a front-panel key, the AKM layer SHALL send the matching item with the keycode of Table 31 as `<Data1>`, SHALL refuse without sending any value that is not a keycode of Table 31, and SHALL offer a "press" operation that sends a Hold and then a Release of the same key, the Release being sent even when the Hold's outcome is an error or a timeout.
- **Rationale**: the spec forbids unlisted codes (note b) and requires every Hold to be followed by a Release; a typed key and a paired press make both the default.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler, *When* the key EXIT is held then released, *Then* the frames carry `20 01 6A` then `20 02 6A` data bytes, each completes on DONE, and the sampler records the key down then up. *Given* the value `102` (`&66`), *When* held, *Then* nothing is sent and the refusal is `ArgumentOutOfRange`. *Given* a simulated sampler that answers the Hold with ERROR, *When* the key is pressed, *Then* a Release is still sent.
- **Dependencies**: RQ-AKM-002; FTR-AKM-001 (RQ-AKM-009, RQ-AKM-005)

### RQ-AKM-074: Data wheel and ASCII keyboard (Set)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller moves the data wheel (`&03`: direction `0` forwards or `1` backwards, 1 to 8 clicks) or sends an ASCII character (`&04`: 0 to 127), the AKM layer SHALL send the matching item with the values encoded in their spec format and SHALL refuse any out-of-range value without sending.
- **Rationale**: the other two controls of the section, same Set-only shape; the ranges are the spec's own.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler, *When* the wheel is moved backwards by 3 clicks, *Then* the data bytes are `03 01 03` and the sampler records one wheel movement of 3 clicks backwards. *Given* 0 or 9 clicks, direction `2`, or an ASCII value `128`, *When* sent, *Then* nothing is sent and the refusal is `ArgumentOutOfRange`. *Given* ASCII `65`, *When* sent, *Then* the data byte is `41` and the sampler records it.
- **Dependencies**: RQ-AKM-002; FTR-AKM-001 (RQ-AKM-009, RQ-AKM-005)

### RQ-AKM-075: A held key is released when the session closes
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a session is closed while a key it held with `&01` has not been released, THEN the AKM layer SHALL send a Key Release for each such key before the session closes, SHALL do so whether the close is requested or follows a failed command, and SHALL leave no key held by a session that is gone.
- **Rationale**: a Hold left unreleased keeps the key down on the sampler indefinitely; the same "known state on the way out" idea as `RQ-AKM-018` and `RQ-AKM-042`. Decided by the owner, session AKM, 2026-10-03 (`DEC-AKM-019`).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a session in which EXIT was held and not released, *When* the session is closed, *Then* a Release of EXIT is sent before the close completes and the simulated sampler has no key down. *Given* a session with no held key, *When* closed, *Then* no §20 frame is sent. *Given* a key held then released by the caller, *When* the session closes, *Then* no further Release is sent.
- **Dependencies**: RQ-AKM-042; RQ-AKM-043; RQ-AKM-073

### RQ-AKM-076: Real-sampler check driven from the PC keyboard
- **Category**: Functional
- **EARS Type**: Complex
- **Statement**: WHEN the owner runs the real-sampler suite with the opt-in `--front-panel` option, the suite SHALL (a) print the mapping between PC keyboard keys and sampler keys, (b) ask the owner to confirm that the sampler is showing a screen the owner has chosen, and stop without sending anything if there is no way to ask or the owner declines, (c) send, for each PC key the owner presses, only the §20 item that the mapping assigns to it, and (d) end when the owner presses the documented end key, releasing every key still held; IF the check fails or is interrupted, THEN the suite SHALL still release every key it held.
- **Rationale**: front-panel keys act on whatever the sampler shows, which no software can know; only the owner can judge what is safe, so the owner picks the screen and every effect is one the owner asked for. The suite sends nothing on its own initiative, unlike the other real-sampler checks.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler and a scripted source of PC keys, *When* the check runs with the owner's confirmation, *Then* each scripted key produces exactly the frames its mapping entry names, and the end key stops the check. *Given* no owner confirmation, *When* the check runs, *Then* it is skipped and no frame is sent. *Given* a check made to throw after a key was held, *When* it ends, *Then* that key was released. *Given* the real sampler, *When* the owner presses the mapped keys, *Then* the sampler reacts and the log lists each key pressed and each confirmation received.
- **Dependencies**: RQ-AKM-017; RQ-AKM-018; RQ-AKM-073; RQ-AKM-074; RQ-AKM-075

### RQ-AKM-077: Coverage of section §20
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Every §20 row of `documents/_index/sysex_spec.items.tsv` SHALL be either covered by a primitive or listed with a reason for its exclusion, and any spec inconsistency met while doing so SHALL be resolved or recorded.
- **Rationale**: same bookkeeping as `RQ-AKM-072`, for the section's four rows.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the 4 command rows of section `20`, *When* compared with the catalogue of primitives and exclusions, *Then* no row is unaccounted for. *Given* any erratum found, *When* the lot ends, *Then* its resolution is recorded with the observation that supports it.
- **Dependencies**: RQ-AKM-073 to RQ-AKM-076

---

## Open points

- **PC-key mapping.** The mapping of `RQ-AKM-076` is a named table of the probe, settled with the owner at the start of its task: the 16 function keys, the digits, the mode keys and the cursor keys have natural PC counterparts; which PC keys stand for the sampler's mode keys (MULTI, FX, EDIT SAMPLE…) is the owner's to choose.
- **Held versus pressed.** A console reports key presses, not key releases. The check sends each PC key as a Hold then a Release; whether a PC key also toggles a long hold (e.g. ENT/PLAY held to audition a sample, as the spec's own example) is settled with the mapping.
- **Which keycodes are valid on an S5000.** Table 31 is the spec's list; the owner's observation of the real sampler may show a listed key that does nothing on the S5000, recorded as an observation, not as an erratum, unless the sampler contradicts the spec.
