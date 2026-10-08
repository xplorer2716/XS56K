# PLAN-MCP-005: MCP Server — The Remaining Sampler Functions

## Overview

Implements `FTR-MCP-005` on top of the server of `PLAN-MCP-001` to `PLAN-MCP-004`: the sampler's settings and MIDI setup, song files, set
lists and scenelists (with their saving and loading through the disk), delete all and clear memory, the effects board, and the
front-panel keys behind a launch option of their own (`ADR-MCP-005`). The owner decided on 2026-10-07 what is in and what is out;
ejecting a disk, formatting a disk and the refresh of the disk list are not part of this plan (DEC-MCP-028).

**Order of the work (DEC-MCP-034), from the least risk to the most.** Reading and plain setting first, deletions after the pattern of
the earlier ones is in place, the keys last, then the README and the real run:

| Step | Task | What | Why here |
|---|---|---|---|
| 1 | TASK-MCP-045 | the sampler's settings (name, clock, play mode, front-panel lock) | a Set and a Get each, nothing is lost, the smallest step |
| 2 | TASK-MCP-046 | the MIDI setup | Set only; the first tools that say they cannot read back |
| 3 | TASK-MCP-047 | song files, set lists, scenelists: list, select, rename | reading and renaming, no loss |
| 4 | TASK-MCP-048 | their deletion | the exact-name rule (DEC-MCP-023) on three more kinds |
| 5 | TASK-MCP-049 | saving and loading them through the disk | needs 047 and 048 and the disk unit of PLAN-MCP-003 |
| 6 | TASK-MCP-050 | delete all programs, samples, multis | the confirmation by count (DEC-MCP-029) |
| 7 | TASK-MCP-051 | clear the sampler's memory | the largest loss, after 050 has set the pattern |
| 8 | TASK-MCP-052 | the effects board | many tools, simulator only, no loss |
| 9 | TASK-MCP-053 | the front-panel keys behind `--allow-front-panel` | the one class that bypasses every confirmation: last |
| 10 | TASK-MCP-054 | README, scripted conversations, counts, closure | after the tools exist |
| 11 | TASK-MCP-055 | the real-sampler run with the owner | after everything is implemented; needs the owner's test data |

**Safety note.** The new tools delete or clear memory items, song files, set lists and scenelists, and can press the sampler's keys.
Every `ctest` run is against the simulated sampler. The real run (TASK-MCP-055) is made with the owner present, on test data listed
beforehand, one slow command at a time, never with the refresh of the disk list and never with `clear_sampler_memory` or a key that
answers a delete or save screen unless the owner says so (AGENTS.md). Every commit carries "(HOL -Human on the loop)" after the
subject the `agnos-git-workflow` skill computes. Each task adds its tools to the README tables in the same commit
(`mcp_readme_names_every_tool_and_option` fails otherwise) and widens `CheckNoDestructiveCalls.cmake` only for the primitive it adds.

## References
- **Requirements**: RQ-MCP-046 to RQ-MCP-057 (`FTR-MCP-005`); RQ-MCP-042, RQ-MCP-043, RQ-MCP-044 (`FTR-MCP-004`); RQ-MCP-014 (`FTR-MCP-002`)
  and RQ-MCP-025 to RQ-MCP-028, RQ-MCP-031 (`FTR-MCP-003`)
- **ADRs**: ADR-MCP-005 (Proposed): DEC-MCP-028 to DEC-MCP-034; ADR-MCP-004: DEC-MCP-023, DEC-MCP-025 (amended), DEC-MCP-027; ADR-MCP-003:
  DEC-MCP-019 (amended earlier)

The plan has 12 tasks (TASK-MCP-044 to TASK-MCP-055), more than the 10 of the CONTEXT MANAGEMENT rule: the work is split across
sessions, with a checkpoint note in `process/3.plan/` at the end of each, unless the owner lifts the limit as he did on 2026-10-05.
Tier S for 044, M for 045 to 051 and 054 to 055, L for 052 and 053 (many tools, a new unit or a new launch option). Every task is
presented to the owner and waits for his approval before it starts (Definition of Ready).

---

## Tasks

