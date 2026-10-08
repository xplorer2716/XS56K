# FTR-MCP-005: MCP Server — The Remaining Sampler Functions (Settings, MIDI Setup, Song Files, Set Lists, Scenelists, Delete All, Effects Board, Front Panel)

## Overview

After FTR-MCP-001 to FTR-MCP-004 the server edits programs, zones, samples and multis and browses, loads and saves through the sampler's
disks (32 tools, 48 with `--allow-disk`). `ADR-MCP-004` (DEC-MCP-025) listed what it still did not offer and left each item to the
owner. On 2026-10-07 the owner decided, item by item (recorded in `ADR-MCP-005`, DEC-MCP-028):

| Item | Owner's decision |
|---|---|
| song files, set lists, scenelists | implement; test data to be made, or put on the sampler by the owner |
| the sampler's settings (name, clock, play mode, front-panel lock) and its MIDI setup | implement |
| the effects board | implement; the owner has no EB20 board, so it can only be verified on the simulated sampler |
| ejecting a disk | **do not implement** |
| delete all programs, samples, multis; clear the sampler's memory | implement, with the standard confirmation mechanism |
| the front-panel keys | implement, possibly behind a launch option of their own |
| the refresh of the disk list | leave as it is (behind `--allow-disk-refresh`, to stay off) |
| formatting a disk | out of scope: it is not in the SysEx specification |

The goal stays the one stated in the root `README.md`: the MCP server covers every function of the samplers that the SysEx
specification offers, except what the owner has excluded. This feature is the last step towards it.

Facts the requirements rest on (read in `juce/akm/include/akm` on 2026-10-07): the AKM layer has `SystemSetup` (name, clock date, play
mode, front-panel lock: a Set and a Get each; `clearSamplerMemory` with a typed confirmation), `MidiConfig` (§04: Set items only, no
Get), `SongPrimitives` (select, rename and delete the current song; count, index and name; set lists: count, name, rename, delete, no
select), `SceneListPrimitives` (select, rename and delete the current scenelist; count, index and name), `MultiFxPrimitives` (card,
channel mute, module type, module enabled, parameter: a Set and a Get each), `FrontPanel` (hold, release and press a key, the data
wheel, an ASCII key), the Delete ALL primitives with typed confirmations (`deleteAllPrograms`, `deleteAllSamples`,
`deleteAllMultis`) and `saveMemoryItem` with the types song file, set list and scenelist.

## Stakeholders
- **Owner**: the repository owner (the owner of the S5000 and of its SCSI2SD disk)
- **Consumers**: an MCP client and the assistant or person using it; the tests; `juce/mcp/README.md`

---

## Functional Requirements

### RQ-MCP-046: Reading and setting the sampler's own settings
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `get_sampler_settings`, the server SHALL answer the sampler's name, its clock date and time, its play mode and its front-panel lock, read from the sampler; WHEN a client calls `set_sampler_setting` with one of those settings and a value, the server SHALL check the value, send it, read it back and answer the value read; IF the value is not valid for the setting (a name longer than the sampler takes, a date that does not exist, a play mode or a lock the sampler does not have), THEN it SHALL send nothing and say what is accepted.
- **Rationale**: the owner asked for the settings of the sampler to be implemented; the AKM layer has a Set and a Get for each.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler, *When* the name is set to "TESTSAMP", *Then* `get_sampler_settings` answers it. *Given* a date of 31 February, *Then* nothing is sent and `isError` is true. *Given* a front-panel lock, *Then* the answer says how to remove it.
- **Dependencies**: FTR-AKM-006 (the system setup primitives); ADR-MCP-005 (DEC-MCP-028, DEC-MCP-030)

### RQ-MCP-047: Setting the sampler's MIDI setup
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `set_midi_setting` with a setting (program change enabled, multi select mode, multi select channel, external APM controller, aftertouch type) and a value, or `set_midi_filter` with an event, a channel and allow or ignore, the server SHALL check the value, send it and answer what it sent; because §04 has no Get, the answer SHALL say that the previous value could not be read and cannot be put back by the server; IF the value is not valid, THEN it SHALL send nothing and say what is accepted.
- **Rationale**: the owner asked for the MIDI setup to be implemented; the section has Set items only, so what the server cannot do is said, not hidden.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler, *When* the multi select channel is set to 3, *Then* the simulated sampler holds 3 and the answer says the previous value is unknown. *Given* a channel of 32, *Then* nothing is sent.
- **Amended 2026-10-08**: the owner chose the sampler's own channel numbers, 0 to 31 (1A = 0 ... 16B = 31, FTR-AKM-009), over the front-panel names 1A to 16B. Channel 3 is therefore held as 3 (the front-panel channel 4A, which the answer also says), and 17 is a valid channel; the first value refused is 32. TASK-MCP-046.
- **Dependencies**: FTR-AKM-009 (the MIDI configuration primitives); ADR-MCP-005 (DEC-MCP-030)

