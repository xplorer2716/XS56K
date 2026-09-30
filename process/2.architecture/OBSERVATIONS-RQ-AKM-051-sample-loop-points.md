# Observations of §0E Loop Start / Loop End on the real S5000 (RQ-AKM-051, TASK-AKM-045, TASK-AKM-046)

The owner ran `xs56k_akm_probe --suite --sample-lifecycle` against the real sampler three times on
2026-09-30, closing `TASK-AKM-045`'s real-sampler run. The first two runs failed check 8; the failure
led to a code fix, confirmed by the third run. This file keeps all three logs verbatim and what they
show. It is an observation record, not an artifact with an ID: the process index ignores it.

## Setup

- AKAI S5000, OS 2.14, sub-version 0. Windows, `JuceMidiBackend`, an ESI M8U eX USB MIDI interface,
  input `MIDIIN2 (ESI M8U eX)`, output `MIDIOUT15 (ESI M8U eX)`, DeviceID 0.
- Program: `xs56k_akm_probe --suite --sample-lifecycle --sample-name "<name>"` (Debug build).
- Samples used: `AMEN` (~143 169 samples, 22 050 Hz, ≈ 6.49 s) and `Honesty` (~627 762–627 777
  samples, 44 100 Hz, ≈ 14.23 s). For the confirming run, the sampler's memory was cleared and
  `Honesty` reloaded fresh, so nothing left over from earlier runs could explain the result.

## What the logs show

| # | Observation | Evidence | Consequence |
|---|---|---|---|
| F1 | First attempt (`amen.log`, `AMEN`): `Set Start Position` (`&20`) sent with test values `{10, 20, 30, 40}`, which `SampleParameterCases.cpp` treated as 4 independent bytes rather than one 28-bit compound word (Msb…Lsb — `items.json`'s own arg names `positionMsb/positionSb2/positionSb1/positionLsb`). Decoded as a compound word that is **21 303 080**, far beyond the sample's length. The sampler answered `DONE` (no `ERROR`) but the value read back was `End Position − 128` exactly. | `amen.log` check 8, "get Set Start Position: read back [0, 8, 93, 65], expected [10, 20, 30, 40]" | Not a firmware bug: the test's own values were nonsensical for a real sample. `SampleSetEndPosition`/`SampleSetLoopStart`/`SampleSetLoopEnd` had the same defect (`{50,60,70,80}`, `{1,1,1,1}`, `{2,2,2,2}` all decode to values far beyond any real sample). Fixed in `SampleParameterCases.cpp`: small, ordered, realistic values (`Start=50 < LoopStart=500 < LoopEnd=1500 < End=2000`). |
| F2 | Second attempt, after F1's fix, still failed check 8, this time on the final restoration check, on two independent samples: `AMEN` (`Loop Start` read back **141 654** instead of the restored **1**) and `Honesty` (`Loop Start` read back **626 263** instead of the restored **1**). | `amen.log`/`honesty.log` (the `194256`/`195605`-era pair superseded by this file's F3 run) — "NOT MET: its settable parameters are back to what they were before" | Confirmed `Set Loop End` (`&2A`), when it runs *after* `Set Loop Start` (`&29`), leaves `Loop Start` wrong — reproducible, not a decode artifact: the grouped `&4B` reply's `Loop Start`/`Loop End` bytes, cross-checked against the owner's own front-panel readings of `Loop Start`/`Loop End`, matched exactly elsewhere in the same logs (see F3). |
| F3 | Fixed by always setting (and restoring) `Loop End` before `Loop Start` — `SampleParameterCases.cpp`'s case order and `GuardedTestSample::restoreParameters`'s send order (offsets still sliced in the wire/`&4B` order, only the send order changed). Confirming run on `Honesty`, sampler memory cleared and the sample freshly reloaded: **8/8 checks passed.** The owner read `Loop Start`/`Loop End` on the sampler's own screen before and after the run: **start 1, end 627762 — identical.** | `akm-suite-20260930-202853.log`, full log below; owner's screen readings | The fix holds against independent ground truth, not just this tool's own decode of the wire. `TASK-AKM-045`'s real-sampler run is done. |
| F4 | The exact mechanism behind F2 is not established. A "Set Loop End preserves the loop length in effect just before it runs" formula fits F2's restoration step exactly (both samples: `new Loop Start = new Loop End − 1499`, where 1499 was the test's own leftover `Loop End(1500) − Loop Start(1)`), but does not fit the *mid-test* reading taken later the same run (`Loop Start` read back as 15, not the value that formula predicts). | cross-checked during the investigation, not itself logged as a separate run | Left open (see Not observed). The fix (Loop End before Loop Start) is justified empirically by F2/F3 regardless of the exact rule. |
| F5 | Decoding validated independently: the grouped `&4B` reply's `Loop Start` (bytes 15–18 of 22) and `Loop End` (bytes 19–22) fields, decoded as 4-byte, most-significant-first, 7-bit-per-byte compound words, matched the owner's own front-panel readings exactly on both `AMEN` (`Loop Start=1`, `Loop End=143154`) and `Honesty` (`Loop Start=1`, `Loop End=627762`), across all runs. | every run's restore step vs. the owner's screen readings | The catalogue's `positionMsb/Sb2/Sb1/Lsb` encoding and this decode are correct; F1–F3 are genuine sampler/test-data findings, not a decode bug. |

## Not observed

The exact rule `Set Loop End` uses when recomputing `Loop Start` (F4); whether `Set Loop Start` has any
reciprocal effect on `Loop End` (no test exercised that direction); whether the same coupling applies to
`Set Start Position`/`Set End Position` (the playback bounds, distinct from the loop bounds) — nothing
in these runs suggests it does, but it was not specifically tested in a scenario that would show it.

## Amendments made because of it

- `juce/tests/support/src/SampleParameterCases.cpp`: realistic, ordered, in-range test values for the
  four 4-byte items; `Loop End` listed before `Loop Start` (F1, F2).
- `juce/tests/support/src/RealSamplerSuite.cpp` (`GuardedTestSample::restoreParameters`): restore sends
  now decoupled from the wire-order offset slicing, with `Loop End` sent before `Loop Start` (F2).
- `documents/_index/sysex_spec.kb.md`: State model section, a new bullet on `&2A` moving `&29`'s value (F2, F4).
- `PLAN-AKM-005-sample-primitives.md`: `TASK-AKM-045`'s Verification note, the real-sampler run.

## The logs, verbatim

### `akm-suite-20260930-202853.log` (confirming run, 8/8 passed)

