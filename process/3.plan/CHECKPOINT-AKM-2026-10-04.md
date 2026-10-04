# Checkpoint — session AKM, 2026-10-04 (after 10 tasks)

Not an AGNOS artifact (no ID, ignored by the index): the checkpoint the CONTEXT MANAGEMENT rule asks for after ten
tasks. Read it first when resuming.

## Done this session (branch `feature/AKM`, all committed, nothing pushed)

| Task | Subject | Commit |
|---|---|---|
| TASK-AKM-083 | Author FTR-AKM-010 and PLAN-AKM-010 (§16), record the owner's §20 real-sampler run | `96c48cf` |
| TASK-AKM-084 | Song file selection, renaming, deletion and information (§16) | `47bae48` |
| TASK-AKM-085 | Set lists (§16) | `5036599` |
| TASK-AKM-086 | `--song-files` real-sampler check; run on the S5000 (it held no song file) | `23cc6a4` |
| TASK-AKM-087 | Coverage of §16 (12/12), docs | `6136f5f` |
| TASK-AKM-088 | Author FTR-AKM-011 and PLAN-AKM-011 (§0C) | `dcacbfc` |
| TASK-AKM-089 | Multi creation, selection, deletion, current index and name | `76271e0` |
| TASK-AKM-090 | Guarded Delete ALL Multis | `437daaa` |
| TASK-AKM-091 | Multi part parameters (12 Set/Get pairs) | `614c818` |
| TASK-AKM-092 | Gets of general information (`&40 &41 &44-&48 &50-&52`) | `cf4f95f` |

`ctest` 661/661 on Windows/MSVC Debug at the last commit; `generate_akm_items.py --check` up to date (360 items).

## Open

Nothing is open in PLAN-AKM-010 and PLAN-AKM-011: TASK-AKM-093 (`b331278`), TASK-AKM-094 (`83a5870`) and TASK-AKM-095
(`404393e`) were done after the checkpoint was first written, §16 and §0C are complete in the catalogue (`ctest` 674/674,
530 of 560 spec lines covered). Sections still at 0 %: §12 Multi FX (18 lines) and §14 Scenelist (12). Still to run on the
real sampler: `--song-files` once a MIDI song file is loaded, `--multi-lifecycle` with the owner's own multis in memory,
`--front-panel` (§20, the owner judged it good, no log).

## Key decisions and facts to keep

- Real S5000 (OS 2.14), ports `--in "MIDIIN2 (ESI M8U eX)" --out "MIDIOUT15 (ESI M8U eX)"`; the owner authorized the
  assistant to run the probe on it (backup of the disk taken, owner away). If the sampler stops answering, the owner
  cannot power-cycle it: note the failed checks in the observations and go on with the simulated sampler.
- The sampler held no MIDI song file and no set list: §16's `--song-files` check read the counts (0) and saw ERROR 4 for
  every item that names something (`OBSERVATIONS-RQ-AKM-085-song-files.md`). Renaming and the REPLYs of the Gets for items
  that exist are still to observe once a song file is loaded.
- Hardware observations of §0C: setting a part's solo clears its mute; `&31` off needs `00 00` (`OBSERVATIONS-RQ-AKM-093-multi.md`).
- No new decision (DEC) was needed so far: §16 and §0C reuse DEC-AKM-003, -011, -012, -013, -014, -015.
- Hand-built request shapes live in the primitives (`renameSetList`, as `loadFile` and `setZoneSample` before it).
- The shell here rewrites heredocs badly: write multi-line scripts to the scratchpad with the Write tool and run them.
- Not run by anyone yet: `--front-panel` (§20) is considered good by the owner (reported 2026-10-04), no log kept.
