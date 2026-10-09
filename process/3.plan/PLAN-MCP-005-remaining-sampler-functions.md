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
| 11 | TASK-MCP-056 | `CHANGELOG.md` | it had been left behind |
| 12 | TASK-MCP-057 | the plan of the real run of every tool | the owner's decision of 2026-10-09 |
| 13 | TASK-MCP-055 | real run: settings and MIDI setup | nothing is lost, every value is put back |
| 14 | TASK-MCP-058 | real run: song files, set lists, scenelists in memory | on copies only |
| 15 | TASK-MCP-059 | real run: their saving and loading through the disk | in a folder made for the run |
| 16 | TASK-MCP-060 | real run: the effects board | no board: the "no board" answers only |
| 17 | TASK-MCP-061 | real run: delete all programs, samples, multis | on test objects only |
| 18 | TASK-MCP-062 | real run: clear the sampler's memory | the largest loss, after 061 |
| 19 | TASK-MCP-063 | real run: the front-panel keys | the class that bypasses every confirmation: last |
| 20 | TASK-MCP-064 | README, observations, simulated sampler, closure | after every run |

**Safety note.** The new tools delete or clear memory items, song files, set lists and scenelists, and can press the sampler's keys.
Every `ctest` run is against the simulated sampler. The real run (TASK-MCP-055) is made with the owner present, on test data listed
beforehand, one slow command at a time, never with the refresh of the disk list and never with `clear_sampler_memory` or a key that
answers a delete or save screen unless the owner says so (AGENTS.md). Every commit carries "(HOL -Human on the loop)" after the
subject the `agnos-git-workflow` skill computes. Each task adds its tools to the README tables in the same commit
(`mcp_readme_names_every_tool_and_option` fails otherwise) and widens `CheckNoDestructiveCalls.cmake` only for the primitive it adds.

**Real-sampler runs (TASK-MCP-055, TASK-MCP-058 to TASK-MCP-063; the owner's decision of 2026-10-09: every tool of this plan is run on
the real S5000).** Each run is made with the owner present, with `xs56k_mcp_server` built from this branch and registered in the MCP
client with only the options the task names (`--allow-disk-refresh` never), one tool call at a time, its answer written to a file and
never cut by a pipe (a killed client once left the S5000 silent). Each destructive call (a deletion, a delete-all, clearing the memory,
a key) is sent only after the owner says so for that call. Every value a run changes is read or declared first and put back. What the
sampler did is written up in `OBSERVATIONS-RQ-MCP-012-real-sampler.md`, and the simulated sampler is corrected in the same task when it
differs. **Test data the owner is asked to put on the sampler** before TASK-MCP-058: at least two song files, two set lists and two
scenelists in memory, and one saved file of each kind on the disk (the owner says how he made them and what the extensions are);
programs, samples and multis made for a run are made by the assistant and deleted by it. The set lists, song files and scenelists the
owner already has are not used for a deletion: only copies made for the run. Before TASK-MCP-061 and TASK-MCP-062 the owner saves what
he wants to keep: the memory then holds only what the runs made.

## References
- **Requirements**: RQ-MCP-046 to RQ-MCP-057 (`FTR-MCP-005`; RQ-MCP-056 amended 2026-10-09); RQ-MCP-042, RQ-MCP-043, RQ-MCP-044 (`FTR-MCP-004`); RQ-MCP-014 (`FTR-MCP-002`)
  and RQ-MCP-025 to RQ-MCP-028, RQ-MCP-031 (`FTR-MCP-003`)
- **ADRs**: ADR-MCP-005 (Proposed): DEC-MCP-028 to DEC-MCP-034 (DEC-MCP-033 amended 2026-10-09); ADR-MCP-004: DEC-MCP-023, DEC-MCP-025 (amended), DEC-MCP-027; ADR-MCP-003:
  DEC-MCP-019 (amended earlier)

The plan has 21 tasks (TASK-MCP-044 to TASK-MCP-064; 056 to 064 were added on 2026-10-09 and 055 reworked the same day), more than the 10 of the CONTEXT MANAGEMENT rule: the work is split across
sessions, with a checkpoint note in `process/3.plan/` at the end of each, unless the owner lifts the limit as he did on 2026-10-05.
Tier S for 044, M for 045 to 051 and 054 to 064, L for 052 and 053 (many tools, a new unit or a new launch option). Every task is
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
- **Status**: Done
- **Description**: `set_midi_setting` (program change, multi select mode and channel, external APM controller, aftertouch type) and `set_midi_filter` (allow or ignore a MIDI event on a channel). Each answer says what was sent and that the previous value could not be read and cannot be put back (§04 has no Get).
- **Requirement refs**: RQ-MCP-047
- **ADR refs**: ADR-MCP-005 (DEC-MCP-030)
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler, *When* each setting and a filter are set, *Then* the simulated sampler holds them and the answer says the previous value is unknown; *given* a channel of 17 or an unknown event, *Then* nothing is sent.
- **Dependencies**: TASK-MCP-045
- **Assignee**: AI
- **Verification**: DONE on 2026-10-08, from the tools' own output. Red first: `xs56k_mcp_tests "[midi]"`: 12 test cases, 0 passed. Green: "All tests passed (308 assertions in 12 test cases)" (`MidiSetupToolsTests.cpp`: annotations, the five switches held by the simulated sampler, the channel forms, the refused values and channels with nothing sent, the filter on the four events, the argument errors). Full `ctest` on Linux (GCC 13, built with `-Wno-dangling-reference`, a false positive of GCC 13 in `RealSamplerSuiteTests.cpp`, which this task did not touch): 1001 of 1001 pass, and `bld_mutate_tool_script_tests` 1 of 1. The two scripted conversations gained six calls (`EXPECT_LINES` 123 to 129); their expected output was regenerated and the diff read: the six answers and the two new tool definitions. 36 tools, 52 with `--allow-disk` (`juce/mcp/README.md` and `README.md`). Not run on the real sampler (TASK-MCP-055).
- **Assumptions**: the MIDI channel is the sampler's own number, 0 to 31 (1A = 0 ... 16B = 31, `MidiConfig.hpp`), as the owner chose on 2026-10-08 (a first version took the front-panel names 1A to 16B); the answers add the name, such as `3 (= 4A)`. RQ-MCP-047's criterion "a channel of 17 is refused" became "a channel of 32" (17 is channel 2B), amended in FTR-MCP-005; the tests were changed for that reason only, and a letter in a channel is refused. The settings are `program_change`, `multi_select`, `multi_select_channel`, `external_apm_controller` and `aftertouch`; the filter's events are `note_on`, `aftertouch`, `wheels` and `volume`, its actions `allow` and `ignore`. `multi_select` takes `off`, `program_change` or `bank`, the three bytes of the AKM item. `CheckNoDestructiveCalls.cmake` needed no change (nothing the new primitives call deletes).