```text
# XS56K AKM real-sampler suite
# started 2026-09-30T20:28:56Z
# target: in="MIDIIN2 (ESI M8U eX)" out="MIDIOUT15 (ESI M8U eX)" device-id=0
# Each check opens a session with Session::open and closes it with Session::close. The suite changes only section 00 settings (checksums, Notification, Still Alive, Sync LCD, Auto screen update), never a stored program, multi or sample, and leaves them as the closing does: checksums off, Still Alive off, Notification on, Sync LCD on, Auto screen update off.
# It also selects the sample named by --sample-name (RQ-AKM-051), skipped if it is empty, and renames it and back, starts and stops auditioning it, round-trips every settable §0E item on it (RQ-AKM-048) and confirms the grouped replies &34/&4B agree with the items they group (RQ-AKM-049): the sample's name and every settable parameter, and the sampler's original current-sample selection, are restored before this check returns, even if it fails half way, and it never sends &07 or &08.
# The log is written between the steps, never while a command is in flight, so that writing it cannot delay the exchanges it records: the times of the frames are those of the wire.
# check 1: open a session and close it
    0.244  OUT  F0 47 5E 00 00 00 00 00 F7
    0.251  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    0.254  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    0.759  OUT  F0 47 5E 00 01 00 04 00 05 F7
    0.769  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    0.771  IN   F0 47 5E 00 01 44 00 04 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
    0.772  OUT  F0 47 5E 00 02 00 03 00 F7
    0.780  IN   F0 47 5E 00 02 4F 00 03 F7 | off: OK dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    0.783  IN   F0 47 5E 00 02 44 00 03 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    0.784  OUT  F0 47 5E 00 03 00 07 01 F7
    0.792  IN   F0 47 5E 00 03 4F 00 07 F7 | off: OK dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    0.795  IN   F0 47 5E 00 03 44 00 07 F7 | off: DONE dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
#   opened as ready after 552 ms; DeviceIDs that answered the discovery: 0
#   as expected: the session is open
#   as expected: the session is bound to the target DeviceID
#   as expected: the session knows the checksums are off
#   closing: the session puts back the settings it changed
    1.098  OUT  F0 47 5E 00 04 00 04 00 08 F7
    1.105  IN   F0 47 5E 00 04 4F 00 04 F7 | off: OK dev 0 ref 04 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    1.108  IN   F0 47 5E 00 04 44 00 04 F7 | off: DONE dev 0 ref 04 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    1.108  OUT  F0 47 5E 00 05 00 07 00 F7
    1.115  IN   F0 47 5E 00 05 4F 00 07 F7 | off: OK dev 0 ref 05 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    1.118  IN   F0 47 5E 00 05 44 00 07 F7 | off: DONE dev 0 ref 05 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    1.118  OUT  F0 47 5E 00 06 00 03 01 F7
    1.125  IN   F0 47 5E 00 06 4F 00 03 F7 | off: OK dev 0 ref 06 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    1.128  IN   F0 47 5E 00 06 44 00 03 F7 | off: DONE dev 0 ref 06 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: Sync LCD
#   closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
#   as expected: every setting the session changed was put back (not put back: none)
#   as expected: the session is closed
#   PASSED: opened as ready after 552 ms; DeviceIDs that answered the discovery: 0; closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
# check 2: Echo returns the bytes sent
    1.294  OUT  F0 47 5E 00 00 00 00 00 F7
    1.301  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    1.304  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    1.807  OUT  F0 47 5E 00 01 00 04 00 05 F7
    1.816  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    1.818  IN   F0 47 5E 00 01 44 00 04 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
    1.819  OUT  F0 47 5E 00 02 00 03 00 F7
    1.827  IN   F0 47 5E 00 02 4F 00 03 F7 | off: OK dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    1.831  IN   F0 47 5E 00 02 44 00 03 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    1.831  OUT  F0 47 5E 00 03 00 07 01 F7
    1.840  IN   F0 47 5E 00 03 4F 00 07 F7 | off: OK dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    1.843  IN   F0 47 5E 00 03 44 00 07 F7 | off: DONE dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    2.198  OUT  F0 47 5E 00 04 00 06 01 23 45 67 F7
    2.206  IN   F0 47 5E 00 04 4F 00 06 F7 | off: OK dev 0 ref 04 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    2.210  IN   F0 47 5E 00 04 52 00 06 01 23 45 67 F7 | off: REPLY dev 0 ref 04 sec 00 item 06 data 01 23 45 67 | on: rejected: checksum missing or wrong
#   Echo 01 23 45 67: REPLY 01 23 45 67 as sent after 12 ms
#   closing: the session puts back the settings it changed
    2.262  OUT  F0 47 5E 00 05 00 04 00 09 F7
    2.269  IN   F0 47 5E 00 05 4F 00 04 F7 | off: OK dev 0 ref 05 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    2.272  IN   F0 47 5E 00 05 44 00 04 F7 | off: DONE dev 0 ref 05 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    2.273  OUT  F0 47 5E 00 06 00 07 00 F7
    2.279  IN   F0 47 5E 00 06 4F 00 07 F7 | off: OK dev 0 ref 06 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    2.283  IN   F0 47 5E 00 06 44 00 07 F7 | off: DONE dev 0 ref 06 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    2.283  OUT  F0 47 5E 00 07 00 03 01 F7
    2.290  IN   F0 47 5E 00 07 4F 00 03 F7 | off: OK dev 0 ref 07 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    2.293  IN   F0 47 5E 00 07 44 00 03 F7 | off: DONE dev 0 ref 07 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: Sync LCD
#   closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
#   as expected: every setting the session changed was put back (not put back: none)
#   as expected: the session is closed
#   PASSED: Echo 01 23 45 67: REPLY 01 23 45 67 as sent after 12 ms; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# check 3: 50 Echo round trips, timed
    2.464  OUT  F0 47 5E 00 00 00 00 00 F7
    2.471  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    2.474  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    2.976  OUT  F0 47 5E 00 01 00 04 00 05 F7
    2.985  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    2.988  IN   F0 47 5E 00 01 44 00 04 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
    2.989  OUT  F0 47 5E 00 02 00 03 00 F7
    2.997  IN   F0 47 5E 00 02 4F 00 03 F7 | off: OK dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    3.000  IN   F0 47 5E 00 02 44 00 03 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    3.000  OUT  F0 47 5E 00 03 00 07 01 F7
    3.009  IN   F0 47 5E 00 03 4F 00 07 F7 | off: OK dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    3.011  IN   F0 47 5E 00 03 44 00 07 F7 | off: DONE dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    3.275  OUT  F0 47 5E 00 04 00 06 00 23 45 67 F7
    3.283  IN   F0 47 5E 00 04 4F 00 06 F7 | off: OK dev 0 ref 04 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.287  IN   F0 47 5E 00 04 52 00 06 00 23 45 67 F7 | off: REPLY dev 0 ref 04 sec 00 item 06 data 00 23 45 67 | on: rejected: checksum missing or wrong
    3.327  OUT  F0 47 5E 00 05 00 06 01 23 45 67 F7
    3.335  IN   F0 47 5E 00 05 4F 00 06 F7 | off: OK dev 0 ref 05 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.339  IN   F0 47 5E 00 05 52 00 06 01 23 45 67 F7 | off: REPLY dev 0 ref 05 sec 00 item 06 data 01 23 45 67 | on: rejected: checksum missing or wrong
    3.377  OUT  F0 47 5E 00 06 00 06 02 23 45 67 F7
    3.385  IN   F0 47 5E 00 06 4F 00 06 F7 | off: OK dev 0 ref 06 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.390  IN   F0 47 5E 00 06 52 00 06 02 23 45 67 F7 | off: REPLY dev 0 ref 06 sec 00 item 06 data 02 23 45 67 | on: rejected: checksum missing or wrong
    3.429  OUT  F0 47 5E 00 07 00 06 03 23 45 67 F7
    3.437  IN   F0 47 5E 00 07 4F 00 06 F7 | off: OK dev 0 ref 07 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.442  IN   F0 47 5E 00 07 52 00 06 03 23 45 67 F7 | off: REPLY dev 0 ref 07 sec 00 item 06 data 03 23 45 67 | on: rejected: checksum missing or wrong
    3.478  OUT  F0 47 5E 00 08 00 06 04 23 45 67 F7
    3.487  IN   F0 47 5E 00 08 4F 00 06 F7 | off: OK dev 0 ref 08 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.491  IN   F0 47 5E 00 08 52 00 06 04 23 45 67 F7 | off: REPLY dev 0 ref 08 sec 00 item 06 data 04 23 45 67 | on: rejected: checksum missing or wrong
    3.529  OUT  F0 47 5E 00 09 00 06 05 23 45 67 F7
    3.536  IN   F0 47 5E 00 09 4F 00 06 F7 | off: OK dev 0 ref 09 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.541  IN   F0 47 5E 00 09 52 00 06 05 23 45 67 F7 | off: REPLY dev 0 ref 09 sec 00 item 06 data 05 23 45 67 | on: rejected: checksum missing or wrong
    3.581  OUT  F0 47 5E 00 0A 00 06 06 23 45 67 F7
    3.589  IN   F0 47 5E 00 0A 4F 00 06 F7 | off: OK dev 0 ref 0A sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.593  IN   F0 47 5E 00 0A 52 00 06 06 23 45 67 F7 | off: REPLY dev 0 ref 0A sec 00 item 06 data 06 23 45 67 | on: rejected: checksum missing or wrong
    3.634  OUT  F0 47 5E 00 0B 00 06 07 23 45 67 F7
    3.643  IN   F0 47 5E 00 0B 4F 00 06 F7 | off: OK dev 0 ref 0B sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.647  IN   F0 47 5E 00 0B 52 00 06 07 23 45 67 F7 | off: REPLY dev 0 ref 0B sec 00 item 06 data 07 23 45 67 | on: rejected: checksum missing or wrong
    3.687  OUT  F0 47 5E 00 0C 00 06 08 23 45 67 F7
    3.696  IN   F0 47 5E 00 0C 4F 00 06 F7 | off: OK dev 0 ref 0C sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.699  IN   F0 47 5E 00 0C 52 00 06 08 23 45 67 F7 | off: REPLY dev 0 ref 0C sec 00 item 06 data 08 23 45 67 | on: rejected: checksum missing or wrong
    3.738  OUT  F0 47 5E 00 0D 00 06 09 23 45 67 F7
    3.747  IN   F0 47 5E 00 0D 4F 00 06 F7 | off: OK dev 0 ref 0D sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.751  IN   F0 47 5E 00 0D 52 00 06 09 23 45 67 F7 | off: REPLY dev 0 ref 0D sec 00 item 06 data 09 23 45 67 | on: rejected: checksum missing or wrong
    3.797  OUT  F0 47 5E 00 0E 00 06 0A 23 45 67 F7
    3.805  IN   F0 47 5E 00 0E 4F 00 06 F7 | off: OK dev 0 ref 0E sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.809  IN   F0 47 5E 00 0E 52 00 06 0A 23 45 67 F7 | off: REPLY dev 0 ref 0E sec 00 item 06 data 0A 23 45 67 | on: rejected: checksum missing or wrong
    3.847  OUT  F0 47 5E 00 0F 00 06 0B 23 45 67 F7
    3.855  IN   F0 47 5E 00 0F 4F 00 06 F7 | off: OK dev 0 ref 0F sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.859  IN   F0 47 5E 00 0F 52 00 06 0B 23 45 67 F7 | off: REPLY dev 0 ref 0F sec 00 item 06 data 0B 23 45 67 | on: rejected: checksum missing or wrong
    3.900  OUT  F0 47 5E 00 10 00 06 0C 23 45 67 F7
    3.909  IN   F0 47 5E 00 10 4F 00 06 F7 | off: OK dev 0 ref 10 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.912  IN   F0 47 5E 00 10 52 00 06 0C 23 45 67 F7 | off: REPLY dev 0 ref 10 sec 00 item 06 data 0C 23 45 67 | on: rejected: checksum missing or wrong
    3.950  OUT  F0 47 5E 00 11 00 06 0D 23 45 67 F7
    3.958  IN   F0 47 5E 00 11 4F 00 06 F7 | off: OK dev 0 ref 11 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    3.962  IN   F0 47 5E 00 11 52 00 06 0D 23 45 67 F7 | off: REPLY dev 0 ref 11 sec 00 item 06 data 0D 23 45 67 | on: rejected: checksum missing or wrong
    4.014  OUT  F0 47 5E 00 12 00 06 0E 23 45 67 F7
    4.022  IN   F0 47 5E 00 12 4F 00 06 F7 | off: OK dev 0 ref 12 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.026  IN   F0 47 5E 00 12 52 00 06 0E 23 45 67 F7 | off: REPLY dev 0 ref 12 sec 00 item 06 data 0E 23 45 67 | on: rejected: checksum missing or wrong
    4.066  OUT  F0 47 5E 00 13 00 06 0F 23 45 67 F7
    4.074  IN   F0 47 5E 00 13 4F 00 06 F7 | off: OK dev 0 ref 13 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.078  IN   F0 47 5E 00 13 52 00 06 0F 23 45 67 F7 | off: REPLY dev 0 ref 13 sec 00 item 06 data 0F 23 45 67 | on: rejected: checksum missing or wrong
    4.119  OUT  F0 47 5E 00 14 00 06 10 23 45 67 F7
    4.127  IN   F0 47 5E 00 14 4F 00 06 F7 | off: OK dev 0 ref 14 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.132  IN   F0 47 5E 00 14 52 00 06 10 23 45 67 F7 | off: REPLY dev 0 ref 14 sec 00 item 06 data 10 23 45 67 | on: rejected: checksum missing or wrong
    4.170  OUT  F0 47 5E 00 15 00 06 11 23 45 67 F7
    4.178  IN   F0 47 5E 00 15 4F 00 06 F7 | off: OK dev 0 ref 15 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.182  IN   F0 47 5E 00 15 52 00 06 11 23 45 67 F7 | off: REPLY dev 0 ref 15 sec 00 item 06 data 11 23 45 67 | on: rejected: checksum missing or wrong
    4.224  OUT  F0 47 5E 00 16 00 06 12 23 45 67 F7
    4.232  IN   F0 47 5E 00 16 4F 00 06 F7 | off: OK dev 0 ref 16 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.236  IN   F0 47 5E 00 16 52 00 06 12 23 45 67 F7 | off: REPLY dev 0 ref 16 sec 00 item 06 data 12 23 45 67 | on: rejected: checksum missing or wrong
    4.276  OUT  F0 47 5E 00 17 00 06 13 23 45 67 F7
    4.284  IN   F0 47 5E 00 17 4F 00 06 F7 | off: OK dev 0 ref 17 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.288  IN   F0 47 5E 00 17 52 00 06 13 23 45 67 F7 | off: REPLY dev 0 ref 17 sec 00 item 06 data 13 23 45 67 | on: rejected: checksum missing or wrong
    4.327  OUT  F0 47 5E 00 18 00 06 14 23 45 67 F7
    4.335  IN   F0 47 5E 00 18 4F 00 06 F7 | off: OK dev 0 ref 18 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.340  IN   F0 47 5E 00 18 52 00 06 14 23 45 67 F7 | off: REPLY dev 0 ref 18 sec 00 item 06 data 14 23 45 67 | on: rejected: checksum missing or wrong
    4.379  OUT  F0 47 5E 00 19 00 06 15 23 45 67 F7
    4.387  IN   F0 47 5E 00 19 4F 00 06 F7 | off: OK dev 0 ref 19 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.391  IN   F0 47 5E 00 19 52 00 06 15 23 45 67 F7 | off: REPLY dev 0 ref 19 sec 00 item 06 data 15 23 45 67 | on: rejected: checksum missing or wrong
    4.429  OUT  F0 47 5E 00 1A 00 06 16 23 45 67 F7
    4.438  IN   F0 47 5E 00 1A 4F 00 06 F7 | off: OK dev 0 ref 1A sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.442  IN   F0 47 5E 00 1A 52 00 06 16 23 45 67 F7 | off: REPLY dev 0 ref 1A sec 00 item 06 data 16 23 45 67 | on: rejected: checksum missing or wrong
    4.478  OUT  F0 47 5E 00 1B 00 06 17 23 45 67 F7
    4.486  IN   F0 47 5E 00 1B 4F 00 06 F7 | off: OK dev 0 ref 1B sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.490  IN   F0 47 5E 00 1B 52 00 06 17 23 45 67 F7 | off: REPLY dev 0 ref 1B sec 00 item 06 data 17 23 45 67 | on: rejected: checksum missing or wrong
    4.529  OUT  F0 47 5E 00 1C 00 06 18 23 45 67 F7
    4.538  IN   F0 47 5E 00 1C 4F 00 06 F7 | off: OK dev 0 ref 1C sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.542  IN   F0 47 5E 00 1C 52 00 06 18 23 45 67 F7 | off: REPLY dev 0 ref 1C sec 00 item 06 data 18 23 45 67 | on: rejected: checksum missing or wrong
    4.586  OUT  F0 47 5E 00 1D 00 06 19 23 45 67 F7
    4.594  IN   F0 47 5E 00 1D 4F 00 06 F7 | off: OK dev 0 ref 1D sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.598  IN   F0 47 5E 00 1D 52 00 06 19 23 45 67 F7 | off: REPLY dev 0 ref 1D sec 00 item 06 data 19 23 45 67 | on: rejected: checksum missing or wrong
    4.643  OUT  F0 47 5E 00 1E 00 06 1A 23 45 67 F7
    4.652  IN   F0 47 5E 00 1E 4F 00 06 F7 | off: OK dev 0 ref 1E sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.655  IN   F0 47 5E 00 1E 52 00 06 1A 23 45 67 F7 | off: REPLY dev 0 ref 1E sec 00 item 06 data 1A 23 45 67 | on: rejected: checksum missing or wrong
    4.702  OUT  F0 47 5E 00 1F 00 06 1B 23 45 67 F7
    4.710  IN   F0 47 5E 00 1F 4F 00 06 F7 | off: OK dev 0 ref 1F sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.714  IN   F0 47 5E 00 1F 52 00 06 1B 23 45 67 F7 | off: REPLY dev 0 ref 1F sec 00 item 06 data 1B 23 45 67 | on: rejected: checksum missing or wrong
    4.756  OUT  F0 47 5E 00 20 00 06 1C 23 45 67 F7
    4.764  IN   F0 47 5E 00 20 4F 00 06 F7 | off: OK dev 0 ref 20 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.769  IN   F0 47 5E 00 20 52 00 06 1C 23 45 67 F7 | off: REPLY dev 0 ref 20 sec 00 item 06 data 1C 23 45 67 | on: rejected: checksum missing or wrong
    4.809  OUT  F0 47 5E 00 21 00 06 1D 23 45 67 F7
    4.817  IN   F0 47 5E 00 21 4F 00 06 F7 | off: OK dev 0 ref 21 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.822  IN   F0 47 5E 00 21 52 00 06 1D 23 45 67 F7 | off: REPLY dev 0 ref 21 sec 00 item 06 data 1D 23 45 67 | on: rejected: checksum missing or wrong
    4.860  OUT  F0 47 5E 00 22 00 06 1E 23 45 67 F7
    4.868  IN   F0 47 5E 00 22 4F 00 06 F7 | off: OK dev 0 ref 22 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.872  IN   F0 47 5E 00 22 52 00 06 1E 23 45 67 F7 | off: REPLY dev 0 ref 22 sec 00 item 06 data 1E 23 45 67 | on: rejected: checksum missing or wrong
    4.916  OUT  F0 47 5E 00 23 00 06 1F 23 45 67 F7
    4.924  IN   F0 47 5E 00 23 4F 00 06 F7 | off: OK dev 0 ref 23 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.928  IN   F0 47 5E 00 23 52 00 06 1F 23 45 67 F7 | off: REPLY dev 0 ref 23 sec 00 item 06 data 1F 23 45 67 | on: rejected: checksum missing or wrong
    4.969  OUT  F0 47 5E 00 24 00 06 20 23 45 67 F7
    4.978  IN   F0 47 5E 00 24 4F 00 06 F7 | off: OK dev 0 ref 24 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    4.982  IN   F0 47 5E 00 24 52 00 06 20 23 45 67 F7 | off: REPLY dev 0 ref 24 sec 00 item 06 data 20 23 45 67 | on: rejected: checksum missing or wrong
    5.021  OUT  F0 47 5E 00 25 00 06 21 23 45 67 F7
    5.029  IN   F0 47 5E 00 25 4F 00 06 F7 | off: OK dev 0 ref 25 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.033  IN   F0 47 5E 00 25 52 00 06 21 23 45 67 F7 | off: REPLY dev 0 ref 25 sec 00 item 06 data 21 23 45 67 | on: rejected: checksum missing or wrong
    5.072  OUT  F0 47 5E 00 26 00 06 22 23 45 67 F7
    5.080  IN   F0 47 5E 00 26 4F 00 06 F7 | off: OK dev 0 ref 26 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.084  IN   F0 47 5E 00 26 52 00 06 22 23 45 67 F7 | off: REPLY dev 0 ref 26 sec 00 item 06 data 22 23 45 67 | on: rejected: checksum missing or wrong
    5.127  OUT  F0 47 5E 00 27 00 06 23 23 45 67 F7
    5.136  IN   F0 47 5E 00 27 4F 00 06 F7 | off: OK dev 0 ref 27 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.139  IN   F0 47 5E 00 27 52 00 06 23 23 45 67 F7 | off: REPLY dev 0 ref 27 sec 00 item 06 data 23 23 45 67 | on: rejected: checksum missing or wrong
    5.178  OUT  F0 47 5E 00 28 00 06 24 23 45 67 F7
    5.187  IN   F0 47 5E 00 28 4F 00 06 F7 | off: OK dev 0 ref 28 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.191  IN   F0 47 5E 00 28 52 00 06 24 23 45 67 F7 | off: REPLY dev 0 ref 28 sec 00 item 06 data 24 23 45 67 | on: rejected: checksum missing or wrong
    5.231  OUT  F0 47 5E 00 29 00 06 25 23 45 67 F7
    5.239  IN   F0 47 5E 00 29 4F 00 06 F7 | off: OK dev 0 ref 29 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.244  IN   F0 47 5E 00 29 52 00 06 25 23 45 67 F7 | off: REPLY dev 0 ref 29 sec 00 item 06 data 25 23 45 67 | on: rejected: checksum missing or wrong
    5.281  OUT  F0 47 5E 00 2A 00 06 26 23 45 67 F7
    5.289  IN   F0 47 5E 00 2A 4F 00 06 F7 | off: OK dev 0 ref 2A sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.293  IN   F0 47 5E 00 2A 52 00 06 26 23 45 67 F7 | off: REPLY dev 0 ref 2A sec 00 item 06 data 26 23 45 67 | on: rejected: checksum missing or wrong
    5.334  OUT  F0 47 5E 00 2B 00 06 27 23 45 67 F7
    5.342  IN   F0 47 5E 00 2B 4F 00 06 F7 | off: OK dev 0 ref 2B sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.346  IN   F0 47 5E 00 2B 52 00 06 27 23 45 67 F7 | off: REPLY dev 0 ref 2B sec 00 item 06 data 27 23 45 67 | on: rejected: checksum missing or wrong
    5.386  OUT  F0 47 5E 00 2C 00 06 28 23 45 67 F7
    5.394  IN   F0 47 5E 00 2C 4F 00 06 F7 | off: OK dev 0 ref 2C sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.398  IN   F0 47 5E 00 2C 52 00 06 28 23 45 67 F7 | off: REPLY dev 0 ref 2C sec 00 item 06 data 28 23 45 67 | on: rejected: checksum missing or wrong
    5.451  OUT  F0 47 5E 00 2D 00 06 29 23 45 67 F7
    5.460  IN   F0 47 5E 00 2D 4F 00 06 F7 | off: OK dev 0 ref 2D sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.464  IN   F0 47 5E 00 2D 52 00 06 29 23 45 67 F7 | off: REPLY dev 0 ref 2D sec 00 item 06 data 29 23 45 67 | on: rejected: checksum missing or wrong
    5.504  OUT  F0 47 5E 00 2E 00 06 2A 23 45 67 F7
    5.512  IN   F0 47 5E 00 2E 4F 00 06 F7 | off: OK dev 0 ref 2E sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.516  IN   F0 47 5E 00 2E 52 00 06 2A 23 45 67 F7 | off: REPLY dev 0 ref 2E sec 00 item 06 data 2A 23 45 67 | on: rejected: checksum missing or wrong
    5.558  OUT  F0 47 5E 00 2F 00 06 2B 23 45 67 F7
    5.566  IN   F0 47 5E 00 2F 4F 00 06 F7 | off: OK dev 0 ref 2F sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.570  IN   F0 47 5E 00 2F 52 00 06 2B 23 45 67 F7 | off: REPLY dev 0 ref 2F sec 00 item 06 data 2B 23 45 67 | on: rejected: checksum missing or wrong
    5.611  OUT  F0 47 5E 00 30 00 06 2C 23 45 67 F7
    5.619  IN   F0 47 5E 00 30 4F 00 06 F7 | off: OK dev 0 ref 30 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.623  IN   F0 47 5E 00 30 52 00 06 2C 23 45 67 F7 | off: REPLY dev 0 ref 30 sec 00 item 06 data 2C 23 45 67 | on: rejected: checksum missing or wrong
    5.663  OUT  F0 47 5E 00 31 00 06 2D 23 45 67 F7
    5.671  IN   F0 47 5E 00 31 4F 00 06 F7 | off: OK dev 0 ref 31 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.675  IN   F0 47 5E 00 31 52 00 06 2D 23 45 67 F7 | off: REPLY dev 0 ref 31 sec 00 item 06 data 2D 23 45 67 | on: rejected: checksum missing or wrong
    5.715  OUT  F0 47 5E 00 32 00 06 2E 23 45 67 F7
    5.723  IN   F0 47 5E 00 32 4F 00 06 F7 | off: OK dev 0 ref 32 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.727  IN   F0 47 5E 00 32 52 00 06 2E 23 45 67 F7 | off: REPLY dev 0 ref 32 sec 00 item 06 data 2E 23 45 67 | on: rejected: checksum missing or wrong
    5.767  OUT  F0 47 5E 00 33 00 06 2F 23 45 67 F7
    5.775  IN   F0 47 5E 00 33 4F 00 06 F7 | off: OK dev 0 ref 33 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.779  IN   F0 47 5E 00 33 52 00 06 2F 23 45 67 F7 | off: REPLY dev 0 ref 33 sec 00 item 06 data 2F 23 45 67 | on: rejected: checksum missing or wrong
    5.822  OUT  F0 47 5E 00 34 00 06 30 23 45 67 F7
    5.830  IN   F0 47 5E 00 34 4F 00 06 F7 | off: OK dev 0 ref 34 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.834  IN   F0 47 5E 00 34 52 00 06 30 23 45 67 F7 | off: REPLY dev 0 ref 34 sec 00 item 06 data 30 23 45 67 | on: rejected: checksum missing or wrong
    5.870  OUT  F0 47 5E 00 35 00 06 31 23 45 67 F7
    5.878  IN   F0 47 5E 00 35 4F 00 06 F7 | off: OK dev 0 ref 35 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    5.882  IN   F0 47 5E 00 35 52 00 06 31 23 45 67 F7 | off: REPLY dev 0 ref 35 sec 00 item 06 data 31 23 45 67 | on: rejected: checksum missing or wrong
#   50 Echo round trips: min 12 ms, median 12 ms, 95th percentile 12 ms, max 13 ms
#   as expected: the slowest round trip is shorter than the command timeout in use (3000 ms)
#   closing: the session puts back the settings it changed
    5.946  OUT  F0 47 5E 00 36 00 04 00 3A F7
    5.953  IN   F0 47 5E 00 36 4F 00 04 F7 | off: OK dev 0 ref 36 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    5.956  IN   F0 47 5E 00 36 44 00 04 F7 | off: DONE dev 0 ref 36 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    5.956  OUT  F0 47 5E 00 37 00 07 00 F7
    5.963  IN   F0 47 5E 00 37 4F 00 07 F7 | off: OK dev 0 ref 37 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    5.966  IN   F0 47 5E 00 37 44 00 07 F7 | off: DONE dev 0 ref 37 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    5.966  OUT  F0 47 5E 00 38 00 03 01 F7
    5.973  IN   F0 47 5E 00 38 4F 00 03 F7 | off: OK dev 0 ref 38 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    5.976  IN   F0 47 5E 00 38 44 00 03 F7 | off: DONE dev 0 ref 38 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: Sync LCD
#   closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
#   as expected: every setting the session changed was put back (not put back: none)
#   as expected: the session is closed
#   PASSED: 50 Echo round trips: min 12 ms, median 12 ms, 95th percentile 12 ms, max 13 ms; closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
# check 4: the operating system version is read
    6.147  OUT  F0 47 5E 00 00 00 00 00 F7
    6.155  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    6.158  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    6.654  OUT  F0 47 5E 00 01 00 04 00 05 F7
    6.663  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    6.666  IN   F0 47 5E 00 01 44 00 04 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
    6.666  OUT  F0 47 5E 00 02 00 03 00 F7
    6.675  IN   F0 47 5E 00 02 4F 00 03 F7 | off: OK dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    6.678  IN   F0 47 5E 00 02 44 00 03 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    6.679  OUT  F0 47 5E 00 03 00 07 01 F7
    6.687  IN   F0 47 5E 00 03 4F 00 07 F7 | off: OK dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    6.690  IN   F0 47 5E 00 03 44 00 07 F7 | off: DONE dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    6.988  OUT  F0 47 5E 00 04 02 00 F7
    6.995  IN   F0 47 5E 00 04 4F 02 00 F7 | off: OK dev 0 ref 04 sec 02 item 00 data - | on: rejected: checksum missing or wrong
    6.998  IN   F0 47 5E 00 04 52 02 00 02 0E F7 | off: REPLY dev 0 ref 04 sec 02 item 00 data 02 0E | on: rejected: checksum missing or wrong
    6.998  OUT  F0 47 5E 00 05 02 01 F7
    7.005  IN   F0 47 5E 00 05 4F 02 01 F7 | off: OK dev 0 ref 05 sec 02 item 01 data - | on: rejected: checksum missing or wrong
    7.008  IN   F0 47 5E 00 05 52 02 01 00 F7 | off: REPLY dev 0 ref 05 sec 02 item 01 data 00 | on: rejected: checksum missing or wrong
#   OS version (section 02, items 00 and 01): OS 2.14, sub-version 0 after 20 ms
#   closing: the session puts back the settings it changed
    7.092  OUT  F0 47 5E 00 06 00 04 00 0A F7
    7.100  IN   F0 47 5E 00 06 4F 00 04 F7 | off: OK dev 0 ref 06 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    7.103  IN   F0 47 5E 00 06 44 00 04 F7 | off: DONE dev 0 ref 06 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    7.103  OUT  F0 47 5E 00 07 00 07 00 F7
    7.110  IN   F0 47 5E 00 07 4F 00 07 F7 | off: OK dev 0 ref 07 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    7.113  IN   F0 47 5E 00 07 44 00 07 F7 | off: DONE dev 0 ref 07 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    7.113  OUT  F0 47 5E 00 08 00 03 01 F7
    7.120  IN   F0 47 5E 00 08 4F 00 03 F7 | off: OK dev 0 ref 08 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    7.123  IN   F0 47 5E 00 08 44 00 03 F7 | off: DONE dev 0 ref 08 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: Sync LCD
#   closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
#   as expected: every setting the session changed was put back (not put back: none)
#   as expected: the session is closed
#   PASSED: OS version (section 02, items 00 and 01): OS 2.14, sub-version 0 after 20 ms; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# check 5: checksums on and off through the session
    7.288  OUT  F0 47 5E 00 00 00 00 00 F7
    7.295  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    7.298  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    7.802  OUT  F0 47 5E 00 01 00 04 00 05 F7
    7.811  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    7.814  IN   F0 47 5E 00 01 44 00 04 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
    7.815  OUT  F0 47 5E 00 02 00 03 00 F7
    7.824  IN   F0 47 5E 00 02 4F 00 03 F7 | off: OK dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    7.826  IN   F0 47 5E 00 02 44 00 03 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    7.827  OUT  F0 47 5E 00 03 00 07 01 F7
    7.836  IN   F0 47 5E 00 03 4F 00 07 F7 | off: OK dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    7.839  IN   F0 47 5E 00 03 44 00 07 F7 | off: DONE dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    8.140  OUT  F0 47 5E 00 04 00 04 01 09 F7
    8.147  IN   F0 47 5E 00 04 4F 00 04 F7 | off: OK dev 0 ref 04 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    8.150  IN   F0 47 5E 00 04 44 00 04 4C F7 | off: DONE dev 0 ref 04 sec 00 item 04 data 4C | on: DONE dev 0 ref 04 sec 00 item 04 data -
# diagnostic: checksum mode changed: on
#   checksums on: DONE after 10 ms
#   as expected: the session follows the sampler: checksums on
    8.203  OUT  F0 47 5E 00 05 00 06 01 23 45 67 5B F7
    8.211  IN   F0 47 5E 00 05 4F 00 06 5A F7 | off: OK dev 0 ref 05 sec 00 item 06 data 5A | on: OK dev 0 ref 05 sec 00 item 06 data -
    8.216  IN   F0 47 5E 00 05 52 00 06 01 23 45 67 2D F7 | off: REPLY dev 0 ref 05 sec 00 item 06 data 01 23 45 67 2D | on: REPLY dev 0 ref 05 sec 00 item 06 data 01 23 45 67
#   Echo with checksums on (the REPLY carries a checksum): REPLY 01 23 45 67 as sent after 13 ms
    8.281  OUT  F0 47 5E 00 06 02 00 08 F7
    8.289  IN   F0 47 5E 00 06 4F 02 00 57 F7 | off: OK dev 0 ref 06 sec 02 item 00 data 57 | on: OK dev 0 ref 06 sec 02 item 00 data -
    8.293  IN   F0 47 5E 00 06 52 02 00 02 0E 6A F7 | off: REPLY dev 0 ref 06 sec 02 item 00 data 02 0E 6A | on: REPLY dev 0 ref 06 sec 02 item 00 data 02 0E
    8.293  OUT  F0 47 5E 00 07 02 01 0A F7
    8.300  IN   F0 47 5E 00 07 4F 02 01 59 F7 | off: OK dev 0 ref 07 sec 02 item 01 data 59 | on: OK dev 0 ref 07 sec 02 item 01 data -
    8.304  IN   F0 47 5E 00 07 52 02 01 00 5C F7 | off: REPLY dev 0 ref 07 sec 02 item 01 data 00 5C | on: REPLY dev 0 ref 07 sec 02 item 01 data 00
#   OS version with checksums on: OS 2.14, sub-version 0 after 22 ms
    8.388  OUT  F0 47 5E 00 08 00 04 00 0C F7
    8.396  IN   F0 47 5E 00 08 4F 00 04 5B F7 | off: OK dev 0 ref 08 sec 00 item 04 data 5B | on: OK dev 0 ref 08 sec 00 item 04 data -
    8.398  IN   F0 47 5E 00 08 44 00 04 F7 | off: DONE dev 0 ref 08 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
#   checksums off again: DONE after 10 ms
#   as expected: the session follows the sampler: checksums off
    8.451  OUT  F0 47 5E 00 09 00 06 01 23 45 67 F7
    8.459  IN   F0 47 5E 00 09 4F 00 06 F7 | off: OK dev 0 ref 09 sec 00 item 06 data - | on: rejected: checksum missing or wrong
    8.463  IN   F0 47 5E 00 09 52 00 06 01 23 45 67 F7 | off: REPLY dev 0 ref 09 sec 00 item 06 data 01 23 45 67 | on: rejected: checksum missing or wrong
#   Echo with checksums off again: REPLY 01 23 45 67 as sent after 12 ms
#   closing: the session puts back the settings it changed
    8.507  OUT  F0 47 5E 00 0A 00 04 00 0E F7
    8.514  IN   F0 47 5E 00 0A 4F 00 04 F7 | off: OK dev 0 ref 0A sec 00 item 04 data - | on: rejected: checksum missing or wrong
    8.517  IN   F0 47 5E 00 0A 44 00 04 F7 | off: DONE dev 0 ref 0A sec 00 item 04 data - | on: rejected: checksum missing or wrong
    8.518  OUT  F0 47 5E 00 0B 00 07 00 F7
    8.525  IN   F0 47 5E 00 0B 4F 00 07 F7 | off: OK dev 0 ref 0B sec 00 item 07 data - | on: rejected: checksum missing or wrong
    8.528  IN   F0 47 5E 00 0B 44 00 07 F7 | off: DONE dev 0 ref 0B sec 00 item 07 data - | on: rejected: checksum missing or wrong
    8.528  OUT  F0 47 5E 00 0C 00 03 01 F7
    8.535  IN   F0 47 5E 00 0C 4F 00 03 F7 | off: OK dev 0 ref 0C sec 00 item 03 data - | on: rejected: checksum missing or wrong
    8.538  IN   F0 47 5E 00 0C 44 00 03 F7 | off: DONE dev 0 ref 0C sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: Sync LCD
#   closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
#   as expected: every setting the session changed was put back (not put back: none)
#   as expected: the session is closed
#   PASSED: checksums on: DONE after 10 ms; Echo with checksums on (the REPLY carries a checksum): REPLY 01 23 45 67 as sent after 13 ms; OS version with checksums on: OS 2.14, sub-version 0 after 22 ms; checksums off again: DONE after 10 ms; Echo with checksums off again: REPLY 01 23 45 67 as sent after 12 ms; closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
# check 6: closing puts back every setting the session changed
    8.721  OUT  F0 47 5E 00 00 00 00 00 F7
    8.729  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    8.731  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    9.235  OUT  F0 47 5E 00 01 00 04 01 06 F7
    9.244  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
    9.247  IN   F0 47 5E 00 01 44 00 04 49 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data 49 | on: DONE dev 0 ref 01 sec 00 item 04 data -
# diagnostic: checksum mode changed: on
    9.248  OUT  F0 47 5E 00 02 00 01 00 03 F7
    9.258  IN   F0 47 5E 00 02 4F 00 01 52 F7 | off: OK dev 0 ref 02 sec 00 item 01 data 52 | on: OK dev 0 ref 02 sec 00 item 01 data -
    9.260  IN   F0 47 5E 00 02 44 00 01 47 F7 | off: DONE dev 0 ref 02 sec 00 item 01 data 47 | on: DONE dev 0 ref 02 sec 00 item 01 data -
    9.261  OUT  F0 47 5E 00 03 00 03 00 06 F7
    9.270  IN   F0 47 5E 00 03 44 00 03 4A F7 | off: DONE dev 0 ref 03 sec 00 item 03 data 4A | on: DONE dev 0 ref 03 sec 00 item 03 data -
    9.271  OUT  F0 47 5E 00 04 00 05 01 0A F7
    9.280  IN   F0 47 5E 00 04 44 00 05 4D F7 | off: DONE dev 0 ref 04 sec 00 item 05 data 4D | on: DONE dev 0 ref 04 sec 00 item 05 data -
    9.280  OUT  F0 47 5E 00 05 00 07 01 0D F7
    9.290  IN   F0 47 5E 00 05 44 00 07 50 F7 | off: DONE dev 0 ref 05 sec 00 item 07 data 50 | on: DONE dev 0 ref 05 sec 00 item 07 data -
#   opened as ready with checksums on, Notification off, Still Alive on, Sync LCD off and Auto screen update on
#   as expected: the session follows the checksum mode it established: on
#   closing: the session puts back the settings it changed
    9.503  OUT  F0 47 5E 00 06 00 04 00 0A F7
    9.511  IN   F0 47 5E 00 06 44 00 04 F7 | off: DONE dev 0 ref 06 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
    9.511  OUT  F0 47 5E 00 07 00 07 00 F7
    9.518  IN   F0 47 5E 00 07 44 00 07 F7 | off: DONE dev 0 ref 07 sec 00 item 07 data - | on: rejected: checksum missing or wrong
    9.518  OUT  F0 47 5E 00 08 00 01 01 F7
    9.525  IN   F0 47 5E 00 08 44 00 01 F7 | off: DONE dev 0 ref 08 sec 00 item 01 data - | on: rejected: checksum missing or wrong
    9.525  OUT  F0 47 5E 00 09 00 03 01 F7
    9.532  IN   F0 47 5E 00 09 4F 00 03 F7 | off: OK dev 0 ref 09 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    9.536  IN   F0 47 5E 00 09 44 00 03 F7 | off: DONE dev 0 ref 09 sec 00 item 03 data - | on: rejected: checksum missing or wrong
    9.536  OUT  F0 47 5E 00 0A 00 05 00 F7
    9.543  IN   F0 47 5E 00 0A 4F 00 05 F7 | off: OK dev 0 ref 0A sec 00 item 05 data - | on: rejected: checksum missing or wrong
    9.546  IN   F0 47 5E 00 0A 44 00 05 F7 | off: DONE dev 0 ref 0A sec 00 item 05 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: notification
#   put back: Sync LCD
#   put back: Auto screen update
#   closed after 42 ms, put back: checksum mode, Still Alive, notification, Sync LCD, Auto screen update
#   as expected: every setting was put back (not put back: none)
#   as expected: the settings put back are the ones the open changed (checksum mode, notification, Still Alive, Sync LCD, Auto screen update)
#   PASSED: opened as ready with checksums on, Notification off, Still Alive on, Sync LCD off and Auto screen update on; closed after 42 ms, put back: checksum mode, Still Alive, notification, Sync LCD, Auto screen update
# check 7: a check that fails half way leaves the sampler in the known state
    9.765  OUT  F0 47 5E 00 00 00 00 00 F7
    9.772  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
    9.775  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
   10.277  OUT  F0 47 5E 00 01 00 04 01 06 F7
   10.287  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
   10.289  IN   F0 47 5E 00 01 44 00 04 49 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data 49 | on: DONE dev 0 ref 01 sec 00 item 04 data -
# diagnostic: checksum mode changed: on
   10.290  OUT  F0 47 5E 00 02 00 03 00 05 F7
   10.300  IN   F0 47 5E 00 02 4F 00 03 54 F7 | off: OK dev 0 ref 02 sec 00 item 03 data 54 | on: OK dev 0 ref 02 sec 00 item 03 data -
   10.302  IN   F0 47 5E 00 02 44 00 03 49 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data 49 | on: DONE dev 0 ref 02 sec 00 item 03 data -
   10.303  OUT  F0 47 5E 00 03 00 07 01 0B F7
   10.313  IN   F0 47 5E 00 03 4F 00 07 59 F7 | off: OK dev 0 ref 03 sec 00 item 07 data 59 | on: OK dev 0 ref 03 sec 00 item 07 data -
   10.315  IN   F0 47 5E 00 03 44 00 07 4E F7 | off: DONE dev 0 ref 03 sec 00 item 07 data 4E | on: DONE dev 0 ref 03 sec 00 item 07 data -
#   as expected: the session follows the checksum mode it established: on
#   the check ended with its session still open: the guard closes it
#   closing: the session puts back the settings it changed
   10.634  OUT  F0 47 5E 00 04 00 04 00 08 F7
   10.641  IN   F0 47 5E 00 04 4F 00 04 57 F7 | off: OK dev 0 ref 04 sec 00 item 04 data 57 | on: OK dev 0 ref 04 sec 00 item 04 data -
   10.644  IN   F0 47 5E 00 04 44 00 04 F7 | off: DONE dev 0 ref 04 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
   10.644  OUT  F0 47 5E 00 05 00 07 00 F7
   10.651  IN   F0 47 5E 00 05 4F 00 07 F7 | off: OK dev 0 ref 05 sec 00 item 07 data - | on: rejected: checksum missing or wrong
   10.654  IN   F0 47 5E 00 05 44 00 07 F7 | off: DONE dev 0 ref 05 sec 00 item 07 data - | on: rejected: checksum missing or wrong
   10.654  OUT  F0 47 5E 00 06 00 03 01 F7
   10.661  IN   F0 47 5E 00 06 4F 00 03 F7 | off: OK dev 0 ref 06 sec 00 item 03 data - | on: rejected: checksum missing or wrong
   10.664  IN   F0 47 5E 00 06 44 00 03 F7 | off: DONE dev 0 ref 06 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: Sync LCD
#   the check failed: this check fails on purpose, with the checksums on
#   as expected: the guard closed the session when the check failed
#   put back by the guard: checksum mode, Still Alive, Sync LCD
#   as expected: the checksum mode was set off and the sampler confirmed it with a DONE
#   as expected: every setting was put back (not put back: none)
#   PASSED: put back by the guard: checksum mode, Still Alive, Sync LCD
# check 8: select the test sample, round-trip every §0E lifecycle and settable-parameter item on it, and restore its name and parameters
   10.858  OUT  F0 47 5E 00 00 00 00 00 F7
   10.866  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
   10.869  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
   11.364  OUT  F0 47 5E 00 01 00 04 00 05 F7
   11.373  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
   11.376  IN   F0 47 5E 00 01 44 00 04 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
   11.377  OUT  F0 47 5E 00 02 00 03 00 F7
   11.386  IN   F0 47 5E 00 02 4F 00 03 F7 | off: OK dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
   11.388  IN   F0 47 5E 00 02 44 00 03 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
   11.389  OUT  F0 47 5E 00 03 00 07 01 F7
   11.398  IN   F0 47 5E 00 03 4F 00 07 F7 | off: OK dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
   11.400  IN   F0 47 5E 00 03 44 00 07 F7 | off: DONE dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
   11.401  OUT  F0 47 5E 00 04 0E 14 F7
   11.411  IN   F0 47 5E 00 04 4F 0E 14 F7 | off: OK dev 0 ref 04 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   11.414  IN   F0 47 5E 00 04 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 04 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   11.767  OUT  F0 47 5E 00 05 0E 05 48 6F 6E 65 73 74 79 00 F7
   11.777  IN   F0 47 5E 00 05 4F 0E 05 F7 | off: OK dev 0 ref 05 sec 0E item 05 data - | on: rejected: checksum missing or wrong
   11.780  IN   F0 47 5E 00 05 44 0E 05 F7 | off: DONE dev 0 ref 05 sec 0E item 05 data - | on: rejected: checksum missing or wrong
#   test sample "Honesty" selected and current
   11.780  OUT  F0 47 5E 00 06 0E 4B F7
   11.787  IN   F0 47 5E 00 06 4F 0E 4B F7 | off: OK dev 0 ref 06 sec 0E item 4B data - | on: rejected: checksum missing or wrong
   11.797  IN   F0 47 5E 00 06 52 0E 4B 00 00 00 00 00 26 28 41 3C 00 00 00 00 00 00 00 00 01 00 26 28 32 F7 | off: REPLY dev 0 ref 06 sec 0E item 4B data 00 00 00 00 00 26 28 41 3C 00 00 00 00 00 00 00 00 01 00 26 28 32 | on: rejected: checksum missing or wrong
#   sample "Honesty" selected as current
   11.801  OUT  F0 47 5E 00 07 0E 14 F7
   11.808  IN   F0 47 5E 00 07 4F 0E 14 F7 | off: OK dev 0 ref 07 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   11.813  IN   F0 47 5E 00 07 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 07 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   11.960  OUT  F0 47 5E 00 08 0E 09 48 6F 6E 65 73 74 79 5F 32 00 F7
   11.970  IN   F0 47 5E 00 08 4F 0E 09 F7 | off: OK dev 0 ref 08 sec 0E item 09 data - | on: rejected: checksum missing or wrong
   11.976  IN   F0 47 5E 00 08 44 0E 09 F7 | off: DONE dev 0 ref 08 sec 0E item 09 data - | on: rejected: checksum missing or wrong
#   rename the test sample: DONE after 15 ms
   11.977  OUT  F0 47 5E 00 09 0E 14 F7
   11.984  IN   F0 47 5E 00 09 4F 0E 14 F7 | off: OK dev 0 ref 09 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   11.990  IN   F0 47 5E 00 09 52 0E 14 48 6F 6E 65 73 74 79 5F 32 00 F7 | off: REPLY dev 0 ref 09 sec 0E item 14 data 48 6F 6E 65 73 74 79 5F 32 00 | on: rejected: checksum missing or wrong
#   as expected: the new name read back
   11.991  OUT  F0 47 5E 00 0A 0E 09 48 6F 6E 65 73 74 79 00 F7
   12.000  IN   F0 47 5E 00 0A 4F 0E 09 F7 | off: OK dev 0 ref 0A sec 0E item 09 data - | on: rejected: checksum missing or wrong
   12.005  IN   F0 47 5E 00 0A 44 0E 09 F7 | off: DONE dev 0 ref 0A sec 0E item 09 data - | on: rejected: checksum missing or wrong
   12.007  OUT  F0 47 5E 00 0B 0E 14 F7
   12.014  IN   F0 47 5E 00 0B 4F 0E 14 F7 | off: OK dev 0 ref 0B sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.019  IN   F0 47 5E 00 0B 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 0B sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   12.171  OUT  F0 47 5E 00 0C 0E 0A F7
   12.177  IN   F0 47 5E 00 0C 4F 0E 0A F7 | off: OK dev 0 ref 0C sec 0E item 0A data - | on: rejected: checksum missing or wrong
   12.181  IN   F0 47 5E 00 0C 44 0E 0A F7 | off: DONE dev 0 ref 0C sec 0E item 0A data - | on: rejected: checksum missing or wrong
#   start auditioning: DONE after 10 ms
   12.181  OUT  F0 47 5E 00 0D 0E 14 F7
   12.188  IN   F0 47 5E 00 0D 4F 0E 14 F7 | off: OK dev 0 ref 0D sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.193  IN   F0 47 5E 00 0D 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 0D sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   12.266  OUT  F0 47 5E 00 0E 0E 0B F7
   12.273  IN   F0 47 5E 00 0E 4F 0E 0B F7 | off: OK dev 0 ref 0E sec 0E item 0B data - | on: rejected: checksum missing or wrong
   12.275  IN   F0 47 5E 00 0E 44 0E 0B F7 | off: DONE dev 0 ref 0E sec 0E item 0B data - | on: rejected: checksum missing or wrong
#   stop auditioning: DONE after 9 ms
   12.280  OUT  F0 47 5E 00 0F 0E 14 F7
   12.287  IN   F0 47 5E 00 0F 4F 0E 14 F7 | off: OK dev 0 ref 0F sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.293  IN   F0 47 5E 00 0F 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 0F sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   12.371  OUT  F0 47 5E 00 10 0E 20 00 00 00 32 F7
   12.379  IN   F0 47 5E 00 10 4F 0E 20 F7 | off: OK dev 0 ref 10 sec 0E item 20 data - | on: rejected: checksum missing or wrong
   12.383  IN   F0 47 5E 00 10 44 0E 20 F7 | off: DONE dev 0 ref 10 sec 0E item 20 data - | on: rejected: checksum missing or wrong
#   set Set Start Position: DONE after 11 ms
   12.386  OUT  F0 47 5E 00 11 0E 40 F7
   12.393  IN   F0 47 5E 00 11 4F 0E 40 F7 | off: OK dev 0 ref 11 sec 0E item 40 data - | on: rejected: checksum missing or wrong
   12.397  IN   F0 47 5E 00 11 52 0E 40 00 00 00 32 F7 | off: REPLY dev 0 ref 11 sec 0E item 40 data 00 00 00 32 | on: rejected: checksum missing or wrong
   12.401  OUT  F0 47 5E 00 12 0E 14 F7
   12.408  IN   F0 47 5E 00 12 4F 0E 14 F7 | off: OK dev 0 ref 12 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.414  IN   F0 47 5E 00 12 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 12 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   12.540  OUT  F0 47 5E 00 13 0E 21 00 00 0F 50 F7
   12.548  IN   F0 47 5E 00 13 4F 0E 21 F7 | off: OK dev 0 ref 13 sec 0E item 21 data - | on: rejected: checksum missing or wrong
   12.552  IN   F0 47 5E 00 13 44 0E 21 F7 | off: DONE dev 0 ref 13 sec 0E item 21 data - | on: rejected: checksum missing or wrong
#   set Set End Position: DONE after 11 ms
   12.558  OUT  F0 47 5E 00 14 0E 41 F7
   12.565  IN   F0 47 5E 00 14 4F 0E 41 F7 | off: OK dev 0 ref 14 sec 0E item 41 data - | on: rejected: checksum missing or wrong
   12.570  IN   F0 47 5E 00 14 52 0E 41 00 00 0F 50 F7 | off: REPLY dev 0 ref 14 sec 0E item 41 data 00 00 0F 50 | on: rejected: checksum missing or wrong
   12.579  OUT  F0 47 5E 00 15 0E 14 F7
   12.586  IN   F0 47 5E 00 15 4F 0E 14 F7 | off: OK dev 0 ref 15 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.591  IN   F0 47 5E 00 15 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 15 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   12.710  OUT  F0 47 5E 00 16 0E 22 48 F7
   12.717  IN   F0 47 5E 00 16 4F 0E 22 F7 | off: OK dev 0 ref 16 sec 0E item 22 data - | on: rejected: checksum missing or wrong
   12.720  IN   F0 47 5E 00 16 44 0E 22 F7 | off: DONE dev 0 ref 16 sec 0E item 22 data - | on: rejected: checksum missing or wrong
#   set Set Original Pitch: DONE after 10 ms
   12.727  OUT  F0 47 5E 00 17 0E 42 F7
   12.735  IN   F0 47 5E 00 17 4F 0E 42 F7 | off: OK dev 0 ref 17 sec 0E item 42 data - | on: rejected: checksum missing or wrong
   12.737  IN   F0 47 5E 00 17 52 0E 42 48 F7 | off: REPLY dev 0 ref 17 sec 0E item 42 data 48 | on: rejected: checksum missing or wrong
   12.738  OUT  F0 47 5E 00 18 0E 14 F7
   12.745  IN   F0 47 5E 00 18 4F 0E 14 F7 | off: OK dev 0 ref 18 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.750  IN   F0 47 5E 00 18 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 18 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   12.874  OUT  F0 47 5E 00 19 0E 23 01 05 F7
   12.881  IN   F0 47 5E 00 19 4F 0E 23 F7 | off: OK dev 0 ref 19 sec 0E item 23 data - | on: rejected: checksum missing or wrong
   12.884  IN   F0 47 5E 00 19 44 0E 23 F7 | off: DONE dev 0 ref 19 sec 0E item 23 data - | on: rejected: checksum missing or wrong
#   set Set Semitone Tune: DONE after 10 ms
   12.891  OUT  F0 47 5E 00 1A 0E 43 F7
   12.898  IN   F0 47 5E 00 1A 4F 0E 43 F7 | off: OK dev 0 ref 1A sec 0E item 43 data - | on: rejected: checksum missing or wrong
   12.901  IN   F0 47 5E 00 1A 52 0E 43 01 05 F7 | off: REPLY dev 0 ref 1A sec 0E item 43 data 01 05 | on: rejected: checksum missing or wrong
   12.903  OUT  F0 47 5E 00 1B 0E 14 F7
   12.909  IN   F0 47 5E 00 1B 4F 0E 14 F7 | off: OK dev 0 ref 1B sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.915  IN   F0 47 5E 00 1B 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 1B sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   13.030  OUT  F0 47 5E 00 1C 0E 24 00 28 F7
   13.037  IN   F0 47 5E 00 1C 4F 0E 24 F7 | off: OK dev 0 ref 1C sec 0E item 24 data - | on: rejected: checksum missing or wrong
   13.040  IN   F0 47 5E 00 1C 44 0E 24 F7 | off: DONE dev 0 ref 1C sec 0E item 24 data - | on: rejected: checksum missing or wrong
#   set Set Fine Tune: DONE after 10 ms
   13.041  OUT  F0 47 5E 00 1D 0E 44 F7
   13.048  IN   F0 47 5E 00 1D 4F 0E 44 F7 | off: OK dev 0 ref 1D sec 0E item 44 data - | on: rejected: checksum missing or wrong
   13.051  IN   F0 47 5E 00 1D 52 0E 44 00 28 F7 | off: REPLY dev 0 ref 1D sec 0E item 44 data 00 28 | on: rejected: checksum missing or wrong
   13.052  OUT  F0 47 5E 00 1E 0E 14 F7
   13.060  IN   F0 47 5E 00 1E 4F 0E 14 F7 | off: OK dev 0 ref 1E sec 0E item 14 data - | on: rejected: checksum missing or wrong
   13.064  IN   F0 47 5E 00 1E 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 1E sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   13.180  OUT  F0 47 5E 00 1F 0E 28 01 F7
   13.187  IN   F0 47 5E 00 1F 4F 0E 28 F7 | off: OK dev 0 ref 1F sec 0E item 28 data - | on: rejected: checksum missing or wrong
   13.190  IN   F0 47 5E 00 1F 44 0E 28 F7 | off: DONE dev 0 ref 1F sec 0E item 28 data - | on: rejected: checksum missing or wrong
#   set Set Playback Mode: DONE after 10 ms
   13.191  OUT  F0 47 5E 00 20 0E 48 F7
   13.198  IN   F0 47 5E 00 20 4F 0E 48 F7 | off: OK dev 0 ref 20 sec 0E item 48 data - | on: rejected: checksum missing or wrong
   13.201  IN   F0 47 5E 00 20 52 0E 48 01 F7 | off: REPLY dev 0 ref 20 sec 0E item 48 data 01 | on: rejected: checksum missing or wrong
   13.202  OUT  F0 47 5E 00 21 0E 14 F7
   13.209  IN   F0 47 5E 00 21 4F 0E 14 F7 | off: OK dev 0 ref 21 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   13.215  IN   F0 47 5E 00 21 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 21 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   13.334  OUT  F0 47 5E 00 22 0E 2A 00 00 0B 5C F7
   13.342  IN   F0 47 5E 00 22 4F 0E 2A F7 | off: OK dev 0 ref 22 sec 0E item 2A data - | on: rejected: checksum missing or wrong
   13.345  IN   F0 47 5E 00 22 44 0E 2A F7 | off: DONE dev 0 ref 22 sec 0E item 2A data - | on: rejected: checksum missing or wrong
#   set Set Loop End: DONE after 12 ms
   13.347  OUT  F0 47 5E 00 23 0E 4A F7
   13.354  IN   F0 47 5E 00 23 4F 0E 4A F7 | off: OK dev 0 ref 23 sec 0E item 4A data - | on: rejected: checksum missing or wrong
   13.359  IN   F0 47 5E 00 23 52 0E 4A 00 00 0B 5C F7 | off: REPLY dev 0 ref 23 sec 0E item 4A data 00 00 0B 5C | on: rejected: checksum missing or wrong
   13.365  OUT  F0 47 5E 00 24 0E 14 F7
   13.372  IN   F0 47 5E 00 24 4F 0E 14 F7 | off: OK dev 0 ref 24 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   13.378  IN   F0 47 5E 00 24 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 24 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   13.491  OUT  F0 47 5E 00 25 0E 29 00 00 03 74 F7
   13.499  IN   F0 47 5E 00 25 4F 0E 29 F7 | off: OK dev 0 ref 25 sec 0E item 29 data - | on: rejected: checksum missing or wrong
   13.503  IN   F0 47 5E 00 25 44 0E 29 F7 | off: DONE dev 0 ref 25 sec 0E item 29 data - | on: rejected: checksum missing or wrong
#   set Set Loop Start: DONE after 11 ms
   13.504  OUT  F0 47 5E 00 26 0E 49 F7
   13.511  IN   F0 47 5E 00 26 4F 0E 49 F7 | off: OK dev 0 ref 26 sec 0E item 49 data - | on: rejected: checksum missing or wrong
   13.515  IN   F0 47 5E 00 26 52 0E 49 00 00 03 74 F7 | off: REPLY dev 0 ref 26 sec 0E item 49 data 00 00 03 74 | on: rejected: checksum missing or wrong
#   8 settable sample parameter items round-tripped
   13.518  OUT  F0 47 5E 00 27 0E 30 F7
   13.525  IN   F0 47 5E 00 27 4F 0E 30 F7 | off: OK dev 0 ref 27 sec 0E item 30 data - | on: rejected: checksum missing or wrong
   13.528  IN   F0 47 5E 00 27 52 0E 30 00 F7 | off: REPLY dev 0 ref 27 sec 0E item 30 data 00 | on: rejected: checksum missing or wrong
   13.534  OUT  F0 47 5E 00 28 0E 31 F7
   13.541  IN   F0 47 5E 00 28 4F 0E 31 F7 | off: OK dev 0 ref 28 sec 0E item 31 data - | on: rejected: checksum missing or wrong
   13.544  IN   F0 47 5E 00 28 52 0E 31 02 F7 | off: REPLY dev 0 ref 28 sec 0E item 31 data 02 | on: rejected: checksum missing or wrong
   13.546  OUT  F0 47 5E 00 29 0E 32 F7
   13.554  IN   F0 47 5E 00 29 4F 0E 32 F7 | off: OK dev 0 ref 29 sec 0E item 32 data - | on: rejected: checksum missing or wrong
   13.558  IN   F0 47 5E 00 29 52 0E 32 00 26 28 41 F7 | off: REPLY dev 0 ref 29 sec 0E item 32 data 00 26 28 41 | on: rejected: checksum missing or wrong
   13.566  OUT  F0 47 5E 00 2A 0E 33 F7
   13.572  IN   F0 47 5E 00 2A 4F 0E 33 F7 | off: OK dev 0 ref 2A sec 0E item 33 data - | on: rejected: checksum missing or wrong
   13.577  IN   F0 47 5E 00 2A 52 0E 33 00 02 58 44 F7 | off: REPLY dev 0 ref 2A sec 0E item 33 data 00 02 58 44 | on: rejected: checksum missing or wrong
   13.581  OUT  F0 47 5E 00 2B 0E 34 F7
   13.588  IN   F0 47 5E 00 2B 4F 0E 34 F7 | off: OK dev 0 ref 2B sec 0E item 34 data - | on: rejected: checksum missing or wrong
   13.594  IN   F0 47 5E 00 2B 52 0E 34 00 02 00 26 28 41 00 02 58 44 F7 | off: REPLY dev 0 ref 2B sec 0E item 34 data 00 02 00 26 28 41 00 02 58 44 | on: rejected: checksum missing or wrong
#   as expected: &34 decodes to the same values as &30-&33 read individually
   13.609  OUT  F0 47 5E 00 2C 0E 40 F7
   13.616  IN   F0 47 5E 00 2C 4F 0E 40 F7 | off: OK dev 0 ref 2C sec 0E item 40 data - | on: rejected: checksum missing or wrong
   13.620  IN   F0 47 5E 00 2C 52 0E 40 00 00 00 32 F7 | off: REPLY dev 0 ref 2C sec 0E item 40 data 00 00 00 32 | on: rejected: checksum missing or wrong
   13.625  OUT  F0 47 5E 00 2D 0E 41 F7
   13.632  IN   F0 47 5E 00 2D 4F 0E 41 F7 | off: OK dev 0 ref 2D sec 0E item 41 data - | on: rejected: checksum missing or wrong
   13.636  IN   F0 47 5E 00 2D 52 0E 41 00 00 0F 50 F7 | off: REPLY dev 0 ref 2D sec 0E item 41 data 00 00 0F 50 | on: rejected: checksum missing or wrong
   13.640  OUT  F0 47 5E 00 2E 0E 42 F7
   13.647  IN   F0 47 5E 00 2E 4F 0E 42 F7 | off: OK dev 0 ref 2E sec 0E item 42 data - | on: rejected: checksum missing or wrong
   13.650  IN   F0 47 5E 00 2E 52 0E 42 48 F7 | off: REPLY dev 0 ref 2E sec 0E item 42 data 48 | on: rejected: checksum missing or wrong
   13.666  OUT  F0 47 5E 00 2F 0E 43 F7
   13.673  IN   F0 47 5E 00 2F 4F 0E 43 F7 | off: OK dev 0 ref 2F sec 0E item 43 data - | on: rejected: checksum missing or wrong
   13.676  IN   F0 47 5E 00 2F 52 0E 43 01 05 F7 | off: REPLY dev 0 ref 2F sec 0E item 43 data 01 05 | on: rejected: checksum missing or wrong
   13.677  OUT  F0 47 5E 00 30 0E 44 F7
   13.684  IN   F0 47 5E 00 30 4F 0E 44 F7 | off: OK dev 0 ref 30 sec 0E item 44 data - | on: rejected: checksum missing or wrong
   13.687  IN   F0 47 5E 00 30 52 0E 44 00 28 F7 | off: REPLY dev 0 ref 30 sec 0E item 44 data 00 28 | on: rejected: checksum missing or wrong
   13.710  OUT  F0 47 5E 00 31 0E 48 F7
   13.717  IN   F0 47 5E 00 31 4F 0E 48 F7 | off: OK dev 0 ref 31 sec 0E item 48 data - | on: rejected: checksum missing or wrong
   13.720  IN   F0 47 5E 00 31 52 0E 48 01 F7 | off: REPLY dev 0 ref 31 sec 0E item 48 data 01 | on: rejected: checksum missing or wrong
   13.725  OUT  F0 47 5E 00 32 0E 49 F7
   13.733  IN   F0 47 5E 00 32 4F 0E 49 F7 | off: OK dev 0 ref 32 sec 0E item 49 data - | on: rejected: checksum missing or wrong
   13.737  IN   F0 47 5E 00 32 52 0E 49 00 00 03 74 F7 | off: REPLY dev 0 ref 32 sec 0E item 49 data 00 00 03 74 | on: rejected: checksum missing or wrong
   13.738  OUT  F0 47 5E 00 33 0E 4A F7
   13.746  IN   F0 47 5E 00 33 4F 0E 4A F7 | off: OK dev 0 ref 33 sec 0E item 4A data - | on: rejected: checksum missing or wrong
   13.750  IN   F0 47 5E 00 33 52 0E 4A 00 00 0B 5C F7 | off: REPLY dev 0 ref 33 sec 0E item 4A data 00 00 0B 5C | on: rejected: checksum missing or wrong
   13.752  OUT  F0 47 5E 00 34 0E 4B F7
   13.760  IN   F0 47 5E 00 34 4F 0E 4B F7 | off: OK dev 0 ref 34 sec 0E item 4B data - | on: rejected: checksum missing or wrong
   13.770  IN   F0 47 5E 00 34 52 0E 4B 00 00 00 32 00 00 0F 50 48 01 05 00 28 01 00 00 03 74 00 00 0B 5C F7 | off: REPLY dev 0 ref 34 sec 0E item 4B data 00 00 00 32 00 00 0F 50 48 01 05 00 28 01 00 00 03 74 00 00 0B 5C | on: rejected: checksum missing or wrong
#   as expected: &4B decodes to the same values as &40-&4A read individually
#   no sample was current before: the wrong-sample refusal is not exercised this run
   13.772  OUT  F0 47 5E 00 35 0E 14 F7
   13.780  IN   F0 47 5E 00 35 4F 0E 14 F7 | off: OK dev 0 ref 35 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   13.785  IN   F0 47 5E 00 35 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 35 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   13.809  OUT  F0 47 5E 00 36 0E 14 F7
   13.818  IN   F0 47 5E 00 36 4F 0E 14 F7 | off: OK dev 0 ref 36 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   13.823  IN   F0 47 5E 00 36 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 36 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   13.826  OUT  F0 47 5E 00 37 0E 20 00 00 00 00 F7
   13.837  IN   F0 47 5E 00 37 4F 0E 20 F7 | off: OK dev 0 ref 37 sec 0E item 20 data - | on: rejected: checksum missing or wrong
   13.839  IN   F0 47 5E 00 37 44 0E 20 F7 | off: DONE dev 0 ref 37 sec 0E item 20 data - | on: rejected: checksum missing or wrong
#   restore Set Start Position: done
   13.867  OUT  F0 47 5E 00 38 0E 21 00 26 28 41 F7
   13.878  IN   F0 47 5E 00 38 4F 0E 21 F7 | off: OK dev 0 ref 38 sec 0E item 21 data - | on: rejected: checksum missing or wrong
   13.880  IN   F0 47 5E 00 38 44 0E 21 F7 | off: DONE dev 0 ref 38 sec 0E item 21 data - | on: rejected: checksum missing or wrong
#   restore Set End Position: done
   13.882  OUT  F0 47 5E 00 39 0E 22 3C F7
   13.891  IN   F0 47 5E 00 39 4F 0E 22 F7 | off: OK dev 0 ref 39 sec 0E item 22 data - | on: rejected: checksum missing or wrong
   13.894  IN   F0 47 5E 00 39 44 0E 22 F7 | off: DONE dev 0 ref 39 sec 0E item 22 data - | on: rejected: checksum missing or wrong
#   restore Set Original Pitch: done
   13.896  OUT  F0 47 5E 00 3A 0E 23 00 00 F7
   13.905  IN   F0 47 5E 00 3A 4F 0E 23 F7 | off: OK dev 0 ref 3A sec 0E item 23 data - | on: rejected: checksum missing or wrong
   13.907  IN   F0 47 5E 00 3A 44 0E 23 F7 | off: DONE dev 0 ref 3A sec 0E item 23 data - | on: rejected: checksum missing or wrong
#   restore Set Semitone Tune: done
   13.908  OUT  F0 47 5E 00 3B 0E 24 00 00 F7
   13.917  IN   F0 47 5E 00 3B 4F 0E 24 F7 | off: OK dev 0 ref 3B sec 0E item 24 data - | on: rejected: checksum missing or wrong
   13.920  IN   F0 47 5E 00 3B 44 0E 24 F7 | off: DONE dev 0 ref 3B sec 0E item 24 data - | on: rejected: checksum missing or wrong
#   restore Set Fine Tune: done
   13.921  OUT  F0 47 5E 00 3C 0E 28 00 F7
   13.930  IN   F0 47 5E 00 3C 4F 0E 28 F7 | off: OK dev 0 ref 3C sec 0E item 28 data - | on: rejected: checksum missing or wrong
   13.934  IN   F0 47 5E 00 3C 44 0E 28 F7 | off: DONE dev 0 ref 3C sec 0E item 28 data - | on: rejected: checksum missing or wrong
#   restore Set Playback Mode: done
   13.941  OUT  F0 47 5E 00 3D 0E 2A 00 26 28 32 F7
   13.951  IN   F0 47 5E 00 3D 4F 0E 2A F7 | off: OK dev 0 ref 3D sec 0E item 2A data - | on: rejected: checksum missing or wrong
   13.954  IN   F0 47 5E 00 3D 44 0E 2A F7 | off: DONE dev 0 ref 3D sec 0E item 2A data - | on: rejected: checksum missing or wrong
#   restore Set Loop End: done
   13.968  OUT  F0 47 5E 00 3E 0E 29 00 00 00 01 F7
   13.977  IN   F0 47 5E 00 3E 4F 0E 29 F7 | off: OK dev 0 ref 3E sec 0E item 29 data - | on: rejected: checksum missing or wrong
   13.981  IN   F0 47 5E 00 3E 44 0E 29 F7 | off: DONE dev 0 ref 3E sec 0E item 29 data - | on: rejected: checksum missing or wrong
#   restore Set Loop Start: done
#   test sample's name and settable parameters restored by the guard
   13.983  OUT  F0 47 5E 00 3F 0E 05 48 6F 6E 65 73 74 79 00 F7
   13.994  IN   F0 47 5E 00 3F 4F 0E 05 F7 | off: OK dev 0 ref 3F sec 0E item 05 data - | on: rejected: checksum missing or wrong
   13.996  IN   F0 47 5E 00 3F 44 0E 05 F7 | off: DONE dev 0 ref 3F sec 0E item 05 data - | on: rejected: checksum missing or wrong
#   as expected: the test sample can still be selected by its original name
   13.998  OUT  F0 47 5E 00 40 0E 14 F7
   14.008  IN   F0 47 5E 00 40 4F 0E 14 F7 | off: OK dev 0 ref 40 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   14.012  IN   F0 47 5E 00 40 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 40 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
#   as expected: its name is back to "Honesty"
   14.014  OUT  F0 47 5E 00 41 0E 4B F7
   14.022  IN   F0 47 5E 00 41 4F 0E 4B F7 | off: OK dev 0 ref 41 sec 0E item 4B data - | on: rejected: checksum missing or wrong
   14.031  IN   F0 47 5E 00 41 52 0E 4B 00 00 00 00 00 26 28 41 3C 00 00 00 00 00 00 00 00 01 00 26 28 32 F7 | off: REPLY dev 0 ref 41 sec 0E item 4B data 00 00 00 00 00 26 28 41 3C 00 00 00 00 00 00 00 00 01 00 26 28 32 | on: rejected: checksum missing or wrong
#   as expected: its settable parameters are back to what they were before
#   closing: the session puts back the settings it changed
   15.204  OUT  F0 47 5E 00 42 00 04 00 46 F7
   15.211  IN   F0 47 5E 00 42 4F 00 04 F7 | off: OK dev 0 ref 42 sec 00 item 04 data - | on: rejected: checksum missing or wrong
   15.214  IN   F0 47 5E 00 42 44 00 04 F7 | off: DONE dev 0 ref 42 sec 00 item 04 data - | on: rejected: checksum missing or wrong
   15.215  OUT  F0 47 5E 00 43 00 07 00 F7
   15.222  IN   F0 47 5E 00 43 4F 00 07 F7 | off: OK dev 0 ref 43 sec 00 item 07 data - | on: rejected: checksum missing or wrong
   15.225  IN   F0 47 5E 00 43 44 00 07 F7 | off: DONE dev 0 ref 43 sec 00 item 07 data - | on: rejected: checksum missing or wrong
   15.225  OUT  F0 47 5E 00 44 00 03 01 F7
   15.232  IN   F0 47 5E 00 44 4F 00 03 F7 | off: OK dev 0 ref 44 sec 00 item 03 data - | on: rejected: checksum missing or wrong
   15.235  IN   F0 47 5E 00 44 44 00 03 F7 | off: DONE dev 0 ref 44 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: Sync LCD
#   closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
#   as expected: every setting the session changed was put back (not put back: none)
#   as expected: the session is closed
#   PASSED: sample "Honesty" selected as current; 8 settable sample parameter items round-tripped; no sample was current before: the wrong-sample refusal is not exercised this run; test sample's name and settable parameters restored by the guard; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# observations
# observation: check 1 PASSED: open a session and close it - opened as ready after 552 ms; DeviceIDs that answered the discovery: 0; closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 2 PASSED: Echo returns the bytes sent - Echo 01 23 45 67: REPLY 01 23 45 67 as sent after 12 ms; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 3 PASSED: 50 Echo round trips, timed - 50 Echo round trips: min 12 ms, median 12 ms, 95th percentile 12 ms, max 13 ms; closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 4 PASSED: the operating system version is read - OS version (section 02, items 00 and 01): OS 2.14, sub-version 0 after 20 ms; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 5 PASSED: checksums on and off through the session - checksums on: DONE after 10 ms; Echo with checksums on (the REPLY carries a checksum): REPLY 01 23 45 67 as sent after 13 ms; OS version with checksums on: OS 2.14, sub-version 0 after 22 ms; checksums off again: DONE after 10 ms; Echo with checksums off again: REPLY 01 23 45 67 as sent after 12 ms; closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 6 PASSED: closing puts back every setting the session changed - opened as ready with checksums on, Notification off, Still Alive on, Sync LCD off and Auto screen update on; closed after 42 ms, put back: checksum mode, Still Alive, notification, Sync LCD, Auto screen update
# observation: check 7 PASSED: a check that fails half way leaves the sampler in the known state - put back by the guard: checksum mode, Still Alive, Sync LCD
# observation: check 8 PASSED: select the test sample, round-trip every §0E lifecycle and settable-parameter item on it, and restore its name and parameters - sample "Honesty" selected as current; 8 settable sample parameter items round-tripped; no sample was current before: the wrong-sample refusal is not exercised this run; test sample's name and settable parameters restored by the guard; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: 8 checks passed, 0 failed, 0 skipped
# observation: discovery answered by DeviceIDs: 0; the target DeviceID is 0, and the session accepts only confirmations that carry it
# observation: OS version 2.14 (sub-version 0)
# observation: 50 Echo round trips of 50: min 12 ms, median 12 ms, 95th percentile 12 ms, max 13 ms
# observation: F0 F7 messages seen: 0
# observation: messages rejected by the session: 0; unsolicited confirmations: 0; late ERRORs after a REPLY: 0
# observation: checksum mode as the sessions followed it: unknown -> off -> off -> off -> off -> off -> on -> off -> on -> off -> on -> off -> off
# observation: sampler left in the known state: checksums off, Still Alive off, Notification on, Sync LCD on, Auto screen update off
# observation: frames sent 181, received 356
```

