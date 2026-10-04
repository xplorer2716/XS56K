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

## Open (PLAN-AKM-011)

- TASK-AKM-093: `&30` rename, `&31` program number (flag then number, hand-built), `&32` part by index and `&33` part
  by name (hand-built), `&34` delete part; model them in the simulated sampler (`setMultiProgramNumber` and
  `setMultiPartProgram` already exist there as test seeds, `MultiRecord` holds the state).
- TASK-AKM-094: `--multi-lifecycle` real-sampler check on a test multi and a test program (RQ-AKM-093: never `&07`,
  never `&01`; put back the current multi; delete both on every exit path). `MultiPartParameterCases` is the shared table.
- TASK-AKM-095: coverage of §0C (`--coverage` says 42 of 47 rows today, 5 to come with TASK-AKM-093), flip `complete`,
  `SUMMARY-akm-sections-coverage.md`, `AGENTS.md`, `CHANGELOG.md`.

## Key decisions and facts to keep

- Real S5000 (OS 2.14), ports `--in "MIDIIN2 (ESI M8U eX)" --out "MIDIOUT15 (ESI M8U eX)"`; the owner authorized the
  assistant to run the probe on it (backup of the disk taken, owner away). If the sampler stops answering, the owner
  cannot power-cycle it: note the failed checks in the observations and go on with the simulated sampler.
- The sampler held no MIDI song file and no set list: §16's `--song-files` check read the counts (0) and saw ERROR 4 for
  every item that names something (`OBSERVATIONS-RQ-AKM-085-song-files.md`). Renaming and the REPLYs of the Gets for items
  that exist are still to observe once a song file is loaded.
- No new decision (DEC) was needed so far: §16 and §0C reuse DEC-AKM-003, -011, -012, -013, -014, -015.
- Hand-built request shapes live in the primitives (`renameSetList`, as `loadFile` and `setZoneSample` before it).
- The shell here rewrites heredocs badly: write multi-line scripts to the scratchpad with the Write tool and run them.
- Not run by anyone yet: `--front-panel` (§20) is considered good by the owner (reported 2026-10-04), no log kept.
