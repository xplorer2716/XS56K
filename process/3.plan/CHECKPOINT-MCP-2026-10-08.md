# Checkpoint — session MCP, 2026-10-08 (PLAN-MCP-005, TASK-MCP-046 to TASK-MCP-054)

Not an AGNOS artifact (no ID, ignored by the index): the hand-off note. Read it first when resuming; it follows
`CHECKPOINT-MCP-2026-10-06.md`. The session ran on Linux (the earlier ones on Windows): `session.yaml` says `platform: linux`. Every commit
carries "(HOL -Human on the loop)". The owner was away for the nine tasks from TASK-MCP-047 on: every choice that may diverge from the
specification is in the `Assumptions` field of its task, in `PLAN-MCP-005`.

## Done since the last checkpoint (branch `feature/MCP`, all committed and pushed at the end)

| Task | Subject |
|---|---|
| TASK-MCP-046 | `set_midi_setting`, `set_midi_filter` (channels are the sampler's own numbers, 0 to 31; RQ-MCP-047's "17" became "32") |
| TASK-MCP-047 | list, select and rename the song files, set lists and scenelists (8 tools) |
| TASK-MCP-048 | `delete_song_file`, `delete_set_list`, `delete_scenelist`; negative test of the source check |
| TASK-MCP-049 | the kinds `song_file`, `set_list`, `scenelist` for `save_memory_item`, `save_all_memory_items`, `load_file` (extensions are placeholders) |
| TASK-MCP-050 | `delete_all_programs`, `delete_all_samples`, `delete_all_multis` (confirm = the count) |
| TASK-MCP-051 | `clear_sampler_memory` (confirm = programs + samples + multis) |
| TASK-MCP-052 | the effects board: 5 tools, Tables 24 and 25 as data (`FxCatalogue`) |
| TASK-MCP-053 | `--allow-front-panel` and the 5 key tools, in their own gateway unit |
| TASK-MCP-054 | READMEs, counts checked against the server by `ctest`, Safety section rewritten |

`ctest` 1076 of 1076 on Linux (GCC 13 with `-Wno-dangling-reference`, a false positive in `RealSamplerSuiteTests.cpp`, kept as a local flag at the
owner's word; CI uses GCC 11). The server lists **56 tools without any option, 72 with `--allow-disk`, 61 with `--allow-front-panel`, 77 with both**.
Nothing of this session has been run on a real sampler.

## Open, in this order

1. **TASK-MCP-055 (Blocked: needs the owner)**: the real run of the new tools, on test data listed in the task (song files, set lists and scenelists
   in memory and on the disk, made by the owner). Exceptions: the effects board (no board), `clear_sampler_memory` (unless the owner decides) and the
   keys (only harmless ones he watches). It will settle: the file extensions of a saved song file, set list and scenelist; whether the real sampler
   refuses a duplicate name on a rename; what it answers to a select of an unknown song file or scenelist and to "which one is current" with none; what
   Clear Sampler Memory does to song files, set lists and scenelists; how long it takes.
2. **The owner's review of the choices that diverge from the specification** (each is in the task's `Assumptions`): a duplicate name is refused by the
   server before anything is sent (047); an unknown name is sent to the sampler on a select (047); the MIDI channel number (046); the ranges of the
   effects parameters are Table 25's and the EB20 rule is enforced (052); `confirm` of the bulk tools is an integer (050, 051); all key tools are
   declared destructive (053).
3. The ADRs (MCP-002 to MCP-005) are still "Proposed".
4. Whether the AKM library covers every function of the specification and the tools every function of the library has **not** been checked item by
   item (see TASK-MCP-054, assumption 4).

## What to remember

- **Never send** the disk refresh, `clear_sampler_memory`, a `delete_all_*` or a key to the owner's sampler without his word (`AGENTS.md`).
- Tests are written first and run red; a full `ctest` runs once per task; the scripted conversations are regenerated and the diff read line by line.
- This container needed `libasound2-dev` (installed with `apt-get`); `rtk` was installed from the owner's `.deb`. The hook in
  `~/.claude/settings.json` that rewrites commands could not be written from the session (permission classifier): `rtk` was typed by hand.
- The Stop hook asks for a commit and a push at each end of turn; the owner asked for a push only when all the tasks are done.
