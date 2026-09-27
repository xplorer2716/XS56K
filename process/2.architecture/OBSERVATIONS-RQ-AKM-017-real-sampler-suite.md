# Observations of the real-sampler suite on the S5000 (RQ-AKM-017, RQ-AKM-018, TASK-AKM-010)

The owner ran `xs56k_akm_probe --suite --in "MIDIIN2 (ESI M8U eX)" --out "MIDIOUT15 (ESI M8U eX)" --power-cycle
--slow-operation` against the real sampler on 2026-09-27. This file keeps the log verbatim and what it shows, and
lists the artifacts amended because of it. It is an observation record, not an artifact with an ID: the process
index ignores it. The first contact and the session smoke test are in `OBSERVATIONS-RQ-AKM-017-first-contact.md`
and `OBSERVATIONS-RQ-AKM-017-session-smoke-test.md`.

## Setup

- The same sampler and path as the earlier runs: AKAI S5000, **OS 2.14, sub-version 0** (read again, check 4),
  Windows, `JuceMidiBackend`, an ESI M8U eX USB MIDI interface, input `MIDIIN2 (ESI M8U eX)`, output
  `MIDIOUT15 (ESI M8U eX)`, DeviceID 0.
- Program: `xs56k_akm_probe --suite --power-cycle --slow-operation`, Debug build (`juce/build/tests/probe/Debug`,
  the tree pushed in `768d605`), every other option at its default (3 s command timeout, 500 ms discovery window,
  50 Echo round trips, the LCD settings switched).
- What ran: `akm::harness::runRealSamplerSuite` (TASK-AKM-010) — nine checks, each opening its own `Session` with
  `Session::open` and closing it with `Session::close` (DEC-AKM-007, DEC-AKM-004); a `GuardedSession` closes the
  session of a check that throws.

## What the log shows

| # | Observation | Evidence | Consequence |
|---|---|---|---|
| F1 | The seven automatic checks all passed: open and close (opened as ready after 537 ms, closed after 31 ms); Echo returns the bytes sent (13 ms); 50 timed Echo round trips (min 12 ms, median 12 ms, 95th percentile 13 ms, max 13 ms); the OS version (2.14 / 0, 20 ms); checksums on and off through one session (on 11 ms, Echo with checksums on 13 ms, OS version with checksums on 22 ms, off again 11 ms, Echo off again 12 ms); a close that puts back five settings at once (checksum mode, Still Alive, notification, Sync LCD, Auto screen update — 44 ms); and a check that throws on purpose with checksums on, whose guard's own close sent checksums off and got its DONE (31 ms). | checks 1–7 | `Session::open`, `Session::close` and the section 00 primitives (TASK-AKM-006, 008, 009, 011) work end to end on real hardware, error recovery (RQ-AKM-018) included. Nothing to correct in their logic. |
| F2 | **The buffered `WireLog` fixes the session smoke test's F10.** The 50 Echo round trips measured here (12–13 ms) match the first contact's simple frames (9–12 ms, unbuffered but no console writes mid-command) and are far below the session smoke test's unbuffered log (34–42 ms). | check 3 | Confirms the diagnosis of TASK-AKM-013's F10: the log's own console writes, not the sampler, had slowed that run's exchanges. The suite's buffered log (flushed between steps, TASK-AKM-013's own follow-up) is not a source of latency. |
| F3 | Discovery answered by DeviceID 0 in every one of the first eight checks; opening took 537 ms, dominated by the 500 ms discovery window, closing 30 to 44 ms. | checks 1–8 | The provisional discovery window (500 ms) and command timeout (2 s) of DEC-AKM-006 are confirmed, with a wide margin (max answer observed: 13 ms). |
| F4 | **"Update the list of disks" (§10/&01) was accepted — an OK after 8 ms — and then answered nothing at all**: no DONE, no ERROR, and **no `F0 F7`**, for the full 3 s command timeout the suite used, although Still Alive was on. | check 8 | Contradicts, for this item on this sampler, the rationale of RQ-AKM-011 ("`&07` makes the sampler send `F0 F7` about every second during long operations"): either this operation does not trigger it, or the sampler was already stuck below its SysEx handler before it could. Either reading leaves the maximum total wait of DEC-AKM-006 unset by this run. |
| F5 | After that timeout, the check's own closing command (checksums off, sent to restore the session) also got **no reply at all**: the checksum mode is reported `unknown`, not put back, and the check fails saying so. | check 8 | The sampler had not merely answered the disk command slowly: it had stopped answering *any* SysEx, the closing command included. |
| F6 | Check 9 (power-cycle) sent one discovery frame and got no answer within the discovery window: it failed as "no sampler at the target DeviceID", exactly as a run against a silent sampler would. **It never reached the point of asking the owner to power-cycle the sampler** — `powerCycle()` only calls `askOwner` after its own `Session::open` succeeds, and that open never completed. The owner independently confirmed seeing no such prompt, and that the sampler was still unresponsive right after the run; power-cycling it by hand (not through the suite) restored it — reloading a program is functional again. | check 9; owner's report | Not a defect in the suite or the probe: closing a session whose open never bound a target correctly sends nothing (there was nothing to put back), which is why check 9 logs no further frame. The suite cannot itself recover a sampler that has stopped answering SysEx; only an actual power cycle could, here done by hand instead of through `--power-cycle`'s own prompt. |
| F7 | Why the disk operation did not finish in the window observed is not established from the SysEx side alone: no removable/SCSI media may have been attached, the sampler may probe such a bus with its own much longer hardware-level timeout, or this may be a firmware wedge specific to an empty disk list. The suite cannot distinguish these. | checks 8, 9 | Open question for whoever specifies FTR-AKM-004 (disk operations): on this evidence, Still Alive is not a safe basis for assuming a long §10 operation stays observable and recoverable through SysEx alone. |
| F8 | 119 frames sent, 227 received over the whole run; the final observation line reads "sampler NOT confirmed in the known state" rather than claiming success. | whole log | The suite reports honestly what it could not confirm, as RQ-AKM-018 asks, rather than assuming the best. |

## Not observed

