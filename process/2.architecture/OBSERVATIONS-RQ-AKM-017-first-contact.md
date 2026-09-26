# Observations of the first contact with an S5000 (RQ-AKM-017, TASK-AKM-012)

The owner ran `xs56k_akm_probe` against the real sampler on 2026-09-26. This file keeps the log verbatim
and what it shows, and lists the artifacts that were amended because of it. It is an observation record,
not an artifact with an ID: the process index ignores it.

## Setup

- Sampler: AKAI S5000, **OS 2.14, sub-version 0** (steps 4 and 5). The specification the project works from is
  version 2.10: the sampler is newer than its documentation.
- Path: Windows, `JuceMidiBackend`, an ESI M8U eX USB MIDI interface; input `MIDIIN2 (ESI M8U eX)`, output
  `MIDIOUT15 (ESI M8U eX)`.
- The sampler's DeviceID turned out to be 0 (F2). The probe was run with `--device-id 0`.

## What the log shows

| # | Observation | Evidence | Consequence |
|---|---|---|---|
| F1 | 15 frames sent, 30 received, complete and in order, none lost; the probe ended with checksums and Still Alive off. | whole log | The transport works through the JUCE backend and a USB interface. |
| F2 | **A confirmation carries the sampler's own DeviceID, not the command's.** A Query addressed to DeviceID 5 was answered by a sampler whose DeviceID is 0, and the confirmations say DeviceID 0. A sampler with DeviceID 0 answers a message for any DeviceID (spec p. 3). | step 3 | Closes the open question of RQ-AKM-007: the DeviceID expected in a confirmation is the bound target (RQ-AKM-039 binds only a target equal to the sampler's own DeviceID). The simulated sampler's default (`Own`) is right. |
| F3 | Every command is answered by an OK, then a DONE, REPLY or ERROR, 3 to 4 ms apart; every confirmation, the OK included, echoes the user-ref, section and item. Notifications were on when the probe started. | steps 1 to 15 | Matches RQ-AKM-009. |
| F4 | Latency: OK 6 to 8 ms after the command is sent, DONE or REPLY 9 to 12 ms after. Longest 12 ms. A 10-byte frame takes about 3.2 ms at 31.25 kbaud. Simple commands only: no slow operation was tried. | every step | Basis of the provisional timeout and window (ADR-AKM-001, DEC-AKM-006). |
| F5 | Checksums were off when the probe started: the confirmations of a command sent with a checksum carry none. | step 1 | The spec's default is confirmed. |
| F6 | A checksum appended to a command while checksums are off is ignored; the Echo is answered normally. | step 7 | DEC-AKM-009's choice to append a checksum in mode unknown is safe. |
| F7 | The checksum covers the bytes from the first user-ref to the last data byte, **the Reply ID included**, low 7 bits. Example: the DONE of ref 17, 17 + 44 + 00 + 04 = 5F. | steps 8 to 12 | Confirms the assumption of TASK-AKM-004. |
| F8 | With checksums on, every confirmation carries one: OK, DONE, REPLY and ERROR. | steps 9, 10, 11 | Confirms RQ-AKM-003. |
| F9 | **Around a checksum-mode command, the OK follows the mode in force when the command arrived, the DONE the mode after it ran.** Switching on: OK without, DONE with a checksum. Switching off: OK with, DONE without. | steps 8 and 12 | The confirmations of such a command must be decoded in mode unknown (RQ-AKM-013, DEC-AKM-007, DEC-AKM-009). The simulated sampler's default was the opposite; corrected. |
| F10 | A command without a checksum while they are on gets an **OK first, then ERROR 129** (data `01 01`), both with a checksum and with the section and item echoed: the sampler read the last byte before F7 as the checksum. | step 11 | The simulated sampler sent the ERROR alone; corrected. |
| F11 | The spec's "checksums off on all samplers" frame (sent with a valid checksum) works whether checksums are on or off. | steps 1 and 12 | Confirms RQ-AKM-013's rationale. |
| F12 | Get OS version answers `02 0E`, Get sub-version `00`. Still Alive (§00/&07) is accepted by OS 2.14: DONE on and off. | steps 4, 5, 14, 15 | RQ-AKM-044 is workable as written; the two OS-dependent items exist on this sampler. |
| F13 | No `F0 F7` arrived: none was provoked, no slow command was sent. | steps 14, 15 | Whether the JUCE backend delivers `F0 F7` stays open (TASK-AKM-010). |

## Not observed

`F0 F7` delivery; latency over repeated runs (RQ-AKM-017 asks for 50 Echo round trips) and of slow
operations; the defaults of Sync LCD, Auto screen update and Still Alive when the sampler is fresh; whether §00
settings survive a power cycle; the second port (B); behaviour of the other sections. All of these are for
TASK-AKM-010.

## Amendments made because of it

- `FTR-AKM-001`: RQ-AKM-007 (matching by DeviceID), RQ-AKM-013 (decoding the confirmations of the checksum
  command), RQ-AKM-017 (status), the open points on DeviceID matching, default state and OS version.
- `ADR-AKM-001`: DEC-AKM-006 (measured latencies, provisional defaults), DEC-AKM-007 (step 5, the reason for the
  checksum default), DEC-AKM-009 (what was observed).
- Simulated sampler: the DONE of the checksum command follows the new mode by default; a frame with a wrong
  checksum gets an OK before the ERROR. Seven tests replay the captured frames through the codec
  (`juce/tests/akm/CapturedFramesTests.cpp`); the codec needed no change.
- `Confirmation.hpp`: the comment on the checksum's coverage now cites the observation.

## The log, verbatim

```text
# XS56K AKM first-contact probe
# started 2026-09-26T19:14:22Z
# target: in="MIDIIN2 (ESI M8U eX)" out="MIDIOUT15 (ESI M8U eX)" device-id=0
# The probe switches checksums and Still Alive on and off, and ends with both off.
# step 1: Checksums off on every sampler, the spec's own frame, to start from a known state
    0.009  OUT  F0 47 5E 00 10 00 04 00 14 F7
    0.017  IN   F0 47 5E 00 10 4F 00 04 F7 | off: OK dev 0 ref 10 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    0.020  IN   F0 47 5E 00 10 44 00 04 F7 | off: DONE dev 0 ref 10 sec 00 item 04 data - | on: rejected: checksum missing or wrong
#   answered after 7 ms, DONE after 10 ms
# step 2: Query to DeviceID 0: every sampler answers, each with a DeviceID
    0.565  OUT  F0 47 5E 00 11 00 00 F7
    0.572  IN   F0 47 5E 00 11 4F 00 00 F7 | off: OK dev 0 ref 11 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    0.575  IN   F0 47 5E 00 11 44 00 00 F7 | off: DONE dev 0 ref 11 sec 00 item 00 data - | on: rejected: checksum missing or wrong
#   answered after 6 ms, DONE after 9 ms
# step 3: Query to DeviceID 5: only a sampler with that DeviceID, or DeviceID 0, answers
    1.114  OUT  F0 47 5E 05 12 00 00 F7
    1.121  IN   F0 47 5E 00 12 4F 00 00 F7 | off: OK dev 0 ref 12 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    1.124  IN   F0 47 5E 00 12 44 00 00 F7 | off: DONE dev 0 ref 12 sec 00 item 00 data - | on: rejected: checksum missing or wrong
#   answered after 6 ms, DONE after 9 ms
# step 4: Get operating system version (section 02, item 00)
    1.663  OUT  F0 47 5E 00 13 02 00 F7
    1.670  IN   F0 47 5E 00 13 4F 02 00 F7 | off: OK dev 0 ref 13 sec 02 item 00 data - | on: rejected: checksum missing or wrong
    1.673  IN   F0 47 5E 00 13 52 02 00 02 0E F7 | off: REPLY dev 0 ref 13 sec 02 item 00 data 02 0E | on: rejected: checksum missing or wrong
#   answered after 6 ms, REPLY after 10 ms
# step 5: Get operating system sub-version (section 02, item 01)
    2.214  OUT  F0 47 5E 00 14 02 01 F7
    2.221  IN   F0 47 5E 00 14 4F 02 01 F7 | off: OK dev 0 ref 14 sec 02 item 01 data - | on: rejected: checksum missing or wrong
    2.224  IN   F0 47 5E 00 14 52 02 01 00 F7 | off: REPLY dev 0 ref 14 sec 02 item 01 data 00 | on: rejected: checksum missing or wrong
#   answered after 6 ms, REPLY after 9 ms
# step 6: Echo 01 02 03 04, checksums off
    2.770  OUT  F0 47 5E 00 15 00 06 01 02 03 04 F7
    2.777  IN   F0 47 5E 00 15 4F 00 06 F7 | off: OK dev 0 ref 15 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.782  IN   F0 47 5E 00 15 52 00 06 01 02 03 04 F7 | off: REPLY dev 0 ref 15 sec 00 item 06 data 01 02 03 04 | on: rejected: checksum missing or wrong
#   answered after 7 ms, REPLY after 11 ms
# step 7: Echo with a checksum the sampler does not expect: ignored while checksums are off
    3.350  OUT  F0 47 5E 00 16 00 06 01 02 03 04 26 F7
    3.358  IN   F0 47 5E 00 16 4F 00 06 F7 | off: OK dev 0 ref 16 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.362  IN   F0 47 5E 00 16 52 00 06 01 02 03 04 F7 | off: REPLY dev 0 ref 16 sec 00 item 06 data 01 02 03 04 | on: rejected: checksum missing or wrong
#   answered after 8 ms, REPLY after 12 ms
# step 8: Checksums on (section 00, item 04), sent without checksum
    3.918  OUT  F0 47 5E 00 17 00 04 01 F7
    3.925  IN   F0 47 5E 00 17 4F 00 04 F7 | off: OK dev 0 ref 17 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    3.928  IN   F0 47 5E 00 17 44 00 04 5F F7 | off: DONE dev 0 ref 17 sec 00 item 04 data 5F | on: DONE dev 0 ref 17 sec 00 item 04 data -
#   answered after 6 ms, DONE after 10 ms
# step 9: Query with checksum, checksums on
    4.474  OUT  F0 47 5E 00 18 00 00 18 F7
    4.482  IN   F0 47 5E 00 18 4F 00 00 67 F7 | off: OK dev 0 ref 18 sec 00 item 00 data 67 | on: OK dev 0 ref 18 sec 00 item 00 data -
    4.485  IN   F0 47 5E 00 18 44 00 00 5C F7 | off: DONE dev 0 ref 18 sec 00 item 00 data 5C | on: DONE dev 0 ref 18 sec 00 item 00 data -
#   answered after 7 ms, DONE after 10 ms
# step 10: Echo with checksum, checksums on
    5.042  OUT  F0 47 5E 00 19 00 06 01 02 03 04 29 F7
    5.051  IN   F0 47 5E 00 19 4F 00 06 6E F7 | off: OK dev 0 ref 19 sec 00 item 06 data 6E | on: OK dev 0 ref 19 sec 00 item 06 data -
    5.055  IN   F0 47 5E 00 19 52 00 06 01 02 03 04 7B F7 | off: REPLY dev 0 ref 19 sec 00 item 06 data 01 02 03 04 7B | on: REPLY dev 0 ref 19 sec 00 item 06 data 01 02 03 04
#   answered after 8 ms, REPLY after 12 ms
# step 11: Query without checksum while checksums are on: expect ERROR 129
    5.599  OUT  F0 47 5E 00 1A 00 00 F7
    5.606  IN   F0 47 5E 00 1A 4F 00 00 69 F7 | off: OK dev 0 ref 1A sec 00 item 00 data 69 | on: OK dev 0 ref 1A sec 00 item 00 data -
    5.610  IN   F0 47 5E 00 1A 45 00 00 01 01 61 F7 | off: ERROR 129 (checksum invalid) dev 0 ref 1A sec 00 item 00 data 01 01 61 | on: ERROR 129 (checksum invalid) dev 0 ref 1A sec 00 item 00 data 01 01
#   answered after 6 ms, ERROR after 10 ms
# step 12: Checksums off on every sampler, sent with a checksum: works whichever the state
    6.164  OUT  F0 47 5E 00 1B 00 04 00 1F F7
    6.171  IN   F0 47 5E 00 1B 4F 00 04 6E F7 | off: OK dev 0 ref 1B sec 00 item 04 data 6E | on: OK dev 0 ref 1B sec 00 item 04 data -
    6.174  IN   F0 47 5E 00 1B 44 00 04 F7 | off: DONE dev 0 ref 1B sec 00 item 04 data - | on: rejected: checksum missing or wrong
#   answered after 7 ms, DONE after 10 ms
# step 13: Query without checksum, checksums off again
    6.749  OUT  F0 47 5E 00 1C 00 00 F7
    6.755  IN   F0 47 5E 00 1C 4F 00 00 F7 | off: OK dev 0 ref 1C sec 00 item 00 data - | on: rejected: checksum missing or wrong
    6.758  IN   F0 47 5E 00 1C 44 00 00 F7 | off: DONE dev 0 ref 1C sec 00 item 00 data - | on: rejected: checksum missing or wrong
#   answered after 6 ms, DONE after 9 ms
# step 14: Still Alive on (section 00, item 07)
    7.311  OUT  F0 47 5E 00 1D 00 07 01 F7
    7.318  IN   F0 47 5E 00 1D 4F 00 07 F7 | off: OK dev 0 ref 1D sec 00 item 07 data - | on: rejected: checksum missing or wrong
    7.321  IN   F0 47 5E 00 1D 44 00 07 F7 | off: DONE dev 0 ref 1D sec 00 item 07 data - | on: rejected: checksum missing or wrong
#   answered after 6 ms, DONE after 9 ms
# step 15: Still Alive off
    7.867  OUT  F0 47 5E 00 1E 00 07 00 F7
    7.873  IN   F0 47 5E 00 1E 4F 00 07 F7 | off: OK dev 0 ref 1E sec 00 item 07 data - | on: rejected: checksum missing or wrong
    7.876  IN   F0 47 5E 00 1E 44 00 07 F7 | off: DONE dev 0 ref 1E sec 00 item 07 data - | on: rejected: checksum missing or wrong
#   answered after 6 ms, DONE after 9 ms
# observations
# observation: OS version 2.14 (sub-version 0)
# observation: DeviceIDs carried by confirmations: 0
# observation: F0 F7 messages seen: 0
# observation: longest DONE, REPLY or ERROR latency: 12 ms
# observation: frames sent 15, received 30
# observation: a sampler answered
```