### RQ-MCP-048: Listing, selecting and renaming song files, set lists and scenelists
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client lists song files, set lists or scenelists, the server SHALL answer the names held by the sampler, in the sampler's order, and mark the current song file or scenelist; WHEN it selects a song file or a scenelist by name, or renames the current one (a set list is renamed by name, the sampler having no current set list), the server SHALL send the command, read the state back and answer it; IF the name is unknown or the new name is not valid, THEN it SHALL send nothing and say so.
- **Rationale**: the owner asked for them; they are the three lists of the sampler that no tool reads today.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding two song files, a set list and two scenelists, *When* each list tool is called, *Then* it answers the names in order with the current one marked. *Given* a rename to a name already used, *Then* the answer is what the sampler reported, with nothing changed.
- **Dependencies**: FTR-AKM-010, FTR-AKM-012; ADR-MCP-005 (DEC-MCP-031)

### RQ-MCP-049: Deleting a song file, a set list or a scenelist
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a client calls `delete_song_file`, `delete_set_list` or `delete_scenelist` and its `confirm` argument is missing or is not the exact name of what is deleted, THEN the server SHALL send nothing and SHALL answer what to give as `confirm`; otherwise it SHALL delete it and answer the names that remain; the three tools SHALL declare themselves destructive in their MCP annotations.
- **Rationale**: the rule of RQ-MCP-042 applies to every deletion.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* each of the three tools, *When* it is called with no `confirm`, a wrong one and the right one, *Then* the first two send nothing and name the `confirm` expected, and the third deletes it.
- **Dependencies**: RQ-MCP-042, RQ-MCP-048; ADR-MCP-004 (DEC-MCP-023); ADR-MCP-005 (DEC-MCP-031)

### RQ-MCP-050: Saving and loading song files, set lists and scenelists through the disk
- **Category**: Functional
- **EARS Type**: State-driven
- **Statement**: WHILE the server runs with `--allow-disk`, `save_memory_item` and `save_all_memory_items` SHALL accept the kinds `song_file`, `set_list` and `scenelist`, and `load_file` SHALL load the corresponding files of the current folder; the rules of RQ-MCP-026 and RQ-MCP-027 (no overwrite unless asked, the count as `confirm` for a bulk save, a listing checked after the save) SHALL apply to them unchanged.
- **Rationale**: the owner asked for them to be implemented, and the AKM layer already saves them (`SaveableMemoryType` has the three).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler with a disk, *When* a song file is saved, *Then* a file of that name is in the folder and a second save is refused without `overwrite`. *Given* that file, *When* it is loaded, *Then* the sampler's song files include it.
- **Dependencies**: RQ-MCP-025, RQ-MCP-026, RQ-MCP-027, RQ-MCP-048; ADR-MCP-005 (DEC-MCP-031)

### RQ-MCP-051: Deleting all programs, all samples or all multis
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a client calls `delete_all_programs`, `delete_all_samples` or `delete_all_multis` and its `confirm` argument is missing or is not the number of items of that kind the sampler holds now, THEN the server SHALL send nothing and SHALL answer the number to give; otherwise it SHALL delete them all and answer that the sampler now holds none; the three tools SHALL declare themselves destructive.
- **Rationale**: the owner asked for them with "the standard confirmation mechanism"; there is no single name to give, and the existing bulk tool `save_all_memory_items` already takes the number of items as `confirm`.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler with 3 programs, *When* `delete_all_programs` is called with `confirm` "2", *Then* nothing is sent and the answer says 3. *When* it is called with "3", *Then* `list_programs` says there is none.
- **Dependencies**: RQ-MCP-042; ADR-MCP-005 (DEC-MCP-029)

### RQ-MCP-052: Clearing the sampler's memory
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a client calls `clear_sampler_memory` and its `confirm` argument is missing or is not the total number of programs, samples and multis the sampler holds now, THEN the server SHALL send nothing and SHALL answer the number to give; IF the sampler holds none of the three, THEN it SHALL send nothing and say so; otherwise it SHALL send Clear Sampler Memory and answer what the sampler holds afterwards; the tool SHALL declare itself destructive.
- **Rationale**: the owner asked for it with the standard confirmation mechanism; it is the largest loss the server can cause, so the figure to give is the one a person can check with `list_programs`, `list_samples` and `list_multis`.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler with 2 programs, 1 sample and 1 multi, *When* `clear_sampler_memory` is called with "3", *Then* nothing is sent and the answer says 4. *When* it is called with "4", *Then* the three lists are empty.
- **Dependencies**: RQ-MCP-042, RQ-MCP-051; ADR-MCP-005 (DEC-MCP-029)

