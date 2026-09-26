# FTR-AKM-001: Transport and SysEx Configuration

## Overview

Phase A, lots A0 and A1 of the S5000 control plan
(`process/1.requirements/DRAFT-s5000-midi-primitives-and-workflows.md`), merged into one feature:
the plan's first hardware test — the Echo Message (§00/&06) — is an item of section §00 (A1) but is
what validates the transport (A0), so neither lot can be proven on the real sampler without the other.

**In scope.** The AKAI S5000/S6000 SysEx *transport*: frame encoding and decoding, value formats,
checksum, confirmation messages (OK / DONE / REPLY / ERROR), error numbers, user-ref correlation,
one-command-at-a-time send-and-wait with a timeout, command sequences that stop at the first failure,
session opening and closing (discovery, DeviceID binding, known §00 state), and every item of section §00 (SysEx
Configuration: `&00` Query, `&01` Notification, `&03` Sync LCD, `&04` Checksum, `&05` Auto screen
update, `&06` Echo, `&07` Still Alive — the spec has no `&02`).

**Out of scope.** Any other section (§02 System, apart from its two version items of RQ-AKM-044, §04
MIDI configuration, and the Program / Keygroup / Zone sections of FTR-AKM-002 to FTR-AKM-004); anything composed from primitives (Phase B
workflows); any user interface. Where the AKM code lives (own library or extension of the MIDI
layer) is an architecture decision, made in an ADR, not here.

**Property of §00 to keep in mind.** §00 has *no Get item*: its settings cannot be read back, so the
plan's "Get after Set confirms the value" criterion cannot apply to it. Its proof is DONE on the
mock and on the real sampler, plus the Echo round trip (RQ-AKM-015); the state left on the real
sampler is handled by RQ-AKM-018.

**Exit criterion.** RQ-AKM-001 to RQ-AKM-016, RQ-AKM-019, RQ-AKM-020 and RQ-AKM-039 to RQ-AKM-044 pass
against the simulated sampler in CI; RQ-AKM-017 has been run against the real S5000 and its observations recorded.

**Sources.** `documents/akai_s5000_s6000_sysex_spec_2.10.pdf.md` (printed pages 1–7, tables 1–3 and
5) via `documents/_index/sysex_spec.kb.md`; the same document's Modification History for 2.10.

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: FTR-AKM-002, FTR-AKM-003, FTR-AKM-004 (all their primitives go through this
  transport); the Phase B workflows; CI (mock-based tests).

---

## Functional Requirements

Terms: a **port** is one MIDI input/output pair of the sampler (the spec's ports A and B decode and
answer independently); a **command** is one SysEx message sent to the sampler; a
**confirmation** is one SysEx message sent back.