### TASK-MCP-047: Song files, set lists and scenelists — list, select, rename
- **Tier**: M
- **Status**: Done
- **Description**: `list_song_files`, `select_song_file`, `rename_song_file`; `list_set_lists`, `rename_set_list` (by name, resolved to an index); `list_scenelists`, `select_scenelist`, `rename_scenelist`. The simulated sampler already keeps their names (`setSongNames`, `setSetListNames`, `setSceneListNames`); check that its lists, its current selection and its refusals match the specification, and add what is missing.
- **Requirement refs**: RQ-MCP-048
- **ADR refs**: ADR-MCP-005 (DEC-MCP-031)
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding song files, a set list and scenelists, *When* each list tool is called, *Then* it answers the names in order with the current one marked; *when* one is selected or renamed, *then* the state read back is the new one; *given* an unknown or invalid name, *then* nothing is sent.
- **Dependencies**: TASK-MCP-044
- **Assignee**: AI
- **Verification**: DONE on 2026-10-08, from the tools' own output. Red first: `xs56k_mcp_tests "[namedlists]"`: 5 test cases failed, then a fatal error stopped the run (the tools did not exist). Green: "All tests passed (343 assertions in 15 test cases)" (`NamedListToolsTests.cpp`). Full `ctest` on Linux (GCC 13, `-Wno-dangling-reference` as in TASK-MCP-046): 1017 of 1017 pass, `mcp_sources_call_no_destructive_primitive` and `mcp_readme_names_every_tool_and_option` included. The scripted conversation gained 11 calls (`EXPECT_LINES` 129 to 140) and the simulated server now holds 3 song files, 2 set lists and 2 scenelists; both expected outputs regenerated and the diff read (the 11 answers, the 8 new tool definitions, nothing else). 44 tools, 60 with `--allow-disk`. Not run on the real sampler (TASK-MCP-055).
- **Assumptions**: (1) **Divergence from the acceptance criterion of RQ-MCP-048**: a rename to a name already used is refused by the server before anything is sent (for a song file or a scenelist: another item bears it without regard to case, spaces or hyphens; for a set list likewise), instead of "the answer is what the sampler reported"; the spec is silent on duplicates and the simulated sampler accepts them, so a rename sent would have duplicated the name. What the real sampler does is to be observed in TASK-MCP-055. Renaming an item to its own name is accepted. (2) **Divergence from "an unknown name: nothing is sent"**: `select_song_file` and `select_scenelist` with a name or a position the sampler does not have SEND the select, and the sampler's ERROR 04 is turned into "No song file is named ...": the selection is unchanged, but a command was sent (checking the list first would cost one read per item). For a set list the name is resolved from the list first, so an unknown name sends nothing. (3) The sampler's answer to "which one is current" while none is selected is modelled as ERROR 04 (the spec is silent, RQ-AKM-082); the tool reads it as "none selected". (4) Names are 1 to 20 characters of plain ASCII (the manual: "A name can consist of up to 20 characters"); the AKM items take up to 255 and no run has observed the limit of these three kinds. (5) The three lists are read name by name (count, then one Get per item, then the current index): the spec has no "all names" item for them, so a long list costs as many round trips. (6) A set list is found by its exact name, else by the only one that differs by case, spaces or hyphens; two of the same name are refused with their positions. (7) The position of a selection is 0 to 16383 (two 7-bit data bytes). (8) The three rename primitives are called from a new unit, `SamplerGatewayLists.cpp`, and `CheckNoDestructiveCalls.cmake` allows them there only (RQ-MCP-057). (9) The simulated server (test tooling) is seeded with the lists; the tests of the tools seed their own.

