# Observations — RQ-AKM-097: the scenelists check on the real S5000

Not an AGNOS artifact (no ID of its own, like the other `OBSERVATIONS-*` files). What the owner's S5000 (OS 2.14, no disk
drive attached) answered to the section 14 items, from one run of `xs56k_akm_probe --suite --scenelists --yes` made by the
assistant under the owner's standing authorization on 2026-10-04 (ports `MIDIIN2 (ESI M8U eX)` / `MIDIOUT15 (ESI M8U eX)`).
Traceability: RQ-AKM-095, RQ-AKM-096, RQ-AKM-097, TASK-AKM-098 (`FTR-AKM-012`, `PLAN-AKM-012`). The log
(`akm-suite-20261004-165127.log`) is not committed.

## The run

7 automatic checks passed, 0 failed, 2 skipped (the two scenelists checks); sampler left in the known state (checksums off,
Still Alive off, Notification on, Sync LCD on, Auto screen update off); no message rejected, no unsolicited confirmation.

The sampler held **no scenelist**, as it held no song file or set list when `--song-files` ran (see
`OBSERVATIONS-RQ-AKM-085-song-files.md`): §14, like §16, cannot create one. Both checks are skipped, as designed, after
reading what an empty memory answers.

## What the sampler answered

| Item | Request | Answer |
|---|---|---|
| `&10` number of scenelists | no data | OK, then REPLY `00 00` (0 scenelist) |
| `&13` current scenelist's index | no data | OK, then ERROR 4 (not found) |
| `&11` name of scenelist 0 | `00 00` | OK, then ERROR 4 |
| `&06` select scenelist 0 | `00 00` | OK, then ERROR 4 |
| `&05` select the scenelist named `XS56K TEST` | the name, null-terminated | OK, then ERROR 4 |

Every request was acknowledged with OK, then answered. The sampler therefore **supports section 14** on this OS (it does not
answer ERROR 0, "not supported"), and with nothing in memory answers ERROR 4 for every item that names a scenelist or asks
for the current one — the same answers as §16's song files, which is what the simulated sampler models.

## What was established

- **Established:** `&10` decodes as a REPLY of two data bytes (count `00 00`), and `&13`, `&11`, `&06`, `&05` answer ERROR 4
  with nothing in memory, so the open point of FTR-AKM-012 about an empty memory is answered, and §14 exists on the S5000.
- **Not established** (nothing to act on): the REPLYs of `&11`, `&13`, `&14` for a scenelist that exists, the selection by index
  and by name, the renaming (`&09`), the deletion (`&08`, never sent by any check), and whether the selection by name is
  case-sensitive. Run `--scenelists` again once a scenelist is loaded (a scenelist is memory item type 6 of §10's save).
