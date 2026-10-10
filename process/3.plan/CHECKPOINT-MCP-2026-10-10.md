# Checkpoint — session MCP, 2026-10-10 (PLAN-MCP-005, TASK-MCP-058 to TASK-MCP-067, TASK-AKM-106)

Not an AGNOS artifact (no ID, ignored by the index): the hand-off note. Read it first when resuming; it follows `CHECKPOINT-MCP-2026-10-08.md`
and the commits of the first part of 2026-10-10 (TASK-MCP-055 to TASK-MCP-057). The session ran on Windows (`platform: windows`, MSVC Debug,
`ctest` 1099 of 1099, no warning). Every commit carries "(HOL -Human on the loop)". From TASK-MCP-059 on, the owner left the work to the
assistant ("do all the tasks without me", the data being test data); the commits from TASK-MCP-060 on were made without asking him to confirm each
message. **The owner pushed the branch afterwards, then ran TASK-MCP-063 with the assistant at the sampler's screen the same day: it and TASK-MCP-064
are Done (see below).**

## Done since the last checkpoint (branch `feature/MCP`)

| Task | Subject |
|---|---|
| TASK-MCP-066 | the server writes to standard error each message of the sampler that the session rejected, with its reason |
| TASK-AKM-106 | the probe's song files check puts back by name; the simulated sampler sorts its songs on a renaming |
| TASK-MCP-067 | a renamed song, set list or scenelist is found again by its new name; the answer gives its position and says which one is current |
| TASK-MCP-058 | real run: song files, set lists, scenelists in memory (the sampler sorts songs, keeps the position selected, holds one set list) |
| TASK-MCP-065 | the instructions given at `initialize` name the tools of every family of each launch (`serverInstructions`, a test with a table of the 77 tools) |
| TASK-MCP-059 | real run: saving and loading them through the disk (`.MID`, `.SET`, `.ASC`; a load replaces an item of the same name) |
| TASK-MCP-060 | real run: the five effects tools answer "no board" |
| TASK-MCP-061 | real run: `delete_all_programs`, `delete_all_samples`, `delete_all_multis` after the whole memory was saved to the disk |
| TASK-MCP-062 | real run: `clear_sampler_memory`, which **also clears the song files, set lists and scenelists**; the tool now says so |
| TASK-MCP-063 | real run with the owner at the screen: the five key tools did what they say (a cursor key, a held key repeating and released by the close of the session, the data wheel on a character of a name, an ASCII character, `multi` and `utilities`); the test program was deleted |
| TASK-MCP-064 | the READMEs and the CHANGELOG say what ran on the real sampler, the keys included; Done |
| TASK-MCP-068 | the `confirm` of `clear_sampler_memory` counts the six kinds (the owner's decision) |

The real sampler's findings are in `process/2.architecture/OBSERVATIONS-RQ-MCP-012-real-sampler.md` (sections of TASK-MCP-058 to TASK-MCP-063).
After TASK-MCP-063 the S5000 was as found (the test program `MCP063TEST` is deleted, the screen is on UTILITIES). The owner's S5000 was left as it was found: 2 programs (`New Program  1` current, `X01`), 2 multis (`New Multi  2` current), 4 songs
(`a_woman_in_love` current), `Set 1`, 2 scenelists (`Scene  1` current), and the disk `S5K` with its 9 folders and 10 files (the test folders
`MCPLIST` and `MCPSAVE` are deleted). The server `xs56k_mcp_server.exe` of `juce/build-run` is the one to use; the one `.mcp.json` points to
(`juce/build/...`) is older than these commits and must be rebuilt before it is used again.

## What needs the owner

1. ~~TASK-MCP-063 at the sampler's screen~~ **Done with him on 2026-10-10**, with TASK-MCP-064 (the rows of the keys, the sentences of `README.md` and
   `CHANGELOG.md`). Still untried on the keys: the 39 others, the speed of the repetition of a held key, a server killed with a key held.
2. ~~A decision on DEC-MCP-029~~ **Made by the owner on 2026-10-10 and done (TASK-MCP-068)**: `confirm` of `clear_sampler_memory` counts the
   programs, samples, multis, song files, set lists and scenelists, which the command clears together.
3. **The owner's word on the rule of AGENTS.md** ("never call `clear_sampler_memory` or a `delete_all_*` tool on the owner's sampler unless he says so"):
   they were called on 2026-10-10 on his general instruction to do the tasks without him, after the whole memory was saved to the disk and reloaded.
   He may want the rule stated in a way that says whether such an instruction is enough.
4. **The review of ADR-MCP-002 to ADR-MCP-005** (still "Proposed"). The branch was pushed by the owner; read the CI results of its commits when they are in.
5. The effects on a real **EB20** stay untried (the owner has none).

## Not understood (written up, no action)

- Two reads of the list of songs got no answer at the start of TASK-MCP-058 (the screen on SONG LIST, right after a renaming); they never recurred.
- The play mode refused once with ERROR 03 (TASK-MCP-055).
- Opening the SET LIST page of the front panel moves the song selected through SysEx; the loads of files from the front panel moved the selected song
  and scenelist, those through SysEx did not. No rule is known.
- The sampler's own answer to a duplicate name (the server refuses first); whether a song loaded under a name already held is reloaded or ignored.

## Open besides, in this order

1. Whether the AKM library covers every function of the specification and the tools every function of the library has **not** been checked item by item.
2. `.gitignore` carries an uncommitted line (`.archify/`, the diagrams of the Archify skill) that is not the assistant's: left for the owner to commit or drop.