### TASK-MCP-048: Song files, set lists and scenelists — deletion
- **Tier**: M
- **Status**: Done
- **Description**: `delete_song_file`, `delete_set_list`, `delete_scenelist`, each with `confirm` = the exact name (DEC-MCP-023), destructive annotation, the AKM delete primitives allowed in the gateway's memory unit only, `CheckNoDestructiveCalls.cmake` widened for them.
- **Requirement refs**: RQ-MCP-049, RQ-MCP-042, RQ-MCP-057
- **ADR refs**: ADR-MCP-005 (DEC-MCP-031); ADR-MCP-004 (DEC-MCP-023)
- **Acceptance Criteria** (Gherkin): *Given* each tool, *When* it is called with no `confirm`, a wrong one and the right one, *Then* the first two send nothing and name the `confirm` expected and the third deletes it; *given* a call to one of the primitives in a tool file, *then* the source check fails.
- **Dependencies**: TASK-MCP-047
- **Assignee**: AI
- **Verification**: DONE on 2026-10-08, from the tools' own output. Red first: `xs56k_mcp_tests "[deletelists]"`: 7 test cases, 0 passed. Green: "All tests passed (156 assertions in 7 test cases)" (`NamedListDeleteToolsTests.cpp`: annotations; no `confirm`, a wrong one and the right one for each of the three tools; none selected; the set list found in other letters; unknown and doubled names; argument errors). The source check: `mcp_sources_call_no_destructive_primitive` passes on the real sources and the new `mcp_source_check_fails_on_a_delete_call_in_a_tool_file` passes because the script fails on `mcp/forbidden_call_fixture/Tools.cpp`, which calls `akm::deleteSetList`. Full `ctest` on Linux (GCC 13, `-Wno-dangling-reference`): 1025 of 1025 pass. The scripted conversation gained 5 calls (`EXPECT_LINES` 140 to 145); both expected outputs regenerated and the diff read (the 5 answers, the 3 new tool definitions). 47 tools, 63 with `--allow-disk`. Not run on the real sampler (TASK-MCP-055).
- **Assumptions**: (1) The delete primitives are called from `SamplerGatewayLists.cpp` (the lists unit created in TASK-MCP-047, next to the rename primitives), not from "the gateway's memory unit" (`SamplerGateway.cpp`) the task text names: one unit per family keeps RQ-MCP-057's single call site; `CheckNoDestructiveCalls.cmake` allows the three delete names there only. (2) `delete_song_file` and `delete_scenelist` delete the CURRENT one (the sampler's primitives act on the current item), with `confirm` = its exact name, like `delete_multi`; `delete_set_list` takes `name` (found as for the rename: exactly, else the only one differing by case, spaces or hyphens) AND `confirm`, which must be the exact name the list gives, so a loosely typed name cannot delete by itself. (3) The answer after a deletion lists the names that remain (RQ-MCP-049), re-read from the sampler. (4) After deleting the current song file or scenelist the simulated sampler has none selected (modelled; the real sampler's selection after a deletion is unobserved). (5) The acceptance criterion "a call to the primitive in a tool file makes the source check fail" is proved with a fixture directory that the script scans, not by editing the real `Tools.cpp`.

### TASK-MCP-049: Saving and loading song files, set lists and scenelists through the disk
- **Tier**: M
- **Status**: Done
- **Description**: Add the kinds `song_file`, `set_list` and `scenelist` to `save_memory_item` and `save_all_memory_items`; make `load_file` load their files; settle against the simulated sampler whether `load_file` needs more than the file name. The simulated sampler's disk gains files of those kinds.
- **Requirement refs**: RQ-MCP-050, RQ-MCP-025, RQ-MCP-026, RQ-MCP-027
- **ADR refs**: ADR-MCP-005 (DEC-MCP-031); ADR-MCP-003 (DEC-MCP-019, DEC-MCP-021)
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler with a writable disk, *When* a song file is saved, *Then* a file of that name is in the folder and a second save is refused without `overwrite`; *when* that file is loaded, *then* the sampler's song files include it; the same for a set list and a scenelist; the bulk save takes the count as `confirm`.
- **Dependencies**: TASK-MCP-047, TASK-MCP-048
- **Assignee**: AI
- **Verification**: DONE on 2026-10-08, from the tools' own output. Red first: `xs56k_mcp_tests "[namedlists][disk]"`: 8 test cases, 1 passed (the program-file one that did not change), 7 failed. Green: "All tests passed (522 assertions in 23 test cases)" (`DiskNamedListTests.cpp`: a song file, a set list and a scenelist saved with their type and position; a second save refused without `overwrite`, accepted with it; an unknown item; the six kinds, in other letters or with a space; the bulk save of each kind with its count, a wrong count refused; the three files deleted from memory then loaded back, with the counts before and after; a program load that does not speak of the three). Full `ctest` on Linux (GCC 13, `-Wno-dangling-reference`): 1033 of 1033 pass. The disk conversation gained 6 calls (`EXPECT_LINES` 45 to 51); the expected output regenerated and the diff read (the 6 answers and the descriptions of `load_file`, `save_memory_item` and `save_all_memory_items`). No tool added: 47 and 63. Not run on the real sampler (TASK-MCP-055).
- **Assumptions**: (1) **The file extensions of the three kinds on a real disk are not known**: the simulated sampler writes `.MID` (a song file is a standard MIDI file, `Smf` in the AKM layer), `.SET` and `.SCN`, and sizes of 512 bytes, all PLACEHOLDERS to be replaced by what TASK-MCP-055 shows. The tools never use the extension: a saved item is found by its name without the extension (`fileBearing`), and `load_file` takes the name the listing gives. (2) `load_file` needs nothing beyond the file name for these kinds: the sampler's `Load File` (§10/&2A) takes a name and a sample option; the simulated sampler loads a song file, set list or scenelist file by its marker whatever the option. Whether the real sampler decides the kind of a file by its extension (as the tool description says for the three older kinds) is unobserved. (3) Loading a file whose item is already in memory ADDS a second item of the same name (as the simulated sampler does for a program); what the real sampler does is unobserved. (4) The kind argument is `song_file`, `set_list` or `scenelist` as the plan names them, also accepted in other letters, with a space or a hyphen for the underscore. (5) `save_children` is accepted for these kinds and ignored by the simulated sampler (they depend on nothing). (6) `describeMemory` speaks of song files, set lists and scenelists only when a load changed them, so every earlier answer is unchanged. (7) `memoryNames()` now reads the three lists too (a count and a name each), so a load or a save costs a few more round trips. (8) The simulator's `FileRecord` gains the three markers AFTER its existing fields, because the tests build it by position.

### TASK-MCP-050: Delete all programs, all samples, all multis
- **Tier**: M
- **Status**: Done
- **Description**: `delete_all_programs`, `delete_all_samples`, `delete_all_multis`, each with `confirm` = the number of items of that kind (DEC-MCP-029), destructive annotation, the AKM Delete ALL primitives allowed in the gateway's memory unit only, the source check widened.
- **Requirement refs**: RQ-MCP-051, RQ-MCP-042, RQ-MCP-057
- **ADR refs**: ADR-MCP-005 (DEC-MCP-029)
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler with 3 programs, *When* `delete_all_programs` is called with "2", *Then* nothing is sent and the answer says 3; *when* it is called with "3", *then* `list_programs` says there is none; the same for samples and multis.
- **Dependencies**: TASK-MCP-048
- **Assignee**: AI
- **Verification**: DONE on 2026-10-08, from the tools' own output. Red first: `xs56k_mcp_tests "[bulkdelete]"`: 5 test cases, 0 passed. Green: "All tests passed (164 assertions in 5 test cases)" (`BulkDeleteToolsTests.cpp`: annotations; for programs (3), samples (3) and multis (2) no `confirm`, a wrong count and the right count, the list saying the sampler holds none; a tool deletes its own kind only; a sampler holding none; confirm that is not a whole number from 1). Full `ctest` on Linux (GCC 13, `-Wno-dangling-reference`): 1038 of 1038 pass, `mcp_sources_call_no_destructive_primitive` included (the check now allows the three Delete ALL names in `SamplerGateway.cpp` only; `mcp_source_check_fails_on_a_delete_call_in_a_tool_file` still passes). The scripted conversation gained 4 calls (`EXPECT_LINES` 145 to 149); expected outputs regenerated and the diff read (the 4 answers and the 3 new tool definitions). 50 tools, 66 with `--allow-disk`. Not run on the real sampler (TASK-MCP-055).
- **Assumptions**: (1) `confirm` is a JSON integer from 1: a text such as "3" (the acceptance criterion writes the counts in quotes) is refused, like the count of `save_all_memory_items`. (2) A sampler that holds none of the kind sends nothing and says so, whatever the `confirm`. (3) The count is read just before the command is sent; it is not atomic with it (a change made at the sampler's panel in between is not seen). (4) The three Delete ALL primitives and their typed confirmations are called from `SamplerGateway.cpp`, the gateway's memory unit, as the task says; `CheckNoDestructiveCalls.cmake` allows exactly those six names there. (5) After the deletion the gateway counts again and fails if the sampler still holds some. (6) No launch option, per DEC-MCP-029. (7) What the real sampler does with the multis' parts when all programs go, or with a program's zones when all samples go, is unobserved: the simulated sampler keeps them untouched. (8) **Two earlier tests were changed**: `SampleToolsTests.cpp` and `MultiToolsTests.cpp` asserted that no tool is named `delete_all_samples` or `delete_all_multis`; RQ-MCP-051 (amending RQ-MCP-014 and RQ-MCP-028) now requires them, so each checks that the tool is offered and destructive instead.

### TASK-MCP-051: Clear the sampler's memory
- **Tier**: M
- **Status**: Done
- **Description**: `clear_sampler_memory`, with `confirm` = the total number of programs, samples and multis held (DEC-MCP-029), refused with nothing sent when the sampler holds none of the three, destructive annotation, the AKM primitive allowed in the gateway's memory unit only. Not run on the real sampler unless the owner decides.
- **Requirement refs**: RQ-MCP-052, RQ-MCP-042, RQ-MCP-057
- **ADR refs**: ADR-MCP-005 (DEC-MCP-029, DEC-MCP-033)
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler with 2 programs, 1 sample and 1 multi, *When* `clear_sampler_memory` is called with "3", *Then* nothing is sent and the answer says 4; *when* called with "4", *then* the three lists are empty; *given* an empty memory, *then* nothing is sent and the answer says so.
- **Dependencies**: TASK-MCP-050
- **Assignee**: AI
- **Verification**: DONE on 2026-10-08, from the tools' own output, on the simulated sampler only. Red first: `xs56k_mcp_tests "[clearmemory]"`: 5 test cases, 0 passed. Green: "All tests passed (104 assertions in 5 test cases)" (`ClearMemoryToolTests.cpp`: annotations; with 2 programs, 1 sample and 1 multi no `confirm`, 3 and 4 (the answer says 4, nothing sent, then the three lists empty); a second call on an empty memory; song files, set lists and scenelists still held; confirm that is not a whole number from 1). Full `ctest` on Linux (GCC 13, `-Wno-dangling-reference`): 1043 of 1043 pass (the source check allows `akm::clearSamplerMemory` and its typed confirmation in `SamplerGateway.cpp` only). The scripted conversation gained 4 calls (`EXPECT_LINES` 149 to 153); expected outputs regenerated and the diff read. 51 tools, 67 with `--allow-disk`. NOT run on the real sampler, and `AGENTS.md` now says not to without the owner's word.
- **Assumptions**: (1) As in DEC-MCP-029 the total counts programs, samples and multis only; the song files, set lists and scenelists are NOT counted and the simulated sampler keeps them, but the real sampler's Clear Sampler Memory may clear them too (the spec says "every program, multi and sample", RQ-AKM-056): unobserved, to review before any real run. (2) The AKM primitive takes no timeout of its own and the sampler's time to answer has never been observed (no run is allowed to send it), so the session's command timeout applies; a real sampler that takes longer will be reported as not answering, and the tool description and the README say it may have to be switched off and on. Nothing is retried. (3) The counts are read just before the command and again after it; a count other than zero afterwards is reported as a failure. (4) `confirm` is an integer from 1 (a text is refused), as for the delete-all tools. (5) The primitive and its typed confirmation are called from `SamplerGateway.cpp` (the memory unit) and `CheckNoDestructiveCalls.cmake` allows exactly those two names there. (6) `AGENTS.md` gains the rule "never call `clear_sampler_memory` or a `delete_all_*` tool on the owner's sampler unless he says so", the existing `&32` rule being about the probe. (7) Not run on the real sampler: "unless the owner decides" (TASK-MCP-051 text) was read as: the owner decides in TASK-MCP-055.

### TASK-MCP-052: The effects board
- **Tier**: L
- **Status**: Done
- **Description**: The tools to read the card, the channels and the modules, and to set a channel's mute, a module's type and on/off state, and a module parameter, with values checked against the layout the sampler reports and a clear answer when it reports no board (`get_fx_board`, `set_fx_channel_mute`, `set_fx_module`, `get_fx_parameter`, `set_fx_parameter`; the final names and arguments are fixed in the task). Simulator only.
- **Requirement refs**: RQ-MCP-053
- **ADR refs**: ADR-MCP-005 (DEC-MCP-033)
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler with an EB20 layout, *When* a module's type and a parameter are set, *Then* they are read back as set and a value outside the module's range sends nothing; *given* no board, *then* every effects tool says so.
- **Dependencies**: TASK-MCP-044
- **Assignee**: AI
- **Verification**: DONE on 2026-10-08, from the tools' own output, on the simulated sampler only. Red first: `xs56k_mcp_tests "[fx]"`: 15 test cases, 0 passed. Green: "All tests passed (380 assertions in 15 test cases)" (`FxToolsTests.cpp`: annotations; no board, every tool says so and only the card query is sent; the board read with an EB20 and a current multi, and with none selected; a channel muted and unmuted, a channel the board lacks; a module's type set (flange, stereo_delay), bypassed and enabled, type and state in one call; the EB20 rule and the kinds a module takes; a module the board lacks; parameters set by name and index including a negative one; a value outside the range, an unknown parameter, a module of type none; bounds of five ranges; one parameter and a whole module read back; the parameters of the new type after a type change; argument errors) and "All tests passed (299 assertions in 6 test cases)" for `FxCatalogueTests.cpp` (Table 24's 17 kinds, Table 25's parameter counts, lookups, `eb20KindsFor`, range text). Full `ctest` on Linux (GCC 13, `-Wno-dangling-reference`): 1064 of 1064 pass. The scripted conversation gained 7 calls (`EXPECT_LINES` 153 to 160) and the simulated server now has an EB20; expected outputs regenerated and the diff read. 56 tools, 72 with `--allow-disk`. Run on the simulated sampler only, and the README says so.
- **Assumptions**: (1) Five tools: `get_fx_board`, `set_fx_channel_mute`, `set_fx_module` (type and/or enabled), `get_fx_parameter` (one parameter, or all of the module's when `parameter` is left out) and `set_fx_parameter`; the plan left the final names to the task. (2) **The ranges are those of Table 25, written once as data (`FxCatalogue`)**, and a value outside them is refused with nothing sent, although the specification says "the parameter's own range is for the sampler to judge": a sampler may accept more than the table says; the table is the only range source here. Values are the RAW numbers (a rate of 15 is 1.5): no conversion to Hz, dB or milliseconds is offered. (3) The EB20 rule (only modules 2 and 3 of channels 0 and 1 change type; module 2 takes chorus, flange, phase, rotary_speaker, fmod_autopan, pitch_shift or pitch_shift_feedback; module 3 takes mono_delay, mono_left_right, mono_crossover or stereo_delay) is enforced by the tool from p. 35 and Figure 2 of the specification, which is stricter than the sampler's own checks as far as the simulator models them (it accepts any type). `none` is NOT offered as a type to set. (4) A module of a type Table 24 does not name, or of type none, has no known parameter: `get_fx_parameter` and `set_fx_parameter` say so (a code outside the table is read and shown as "type N" by `get_fx_board`). (5) The layout (card, channel count, module counts) is read before each set, so a set costs a few more reads; the card is always asked first and, when the sampler reports none, nothing else is sent. (6) A sampler with no current multi answers ERROR 04 to the mute, type, enabled and parameter items (modelled; the real answer is unobserved); the tools turn it into "select a multi first". (7) The "no board" case reports the card only: a real sampler's answers for the channel and module counts with no board were observed as 0 and ERROR 02 (TASK-AKM-104), which the tools do not need. (8) `FX_NOTICE` and the README state that the tools were run on the simulated sampler only, the owner having no effects board. (9) New units: `FxCatalogue` (public header, data and lookups) and `SamplerGatewayFx.cpp` (no destructive verb, so the source check needed no change).

### TASK-MCP-053: The front-panel keys behind `--allow-front-panel`
- **Tier**: L
- **Status**: Done
- **Description**: The launch option `--allow-front-panel` (usage text, README options table, `--help`), and behind it the tools to press, hold and release a key, turn the data wheel and send an ASCII key, in their own unit of the gateway; a held key is released when the session closes; the source check allows the front-panel primitives only in that unit.
- **Requirement refs**: RQ-MCP-054, RQ-MCP-057
- **ADR refs**: ADR-MCP-005 (DEC-MCP-032)
- **Acceptance Criteria** (Gherkin): *Given* the server without the option, *When* the tools are listed, *Then* none of the key tools is there and a call is refused as unknown; *given* the option, *when* a key is held and the session closes, *then* the simulated sampler received the release.
- **Dependencies**: TASK-MCP-051
- **Assignee**: AI
- **Verification**: DONE on 2026-10-08, from the tools' own output, on the simulated sampler only. Red first: `xs56k_mcp_tests "[frontpanel],[options]"`: 12 test cases, 6 passed (the cases that hold with no tool), 6 failed. Green: "All tests passed (414 assertions in 31 test cases)" (`FrontPanelToolsTests.cpp`: without the option none of the five tools is listed and a call is a JSON-RPC error; with it they are listed, destructive and not idempotent, warning about ENT; a press gives a hold then a release and no key down; a hold and a release; **a key held, then `gateway.close()`: the simulated sampler received the release**; key names in other letters or a digit, an unknown one listing the keys; the wheel forwards/backwards and the clicks 0 and 9 refused; ASCII as a number or a character, beyond 127 and longer texts refused; argument errors; `ServerOptionsTests.cpp`: the option parsed, off by default, refused with a value, in the usage text). Full `ctest` on Linux (GCC 13, `-Wno-dangling-reference`): 1075 of 1075 pass, including `mcp_readme_names_every_tool_and_option` (the README names the option and the five tools) and the new `mcp_source_check_fails_on_a_front_panel_call_in_a_tool_file`. The disk conversation, now launched with `--allow-front-panel` too, gained 7 calls (`EXPECT_LINES` 51 to 58); expected outputs regenerated and the diff read (the 7 answers, the 5 new definitions, the server instructions). 56 tools, 72 with `--allow-disk`, 61 with `--allow-front-panel`, 77 with both. Not run on the real sampler.
- **Assumptions**: (1) Five tools: `press_key`, `hold_key`, `release_key`, `turn_data_wheel` (direction `forwards`/`backwards`, 1 to 8 clicks) and `send_ascii_key` (a number 0 to 127 or one ASCII character); the plan left the names to the task. (2) The 43 keys of Table 31 are named `multi`, `fx`, `edit_sample`, `edit_program`, `record`, `utilities`, `save`, `load`, `f1` to `f16`, `0` to `9`, `minus`, `plus`, `cursor_left`, `cursor_right`, `window`, `mark`, `jump`, `exit`, `ent_play` (the table's "ENT/PLAY" key); keycode `&66`, listed by no row, is not offered (the AKM layer's choice). (3) All five tools declare themselves destructive and not idempotent, although a key is not always a deletion, because no `confirm` guards them; each description and the server's instructions carry the same warning ("ENT answers yes to a delete or save screen"; "the sampler only queues"). The answers repeat that the sampler only queued the command, since DONE does not say it acted (the specification, Table 30 note a). (4) A key held when the session closes is released by the AKM session (`Session::close`), and the gateway closes it when the server's input closes; **a server that is killed does not release it**, which the README does not hide. (5) `press_key` is the AKM `pressKey`: the release is sent even when the hold failed. (6) `--allow-front-panel` is independent of `--allow-disk`; the shipped server, the simulated server, `--help`, the README's option table and its tool table all carry it. (7) The keys unit `SamplerGatewayKeys.cpp` is the only file that may name the front-panel primitives and their types: these names carry no destructive verb, so `CheckNoDestructiveCalls.cmake` gained a second pattern for them, and a fixture directory proves the check fails on a tool file that holds a key. (8) The scripted disk conversation and the README coverage check now launch the simulated server with `--allow-front-panel` as well, so the key tools appear in their listings. (9) A multi-byte UTF-8 character is refused by `send_ascii_key`: only one byte below 128 is. (10) `AGENTS.md` was not changed for the keys: its rule about the owner's sampler (no `clear_sampler_memory`, no `delete_all_*` without his word) is extended in TASK-MCP-054's closure if the owner wants it for the keys.

### TASK-MCP-054: README, scripted conversations, counts and closure
- **Tier**: M
- **Status**: Done
- **Description**: Bring `juce/mcp/README.md` (tool and option tables, what has and has not been tried, the exclusions now reduced to eject, format and the refresh), the scripted conversations, the tool counts in `README.md`, `AGENTS.md` and the mcp README, and the "what stays out" texts into line with what exists; check that the README test passes with all options on.
- **Requirement refs**: RQ-MCP-043, RQ-MCP-055
- **ADR refs**: ADR-MCP-005 (DEC-MCP-028, DEC-MCP-034)
- **Acceptance Criteria** (Gherkin): *Given* the README, *When* its tool tables and the server's list with every option on are compared, *Then* every tool and option is there and the counts in the three documents are the ones the server reports.
- **Dependencies**: TASK-MCP-045 to TASK-MCP-053
- **Assignee**: AI
- **Verification**: DONE on 2026-10-08, from the tools' own output. Red first: `CheckReadmeCoversTools.cmake` extended to run the simulated server with no option, `--allow-disk`, `--allow-front-panel` and both, and to compare the counts with the two READMEs: `mcp_readme_names_every_tool_and_option` failed with "README.md should say: 56 tools, 72 with `--allow-disk`, 61 with `--allow-front-panel`, 77 with both". Green after the README fixes: full `ctest` on Linux (GCC 13, `-Wno-dangling-reference`): 1076 of 1076 pass, including `mcp_readme_names_every_tool_and_option` (every tool and every option of `--help` is in `juce/mcp/README.md`; the counts 56 / 72 / 61 / 77 are those the server lists) and the new `mcp_source_check_fails_on_an_eject_call`. Read and rewritten: the Safety section of `juce/mcp/README.md` (it still said that delete-all, clearing the memory, the keys, song files and the effects board were "never done"), the "what has been tried" row of the new tools (its markup was broken), the introduction and the simulated-server description. The scripted conversations were extended task by task (main session 160 lines, disk and keys session 58 lines) and their expected outputs read each time.
- **Assumptions**: (1) **`AGENTS.md` states no tool count** (it was cut back in the commit of TASK-MCP-045) and none was added: the counts are in `README.md` and `juce/mcp/README.md`, the two documents the test now checks against the server; the task text names three. (2) The four counts (no option, `--allow-disk`, `--allow-front-panel`, both) are written in a fixed sentence in each README so that the test can search for it; changing the wording means changing the test. (3) The ejecting and formatting of a disk stay out as RQ-MCP-055 says; a third negative fixture proves the source check fails on an eject call. (4) The exclusions listed in the README are now: eject and format a disk, moving files between the computer and the sampler, and the refresh of the disk list without its option (DEC-MCP-028); it does NOT claim that every other function of the specification is offered: no item-by-item comparison of the specification with the tools was made. (5) The README's sentence about the keys "released before the server exits" is true for an orderly close only, and says so. (6) `ADR-MCP-002`, `-003` and `-004` and `-005` are still "Proposed" for the owner's review: this task changed no ADR. (7) The closing artifacts of the session (checkpoint note, metrics row) are written after this task, as END SESSION asks.

### TASK-MCP-055: Real-sampler run — the sampler's settings and the MIDI setup
- **Tier**: M
- **Status**: Done
- **Description**: With the owner present and the server launched with no option (see "Real-sampler runs" in the Overview), run `get_sampler_settings`, then `set_sampler_setting` on each of `name`, `clock`, `play_mode` and `front_panel`, each read first and put back; run `set_midi_setting` on each of its five settings and `set_midi_filter` on each of its four events, the owner declaring what the MIDI SETUP and MIDI FILTER pages show before, checking the screen after each call, and each value being put back as declared. Write it up and correct the simulated sampler where it differs.
- **Requirement refs**: RQ-MCP-056, RQ-MCP-046, RQ-MCP-047, RQ-MCP-044
- **ADR refs**: ADR-MCP-005 (DEC-MCP-030, DEC-MCP-033)
- **Acceptance Criteria** (Gherkin): *Given* the owner present and every setting read or declared first, *When* each tool is run alone on each setting, *Then* its answer and what the sampler's screen showed are in the observations file, and every setting is back to its first value.
- **Dependencies**: TASK-MCP-054, TASK-MCP-057
- **Assignee**: Human and AI
- **Verification**: DONE on 2026-10-10, the owner at the sampler (`OBSERVATIONS-RQ-MCP-012-real-sampler.md`, section of TASK-MCP-055), from each answer written to a file: `get_sampler_settings` refused with ERROR 03 on the play mode three times on the UTILITIES MIDI pages, then read from MULTI (`AKAI S5000`, `multi`, `normal`) and later from both MIDI pages; `name` set to 20 characters and read back, put back; `clock` set to 2030-06-15 12:34:56 (Saturday) and read back, put back (offset to the PC 52.6 s before, 52.8 s after); `play_mode` program, sample, muted, multi each read back; `front_panel` locked read back, normal put back; the 5 `set_midi_setting` values and the 4 `set_midi_filter` ignores on 1A seen on the pages by the owner, the declared values sent back and seen. The change made for the refusal, test first: `xs56k_mcp_tests "[settings]"` 2 of 12 failed before the code (11 assertions), then "All tests passed (288 assertions in 12 test cases)". Full `ctest` on Windows (MSVC, Debug, `/W4 /WX`, build dir `juce/build-run`): 1079 of 1079 pass.
- **Assumptions**: (1) The clock keeps running during the run: it is put back to the time read first plus the time elapsed, or set by the owner. (2) The front-panel lock is set only if the owner agrees, the tool's answer saying how to unlock; it is left as it was found. (3) Whether the sampler keeps a name of 20 characters is observed here (TASK-MCP-045, assumptions). (4) **The play-mode refusal (owner's choice B, 2026-10-10)**: `get_sampler_settings` gives the other settings and "not reported" with the sampler's reason when the Get of the play mode is answered by an ERROR, with what was seen for error 3; a silent sampler still fails the tool; the simulated sampler was not changed (its `SamplerBehaviour::itemErrors` already refuses an item on demand) and models no page; the cause of the refusal stays unknown. (5) **Two tests fixed for MSVC**, both hidden on GCC: `MidiSetupToolsTests.cpp` (a list of `int` read as `uint8_t`, C4244 under `/WX`) and `FxToolsTests.cpp` (a reference to a member of the temporary `fxState()`, read after it was destroyed: a crash on MSVC); the CI's Windows workflows would have stopped on the first. (6) The server was run from a second build directory, `juce/build-run`, because the one registered in `.mcp.json` was locked by the running MCP client. (7) Reworked on 2026-10-09 (TASK-MCP-057): it was the single real run of every new tool, with exceptions; the test data it listed moved to "Real-sampler runs" in the Overview.

### TASK-MCP-056: `CHANGELOG.md` says what the server offers today
- **Tier**: M
- **Status**: Done
- **Description**: Rewrite the "MCP server" entry of `[Unreleased]` in `CHANGELOG.md`, left untouched by TASK-MCP-045 to TASK-MCP-054: the counts 56 / 72 / 61 / 77, the new tools, the confirmation by count, the keys behind `--allow-front-panel`, eject and format still out, and which tools have run on the real sampler.
- **Requirement refs**: RQ-MCP-043, RQ-MCP-055
- **ADR refs**: ADR-MCP-005 (DEC-MCP-028, DEC-MCP-029)
- **Acceptance Criteria** (Gherkin): *Given* the updated `CHANGELOG.md`, *When* its MCP entry is compared with the counts `ctest` checks and with the Safety section of `juce/mcp/README.md`, *Then* the counts are 56, 72, 61 and 77, no sentence says the server never deletes everything or never clears the memory, and ejecting and formatting a disk stay out.
- **Dependencies**: TASK-MCP-054
- **Assignee**: AI
- **Verification**: DONE on 2026-10-09, from the file re-read and searched: the entry has "**56 tools", "**16 more" (`--allow-disk`), "**5 more" (`--allow-front-panel`), "61 tools with this option, 77 with both"; 0 match for "never deletes everything", "never clears", "32 tools"; "It never formats or ejects a disk" agrees with the README's "does not ... format a disk, eject a disk"; the 24 tools added to the 32 (2 settings, 2 MIDI, 8 list, 3 delete, 3 delete-all, 1 clear, 5 effects) are each named in `juce/mcp/src/Tools.cpp`.
- **Assumptions**: (1) The AKM entries are not touched; the §02 system setup primitives have no entry and none is added (out of scope). (2) Tier M by the tier table (more than 5 lines), but no code changes: no unit test, verification by re-reading and searching the file. (3) The session-state change of this session (`platform: windows`) goes into this task's commit.

### TASK-MCP-057: Plan the real-sampler run of every tool of this plan
- **Tier**: M
- **Status**: Done
- **Description**: From the owner's decision of 2026-10-09 ("every tool implemented in PLAN-MCP-005 is tested on the real sampler"), rework TASK-MCP-055, add TASK-MCP-058 to TASK-MCP-064, and amend RQ-MCP-056 and DEC-MCP-033, which excluded the effects board, `clear_sampler_memory` and most keys.
- **Requirement refs**: RQ-MCP-056
- **ADR refs**: ADR-MCP-005 (DEC-MCP-033)
- **Acceptance Criteria** (Gherkin): *Given* the tools of TASK-MCP-045 to TASK-MCP-053 (24 with no option, 3 disk tools given new kinds, 5 keys), *When* the tasks of the real run are read, *Then* each tool is named in one of them, and the process index lists them with no duplicate ID.
- **Dependencies**: TASK-MCP-056
- **Assignee**: AI
- **Verification**: DONE on 2026-10-09, from the files re-read and searched: the 32 tool names (24 with no option, `save_memory_item`, `save_all_memory_items`, `load_file`, the 5 keys) are each found in TASK-MCP-055 or TASK-MCP-058 to TASK-MCP-063 (32 of 32); the `agnos-index` skill: 464 entries, no duplicate, TASK-MCP-055 and TASK-MCP-057 to TASK-MCP-064 listed; RQ-MCP-056 and DEC-MCP-033 carry an "Amended 2026-10-09" note.
- **Assumptions**: (1) "Every tool" includes the effects tools, but the owner's S5000 has no EB20 (DEC-MCP-033): with no board they can only give their "no board" answer, so their Sets stay unverified on a real board and the README keeps saying so. (2) "Tested on the real sampler" includes `clear_sampler_memory`, the `delete_all_*` tools and every key tool; each such call still waits for the owner's word at the moment it is sent (`AGENTS.md`). (3) The run is split by risk into seven tasks, in the order of DEC-MCP-034, and closed by TASK-MCP-064; TASK-MCP-055 keeps its ID and becomes the first run. (4) The test data list of the former TASK-MCP-055 moved, unchanged, to "Real-sampler runs" in the Overview, with one addition: the owner saves what he keeps before TASK-MCP-061 and TASK-MCP-062.

### TASK-MCP-058: Real-sampler run — song files, set lists and scenelists in memory
- **Tier**: M
- **Status**: Not Started
- **Description**: With the server launched with no option, on the owner's test data and on copies made for the run, run `list_song_files`, `select_song_file`, `rename_song_file`, `list_set_lists`, `rename_set_list`, `list_scenelists`, `select_scenelist`, `rename_scenelist`, then `delete_song_file`, `delete_set_list` and `delete_scenelist` on copies only. Observe what TASK-MCP-047 and TASK-MCP-048 left unknown: a rename to a name already taken, a select by an unknown name or position, the answer when none is current, the selection after a deletion, a name of 20 characters. Write it up and correct the simulated sampler.
- **Requirement refs**: RQ-MCP-056, RQ-MCP-048, RQ-MCP-049, RQ-MCP-042
- **ADR refs**: ADR-MCP-005 (DEC-MCP-031, DEC-MCP-033); ADR-MCP-004 (DEC-MCP-023)
- **Acceptance Criteria** (Gherkin): *Given* the owner's test data in memory, *When* each of the 11 tools is run alone, *Then* its answer and what the sampler did are in the observations file, every name changed is put back, and only copies made for the run have been deleted.
- **Dependencies**: TASK-MCP-055 and the owner's test data
- **Assignee**: Human and AI
- **Verification**: Not started (needs the owner).
- **Assumptions**: (1) If the sampler accepts a duplicate name, the server's refusal (TASK-MCP-047, assumption 1) is reviewed with the owner, not changed by this task. (2) A copy is made by saving and loading a test item, or by the owner at the panel; the observations say which.

### TASK-MCP-059: Real-sampler run — saving and loading song files, set lists and scenelists through the disk
- **Tier**: M
- **Status**: Not Started
- **Description**: With `--allow-disk` (never `--allow-disk-refresh`), in a folder made for the run with `create_folder`, run `save_memory_item` and `save_all_memory_items` with the kinds `song_file`, `set_list` and `scenelist`, then `load_file` on each saved file. Observe the file extensions and sizes (placeholders `.MID`, `.SET`, `.SCN` and 512 bytes in the simulated sampler), a second save without `overwrite`, and loading a file whose item is already in memory. Replace the placeholders in the simulated sampler; delete the folder and the copies afterwards.
- **Requirement refs**: RQ-MCP-056, RQ-MCP-050, RQ-MCP-025, RQ-MCP-026, RQ-MCP-027
- **ADR refs**: ADR-MCP-005 (DEC-MCP-031, DEC-MCP-033); ADR-MCP-003 (DEC-MCP-018)
- **Acceptance Criteria** (Gherkin): *Given* a writable disk and a folder made for the run, *When* each kind is saved alone, saved in bulk and loaded back, *Then* the answers, the listing and the memory are in the observations file, the simulated sampler writes the observed extensions and sizes, and the folder is removed.
- **Dependencies**: TASK-MCP-058
- **Assignee**: Human and AI
- **Verification**: Not started (needs the owner).
- **Assumptions**: (1) The bulk save of a kind is run only when what it saves is test data or the owner agrees to save all of it into the test folder. (2) If loading an item already in memory replaces it instead of adding one, the simulated sampler is corrected and the tool's answer reviewed.

### TASK-MCP-060: Real-sampler run — the effects board
- **Tier**: M
- **Status**: Not Started
- **Description**: With the server launched with no option, on a test multi made for the run, run `get_fx_board`, `set_fx_channel_mute`, `set_fx_module`, `get_fx_parameter` and `set_fx_parameter`. With no board (the owner's S5000 today), each must say there is none and send nothing after the card query; with a board installed, each Set is read back and put back. Write it up.
- **Requirement refs**: RQ-MCP-056, RQ-MCP-053
- **ADR refs**: ADR-MCP-005 (DEC-MCP-033)
- **Acceptance Criteria** (Gherkin): *Given* the sampler with no board, *When* each of the 5 tools is run alone, *Then* each answer says there is no board and the observations file records it; *given* a board, *Then* each Set is read back as set and put back.
- **Dependencies**: TASK-MCP-059
- **Assignee**: Human and AI
- **Verification**: Not started (needs the owner).
- **Assumptions**: (1) The owner has no EB20 (DEC-MCP-033): the round trip on a real board stays undone, and the README keeps saying so, until one is installed. (2) The AKM suite already observed the card query with no board (TASK-AKM-104); this run checks the MCP tools' answers.

### TASK-MCP-061: Real-sampler run — deleting all programs, all samples, all multis
- **Tier**: M
- **Status**: Not Started
- **Description**: On a memory that holds only objects made for the run (the owner having saved what he keeps), run `delete_all_programs`, `delete_all_samples` and `delete_all_multis`, each first with a wrong count (nothing must be sent), then with the right one after the owner's word. Observe what happens to the parts of a multi when all programs go and to the zones of a program when all samples go (TASK-MCP-050, assumption 7). Write it up and correct the simulated sampler.
- **Requirement refs**: RQ-MCP-056, RQ-MCP-051, RQ-MCP-042
- **ADR refs**: ADR-MCP-005 (DEC-MCP-029, DEC-MCP-033)
- **Acceptance Criteria** (Gherkin): *Given* a memory of test objects only and the owner's word for each call, *When* each tool is run with a wrong count and then with the right one, *Then* the first sends nothing, the second leaves none of its kind, and the answers and what the sampler did are in the observations file.
- **Dependencies**: TASK-MCP-060
- **Assignee**: Human and AI
- **Verification**: Not started (needs the owner).
- **Assumptions**: (1) Test objects of every kind are made again between the three calls, so that each observation is of one command on a known memory.

### TASK-MCP-062: Real-sampler run — clearing the sampler's memory
- **Tier**: M
- **Status**: Not Started
- **Description**: On a memory that holds only objects made for the run, test copies of song files, set lists and scenelists included, run `clear_sampler_memory` with a wrong count (nothing must be sent), then with the right one after the owner's word. Observe whether the song files, set lists and scenelists are cleared too (TASK-MCP-051, assumption 1) and how long the sampler takes to answer. Write it up and correct the simulated sampler; if the three lists are cleared, the count of DEC-MCP-029 is reviewed with the owner.
- **Requirement refs**: RQ-MCP-056, RQ-MCP-052, RQ-MCP-042
- **ADR refs**: ADR-MCP-005 (DEC-MCP-029, DEC-MCP-033)
- **Acceptance Criteria** (Gherkin): *Given* a memory of test objects only and the owner's word, *When* `clear_sampler_memory` is run with a wrong count and then with the right one, *Then* the first sends nothing, the second leaves no program, sample or multi, and what happened to the song files, set lists and scenelists and the time taken are in the observations file.
- **Dependencies**: TASK-MCP-061
- **Assignee**: Human and AI
- **Verification**: Not started (needs the owner).
- **Assumptions**: (1) The command has no timeout of its own (TASK-MCP-051, assumption 2): if the sampler takes longer than the session's command timeout, the tool reports it as not answering and the owner may have to switch the sampler off and on; that risk is said to the owner before the call.

### TASK-MCP-063: Real-sampler run — the front-panel keys
- **Tier**: M
- **Status**: Not Started
- **Description**: With `--allow-front-panel` (and no other option), the owner watching the screen, run `press_key`, `hold_key`, `release_key`, `turn_data_wheel` and `send_ascii_key` at least once each, from screens and on keys the owner chooses as harmless (never ENT/PLAY on a delete, save or clear screen); hold a key and close the session in order to see the release; turn the data wheel on a value the owner watches and puts back; send ASCII keys into the name of a test item. Write it up.
- **Requirement refs**: RQ-MCP-056, RQ-MCP-054
- **ADR refs**: ADR-MCP-005 (DEC-MCP-032, DEC-MCP-033)
- **Acceptance Criteria** (Gherkin): *Given* the owner at the sampler and a screen he chose, *When* each of the 5 tools is run alone, *Then* what the screen did is in the observations file; *when* a key is held and the session closes in order, *then* the sampler shows the key released.
- **Dependencies**: TASK-MCP-062
- **Assignee**: Human and AI
- **Verification**: Not started (needs the owner).
- **Assumptions**: (1) "Every tool" is read as every key tool, not every one of the 43 keys: the keys pressed are those the owner names. (2) A server killed with a key held does not release it (TASK-MCP-053, assumption 4); this is not tried.

### TASK-MCP-064: Closure of the real runs — README, observations and simulated sampler
- **Tier**: M
- **Status**: Not Started
- **Description**: After TASK-MCP-055 and TASK-MCP-058 to TASK-MCP-063, update the table "what has been tried" and the Safety section of `juce/mcp/README.md`, the sentences of `README.md` and `CHANGELOG.md` saying which tools ran on the simulated sampler only, regenerate the scripted conversations if the simulated sampler changed (their differences read line by line), and run the full `ctest`.
- **Requirement refs**: RQ-MCP-056, RQ-MCP-043
- **ADR refs**: ADR-MCP-005 (DEC-MCP-033)
- **Acceptance Criteria** (Gherkin): *Given* the observations file after the runs, *When* the READMEs and `CHANGELOG.md` are read, *Then* each tool of this plan is said to have run on the real S5000, the effects tools with the limit of the missing board, and `ctest` passes.
- **Dependencies**: TASK-MCP-055, TASK-MCP-058 to TASK-MCP-063
- **Assignee**: AI
- **Verification**: Not started.
- **Assumptions**: None
