# Observations — RQ-AKM-085: the song files check on the real S5000

Not an AGNOS artifact (no ID of its own, like the other `OBSERVATIONS-*` files). What the owner's S5000 (OS 2.14,
no disk drive attached) did with the section 16 items, from `xs56k_akm_probe --suite --song-files`, run by the
assistant under the owner's standing authorization of 2026-10-04. Traceability: RQ-AKM-082, RQ-AKM-083,
RQ-AKM-084, RQ-AKM-085, TASK-AKM-086 (`FTR-AKM-010`, `PLAN-AKM-010`).

## Run of 2026-10-04 (`akm-suite-20261004-130836.log`, not committed)

The seven automatic checks passed (50 Echo round trips: median 13 ms, maximum 15 ms) and the sampler was left in the
known state. The sampler held **no song file and no set list**: the check read what it could, then skipped, as
designed (a song file or a set list cannot be created through §16, and loading one is a §10 operation that the
suite does not send).

| Item | Sent | Sampler's answer |
|---|---|---|
| `&10` Number of song files | `16 10` | OK, then REPLY `00 00` (0) |
| `&20` Number of set lists | `16 20` | OK, then REPLY `00 00` (0) |
| `&13` Current song file's index, none held | `16 13` | OK, then **ERROR 4** (not found) |
| `&11` Name of song file 0, none held | `16 11 00 00` | OK, then **ERROR 4** |
| `&21` Name of set list 0, none held | `16 21 00 00` | OK, then **ERROR 4** |
| `&06` Select song file 0 by index, none held | `16 06 00 00` | OK, then **ERROR 4** |
| `&05` Select a song file by name, none held | `16 05 "XS56K TEST"` | OK, then **ERROR 4** |

Nothing was renamed or deleted: those items were not sent.

## What was established

- **Established:** the S5000 answers section 16 (it does not answer ERROR 0, "not supported"). The two counts
  (`&10`, `&20`) are REPLYs of two data bytes, decoded by the primitives. With nothing in memory, every item that names
  a song file or a set list, or asks for the current one, answers ERROR 4 — what the simulated sampler models
  (`SimulatedSampler.cpp` `executeSongFiles`, a modelling choice until now) and the first open point of
  `FTR-AKM-010`.
- **Not established** (the sampler held nothing): the shape of the REPLYs of `&11`, `&13`, `&14`, `&21` for an item
  that exists (the primitives decode them as the spec's Table 29 gives them); selection by name and by index of a song
  file that exists; renaming; whether a song file named by `&05` is matched case-sensitively; the maximum name
  length; what the sampler answers to `&08`, `&09`, `&22`, `&23` (deliberately not tried: the first two act on a
  song file that does not exist, and none of the four is sent to a sampler that holds something unless the check
  puts it back). The check is to be run again by the owner once a MIDI song file is loaded
  (`xs56k_akm_probe --suite --song-files`, a few seconds, and it puts back every name and the selection).
- **Open point of `FTR-AKM-010` closed in part:** "what an empty memory answers" — ERROR 4 on the S5000, as modelled.
