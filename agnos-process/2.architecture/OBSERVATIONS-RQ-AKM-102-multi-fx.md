# Observations — RQ-AKM-102: the Multi FX check on the real S5000

Not an AGNOS artifact (no ID of its own, like the other `OBSERVATIONS-*` files). What the owner's S5000 (OS 2.14, no disk
drive, **no FX board installed**) answered to the section 12 Gets, from one run of `xs56k_akm_probe --suite --multi-fx
--yes` made by the assistant under the owner's standing authorization on 2026-10-04 (ports `MIDIIN2 (ESI M8U eX)` /
`MIDIOUT15 (ESI M8U eX)`). Traceability: RQ-AKM-099, RQ-AKM-100, RQ-AKM-101, RQ-AKM-102, TASK-AKM-104 (`FTR-AKM-013`,
`PLAN-AKM-013`). The log (`akm-suite-20261004-173211.log`) is not committed.

## The run

7 automatic checks passed, 1 skipped (the Multi FX check, as designed with no board), 1 passed (the check made to fail
half way); sampler left in the known state (checksums off, Still Alive off, Notification on, Sync LCD on, Auto screen
update off); no message rejected, no unsolicited confirmation. The sampler held no multi: the check created the test multi
`XS56K_MULTI_TEST` (index 0, made current by the create), asked the Gets below with it current, selected it again by its
index and deleted it, twice (the second time for the check that fails on purpose); the multis were verified back to none.
No section 12 Set was sent.

## What the sampler answered

| Item | Request | Answer |
|---|---|---|
| `&01` FX card installed | no data | OK, then REPLY `00` (none) |
| `&10` number of FX channels | no data | OK, then REPLY `00` (0 channels) |
| `&11` modules of channel 0 | `00` | OK, then REPLY `00` (0 modules): **not an error** |
| `&21` mute status of channel 0 | `00` | OK, then ERROR 2 (parameter out of range), data `00 02` |
| `&31` type of module 0 of channel 0 | `00 00` | OK, then ERROR 2 |
| `&41` enabled state of module 0 of channel 0 | `00 00` | OK, then ERROR 2 |
| `&51` parameter 0 of module 0 of channel 0 | `00 00 00` | OK, then ERROR 2 |

The sampler therefore **supports section 12** on this OS without a board, answering for a hardware with no channel.

## What was established

- **Established:** with no board the sampler says so (`&01` = 0), counts 0 channels, counts 0 modules for a channel that
  does not exist (a REPLY, not an error), and answers ERROR 2 ("parameter out of range"), not ERROR 4, to every Get that
  names a channel and a module it does not have. The simulated sampler was changed to give those answers (it first
  modelled ERROR 4 for all three, a modelling choice the spec left open): a channel or a module the board lacks is ERROR 2
  for the items that act on it, and a module count of 0 for `&11`. The open point of FTR-AKM-013 about a sampler with no
  board is answered.
- **Not established** (no board to run them): every Set of the section, the REPLYs of the Gets for a channel and a module
  that exist, the signed parameter values on hardware, which modules may change type (the spec says only modules 2 and 3 of
  channels 0 and 1 with an EB20), what the sampler answers when no multi is current (the run always had the test multi
  current), what an out-of-range parameter index or value answers, and whether the board's layout is the one the kb
  gives Figure 2. They stay tested on the simulated sampler only, and the check's round trip with a board is untested on
  hardware: the owner has no EB20.
