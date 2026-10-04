# FTR-AKM-013: Multi FX Primitives (§12)

## Overview

Phase A, new lot (session AKM, 2026-10-04). Section `12` is the sampler's Multi FX control: "effects settings belong
to a Multi, and this section enables the adjustment of effects for the currently selected multi" (spec p. 35). The
protocol presents the effects hardware as a series of effects channels, each with a number of modules that can be
bypassed or, depending on the hardware, changed for other modules. This feature adds one tested primitive per command
row: the discovery of the board and of its layout (`&01` card installed, `&10` number of channels, `&11` number of
modules of a channel), the mute status of a channel (`&20`, `&21`), the type of a module (`&30`, `&31`), the enabled
state of a module (`&40`, `&41`) and the value of a module's parameter (`&50`, `&51`) — plus a real-sampler check that
reads what the sampler answers and, when a board is installed, round-trips the items on a test multi.

**In scope** (counts from `documents/_index/sysex_spec.items.tsv`, section `12`: 11 commands and 7 REPLY formats, 18
rows — every row): `&01`, `&10`, `&11`, `&20`, `&21`, `&30`, `&31`, `&40`, `&41`, `&50`, `&51`.

**Out of scope.** Sections other than §12. The alternative addressing of a multi's FX by index (§32, "Alt by index",
which reuses these items and has no row of its own). Naming the parameters of Table 25 and their ranges: the primitives
carry the parameter index and the signed value as given and leave the judgement of a range to the sampler — a Phase B
concern, since which parameters exist depends on the module type the editor shows.

**Depends on** FTR-AKM-001 (transport) and FTR-AKM-011 (a multi must be current, `RQ-AKM-087`). Shapes: a channel and
a module are zero-based `Byte` indices, a Get's REPLY is one or three data bytes, and a parameter value is a **signed
compound word** (a sign byte, then the magnitude as a most-significant and a least-significant byte), the
`signed_word` value format the catalogue has had since `RQ-AKM-002` and no item has yet used. No hand-built request
is needed.

**No board on the owner's sampler.** The owner's S5000 has no EB20 effects board installed, so every Set of this
section is untestable on that sampler: its real-sampler check reads, with a test multi current, the answer to `&01` and,
when the board is absent, what the other Gets answer (observations), and sends no Set. The round trip with a board
installed is delivered and tested on the simulated sampler only, and recorded as not verified on hardware
(`RQ-AKM-102`).