### RQ-MCP-053: The effects board
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client asks for the effects board, the server SHALL answer the card the sampler reports, its channels and their modules; WHEN it sets a channel's mute, a module's type or on/off state, or a module parameter, the server SHALL check the value against the board's layout, send it, read it back and answer the value read; IF the sampler reports no effects board, THEN every effects tool SHALL say so and send nothing more; the README SHALL say that these tools have been run on the simulated sampler only.
- **Rationale**: the owner asked for the effects board to be implemented; he has no EB20 board, so the tools cannot be verified on a real board.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler with an EB20 layout, *When* a module's type and a parameter are set, *Then* they are read back as set. *Given* no board, *Then* the answer says there is none.
- **Dependencies**: FTR-AKM-013; ADR-MCP-005 (DEC-MCP-033)

### RQ-MCP-054: The front-panel keys, behind a launch option of their own
- **Category**: Functional
- **EARS Type**: State-driven
- **Statement**: WHILE the server runs with `--allow-front-panel`, it SHALL offer tools to press, hold and release a front-panel key, to turn the data wheel and to send an ASCII key; WHILE it runs without that option, none of these tools SHALL be listed and a call to one SHALL be refused as an unknown tool; WHEN the session closes with a key held, the server SHALL release it.
- **Rationale**: the owner asked for them, possibly behind an option of their own; a key can answer "ENT" to any delete or save screen of the sampler, which no `confirm` of this server guards.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the server without the option, *When* the tools are listed, *Then* none of the key tools is there. *Given* the option, *When* a key is held and the session closes, *Then* the simulated sampler received the release.
- **Dependencies**: FTR-AKM-008; ADR-MCP-005 (DEC-MCP-032)

### RQ-MCP-055: What stays not offered
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: The server SHALL expose no tool that ejects a disk (the owner's decision of 2026-10-07), none that formats a disk or moves files between the computer and the sampler (they are not in the SysEx specification), and SHALL keep the refresh of the disk list behind `--allow-disk-refresh` as it is.
- **Rationale**: the owner decided each of the three; the source check keeps them true.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the sources of `juce/mcp`, *When* searched for the eject and format primitives, *Then* there is no call. *Given* the server without `--allow-disk-refresh`, *Then* `list_disks` has no `refresh` argument.
- **Dependencies**: RQ-MCP-028, RQ-MCP-031; ADR-MCP-005 (DEC-MCP-028)

---

## Non-Functional Requirements

### RQ-MCP-056: Verified on the simulator, then on the real sampler with test data
- **Category**: Non-Functional
- **NFR Type**: Reliability
- **EARS Type**: Ubiquitous
- **Statement**: Every requirement of this feature SHALL be verified in `ctest` against the simulated sampler; the tools SHALL then be run on the real S5000 with the owner present, on test data that is made for the run or that the owner puts on the sampler and lists beforehand (song files, set lists and scenelists in memory and on the disk, programs, samples and multis made for the run), with these exceptions, which the README SHALL list: the effects board (no board), `clear_sampler_memory` (not run on the real sampler unless the owner decides it), and the front-panel keys other than ones the owner watches doing nothing harmful.
- **Metric**: each new tool run on the real sampler, or each exception listed.
- **Measurement Method**: `ctest`; scripted runs on the real sampler, written up in `OBSERVATIONS-RQ-MCP-012-real-sampler.md`.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the observations file, *When* it is read after the real run, *Then* each new tool has its answer and what the sampler did, or is in the list of exceptions with the reason.
- **Dependencies**: RQ-MCP-044; ADR-MCP-005 (DEC-MCP-033)

### RQ-MCP-057: Each new destructive primitive is called from one place only
- **Category**: Non-Functional
- **NFR Type**: Safety
- **EARS Type**: Ubiquitous
- **Statement**: The source check (`CheckNoDestructiveCalls.cmake`) SHALL allow each new primitive that deletes or clears something (the Delete ALL primitives, Clear Sampler Memory, the deletion of a song file, a set list or a scenelist) only in the gateway unit that owns it, SHALL allow the front-panel primitives only in a unit that is compiled into the tool list only when `--allow-front-panel` is given, and SHALL keep the eject and format primitives forbidden everywhere.
- **Metric**: number of calls to a destructive primitive outside its allowed unit: 0.
- **Measurement Method**: the `ctest` entry `mcp_sources_call_no_destructive_primitive`, with a case that plants a call in another file and expects the check to fail.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the sources, *When* the check runs, *Then* it passes; *Given* a call to `deleteAllPrograms` in a tool file, *Then* it fails.
- **Dependencies**: RQ-MCP-008, RQ-MCP-014; ADR-MCP-005 (DEC-MCP-029, DEC-MCP-032)
