# Observations: the REPLY of Get Clock Time & Date carries section 0B, not 02 (RQ-AKM-059)

An observation record, not an artifact with an ID: the process index ignores it. It supports `RQ-AKM-059`
(`FTR-AKM-006`), `DEC-AKM-016` (`ADR-AKM-001`) and `TASK-AKM-053`/`TASK-AKM-055` (`PLAN-AKM-006`).

## Setup

- AKAI S5000, OS 2.14, sub-version 0; Windows; ESI M8U eX USB MIDI interface; input `MIDIIN2 (ESI M8U eX)`,
  output `MIDIOUT15 (ESI M8U eX)`; DeviceID 0.
- Two independent captures, a day apart:
  1. `xs56k_akm_probe --suite --system-setup --in "MIDIIN2 (ESI M8U eX)" --out "MIDIOUT15 (ESI M8U eX)"`, run
     by the owner on 2026-10-01 (`akm-suite-20261001-215601.log`, `juce/build/tests/probe/Debug/`), before
     `DEC-AKM-016`'s fix existed: the session's `matches()` only accepted a REPLY under the command's own
     section, so Get Clock Time & Date timed out.
  2. A raw SysEx frame typed by hand and sent/received in MIDI-OX on 2026-10-02, independently of any AKM
     code: `F0 47 5E 00 7F 02 05 F7` sent to the sampler's input.

## What was observed

| # | Observation | Evidence | Consequence |
|---|---|---|---|
| G1 | `xs56k_akm_probe --suite --system-setup` check 8 ("round-trip the sampler's name, Play Mode, front-panel lock and clock, and put them back") failed at its first step, reading the clock (`&05`), with `TIMEOUT`, although the wire shows a REPLY arriving 6 ms after the command. Check 9 (the deliberate-failure check) failed the same way. | `akm-suite-20261001-215601.log` lines 434–479 (check 8), the REPLY frame at timestamp 9.972: `F0 47 5E 00 04 52 02 03 ...` is item `03` (name), not `05` — the frame actually answering `&05` is the next one, at 10.008: `F0 47 5E 00 07 52 0B 05 0F 6A 0A 02 06 00 0D 2C F7`. | The REPLY's section byte is `0B`, not `02`: the session's confirmation matcher (`RQ-AKM-007`) compares the REPLY's section against the command's own and rejects it as unsolicited, so the REPLY is logged (`# diagnostic: unsolicited confirmation`) but completes nothing; the command then times out. |
| G2 | Decoded data of the REPLY above: `0F 6A 0A 02 06 00 0D 2C` → year `15×128+106 = 2026`, month `10`, day `02`, day of week `6` (Friday), `00:13:44`. | same frame | Confirms `RQ-AKM-054`'s year encoding (the whole year, not an offset from 1980) and the day-of-week convention (1 = Sunday) against real data: 2026-10-02 is indeed a Friday. |
| G3 | The owner sent `F0 47 5E 00 7F 02 05 F7` (Get Clock Time & Date, no data) directly from MIDI-OX, independently of any session or AKM code, and captured the two confirmations: `F0 47 5E 00 7F 4F 02 05 F7` (OK, section `02`, as the command's own) then `F0 47 5E 00 7F 52 0B 05 0F 6A 0A 02 06 00 2A 2D F7` (REPLY, section **`0B`**, item `05`, 8 data bytes). | pasted MIDI-OX capture, session AKM, 2026-10-02 | Reproduces G1 with no AKM code in the path at all: not an artifact of the session, the scenario driver or the probe — the sampler itself answers this one item's REPLY under section `0B` while its OK keeps section `02`. Decoded: 2026-10-02, 00:42:45, day of week 6 (Friday) — consistent with G2 and with the wall-clock date, 40 minutes later. |
| G4 | No other item observed across every real-sampler run so far (`OBSERVATIONS-RQ-AKM-017-*`, `OBSERVATIONS-RQ-AKM-051-*`) answers under a section other than its command's. | the whole observation record to date | The anomaly is specific to `&05`'s REPLY; `DEC-AKM-016`'s fix is scoped to the one item the catalogue names (`ItemDescriptor::replySection`), not a blanket relaxation of the match. |

| G5 | With `DEC-AKM-016` built, the owner re-ran `xs56k_akm_probe --suite --system-setup` against the same S5000: **all 9 checks passed**, the two system setup checks included (`akm-suite-20261001-223720.log`). The clock round-trips under section `0B` throughout (e.g. set `2030-06-15 08:05:09`, read back identical; restored to `2026-10-02 00:54:57`, drift 0 s); **unsolicited confirmations dropped from 2 (G1's run) to 0** — the exact two REPLYs G1 flagged as unsolicited are now matched. | `akm-suite-20261001-223720.log`, checks 8–9, final observation line | Confirms `DEC-AKM-016` end to end, not only the one manual frame of G3: the real-sampler suite's own restore-under-failure path (check 9) also round-trips the clock correctly through section `0B`. |
| G6 | The same run: Play Mode `3` (Muted) was **set and read back without error** on the real S5000 — `11.251 OUT ... 02 10 03`, REPLY `02 20 03`. Wave memory decoded as `158548694` of `158548694` bytes free (100 %, the sampler holding nothing loaded) from the four-byte compound double word, confirming `TASK-AKM-049`'s choice of `Dword`. | same log, check 8 | Resolves the `&10`/`&20` erratum (`RQ-AKM-057`, `sysex_spec.kb.md`): the item's own text (`3 = Muted`) is what the real sampler follows, not the data column's narrower `"0, 1, 2"`. `TASK-AKM-054` records this formally against section `02`'s coverage. |

## Resolution

- **`DEC-AKM-016`** (`ADR-AKM-001`): the catalogue records, per item, the section its REPLY carries when it
  is not the command's own (`items.json`'s `replySection`, `0x0B` for `SystemGetClock` only); the session's
  confirmation matcher accepts a REPLY under either section for that item, and the codec's REPLY-length
  lookup (`findReplyItem`, used while the checksum mode is unknown) does the same. No other confirmation
  kind (OK, DONE, ERROR), and no other item, is affected.
- **`RQ-AKM-059`** (`FTR-AKM-006`) states the requirement; `TASK-AKM-055` (`PLAN-AKM-006`) implements it,
  proved on the simulated sampler (`ReplySectionTests.cpp`) and confirmed twice on the real S5000 as above.
- **Done** (G5): `xs56k_akm_probe --suite --system-setup` re-run against the real sampler with `DEC-AKM-016`
  built — all 9 checks pass, 0 unsolicited confirmations. `TASK-AKM-053` and `TASK-AKM-055` are closed on
  this evidence.
