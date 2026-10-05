# Observations — RQ-AKM-076: the front panel check on the real S5000

Not an AGNOS artifact (no ID of its own, like the other `OBSERVATIONS-*` files). What the owner's S5000 (OS 2.14, no
disk drive attached) did with the section 20 items, from two runs of `xs56k_akm_probe --suite --front-panel` made by the
owner on 2026-10-04, the keys pressed by the owner on a screen of the owner's choice. Traceability: RQ-AKM-073,
RQ-AKM-074, RQ-AKM-075, RQ-AKM-076, TASK-AKM-073 (`FTR-AKM-008`, `PLAN-AKM-008`). The logs
(`akm-suite-20261004-162529.log`, `akm-suite-20261004-162917.log`) are not committed.

## The two runs

| Run | Log | Checks | PC keys pressed | §20 commands sent |
|---|---|---|---|---|
| 1 | `akm-suite-20261004-162529.log` | 8 passed, 0 failed | 129 | 250 |
| 2 | `akm-suite-20261004-162917.log` | 8 passed, 0 failed | 278 | 343 |

Both: the seven automatic checks passed (50 Echo round trips, 12 ms every time), no message rejected, no unsolicited
confirmation, nothing lost, every key released, the sampler left in the known state. Every §20 frame was answered OK then
DONE; a press (Hold then Release) took 19-20 ms, a data wheel step 10 ms. The owner's own verdict, on the sampler's
screen: **the shortcuts worked** ("cela semble fonctionnel pour tous les raccourcis").

## What was sent, from the logs

| Sampler key (code) | PC key | Seen in |
|---|---|---|
| F1 to F8 (`48`-`4F`) | F1-F8 | runs 1 and 2 |
| digit 5 (`5D`) | `5` | run 2 |
| CURSOR < (`64`), CURSOR > (`65`) | arrows | runs 1 and 2 |
| ENT/PLAY (`6B`), a short press | Enter | runs 1 and 2 |
| ENT/PLAY held, then released by the next Space | Space | runs 1 and 2 (held about five seconds in run 1) |
| MULTI (`44`), FX (`40`) | `m`, `x` | run 1 (`m`, `x`), run 2 (`x`) |
| SAVE (`46`), LOAD (`47`) | `v`, `l` | run 2 |
| WINDOW (`67`), MARK (`68`), JUMP (`69`) | `w`, `k`, `j` | run 2 |
| data wheel, 1 click forwards and backwards (`20 03 00 01`, `20 03 01 01`) | up/down arrows | runs 1 and 2 |
| data wheel, 8 clicks forwards and backwards (`20 03 00 08`, `20 03 01 08`) | Page Up / Page Down | run 2 |

The text mode was entered and left (Tab) several times in run 2, but only arrow keys were pressed inside it, which the
check answered with "no sampler key, nothing sent": no ASCII character was sent.

## What was established

- **Established:** the S5000 accepts the Hold, the Release and the data wheel items of §20 for the keys above: each is
  answered OK then DONE, and the owner reports that the shortcuts worked on the sampler (the effect on the screen is the
  owner's word, the log only shows the acceptance). A held ENT/PLAY, released by a later Space or Enter, is accepted too
  (the audition use of the spec's own example). Long runs of the same cursor key and bursts of wheel steps a few tens of
  milliseconds apart were taken without an error or a lost command.
- **Not established** (not pressed in these runs): EXIT (Escape), `-` and `+`, the digits other than 5, EDIT SAMPLE, EDIT
  PROGRAM, RECORD and UTILITIES (`s`, `p`, `r`, `u`); the ASCII keyboard item `&04` (the text mode was never given a
  character), so whether the S5000 takes Backspace and Enter as ASCII 8 and 13 is still open; and whether the sampler
  counts the Holds of one key (a key held twice and released once). The keycode `&66`, which Table 31 leaves out, was
  never sent, as designed.
- **The data wheel items** were sent as the spec gives them (`00` forwards, `01` backwards, then the number of clicks) and
  accepted; which way the value moved on the sampler is not in the log.