### TASK-MCP-044: Author FTR-MCP-005, ADR-MCP-005 and PLAN-MCP-005, and amend the "never offered" texts
- **Tier**: S
- **Status**: Done
- **Description**: Write the feature, the decision record and this plan from the owner's decisions of 2026-10-07; amend RQ-MCP-014, RQ-MCP-028 and ADR-MCP-004 DEC-MCP-025 so that they point to the new decisions instead of saying "never".
- **Requirement refs**: RQ-MCP-046 to RQ-MCP-057
- **ADR refs**: ADR-MCP-005 (DEC-MCP-028 to DEC-MCP-034); ADR-MCP-004 (DEC-MCP-025)
- **Acceptance Criteria** (Gherkin): *Given* the owner's decisions, *When* the three documents are read, *Then* each item of the owner's list is a requirement or an exclusion, every task of this plan names its requirements and decisions, and the process index lists them with no duplicate ID.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: DONE on 2026-10-07: the `agnos-index` skill reports the new FTR, ADR, PLAN and their tasks with no duplicate; RQ-MCP-014, RQ-MCP-028 and DEC-MCP-025 each carry an "Amended 2026-10-07" note pointing to ADR-MCP-005.
- **Assumptions**: the owner's "tout supprimer" is read as the four items of the table (delete all programs, samples, multis; clear memory); "mécanisme standard de confirmation" is read as the count for the bulk tools (DEC-MCP-029), because the bulk tool `save_all_memory_items` already takes a count and no single name exists; the owner reviews both when he reviews the ADR.

### TASK-MCP-045: The sampler's settings
- **Tier**: M
- **Status**: Done
- **Description**: `get_sampler_settings` (name, clock, play mode, front-panel lock) and `set_sampler_setting` (a setting and a value: validate, send, read back, answer). Check what the simulated sampler already models of the four settings and complete it; add the tools, their catalogue of settings and values, the README rows and the scripted conversation lines.
- **Requirement refs**: RQ-MCP-046
- **ADR refs**: ADR-MCP-005 (DEC-MCP-030)
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler, *When* the name, the clock, the play mode and the front-panel lock are set, *Then* `get_sampler_settings` answers each as set; *given* an invalid date, a name that is too long or a lock the sampler does not have, *Then* nothing is sent and the answer says what is accepted.
- **Dependencies**: TASK-MCP-044
- **Assignee**: AI
- **Verification**: DONE on 2026-10-07, from the tools' own output. Red first: `xs56k_mcp_tests "[settings]"` ("test cases: 9 | 0 passed | 9 failed", "assertions: 231 | 54 passed | 177 failed": the tools did not exist). Green after the gateway methods and the two tools: "All tests passed (263 assertions in 9 test cases)" (`SamplerSettingsToolsTests.cpp`: the tool annotations; the name set, read back and refused at 21 characters, empty or beyond ASCII; the clock set for five dates including a leap day and the two ends of 1980-2079, with the day of the week worked out, and refused for twelve bad inputs, each naming the field and the format; the four play modes read back, with "plays nothing" said for muted only; the front-panel lock with how to unlock it; the missing, unknown and badly typed arguments, with nothing sent). `ctest` excluding only `bld_mutate_tool_script_tests`: 989 of 989 pass, after the two scripted conversations were extended with nine calls (`simulated_session.jsonl`, `EXPECT_LINES` 114 to 123) and their expected output regenerated (the differences read one by one: the nine new answers and the two new tool definitions in the discovery answer). The tool tables of `juce/mcp/README.md` and the counts in the three documents (34 tools, 50 with `--allow-disk`) are up to date. Not run on the real sampler yet (TASK-MCP-055).
- **Assumptions**: the clock text is `YYYY-MM-DD HH:MM:SS` (a `T` is accepted in place of the space) and the day of the week is worked out by the tool, since the sampler takes it as a field of its own and a wrong one would be the caller's mistake; the checks run in the order year, month, day, hours, minutes, seconds and name the first field outside its range; the sampler's name is 1 to 20 characters of printable ASCII (the AKM item takes up to 20; whether the sampler keeps a name of 20 is not observed); the setting names are `name`, `clock`, `play_mode` and `front_panel`; Muted (play mode 3) was confirmed on a real S5000 by the AKM suite on 2026-10-01 (`sysex_spec.kb.md`), so the tool offers it, with a warning that it plays nothing.

### TASK-MCP-046: The MIDI setup
- **Tier**: M
- **Status**: Not Started
- **Description**: `set_midi_setting` (program change, multi select mode and channel, external APM controller, aftertouch type) and `set_midi_filter` (allow or ignore a MIDI event on a channel). Each answer says what was sent and that the previous value could not be read and cannot be put back (§04 has no Get).
- **Requirement refs**: RQ-MCP-047
- **ADR refs**: ADR-MCP-005 (DEC-MCP-030)
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler, *When* each setting and a filter are set, *Then* the simulated sampler holds them and the answer says the previous value is unknown; *given* a channel of 17 or an unknown event, *Then* nothing is sent.
- **Dependencies**: TASK-MCP-045
- **Assignee**: AI
- **Verification**: Not started.
- **Assumptions**: None yet.

