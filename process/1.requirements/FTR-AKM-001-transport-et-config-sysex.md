# FTR-AKM-001: Transport and SysEx Configuration

## Overview

Phase A, lots A0 and A1 of the S5000 control plan
(`process/1.requirements/DRAFT-s5000-midi-primitives-and-workflows.md`), merged into one feature:
the plan's first hardware test — the Echo Message (§00/&06) — is an item of section §00 (A1) but is
what validates the transport (A0), so neither lot can be proven on the real sampler without the other.

**In scope.** The AKAI S5000/S6000 SysEx *transport*: frame encoding and decoding, value formats,
checksum, confirmation messages (OK / DONE / REPLY / ERROR), error numbers, user-ref correlation,
one-command-at-a-time send-and-wait with a timeout, and every item of section §00 (SysEx
Configuration: `&00` Query, `&01` Notification, `&03` Sync LCD, `&04` Checksum, `&05` Auto screen
update, `&06` Echo, `&07` Still Alive — the spec has no `&02`).

**Out of scope.** Any other section (§02 System, §04 MIDI configuration, and the Program / Keygroup /
Zone sections of FTR-AKM-002 to FTR-AKM-004); anything composed from primitives (Phase B
workflows); any user interface. Where the AKM code lives (own library or extension of the MIDI
layer) is an architecture decision, made in an ADR, not here.

**Property of §00 to keep in mind.** §00 has *no Get item*: its settings cannot be read back, so the
plan's "Get after Set confirms the value" criterion cannot apply to it. Its proof is DONE on the
mock and on the real sampler, plus the Echo round trip (RQ-AKM-015); the state left on the real
sampler is handled by RQ-AKM-018.

**Exit criterion.** RQ-AKM-001 to RQ-AKM-016, RQ-AKM-019, RQ-AKM-020, RQ-AKM-039 and RQ-AKM-040 pass
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
- **Statement**: WHILE checksum mode is ON for a port, the AKM layer SHALL append to every command a checksum equal to the unsigned 8-bit wrapping sum of all bytes from the first user-ref to the last data byte, ANDed with `7F`, and SHALL verify the checksum, taken as the last byte before `F7`, of every received confirmation; WHILE it is OFF, it SHALL treat every byte between the item and `F7` of a received confirmation as data.
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
- **Statement**: WHEN a command is sent, the AKM layer SHALL stamp it with a user-ref that cycles through the 7-bit range so that consecutive commands never share one, and SHALL match every received confirmation to the pending command by DeviceID and echoed user-refs; a confirmation matching no pending command SHALL be reported as unsolicited and SHALL NOT complete anything.
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
- **Statement**: IF no DONE, REPLY or ERROR is received for the pending command within its timeout, THEN the AKM layer SHALL complete it as timed out, release the port for the next queued command and report the timeout; the timeout SHALL be a named, configurable value, overridable per command, and its default SHALL be set from the measurements of RQ-AKM-017.
- **Rationale**: the spec guarantees no time bound (only the Still Alive mechanism for long operations), so without a timeout one lost frame blocks the port forever; the value can only be determined on the real sampler.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler that never answers and a timeout T, *When* T elapses, *Then* the command completes as timed out and the next queued command is sent. *Given* a per-command timeout of 2T, *When* the sampler answers at 1.5T, *Then* the command succeeds.
- **Dependencies**: RQ-AKM-008; RQ-AKM-017

### RQ-AKM-011: Still Alive handling
- **Category**: Functional
- **EARS Type**: State-driven
- **Statement**: WHILE Still Alive monitoring is ON and a command is pending, the AKM layer SHALL treat each received `F0 F7` message as proof that the sampler is busy and SHALL restart the timeout of the pending command.
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
- **Statement**: The behaviour of RQ-AKM-001 to RQ-AKM-015 SHALL be verifiable without hardware against a scripted simulated sampler built on the in-memory loopback backend, able to answer with OK, DONE, REPLY and ERROR, to answer late or never, to send duplicate, unsolicited, foreign and malformed frames, and to emit `F0 F7`.
- **Rationale**: the plan's method (Phase A, "Méthode de validation"): the mock validates the controller's logic in CI, the real sampler validates the firmware's behaviour.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler scripted to stay silent, *When* the timeout scenario of RQ-AKM-010 runs, *Then* it completes in CI in under one second of test time. *Given* a fresh checkout without any MIDI hardware, *When* the test suite runs, *Then* every scenario of RQ-AKM-001 to RQ-AKM-015 runs and passes.
- **Dependencies**: RQ-MID-041

### RQ-AKM-017: Validation against the real sampler
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a real S5000 is connected, the AKM layer's primitives of this feature SHALL be exercisable against it through the system's real MIDI ports, and the observed behaviour SHALL be recorded: the response latency of Echo and Query over repeated runs, whether OK precedes DONE or REPLY, whether confirmations carry a checksum while it is ON, the exact frame shapes of DONE and ERROR, which DeviceID a confirmation carries when the command was sent with DeviceID 0, with the sampler's own DeviceID and with a different one, and the sampler's behaviour on the default settings of every §00 item.
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