**Real-hardware risk.** The Sets change the effects of the current multi, a stored setting: the check works only on a
test multi it creates and deletes (`RQ-AKM-093`'s guard) and puts back every value it changes.

**Sources.** `documents/_index/sysex_spec.kb.md` (§12 in Table 4, lines 37-38, 104-105 and the FX section, lines
156-176), `documents/_index/sysex_spec.items.tsv` (section `12`), `documents/akai_s5000_s6000_sysex_spec_2.10.pdf.md`
printed pp. 35-38 (Figure 2 and Tables 22 to 25).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the Phase B workflows that show and edit a multi's effects; CI (simulated sampler only).

---

## Functional Requirements

### RQ-AKM-099: FX board and layout discovery
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller asks whether an FX board is installed (`&01`), for the number of FX channels (`&10`) or for the number of modules of a channel (`&11`, the zero-based channel as one data byte), the AKM layer SHALL send the item and decode its one-byte REPLY (Table 23: `&01` is 0 for none and 1 for the EB20), SHALL refuse a channel outside 0-127 without sending, and SHALL report an empty result, not a value, when the REPLY does not decode.
- **Rationale**: "prior to use, the configuration of all of the effects channels and modules must be determined" (spec p. 35): these are the three Gets that give the layout.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler with an EB20 laid out as Figure 2 (four channels, six modules on channels 0 and 1, two on channels 2 and 3), *When* the card, the number of channels and the number of modules of each channel are read, *Then* the card is the EB20, the channel count is 4 and the module counts are 6, 6, 2, 2. *Given* a simulated sampler with no board, *When* the card is read, *Then* it is none and no Set is needed to know it. *Given* a channel of 128, *When* the modules are asked for, *Then* the request is refused without sending.
- **Dependencies**: RQ-AKM-002; FTR-AKM-001 (RQ-AKM-009, RQ-AKM-005)

### RQ-AKM-100: Channel mute and module type and state
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller sets or gets the mute status of a channel (`&20`, `&21`: 0 = ON, 1 = MUTE), the type of a module (`&30`, `&31`: a Table 24 code 0-16) or the enabled state of a module (`&40`, `&41`: 0 = disabled, 1 = enabled), the AKM layer SHALL send the matching item with the channel and, for a module, the module as zero-based data bytes, decode the REPLYs of Table 23, and SHALL report the sampler's ERROR unchanged for a channel, a module or a type the sampler does not accept.
- **Rationale**: the configuration items of the section; the module type takes the codes of Table 24, which the layer names so that a caller does not write bare numbers.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated EB20, *When* channel 1 is muted, *Then* the frame carries section `12`, item `20`, then `01 01` and `&21` reads MUTE; set back, it reads ON. *Given* module 2 of channel 0 set to the type Flange, *When* its type is read, *Then* it is Flange (`03`). *Given* module 3 of channel 0 disabled then read, *Then* it is disabled, and enabled again it is enabled. *Given* a channel or a module the sampler does not have, *When* any of these items is sent, *Then* the sampler's ERROR is reported unchanged. *Given* a module type code that Table 24 does not name, *When* it is read, *Then* it is passed through unchanged, and a code outside 0-127 is refused without sending.
- **Dependencies**: RQ-AKM-099

### RQ-AKM-101: FX parameter values as signed compound words
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a caller sets (`&50`) or gets (`&51`) the value of a parameter of a module of a channel, the AKM layer SHALL send the channel, the module and the parameter index as zero-based data bytes and, for a Set, the value as a signed compound word (the sign byte, 0 for positive and 1 for negative, then the magnitude as a most- and a least-significant byte, magnitude = LSB + 128 × MSB), SHALL decode the REPLY of `&51` to the same signed value, and SHALL refuse a value whose magnitude exceeds 16383 without sending.
- **Rationale**: "parameter values are always passed as a Signed Compound Word even although many parameters only require a single unsigned byte" (spec p. 35), so one item serves every parameter of every module.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated EB20, *When* parameter 1 of module 2 of channel 0 is set to -25, *Then* the frame carries `00 02 01 01 00 19` after the item and `&51` reads -25. *Given* a parameter set to 4000, *When* it is read back, *Then* the frame carried `00 1F 20` after the three index bytes (4000 = 31 × 128 + 32) and the value reads 4000. *Given* a magnitude of 16384, *When* it is set, *Then* the request is refused without sending.
- **Dependencies**: RQ-AKM-099; RQ-AKM-002

### RQ-AKM-102: Real-sampler check reads, and round-trips only with a board
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the owner runs the real-sampler suite with the opt-in `--multi-fx` option, the suite SHALL create a test multi under a reserved name (and stop without touching anything if a multi already bears it), read whether an FX board is installed and, when none is, read the number of channels and the first channel's modules, mute status, first module's type, state and first parameter and log each answer as an observation, send no Set and be skipped; WHEN a board is installed, the suite SHALL round-trip the mute status of a channel, the enabled state of a module, the type of a changeable module and a parameter of a module, putting each value back and reading the put-back value, and SHALL delete the test multi and select again the multi that was current, even when a check fails half way.
- **Rationale**: the owner's sampler has no board, so the empty-board answers are the observations to make; the round trip is built for a board and tested on the simulated sampler (`RQ-AKM-018`).
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler with an EB20, *When* the check runs, *Then* every read agrees, every value changed is put back and read back, the test multi is deleted and the current multi selected again. *Given* a sampler with no board, *When* the check runs, *Then* it is skipped, nothing but Gets is sent in section `12` and the test multi is deleted. *Given* a check made to fail with the test multi current, *When* it ends, *Then* the test multi is gone. *Given* the real sampler, *When* the owner runs the check, *Then* the log lists each step and each answer.
- **Dependencies**: RQ-AKM-017; RQ-AKM-018; RQ-AKM-093; RQ-AKM-099; RQ-AKM-100; RQ-AKM-101

### RQ-AKM-103: Coverage of section §12
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Every §12 row of `documents/_index/sysex_spec.items.tsv` SHALL be either covered by a primitive or listed with a reason for its exclusion, and any spec inconsistency met while doing so SHALL be resolved or recorded.
- **Rationale**: same bookkeeping as `RQ-AKM-098`, for the section's eighteen rows.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the 11 command rows of section `12`, *When* compared with the catalogue of primitives and exclusions, *Then* no row is unaccounted for. *Given* any erratum found, *When* the lot ends, *Then* its resolution is recorded with the observation that supports it.
- **Dependencies**: RQ-AKM-099 to RQ-AKM-102

---

## Open points

- **What a sampler with no board answers.** The spec does not say what `&10`, `&11` and the others answer when `&01` is 0, nor what any item answers with no multi current; the model answers 0 channels and ERROR `04` and the real answers are observations, not errata. Observed on the owner's S5000 (no board): recorded by TASK-AKM-105.
- **Which modules may change type.** The spec says that with the EB20 "only modules 2 and 3, of channels 0 and 1 may be changed" (p. 35); what the sampler answers to a type change of another module is not stated and cannot be observed without a board. The simulated sampler does not enforce it; the check tries only module 2 of channel 0.
- **The reverb column of Figure 2.** The extracted text of the figure lists, for channel 0, six columns (ring-mod/distortion, EQ, modulation, delay, reverb, output control) while channels 2 and 3 carry the reverb input and the reverb; the layout of the simulated EB20 follows the kb (`sysex_spec.kb.md` line 157-159) and is a test fixture, not a claim about the hardware.
- **Ranges of the parameters** (Table 25) and the way an out-of-range value is answered are not stated; the catalogue uses the signed-word ceiling (`-16383` to `16383`).
- **"0 = ON, 1 = MUTE"** (`&20`, `&21`): read literally, the value 0 means the channel is on (not muted); the primitives expose a boolean `muted`.
