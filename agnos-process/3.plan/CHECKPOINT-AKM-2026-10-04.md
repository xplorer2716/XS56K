# Checkpoint — session AKM, 2026-10-04 (after 23 tasks)

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

## Done after the first checkpoint (same day, second batch of ten tasks)

| Task | Subject | Commit |
|---|---|---|
| TASK-AKM-093 | Multi renaming, program number and part assignment (§0C) | `b331278` |
| TASK-AKM-094 | `--multi-lifecycle` real-sampler check (3 runs on the S5000) | `83a5870` |
| TASK-AKM-095 | Coverage of §0C (47/47), docs | `404393e` |
| TASK-AKM-096 | Author FTR-AKM-012 and PLAN-AKM-012 (§14 scenelist) | `ad07a7a` |
| TASK-AKM-097 | Scenelist selection, renaming, deletion and information (§14) | `d2dd904` |
| TASK-AKM-098 | `--scenelists` real-sampler check (the S5000 supports §14 and held none) | `6b95ffa` |
| TASK-AKM-099 | Coverage of §14 (8/8), docs | `6315d0a` |
| TASK-AKM-100 | Author FTR-AKM-013 and PLAN-AKM-013 (§12 Multi FX) | `80a2085` |
| TASK-AKM-101 | FX board and layout discovery, §12 in the simulated sampler | `5dc7489` |
| TASK-AKM-102 | Channel mute, module type and module state | `4add695` |
| TASK-AKM-103 | FX parameter values (sign + two-byte magnitude) | `98db0ae` |
| TASK-AKM-104 | `--multi-fx` real-sampler check (no EB20: only the empty-board answers) | `28ccbc4` |
| TASK-AKM-105 | Coverage of §12 (11/11), 560 of 560 spec lines, docs | `c58ba7a` |

`ctest` 711/711 on Windows/MSVC Debug at the last commit; `generate_akm_items.py --check` up to date (384 items).

## Open

Nothing is open in PLAN-AKM-010 to PLAN-AKM-013: every section of the SysEx spec is catalogued (560 of 560 spec lines).
Still to run on the real sampler, all needing something the owner's sampler does not hold: `--song-files` once a MIDI song
file or a set list is loaded, `--scenelists` once a scenelist is loaded, `--multi-lifecycle` with the owner's own multis in
memory, every Set of §12 with an EB20 (the owner has none: tested on the simulated sampler only), and the `--front-panel`
keys not yet pressed (Escape, `-`/`+`, other digits, four mode keys, text-mode ASCII; §20 itself ran twice, see
OBSERVATIONS-RQ-AKM-076-front-panel.md). The end-of-session actions are not done yet: the report with the four friction
counters, the row in `process/_sessionstate/METRICS_LOG.md` and the self-improvement proposal.

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
- The shell here rewrites heredocs badly: write multi-line scripts to the scratchpad with the Write tool and run them. A
  Python script that rewrites a repository file must open it with `newline=''` (a text-mode write turns LF into CRLF on this
  machine: it did to two files in TASK-AKM-102, caught by the commit's stat and put back).
- §14 and §12 observations on the S5000: §14 is supported (count 0, ERROR 4 for the rest, nothing loaded); §12 with no board
  and a test multi current: `&01` 0, `&10` 0, `&11` 0, ERROR 2 (not 4) for the Gets naming a channel and a module
  (`OBSERVATIONS-RQ-AKM-097-scenelist.md`, `OBSERVATIONS-RQ-AKM-102-multi-fx.md`).
- `generate_akm_items.py` cuts a description the PDF merged after the last domain of a data column (§12 &30); §10 &2C's
  mid-column one is left as it was. `RealSamplerSuite.cpp` needs `/bigobj` on MSVC (C1128).
- §20 ran twice on the real S5000 on 2026-10-04 (logs given by the owner): all accepted, owner says the shortcuts worked.