### RQ-AKM-039: Target DeviceID from configuration
- **Category**: Functional
- **EARS Type**: Complex
- **Statement**: WHEN a session is opened, the AKM layer SHALL take the target DeviceID (0–31) from the configuration it is given, with `0` (the sampler's own default) as the default value, SHALL use it for every command of that session and SHALL expose no per-call DeviceID; WHEN the session is opened it SHALL then send a Query to that DeviceID, and IF the target does not answer, THEN it SHALL report "no sampler at DeviceID N" together with the DeviceIDs that did answer; IF the target is `0` and more than one sampler answers, THEN it SHALL report the ambiguity instead of opening the session.
- **Rationale**: owner decision — the DeviceID is an application setting with a default value; the layer does not own the settings storage, it receives the value. The DeviceID is set on the sampler itself and cannot be read or changed by SysEx, so only a Query at connection detects a value changed on the machine between two sessions; DeviceID `0` is a broadcast (spec p. 3), so with several samplers each command would draw several DONE and break RQ-AKM-009.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a configuration with DeviceID 5 and a simulated sampler with DeviceID 5, *When* the session opens, *Then* the Query is addressed to DeviceID 5 and every later command carries DeviceID 5. *Given* a configuration with DeviceID 5 and a simulated sampler answering only as DeviceID 3, *When* the session opens, *Then* it is refused with "no sampler at DeviceID 5" and the list `[3]`. *Given* the default configuration and two simulated samplers, *When* the session opens, *Then* it is refused as ambiguous and no command is sent. *Given* a DeviceID of 32, *When* the session is opened, *Then* it is refused.
- **Dependencies**: RQ-AKM-001; RQ-AKM-012

### RQ-AKM-040: Known §00 state when a session opens
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a session has been opened (RQ-AKM-039), the AKM layer SHALL put the port's §00 settings in a known state by sending each of them explicitly — checksum mode first, with a valid checksum appended (RQ-AKM-013), then Notification, Sync LCD, Auto screen update and Still Alive — with the values given in its configuration, and SHALL report the session as ready only after every one has completed.
- **Rationale**: §00 has no Get item, so the state a previous session, another program or a crash left on the sampler cannot be read; assuming it would silently break framing (a checksum left ON) or completion (Notification left OFF).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler left with checksum ON, *When* a session opens with checksum OFF configured, *Then* the checksum-mode command carries a valid checksum, completes on DONE, and the commands after it carry none. *Given* a configuration of Notification OFF, Sync LCD OFF, Auto screen update OFF and Still Alive ON, *When* the session opens, *Then* the four items are sent with those values before the session is reported ready. *Given* one of the settings times out, *When* the session opens, *Then* it is reported as failed rather than ready.
- **Dependencies**: RQ-AKM-013; RQ-AKM-014; RQ-AKM-039

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
- **Statement**: WHILE confirmations are delivered on the backend's callback thread (RQ-MID-024), the AKM layer SHALL process them without blocking that thread, SHALL deliver completions in the order confirmations arrived, and SHALL not require its callers to block the UI thread while waiting.
- **Metric**: 0 calls that wait on a caller from the backend's callback thread; completions delivered in arrival order over 1000 scripted exchanges.
- **Measurement Method**: a scenario with the simulated sampler answering from a separate thread and a caller that never waits; ordering asserted over 1000 exchanges.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a caller that submits a command and returns immediately, *When* the confirmation arrives on another thread, *Then* the completion is delivered without the caller having blocked. *Given* 1000 commands answered in order, *When* they complete, *Then* the completions are delivered in that order.
- **Dependencies**: RQ-MID-024; RQ-AKM-008

---

## Open points

- **Timeout and discovery-window values** (RQ-AKM-010, RQ-AKM-012): unknown until RQ-AKM-017 has run. Until then the architecture may use a provisional named constant, replaced by the measured value.
- **Where the AKM code lives** (own library or extension of the MIDI layer) and how time is injected for the timeout scenarios: architecture, to be decided in an ADR (ADR-AKM-001).
- **Application settings** (storage, editing UI, MIDI port names, target DeviceID, the §00 values of RQ-AKM-040): not part of this feature, which only receives them as configuration when a session is opened (RQ-AKM-039); to be specified with the application's settings layer, which does not exist in this repository yet.
- **Matching a confirmation's DeviceID when the target is `0`** (RQ-AKM-007): the rule is fixed after the observations of RQ-AKM-017, because the spec is ambiguous about which DeviceID a reply carries (p. 6: the sampler's own; the confirmation format: as sent).
- **Default state of the §00 items** (RQ-AKM-018): Table 5 states none for `&01`, `&03`, `&05`, `&07` (only checksum is documented as off by default, p. 4); to be observed and recorded under RQ-AKM-017.
- **Sync LCD across ports** (RQ-AKM-014): Table 5 footnote a warns that with synchronisation on, a program change made by SysEx on one port also changes the selection on another port that has it on, and advises turning it off except when needed. Whether the AKM layer turns it off by default is a decision for the lots that select programs (FTR-AKM-002), not for this transport.
