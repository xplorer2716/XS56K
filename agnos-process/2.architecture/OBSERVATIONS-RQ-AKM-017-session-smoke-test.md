# Observations of a session on the real S5000 (RQ-AKM-017, TASK-AKM-013)

The owner ran `xs56k_akm_probe --session` against the real sampler on 2026-09-26. This file keeps the log
verbatim and what it shows, and lists the artifacts that were amended because of it. It is an observation record,
not an artifact with an ID: the process index ignores it. The first contact is in
`OBSERVATIONS-RQ-AKM-017-first-contact.md`.

## Setup

- The same sampler and path as the first contact: AKAI S5000, **OS 2.14, sub-version 0** (read again, step 3),
  Windows, `JuceMidiBackend`, an ESI M8U eX USB MIDI interface, input `MIDIIN2 (ESI M8U eX)`, output
  `MIDIOUT15 (ESI M8U eX)`, DeviceID 0.
- Program: `xs56k_akm_probe --session` from the Release build of commit `7fa6f43`, every option at its default
  (3 s per step, 500 ms discovery window, 50 Echo round trips, the LCD settings switched).
- What ran: a real `Session` — the real scheduler thread, the worker-thread executor, JUCE's input callbacks —
  driven through the section 00 primitives. The opening (discovery, target check, binding) is done by hand by the
  scenario, `Session::open` being TASK-AKM-009.

## What the log shows

| # | Observation | Evidence | Consequence |
|---|---|---|---|
| F1 | The session works end to end: 24 steps, 75 frames sent, 148 received (every command is answered by an OK and a DONE or REPLY, but the two that follow Notification off, F6), 50 of 50 Echo round trips, the OS version 2.14 / 0 read through the session, **no message rejected by the session, no unsolicited confirmation, no late ERROR, no input error**; the run ended with checksums off, Still Alive off, Notification on, Sync LCD on and Auto screen update off. | whole log | The session core (TASK-AKM-006) and the section 00 primitives (TASK-AKM-008) work on the JUCE backend and the S5000. Nothing to correct in their logic. |
| F2 | A Query to DeviceID 0 with a checksum appended (`... 00 00 00 F7`) is accepted while checksums are off: OK, then DONE, both with DeviceID 0. | step 1 | DEC-AKM-007 step 2 (discovery before the mode is known) is safe on the real sampler; the first contact had shown it for the Echo only. |
| F3 | The discovery window of 500 ms ended 517 ms after the submit. | step 1 | The named window works. The 17 ms are the send and the timer's granularity on Windows; the provisional default stays. |
| F4 | **Both transitions of the checksum mode, through the session, reproduce the first contact.** Switching on: the OK carries no checksum (the mode in force), the DONE carries one (`... 44 00 04 7F F7`). Switching off: the OK carries one (`... 4F 00 04 0E F7`), the DONE none. The session, decoding those confirmations in mode unknown, followed the sampler: unknown, off, on, off, and framed each next command accordingly. | steps 2, 6, 9 | Confirms DEC-AKM-009 in the code path itself; before, only captured frames were replayed. |
| F5 | With checksums on, the REPLYs of the Echo (4 data bytes), of the OS version (2) and of the sub-version (1) are decoded, checksum stripped; OK and DONE too. | steps 7, 8 | The lengths of the item catalogue (DEC-AKM-012) hold on the real sampler. |
| F6 | After Notification off, the sampler sends no OK: the Echo is answered by its REPLY alone. **The command that switches Notification back on gets no OK either**: an OK follows the setting in force when the command arrived, the same rule as for checksums. | steps 11 to 13 | The simulated sampler already models it (TASK-AKM-007); now observed. It is why 148 = 2 x 75 - 2. |
| F7 | Sync LCD (§00/&03) and Auto screen update (§00/&05) are accepted by OS 2.14, on and off: OK, DONE. Still Alive on and off: DONE (known). | steps 14 to 19 | RQ-AKM-014 is workable as written; no ERROR 0 on this OS. |
| F8 | JUCE delivered every message whole: 148 messages, none split and none merged, no input error. | whole log | The SysEx path of the backend is sound at this rate (one command at a time). |
| F9 | The 75 commands took user-refs 00 to 4A, each confirmation carrying its command's; the wrap-around at 7F was not reached. | whole log | RQ-AKM-007 holds; the wrap stays covered by the simulated sampler only. |
| F10 | **The log slowed down the exchanges it recorded.** Measured on this log: OUT to OK 10 to 17 ms (median 13), OUT to DONE or REPLY 12 to 29 ms (median 25), 9 to 16 ms (median 12) between the two confirmations of a command, against 6 to 8, 9 to 12 and 3 to 4 ms at the first contact. A command with no OK (Notification off) is answered 12 to 15 ms after it is sent. The 50 Echo round trips, timed by the scenario from submit to completion, took 34 to 42 ms (median 40). The lines of an OK are 129 to 135 characters and the gap after them is the 12 ms: `LoggingInputPort` wrote each line to the console from JUCE's input callback, before passing the message on to the session, and `LoggingOutputPort` wrote before sending; a long console line costs about that. | analysis of the log | **These times are not the sampler's and are not a source for DEC-AKM-006**; the first contact's remain. `WireLog` now records in memory and writes between the steps, never while a command is in flight (this task's follow-up commit); a second run will show whether the times fall back to the first contact's, which is the check of this diagnosis. |
| F11 | The `on:` reading of the Echo REPLY at 1.561 s happens to be valid: its payload byte 67 equals the checksum of that frame. | line at 1.561 | None. The two readings in the log are a diagnostic aid; the session used the mode it knew (off). |