### RQ-AKM-001: Command frame encoding
- **Category**: Functional
- **EARS Type**: Complex
- **Statement**: WHEN a command (section, item, data bytes) is submitted for a target DeviceID, the AKM layer SHALL encode it as `F0 47 5E <dev> <user-ref…> <section> <item> <data…> [<checksum>] F7`, where `<dev>` holds the DeviceID (0–31) in bits 0–4 and the number of user-refs minus one in bits 5–6 (1 to 4 user-refs), and every byte between `5E` and `F7` is at most `7F`; IF the DeviceID is above 31, the user-ref count is outside 1–4 or any byte is above `7F`, THEN it SHALL refuse the command and produce no bytes.
- **Rationale**: framing and Table 1 of the spec (pp. 3–5); a malformed frame sent to the sampler is silently ignored, so it must be impossible to build one.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* DeviceID 5, one user-ref `10`, section `0C`, item `1B`, data `35 6D` and checksum ON, *When* encoded, *Then* the frame is `F0 47 5E 05 10 0C 1B 35 6D 59 F7` (the spec's own example, p. 5). *Given* two user-refs, *When* encoded, *Then* bit 5 of `<dev>` is set and bit 6 clear. *Given* DeviceID 32 or a data byte `80`, *When* encoding is requested, *Then* it is refused and no bytes are produced.
- **Dependencies**: RQ-MID-011

### RQ-AKM-002: Value formats
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The AKM layer SHALL encode and decode the value formats of the spec (pp. 8–9): word (`MSB LSB`, 14 bits), dword (4 bytes, most significant first), qword (8 bytes, most significant first, base 128), signed values (a sign byte, `00` positive or `01` negative, then the magnitude as byte, word or dword) and null-terminated ASCII strings, including lists of concatenated strings.
- **Rationale**: every item of every later section is expressed in these formats; getting one wrong corrupts values silently.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the word bytes `03 01`, *When* decoded, *Then* the value is 385. *Given* the signed word −37, *When* encoded and decoded, *Then* the bytes are `01 00 25` and the value round-trips. *Given* the string `AB`, *When* encoded, *Then* the bytes are `41 42 00`. *Given* the minimum and maximum value of each width, *When* encoded then decoded, *Then* each value is unchanged.
- **Dependencies**: None

### RQ-AKM-003: Checksum generation and verification
- **Category**: Functional
- **EARS Type**: State-driven
- **Statement**: WHILE checksum mode is ON for a port, the AKM layer SHALL append to every command a checksum equal to the unsigned 8-bit wrapping sum of all bytes from the first user-ref to the last data byte, ANDed with `7F`, and SHALL verify the checksum, taken as the last byte before `F7`, of every received confirmation; WHILE it is OFF, it SHALL treat every byte between the item and `F7` of a received confirmation as data; WHILE it is unknown, RQ-AKM-041 applies.
- **Rationale**: spec p. 4 — the sampler appends a checksum to its confirmations only when the mode is on, and since data length is variable the parser cannot tell a checksum from data unless it knows the mode.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* checksum ON and the bytes `10 0C 1B 35 6D`, *When* the checksum is computed, *Then* it is `59`. *Given* checksum ON and a received confirmation whose last byte before `F7` is wrong, *When* it is decoded, *Then* it is rejected (RQ-AKM-006). *Given* checksum OFF and a confirmation `… 52 0A 05 01 02 F7`, *When* it is decoded, *Then* the data is `01 02`; *Given* checksum ON and the same bytes with a valid checksum appended, *When* decoded, *Then* the data is `01 02`.
- **Dependencies**: RQ-AKM-001; RQ-AKM-004

### RQ-AKM-004: Confirmation decoding
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a complete SysEx message beginning `F0 47 5E` is received, the AKM layer SHALL decode its DeviceID, its user-refs (their count given by bits 5–6 of `<dev>`), its Reply ID (`4F` OK, `44` DONE, `52` REPLY, `45` ERROR), its section, its item and its data bytes, and expose them as a typed confirmation.
- **Rationale**: Table 2 and the confirmation format (pp. 5–6); decoding is the base of correlation and completion.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a confirmation frame with DeviceID `01`, user-ref `10`, Reply ID `44`, section `00`, item `04` and no data (constructed from Table 2; replaced by a captured frame under RQ-AKM-017), *When* decoded, *Then* the type is DONE, the DeviceID 1, the user-refs `[10]`, section 0, item 4 and the data empty. *Given* a Reply ID of each of the four kinds, *When* decoded, *Then* each is classified correctly. *Given* `<dev>` with bits 5–6 equal to `01`, *When* decoded, *Then* two user-refs are read.
- **Dependencies**: RQ-MID-021; RQ-AKM-003

### RQ-AKM-005: Error number decoding
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF an ERROR confirmation is received, THEN the AKM layer SHALL report its error number (`Data1 × 128 + Data2`) together with the meaning given in Table 3 of the spec (`00` not supported, `01` invalid format or insufficient data, `02` parameter out of range, `03` unknown error, `04` requested item not found, `05` item could not be created, `06` deletion failed, `81` checksum invalid, `101`–`112` disk errors, `181` requested keygroup does not exist in the current program), and SHALL keep the raw number when it is not in the table.
- **Rationale**: Table 3 (pp. 6–7); callers must distinguish "not found" from "out of range" to react correctly.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* an ERROR with `Data1 = 03` and `Data2 = 01`, *When* decoded, *Then* the number is 385 and its meaning is "requested keygroup does not exist in the current program". *Given* an ERROR with `Data1 = 7F` and `Data2 = 7F`, *When* decoded, *Then* the number 16383 is reported with the meaning "unknown", the raw number kept.
- **Dependencies**: RQ-AKM-002; RQ-AKM-004

### RQ-AKM-006: Malformed and foreign messages
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a received SysEx message is not addressed from an S5000/S6000 (bytes other than `47 5E` after `F0`), is truncated, carries an unknown Reply ID or, while checksum mode is ON, a wrong checksum, THEN the AKM layer SHALL discard it without completing any pending command and without throwing, and SHALL report a diagnostic naming the reason; the two-byte message `F0 F7` is not malformed (RQ-AKM-011).
- **Rationale**: the MIDI bus is shared (spec p. 3) and the input can carry other devices' SysEx; one stray message must not corrupt the exchange.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a pending command and a received `F0 43 10 4C … F7` (another manufacturer), *When* it arrives, *Then* the command stays pending and a "foreign message" diagnostic is reported. *Given* a confirmation with Reply ID `58`, *When* it arrives, *Then* it is discarded with an "unknown reply ID" diagnostic. *Given* a message shorter than the minimum confirmation, *When* it arrives, *Then* it is discarded and no exception is raised.
- **Dependencies**: RQ-AKM-004; RQ-MID-021

### RQ-AKM-007: User-ref allocation and confirmation matching
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a command is sent, the AKM layer SHALL stamp it with a user-ref that cycles through the 7-bit range so that consecutive commands never share one, and SHALL match every received confirmation to the pending command by DeviceID (when the bound target is `0`, any responder is accepted, until RQ-AKM-017 has shown which DeviceID a confirmation carries), echoed user-refs, section and item; a confirmation matching no pending command SHALL be reported as unsolicited and SHALL NOT complete anything, and a late ERROR after a REPLY SHALL be reported with the identity of the command that had already completed.
- **Rationale**: the spec allows a REPLY to be followed by an ERROR (p. 6); with cycling user-refs that late ERROR cannot be attributed to the next command.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* two consecutive commands, *When* they are sent, *Then* their user-refs differ. *Given* command 1 completed by a REPLY and command 2 pending, *When* an ERROR carrying command 1's user-ref arrives, *Then* it is reported as unsolicited and command 2 remains pending.
- **Dependencies**: RQ-AKM-001; RQ-AKM-004

### RQ-AKM-008: One outstanding command per port
- **Category**: Functional
- **EARS Type**: State-driven
- **Statement**: WHILE a command sent through a port awaits its completion (RQ-AKM-009), the AKM layer SHALL NOT send another command through that port, and SHALL queue submitted commands in order; commands on different ports are independent.
- **Rationale**: the sampler buffers input but the buffer can overflow and lose data (spec p. 1); since OS 2.10 one outstanding message per port synchronised on DONE/REPLY/ERROR guarantees musical MIDI is not disturbed (Modification History). This replaces the fixed-delay pacing of the ported Xpander framework.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* three commands submitted back to back on a simulated sampler that answers only when told to, *When* they are submitted, *Then* only the first frame is on the wire; *When* its DONE arrives, *Then* the second frame is sent, and so on in submission order. *Given* two ports, *When* one command is pending on each, *Then* both frames are on the wire.
- **Dependencies**: RQ-AKM-009

### RQ-AKM-009: Completion of a command
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN confirmations arrive for the pending command, the AKM layer SHALL treat OK as an intermediate acknowledgement that is not required, and SHALL complete the command on the first DONE (success of a write), REPLY (success of a read, carrying its data bytes) or ERROR (failure, carrying its error number), then release the port for the next queued command.
- **Rationale**: the normal flow is OK then DONE, REPLY or ERROR (p. 6); OK can be disabled (`&01`) but DONE, REPLY and ERROR cannot, so exactly one of them always ends a command.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* notification OFF and a DONE without any OK, *When* the DONE arrives, *Then* the command completes as successful. *Given* an OK then a REPLY with data `01 02`, *When* they arrive, *Then* the command completes with data `01 02` and the OK did not complete it. *Given* an OK then an ERROR `00 02`, *When* they arrive, *Then* the command completes as failed with error number 2.
- **Dependencies**: RQ-AKM-004; RQ-AKM-007

### RQ-AKM-010: Timeout
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF no DONE, REPLY or ERROR is received for the pending command within its timeout, or it has been pending longer than its maximum total wait (which Still Alive restarts, RQ-AKM-011, cannot extend), THEN the AKM layer SHALL complete it as timed out, release the port for the next queued command and report the timeout; the timeout and the maximum total wait SHALL be named, configurable values, overridable per command, and their defaults SHALL be set from the measurements of RQ-AKM-017.
- **Rationale**: the spec guarantees no time bound (only the Still Alive mechanism for long operations), so without a timeout one lost frame blocks the port forever; the value can only be determined on the real sampler.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler that never answers and a timeout T, *When* T elapses, *Then* the command completes as timed out and the next queued command is sent. *Given* a per-command timeout of 2T, *When* the sampler answers at 1.5T, *Then* the command succeeds. *Given* a sampler that sends `F0 F7` forever and never answers, *When* the maximum total wait elapses, *Then* the command completes as timed out.
- **Dependencies**: RQ-AKM-008; RQ-AKM-017

### RQ-AKM-011: Still Alive handling
- **Category**: Functional
- **EARS Type**: State-driven
- **Statement**: WHILE Still Alive monitoring is ON and a command is pending, the AKM layer SHALL treat each received `F0 F7` message as proof that the sampler is busy and SHALL restart the timeout of the pending command, without ever extending its maximum total wait (RQ-AKM-010).
- **Rationale**: `&07` makes the sampler send `F0 F7` about every second during long operations (spec, §00 footnote b); without it a slow operation would be timed out although healthy.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a pending command with timeout T and `F0 F7` received every T/2 for 3T followed by a DONE, *When* they arrive, *Then* the command completes as successful and never times out. *Given* Still Alive OFF, *When* `F0 F7` arrives, *Then* it does not restart the timeout.
- **Dependencies**: RQ-AKM-010; RQ-AKM-014

### RQ-AKM-012: Sampler discovery (Query)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN discovery is requested, the AKM layer SHALL send Query (§00/&00) with DeviceID 0, collect the OK and DONE confirmations received during a discovery window, and return the distinct DeviceIDs (bits 0–4) that answered; discovery is the only command that completes when its window ends rather than on its first DONE.
- **Rationale**: DeviceID 0 makes every sampler in the chain answer, each with its own DeviceID (p. 6), so a single DONE does not end the exchange.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* two simulated samplers with DeviceIDs 3 and 7, *When* discovery runs, *Then* the result is {3, 7}. *Given* no sampler answering, *When* the window ends, *Then* the result is empty and no error is raised. *Given* a sampler answering with user-ref count bits set in `<dev>`, *When* discovery runs, *Then* only bits 0–4 are reported as its DeviceID. The window length is a named, configurable value, set from RQ-AKM-017.
- **Dependencies**: RQ-AKM-004; RQ-AKM-017

### RQ-AKM-013: Checksum mode command (§00/&04)
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN checksum mode is set ON or OFF for a port, the AKM layer SHALL send §00/&04 with a valid checksum appended, whatever mode it currently assumes, and on DONE SHALL switch the port's mode so that RQ-AKM-003 applies to all following traffic; IF the command fails or times out, THEN it SHALL report the port's mode as unknown until a later mode command succeeds.
- **Rationale**: a checksum appended while the mode is off is tolerated by the sampler (p. 4, and the spec's own "turn off on all samplers" example), so the enabling and the disabling message are both accepted whichever state the sampler is really in; the mode cannot be read back.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* checksum OFF, *When* mode ON is set and DONE arrives, *Then* the next command carries a checksum. *Given* checksum ON, *When* mode OFF is set and DONE arrives, *Then* the next command carries none. *Given* the mode command times out, *When* the mode is queried, *Then* it is reported as unknown.
- **Dependencies**: RQ-AKM-003; RQ-AKM-009

### RQ-AKM-014: Other configuration toggles
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN Notification (`&01`), Sync LCD (`&03`), Auto screen update (`&05`) or Still Alive (`&07`) is set to `0` (OFF) or `1` (ON), the AKM layer SHALL send that §00 item with that single data byte and complete on DONE; IF a value other than `0` or `1` is requested, THEN it SHALL refuse it without sending.
- **Rationale**: these items are prerequisites of the later lots (deterministic LCD sync, optional acknowledgement) and share one shape.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* each of the four items and each value `0`, `1`, *When* set, *Then* the frame carries section `00`, that item code and that data byte, and the command completes on DONE. *Given* the value `2`, *When* set, *Then* it is refused and no frame is sent.
- **Dependencies**: RQ-AKM-009

### RQ-AKM-015: Echo round trip
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN an echo is requested with four data bytes each between `00` and `7F`, the AKM layer SHALL send §00/&06 and complete on a REPLY whose four data bytes equal those sent; IF the REPLY differs, THEN it SHALL fail with an echo-mismatch error naming both byte sequences.
- **Rationale**: the spec designs Echo "for debugging a controlling program" (§00 Table 5); it is the first test to run against the real sampler.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the bytes `01 23 45 67` and a sampler that echoes them, *When* echo runs, *Then* it succeeds. *Given* a sampler that answers `01 23 45 66`, *When* echo runs, *Then* it fails with the echo-mismatch error. *Given* a byte `80`, *When* echo is requested, *Then* it is refused without sending.
- **Dependencies**: RQ-AKM-009

### RQ-AKM-016: Validation against a simulated sampler
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The behaviour of RQ-AKM-001 to RQ-AKM-015 and RQ-AKM-039 to RQ-AKM-043 SHALL be verifiable without hardware against a simulated sampler that models the spec's behaviour — addressing (including DeviceID `0`), §00 state kept per port across sessions, checksum handling and ERROR `81`, the OK / DONE / REPLY / ERROR flows including a REPLY followed by an ERROR, items unsupported by older OS versions — and that can answer late, never or twice, send foreign and malformed frames, and emit `F0 F7`, delivering either on the sending thread or from another thread, in both cases possibly before the send returns.
- **Rationale**: the plan's method (Phase A, "Méthode de validation"): the mock validates the controller's logic in CI, the real sampler validates the firmware's behaviour.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler scripted to stay silent, *When* the timeout scenario of RQ-AKM-010 runs, *Then* it completes in CI in under one second of test time. *Given* the simulated sampler delivering a confirmation from another thread before the send returns, *When* a command is sent, *Then* it still completes exactly once. *Given* a fresh checkout without any MIDI hardware, *When* the test suite runs, *Then* every scenario of RQ-AKM-001 to RQ-AKM-015 and RQ-AKM-039 to RQ-AKM-043 runs and passes.
- **Dependencies**: RQ-MID-040

### RQ-AKM-017: Validation against the real sampler
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a real S5000 is connected, the AKM layer's primitives of this feature SHALL be exercisable against it through the system's real MIDI ports, and the observed behaviour SHALL be recorded: the response latency of Echo and Query over repeated runs, whether OK precedes DONE or REPLY, whether confirmations carry a checksum while it is ON, the exact frame shapes of DONE and ERROR, which DeviceID a confirmation carries when the command was sent with DeviceID 0, with the sampler's own DeviceID and with a different one, the sampler's behaviour on the default settings of every §00 item, whether §00 settings survive a power cycle, whether the JUCE backend delivers the two-byte `F0 F7` of Still Alive on the tested system, and the sampler's OS version.
- **Rationale**: the spec gives no timing guarantee and the plan defers the timeout and window values to measurement on the real sampler; the observations also replace the constructed frames used in the acceptance criteria above.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the S5000 connected and its ports visible to the system, *When* the Echo test runs, *Then* the REPLY equals the bytes sent. *Given* 50 repeated Echo round trips, *When* they complete, *Then* the minimum, median and maximum latency are recorded and the RQ-AKM-010 default and the RQ-AKM-012 window are set from them with the margin stated. *Given* the observed frames, *When* they are recorded, *Then* at least one captured DONE, REPLY and ERROR frame replaces the constructed examples of RQ-AKM-004.
- **Dependencies**: RQ-AKM-001 to RQ-AKM-015

### RQ-AKM-018: Real-sampler tests leave a known state
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: The real-sampler tests of this feature SHALL only change §00 session settings, never stored programs, multis or samples, and IF a test ends, fails or is interrupted, THEN it SHALL put those settings back to the values recorded under RQ-AKM-017 as the sampler's defaults (checksum OFF included) before returning.
- **Rationale**: §00 has no Get item, so the state found cannot be read back; leaving checksum ON would make every later session fail silently, and a test against the real sampler is not idempotent like the mock.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a real-sampler test that sets checksum ON then fails an assertion, *When* the test ends, *Then* checksum OFF has been sent and DONE received. *Given* the whole real-sampler suite, *When* it ends, *Then* no program, multi or sample of the sampler has been created, changed or deleted.
- **Dependencies**: RQ-AKM-013; RQ-AKM-014; RQ-AKM-017

### RQ-AKM-039: Session opening — discovery, then target DeviceID from configuration
- **Category**: Functional
- **EARS Type**: Complex
- **Statement**: WHEN a session is opened, the AKM layer SHALL start the port's input, run a broadcast discovery (a Query with DeviceID 0, RQ-AKM-012) and only then bind the target DeviceID (0–31) taken from the configuration it is given, with `0` (the sampler's own default) as the default value, using it for every later command of the session and exposing no per-call DeviceID; IF the target is not among the DeviceIDs that answered, THEN it SHALL fail with "no sampler at DeviceID N" together with the DeviceIDs that did answer; IF more than one sampler answered and either the target is `0` or a sampler answered with DeviceID `0`, THEN it SHALL fail as ambiguous; no command other than the discovery SHALL be sent before the open has succeeded.
- **Rationale**: owner decision — the DeviceID is an application setting with a default value; the layer does not own the settings storage, it receives the value. The DeviceID is set on the sampler itself and cannot be read or changed by SysEx. A sampler answers a message only if it carries its own DeviceID or `0` (spec p. 3), so the list of samplers present can only come from a broadcast, and a sampler whose DeviceID is `0` would also execute every command addressed to another sampler, which breaks RQ-AKM-009 and edits the wrong machine.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a configuration with DeviceID 5 and a simulated sampler with DeviceID 5, *When* the session opens, *Then* the first frame is a Query with DeviceID 0 and every later command carries DeviceID 5. *Given* a configuration with DeviceID 5 and a simulated sampler with DeviceID 3 only, *When* the session opens, *Then* it fails with "no sampler at DeviceID 5" and the list `[3]`. *Given* the default configuration and two simulated samplers, *When* the session opens, *Then* it fails as ambiguous and no frame other than the discovery was sent. *Given* a configuration with DeviceID 3 and simulated samplers with DeviceIDs 0 and 3, *When* the session opens, *Then* it fails as ambiguous. *Given* a DeviceID of 32, *When* the session is opened, *Then* it is refused before any frame is sent.
- **Dependencies**: RQ-AKM-001; RQ-AKM-012; RQ-AKM-041

### RQ-AKM-040: Known §00 state when a session opens
- **Category**: Functional
- **EARS Type**: Complex
- **Statement**: WHEN the target is bound (RQ-AKM-039), the AKM layer SHALL establish the port's §00 settings by sending explicitly the checksum mode first, with a checksum appended whatever mode it assumes (RQ-AKM-013, RQ-AKM-041), then each other setting of its configuration that is set to on or off, and SHALL report the session ready only after every one has completed; a setting configured as "unchanged" SHALL NOT be sent; IF the sampler answers ERROR `00` (not supported) to Sync LCD, Auto screen update or Still Alive, THEN the session SHALL open in a degraded mode that lists the unsupported items, and IF any other setting fails or times out, THEN the open SHALL fail. The provisional defaults are set by ADR-AKM-001 (DEC-AKM-007): checksum off, Sync LCD off, Still Alive on, Notification and Auto screen update unchanged.
- **Rationale**: §00 has no Get item, so the state a previous session, another program or a crash left on the sampler cannot be read; assuming it would silently break framing (a checksum left on) or change which confirmations arrive. Sync LCD (`&03`) exists since OS 2.00 and Still Alive (`&07`) since OS 2.10 (spec Modification History), so an older sampler answers ERROR `00`, which is not a reason to refuse the connection.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler left with checksum on, *When* a session opens with checksum off configured, *Then* the checksum-mode command carries a valid checksum, completes on DONE, and the commands after it carry none. *Given* a configuration of Sync LCD off, Still Alive on, Notification unchanged and Auto screen update unchanged, *When* the session opens, *Then* the two configured items are sent with those values before the session is reported ready and the other two are not sent. *Given* a simulated sampler that answers ERROR `00` to Still Alive, *When* the session opens, *Then* it opens degraded and reports that item as unsupported. *Given* the checksum-mode command timing out, *When* the session opens, *Then* the open fails.
- **Dependencies**: RQ-AKM-013; RQ-AKM-014; RQ-AKM-039

### RQ-AKM-041: Unknown checksum mode
- **Category**: Functional
- **EARS Type**: State-driven
- **Statement**: WHILE a port's checksum mode is unknown — at session start, after a checksum-mode command failed or timed out, and after a configured number of consecutive received confirmations failed verification (provisionally 3, a named value) — the AKM layer SHALL append a checksum to every command, SHALL decode each confirmation by the expected data length of its item (OK and DONE none, ERROR two, Echo four), accepting one extra trailing byte only if it is a valid checksum, and SHALL refuse with a "checksum mode unknown" result any command whose REPLY has a variable length; every change of the mode SHALL be reported.
- **Rationale**: §00 cannot be read back, and a sampler configured by a previous session, or one that has rebooted, may or may not add a checksum to its confirmations (p. 4); since data length is variable, a parser cannot tell a checksum from data unless the length is known. A checksum sent while checksums are off is ignored by the sampler (p. 4).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the mode unknown and a DONE frame without checksum, *When* it is decoded, *Then* it is a DONE. *Given* the mode unknown and the same DONE with a valid checksum byte appended, *When* it is decoded, *Then* it is a DONE. *Given* the mode unknown and the same DONE with a wrong extra byte, *When* it is decoded, *Then* it is rejected (RQ-AKM-006). *Given* the mode unknown and a command whose REPLY has a variable length, *When* it is submitted, *Then* it is refused without sending. *Given* checksum on and three consecutive confirmations failing verification, *When* the third is received, *Then* the mode is reported as unknown.
- **Dependencies**: RQ-AKM-003; RQ-AKM-004; RQ-AKM-013

### RQ-AKM-042: Session closing
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a session is closed, the AKM layer SHALL complete the command in flight and every queued command as cancelled, then, where the sampler still answers, restore the §00 settings it changed to the sampler's documented defaults (checksum off, Sync LCD on; the other defaults being those recorded under RQ-AKM-017), then stop the port's input; IF the restoring commands fail or time out, THEN it SHALL still finish closing and report the failure; closing a session from one of its own completions SHALL be refused.
- **Rationale**: leaving checksum on or Sync LCD off would disturb the next program that uses the port — exactly the unknown state RQ-AKM-040 has to work around; and JUCE's input cannot be stopped from inside its own callback (it would never return).
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a session opened with checksum on and two commands queued, *When* it is closed, *Then* the in-flight and queued commands complete as cancelled, then a checksum-mode command turning it off is sent with a valid checksum and completes on DONE. *Given* a sampler that no longer answers, *When* the session is closed, *Then* closing returns once the restoring commands have timed out and the failure is reported. *Given* a close requested from a completion, *When* it is called, *Then* it is refused.
- **Dependencies**: RQ-AKM-013; RQ-AKM-040

### RQ-AKM-043: Command sequences stop at the first failure
- **Category**: Functional
- **EARS Type**: Complex
- **Statement**: WHEN a caller submits a sequence of commands, the AKM layer SHALL run them in order with no other queued command interleaved, and IF one of them fails (ERROR, timeout or refusal), THEN it SHALL complete the remaining ones as cancelled and report the index of the failure.
- **Rationale**: §0A, §08 and §06 act on the *current* program and keygroup (spec state model), so "select, then set" is one unit: a select that fails followed by a set would edit the wrong item.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a sequence of a select and two sets and an ERROR answering the select, *When* it runs, *Then* the two sets are never sent, both complete as cancelled and the failure index is 0. *Given* a sequence running and another command submitted meanwhile, *When* the sequence ends, *Then* the other command ran after it, not between its commands. *Given* every command succeeding, *When* the sequence ends, *Then* the result of each is reported in order.
- **Dependencies**: RQ-AKM-008; RQ-AKM-009

### RQ-AKM-044: Sampler operating-system version
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the version of the sampler's operating system is requested, the AKM layer SHALL send Get Operating System Software Version (§02/&00) and Get the Sub-Version (§02/&01) and return the major and minor numbers (Data1 and Data2 of the first REPLY) and the sub-version (Data1 of the second), so that the application can decide which items the connected sampler supports instead of trying them and interpreting ERROR `00`.
- **Rationale**: owner decision (TASK-AKM-012): items exist only since certain OS versions (Sync LCD since 2.00, Still Alive since 2.10, RQ-AKM-013 and RQ-AKM-040), and an application that adapts to the version it meets needs to read it; §02/&00 and §02/&01 are the two items of Table 6 that give it (the spec says the sub-version is always zero for now).
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler running OS 2.10, *When* the version is requested, *Then* the result is major 2, minor 10, sub-version 0. *Given* a sampler that answers ERROR to the sub-version request, *When* the version is requested, *Then* the major and minor numbers are still returned and the sub-version is reported as unavailable. *Given* a sampler that answers ERROR to the first request, *When* the version is requested, *Then* the failure is reported and nothing is assumed about the version.
- **Dependencies**: RQ-AKM-004; RQ-AKM-009; RQ-AKM-013; RQ-AKM-017
- **Status**: observed on the real sampler by the first-contact probe (TASK-AKM-012); the typed primitive comes with the item catalogue (TASK-AKM-008).

---

## Non-Functional Requirements

### RQ-AKM-019: Independence from the MIDI backend
- **Category**: Non-Functional
- **NFR Type**: Maintainability
- **EARS Type**: Ubiquitous
- **Statement**: The AKM layer SHALL depend only on the abstract MIDI backend interface and SHALL expose no JUCE type in its public headers, so the same test scenario runs unchanged on the in-memory backend and on the real one.
- **Metric**: 0 JUCE includes in the public headers of the AKM layer; 1 test scenario source shared by both backends.
- **Measurement Method**: search of the public headers for JUCE includes; the suite of RQ-AKM-016 and the real-sampler suite of RQ-AKM-017 built from the same scenario definitions.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the public headers of the AKM layer, *When* searched for a JUCE include, *Then* there is no match. *Given* the Echo scenario, *When* run on each backend, *Then* it is the same source and both pass.
- **Dependencies**: RQ-MID-040

### RQ-AKM-020: Non-blocking completion
- **Category**: Non-Functional
- **NFR Type**: Reliability
- **EARS Type**: State-driven
- **Statement**: WHILE confirmations are delivered on the backend's callback thread (RQ-MID-024), the AKM layer SHALL only record them there, without blocking that thread and without sending a frame or invoking a completion from it; it SHALL invoke every completion and diagnostic on the session's own thread, one at a time, in the order the commands completed, and SHALL NOT require its callers to block the UI thread while waiting.
- **Metric**: 0 calls that wait on a caller from the backend's callback thread; completions delivered in arrival order over 1000 scripted exchanges.
- **Measurement Method**: a scenario with the simulated sampler answering from a separate thread and a caller that never waits; ordering asserted over 1000 exchanges.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a caller that submits a command and returns immediately, *When* the confirmation arrives on another thread, *Then* the completion is delivered without the caller having blocked. *Given* 1000 commands answered in order, *When* they complete, *Then* the completions are delivered in that order. *Given* a completion that submits a new command while others are queued, *When* it runs, *Then* the new command is queued behind them. *Given* a completion, *When* it runs, *Then* it does so on the session's thread and never on the backend's callback thread.
- **Dependencies**: RQ-MID-024; RQ-AKM-008

---

## Open points

- **Timeout, maximum total wait and discovery-window values** (RQ-AKM-010, RQ-AKM-012), and the number of failed verifications that makes the checksum mode unknown (RQ-AKM-041): unknown until RQ-AKM-017 has run. Until then the architecture uses provisional named constants, replaced by the measured values.
- **Where the AKM code lives, threading, time injection and the shape of primitives**: decided in ADR-AKM-001 (Accepted, amended after an independent review, `process/2.architecture/REVIEW-ADR-AKM-001-opus.md`).
- **Application settings** (storage, editing UI, MIDI port names, target DeviceID, the §00 values of RQ-AKM-040): not part of this feature, which only receives them as configuration when a session is opened (RQ-AKM-039); to be specified with the application's settings layer, which does not exist in this repository yet.
- **Matching a confirmation's DeviceID when the target is `0`** (RQ-AKM-007): provisionally any responder is accepted, matching on user-refs, section and item; the final rule is fixed after the observations of RQ-AKM-017, because the spec is ambiguous about which DeviceID a reply carries (p. 6: the sampler's own; the confirmation format: as sent).
- **Default state of the §00 items** (RQ-AKM-018, RQ-AKM-042): the spec documents checksum as off by default (p. 4) and synchronisation as on by default (introductions of §0A and §0E); it states no default for Notification, Auto screen update or Still Alive (Table 5 states none for any item); those are observed and recorded under RQ-AKM-017.
- **Sync LCD across ports** (RQ-AKM-014, RQ-AKM-040): Table 5 footnote a warns that with synchronisation on, a program change made by SysEx on one port also changes the selection on another port that has it on, and advises turning it off except when needed. ADR-AKM-001 (DEC-AKM-007) provisionally sets it off when a session opens and restores it on close (RQ-AKM-042); FTR-AKM-002 revisits this when the first program-selecting primitive exists.
- **Older OS versions** (RQ-AKM-040): the spec says Sync LCD exists since OS 2.00 and Still Alive since OS 2.10; how a sampler on an older OS answers, and which OS the owner's sampler runs, are recorded under RQ-AKM-017 (the OS version is read with §02/&00, RQ-AKM-044).
