# Checkpoint — session MCP, 2026-10-06 (after PLAN-MCP-003's end and PLAN-MCP-004)

Not an AGNOS artifact (no ID, ignored by the index): the hand-off note the CONTEXT MANAGEMENT rule asks for, because this session has
gone past 10 tasks (the owner lifted the cap on 2026-10-05: "tu peux faire toutes les taches"). Read it first when resuming; it follows
`CHECKPOINT-MCP-2026-10-05.md`. Every commit carries "(HOL -Human on the loop)" after its AGNOS subject.

## Done since the last checkpoint (branch `feature/MCP`, all committed)

| Task | Subject |
|---|---|
| TASK-MCP-025 | the disk-list refresh needs `--allow-disk-refresh` (it hung the real S5000 with its SCSI2SD) |
| TASK-MCP-026 | `create_folder` |
| TASK-MCP-027 | a save is verified against a listing the sampler has refreshed (the S5000 keeps a folder's file list) |
| TASK-MCP-028 | FTR-MCP-004, ADR-MCP-004, PLAN-MCP-004 (from the review of the ADRs for what was excluded without being asked) |
| TASK-MCP-029 | `set_zone_sample`, `get_zone_samples` |
| TASK-MCP-030 | `add_keygroups`, `delete_keygroup` |
| TASK-MCP-031 | `rename_sample`, `delete_sample` |
| TASK-MCP-032 | `create_multi`, `rename_multi`, `delete_multi`, `set_part_program`, `clear_part`, `set_multi_program_number`, `get_part_programs` |
| TASK-MCP-033 | `rename_file`, `rename_folder`, `delete_file`, `delete_folder` |
| TASK-MCP-034 | `get_system_info`, `get_disk_space` |
| TASK-MCP-035 | `audition_sample`, `audition_file` |
| TASK-MCP-036 | the README rewritten as a reference, checked by `mcp_readme_names_every_tool_and_option`; the scripted conversations extended |

`ctest` 966 of 966 on Windows/MSVC Debug. The server has **32 tools without an option and 48 with `--allow-disk`**.

## Open, in this order

1. **TASK-MCP-023 (In Progress)**: what is left of the real run of PLAN-MCP-003 — the same load without `with_dependents` as a control,
   the save of a multi and its file extension (`.AKM` is an assumption), then **complete the simulated sampler** with what the real run
   showed: the error code with no disk selected (257 against 4), the size of a saved program (516 bytes against 4096), the path written
   with a backslash (`AKWF\AKWF_oboe` against `AKWF/AKWF_oboe`), the order of a folder listing (the disk's, not alphabetical), the time
   of a large load.
2. **TASK-MCP-037 (Blocked: needs the owner)**: run the 21 new tools on the real S5000, one at a time, **on objects made for the run only**
   (a program created for it, samples loaded from the disk, a multi created for it, the `MCPTEST*` folders and a test file), write it
   up in `OBSERVATIONS-RQ-MCP-012-real-sampler.md`, update the README's table "what has been tried", correct the simulator. Things the
   real run must settle: what the sampler does on `delete_keygroup`, on `set_zone_sample` of a sample already used, on the rename of a
   file (the extension is appended; seen for `.AKP`, **not for `.WAV`**), and on deleting a folder with contents.
3. The pull request's CI after the latest pushes; the ADRs (MCP-002, 003, 004) are "Proposed" for the owner's review.
4. The owner's decision on what ADR-MCP-004 DEC-MCP-025 leaves out: song files, set lists, scenelists, MIDI setup and the other
   settings, the effects board, eject, delete all and clear memory, the front-panel keys.
5. END SESSION: the summary report and the row of `process/_sessionstate/METRICS_LOG.md` (not written yet).

## On the owner's disk (S5K) after the real runs — to delete by hand, no tool deleted them

`MCPSAVETEST.AKP` at the root; the folders `MCPTEST`, `MCPTEST2` and `MCPTEST3` with 13 `.WAV` files each. `Al_Jarreau-Flame.WAV` at
the root is the owner's.

## What to remember

- **The refresh of the disk list hangs this sampler** (the owner's SCSI2SD). Never send it. Never pipe a script that talks to the
  sampler into `head` or anything that can end early (a killed client left the S5000 silent once); write the output to a file.
- **Process.** Re-read `.github/instructions/agnos-sw-eng.instructions.md` after any compaction. Tests are written first and **run
  red**, then green; shell commands are PowerShell (session platform `windows`); a full `ctest` is run once per batch, per task only
  the targeted tests and `ctest -E bld_mutate_tool_script_tests` (the one 42-second test, unrelated to the server).
- **What was invented and corrected:** `.AKS` (a sample file is `.WAV`, a program `.AKP`); the claim that loading with dependents
  worked before a program with a sample was tried; the claim that the sampler had no disk. State what was observed, not what was
  assumed.