### TASK-MCP-047: Song files, set lists and scenelists — list, select, rename
- **Tier**: M
- **Status**: Not Started
- **Description**: `list_song_files`, `select_song_file`, `rename_song_file`; `list_set_lists`, `rename_set_list` (by name, resolved to an index); `list_scenelists`, `select_scenelist`, `rename_scenelist`. The simulated sampler already keeps their names (`setSongNames`, `setSetListNames`, `setSceneListNames`); check that its lists, its current selection and its refusals match the specification, and add what is missing.
- **Requirement refs**: RQ-MCP-048
- **ADR refs**: ADR-MCP-005 (DEC-MCP-031)
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding song files, a set list and scenelists, *When* each list tool is called, *Then* it answers the names in order with the current one marked; *when* one is selected or renamed, *then* the state read back is the new one; *given* an unknown or invalid name, *then* nothing is sent.
- **Dependencies**: TASK-MCP-044
- **Assignee**: AI
- **Verification**: Not started.
- **Assumptions**: None yet.

### TASK-MCP-048: Song files, set lists and scenelists — deletion
- **Tier**: M
- **Status**: Not Started
- **Description**: `delete_song_file`, `delete_set_list`, `delete_scenelist`, each with `confirm` = the exact name (DEC-MCP-023), destructive annotation, the AKM delete primitives allowed in the gateway's memory unit only, `CheckNoDestructiveCalls.cmake` widened for them.
- **Requirement refs**: RQ-MCP-049, RQ-MCP-042, RQ-MCP-057
- **ADR refs**: ADR-MCP-005 (DEC-MCP-031); ADR-MCP-004 (DEC-MCP-023)
- **Acceptance Criteria** (Gherkin): *Given* each tool, *When* it is called with no `confirm`, a wrong one and the right one, *Then* the first two send nothing and name the `confirm` expected and the third deletes it; *given* a call to one of the primitives in a tool file, *then* the source check fails.
- **Dependencies**: TASK-MCP-047
- **Assignee**: AI
- **Verification**: Not started.
- **Assumptions**: None yet.

### TASK-MCP-049: Saving and loading song files, set lists and scenelists through the disk
- **Tier**: M
- **Status**: Not Started
- **Description**: Add the kinds `song_file`, `set_list` and `scenelist` to `save_memory_item` and `save_all_memory_items`; make `load_file` load their files; settle against the simulated sampler whether `load_file` needs more than the file name. The simulated sampler's disk gains files of those kinds.
- **Requirement refs**: RQ-MCP-050, RQ-MCP-025, RQ-MCP-026, RQ-MCP-027
- **ADR refs**: ADR-MCP-005 (DEC-MCP-031); ADR-MCP-003 (DEC-MCP-019, DEC-MCP-021)
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler with a writable disk, *When* a song file is saved, *Then* a file of that name is in the folder and a second save is refused without `overwrite`; *when* that file is loaded, *then* the sampler's song files include it; the same for a set list and a scenelist; the bulk save takes the count as `confirm`.
- **Dependencies**: TASK-MCP-047, TASK-MCP-048
- **Assignee**: AI
- **Verification**: Not started.
- **Assumptions**: None yet. The file extensions of the three kinds on a real disk are not known; the simulator's choice is marked as an assumption until the real run (TASK-MCP-055) shows them.

### TASK-MCP-050: Delete all programs, all samples, all multis
- **Tier**: M
- **Status**: Not Started
- **Description**: `delete_all_programs`, `delete_all_samples`, `delete_all_multis`, each with `confirm` = the number of items of that kind (DEC-MCP-029), destructive annotation, the AKM Delete ALL primitives allowed in the gateway's memory unit only, the source check widened.
- **Requirement refs**: RQ-MCP-051, RQ-MCP-042, RQ-MCP-057
- **ADR refs**: ADR-MCP-005 (DEC-MCP-029)
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler with 3 programs, *When* `delete_all_programs` is called with "2", *Then* nothing is sent and the answer says 3; *when* it is called with "3", *then* `list_programs` says there is none; the same for samples and multis.
- **Dependencies**: TASK-MCP-048
- **Assignee**: AI
- **Verification**: Not started.
- **Assumptions**: None yet.

### TASK-MCP-051: Clear the sampler's memory
- **Tier**: M
- **Status**: Not Started
- **Description**: `clear_sampler_memory`, with `confirm` = the total number of programs, samples and multis held (DEC-MCP-029), refused with nothing sent when the sampler holds none of the three, destructive annotation, the AKM primitive allowed in the gateway's memory unit only. Not run on the real sampler unless the owner decides.
- **Requirement refs**: RQ-MCP-052, RQ-MCP-042, RQ-MCP-057
- **ADR refs**: ADR-MCP-005 (DEC-MCP-029, DEC-MCP-033)
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler with 2 programs, 1 sample and 1 multi, *When* `clear_sampler_memory` is called with "3", *Then* nothing is sent and the answer says 4; *when* called with "4", *then* the three lists are empty; *given* an empty memory, *then* nothing is sent and the answer says so.
- **Dependencies**: TASK-MCP-050
- **Assignee**: AI
- **Verification**: Not started.
- **Assumptions**: None yet.