## Not observed

`F0 F7` delivery (Still Alive was on, but no slow command was sent); clean latencies (F10); the defaults of Sync LCD,
Auto screen update and Still Alive on a fresh sampler (they cannot be read back); whether §00 settings survive a
power cycle; the second port (B); slow operations and so the real timeout and maximum total wait; the wrap-around of
the user-refs; an OS older than 2.14. All of these are for TASK-AKM-010.

## Amendments made because of it

- `WireLog` records frames and notes in memory and writes them on `flush()`; the scenario flushes at the start of
  each step and of each timed Echo, and at its end; tests added (nothing written before the flush, order kept,
  recording never waits for a blocked stream, the destructor writes the rest, progressive output of the scenario).
- ADR-AKM-001: DEC-AKM-006 says the log of this run is not a source of latency; DEC-AKM-007 and DEC-AKM-009 record
  what the session did on the real sampler; the risks paragraph is brought up to date.
- FTR-AKM-001: the status of RQ-AKM-017.
- PLAN-AKM-001: TASK-AKM-013.

## The log, verbatim

`akm-session-20260926-215602.log`, unchanged.

```text
# XS56K AKM session smoke test
# started 2026-09-26T21:56:08Z
# target: in="MIDIIN2 (ESI M8U eX)" out="MIDIOUT15 (ESI M8U eX)" device-id=0
# It drives a Session through the section 00 primitives. It switches checksums, Notification, Sync LCD, Auto screen update and Still Alive on and off, and ends by putting them back: checksums off, Still Alive off, Notification on, Sync LCD on, Auto screen update off.
# step 1: Discovery: a Query to DeviceID 0, every sampler answers with its own DeviceID
    0.079  OUT  F0 47 5E 00 00 00 00 00 F7
    0.091  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    0.101  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
#   answered by DeviceIDs: 0 after 517 ms
#   DeviceID 0 is bound as the target
# step 2: Checksums off (section 00, item 04), sent with a checksum whatever the sampler's state
    0.634  OUT  F0 47 5E 00 01 00 04 00 05 F7
    0.645  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    0.658  IN   F0 47 5E 00 01 44 00 04 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
#   DONE after 40 ms
# step 3: Operating system version (section 02, items 00 and 01)
    0.686  OUT  F0 47 5E 00 02 02 00 F7
    0.697  IN   F0 47 5E 00 02 4F 02 00 F7 | off: OK dev 0 ref 02 sec 02 item 00 data - | on: rejected: checksum missing or wrong
    0.707  IN   F0 47 5E 00 02 52 02 00 02 0E F7 | off: REPLY dev 0 ref 02 sec 02 item 00 data 02 0E | on: rejected: checksum missing or wrong
    0.723  OUT  F0 47 5E 00 03 02 01 F7
    0.734  IN   F0 47 5E 00 03 4F 02 01 F7 | off: OK dev 0 ref 03 sec 02 item 01 data - | on: rejected: checksum missing or wrong
    0.748  IN   F0 47 5E 00 03 52 02 01 00 F7 | off: REPLY dev 0 ref 03 sec 02 item 01 data 00 | on: rejected: checksum missing or wrong
#   OS 2.14, sub-version 0 after 74 ms
# step 4: Echo 01 23 45 67, checksums off
    0.783  OUT  F0 47 5E 00 04 00 06 01 23 45 67 F7
    0.796  IN   F0 47 5E 00 04 4F 00 06 F7 | off: OK dev 0 ref 04 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    0.809  IN   F0 47 5E 00 04 52 00 06 01 23 45 67 F7 | off: REPLY dev 0 ref 04 sec 00 item 06 data 01 23 45 67 | on: rejected: checksum missing or wrong
#   REPLY 01 23 45 67 as sent after 41 ms
# step 5: 50 Echo round trips, one after the other, timed from submit to completion
    0.837  OUT  F0 47 5E 00 05 00 06 00 23 45 67 F7
    0.851  IN   F0 47 5E 00 05 4F 00 06 F7 | off: OK dev 0 ref 05 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    0.863  IN   F0 47 5E 00 05 52 00 06 00 23 45 67 F7 | off: REPLY dev 0 ref 05 sec 00 item 06 data 00 23 45 67 | on: rejected: checksum missing or wrong
    0.879  OUT  F0 47 5E 00 06 00 06 01 23 45 67 F7
    0.892  IN   F0 47 5E 00 06 4F 00 06 F7 | off: OK dev 0 ref 06 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    0.905  IN   F0 47 5E 00 06 52 00 06 01 23 45 67 F7 | off: REPLY dev 0 ref 06 sec 00 item 06 data 01 23 45 67 | on: rejected: checksum missing or wrong
    0.922  OUT  F0 47 5E 00 07 00 06 02 23 45 67 F7
    0.935  IN   F0 47 5E 00 07 4F 00 06 F7 | off: OK dev 0 ref 07 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    0.947  IN   F0 47 5E 00 07 52 00 06 02 23 45 67 F7 | off: REPLY dev 0 ref 07 sec 00 item 06 data 02 23 45 67 | on: rejected: checksum missing or wrong
    0.966  OUT  F0 47 5E 00 08 00 06 03 23 45 67 F7
    0.979  IN   F0 47 5E 00 08 4F 00 06 F7 | off: OK dev 0 ref 08 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    0.992  IN   F0 47 5E 00 08 52 00 06 03 23 45 67 F7 | off: REPLY dev 0 ref 08 sec 00 item 06 data 03 23 45 67 | on: rejected: checksum missing or wrong
    1.008  OUT  F0 47 5E 00 09 00 06 04 23 45 67 F7
    1.021  IN   F0 47 5E 00 09 4F 00 06 F7 | off: OK dev 0 ref 09 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.033  IN   F0 47 5E 00 09 52 00 06 04 23 45 67 F7 | off: REPLY dev 0 ref 09 sec 00 item 06 data 04 23 45 67 | on: rejected: checksum missing or wrong
    1.049  OUT  F0 47 5E 00 0A 00 06 05 23 45 67 F7
    1.062  IN   F0 47 5E 00 0A 4F 00 06 F7 | off: OK dev 0 ref 0A sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.074  IN   F0 47 5E 00 0A 52 00 06 05 23 45 67 F7 | off: REPLY dev 0 ref 0A sec 00 item 06 data 05 23 45 67 | on: rejected: checksum missing or wrong
    1.088  OUT  F0 47 5E 00 0B 00 06 06 23 45 67 F7
    1.101  IN   F0 47 5E 00 0B 4F 00 06 F7 | off: OK dev 0 ref 0B sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.113  IN   F0 47 5E 00 0B 52 00 06 06 23 45 67 F7 | off: REPLY dev 0 ref 0B sec 00 item 06 data 06 23 45 67 | on: rejected: checksum missing or wrong
    1.128  OUT  F0 47 5E 00 0C 00 06 07 23 45 67 F7
    1.141  IN   F0 47 5E 00 0C 4F 00 06 F7 | off: OK dev 0 ref 0C sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.153  IN   F0 47 5E 00 0C 52 00 06 07 23 45 67 F7 | off: REPLY dev 0 ref 0C sec 00 item 06 data 07 23 45 67 | on: rejected: checksum missing or wrong
    1.169  OUT  F0 47 5E 00 0D 00 06 08 23 45 67 F7
    1.183  IN   F0 47 5E 00 0D 4F 00 06 F7 | off: OK dev 0 ref 0D sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.196  IN   F0 47 5E 00 0D 52 00 06 08 23 45 67 F7 | off: REPLY dev 0 ref 0D sec 00 item 06 data 08 23 45 67 | on: rejected: checksum missing or wrong
    1.211  OUT  F0 47 5E 00 0E 00 06 09 23 45 67 F7
    1.224  IN   F0 47 5E 00 0E 4F 00 06 F7 | off: OK dev 0 ref 0E sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.237  IN   F0 47 5E 00 0E 52 00 06 09 23 45 67 F7 | off: REPLY dev 0 ref 0E sec 00 item 06 data 09 23 45 67 | on: rejected: checksum missing or wrong
    1.253  OUT  F0 47 5E 00 0F 00 06 0A 23 45 67 F7
    1.266  IN   F0 47 5E 00 0F 4F 00 06 F7 | off: OK dev 0 ref 0F sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.279  IN   F0 47 5E 00 0F 52 00 06 0A 23 45 67 F7 | off: REPLY dev 0 ref 0F sec 00 item 06 data 0A 23 45 67 | on: rejected: checksum missing or wrong
    1.295  OUT  F0 47 5E 00 10 00 06 0B 23 45 67 F7
    1.308  IN   F0 47 5E 00 10 4F 00 06 F7 | off: OK dev 0 ref 10 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.321  IN   F0 47 5E 00 10 52 00 06 0B 23 45 67 F7 | off: REPLY dev 0 ref 10 sec 00 item 06 data 0B 23 45 67 | on: rejected: checksum missing or wrong
    1.340  OUT  F0 47 5E 00 11 00 06 0C 23 45 67 F7
    1.353  IN   F0 47 5E 00 11 4F 00 06 F7 | off: OK dev 0 ref 11 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.366  IN   F0 47 5E 00 11 52 00 06 0C 23 45 67 F7 | off: REPLY dev 0 ref 11 sec 00 item 06 data 0C 23 45 67 | on: rejected: checksum missing or wrong
    1.386  OUT  F0 47 5E 00 12 00 06 0D 23 45 67 F7
    1.398  IN   F0 47 5E 00 12 4F 00 06 F7 | off: OK dev 0 ref 12 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.409  IN   F0 47 5E 00 12 52 00 06 0D 23 45 67 F7 | off: REPLY dev 0 ref 12 sec 00 item 06 data 0D 23 45 67 | on: rejected: checksum missing or wrong
    1.424  OUT  F0 47 5E 00 13 00 06 0E 23 45 67 F7
    1.437  IN   F0 47 5E 00 13 4F 00 06 F7 | off: OK dev 0 ref 13 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.453  IN   F0 47 5E 00 13 52 00 06 0E 23 45 67 F7 | off: REPLY dev 0 ref 13 sec 00 item 06 data 0E 23 45 67 | on: rejected: checksum missing or wrong
    1.462  OUT  F0 47 5E 00 14 00 06 0F 23 45 67 F7
    1.479  IN   F0 47 5E 00 14 4F 00 06 F7 | off: OK dev 0 ref 14 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.488  IN   F0 47 5E 00 14 52 00 06 0F 23 45 67 F7 | off: REPLY dev 0 ref 14 sec 00 item 06 data 0F 23 45 67 | on: rejected: checksum missing or wrong
    1.499  OUT  F0 47 5E 00 15 00 06 10 23 45 67 F7
    1.513  IN   F0 47 5E 00 15 4F 00 06 F7 | off: OK dev 0 ref 15 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.526  IN   F0 47 5E 00 15 52 00 06 10 23 45 67 F7 | off: REPLY dev 0 ref 15 sec 00 item 06 data 10 23 45 67 | on: rejected: checksum missing or wrong
    1.537  OUT  F0 47 5E 00 16 00 06 11 23 45 67 F7
    1.548  IN   F0 47 5E 00 16 4F 00 06 F7 | off: OK dev 0 ref 16 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.561  IN   F0 47 5E 00 16 52 00 06 11 23 45 67 F7 | off: REPLY dev 0 ref 16 sec 00 item 06 data 11 23 45 67 | on: REPLY dev 0 ref 16 sec 00 item 06 data 11 23 45
    1.578  OUT  F0 47 5E 00 17 00 06 12 23 45 67 F7
    1.591  IN   F0 47 5E 00 17 4F 00 06 F7 | off: OK dev 0 ref 17 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.603  IN   F0 47 5E 00 17 52 00 06 12 23 45 67 F7 | off: REPLY dev 0 ref 17 sec 00 item 06 data 12 23 45 67 | on: rejected: checksum missing or wrong
    1.621  OUT  F0 47 5E 00 18 00 06 13 23 45 67 F7
    1.634  IN   F0 47 5E 00 18 4F 00 06 F7 | off: OK dev 0 ref 18 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.647  IN   F0 47 5E 00 18 52 00 06 13 23 45 67 F7 | off: REPLY dev 0 ref 18 sec 00 item 06 data 13 23 45 67 | on: rejected: checksum missing or wrong
    1.661  OUT  F0 47 5E 00 19 00 06 14 23 45 67 F7
    1.674  IN   F0 47 5E 00 19 4F 00 06 F7 | off: OK dev 0 ref 19 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.686  IN   F0 47 5E 00 19 52 00 06 14 23 45 67 F7 | off: REPLY dev 0 ref 19 sec 00 item 06 data 14 23 45 67 | on: rejected: checksum missing or wrong
    1.703  OUT  F0 47 5E 00 1A 00 06 15 23 45 67 F7
    1.716  IN   F0 47 5E 00 1A 4F 00 06 F7 | off: OK dev 0 ref 1A sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.730  IN   F0 47 5E 00 1A 52 00 06 15 23 45 67 F7 | off: REPLY dev 0 ref 1A sec 00 item 06 data 15 23 45 67 | on: rejected: checksum missing or wrong
    1.746  OUT  F0 47 5E 00 1B 00 06 16 23 45 67 F7
    1.759  IN   F0 47 5E 00 1B 4F 00 06 F7 | off: OK dev 0 ref 1B sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.772  IN   F0 47 5E 00 1B 52 00 06 16 23 45 67 F7 | off: REPLY dev 0 ref 1B sec 00 item 06 data 16 23 45 67 | on: rejected: checksum missing or wrong
    1.786  OUT  F0 47 5E 00 1C 00 06 17 23 45 67 F7
    1.799  IN   F0 47 5E 00 1C 4F 00 06 F7 | off: OK dev 0 ref 1C sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.812  IN   F0 47 5E 00 1C 52 00 06 17 23 45 67 F7 | off: REPLY dev 0 ref 1C sec 00 item 06 data 17 23 45 67 | on: rejected: checksum missing or wrong
    1.828  OUT  F0 47 5E 00 1D 00 06 18 23 45 67 F7
    1.841  IN   F0 47 5E 00 1D 4F 00 06 F7 | off: OK dev 0 ref 1D sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.855  IN   F0 47 5E 00 1D 52 00 06 18 23 45 67 F7 | off: REPLY dev 0 ref 1D sec 00 item 06 data 18 23 45 67 | on: rejected: checksum missing or wrong
    1.872  OUT  F0 47 5E 00 1E 00 06 19 23 45 67 F7
    1.885  IN   F0 47 5E 00 1E 4F 00 06 F7 | off: OK dev 0 ref 1E sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.897  IN   F0 47 5E 00 1E 52 00 06 19 23 45 67 F7 | off: REPLY dev 0 ref 1E sec 00 item 06 data 19 23 45 67 | on: rejected: checksum missing or wrong
    1.914  OUT  F0 47 5E 00 1F 00 06 1A 23 45 67 F7
    1.927  IN   F0 47 5E 00 1F 4F 00 06 F7 | off: OK dev 0 ref 1F sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.940  IN   F0 47 5E 00 1F 52 00 06 1A 23 45 67 F7 | off: REPLY dev 0 ref 1F sec 00 item 06 data 1A 23 45 67 | on: rejected: checksum missing or wrong
    1.955  OUT  F0 47 5E 00 20 00 06 1B 23 45 67 F7
    1.968  IN   F0 47 5E 00 20 4F 00 06 F7 | off: OK dev 0 ref 20 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.981  IN   F0 47 5E 00 20 52 00 06 1B 23 45 67 F7 | off: REPLY dev 0 ref 20 sec 00 item 06 data 1B 23 45 67 | on: rejected: checksum missing or wrong
    1.997  OUT  F0 47 5E 00 21 00 06 1C 23 45 67 F7
    2.010  IN   F0 47 5E 00 21 4F 00 06 F7 | off: OK dev 0 ref 21 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.022  IN   F0 47 5E 00 21 52 00 06 1C 23 45 67 F7 | off: REPLY dev 0 ref 21 sec 00 item 06 data 1C 23 45 67 | on: rejected: checksum missing or wrong
    2.039  OUT  F0 47 5E 00 22 00 06 1D 23 45 67 F7
    2.053  IN   F0 47 5E 00 22 4F 00 06 F7 | off: OK dev 0 ref 22 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.064  IN   F0 47 5E 00 22 52 00 06 1D 23 45 67 F7 | off: REPLY dev 0 ref 22 sec 00 item 06 data 1D 23 45 67 | on: rejected: checksum missing or wrong
    2.080  OUT  F0 47 5E 00 23 00 06 1E 23 45 67 F7
    2.093  IN   F0 47 5E 00 23 4F 00 06 F7 | off: OK dev 0 ref 23 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.106  IN   F0 47 5E 00 23 52 00 06 1E 23 45 67 F7 | off: REPLY dev 0 ref 23 sec 00 item 06 data 1E 23 45 67 | on: rejected: checksum missing or wrong
    2.122  OUT  F0 47 5E 00 24 00 06 1F 23 45 67 F7
    2.135  IN   F0 47 5E 00 24 4F 00 06 F7 | off: OK dev 0 ref 24 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.147  IN   F0 47 5E 00 24 52 00 06 1F 23 45 67 F7 | off: REPLY dev 0 ref 24 sec 00 item 06 data 1F 23 45 67 | on: rejected: checksum missing or wrong
    2.161  OUT  F0 47 5E 00 25 00 06 20 23 45 67 F7
    2.174  IN   F0 47 5E 00 25 4F 00 06 F7 | off: OK dev 0 ref 25 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.186  IN   F0 47 5E 00 25 52 00 06 20 23 45 67 F7 | off: REPLY dev 0 ref 25 sec 00 item 06 data 20 23 45 67 | on: rejected: checksum missing or wrong
    2.200  OUT  F0 47 5E 00 26 00 06 21 23 45 67 F7
    2.212  IN   F0 47 5E 00 26 4F 00 06 F7 | off: OK dev 0 ref 26 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.225  IN   F0 47 5E 00 26 52 00 06 21 23 45 67 F7 | off: REPLY dev 0 ref 26 sec 00 item 06 data 21 23 45 67 | on: rejected: checksum missing or wrong
    2.240  OUT  F0 47 5E 00 27 00 06 22 23 45 67 F7
    2.252  IN   F0 47 5E 00 27 4F 00 06 F7 | off: OK dev 0 ref 27 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.263  IN   F0 47 5E 00 27 52 00 06 22 23 45 67 F7 | off: REPLY dev 0 ref 27 sec 00 item 06 data 22 23 45 67 | on: rejected: checksum missing or wrong
    2.279  OUT  F0 47 5E 00 28 00 06 23 23 45 67 F7
    2.292  IN   F0 47 5E 00 28 4F 00 06 F7 | off: OK dev 0 ref 28 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.304  IN   F0 47 5E 00 28 52 00 06 23 23 45 67 F7 | off: REPLY dev 0 ref 28 sec 00 item 06 data 23 23 45 67 | on: rejected: checksum missing or wrong
    2.320  OUT  F0 47 5E 00 29 00 06 24 23 45 67 F7
    2.333  IN   F0 47 5E 00 29 4F 00 06 F7 | off: OK dev 0 ref 29 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.347  IN   F0 47 5E 00 29 52 00 06 24 23 45 67 F7 | off: REPLY dev 0 ref 29 sec 00 item 06 data 24 23 45 67 | on: rejected: checksum missing or wrong
    2.361  OUT  F0 47 5E 00 2A 00 06 25 23 45 67 F7
    2.374  IN   F0 47 5E 00 2A 4F 00 06 F7 | off: OK dev 0 ref 2A sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.387  IN   F0 47 5E 00 2A 52 00 06 25 23 45 67 F7 | off: REPLY dev 0 ref 2A sec 00 item 06 data 25 23 45 67 | on: rejected: checksum missing or wrong
    2.402  OUT  F0 47 5E 00 2B 00 06 26 23 45 67 F7
    2.415  IN   F0 47 5E 00 2B 4F 00 06 F7 | off: OK dev 0 ref 2B sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.428  IN   F0 47 5E 00 2B 52 00 06 26 23 45 67 F7 | off: REPLY dev 0 ref 2B sec 00 item 06 data 26 23 45 67 | on: rejected: checksum missing or wrong
    2.446  OUT  F0 47 5E 00 2C 00 06 27 23 45 67 F7
    2.459  IN   F0 47 5E 00 2C 4F 00 06 F7 | off: OK dev 0 ref 2C sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.468  IN   F0 47 5E 00 2C 52 00 06 27 23 45 67 F7 | off: REPLY dev 0 ref 2C sec 00 item 06 data 27 23 45 67 | on: rejected: checksum missing or wrong
    2.483  OUT  F0 47 5E 00 2D 00 06 28 23 45 67 F7
    2.496  IN   F0 47 5E 00 2D 4F 00 06 F7 | off: OK dev 0 ref 2D sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.510  IN   F0 47 5E 00 2D 52 00 06 28 23 45 67 F7 | off: REPLY dev 0 ref 2D sec 00 item 06 data 28 23 45 67 | on: rejected: checksum missing or wrong
    2.527  OUT  F0 47 5E 00 2E 00 06 29 23 45 67 F7
    2.540  IN   F0 47 5E 00 2E 4F 00 06 F7 | off: OK dev 0 ref 2E sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.552  IN   F0 47 5E 00 2E 52 00 06 29 23 45 67 F7 | off: REPLY dev 0 ref 2E sec 00 item 06 data 29 23 45 67 | on: rejected: checksum missing or wrong
    2.568  OUT  F0 47 5E 00 2F 00 06 2A 23 45 67 F7
    2.581  IN   F0 47 5E 00 2F 4F 00 06 F7 | off: OK dev 0 ref 2F sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.593  IN   F0 47 5E 00 2F 52 00 06 2A 23 45 67 F7 | off: REPLY dev 0 ref 2F sec 00 item 06 data 2A 23 45 67 | on: rejected: checksum missing or wrong
    2.609  OUT  F0 47 5E 00 30 00 06 2B 23 45 67 F7
    2.622  IN   F0 47 5E 00 30 4F 00 06 F7 | off: OK dev 0 ref 30 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.634  IN   F0 47 5E 00 30 52 00 06 2B 23 45 67 F7 | off: REPLY dev 0 ref 30 sec 00 item 06 data 2B 23 45 67 | on: rejected: checksum missing or wrong
    2.651  OUT  F0 47 5E 00 31 00 06 2C 23 45 67 F7
    2.663  IN   F0 47 5E 00 31 4F 00 06 F7 | off: OK dev 0 ref 31 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.675  IN   F0 47 5E 00 31 52 00 06 2C 23 45 67 F7 | off: REPLY dev 0 ref 31 sec 00 item 06 data 2C 23 45 67 | on: rejected: checksum missing or wrong
    2.696  OUT  F0 47 5E 00 32 00 06 2D 23 45 67 F7
    2.709  IN   F0 47 5E 00 32 4F 00 06 F7 | off: OK dev 0 ref 32 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.722  IN   F0 47 5E 00 32 52 00 06 2D 23 45 67 F7 | off: REPLY dev 0 ref 32 sec 00 item 06 data 2D 23 45 67 | on: rejected: checksum missing or wrong
    2.737  OUT  F0 47 5E 00 33 00 06 2E 23 45 67 F7
    2.749  IN   F0 47 5E 00 33 4F 00 06 F7 | off: OK dev 0 ref 33 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.761  IN   F0 47 5E 00 33 52 00 06 2E 23 45 67 F7 | off: REPLY dev 0 ref 33 sec 00 item 06 data 2E 23 45 67 | on: rejected: checksum missing or wrong
    2.777  OUT  F0 47 5E 00 34 00 06 2F 23 45 67 F7
    2.789  IN   F0 47 5E 00 34 4F 00 06 F7 | off: OK dev 0 ref 34 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.799  IN   F0 47 5E 00 34 52 00 06 2F 23 45 67 F7 | off: REPLY dev 0 ref 34 sec 00 item 06 data 2F 23 45 67 | on: rejected: checksum missing or wrong
    2.816  OUT  F0 47 5E 00 35 00 06 30 23 45 67 F7
    2.829  IN   F0 47 5E 00 35 4F 00 06 F7 | off: OK dev 0 ref 35 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.840  IN   F0 47 5E 00 35 52 00 06 30 23 45 67 F7 | off: REPLY dev 0 ref 35 sec 00 item 06 data 30 23 45 67 | on: rejected: checksum missing or wrong
    2.855  OUT  F0 47 5E 00 36 00 06 31 23 45 67 F7
    2.867  IN   F0 47 5E 00 36 4F 00 06 F7 | off: OK dev 0 ref 36 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.877  IN   F0 47 5E 00 36 52 00 06 31 23 45 67 F7 | off: REPLY dev 0 ref 36 sec 00 item 06 data 31 23 45 67 | on: rejected: checksum missing or wrong
#   50 of 50 round trips succeeded; min 34 ms, median 40 ms, max 42 ms
# step 6: Checksums on (section 00, item 04)
    2.901  OUT  F0 47 5E 00 37 00 04 01 3C F7
    2.912  IN   F0 47 5E 00 37 4F 00 04 F7 | off: OK dev 0 ref 37 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    2.924  IN   F0 47 5E 00 37 44 00 04 7F F7 | off: DONE dev 0 ref 37 sec 00 item 04 data 7F | on: DONE dev 0 ref 37 sec 00 item 04 data -
# diagnostic: checksum mode changed: on
#   DONE after 38 ms
# step 7: Echo with checksums on: the REPLY carries a checksum
    2.949  OUT  F0 47 5E 00 38 00 06 01 23 45 67 0E F7
    2.965  IN   F0 47 5E 00 38 4F 00 06 0D F7 | off: OK dev 0 ref 38 sec 00 item 06 data 0D | on: OK dev 0 ref 38 sec 00 item 06 data -
    2.976  IN   F0 47 5E 00 38 52 00 06 01 23 45 67 60 F7 | off: REPLY dev 0 ref 38 sec 00 item 06 data 01 23 45 67 60 | on: REPLY dev 0 ref 38 sec 00 item 06 data 01 23 45 67
#   REPLY 01 23 45 67 as sent after 46 ms
# step 8: Operating system version with checksums on
    3.005  OUT  F0 47 5E 00 39 02 00 3B F7
    3.017  IN   F0 47 5E 00 39 4F 02 00 0A F7 | off: OK dev 0 ref 39 sec 02 item 00 data 0A | on: OK dev 0 ref 39 sec 02 item 00 data -
    3.030  IN   F0 47 5E 00 39 52 02 00 02 0E 1D F7 | off: REPLY dev 0 ref 39 sec 02 item 00 data 02 0E 1D | on: REPLY dev 0 ref 39 sec 02 item 00 data 02 0E
    3.045  OUT  F0 47 5E 00 3A 02 01 3D F7
    3.058  IN   F0 47 5E 00 3A 4F 02 01 0C F7 | off: OK dev 0 ref 3A sec 02 item 01 data 0C | on: OK dev 0 ref 3A sec 02 item 01 data -
    3.070  IN   F0 47 5E 00 3A 52 02 01 00 0F F7 | off: REPLY dev 0 ref 3A sec 02 item 01 data 00 0F | on: REPLY dev 0 ref 3A sec 02 item 01 data 00
#   OS 2.14, sub-version 0 after 78 ms
# step 9: Checksums off again
    3.092  OUT  F0 47 5E 00 3B 00 04 00 3F F7
    3.104  IN   F0 47 5E 00 3B 4F 00 04 0E F7 | off: OK dev 0 ref 3B sec 00 item 04 data 0E | on: OK dev 0 ref 3B sec 00 item 04 data -
    3.116  IN   F0 47 5E 00 3B 44 00 04 F7 | off: DONE dev 0 ref 3B sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
#   DONE after 39 ms
# step 10: Echo with checksums off again
    3.143  OUT  F0 47 5E 00 3C 00 06 01 23 45 67 F7
    3.156  IN   F0 47 5E 00 3C 4F 00 06 F7 | off: OK dev 0 ref 3C sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.168  IN   F0 47 5E 00 3C 52 00 06 01 23 45 67 F7 | off: REPLY dev 0 ref 3C sec 00 item 06 data 01 23 45 67 | on: rejected: checksum missing or wrong
#   REPLY 01 23 45 67 as sent after 38 ms
# step 11: Notification off (section 00, item 01)
    3.191  OUT  F0 47 5E 00 3D 00 01 00 F7
    3.202  IN   F0 47 5E 00 3D 4F 00 01 F7 | off: OK dev 0 ref 3D sec 00 item 01 data - | on: rejected: checksum missing or wrong
    3.215  IN   F0 47 5E 00 3D 44 00 01 F7 | off: DONE dev 0 ref 3D sec 00 item 01 data - | on: rejected: checksum missing or wrong
#   DONE after 37 ms
# step 12: Echo with Notification off: only the REPLY is expected, no OK
    3.241  OUT  F0 47 5E 00 3E 00 06 01 23 45 67 F7
    3.256  IN   F0 47 5E 00 3E 52 00 06 01 23 45 67 F7 | off: REPLY dev 0 ref 3E sec 00 item 06 data 01 23 45 67 | on: rejected: checksum missing or wrong
#   REPLY 01 23 45 67 as sent after 27 ms
# step 13: Notification on
    3.277  OUT  F0 47 5E 00 3F 00 01 01 F7
    3.289  IN   F0 47 5E 00 3F 44 00 01 F7 | off: DONE dev 0 ref 3F sec 00 item 01 data - | on: rejected: checksum missing or wrong
#   DONE after 24 ms
# step 14: Sync LCD off (section 00, item 03)
    3.314  OUT  F0 47 5E 00 40 00 03 00 F7
    3.324  IN   F0 47 5E 00 40 4F 00 03 F7 | off: OK dev 0 ref 40 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    3.336  IN   F0 47 5E 00 40 44 00 03 F7 | off: DONE dev 0 ref 40 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   DONE after 34 ms
# step 15: Sync LCD on
    3.353  OUT  F0 47 5E 00 41 00 03 01 F7
    3.363  IN   F0 47 5E 00 41 4F 00 03 F7 | off: OK dev 0 ref 41 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    3.374  IN   F0 47 5E 00 41 44 00 03 F7 | off: DONE dev 0 ref 41 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   DONE after 34 ms
# step 16: Auto screen update on (section 00, item 05)
    3.395  OUT  F0 47 5E 00 42 00 05 01 F7
    3.406  IN   F0 47 5E 00 42 4F 00 05 F7 | off: OK dev 0 ref 42 sec 00 item 05 data - | on: rejected: checksum missing or wrong
    3.419  IN   F0 47 5E 00 42 44 00 05 F7 | off: DONE dev 0 ref 42 sec 00 item 05 data - | on: rejected: checksum missing or wrong
#   DONE after 37 ms
# step 17: Auto screen update off
    3.440  OUT  F0 47 5E 00 43 00 05 00 F7
    3.451  IN   F0 47 5E 00 43 4F 00 05 F7 | off: OK dev 0 ref 43 sec 00 item 05 data - | on: rejected: checksum missing or wrong
    3.464  IN   F0 47 5E 00 43 44 00 05 F7 | off: DONE dev 0 ref 43 sec 00 item 05 data - | on: rejected: checksum missing or wrong
#   DONE after 36 ms
# step 18: Still Alive on (section 00, item 07)
    3.483  OUT  F0 47 5E 00 44 00 07 01 F7
    3.494  IN   F0 47 5E 00 44 4F 00 07 F7 | off: OK dev 0 ref 44 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    3.507  IN   F0 47 5E 00 44 44 00 07 F7 | off: DONE dev 0 ref 44 sec 00 item 07 data - | on: rejected: checksum missing or wrong
#   DONE after 36 ms
# step 19: Still Alive off
    3.525  OUT  F0 47 5E 00 45 00 07 00 F7
    3.537  IN   F0 47 5E 00 45 4F 00 07 F7 | off: OK dev 0 ref 45 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    3.548  IN   F0 47 5E 00 45 44 00 07 F7 | off: DONE dev 0 ref 45 sec 00 item 07 data - | on: rejected: checksum missing or wrong
#   DONE after 35 ms
# closing: leave the sampler in a known state, whatever happened above
# step 20: Closing: checksums off, sent with a checksum whatever the state
    3.578  OUT  F0 47 5E 00 46 00 04 00 4A F7
    3.589  IN   F0 47 5E 00 46 4F 00 04 F7 | off: OK dev 0 ref 46 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    3.599  IN   F0 47 5E 00 46 44 00 04 F7 | off: DONE dev 0 ref 46 sec 00 item 04 data - | on: rejected: checksum missing or wrong
#   DONE after 31 ms
# step 21: Closing: Still Alive off
    3.616  OUT  F0 47 5E 00 47 00 07 00 F7
    3.626  IN   F0 47 5E 00 47 4F 00 07 F7 | off: OK dev 0 ref 47 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    3.638  IN   F0 47 5E 00 47 44 00 07 F7 | off: DONE dev 0 ref 47 sec 00 item 07 data - | on: rejected: checksum missing or wrong
#   DONE after 32 ms
# step 22: Closing: Notification on
    3.655  OUT  F0 47 5E 00 48 00 01 01 F7
    3.666  IN   F0 47 5E 00 48 4F 00 01 F7 | off: OK dev 0 ref 48 sec 00 item 01 data - | on: rejected: checksum missing or wrong
    3.678  IN   F0 47 5E 00 48 44 00 01 F7 | off: DONE dev 0 ref 48 sec 00 item 01 data - | on: rejected: checksum missing or wrong
#   DONE after 33 ms
# step 23: Closing: Sync LCD on
    3.694  OUT  F0 47 5E 00 49 00 03 01 F7
    3.705  IN   F0 47 5E 00 49 4F 00 03 F7 | off: OK dev 0 ref 49 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    3.717  IN   F0 47 5E 00 49 44 00 03 F7 | off: DONE dev 0 ref 49 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   DONE after 33 ms
# step 24: Closing: Auto screen update off
    3.736  OUT  F0 47 5E 00 4A 00 05 00 F7
    3.746  IN   F0 47 5E 00 4A 4F 00 05 F7 | off: OK dev 0 ref 4A sec 00 item 05 data - | on: rejected: checksum missing or wrong
    3.758  IN   F0 47 5E 00 4A 44 00 05 F7 | off: DONE dev 0 ref 4A sec 00 item 05 data - | on: rejected: checksum missing or wrong
#   DONE after 31 ms
# observations
# observation: discovery answered by DeviceIDs: 0
# observation: OS version 2.14 (sub-version 0)
# observation: 50 Echo round trips: min 34 ms, median 40 ms, max 42 ms
# observation: F0 F7 messages seen: 0
# observation: messages rejected by the session: 0; unsolicited confirmations: 0; late ERRORs after a REPLY: 0
# observation: checksum mode as the session followed it: unknown -> off -> on -> off
# observation: steps that did not go as they had to: none
# observation: sampler left in the known state: checksums off, Still Alive off, Notification on, Sync LCD on, Auto screen update off
# observation: frames sent 75, received 148
```