### `amen.log` (first failing run — unrealistic test values, F1)

Full log kept at `juce/build/tests/probe/Debug/amen.log`. Check 8 excerpt (the rest mirrors the passing
run's checks 1–7):

```text
# check 8: select the test sample, round-trip every §0E lifecycle and settable-parameter item on it, and restore its name and parameters
   10.875  OUT  F0 47 5E 00 00 00 00 00 F7
   10.882  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
   10.885  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
   11.383  OUT  F0 47 5E 00 01 00 04 00 05 F7
   11.392  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
   11.394  IN   F0 47 5E 00 01 44 00 04 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
   11.395  OUT  F0 47 5E 00 02 00 03 00 F7
   11.404  IN   F0 47 5E 00 02 4F 00 03 F7 | off: OK dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
   11.406  IN   F0 47 5E 00 02 44 00 03 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
   11.407  OUT  F0 47 5E 00 03 00 07 01 F7
   11.417  IN   F0 47 5E 00 03 4F 00 07 F7 | off: OK dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
   11.419  IN   F0 47 5E 00 03 44 00 07 F7 | off: DONE dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
   11.421  OUT  F0 47 5E 00 04 0E 14 F7
   11.430  IN   F0 47 5E 00 04 4F 0E 14 F7 | off: OK dev 0 ref 04 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   11.434  IN   F0 47 5E 00 04 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 04 sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
   11.818  OUT  F0 47 5E 00 05 0E 05 41 4D 45 4E 00 F7
   11.826  IN   F0 47 5E 00 05 4F 0E 05 F7 | off: OK dev 0 ref 05 sec 0E item 05 data - | on: rejected: checksum missing or wrong
   11.830  IN   F0 47 5E 00 05 44 0E 05 F7 | off: DONE dev 0 ref 05 sec 0E item 05 data - | on: rejected: checksum missing or wrong
#   test sample "AMEN" selected and current
   11.832  OUT  F0 47 5E 00 06 0E 4B F7
   11.840  IN   F0 47 5E 00 06 4F 0E 4B F7 | off: OK dev 0 ref 06 sec 0E item 4B data - | on: rejected: checksum missing or wrong
   11.850  IN   F0 47 5E 00 06 52 0E 4B 00 00 00 00 00 08 5E 41 3C 00 00 00 00 00 00 00 00 01 00 08 5E 31 F7 | off: REPLY dev 0 ref 06 sec 0E item 4B data 00 00 00 00 00 08 5E 41 3C 00 00 00 00 00 00 00 00 01 00 08 5E 31 | on: rejected: checksum missing or wrong
#   sample "AMEN" selected as current
   11.850  OUT  F0 47 5E 00 07 0E 14 F7
   11.857  IN   F0 47 5E 00 07 4F 0E 14 F7 | off: OK dev 0 ref 07 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   11.862  IN   F0 47 5E 00 07 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 07 sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
   11.993  OUT  F0 47 5E 00 08 0E 09 41 4D 45 4E 5F 32 00 F7
   12.001  IN   F0 47 5E 00 08 4F 0E 09 F7 | off: OK dev 0 ref 08 sec 0E item 09 data - | on: rejected: checksum missing or wrong
   12.007  IN   F0 47 5E 00 08 44 0E 09 F7 | off: DONE dev 0 ref 08 sec 0E item 09 data - | on: rejected: checksum missing or wrong
#   rename the test sample: DONE after 14 ms
   12.009  OUT  F0 47 5E 00 09 0E 14 F7
   12.016  IN   F0 47 5E 00 09 4F 0E 14 F7 | off: OK dev 0 ref 09 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.021  IN   F0 47 5E 00 09 52 0E 14 41 4D 45 4E 5F 32 00 F7 | off: REPLY dev 0 ref 09 sec 0E item 14 data 41 4D 45 4E 5F 32 00 | on: rejected: checksum missing or wrong
#   as expected: the new name read back
   12.023  OUT  F0 47 5E 00 0A 0E 09 41 4D 45 4E 00 F7
   12.031  IN   F0 47 5E 00 0A 4F 0E 09 F7 | off: OK dev 0 ref 0A sec 0E item 09 data - | on: rejected: checksum missing or wrong
   12.036  IN   F0 47 5E 00 0A 44 0E 09 F7 | off: DONE dev 0 ref 0A sec 0E item 09 data - | on: rejected: checksum missing or wrong
   12.051  OUT  F0 47 5E 00 0B 0E 14 F7
   12.057  IN   F0 47 5E 00 0B 4F 0E 14 F7 | off: OK dev 0 ref 0B sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.062  IN   F0 47 5E 00 0B 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 0B sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
   12.220  OUT  F0 47 5E 00 0C 0E 0A F7
   12.227  IN   F0 47 5E 00 0C 4F 0E 0A F7 | off: OK dev 0 ref 0C sec 0E item 0A data - | on: rejected: checksum missing or wrong
   12.230  IN   F0 47 5E 00 0C 44 0E 0A F7 | off: DONE dev 0 ref 0C sec 0E item 0A data - | on: rejected: checksum missing or wrong
#   start auditioning: DONE after 9 ms
   12.230  OUT  F0 47 5E 00 0D 0E 14 F7
   12.237  IN   F0 47 5E 00 0D 4F 0E 14 F7 | off: OK dev 0 ref 0D sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.241  IN   F0 47 5E 00 0D 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 0D sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
   12.315  OUT  F0 47 5E 00 0E 0E 0B F7
   12.321  IN   F0 47 5E 00 0E 4F 0E 0B F7 | off: OK dev 0 ref 0E sec 0E item 0B data - | on: rejected: checksum missing or wrong
   12.325  IN   F0 47 5E 00 0E 44 0E 0B F7 | off: DONE dev 0 ref 0E sec 0E item 0B data - | on: rejected: checksum missing or wrong
#   stop auditioning: DONE after 10 ms
   12.336  OUT  F0 47 5E 00 0F 0E 14 F7
   12.342  IN   F0 47 5E 00 0F 4F 0E 14 F7 | off: OK dev 0 ref 0F sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.347  IN   F0 47 5E 00 0F 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 0F sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
   12.435  OUT  F0 47 5E 00 10 0E 20 00 00 00 32 F7
   12.443  IN   F0 47 5E 00 10 4F 0E 20 F7 | off: OK dev 0 ref 10 sec 0E item 20 data - | on: rejected: checksum missing or wrong
   12.446  IN   F0 47 5E 00 10 44 0E 20 F7 | off: DONE dev 0 ref 10 sec 0E item 20 data - | on: rejected: checksum missing or wrong
#   set Set Start Position: DONE after 12 ms
   12.448  OUT  F0 47 5E 00 11 0E 40 F7
   12.455  IN   F0 47 5E 00 11 4F 0E 40 F7 | off: OK dev 0 ref 11 sec 0E item 40 data - | on: rejected: checksum missing or wrong
   12.459  IN   F0 47 5E 00 11 52 0E 40 00 00 00 32 F7 | off: REPLY dev 0 ref 11 sec 0E item 40 data 00 00 00 32 | on: rejected: checksum missing or wrong
   12.461  OUT  F0 47 5E 00 12 0E 14 F7
   12.468  IN   F0 47 5E 00 12 4F 0E 14 F7 | off: OK dev 0 ref 12 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.473  IN   F0 47 5E 00 12 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 12 sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
   12.588  OUT  F0 47 5E 00 13 0E 21 00 00 0F 50 F7
   12.596  IN   F0 47 5E 00 13 4F 0E 21 F7 | off: OK dev 0 ref 13 sec 0E item 21 data - | on: rejected: checksum missing or wrong
   12.600  IN   F0 47 5E 00 13 44 0E 21 F7 | off: DONE dev 0 ref 13 sec 0E item 21 data - | on: rejected: checksum missing or wrong
#   set Set End Position: DONE after 11 ms
   12.603  OUT  F0 47 5E 00 14 0E 41 F7
   12.610  IN   F0 47 5E 00 14 4F 0E 41 F7 | off: OK dev 0 ref 14 sec 0E item 41 data - | on: rejected: checksum missing or wrong
   12.613  IN   F0 47 5E 00 14 52 0E 41 00 00 0F 50 F7 | off: REPLY dev 0 ref 14 sec 0E item 41 data 00 00 0F 50 | on: rejected: checksum missing or wrong
   12.615  OUT  F0 47 5E 00 15 0E 14 F7
   12.621  IN   F0 47 5E 00 15 4F 0E 14 F7 | off: OK dev 0 ref 15 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.626  IN   F0 47 5E 00 15 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 15 sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
   12.738  OUT  F0 47 5E 00 16 0E 22 48 F7
   12.745  IN   F0 47 5E 00 16 4F 0E 22 F7 | off: OK dev 0 ref 16 sec 0E item 22 data - | on: rejected: checksum missing or wrong
   12.748  IN   F0 47 5E 00 16 44 0E 22 F7 | off: DONE dev 0 ref 16 sec 0E item 22 data - | on: rejected: checksum missing or wrong
#   set Set Original Pitch: DONE after 10 ms
   12.756  OUT  F0 47 5E 00 17 0E 42 F7
   12.764  IN   F0 47 5E 00 17 4F 0E 42 F7 | off: OK dev 0 ref 17 sec 0E item 42 data - | on: rejected: checksum missing or wrong
   12.767  IN   F0 47 5E 00 17 52 0E 42 48 F7 | off: REPLY dev 0 ref 17 sec 0E item 42 data 48 | on: rejected: checksum missing or wrong
   12.768  OUT  F0 47 5E 00 18 0E 14 F7
   12.775  IN   F0 47 5E 00 18 4F 0E 14 F7 | off: OK dev 0 ref 18 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.779  IN   F0 47 5E 00 18 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 18 sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
   12.890  OUT  F0 47 5E 00 19 0E 23 01 05 F7
   12.897  IN   F0 47 5E 00 19 4F 0E 23 F7 | off: OK dev 0 ref 19 sec 0E item 23 data - | on: rejected: checksum missing or wrong
   12.900  IN   F0 47 5E 00 19 44 0E 23 F7 | off: DONE dev 0 ref 19 sec 0E item 23 data - | on: rejected: checksum missing or wrong
#   set Set Semitone Tune: DONE after 10 ms
   12.905  OUT  F0 47 5E 00 1A 0E 43 F7
   12.912  IN   F0 47 5E 00 1A 4F 0E 43 F7 | off: OK dev 0 ref 1A sec 0E item 43 data - | on: rejected: checksum missing or wrong
   12.916  IN   F0 47 5E 00 1A 52 0E 43 01 05 F7 | off: REPLY dev 0 ref 1A sec 0E item 43 data 01 05 | on: rejected: checksum missing or wrong
   12.922  OUT  F0 47 5E 00 1B 0E 14 F7
   12.930  IN   F0 47 5E 00 1B 4F 0E 14 F7 | off: OK dev 0 ref 1B sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.934  IN   F0 47 5E 00 1B 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 1B sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
   13.044  OUT  F0 47 5E 00 1C 0E 24 00 28 F7
   13.051  IN   F0 47 5E 00 1C 4F 0E 24 F7 | off: OK dev 0 ref 1C sec 0E item 24 data - | on: rejected: checksum missing or wrong
   13.054  IN   F0 47 5E 00 1C 44 0E 24 F7 | off: DONE dev 0 ref 1C sec 0E item 24 data - | on: rejected: checksum missing or wrong
#   set Set Fine Tune: DONE after 10 ms
   13.055  OUT  F0 47 5E 00 1D 0E 44 F7
   13.062  IN   F0 47 5E 00 1D 4F 0E 44 F7 | off: OK dev 0 ref 1D sec 0E item 44 data - | on: rejected: checksum missing or wrong
   13.066  IN   F0 47 5E 00 1D 52 0E 44 00 28 F7 | off: REPLY dev 0 ref 1D sec 0E item 44 data 00 28 | on: rejected: checksum missing or wrong
   13.068  OUT  F0 47 5E 00 1E 0E 14 F7
   13.075  IN   F0 47 5E 00 1E 4F 0E 14 F7 | off: OK dev 0 ref 1E sec 0E item 14 data - | on: rejected: checksum missing or wrong
   13.080  IN   F0 47 5E 00 1E 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 1E sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
   13.191  OUT  F0 47 5E 00 1F 0E 28 01 F7
   13.198  IN   F0 47 5E 00 1F 4F 0E 28 F7 | off: OK dev 0 ref 1F sec 0E item 28 data - | on: rejected: checksum missing or wrong
   13.202  IN   F0 47 5E 00 1F 44 0E 28 F7 | off: DONE dev 0 ref 1F sec 0E item 28 data - | on: rejected: checksum missing or wrong
#   set Set Playback Mode: DONE after 11 ms
   13.204  OUT  F0 47 5E 00 20 0E 48 F7
   13.211  IN   F0 47 5E 00 20 4F 0E 48 F7 | off: OK dev 0 ref 20 sec 0E item 48 data - | on: rejected: checksum missing or wrong
   13.214  IN   F0 47 5E 00 20 52 0E 48 01 F7 | off: REPLY dev 0 ref 20 sec 0E item 48 data 01 | on: rejected: checksum missing or wrong
   13.215  OUT  F0 47 5E 00 21 0E 14 F7
   13.222  IN   F0 47 5E 00 21 4F 0E 14 F7 | off: OK dev 0 ref 21 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   13.227  IN   F0 47 5E 00 21 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 21 sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
   13.350  OUT  F0 47 5E 00 22 0E 29 00 00 03 74 F7
   13.358  IN   F0 47 5E 00 22 4F 0E 29 F7 | off: OK dev 0 ref 22 sec 0E item 29 data - | on: rejected: checksum missing or wrong
   13.361  IN   F0 47 5E 00 22 44 0E 29 F7 | off: DONE dev 0 ref 22 sec 0E item 29 data - | on: rejected: checksum missing or wrong
#   set Set Loop Start: DONE after 11 ms
   13.366  OUT  F0 47 5E 00 23 0E 49 F7
   13.373  IN   F0 47 5E 00 23 4F 0E 49 F7 | off: OK dev 0 ref 23 sec 0E item 49 data - | on: rejected: checksum missing or wrong
   13.377  IN   F0 47 5E 00 23 52 0E 49 00 00 03 74 F7 | off: REPLY dev 0 ref 23 sec 0E item 49 data 00 00 03 74 | on: rejected: checksum missing or wrong
   13.377  OUT  F0 47 5E 00 24 0E 14 F7
   13.384  IN   F0 47 5E 00 24 4F 0E 14 F7 | off: OK dev 0 ref 24 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   13.388  IN   F0 47 5E 00 24 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 24 sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
   13.508  OUT  F0 47 5E 00 25 0E 2A 00 00 0B 5C F7
   13.516  IN   F0 47 5E 00 25 4F 0E 2A F7 | off: OK dev 0 ref 25 sec 0E item 2A data - | on: rejected: checksum missing or wrong
   13.519  IN   F0 47 5E 00 25 44 0E 2A F7 | off: DONE dev 0 ref 25 sec 0E item 2A data - | on: rejected: checksum missing or wrong
#   set Set Loop End: DONE after 11 ms
   13.522  OUT  F0 47 5E 00 26 0E 4A F7
   13.529  IN   F0 47 5E 00 26 4F 0E 4A F7 | off: OK dev 0 ref 26 sec 0E item 4A data - | on: rejected: checksum missing or wrong
   13.533  IN   F0 47 5E 00 26 52 0E 4A 00 00 0B 5C F7 | off: REPLY dev 0 ref 26 sec 0E item 4A data 00 00 0B 5C | on: rejected: checksum missing or wrong
#   8 settable sample parameter items round-tripped
   13.538  OUT  F0 47 5E 00 27 0E 30 F7
   13.545  IN   F0 47 5E 00 27 4F 0E 30 F7 | off: OK dev 0 ref 27 sec 0E item 30 data - | on: rejected: checksum missing or wrong
   13.548  IN   F0 47 5E 00 27 52 0E 30 00 F7 | off: REPLY dev 0 ref 27 sec 0E item 30 data 00 | on: rejected: checksum missing or wrong
   13.555  OUT  F0 47 5E 00 28 0E 31 F7
   13.562  IN   F0 47 5E 00 28 4F 0E 31 F7 | off: OK dev 0 ref 28 sec 0E item 31 data - | on: rejected: checksum missing or wrong
   13.566  IN   F0 47 5E 00 28 52 0E 31 01 F7 | off: REPLY dev 0 ref 28 sec 0E item 31 data 01 | on: rejected: checksum missing or wrong
   13.569  OUT  F0 47 5E 00 29 0E 32 F7
   13.575  IN   F0 47 5E 00 29 4F 0E 32 F7 | off: OK dev 0 ref 29 sec 0E item 32 data - | on: rejected: checksum missing or wrong
   13.580  IN   F0 47 5E 00 29 52 0E 32 00 08 5E 41 F7 | off: REPLY dev 0 ref 29 sec 0E item 32 data 00 08 5E 41 | on: rejected: checksum missing or wrong
   13.585  OUT  F0 47 5E 00 2A 0E 33 F7
   13.592  IN   F0 47 5E 00 2A 4F 0E 33 F7 | off: OK dev 0 ref 2A sec 0E item 33 data - | on: rejected: checksum missing or wrong
   13.596  IN   F0 47 5E 00 2A 52 0E 33 00 01 2C 22 F7 | off: REPLY dev 0 ref 2A sec 0E item 33 data 00 01 2C 22 | on: rejected: checksum missing or wrong
   13.597  OUT  F0 47 5E 00 2B 0E 34 F7
   13.604  IN   F0 47 5E 00 2B 4F 0E 34 F7 | off: OK dev 0 ref 2B sec 0E item 34 data - | on: rejected: checksum missing or wrong
   13.610  IN   F0 47 5E 00 2B 52 0E 34 00 01 00 08 5E 41 00 01 2C 22 F7 | off: REPLY dev 0 ref 2B sec 0E item 34 data 00 01 00 08 5E 41 00 01 2C 22 | on: rejected: checksum missing or wrong
#   as expected: &34 decodes to the same values as &30-&33 read individually
   13.617  OUT  F0 47 5E 00 2C 0E 40 F7
   13.625  IN   F0 47 5E 00 2C 4F 0E 40 F7 | off: OK dev 0 ref 2C sec 0E item 40 data - | on: rejected: checksum missing or wrong
   13.629  IN   F0 47 5E 00 2C 52 0E 40 00 00 00 32 F7 | off: REPLY dev 0 ref 2C sec 0E item 40 data 00 00 00 32 | on: rejected: checksum missing or wrong
   13.640  OUT  F0 47 5E 00 2D 0E 41 F7
   13.648  IN   F0 47 5E 00 2D 4F 0E 41 F7 | off: OK dev 0 ref 2D sec 0E item 41 data - | on: rejected: checksum missing or wrong
   13.652  IN   F0 47 5E 00 2D 52 0E 41 00 00 0F 50 F7 | off: REPLY dev 0 ref 2D sec 0E item 41 data 00 00 0F 50 | on: rejected: checksum missing or wrong
   13.654  OUT  F0 47 5E 00 2E 0E 42 F7
   13.662  IN   F0 47 5E 00 2E 4F 0E 42 F7 | off: OK dev 0 ref 2E sec 0E item 42 data - | on: rejected: checksum missing or wrong
   13.665  IN   F0 47 5E 00 2E 52 0E 42 48 F7 | off: REPLY dev 0 ref 2E sec 0E item 42 data 48 | on: rejected: checksum missing or wrong
   13.671  OUT  F0 47 5E 00 2F 0E 43 F7
   13.679  IN   F0 47 5E 00 2F 4F 0E 43 F7 | off: OK dev 0 ref 2F sec 0E item 43 data - | on: rejected: checksum missing or wrong
   13.682  IN   F0 47 5E 00 2F 52 0E 43 01 05 F7 | off: REPLY dev 0 ref 2F sec 0E item 43 data 01 05 | on: rejected: checksum missing or wrong
   13.684  OUT  F0 47 5E 00 30 0E 44 F7
   13.692  IN   F0 47 5E 00 30 4F 0E 44 F7 | off: OK dev 0 ref 30 sec 0E item 44 data - | on: rejected: checksum missing or wrong
   13.695  IN   F0 47 5E 00 30 52 0E 44 00 28 F7 | off: REPLY dev 0 ref 30 sec 0E item 44 data 00 28 | on: rejected: checksum missing or wrong
   13.702  OUT  F0 47 5E 00 31 0E 48 F7
   13.710  IN   F0 47 5E 00 31 4F 0E 48 F7 | off: OK dev 0 ref 31 sec 0E item 48 data - | on: rejected: checksum missing or wrong
   13.713  IN   F0 47 5E 00 31 52 0E 48 01 F7 | off: REPLY dev 0 ref 31 sec 0E item 48 data 01 | on: rejected: checksum missing or wrong
   13.717  OUT  F0 47 5E 00 32 0E 49 F7
   13.726  IN   F0 47 5E 00 32 4F 0E 49 F7 | off: OK dev 0 ref 32 sec 0E item 49 data - | on: rejected: checksum missing or wrong
   13.730  IN   F0 47 5E 00 32 52 0E 49 00 00 00 0F F7 | off: REPLY dev 0 ref 32 sec 0E item 49 data 00 00 00 0F | on: rejected: checksum missing or wrong
   13.741  OUT  F0 47 5E 00 33 0E 4A F7
   13.749  IN   F0 47 5E 00 33 4F 0E 4A F7 | off: OK dev 0 ref 33 sec 0E item 4A data - | on: rejected: checksum missing or wrong
   13.753  IN   F0 47 5E 00 33 52 0E 4A 00 00 0B 5C F7 | off: REPLY dev 0 ref 33 sec 0E item 4A data 00 00 0B 5C | on: rejected: checksum missing or wrong
   13.771  OUT  F0 47 5E 00 34 0E 4B F7
   13.779  IN   F0 47 5E 00 34 4F 0E 4B F7 | off: OK dev 0 ref 34 sec 0E item 4B data - | on: rejected: checksum missing or wrong
   13.789  IN   F0 47 5E 00 34 52 0E 4B 00 00 00 32 00 00 0F 50 48 01 05 00 28 01 00 00 00 0F 00 00 0B 5C F7 | off: REPLY dev 0 ref 34 sec 0E item 4B data 00 00 00 32 00 00 0F 50 48 01 05 00 28 01 00 00 00 0F 00 00 0B 5C | on: rejected: checksum missing or wrong
#   as expected: &4B decodes to the same values as &40-&4A read individually
#   no sample was current before: the wrong-sample refusal is not exercised this run
   13.791  OUT  F0 47 5E 00 35 0E 14 F7
   13.799  IN   F0 47 5E 00 35 4F 0E 14 F7 | off: OK dev 0 ref 35 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   13.804  IN   F0 47 5E 00 35 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 35 sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
   13.818  OUT  F0 47 5E 00 36 0E 14 F7
   13.826  IN   F0 47 5E 00 36 4F 0E 14 F7 | off: OK dev 0 ref 36 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   13.830  IN   F0 47 5E 00 36 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 36 sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
   13.840  OUT  F0 47 5E 00 37 0E 20 00 00 00 00 F7
   13.850  IN   F0 47 5E 00 37 4F 0E 20 F7 | off: OK dev 0 ref 37 sec 0E item 20 data - | on: rejected: checksum missing or wrong
   13.854  IN   F0 47 5E 00 37 44 0E 20 F7 | off: DONE dev 0 ref 37 sec 0E item 20 data - | on: rejected: checksum missing or wrong
#   restore Set Start Position: done
   13.870  OUT  F0 47 5E 00 38 0E 21 00 08 5E 41 F7
   13.880  IN   F0 47 5E 00 38 4F 0E 21 F7 | off: OK dev 0 ref 38 sec 0E item 21 data - | on: rejected: checksum missing or wrong
   13.883  IN   F0 47 5E 00 38 44 0E 21 F7 | off: DONE dev 0 ref 38 sec 0E item 21 data - | on: rejected: checksum missing or wrong
#   restore Set End Position: done
   13.885  OUT  F0 47 5E 00 39 0E 22 3C F7
   13.894  IN   F0 47 5E 00 39 4F 0E 22 F7 | off: OK dev 0 ref 39 sec 0E item 22 data - | on: rejected: checksum missing or wrong
   13.897  IN   F0 47 5E 00 39 44 0E 22 F7 | off: DONE dev 0 ref 39 sec 0E item 22 data - | on: rejected: checksum missing or wrong
#   restore Set Original Pitch: done
   13.899  OUT  F0 47 5E 00 3A 0E 23 00 00 F7
   13.908  IN   F0 47 5E 00 3A 4F 0E 23 F7 | off: OK dev 0 ref 3A sec 0E item 23 data - | on: rejected: checksum missing or wrong
   13.911  IN   F0 47 5E 00 3A 44 0E 23 F7 | off: DONE dev 0 ref 3A sec 0E item 23 data - | on: rejected: checksum missing or wrong
#   restore Set Semitone Tune: done
   13.913  OUT  F0 47 5E 00 3B 0E 24 00 00 F7
   13.922  IN   F0 47 5E 00 3B 4F 0E 24 F7 | off: OK dev 0 ref 3B sec 0E item 24 data - | on: rejected: checksum missing or wrong
   13.924  IN   F0 47 5E 00 3B 44 0E 24 F7 | off: DONE dev 0 ref 3B sec 0E item 24 data - | on: rejected: checksum missing or wrong
#   restore Set Fine Tune: done
   13.941  OUT  F0 47 5E 00 3C 0E 28 00 F7
   13.950  IN   F0 47 5E 00 3C 4F 0E 28 F7 | off: OK dev 0 ref 3C sec 0E item 28 data - | on: rejected: checksum missing or wrong
   13.953  IN   F0 47 5E 00 3C 44 0E 28 F7 | off: DONE dev 0 ref 3C sec 0E item 28 data - | on: rejected: checksum missing or wrong
#   restore Set Playback Mode: done
   13.955  OUT  F0 47 5E 00 3D 0E 29 00 00 00 01 F7
   13.964  IN   F0 47 5E 00 3D 4F 0E 29 F7 | off: OK dev 0 ref 3D sec 0E item 29 data - | on: rejected: checksum missing or wrong
   13.968  IN   F0 47 5E 00 3D 44 0E 29 F7 | off: DONE dev 0 ref 3D sec 0E item 29 data - | on: rejected: checksum missing or wrong
#   restore Set Loop Start: done
   13.970  OUT  F0 47 5E 00 3E 0E 2A 00 08 5E 31 F7
   13.979  IN   F0 47 5E 00 3E 4F 0E 2A F7 | off: OK dev 0 ref 3E sec 0E item 2A data - | on: rejected: checksum missing or wrong
   13.983  IN   F0 47 5E 00 3E 44 0E 2A F7 | off: DONE dev 0 ref 3E sec 0E item 2A data - | on: rejected: checksum missing or wrong
#   restore Set Loop End: done
#   test sample's name and settable parameters restored by the guard
   13.986  OUT  F0 47 5E 00 3F 0E 05 41 4D 45 4E 00 F7
   13.996  IN   F0 47 5E 00 3F 4F 0E 05 F7 | off: OK dev 0 ref 3F sec 0E item 05 data - | on: rejected: checksum missing or wrong
   13.999  IN   F0 47 5E 00 3F 44 0E 05 F7 | off: DONE dev 0 ref 3F sec 0E item 05 data - | on: rejected: checksum missing or wrong
#   as expected: the test sample can still be selected by its original name
   14.001  OUT  F0 47 5E 00 40 0E 14 F7
   14.009  IN   F0 47 5E 00 40 4F 0E 14 F7 | off: OK dev 0 ref 40 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   14.013  IN   F0 47 5E 00 40 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 40 sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
#   as expected: its name is back to "AMEN"
   14.015  OUT  F0 47 5E 00 41 0E 4B F7
   14.023  IN   F0 47 5E 00 41 4F 0E 4B F7 | off: OK dev 0 ref 41 sec 0E item 4B data - | on: rejected: checksum missing or wrong
   14.032  IN   F0 47 5E 00 41 52 0E 4B 00 00 00 00 00 08 5E 41 3C 00 00 00 00 00 00 08 52 56 00 08 5E 31 F7 | off: REPLY dev 0 ref 41 sec 0E item 4B data 00 00 00 00 00 08 5E 41 3C 00 00 00 00 00 00 08 52 56 00 08 5E 31 | on: rejected: checksum missing or wrong
#   NOT MET: its settable parameters are back to what they were before
#   the check ended with its session still open: the guard closes it
#   closing: the session puts back the settings it changed
   15.196  OUT  F0 47 5E 00 42 00 04 00 46 F7
   15.203  IN   F0 47 5E 00 42 4F 00 04 F7 | off: OK dev 0 ref 42 sec 00 item 04 data - | on: rejected: checksum missing or wrong
   15.206  IN   F0 47 5E 00 42 44 00 04 F7 | off: DONE dev 0 ref 42 sec 00 item 04 data - | on: rejected: checksum missing or wrong
   15.206  OUT  F0 47 5E 00 43 00 07 00 F7
   15.214  IN   F0 47 5E 00 43 4F 00 07 F7 | off: OK dev 0 ref 43 sec 00 item 07 data - | on: rejected: checksum missing or wrong
   15.217  IN   F0 47 5E 00 43 44 00 07 F7 | off: DONE dev 0 ref 43 sec 00 item 07 data - | on: rejected: checksum missing or wrong
   15.217  OUT  F0 47 5E 00 44 00 03 01 F7
   15.224  IN   F0 47 5E 00 44 4F 00 03 F7 | off: OK dev 0 ref 44 sec 00 item 03 data - | on: rejected: checksum missing or wrong
   15.227  IN   F0 47 5E 00 44 44 00 03 F7 | off: DONE dev 0 ref 44 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: Sync LCD
#   FAILED: not met: its settable parameters are back to what they were before
# observations
# observation: check 1 PASSED: open a session and close it - opened as ready after 547 ms; DeviceIDs that answered the discovery: 0; closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 2 PASSED: Echo returns the bytes sent - Echo 01 23 45 67: REPLY 01 23 45 67 as sent after 12 ms; closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 3 PASSED: 50 Echo round trips, timed - 50 Echo round trips: min 12 ms, median 12 ms, 95th percentile 13 ms, max 13 ms; closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 4 PASSED: the operating system version is read - OS version (section 02, items 00 and 01): OS 2.14, sub-version 0 after 20 ms; closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 5 PASSED: checksums on and off through the session - checksums on: DONE after 11 ms; Echo with checksums on (the REPLY carries a checksum): REPLY 01 23 45 67 as sent after 13 ms; OS version with checksums on: OS 2.14, sub-version 0 after 22 ms; checksums off again: DONE after 11 ms; Echo with checksums off again: REPLY 01 23 45 67 as sent after 12 ms; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 6 PASSED: closing puts back every setting the session changed - opened as ready with checksums on, Notification off, Still Alive on, Sync LCD off and Auto screen update on; closed after 42 ms, put back: checksum mode, Still Alive, notification, Sync LCD, Auto screen update
# observation: check 7 PASSED: a check that fails half way leaves the sampler in the known state - put back by the guard: checksum mode, Still Alive, Sync LCD
# observation: check 8 FAILED: select the test sample, round-trip every §0E lifecycle and settable-parameter item on it, and restore its name and parameters - not met: its settable parameters are back to what they were before
# observation: 7 checks passed, 1 failed, 0 skipped
# observation: discovery answered by DeviceIDs: 0; the target DeviceID is 0, and the session accepts only confirmations that carry it
# observation: OS version 2.14 (sub-version 0)
# observation: 50 Echo round trips of 50: min 12 ms, median 12 ms, 95th percentile 13 ms, max 13 ms
# observation: F0 F7 messages seen: 0
# observation: messages rejected by the session: 0; unsolicited confirmations: 0; late ERRORs after a REPLY: 0
# observation: checksum mode as the sessions followed it: unknown -> off -> off -> off -> off -> off -> on -> off -> on -> off -> on -> off -> off
# observation: sampler left in the known state: checksums off, Still Alive off, Notification on, Sync LCD on, Auto screen update off
# observation: frames sent 181, received 356
```

### `honesty.log` (second failing run, after F1's fix — Loop Start not restored, F2)

Full log kept at `juce/build/tests/probe/Debug/honesty.log`. Check 8 excerpt:

```text
# check 8: select the test sample, round-trip every §0E lifecycle and settable-parameter item on it, and restore its name and parameters
   10.520  OUT  F0 47 5E 00 00 00 00 00 F7
   10.527  IN   F0 47 5E 00 00 4F 00 00 F7 | off: OK dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
   10.530  IN   F0 47 5E 00 00 44 00 00 F7 | off: DONE dev 0 ref 00 sec 00 item 00 data - | on: rejected: checksum missing or wrong
   11.029  OUT  F0 47 5E 00 01 00 04 00 05 F7
   11.038  IN   F0 47 5E 00 01 4F 00 04 F7 | off: OK dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
   11.041  IN   F0 47 5E 00 01 44 00 04 F7 | off: DONE dev 0 ref 01 sec 00 item 04 data - | on: rejected: checksum missing or wrong
# diagnostic: checksum mode changed: off
   11.042  OUT  F0 47 5E 00 02 00 03 00 F7
   11.051  IN   F0 47 5E 00 02 4F 00 03 F7 | off: OK dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
   11.054  IN   F0 47 5E 00 02 44 00 03 F7 | off: DONE dev 0 ref 02 sec 00 item 03 data - | on: rejected: checksum missing or wrong
   11.055  OUT  F0 47 5E 00 03 00 07 01 F7
   11.064  IN   F0 47 5E 00 03 4F 00 07 F7 | off: OK dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
   11.066  IN   F0 47 5E 00 03 44 00 07 F7 | off: DONE dev 0 ref 03 sec 00 item 07 data - | on: rejected: checksum missing or wrong
   11.075  OUT  F0 47 5E 00 04 0E 14 F7
   11.084  IN   F0 47 5E 00 04 4F 0E 14 F7 | off: OK dev 0 ref 04 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   11.088  IN   F0 47 5E 00 04 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 04 sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
   11.460  OUT  F0 47 5E 00 05 0E 05 48 6F 6E 65 73 74 79 00 F7
   11.470  IN   F0 47 5E 00 05 4F 0E 05 F7 | off: OK dev 0 ref 05 sec 0E item 05 data - | on: rejected: checksum missing or wrong
   11.472  IN   F0 47 5E 00 05 44 0E 05 F7 | off: DONE dev 0 ref 05 sec 0E item 05 data - | on: rejected: checksum missing or wrong
#   test sample "Honesty" selected and current
   11.476  OUT  F0 47 5E 00 06 0E 4B F7
   11.482  IN   F0 47 5E 00 06 4F 0E 4B F7 | off: OK dev 0 ref 06 sec 0E item 4B data - | on: rejected: checksum missing or wrong
   11.492  IN   F0 47 5E 00 06 52 0E 4B 00 00 00 00 00 26 28 41 3C 00 00 00 00 00 00 00 00 01 00 26 28 32 F7 | off: REPLY dev 0 ref 06 sec 0E item 4B data 00 00 00 00 00 26 28 41 3C 00 00 00 00 00 00 00 00 01 00 26 28 32 | on: rejected: checksum missing or wrong
#   sample "Honesty" selected as current
   11.497  OUT  F0 47 5E 00 07 0E 14 F7
   11.504  IN   F0 47 5E 00 07 4F 0E 14 F7 | off: OK dev 0 ref 07 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   11.510  IN   F0 47 5E 00 07 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 07 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   11.638  OUT  F0 47 5E 00 08 0E 09 48 6F 6E 65 73 74 79 5F 32 00 F7
   11.648  IN   F0 47 5E 00 08 4F 0E 09 F7 | off: OK dev 0 ref 08 sec 0E item 09 data - | on: rejected: checksum missing or wrong
   11.653  IN   F0 47 5E 00 08 44 0E 09 F7 | off: DONE dev 0 ref 08 sec 0E item 09 data - | on: rejected: checksum missing or wrong
#   rename the test sample: DONE after 15 ms
   11.656  OUT  F0 47 5E 00 09 0E 14 F7
   11.663  IN   F0 47 5E 00 09 4F 0E 14 F7 | off: OK dev 0 ref 09 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   11.669  IN   F0 47 5E 00 09 52 0E 14 48 6F 6E 65 73 74 79 5F 32 00 F7 | off: REPLY dev 0 ref 09 sec 0E item 14 data 48 6F 6E 65 73 74 79 5F 32 00 | on: rejected: checksum missing or wrong
#   as expected: the new name read back
   11.670  OUT  F0 47 5E 00 0A 0E 09 48 6F 6E 65 73 74 79 00 F7
   11.680  IN   F0 47 5E 00 0A 4F 0E 09 F7 | off: OK dev 0 ref 0A sec 0E item 09 data - | on: rejected: checksum missing or wrong
   11.684  IN   F0 47 5E 00 0A 44 0E 09 F7 | off: DONE dev 0 ref 0A sec 0E item 09 data - | on: rejected: checksum missing or wrong
   11.689  OUT  F0 47 5E 00 0B 0E 14 F7
   11.696  IN   F0 47 5E 00 0B 4F 0E 14 F7 | off: OK dev 0 ref 0B sec 0E item 14 data - | on: rejected: checksum missing or wrong
   11.702  IN   F0 47 5E 00 0B 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 0B sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   11.863  OUT  F0 47 5E 00 0C 0E 0A F7
   11.870  IN   F0 47 5E 00 0C 4F 0E 0A F7 | off: OK dev 0 ref 0C sec 0E item 0A data - | on: rejected: checksum missing or wrong
   11.873  IN   F0 47 5E 00 0C 44 0E 0A F7 | off: DONE dev 0 ref 0C sec 0E item 0A data - | on: rejected: checksum missing or wrong
#   start auditioning: DONE after 10 ms
   11.876  OUT  F0 47 5E 00 0D 0E 14 F7
   11.884  IN   F0 47 5E 00 0D 4F 0E 14 F7 | off: OK dev 0 ref 0D sec 0E item 14 data - | on: rejected: checksum missing or wrong
   11.890  IN   F0 47 5E 00 0D 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 0D sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   11.974  OUT  F0 47 5E 00 0E 0E 0B F7
   11.981  IN   F0 47 5E 00 0E 4F 0E 0B F7 | off: OK dev 0 ref 0E sec 0E item 0B data - | on: rejected: checksum missing or wrong
   11.984  IN   F0 47 5E 00 0E 44 0E 0B F7 | off: DONE dev 0 ref 0E sec 0E item 0B data - | on: rejected: checksum missing or wrong
#   stop auditioning: DONE after 9 ms
   11.985  OUT  F0 47 5E 00 0F 0E 14 F7
   11.992  IN   F0 47 5E 00 0F 4F 0E 14 F7 | off: OK dev 0 ref 0F sec 0E item 14 data - | on: rejected: checksum missing or wrong
   11.997  IN   F0 47 5E 00 0F 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 0F sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   12.074  OUT  F0 47 5E 00 10 0E 20 00 00 00 32 F7
   12.082  IN   F0 47 5E 00 10 4F 0E 20 F7 | off: OK dev 0 ref 10 sec 0E item 20 data - | on: rejected: checksum missing or wrong
   12.087  IN   F0 47 5E 00 10 44 0E 20 F7 | off: DONE dev 0 ref 10 sec 0E item 20 data - | on: rejected: checksum missing or wrong
#   set Set Start Position: DONE after 12 ms
   12.091  OUT  F0 47 5E 00 11 0E 40 F7
   12.097  IN   F0 47 5E 00 11 4F 0E 40 F7 | off: OK dev 0 ref 11 sec 0E item 40 data - | on: rejected: checksum missing or wrong
   12.101  IN   F0 47 5E 00 11 52 0E 40 00 00 00 32 F7 | off: REPLY dev 0 ref 11 sec 0E item 40 data 00 00 00 32 | on: rejected: checksum missing or wrong
   12.104  OUT  F0 47 5E 00 12 0E 14 F7
   12.111  IN   F0 47 5E 00 12 4F 0E 14 F7 | off: OK dev 0 ref 12 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.117  IN   F0 47 5E 00 12 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 12 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   12.232  OUT  F0 47 5E 00 13 0E 21 00 00 0F 50 F7
   12.240  IN   F0 47 5E 00 13 4F 0E 21 F7 | off: OK dev 0 ref 13 sec 0E item 21 data - | on: rejected: checksum missing or wrong
   12.244  IN   F0 47 5E 00 13 44 0E 21 F7 | off: DONE dev 0 ref 13 sec 0E item 21 data - | on: rejected: checksum missing or wrong
#   set Set End Position: DONE after 11 ms
   12.247  OUT  F0 47 5E 00 14 0E 41 F7
   12.254  IN   F0 47 5E 00 14 4F 0E 41 F7 | off: OK dev 0 ref 14 sec 0E item 41 data - | on: rejected: checksum missing or wrong
   12.258  IN   F0 47 5E 00 14 52 0E 41 00 00 0F 50 F7 | off: REPLY dev 0 ref 14 sec 0E item 41 data 00 00 0F 50 | on: rejected: checksum missing or wrong
   12.259  OUT  F0 47 5E 00 15 0E 14 F7
   12.266  IN   F0 47 5E 00 15 4F 0E 14 F7 | off: OK dev 0 ref 15 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.272  IN   F0 47 5E 00 15 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 15 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   12.387  OUT  F0 47 5E 00 16 0E 22 48 F7
   12.395  IN   F0 47 5E 00 16 4F 0E 22 F7 | off: OK dev 0 ref 16 sec 0E item 22 data - | on: rejected: checksum missing or wrong
   12.398  IN   F0 47 5E 00 16 44 0E 22 F7 | off: DONE dev 0 ref 16 sec 0E item 22 data - | on: rejected: checksum missing or wrong
#   set Set Original Pitch: DONE after 11 ms
   12.401  OUT  F0 47 5E 00 17 0E 42 F7
   12.408  IN   F0 47 5E 00 17 4F 0E 42 F7 | off: OK dev 0 ref 17 sec 0E item 42 data - | on: rejected: checksum missing or wrong
   12.411  IN   F0 47 5E 00 17 52 0E 42 48 F7 | off: REPLY dev 0 ref 17 sec 0E item 42 data 48 | on: rejected: checksum missing or wrong
   12.414  OUT  F0 47 5E 00 18 0E 14 F7
   12.421  IN   F0 47 5E 00 18 4F 0E 14 F7 | off: OK dev 0 ref 18 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.426  IN   F0 47 5E 00 18 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 18 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   12.538  OUT  F0 47 5E 00 19 0E 23 01 05 F7
   12.547  IN   F0 47 5E 00 19 4F 0E 23 F7 | off: OK dev 0 ref 19 sec 0E item 23 data - | on: rejected: checksum missing or wrong
   12.549  IN   F0 47 5E 00 19 44 0E 23 F7 | off: DONE dev 0 ref 19 sec 0E item 23 data - | on: rejected: checksum missing or wrong
#   set Set Semitone Tune: DONE after 10 ms
   12.552  OUT  F0 47 5E 00 1A 0E 43 F7
   12.559  IN   F0 47 5E 00 1A 4F 0E 43 F7 | off: OK dev 0 ref 1A sec 0E item 43 data - | on: rejected: checksum missing or wrong
   12.563  IN   F0 47 5E 00 1A 52 0E 43 01 05 F7 | off: REPLY dev 0 ref 1A sec 0E item 43 data 01 05 | on: rejected: checksum missing or wrong
   12.565  OUT  F0 47 5E 00 1B 0E 14 F7
   12.572  IN   F0 47 5E 00 1B 4F 0E 14 F7 | off: OK dev 0 ref 1B sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.577  IN   F0 47 5E 00 1B 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 1B sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   12.689  OUT  F0 47 5E 00 1C 0E 24 00 28 F7
   12.696  IN   F0 47 5E 00 1C 4F 0E 24 F7 | off: OK dev 0 ref 1C sec 0E item 24 data - | on: rejected: checksum missing or wrong
   12.699  IN   F0 47 5E 00 1C 44 0E 24 F7 | off: DONE dev 0 ref 1C sec 0E item 24 data - | on: rejected: checksum missing or wrong
#   set Set Fine Tune: DONE after 10 ms
   12.701  OUT  F0 47 5E 00 1D 0E 44 F7
   12.708  IN   F0 47 5E 00 1D 4F 0E 44 F7 | off: OK dev 0 ref 1D sec 0E item 44 data - | on: rejected: checksum missing or wrong
   12.712  IN   F0 47 5E 00 1D 52 0E 44 00 28 F7 | off: REPLY dev 0 ref 1D sec 0E item 44 data 00 28 | on: rejected: checksum missing or wrong
   12.713  OUT  F0 47 5E 00 1E 0E 14 F7
   12.720  IN   F0 47 5E 00 1E 4F 0E 14 F7 | off: OK dev 0 ref 1E sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.725  IN   F0 47 5E 00 1E 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 1E sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   12.840  OUT  F0 47 5E 00 1F 0E 28 01 F7
   12.847  IN   F0 47 5E 00 1F 4F 0E 28 F7 | off: OK dev 0 ref 1F sec 0E item 28 data - | on: rejected: checksum missing or wrong
   12.851  IN   F0 47 5E 00 1F 44 0E 28 F7 | off: DONE dev 0 ref 1F sec 0E item 28 data - | on: rejected: checksum missing or wrong
#   set Set Playback Mode: DONE after 10 ms
   12.852  OUT  F0 47 5E 00 20 0E 48 F7
   12.858  IN   F0 47 5E 00 20 4F 0E 48 F7 | off: OK dev 0 ref 20 sec 0E item 48 data - | on: rejected: checksum missing or wrong
   12.861  IN   F0 47 5E 00 20 52 0E 48 01 F7 | off: REPLY dev 0 ref 20 sec 0E item 48 data 01 | on: rejected: checksum missing or wrong
   12.867  OUT  F0 47 5E 00 21 0E 14 F7
   12.874  IN   F0 47 5E 00 21 4F 0E 14 F7 | off: OK dev 0 ref 21 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   12.880  IN   F0 47 5E 00 21 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 21 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   12.992  OUT  F0 47 5E 00 22 0E 29 00 00 03 74 F7
   13.001  IN   F0 47 5E 00 22 4F 0E 29 F7 | off: OK dev 0 ref 22 sec 0E item 29 data - | on: rejected: checksum missing or wrong
   13.004  IN   F0 47 5E 00 22 44 0E 29 F7 | off: DONE dev 0 ref 22 sec 0E item 29 data - | on: rejected: checksum missing or wrong
#   set Set Loop Start: DONE after 11 ms
   13.009  OUT  F0 47 5E 00 23 0E 49 F7
   13.016  IN   F0 47 5E 00 23 4F 0E 49 F7 | off: OK dev 0 ref 23 sec 0E item 49 data - | on: rejected: checksum missing or wrong
   13.020  IN   F0 47 5E 00 23 52 0E 49 00 00 03 74 F7 | off: REPLY dev 0 ref 23 sec 0E item 49 data 00 00 03 74 | on: rejected: checksum missing or wrong
   13.029  OUT  F0 47 5E 00 24 0E 14 F7
   13.036  IN   F0 47 5E 00 24 4F 0E 14 F7 | off: OK dev 0 ref 24 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   13.041  IN   F0 47 5E 00 24 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 24 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   13.156  OUT  F0 47 5E 00 25 0E 2A 00 00 0B 5C F7
   13.164  IN   F0 47 5E 00 25 4F 0E 2A F7 | off: OK dev 0 ref 25 sec 0E item 2A data - | on: rejected: checksum missing or wrong
   13.167  IN   F0 47 5E 00 25 44 0E 2A F7 | off: DONE dev 0 ref 25 sec 0E item 2A data - | on: rejected: checksum missing or wrong
#   set Set Loop End: DONE after 11 ms
   13.169  OUT  F0 47 5E 00 26 0E 4A F7
   13.176  IN   F0 47 5E 00 26 4F 0E 4A F7 | off: OK dev 0 ref 26 sec 0E item 4A data - | on: rejected: checksum missing or wrong
   13.181  IN   F0 47 5E 00 26 52 0E 4A 00 00 0B 5C F7 | off: REPLY dev 0 ref 26 sec 0E item 4A data 00 00 0B 5C | on: rejected: checksum missing or wrong
#   8 settable sample parameter items round-tripped
   13.183  OUT  F0 47 5E 00 27 0E 30 F7
   13.191  IN   F0 47 5E 00 27 4F 0E 30 F7 | off: OK dev 0 ref 27 sec 0E item 30 data - | on: rejected: checksum missing or wrong
   13.193  IN   F0 47 5E 00 27 52 0E 30 00 F7 | off: REPLY dev 0 ref 27 sec 0E item 30 data 00 | on: rejected: checksum missing or wrong
   13.197  OUT  F0 47 5E 00 28 0E 31 F7
   13.204  IN   F0 47 5E 00 28 4F 0E 31 F7 | off: OK dev 0 ref 28 sec 0E item 31 data - | on: rejected: checksum missing or wrong
   13.207  IN   F0 47 5E 00 28 52 0E 31 02 F7 | off: REPLY dev 0 ref 28 sec 0E item 31 data 02 | on: rejected: checksum missing or wrong
   13.208  OUT  F0 47 5E 00 29 0E 32 F7
   13.214  IN   F0 47 5E 00 29 4F 0E 32 F7 | off: OK dev 0 ref 29 sec 0E item 32 data - | on: rejected: checksum missing or wrong
   13.218  IN   F0 47 5E 00 29 52 0E 32 00 26 28 41 F7 | off: REPLY dev 0 ref 29 sec 0E item 32 data 00 26 28 41 | on: rejected: checksum missing or wrong
   13.228  OUT  F0 47 5E 00 2A 0E 33 F7
   13.235  IN   F0 47 5E 00 2A 4F 0E 33 F7 | off: OK dev 0 ref 2A sec 0E item 33 data - | on: rejected: checksum missing or wrong
   13.239  IN   F0 47 5E 00 2A 52 0E 33 00 02 58 44 F7 | off: REPLY dev 0 ref 2A sec 0E item 33 data 00 02 58 44 | on: rejected: checksum missing or wrong
   13.244  OUT  F0 47 5E 00 2B 0E 34 F7
   13.251  IN   F0 47 5E 00 2B 4F 0E 34 F7 | off: OK dev 0 ref 2B sec 0E item 34 data - | on: rejected: checksum missing or wrong
   13.257  IN   F0 47 5E 00 2B 52 0E 34 00 02 00 26 28 41 00 02 58 44 F7 | off: REPLY dev 0 ref 2B sec 0E item 34 data 00 02 00 26 28 41 00 02 58 44 | on: rejected: checksum missing or wrong
#   as expected: &34 decodes to the same values as &30-&33 read individually
   13.260  OUT  F0 47 5E 00 2C 0E 40 F7
   13.267  IN   F0 47 5E 00 2C 4F 0E 40 F7 | off: OK dev 0 ref 2C sec 0E item 40 data - | on: rejected: checksum missing or wrong
   13.270  IN   F0 47 5E 00 2C 52 0E 40 00 00 00 32 F7 | off: REPLY dev 0 ref 2C sec 0E item 40 data 00 00 00 32 | on: rejected: checksum missing or wrong
   13.280  OUT  F0 47 5E 00 2D 0E 41 F7
   13.287  IN   F0 47 5E 00 2D 4F 0E 41 F7 | off: OK dev 0 ref 2D sec 0E item 41 data - | on: rejected: checksum missing or wrong
   13.291  IN   F0 47 5E 00 2D 52 0E 41 00 00 0F 50 F7 | off: REPLY dev 0 ref 2D sec 0E item 41 data 00 00 0F 50 | on: rejected: checksum missing or wrong
   13.302  OUT  F0 47 5E 00 2E 0E 42 F7
   13.309  IN   F0 47 5E 00 2E 4F 0E 42 F7 | off: OK dev 0 ref 2E sec 0E item 42 data - | on: rejected: checksum missing or wrong
   13.312  IN   F0 47 5E 00 2E 52 0E 42 48 F7 | off: REPLY dev 0 ref 2E sec 0E item 42 data 48 | on: rejected: checksum missing or wrong
   13.318  OUT  F0 47 5E 00 2F 0E 43 F7
   13.326  IN   F0 47 5E 00 2F 4F 0E 43 F7 | off: OK dev 0 ref 2F sec 0E item 43 data - | on: rejected: checksum missing or wrong
   13.329  IN   F0 47 5E 00 2F 52 0E 43 01 05 F7 | off: REPLY dev 0 ref 2F sec 0E item 43 data 01 05 | on: rejected: checksum missing or wrong
   13.334  OUT  F0 47 5E 00 30 0E 44 F7
   13.341  IN   F0 47 5E 00 30 4F 0E 44 F7 | off: OK dev 0 ref 30 sec 0E item 44 data - | on: rejected: checksum missing or wrong
   13.345  IN   F0 47 5E 00 30 52 0E 44 00 28 F7 | off: REPLY dev 0 ref 30 sec 0E item 44 data 00 28 | on: rejected: checksum missing or wrong
   13.350  OUT  F0 47 5E 00 31 0E 48 F7
   13.357  IN   F0 47 5E 00 31 4F 0E 48 F7 | off: OK dev 0 ref 31 sec 0E item 48 data - | on: rejected: checksum missing or wrong
   13.360  IN   F0 47 5E 00 31 52 0E 48 01 F7 | off: REPLY dev 0 ref 31 sec 0E item 48 data 01 | on: rejected: checksum missing or wrong
   13.365  OUT  F0 47 5E 00 32 0E 49 F7
   13.373  IN   F0 47 5E 00 32 4F 0E 49 F7 | off: OK dev 0 ref 32 sec 0E item 49 data - | on: rejected: checksum missing or wrong
   13.377  IN   F0 47 5E 00 32 52 0E 49 00 00 00 0F F7 | off: REPLY dev 0 ref 32 sec 0E item 49 data 00 00 00 0F | on: rejected: checksum missing or wrong
   13.382  OUT  F0 47 5E 00 33 0E 4A F7
   13.390  IN   F0 47 5E 00 33 4F 0E 4A F7 | off: OK dev 0 ref 33 sec 0E item 4A data - | on: rejected: checksum missing or wrong
   13.394  IN   F0 47 5E 00 33 52 0E 4A 00 00 0B 5C F7 | off: REPLY dev 0 ref 33 sec 0E item 4A data 00 00 0B 5C | on: rejected: checksum missing or wrong
   13.403  OUT  F0 47 5E 00 34 0E 4B F7
   13.411  IN   F0 47 5E 00 34 4F 0E 4B F7 | off: OK dev 0 ref 34 sec 0E item 4B data - | on: rejected: checksum missing or wrong
   13.421  IN   F0 47 5E 00 34 52 0E 4B 00 00 00 32 00 00 0F 50 48 01 05 00 28 01 00 00 00 0F 00 00 0B 5C F7 | off: REPLY dev 0 ref 34 sec 0E item 4B data 00 00 00 32 00 00 0F 50 48 01 05 00 28 01 00 00 00 0F 00 00 0B 5C | on: rejected: checksum missing or wrong
#   as expected: &4B decodes to the same values as &40-&4A read individually
   13.434  OUT  F0 47 5E 00 35 0E 05 41 4D 45 4E 00 F7
   13.444  IN   F0 47 5E 00 35 4F 0E 05 F7 | off: OK dev 0 ref 35 sec 0E item 05 data - | on: rejected: checksum missing or wrong
   13.447  IN   F0 47 5E 00 35 44 0E 05 F7 | off: DONE dev 0 ref 35 sec 0E item 05 data - | on: rejected: checksum missing or wrong
#   select the sample that was current before (to prove the wrong-sample refusal): done
#   as expected: navigated away to the sample that was current before
   13.450  OUT  F0 47 5E 00 36 0E 14 F7
   13.458  IN   F0 47 5E 00 36 4F 0E 14 F7 | off: OK dev 0 ref 36 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   13.463  IN   F0 47 5E 00 36 52 0E 14 41 4D 45 4E 00 F7 | off: REPLY dev 0 ref 36 sec 0E item 14 data 41 4D 45 4E 00 | on: rejected: checksum missing or wrong
#   as expected: acting on the sample that is current but not the test one was refused before sending
   13.467  OUT  F0 47 5E 00 37 0E 05 48 6F 6E 65 73 74 79 00 F7
   13.479  IN   F0 47 5E 00 37 4F 0E 05 F7 | off: OK dev 0 ref 37 sec 0E item 05 data - | on: rejected: checksum missing or wrong
   13.482  IN   F0 47 5E 00 37 44 0E 05 F7 | off: DONE dev 0 ref 37 sec 0E item 05 data - | on: rejected: checksum missing or wrong
#   reselect the test sample: done
#   as expected: reselected the test sample
   13.484  OUT  F0 47 5E 00 38 0E 14 F7
   13.493  IN   F0 47 5E 00 38 4F 0E 14 F7 | off: OK dev 0 ref 38 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   13.498  IN   F0 47 5E 00 38 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 38 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   13.504  OUT  F0 47 5E 00 39 0E 14 F7
   13.512  IN   F0 47 5E 00 39 4F 0E 14 F7 | off: OK dev 0 ref 39 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   13.517  IN   F0 47 5E 00 39 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 39 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
   13.520  OUT  F0 47 5E 00 3A 0E 20 00 00 00 00 F7
   13.529  IN   F0 47 5E 00 3A 4F 0E 20 F7 | off: OK dev 0 ref 3A sec 0E item 20 data - | on: rejected: checksum missing or wrong
   13.533  IN   F0 47 5E 00 3A 44 0E 20 F7 | off: DONE dev 0 ref 3A sec 0E item 20 data - | on: rejected: checksum missing or wrong
#   restore Set Start Position: done
   13.536  OUT  F0 47 5E 00 3B 0E 21 00 26 28 41 F7
   13.545  IN   F0 47 5E 00 3B 4F 0E 21 F7 | off: OK dev 0 ref 3B sec 0E item 21 data - | on: rejected: checksum missing or wrong
   13.549  IN   F0 47 5E 00 3B 44 0E 21 F7 | off: DONE dev 0 ref 3B sec 0E item 21 data - | on: rejected: checksum missing or wrong
#   restore Set End Position: done
   13.566  OUT  F0 47 5E 00 3C 0E 22 3C F7
   13.575  IN   F0 47 5E 00 3C 4F 0E 22 F7 | off: OK dev 0 ref 3C sec 0E item 22 data - | on: rejected: checksum missing or wrong
   13.578  IN   F0 47 5E 00 3C 44 0E 22 F7 | off: DONE dev 0 ref 3C sec 0E item 22 data - | on: rejected: checksum missing or wrong
#   restore Set Original Pitch: done
   13.582  OUT  F0 47 5E 00 3D 0E 23 00 00 F7
   13.591  IN   F0 47 5E 00 3D 4F 0E 23 F7 | off: OK dev 0 ref 3D sec 0E item 23 data - | on: rejected: checksum missing or wrong
   13.594  IN   F0 47 5E 00 3D 44 0E 23 F7 | off: DONE dev 0 ref 3D sec 0E item 23 data - | on: rejected: checksum missing or wrong
#   restore Set Semitone Tune: done
   13.604  OUT  F0 47 5E 00 3E 0E 24 00 00 F7
   13.613  IN   F0 47 5E 00 3E 4F 0E 24 F7 | off: OK dev 0 ref 3E sec 0E item 24 data - | on: rejected: checksum missing or wrong
   13.616  IN   F0 47 5E 00 3E 44 0E 24 F7 | off: DONE dev 0 ref 3E sec 0E item 24 data - | on: rejected: checksum missing or wrong
#   restore Set Fine Tune: done
   13.620  OUT  F0 47 5E 00 3F 0E 28 00 F7
   13.629  IN   F0 47 5E 00 3F 4F 0E 28 F7 | off: OK dev 0 ref 3F sec 0E item 28 data - | on: rejected: checksum missing or wrong
   13.633  IN   F0 47 5E 00 3F 44 0E 28 F7 | off: DONE dev 0 ref 3F sec 0E item 28 data - | on: rejected: checksum missing or wrong
#   restore Set Playback Mode: done
   13.636  OUT  F0 47 5E 00 40 0E 29 00 00 00 01 F7
   13.645  IN   F0 47 5E 00 40 4F 0E 29 F7 | off: OK dev 0 ref 40 sec 0E item 29 data - | on: rejected: checksum missing or wrong
   13.649  IN   F0 47 5E 00 40 44 0E 29 F7 | off: DONE dev 0 ref 40 sec 0E item 29 data - | on: rejected: checksum missing or wrong
#   restore Set Loop Start: done
   13.650  OUT  F0 47 5E 00 41 0E 2A 00 26 28 32 F7
   13.660  IN   F0 47 5E 00 41 4F 0E 2A F7 | off: OK dev 0 ref 41 sec 0E item 2A data - | on: rejected: checksum missing or wrong
   13.663  IN   F0 47 5E 00 41 44 0E 2A F7 | off: DONE dev 0 ref 41 sec 0E item 2A data - | on: rejected: checksum missing or wrong
#   restore Set Loop End: done
   13.667  OUT  F0 47 5E 00 42 0E 05 41 4D 45 4E 00 F7
   13.677  IN   F0 47 5E 00 42 4F 0E 05 F7 | off: OK dev 0 ref 42 sec 0E item 05 data - | on: rejected: checksum missing or wrong
   13.680  IN   F0 47 5E 00 42 44 0E 05 F7 | off: DONE dev 0 ref 42 sec 0E item 05 data - | on: rejected: checksum missing or wrong
#   reselect the sample that was current before: done
#   test sample's name and settable parameters restored by the guard
   13.686  OUT  F0 47 5E 00 43 0E 05 48 6F 6E 65 73 74 79 00 F7
   13.696  IN   F0 47 5E 00 43 4F 0E 05 F7 | off: OK dev 0 ref 43 sec 0E item 05 data - | on: rejected: checksum missing or wrong
   13.699  IN   F0 47 5E 00 43 44 0E 05 F7 | off: DONE dev 0 ref 43 sec 0E item 05 data - | on: rejected: checksum missing or wrong
#   as expected: the test sample can still be selected by its original name
   13.703  OUT  F0 47 5E 00 44 0E 14 F7
   13.712  IN   F0 47 5E 00 44 4F 0E 14 F7 | off: OK dev 0 ref 44 sec 0E item 14 data - | on: rejected: checksum missing or wrong
   13.717  IN   F0 47 5E 00 44 52 0E 14 48 6F 6E 65 73 74 79 00 F7 | off: REPLY dev 0 ref 44 sec 0E item 14 data 48 6F 6E 65 73 74 79 00 | on: rejected: checksum missing or wrong
#   as expected: its name is back to "Honesty"
   13.720  OUT  F0 47 5E 00 45 0E 4B F7
   13.728  IN   F0 47 5E 00 45 4F 0E 4B F7 | off: OK dev 0 ref 45 sec 0E item 4B data - | on: rejected: checksum missing or wrong
   13.738  IN   F0 47 5E 00 45 52 0E 4B 00 00 00 00 00 26 28 41 3C 00 00 00 00 00 00 26 1C 57 00 26 28 32 F7 | off: REPLY dev 0 ref 45 sec 0E item 4B data 00 00 00 00 00 26 28 41 3C 00 00 00 00 00 00 26 1C 57 00 26 28 32 | on: rejected: checksum missing or wrong
#   NOT MET: its settable parameters are back to what they were before
#   the check ended with its session still open: the guard closes it
#   closing: the session puts back the settings it changed
   15.056  OUT  F0 47 5E 00 46 00 04 00 4A F7
   15.064  IN   F0 47 5E 00 46 4F 00 04 F7 | off: OK dev 0 ref 46 sec 00 item 04 data - | on: rejected: checksum missing or wrong
   15.067  IN   F0 47 5E 00 46 44 00 04 F7 | off: DONE dev 0 ref 46 sec 00 item 04 data - | on: rejected: checksum missing or wrong
   15.067  OUT  F0 47 5E 00 47 00 07 00 F7
   15.074  IN   F0 47 5E 00 47 4F 00 07 F7 | off: OK dev 0 ref 47 sec 00 item 07 data - | on: rejected: checksum missing or wrong
   15.077  IN   F0 47 5E 00 47 44 00 07 F7 | off: DONE dev 0 ref 47 sec 00 item 07 data - | on: rejected: checksum missing or wrong
   15.077  OUT  F0 47 5E 00 48 00 03 01 F7
   15.084  IN   F0 47 5E 00 48 4F 00 03 F7 | off: OK dev 0 ref 48 sec 00 item 03 data - | on: rejected: checksum missing or wrong
   15.087  IN   F0 47 5E 00 48 44 00 03 F7 | off: DONE dev 0 ref 48 sec 00 item 03 data - | on: rejected: checksum missing or wrong
#   put back: checksum mode
#   put back: Still Alive
#   put back: Sync LCD
#   FAILED: not met: its settable parameters are back to what they were before
# observations
# observation: check 1 PASSED: open a session and close it - opened as ready after 540 ms; DeviceIDs that answered the discovery: 0; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 2 PASSED: Echo returns the bytes sent - Echo 01 23 45 67: REPLY 01 23 45 67 as sent after 12 ms; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 3 PASSED: 50 Echo round trips, timed - 50 Echo round trips: min 12 ms, median 12 ms, 95th percentile 13 ms, max 13 ms; closed after 31 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 4 PASSED: the operating system version is read - OS version (section 02, items 00 and 01): OS 2.14, sub-version 0 after 20 ms; closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 5 PASSED: checksums on and off through the session - checksums on: DONE after 11 ms; Echo with checksums on (the REPLY carries a checksum): REPLY 01 23 45 67 as sent after 13 ms; OS version with checksums on: OS 2.14, sub-version 0 after 22 ms; checksums off again: DONE after 10 ms; Echo with checksums off again: REPLY 01 23 45 67 as sent after 12 ms; closed after 30 ms, put back: checksum mode, Still Alive, Sync LCD
# observation: check 6 PASSED: closing puts back every setting the session changed - opened as ready with checksums on, Notification off, Still Alive on, Sync LCD off and Auto screen update on; closed after 42 ms, put back: checksum mode, Still Alive, notification, Sync LCD, Auto screen update
# observation: check 7 PASSED: a check that fails half way leaves the sampler in the known state - put back by the guard: checksum mode, Still Alive, Sync LCD
# observation: check 8 FAILED: select the test sample, round-trip every §0E lifecycle and settable-parameter item on it, and restore its name and parameters - not met: its settable parameters are back to what they were before
# observation: 7 checks passed, 1 failed, 0 skipped
# observation: discovery answered by DeviceIDs: 0; the target DeviceID is 0, and the session accepts only confirmations that carry it
# observation: OS version 2.14 (sub-version 0)
# observation: 50 Echo round trips of 50: min 12 ms, median 12 ms, 95th percentile 13 ms, max 13 ms
# observation: F0 F7 messages seen: 0
# observation: messages rejected by the session: 0; unsolicited confirmations: 0; late ERRORs after a REPLY: 0
# observation: checksum mode as the sessions followed it: unknown -> off -> off -> off -> off -> off -> on -> off -> on -> off -> on -> off -> off
# observation: sampler left in the known state: checksums off, Still Alive off, Notification on, Sync LCD on, Auto screen update off
# observation: frames sent 185, received 364
```