### TASK-MCP-052: The effects board
- **Tier**: L
- **Status**: Not Started
- **Description**: The tools to read the card, the channels and the modules, and to set a channel's mute, a module's type and on/off state, and a module parameter, with values checked against the layout the sampler reports and a clear answer when it reports no board (`get_fx_board`, `set_fx_channel_mute`, `set_fx_module`, `get_fx_parameter`, `set_fx_parameter`; the final names and arguments are fixed in the task). Simulator only.
- **Requirement refs**: RQ-MCP-053
- **ADR refs**: ADR-MCP-005 (DEC-MCP-033)
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler with an EB20 layout, *When* a module's type and a parameter are set, *Then* they are read back as set and a value outside the module's range sends nothing; *given* no board, *then* every effects tool says so.
- **Dependencies**: TASK-MCP-044
- **Assignee**: AI
- **Verification**: Not started.
- **Assumptions**: None yet. The README states that these tools have been run on the simulated sampler only.

### TASK-MCP-053: The front-panel keys behind `--allow-front-panel`
- **Tier**: L
- **Status**: Not Started
- **Description**: The launch option `--allow-front-panel` (usage text, README options table, `--help`), and behind it the tools to press, hold and release a key, turn the data wheel and send an ASCII key, in their own unit of the gateway; a held key is released when the session closes; the source check allows the front-panel primitives only in that unit.
- **Requirement refs**: RQ-MCP-054, RQ-MCP-057
- **ADR refs**: ADR-MCP-005 (DEC-MCP-032)
- **Acceptance Criteria** (Gherkin): *Given* the server without the option, *When* the tools are listed, *Then* none of the key tools is there and a call is refused as unknown; *given* the option, *when* a key is held and the session closes, *then* the simulated sampler received the release.
- **Dependencies**: TASK-MCP-051
- **Assignee**: AI
- **Verification**: Not started.
- **Assumptions**: None yet. The README warns that a key can answer "ENT" to a delete or save screen.

### TASK-MCP-054: README, scripted conversations, counts and closure
- **Tier**: M
- **Status**: Not Started
- **Description**: Bring `juce/mcp/README.md` (tool and option tables, what has and has not been tried, the exclusions now reduced to eject, format and the refresh), the scripted conversations, the tool counts in `README.md`, `AGENTS.md` and the mcp README, and the "what stays out" texts into line with what exists; check that the README test passes with all options on.
- **Requirement refs**: RQ-MCP-043, RQ-MCP-055
- **ADR refs**: ADR-MCP-005 (DEC-MCP-028, DEC-MCP-034)
- **Acceptance Criteria** (Gherkin): *Given* the README, *When* its tool tables and the server's list with every option on are compared, *Then* every tool and option is there and the counts in the three documents are the ones the server reports.
- **Dependencies**: TASK-MCP-045 to TASK-MCP-053
- **Assignee**: AI
- **Verification**: Not started.
- **Assumptions**: None yet.

### TASK-MCP-055: Real-sampler run of the new tools with the owner
- **Tier**: M
- **Status**: Blocked
- **Description**: With the owner present, run each new tool on the real S5000, one at a time, on test data (below), write up what the sampler did in `OBSERVATIONS-RQ-MCP-012-real-sampler.md`, update the README's table "what has been tried", and correct the simulated sampler for what it had wrong. Exceptions, listed in the README: the effects board (no board), `clear_sampler_memory` (unless the owner decides), the front-panel keys other than harmless ones the owner watches.
- **Requirement refs**: RQ-MCP-056, RQ-MCP-044
- **ADR refs**: ADR-MCP-005 (DEC-MCP-033)
- **Acceptance Criteria** (Gherkin): *Given* the owner present and the test data on the sampler, *When* each tool is run alone, *Then* its answer and what the sampler did are in the observations file, the objects made for the run are removed or listed, and the simulated sampler matches what was seen.
- **Dependencies**: TASK-MCP-054 (and the owner's test data)
- **Assignee**: Human and AI
- **Verification**: Not started (Blocked: needs the owner).
- **Assumptions**: **Test data the owner is asked to put on the sampler** before the run, listed here so that nothing is invented: at least two song files, two set lists and two scenelists in the sampler's memory, and one saved file of each kind on the disk (the owner says how he made them and what the extensions are); programs, samples and multis made for the run are made by the assistant and deleted by it. The set lists, song files and scenelists the owner already has must not be used for a deletion test: only copies made for the run.