Whether §00 settings survive a *graceful* power cycle (check 9 never reached it; the recovery power cycle done by
hand after check 8 is not that test, since it followed an already-wedged sampler rather than a session the owner
chose to interrupt); a clean measurement of how long "update the list of disks" actually takes, or whether it
would have answered eventually; the same operation with a disk drive or removable media attached; the second port
(B); an OS older than 2.14; how often an ERROR follows a REPLY (RQ-AKM-006's late-error case). A repeat run with
`--power-cycle` alone (without `--slow-operation`), whenever the owner chooses to, would settle the first of these.

## Amendments made because of it

- `ADR-AKM-001`: DEC-AKM-006 (the timeout and discovery window confirmed at their provisional values; the maximum
  total wait left open, with the reason); DEC-AKM-008 (the `--slow-operation` risk observed); the Status paragraph
  and the Risks paragraph brought up to date.
- `FTR-AKM-001`: RQ-AKM-017 (status); RQ-AKM-018 (status: the guarantee held even where nothing could be restored,
  because nothing had been changed); RQ-AKM-011 (status: the rationale is not confirmed for this operation); the
  timeout/window open point.
- `PLAN-AKM-001`: TASK-AKM-010 set to Done.
- `AGENTS.md`: a caution added to the real-sampler suite entry about `--slow-operation`.

## The log, verbatim

`akm-suite-20260927-083635.log`, unchanged.

```text
# XS56K AKM real-sampler suite
# started 2026-09-27T08:36:38Z
# target: in="MIDIIN2 (ESI M8U eX)" out="MIDIOUT15 (ESI M8U eX)" device-id=0
# Each check opens a session with Session::open and closes it with Session::close. The suite changes only section 00 settings (checksums, Notification, Still Alive, Sync LCD, Auto screen update), never a stored program, multi or sample, and leaves them as the closing does: checksums off, Still Alive off, Notification on, Sync LCD on, Auto screen update off.
# It also sends one command outside sections 00 and 02, asked for with --slow-operation: update the list of disks (section 10, item 01).
# It also asks you to power-cycle the sampler while a session is open (--power-cycle).
# The log is written between the steps, never while a command is in flight, so that writing it cannot delay the exchanges it records: the times of the frames are those of the wire.
# check 1: open a session and close it
    0.149  OUT  F0 47 5E 00 00 00 00 00 F7
    0.156  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    0.159  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    0.654  OUT  F0 47 5E 00 01 00 04 00 05 F7
    0.662  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    0.665  IN   F0 47 5E 00 01 44 00 04 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
    0.665  OUT  F0 47 5E 00 02 00 03 00 F7
    0.672  IN   F0 47 5E 00 02 4F 00 03 F7 | off: OK dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    0.675  IN   F0 47 5E 00 02 44 00 03 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    0.675  OUT  F0 47 5E 00 03 00 07 01 F7
    0.682  IN   F0 47 5E 00 03 4F 00 07 F7 | off: OK dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    0.685  IN   F0 47 5E 00 03 44 00 07 F7 | off: DONE dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
#   opened as ready after 537 ms; DeviceIDs that answered the discovery: 0
#   as expected: the session is open
#   as expected: the session is bound to the target DeviceID
#   as expected: the session knows the checksums are off
#   closing: the session puts back the settings it changed
    0.858  OUT  F0 47 5E 00 04 00 04 00 08 F7
    0.865  IN   F0 47 5E 00 04 4F 00 04 F7 | off: OK dev 0 ref 04 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    0.868  IN   F0 47 5E 00 04 44 00 04 F7 | off: DONE dev 0 ref 04 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    0.868  OUT  F0 47 5E 00 05 00 07 00 F7
    0.876  IN   F0 47 5E 00 05 4F 00 07 F7 | off: OK dev 0 ref 05 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    0.878  IN   F0 47 5E 00 05 44 00 07 F7 | off: DONE dev 0 ref 05 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    0.879  OUT  F0 47 5E 00 06 00 03 01 F7
    0.886  IN   F0 47 5E 00 06 4F 00 03 F7 | off: OK dev 0 ref 06 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    0.888  IN   F0 47 5E 00 06 44 00 03 F7 | off: DONE dev 0 ref 06 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: Sync LCD
#   closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
#   as expected: every setting the session changed was put back (not put back: none)
#   as expected: the session is closed
#   PASSED: opened as ready after 537 ms; DeviceIDs that answered the discovery: 0; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# check 2: Echo returns the bytes sent
    1.049  OUT  F0 47 5E 00 00 00 00 00 F7
    1.057  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    1.059  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    1.552  OUT  F0 47 5E 00 01 00 04 00 05 F7
    1.560  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    1.562  IN   F0 47 5E 00 01 44 00 04 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
    1.563  OUT  F0 47 5E 00 02 00 03 00 F7
    1.570  IN   F0 47 5E 00 02 4F 00 03 F7 | off: OK dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    1.573  IN   F0 47 5E 00 02 44 00 03 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    1.573  OUT  F0 47 5E 00 03 00 07 01 F7
    1.581  IN   F0 47 5E 00 03 4F 00 07 F7 | off: OK dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    1.584  IN   F0 47 5E 00 03 44 00 07 F7 | off: DONE dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    1.738  OUT  F0 47 5E 00 04 00 06 01 23 45 67 F7
    1.746  IN   F0 47 5E 00 04 4F 00 06 F7 | off: OK dev 0 ref 04 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    1.751  IN   F0 47 5E 00 04 52 00 06 01 23 45 67 F7 | off: REPLY dev 0 ref 04 sec 00 item 06 data 01 23 45 67 | on: rejected: checksum missing or wrong
#   Echo 01 23 45 67: REPLY 01 23 45 67 as sent after 13 ms
#   closing: the session puts back the settings it changed
    1.798  OUT  F0 47 5E 00 05 00 04 00 09 F7
    1.806  IN   F0 47 5E 00 05 4F 00 04 F7 | off: OK dev 0 ref 05 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    1.809  IN   F0 47 5E 00 05 44 00 04 F7 | off: DONE dev 0 ref 05 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    1.809  OUT  F0 47 5E 00 06 00 07 00 F7
    1.817  IN   F0 47 5E 00 06 4F 00 07 F7 | off: OK dev 0 ref 06 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    1.819  IN   F0 47 5E 00 06 44 00 07 F7 | off: DONE dev 0 ref 06 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    1.819  OUT  F0 47 5E 00 07 00 03 01 F7
    1.826  IN   F0 47 5E 00 07 4F 00 03 F7 | off: OK dev 0 ref 07 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    1.829  IN   F0 47 5E 00 07 44 00 03 F7 | off: DONE dev 0 ref 07 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: Sync LCD
#   closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
#   as expected: every setting the session changed was put back (not put back: none)
#   as expected: the session is closed
#   PASSED: Echo 01 23 45 67: REPLY 01 23 45 67 as sent after 13 ms; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# check 3: 50 Echo round trips, timed
    1.981  OUT  F0 47 5E 00 00 00 00 00 F7
    1.988  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    1.991  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    2.502  OUT  F0 47 5E 00 01 00 04 00 05 F7
    2.512  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    2.514  IN   F0 47 5E 00 01 44 00 04 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
    2.515  OUT  F0 47 5E 00 02 00 03 00 F7
    2.524  IN   F0 47 5E 00 02 4F 00 03 F7 | off: OK dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    2.527  IN   F0 47 5E 00 02 44 00 03 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    2.528  OUT  F0 47 5E 00 03 00 07 01 F7
    2.537  IN   F0 47 5E 00 03 4F 00 07 F7 | off: OK dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    2.538  IN   F0 47 5E 00 03 44 00 07 F7 | off: DONE dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    2.694  OUT  F0 47 5E 00 04 00 06 00 23 45 67 F7
    2.702  IN   F0 47 5E 00 04 4F 00 06 F7 | off: OK dev 0 ref 04 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.706  IN   F0 47 5E 00 04 52 00 06 00 23 45 67 F7 | off: REPLY dev 0 ref 04 sec 00 item 06 data 00 23 45 67 | on: rejected: checksum missing or wrong
    2.745  OUT  F0 47 5E 00 05 00 06 01 23 45 67 F7
    2.753  IN   F0 47 5E 00 05 4F 00 06 F7 | off: OK dev 0 ref 05 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.758  IN   F0 47 5E 00 05 52 00 06 01 23 45 67 F7 | off: REPLY dev 0 ref 05 sec 00 item 06 data 01 23 45 67 | on: rejected: checksum missing or wrong
    2.796  OUT  F0 47 5E 00 06 00 06 02 23 45 67 F7
    2.804  IN   F0 47 5E 00 06 4F 00 06 F7 | off: OK dev 0 ref 06 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.809  IN   F0 47 5E 00 06 52 00 06 02 23 45 67 F7 | off: REPLY dev 0 ref 06 sec 00 item 06 data 02 23 45 67 | on: rejected: checksum missing or wrong
    2.846  OUT  F0 47 5E 00 07 00 06 03 23 45 67 F7
    2.854  IN   F0 47 5E 00 07 4F 00 06 F7 | off: OK dev 0 ref 07 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.859  IN   F0 47 5E 00 07 52 00 06 03 23 45 67 F7 | off: REPLY dev 0 ref 07 sec 00 item 06 data 03 23 45 67 | on: rejected: checksum missing or wrong
    2.899  OUT  F0 47 5E 00 08 00 06 04 23 45 67 F7
    2.908  IN   F0 47 5E 00 08 4F 00 06 F7 | off: OK dev 0 ref 08 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.912  IN   F0 47 5E 00 08 52 00 06 04 23 45 67 F7 | off: REPLY dev 0 ref 08 sec 00 item 06 data 04 23 45 67 | on: rejected: checksum missing or wrong
    2.953  OUT  F0 47 5E 00 09 00 06 05 23 45 67 F7
    2.962  IN   F0 47 5E 00 09 4F 00 06 F7 | off: OK dev 0 ref 09 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.966  IN   F0 47 5E 00 09 52 00 06 05 23 45 67 F7 | off: REPLY dev 0 ref 09 sec 00 item 06 data 05 23 45 67 | on: rejected: checksum missing or wrong
    3.005  OUT  F0 47 5E 00 0A 00 06 06 23 45 67 F7
    3.013  IN   F0 47 5E 00 0A 4F 00 06 F7 | off: OK dev 0 ref 0A sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.018  IN   F0 47 5E 00 0A 52 00 06 06 23 45 67 F7 | off: REPLY dev 0 ref 0A sec 00 item 06 data 06 23 45 67 | on: rejected: checksum missing or wrong
    3.056  OUT  F0 47 5E 00 0B 00 06 07 23 45 67 F7
    3.064  IN   F0 47 5E 00 0B 4F 00 06 F7 | off: OK dev 0 ref 0B sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.068  IN   F0 47 5E 00 0B 52 00 06 07 23 45 67 F7 | off: REPLY dev 0 ref 0B sec 00 item 06 data 07 23 45 67 | on: rejected: checksum missing or wrong
    3.108  OUT  F0 47 5E 00 0C 00 06 08 23 45 67 F7
    3.117  IN   F0 47 5E 00 0C 4F 00 06 F7 | off: OK dev 0 ref 0C sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.120  IN   F0 47 5E 00 0C 52 00 06 08 23 45 67 F7 | off: REPLY dev 0 ref 0C sec 00 item 06 data 08 23 45 67 | on: rejected: checksum missing or wrong
    3.153  OUT  F0 47 5E 00 0D 00 06 09 23 45 67 F7
    3.161  IN   F0 47 5E 00 0D 4F 00 06 F7 | off: OK dev 0 ref 0D sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.165  IN   F0 47 5E 00 0D 52 00 06 09 23 45 67 F7 | off: REPLY dev 0 ref 0D sec 00 item 06 data 09 23 45 67 | on: rejected: checksum missing or wrong
    3.201  OUT  F0 47 5E 00 0E 00 06 0A 23 45 67 F7
    3.209  IN   F0 47 5E 00 0E 4F 00 06 F7 | off: OK dev 0 ref 0E sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.213  IN   F0 47 5E 00 0E 52 00 06 0A 23 45 67 F7 | off: REPLY dev 0 ref 0E sec 00 item 06 data 0A 23 45 67 | on: rejected: checksum missing or wrong
    3.252  OUT  F0 47 5E 00 0F 00 06 0B 23 45 67 F7
    3.261  IN   F0 47 5E 00 0F 4F 00 06 F7 | off: OK dev 0 ref 0F sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.265  IN   F0 47 5E 00 0F 52 00 06 0B 23 45 67 F7 | off: REPLY dev 0 ref 0F sec 00 item 06 data 0B 23 45 67 | on: rejected: checksum missing or wrong
    3.301  OUT  F0 47 5E 00 10 00 06 0C 23 45 67 F7
    3.310  IN   F0 47 5E 00 10 4F 00 06 F7 | off: OK dev 0 ref 10 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.314  IN   F0 47 5E 00 10 52 00 06 0C 23 45 67 F7 | off: REPLY dev 0 ref 10 sec 00 item 06 data 0C 23 45 67 | on: rejected: checksum missing or wrong
    3.351  OUT  F0 47 5E 00 11 00 06 0D 23 45 67 F7
    3.359  IN   F0 47 5E 00 11 4F 00 06 F7 | off: OK dev 0 ref 11 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.363  IN   F0 47 5E 00 11 52 00 06 0D 23 45 67 F7 | off: REPLY dev 0 ref 11 sec 00 item 06 data 0D 23 45 67 | on: rejected: checksum missing or wrong
    3.399  OUT  F0 47 5E 00 12 00 06 0E 23 45 67 F7
    3.407  IN   F0 47 5E 00 12 4F 00 06 F7 | off: OK dev 0 ref 12 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.411  IN   F0 47 5E 00 12 52 00 06 0E 23 45 67 F7 | off: REPLY dev 0 ref 12 sec 00 item 06 data 0E 23 45 67 | on: rejected: checksum missing or wrong
    3.448  OUT  F0 47 5E 00 13 00 06 0F 23 45 67 F7
    3.457  IN   F0 47 5E 00 13 4F 00 06 F7 | off: OK dev 0 ref 13 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.461  IN   F0 47 5E 00 13 52 00 06 0F 23 45 67 F7 | off: REPLY dev 0 ref 13 sec 00 item 06 data 0F 23 45 67 | on: rejected: checksum missing or wrong
    3.496  OUT  F0 47 5E 00 14 00 06 10 23 45 67 F7
    3.503  IN   F0 47 5E 00 14 4F 00 06 F7 | off: OK dev 0 ref 14 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.509  IN   F0 47 5E 00 14 52 00 06 10 23 45 67 F7 | off: REPLY dev 0 ref 14 sec 00 item 06 data 10 23 45 67 | on: rejected: checksum missing or wrong
    3.546  OUT  F0 47 5E 00 15 00 06 11 23 45 67 F7
    3.555  IN   F0 47 5E 00 15 4F 00 06 F7 | off: OK dev 0 ref 15 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.559  IN   F0 47 5E 00 15 52 00 06 11 23 45 67 F7 | off: REPLY dev 0 ref 15 sec 00 item 06 data 11 23 45 67 | on: rejected: checksum missing or wrong
    3.598  OUT  F0 47 5E 00 16 00 06 12 23 45 67 F7
    3.606  IN   F0 47 5E 00 16 4F 00 06 F7 | off: OK dev 0 ref 16 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.610  IN   F0 47 5E 00 16 52 00 06 12 23 45 67 F7 | off: REPLY dev 0 ref 16 sec 00 item 06 data 12 23 45 67 | on: rejected: checksum missing or wrong
    3.646  OUT  F0 47 5E 00 17 00 06 13 23 45 67 F7
    3.654  IN   F0 47 5E 00 17 4F 00 06 F7 | off: OK dev 0 ref 17 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.658  IN   F0 47 5E 00 17 52 00 06 13 23 45 67 F7 | off: REPLY dev 0 ref 17 sec 00 item 06 data 13 23 45 67 | on: rejected: checksum missing or wrong
    3.693  OUT  F0 47 5E 00 18 00 06 14 23 45 67 F7
    3.701  IN   F0 47 5E 00 18 4F 00 06 F7 | off: OK dev 0 ref 18 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.706  IN   F0 47 5E 00 18 52 00 06 14 23 45 67 F7 | off: REPLY dev 0 ref 18 sec 00 item 06 data 14 23 45 67 | on: rejected: checksum missing or wrong
    3.745  OUT  F0 47 5E 00 19 00 06 15 23 45 67 F7
    3.754  IN   F0 47 5E 00 19 4F 00 06 F7 | off: OK dev 0 ref 19 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.758  IN   F0 47 5E 00 19 52 00 06 15 23 45 67 F7 | off: REPLY dev 0 ref 19 sec 00 item 06 data 15 23 45 67 | on: rejected: checksum missing or wrong
    3.796  OUT  F0 47 5E 00 1A 00 06 16 23 45 67 F7
    3.804  IN   F0 47 5E 00 1A 4F 00 06 F7 | off: OK dev 0 ref 1A sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.808  IN   F0 47 5E 00 1A 52 00 06 16 23 45 67 F7 | off: REPLY dev 0 ref 1A sec 00 item 06 data 16 23 45 67 | on: rejected: checksum missing or wrong
    3.845  OUT  F0 47 5E 00 1B 00 06 17 23 45 67 F7
    3.853  IN   F0 47 5E 00 1B 4F 00 06 F7 | off: OK dev 0 ref 1B sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.857  IN   F0 47 5E 00 1B 52 00 06 17 23 45 67 F7 | off: REPLY dev 0 ref 1B sec 00 item 06 data 17 23 45 67 | on: rejected: checksum missing or wrong
    3.894  OUT  F0 47 5E 00 1C 00 06 18 23 45 67 F7
    3.903  IN   F0 47 5E 00 1C 4F 00 06 F7 | off: OK dev 0 ref 1C sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.907  IN   F0 47 5E 00 1C 52 00 06 18 23 45 67 F7 | off: REPLY dev 0 ref 1C sec 00 item 06 data 18 23 45 67 | on: rejected: checksum missing or wrong
    3.947  OUT  F0 47 5E 00 1D 00 06 19 23 45 67 F7
    3.956  IN   F0 47 5E 00 1D 4F 00 06 F7 | off: OK dev 0 ref 1D sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.959  IN   F0 47 5E 00 1D 52 00 06 19 23 45 67 F7 | off: REPLY dev 0 ref 1D sec 00 item 06 data 19 23 45 67 | on: rejected: checksum missing or wrong
    4.006  OUT  F0 47 5E 00 1E 00 06 1A 23 45 67 F7
    4.014  IN   F0 47 5E 00 1E 4F 00 06 F7 | off: OK dev 0 ref 1E sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.020  IN   F0 47 5E 00 1E 52 00 06 1A 23 45 67 F7 | off: REPLY dev 0 ref 1E sec 00 item 06 data 1A 23 45 67 | on: rejected: checksum missing or wrong
    4.058  OUT  F0 47 5E 00 1F 00 06 1B 23 45 67 F7
    4.067  IN   F0 47 5E 00 1F 4F 00 06 F7 | off: OK dev 0 ref 1F sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.070  IN   F0 47 5E 00 1F 52 00 06 1B 23 45 67 F7 | off: REPLY dev 0 ref 1F sec 00 item 06 data 1B 23 45 67 | on: rejected: checksum missing or wrong
    4.107  OUT  F0 47 5E 00 20 00 06 1C 23 45 67 F7
    4.115  IN   F0 47 5E 00 20 4F 00 06 F7 | off: OK dev 0 ref 20 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.120  IN   F0 47 5E 00 20 52 00 06 1C 23 45 67 F7 | off: REPLY dev 0 ref 20 sec 00 item 06 data 1C 23 45 67 | on: rejected: checksum missing or wrong
    4.156  OUT  F0 47 5E 00 21 00 06 1D 23 45 67 F7
    4.164  IN   F0 47 5E 00 21 4F 00 06 F7 | off: OK dev 0 ref 21 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.168  IN   F0 47 5E 00 21 52 00 06 1D 23 45 67 F7 | off: REPLY dev 0 ref 21 sec 00 item 06 data 1D 23 45 67 | on: rejected: checksum missing or wrong
    4.203  OUT  F0 47 5E 00 22 00 06 1E 23 45 67 F7
    4.211  IN   F0 47 5E 00 22 4F 00 06 F7 | off: OK dev 0 ref 22 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.215  IN   F0 47 5E 00 22 52 00 06 1E 23 45 67 F7 | off: REPLY dev 0 ref 22 sec 00 item 06 data 1E 23 45 67 | on: rejected: checksum missing or wrong
    4.255  OUT  F0 47 5E 00 23 00 06 1F 23 45 67 F7
    4.264  IN   F0 47 5E 00 23 4F 00 06 F7 | off: OK dev 0 ref 23 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.268  IN   F0 47 5E 00 23 52 00 06 1F 23 45 67 F7 | off: REPLY dev 0 ref 23 sec 00 item 06 data 1F 23 45 67 | on: rejected: checksum missing or wrong
    4.307  OUT  F0 47 5E 00 24 00 06 20 23 45 67 F7
    4.315  IN   F0 47 5E 00 24 4F 00 06 F7 | off: OK dev 0 ref 24 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.320  IN   F0 47 5E 00 24 52 00 06 20 23 45 67 F7 | off: REPLY dev 0 ref 24 sec 00 item 06 data 20 23 45 67 | on: rejected: checksum missing or wrong
    4.359  OUT  F0 47 5E 00 25 00 06 21 23 45 67 F7
    4.367  IN   F0 47 5E 00 25 4F 00 06 F7 | off: OK dev 0 ref 25 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.371  IN   F0 47 5E 00 25 52 00 06 21 23 45 67 F7 | off: REPLY dev 0 ref 25 sec 00 item 06 data 21 23 45 67 | on: rejected: checksum missing or wrong
    4.410  OUT  F0 47 5E 00 26 00 06 22 23 45 67 F7
    4.419  IN   F0 47 5E 00 26 4F 00 06 F7 | off: OK dev 0 ref 26 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.422  IN   F0 47 5E 00 26 52 00 06 22 23 45 67 F7 | off: REPLY dev 0 ref 26 sec 00 item 06 data 22 23 45 67 | on: rejected: checksum missing or wrong
    4.460  OUT  F0 47 5E 00 27 00 06 23 23 45 67 F7
    4.468  IN   F0 47 5E 00 27 4F 00 06 F7 | off: OK dev 0 ref 27 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.473  IN   F0 47 5E 00 27 52 00 06 23 23 45 67 F7 | off: REPLY dev 0 ref 27 sec 00 item 06 data 23 23 45 67 | on: rejected: checksum missing or wrong
    4.518  OUT  F0 47 5E 00 28 00 06 24 23 45 67 F7
    4.526  IN   F0 47 5E 00 28 4F 00 06 F7 | off: OK dev 0 ref 28 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.530  IN   F0 47 5E 00 28 52 00 06 24 23 45 67 F7 | off: REPLY dev 0 ref 28 sec 00 item 06 data 24 23 45 67 | on: rejected: checksum missing or wrong
    4.569  OUT  F0 47 5E 00 29 00 06 25 23 45 67 F7
    4.578  IN   F0 47 5E 00 29 4F 00 06 F7 | off: OK dev 0 ref 29 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.582  IN   F0 47 5E 00 29 52 00 06 25 23 45 67 F7 | off: REPLY dev 0 ref 29 sec 00 item 06 data 25 23 45 67 | on: rejected: checksum missing or wrong
    4.621  OUT  F0 47 5E 00 2A 00 06 26 23 45 67 F7
    4.629  IN   F0 47 5E 00 2A 4F 00 06 F7 | off: OK dev 0 ref 2A sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.634  IN   F0 47 5E 00 2A 52 00 06 26 23 45 67 F7 | off: REPLY dev 0 ref 2A sec 00 item 06 data 26 23 45 67 | on: rejected: checksum missing or wrong
    4.675  OUT  F0 47 5E 00 2B 00 06 27 23 45 67 F7
    4.683  IN   F0 47 5E 00 2B 4F 00 06 F7 | off: OK dev 0 ref 2B sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.688  IN   F0 47 5E 00 2B 52 00 06 27 23 45 67 F7 | off: REPLY dev 0 ref 2B sec 00 item 06 data 27 23 45 67 | on: rejected: checksum missing or wrong
    4.725  OUT  F0 47 5E 00 2C 00 06 28 23 45 67 F7
    4.733  IN   F0 47 5E 00 2C 4F 00 06 F7 | off: OK dev 0 ref 2C sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.737  IN   F0 47 5E 00 2C 52 00 06 28 23 45 67 F7 | off: REPLY dev 0 ref 2C sec 00 item 06 data 28 23 45 67 | on: rejected: checksum missing or wrong
    4.774  OUT  F0 47 5E 00 2D 00 06 29 23 45 67 F7
    4.782  IN   F0 47 5E 00 2D 4F 00 06 F7 | off: OK dev 0 ref 2D sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.786  IN   F0 47 5E 00 2D 52 00 06 29 23 45 67 F7 | off: REPLY dev 0 ref 2D sec 00 item 06 data 29 23 45 67 | on: rejected: checksum missing or wrong
    4.821  OUT  F0 47 5E 00 2E 00 06 2A 23 45 67 F7
    4.829  IN   F0 47 5E 00 2E 4F 00 06 F7 | off: OK dev 0 ref 2E sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.833  IN   F0 47 5E 00 2E 52 00 06 2A 23 45 67 F7 | off: REPLY dev 0 ref 2E sec 00 item 06 data 2A 23 45 67 | on: rejected: checksum missing or wrong
    4.870  OUT  F0 47 5E 00 2F 00 06 2B 23 45 67 F7
    4.878  IN   F0 47 5E 00 2F 4F 00 06 F7 | off: OK dev 0 ref 2F sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.882  IN   F0 47 5E 00 2F 52 00 06 2B 23 45 67 F7 | off: REPLY dev 0 ref 2F sec 00 item 06 data 2B 23 45 67 | on: rejected: checksum missing or wrong
    4.921  OUT  F0 47 5E 00 30 00 06 2C 23 45 67 F7
    4.929  IN   F0 47 5E 00 30 4F 00 06 F7 | off: OK dev 0 ref 30 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.933  IN   F0 47 5E 00 30 52 00 06 2C 23 45 67 F7 | off: REPLY dev 0 ref 30 sec 00 item 06 data 2C 23 45 67 | on: rejected: checksum missing or wrong
    4.976  OUT  F0 47 5E 00 31 00 06 2D 23 45 67 F7
    4.985  IN   F0 47 5E 00 31 4F 00 06 F7 | off: OK dev 0 ref 31 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.989  IN   F0 47 5E 00 31 52 00 06 2D 23 45 67 F7 | off: REPLY dev 0 ref 31 sec 00 item 06 data 2D 23 45 67 | on: rejected: checksum missing or wrong
    5.026  OUT  F0 47 5E 00 32 00 06 2E 23 45 67 F7
    5.034  IN   F0 47 5E 00 32 4F 00 06 F7 | off: OK dev 0 ref 32 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.038  IN   F0 47 5E 00 32 52 00 06 2E 23 45 67 F7 | off: REPLY dev 0 ref 32 sec 00 item 06 data 2E 23 45 67 | on: rejected: checksum missing or wrong
    5.076  OUT  F0 47 5E 00 33 00 06 2F 23 45 67 F7
    5.084  IN   F0 47 5E 00 33 4F 00 06 F7 | off: OK dev 0 ref 33 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.088  IN   F0 47 5E 00 33 52 00 06 2F 23 45 67 F7 | off: REPLY dev 0 ref 33 sec 00 item 06 data 2F 23 45 67 | on: rejected: checksum missing or wrong
    5.125  OUT  F0 47 5E 00 34 00 06 30 23 45 67 F7
    5.133  IN   F0 47 5E 00 34 4F 00 06 F7 | off: OK dev 0 ref 34 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.137  IN   F0 47 5E 00 34 52 00 06 30 23 45 67 F7 | off: REPLY dev 0 ref 34 sec 00 item 06 data 30 23 45 67 | on: rejected: checksum missing or wrong
    5.175  OUT  F0 47 5E 00 35 00 06 31 23 45 67 F7
    5.184  IN   F0 47 5E 00 35 4F 00 06 F7 | off: OK dev 0 ref 35 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.188  IN   F0 47 5E 00 35 52 00 06 31 23 45 67 F7 | off: REPLY dev 0 ref 35 sec 00 item 06 data 31 23 45 67 | on: rejected: checksum missing or wrong
#   50 Echo round trips: min 12 ms, median 12 ms, 95th percentile 13 ms, max 13 ms
#   as expected: the slowest round trip is shorter than the command timeout in use (3000 ms)
#   closing: the session puts back the settings it changed
    5.240  OUT  F0 47 5E 00 36 00 04 00 3A F7
    5.247  IN   F0 47 5E 00 36 4F 00 04 F7 | off: OK dev 0 ref 36 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    5.250  IN   F0 47 5E 00 36 44 00 04 F7 | off: DONE dev 0 ref 36 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    5.250  OUT  F0 47 5E 00 37 00 07 00 F7
    5.257  IN   F0 47 5E 00 37 4F 00 07 F7 | off: OK dev 0 ref 37 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    5.260  IN   F0 47 5E 00 37 44 00 07 F7 | off: DONE dev 0 ref 37 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    5.261  OUT  F0 47 5E 00 38 00 03 01 F7
    5.268  IN   F0 47 5E 00 38 4F 00 03 F7 | off: OK dev 0 ref 38 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    5.271  IN   F0 47 5E 00 38 44 00 03 F7 | off: DONE dev 0 ref 38 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: Sync LCD
#   closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
#   as expected: every setting the session changed was put back (not put back: none)
#   as expected: the session is closed
#   PASSED: 50 Echo round trips: min 12 ms, median 12 ms, 95th percentile 13 ms, max 13 ms; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# check 4: the operating system version is read
    5.429  OUT  F0 47 5E 00 00 00 00 00 F7
    5.436  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    5.439  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    5.940  OUT  F0 47 5E 00 01 00 04 00 05 F7
    5.949  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    5.952  IN   F0 47 5E 00 01 44 00 04 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
    5.953  OUT  F0 47 5E 00 02 00 03 00 F7
    5.962  IN   F0 47 5E 00 02 4F 00 03 F7 | off: OK dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    5.964  IN   F0 47 5E 00 02 44 00 03 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    5.965  OUT  F0 47 5E 00 03 00 07 01 F7
    5.974  IN   F0 47 5E 00 03 4F 00 07 F7 | off: OK dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    5.976  IN   F0 47 5E 00 03 44 00 07 F7 | off: DONE dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    6.138  OUT  F0 47 5E 00 04 02 00 F7
    6.144  IN   F0 47 5E 00 04 4F 02 00 F7 | off: OK dev 0 ref 04 sec 02 item 00 data - | on: rejected: checksum missing or wrong
    6.148  IN   F0 47 5E 00 04 52 02 00 02 0E F7 | off: REPLY dev 0 ref 04 sec 02 item 00 data 02 0E | on: rejected: checksum missing or wrong
    6.148  OUT  F0 47 5E 00 05 02 01 F7
    6.155  IN   F0 47 5E 00 05 4F 02 01 F7 | off: OK dev 0 ref 05 sec 02 item 01 data - | on: rejected: checksum missing or wrong
    6.158  IN   F0 47 5E 00 05 52 02 01 00 F7 | off: REPLY dev 0 ref 05 sec 02 item 01 data 00 | on: rejected: checksum missing or wrong
#   OS version (section 02, items 00 and 01): OS 2.14, sub-version 0 after 20 ms
#   closing: the session puts back the settings it changed
    6.232  OUT  F0 47 5E 00 06 00 04 00 0A F7
    6.240  IN   F0 47 5E 00 06 4F 00 04 F7 | off: OK dev 0 ref 06 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    6.243  IN   F0 47 5E 00 06 44 00 04 F7 | off: DONE dev 0 ref 06 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    6.243  OUT  F0 47 5E 00 07 00 07 00 F7
    6.250  IN   F0 47 5E 00 07 4F 00 07 F7 | off: OK dev 0 ref 07 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    6.253  IN   F0 47 5E 00 07 44 00 07 F7 | off: DONE dev 0 ref 07 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    6.253  OUT  F0 47 5E 00 08 00 03 01 F7
    6.260  IN   F0 47 5E 00 08 4F 00 03 F7 | off: OK dev 0 ref 08 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    6.263  IN   F0 47 5E 00 08 44 00 03 F7 | off: DONE dev 0 ref 08 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: Sync LCD
#   closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
#   as expected: every setting the session changed was put back (not put back: none)
#   as expected: the session is closed
#   PASSED: OS version (section 02, items 00 and 01): OS 2.14, sub-version 0 after 20 ms; closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
# check 5: checksums on and off through the session
    6.422  OUT  F0 47 5E 00 00 00 00 00 F7
    6.429  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    6.432  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    6.929  OUT  F0 47 5E 00 01 00 04 00 05 F7
    6.938  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    6.941  IN   F0 47 5E 00 01 44 00 04 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
    6.942  OUT  F0 47 5E 00 02 00 03 00 F7
    6.949  IN   F0 47 5E 00 02 4F 00 03 F7 | off: OK dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    6.952  IN   F0 47 5E 00 02 44 00 03 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    6.952  OUT  F0 47 5E 00 03 00 07 01 F7
    6.960  IN   F0 47 5E 00 03 4F 00 07 F7 | off: OK dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    6.962  IN   F0 47 5E 00 03 44 00 07 F7 | off: DONE dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    7.106  OUT  F0 47 5E 00 04 00 04 01 09 F7
    7.114  IN   F0 47 5E 00 04 4F 00 04 F7 | off: OK dev 0 ref 04 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    7.117  IN   F0 47 5E 00 04 44 00 04 4C F7 | off: DONE dev 0 ref 04 sec 00 item 04 data 4C | on: DONE dev 0 ref 04 sec 00 item 04 data -
# diagnostic: checksum mode changed: on
#   checksums on: DONE after 11 ms
#   as expected: the session follows the sampler: checksums on
    7.166  OUT  F0 47 5E 00 05 00 06 01 23 45 67 5B F7
    7.174  IN   F0 47 5E 00 05 4F 00 06 5A F7 | off: OK dev 0 ref 05 sec 00 item 06 data 5A | on: OK dev 0 ref 05 sec 00 item 06 data -
    7.179  IN   F0 47 5E 00 05 52 00 06 01 23 45 67 2D F7 | off: REPLY dev 0 ref 05 sec 00 item 06 data 01 23 45 67 2D | on: REPLY dev 0 ref 05 sec 00 item 06 data 01 23 45 67
#   Echo with checksums on (the REPLY carries a checksum): REPLY 01 23 45 67 as sent after 13 ms
    7.227  OUT  F0 47 5E 00 06 02 00 08 F7
    7.235  IN   F0 47 5E 00 06 4F 02 00 57 F7 | off: OK dev 0 ref 06 sec 02 item 00 data 57 | on: OK dev 0 ref 06 sec 02 item 00 data -
    7.238  IN   F0 47 5E 00 06 52 02 00 02 0E 6A F7 | off: REPLY dev 0 ref 06 sec 02 item 00 data 02 0E 6A | on: REPLY dev 0 ref 06 sec 02 item 00 data 02 0E
    7.239  OUT  F0 47 5E 00 07 02 01 0A F7
    7.246  IN   F0 47 5E 00 07 4F 02 01 59 F7 | off: OK dev 0 ref 07 sec 02 item 01 data 59 | on: OK dev 0 ref 07 sec 02 item 01 data -
    7.249  IN   F0 47 5E 00 07 52 02 01 00 5C F7 | off: REPLY dev 0 ref 07 sec 02 item 01 data 00 5C | on: REPLY dev 0 ref 07 sec 02 item 01 data 00
#   OS version with checksums on: OS 2.14, sub-version 0 after 22 ms
    7.331  OUT  F0 47 5E 00 08 00 04 00 0C F7
    7.339  IN   F0 47 5E 00 08 4F 00 04 5B F7 | off: OK dev 0 ref 08 sec 00 item 04 data 5B | on: OK dev 0 ref 08 sec 00 item 04 data -
    7.342  IN   F0 47 5E 00 08 44 00 04 F7 | off: DONE dev 0 ref 08 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
#   checksums off again: DONE after 11 ms
#   as expected: the session follows the sampler: checksums off
    7.394  OUT  F0 47 5E 00 09 00 06 01 23 45 67 F7
    7.404  IN   F0 47 5E 00 09 4F 00 06 F7 | off: OK dev 0 ref 09 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    7.406  IN   F0 47 5E 00 09 52 00 06 01 23 45 67 F7 | off: REPLY dev 0 ref 09 sec 00 item 06 data 01 23 45 67 | on: rejected: checksum missing or wrong
#   Echo with checksums off again: REPLY 01 23 45 67 as sent after 12 ms
#   closing: the session puts back the settings it changed
    7.450  OUT  F0 47 5E 00 0A 00 04 00 0E F7
    7.458  IN   F0 47 5E 00 0A 4F 00 04 F7 | off: OK dev 0 ref 0A sec 00 item 04 data - | on: rejected: checksum missing or wrong
    7.461  IN   F0 47 5E 00 0A 44 00 04 F7 | off: DONE dev 0 ref 0A sec 00 item 04 data - | on: rejected: checksum missing or wrong
    7.461  OUT  F0 47 5E 00 0B 00 07 00 F7
    7.468  IN   F0 47 5E 00 0B 4F 00 07 F7 | off: OK dev 0 ref 0B sec 00 item 07 data - | on: rejected: checksum missing or wrong
    7.471  IN   F0 47 5E 00 0B 44 00 07 F7 | off: DONE dev 0 ref 0B sec 00 item 07 data - | on: rejected: checksum missing or wrong
    7.471  OUT  F0 47 5E 00 0C 00 03 01 F7
    7.478  IN   F0 47 5E 00 0C 4F 00 03 F7 | off: OK dev 0 ref 0C sec 00 item 03 data - | on: rejected: checksum missing or wrong
    7.481  IN   F0 47 5E 00 0C 44 00 03 F7 | off: DONE dev 0 ref 0C sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: Sync LCD
#   closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
#   as expected: every setting the session changed was put back (not put back: none)
#   as expected: the session is closed
#   PASSED: checksums on: DONE after 11 ms; Echo with checksums on (the REPLY carries a checksum): REPLY 01 23 45 67 as sent after 13 ms; OS version with checksums on: OS 2.14, sub-version 0 after 22 ms; checksums off again: DONE after 11 ms; Echo with checksums off again: REPLY 01 23 45 67 as sent after 12 ms; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# check 6: closing puts back every setting the session changed
    7.662  OUT  F0 47 5E 00 00 00 00 00 F7
    7.669  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    7.672  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    8.178  OUT  F0 47 5E 00 01 00 04 01 06 F7
    8.185  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    8.188  IN   F0 47 5E 00 01 44 00 04 49 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data 49 | on: DONE dev 0 ref 01 sec 00 item 04 data -
# diagnostic: checksum mode changed: on
    8.188  OUT  F0 47 5E 00 02 00 01 00 03 F7
    8.196  IN   F0 47 5E 00 02 4F 00 01 52 F7 | off: OK dev 0 ref 02 sec 00 item 01 data 52 | on: OK dev 0 ref 02 sec 00 item 01 data -
    8.199  IN   F0 47 5E 00 02 44 00 01 47 F7 | off: DONE dev 0 ref 02 sec 00 item 01 data 47 | on: DONE dev 0 ref 02 sec 00 item 01 data -
    8.199  OUT  F0 47 5E 00 03 00 03 00 06 F7
    8.207  IN   F0 47 5E 00 03 44 00 03 4A F7 | off: DONE dev 0 ref 03 sec 00 item 03 data 4A | on: DONE dev 0 ref 03 sec 00 item 03 data -
    8.207  OUT  F0 47 5E 00 04 00 05 01 0A F7
    8.214  IN   F0 47 5E 00 04 44 00 05 4D F7 | off: DONE dev 0 ref 04 sec 00 item 05 data 4D | on: DONE dev 0 ref 04 sec 00 item 05 data -
    8.215  OUT  F0 47 5E 00 05 00 07 01 0D F7
    8.222  IN   F0 47 5E 00 05 44 00 07 50 F7 | off: DONE dev 0 ref 05 sec 00 item 07 data 50 | on: DONE dev 0 ref 05 sec 00 item 07 data -
#   opened as ready with checksums on, Notification off, Still Alive on, Sync LCD off and Auto screen update on
#   as expected: the session follows the checksum mode it established: on
#   closing: the session puts back the settings it changed
    8.428  OUT  F0 47 5E 00 06 00 04 00 0A F7
    8.436  IN   F0 47 5E 00 06 44 00 04 F7 | off: DONE dev 0 ref 06 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
    8.436  OUT  F0 47 5E 00 07 00 07 00 F7
    8.443  IN   F0 47 5E 00 07 44 00 07 F7 | off: DONE dev 0 ref 07 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    8.444  OUT  F0 47 5E 00 08 00 01 01 F7
    8.451  IN   F0 47 5E 00 08 44 00 01 F7 | off: DONE dev 0 ref 08 sec 00 item 01 data - | on: rejected: checksum missing or wrong
    8.452  OUT  F0 47 5E 00 09 00 03 01 F7
    8.459  IN   F0 47 5E 00 09 4F 00 03 F7 | off: OK dev 0 ref 09 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    8.462  IN   F0 47 5E 00 09 44 00 03 F7 | off: DONE dev 0 ref 09 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    8.462  OUT  F0 47 5E 00 0A 00 05 00 F7
    8.469  IN   F0 47 5E 00 0A 4F 00 05 F7 | off: OK dev 0 ref 0A sec 00 item 05 data - | on: rejected: checksum missing or wrong
    8.472  IN   F0 47 5E 00 0A 44 00 05 F7 | off: DONE dev 0 ref 0A sec 00 item 05 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: notification
#   put back: Sync LCD
#   put back: Auto screen update
#   closed after 44 ms, put back: checksum mode, Still Alive, notification, Sync LCD, Auto screen update
#   as expected: every setting was put back (not put back: none)
#   as expected: the settings put back are the ones the open changed (checksum mode, notification, Still Alive, Sync LCD, Auto screen update)
#   PASSED: opened as ready with checksums on, Notification off, Still Alive on, Sync LCD off and Auto screen update on; closed after 44 ms, put back: checksum mode, Still Alive, notification, Sync LCD, Auto screen update
# check 7: a check that fails half way leaves the sampler in the known state
    8.693  OUT  F0 47 5E 00 00 00 00 00 F7
    8.700  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    8.703  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    9.205  OUT  F0 47 5E 00 01 00 04 01 06 F7
    9.214  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    9.217  IN   F0 47 5E 00 01 44 00 04 49 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data 49 | on: DONE dev 0 ref 01 sec 00 item 04 data -
# diagnostic: checksum mode changed: on
    9.218  OUT  F0 47 5E 00 02 00 03 00 05 F7
    9.227  IN   F0 47 5E 00 02 4F 00 03 54 F7 | off: OK dev 0 ref 02 sec 00 item 03 data 54 | on: OK dev 0 ref 02 sec 00 item 03 data -
    9.230  IN   F0 47 5E 00 02 44 00 03 49 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data 49 | on: DONE dev 0 ref 02 sec 00 item 03 data -
    9.231  OUT  F0 47 5E 00 03 00 07 01 0B F7
    9.240  IN   F0 47 5E 00 03 4F 00 07 59 F7 | off: OK dev 0 ref 03 sec 00 item 07 data 59 | on: OK dev 0 ref 03 sec 00 item 07 data -
    9.244  IN   F0 47 5E 00 03 44 00 07 4E F7 | off: DONE dev 0 ref 03 sec 00 item 07 data 4E | on: DONE dev 0 ref 03 sec 00 item 07 data -
#   as expected: the session follows the checksum mode it established: on
#   the check ended with its session still open: the guard closes it
#   closing: the session puts back the settings it changed
    9.424  OUT  F0 47 5E 00 04 00 04 00 08 F7
    9.432  IN   F0 47 5E 00 04 4F 00 04 57 F7 | off: OK dev 0 ref 04 sec 00 item 04 data 57 | on: OK dev 0 ref 04 sec 00 item 04 data -
    9.435  IN   F0 47 5E 00 04 44 00 04 F7 | off: DONE dev 0 ref 04 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
    9.435  OUT  F0 47 5E 00 05 00 07 00 F7
    9.442  IN   F0 47 5E 00 05 4F 00 07 F7 | off: OK dev 0 ref 05 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    9.445  IN   F0 47 5E 00 05 44 00 07 F7 | off: DONE dev 0 ref 05 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    9.445  OUT  F0 47 5E 00 06 00 03 01 F7
    9.452  IN   F0 47 5E 00 06 4F 00 03 F7 | off: OK dev 0 ref 06 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    9.455  IN   F0 47 5E 00 06 44 00 03 F7 | off: DONE dev 0 ref 06 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: Sync LCD
#   the check failed: this check fails on purpose, with the checksums on
#   as expected: the guard closed the session when the check failed
#   put back by the guard: checksum mode, Still Alive, Sync LCD
#   as expected: the checksum mode was set off and the sampler confirmed it with a DONE
#   as expected: every setting was put back (not put back: none)
#   PASSED: put back by the guard: checksum mode, Still Alive, Sync LCD
# check 8: a slow operation with Still Alive on
    9.627  OUT  F0 47 5E 00 00 00 00 00 F7
    9.634  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    9.637  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
   10.134  OUT  F0 47 5E 00 01 00 04 00 05 F7
   10.144  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
   10.146  IN   F0 47 5E 00 01 44 00 04 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
   10.147  OUT  F0 47 5E 00 02 00 03 00 F7
   10.156  IN   F0 47 5E 00 02 4F 00 03 F7 | off: OK dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
   10.158  IN   F0 47 5E 00 02 44 00 03 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
   10.159  OUT  F0 47 5E 00 03 00 07 01 F7
   10.168  IN   F0 47 5E 00 03 4F 00 07 F7 | off: OK dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
   10.171  IN   F0 47 5E 00 03 44 00 07 F7 | off: DONE dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
#   as expected: Still Alive is on: a received F0 F7 restarts the pending command's timeout
   10.331  OUT  F0 47 5E 00 04 10 01 F7
   10.339  IN   F0 47 5E 00 04 4F 10 01 F7 | off: OK dev 0 ref 04 sec 10 item 01 data - | on: rejected: checksum missing or wrong
#   update the list of disks (section 10, item 01): TIMEOUT after 3008 ms; F0 F7 messages that reached the host meanwhile: 0
#   the check ended with its session still open: the guard closes it
#   closing: the session puts back the settings it changed
   13.394  OUT  F0 47 5E 00 05 00 04 00 09 F7
# diagnostic: checksum mode changed: unknown
#   NOT put back: checksum mode
#   NOT put back: Still Alive
#   NOT put back: Sync LCD
#   FAILED: timed out although Still Alive is on: the sampler sent no F0 F7 while it worked, or the backend did not deliver them
# check 9: a power cycle while a session is open
   16.452  OUT  F0 47 5E 00 00 00 00 00 F7
#   the check ended with its session still open: the guard closes it
#   closing: the session puts back the settings it changed
#   FAILED: the open ended as no sampler at the target DeviceID; DeviceIDs that answered the discovery: none
# observations
# observation: check 1 PASSED: open a session and close it - opened as ready after 537 ms; DeviceIDs that answered the discovery: 0; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 2 PASSED: Echo returns the bytes sent - Echo 01 23 45 67: REPLY 01 23 45 67 as sent after 13 ms; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 3 PASSED: 50 Echo round trips, timed - 50 Echo round trips: min 12 ms, median 12 ms, 95th percentile 13 ms, max 13 ms; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 4 PASSED: the operating system version is read - OS version (section 02, items 00 and 01): OS 2.14, sub-version 0 after 20 ms; closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 5 PASSED: checksums on and off through the session - checksums on: DONE after 11 ms; Echo with checksums on (the REPLY carries a checksum): REPLY 01 23 45 67 as sent after 13 ms; OS version with checksums on: OS 2.14, sub-version 0 after 22 ms; checksums off again: DONE after 11 ms; Echo with checksums off again: REPLY 01 23 45 67 as sent after 12 ms; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 6 PASSED: closing puts back every setting the session changed - opened as ready with checksums on, Notification off, Still Alive on, Sync LCD off and Auto screen update on; closed after 44 ms, put back: checksum mode, Still Alive, notification, Sync LCD, Auto screen update
# observation: check 7 PASSED: a check that fails half way leaves the sampler in the known state - put back by the guard: checksum mode, Still Alive, Sync LCD
# observation: check 8 FAILED: a slow operation with Still Alive on - timed out although Still Alive is on: the sampler sent no F0 F7 while it worked, or the backend did not deliver them
# observation: check 9 FAILED: a power cycle while a session is open - the open ended as no sampler at the target DeviceID; DeviceIDs that answered the discovery: none
# observation: 7 checks passed, 2 failed, 0 skipped
# observation: discovery answered by DeviceIDs: 0; the target DeviceID is 0, and the session accepts only confirmations that carry it
# observation: OS version 2.14 (sub-version 0)
# observation: 50 Echo round trips of 50: min 12 ms, median 12 ms, 95th percentile 13 ms, max 13 ms
# observation: F0 F7 messages seen: 0
# observation: messages rejected by the session: 0; unsolicited confirmations: 0; late ERRORs after a REPLY: 0
# observation: checksum mode as the sessions followed it: unknown -> off -> off -> off -> off -> off -> on -> off -> on -> off -> on -> off -> off -> unknown
# observation: sampler NOT confirmed in the known state (checksums off, Still Alive off, Notification on, Sync LCD on, Auto screen update off)
# observation: frames sent 119, received 227
```
