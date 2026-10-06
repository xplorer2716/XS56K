# FTR-MCP-004: MCP Server — Completing the Tools (Zone Samples, Keygroups, Samples, Multis, Files, Information, Audition) and a Reference README

## Overview

After FTR-MCP-001 to FTR-MCP-003 the server can edit programs, zones, samples and multis, create, rename and delete programs, and
browse, load and save through the sampler's disks. On 2026-10-06 the owner asked for an independent review of the decisions of
ADR-MCP-001 to ADR-MCP-003 to find what the assistant had excluded on its own initiative and the owner had never asked to exclude
(the trigger: create-folder, "never offered" in ADR-MCP-003 although the AKM layer had it and the owner never said no), then for the
missing tools to be implemented, "with a confirmation for what is destructive", and for the README of the server to become a clear
reference for a human.

The review (summarised in `PLAN-MCP-004`) found, among the exclusions that limit a person most:

- a program cannot be made to play a sample: **no tool assigns a sample to a zone** (AKM `setZoneSample` and `getZoneSample`);
- a **multi** cannot be built: no create, rename or delete of a multi, and no assignment of a program to a part;
- the **keygroup count is fixed** at `create_program` (AKM `addKeygroupsToProgram`, `deleteKeygroupFromProgram`);
- **files and folders** cannot be renamed or deleted (create-folder was added by FTR-MCP-003 RQ-MCP-032);
- the **current sample** cannot be renamed or deleted, nor the current multi deleted;
- nothing says how much **memory** or **disk space** is free, which matters before loading a large sample;
- a sample or a file on disk cannot be **auditioned**.

**Guards.** Every tool that deletes something takes a `confirm` argument that must be the exact name of what is deleted (the pattern
of `delete_program`, ADR-MCP-002 DEC-MCP-011), and nothing is sent otherwise. No new launch option is added: the disk tools stay
behind `--allow-disk`, the refresh behind `--allow-disk-refresh`; fewer options is simpler for the person who has to configure the
server. What stays not offered, and what is left to the owner to decide, is listed in ADR-MCP-004 (DEC-MCP-025).

**Vocabulary** is unchanged: names as the sampler's screen shows them, zones 1 to 4, parts from 1, sizes in bytes.

**Out of scope here (owner to decide, ADR-MCP-004 DEC-MCP-025).** Song files, set lists, scenelists, MIDI setup, the sampler's name,
clock, play mode and front-panel lock, the effects board, ejecting a disk, deleting all programs, samples or multis, clearing the
sampler's memory, the front-panel keys, saving or loading song files, scenelists and MIDI files.

**Depends on** FTR-MCP-001 to FTR-MCP-003, FTR-AKM-001 and the AKM primitives named in each requirement, ADR-MCP-001 to ADR-MCP-003.

**Sources.** `process/2.architecture/ADR-MCP-001` to `ADR-MCP-003`, `juce/akm/include/akm/*.hpp`, `documents/_index/sysex_spec.items.tsv`,
`process/2.architecture/OBSERVATIONS-RQ-MCP-012-real-sampler.md`.

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: an MCP client (Claude Code or another) and the person using it; CI (simulated sampler only).

---

## Functional Requirements

### RQ-MCP-034: Assigning a sample to a zone
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `set_zone_sample` with a sample name, a zone (1 to 4) and optionally a keygroup, the server SHALL assign that sample of the sampler's memory to that zone of that keygroup of the current program and read the assignment back; WHEN it calls `get_zone_samples` it SHALL answer the sample assigned to each zone of a keygroup, or "no sample". A sample that is not in memory, a zone outside 1 to 4 and a keygroup that does not exist SHALL be refused with nothing sent, and the answer SHALL list what exists.
- **Rationale**: without it a program cannot play a loaded sample.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a program with two keygroups and a sample KICK in memory, *When* `set_zone_sample` is called with KICK, zone 1 and keygroup 2, *Then* the sampler is sent the assignment for keygroup 2 and the answer says zone 1 of keygroup 2 plays KICK as read back; *When* `get_zone_samples` is called for keygroup 2, *Then* zone 1 is KICK and the others have no sample.
- **Dependencies**: RQ-MCP-005, RQ-MCP-015; ADR-MCP-004 (DEC-MCP-023)

### RQ-MCP-035: Adding and deleting keygroups
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `add_keygroups` with a count (1 to the keygroups the program can still take), the server SHALL add that many keygroups to the current program and answer the new count; WHEN it calls `delete_keygroup` with a keygroup number and `confirm` equal to the current program's exact name, the server SHALL delete that keygroup and answer the new count; any other `confirm`, a keygroup that does not exist, or the last keygroup of the program SHALL be refused with nothing sent.
- **Rationale**: the count of keygroups is otherwise fixed when the program is created; a deleted keygroup loses its settings and zones.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a program PAD with 2 keygroups, *When* `add_keygroups` is called with 2, *Then* the program has 4. *When* `delete_keygroup` is called for keygroup 3 with `confirm` "PAD", *Then* the program has 3; with `confirm` "pad2", *Then* nothing is sent.
- **Dependencies**: RQ-MCP-013, RQ-MCP-015; ADR-MCP-004 (DEC-MCP-023, DEC-MCP-024)

### RQ-MCP-036: Renaming and deleting the current sample
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `rename_sample` with a name, the server SHALL rename the current sample and read the list back; WHEN it calls `delete_sample` with `confirm` equal to the current sample's exact name, the server SHALL delete the current sample from the sampler's memory; any other `confirm`, no current sample, or a new name that another sample already bears SHALL be refused with nothing sent.
- **Rationale**: a sample can only come from the disk or be recorded; without this a person cannot free memory or fix a name through the server.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* samples KICK and SNARE with KICK current, *When* `rename_sample` is called with "KICK2", *Then* the list has KICK2 and SNARE. *When* `delete_sample` is called with `confirm` "KICK2", *Then* only SNARE remains; with another `confirm`, nothing is sent.
- **Dependencies**: RQ-MCP-020; ADR-MCP-004 (DEC-MCP-023)

### RQ-MCP-037: Creating, renaming and deleting a multi
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `create_multi` with a name, `rename_multi` with a name, or `delete_multi` with `confirm` equal to the current multi's exact name, the server SHALL create a multi and make it current, rename the current multi, or delete the current multi, and read the list of multis back; a name that another multi bears, a missing current multi and a wrong `confirm` SHALL be refused with nothing sent.
- **Rationale**: a multi cannot be built through the server without them.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* two multis, *When* `create_multi` is called with "STAGE", *Then* STAGE is listed and current; *When* `delete_multi` is called with `confirm` "STAGE", *Then* it is gone; with another `confirm`, nothing is sent.
- **Dependencies**: RQ-MCP-021; ADR-MCP-004 (DEC-MCP-023)

### RQ-MCP-038: What a multi's parts play
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `set_part_program` with a part (numbered from 1) and a program (by name or by position), the server SHALL assign that program to that part of the current multi; WHEN it calls `clear_part` with a part and `confirm` equal to the current multi's exact name it SHALL remove the program of that part; WHEN it calls `set_multi_program_number` with a number from 1 to 128, or none, it SHALL set or clear the current multi's program number; a program or a part that does not exist, no current multi and a wrong `confirm` SHALL be refused with nothing sent.
- **Rationale**: the existing multi tools edit the parameters of a part but cannot say what the part plays; `get_part_programs` (added with TASK-MCP-032) lists the parts that play a program, numbered from 1, and answers that none does when none does.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a current multi LIVE and programs BASS and LEAD, *When* `set_part_program` is called with part 2 and "LEAD", *Then* the sampler is sent the assignment for the part index 1 and the answer says part 2 plays LEAD; *When* `clear_part` is called for part 2 with `confirm` "LIVE", *Then* the part has no program.
- **Dependencies**: RQ-MCP-021, RQ-MCP-037; ADR-MCP-004 (DEC-MCP-023)

### RQ-MCP-039: Renaming and deleting files and folders
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `rename_file` or `rename_folder` with a name and a new name, the server SHALL rename it in the current folder of the current writable disk, read the folder back and answer the name the sampler gave (the sampler appends the file's extension itself); WHEN it calls `delete_file` with a name and `confirm` equal to that exact name, the server SHALL delete the file; WHEN it calls `delete_folder` with a name and `confirm` equal to that exact name it SHALL delete the folder, but SHALL refuse a folder that holds files or folders unless `delete_contents` is true, in which case the answer says how many items go with it. A disk that is not selected or not writable, a name that is not in the folder, a new name that already exists and a wrong `confirm` SHALL be refused with nothing sent. These tools exist only with `--allow-disk`.
- **Rationale**: the owner asked for the missing operations; a deletion is irreversible and takes a folder's contents with it.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a writable disk holding a file TEST.AKP and a folder OLD with two files, *When* `delete_file` is called with "TEST.AKP" and `confirm` "TEST.AKP", *Then* the file is gone from the listing; with another `confirm`, nothing is sent. *When* `delete_folder` is called for OLD with its name as `confirm`, *Then* it is refused and says the folder holds 2 items; with `delete_contents` true it is deleted.
- **Dependencies**: RQ-MCP-023, RQ-MCP-024, RQ-MCP-032; ADR-MCP-004 (DEC-MCP-023, DEC-MCP-024)

### RQ-MCP-040: Memory and disk space, and what sampler this is
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `get_system_info` the server SHALL answer the sampler's model, its OS version, the free wave memory (percent and bytes) and the free program-and-keygroup memory (percent); WHEN it calls `get_disk_space` (with `--allow-disk`) it SHALL answer the free space of the current disk in bytes.
- **Rationale**: a 40 MB sample took 60 s to load: a person needs to know whether there is room first.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler, *When* `get_system_info` is called, *Then* the answer states the model, the OS version and both free-memory figures; *When* `get_disk_space` is called with a disk selected, *Then* it gives the free bytes, and with no disk it says to select one.
- **Dependencies**: RQ-MCP-005; ADR-MCP-004 (DEC-MCP-024)

### RQ-MCP-041: Auditioning a sample or a file
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `audition_sample` with `action` start or stop, the server SHALL start or stop the audition of the current sample; WHEN it calls `audition_file` (with `--allow-disk`) with `action` start and a file name of the current folder, or stop, it SHALL start or stop the audition of that file; the answer SHALL say that a started audition plays until stopped.
- **Rationale**: a person choosing a sample wants to hear it.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* a current sample, *When* `audition_sample` is called with start then stop, *Then* the sampler is sent the audition start then the stop.
- **Dependencies**: RQ-MCP-020, RQ-MCP-024; ADR-MCP-004 (DEC-MCP-024)

---

## Non-Functional Requirements

### RQ-MCP-042: Every deletion is confirmed by the exact name
- **Category**: Non-Functional
- **NFR Type**: Safety
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a tool deletes something (a keygroup, a sample, a multi, a part's program, a file or a folder) and its `confirm` argument is missing or is not the exact name of what is deleted (for a keygroup: the program's name), THEN the server SHALL send nothing to the sampler and SHALL answer what to give as `confirm`; the tools that delete SHALL declare themselves destructive in their MCP annotations.
- **Metric**: number of delete frames the simulated sampler accepts with a missing or wrong `confirm`: 0.
- **Measurement Method**: a `ctest` case per tool with a missing, a wrong and a right `confirm`; the tool list is checked for the destructive annotations.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* each deleting tool, *When* it is called with no `confirm` or a wrong one, *Then* nothing is sent and the answer names the exact `confirm` expected.
- **Dependencies**: RQ-MCP-016, RQ-MCP-026; ADR-MCP-004 (DEC-MCP-023)

### RQ-MCP-043: The README is a reference for a person
- **Category**: Non-Functional
- **NFR Type**: Usability
- **EARS Type**: Ubiquitous
- **Statement**: `juce/mcp/README.md` SHALL be a reference document for the person who installs and uses the server, not notes for an AI: it SHALL say, before anything else, what the server does and what it needs; list every launch option with its default and what it switches on; say, for every tool, which option it needs (none, `--allow-disk`, `--allow-disk-refresh`), what it does in plain words, its arguments, whether it changes or deletes anything and which `confirm` it asks for; and say what has and has not been tried on a real sampler.
- **Metric**: a reader finds, in the first screen, what the server does and the command that starts it; every tool of `tools/list` is in the README's tool reference, and every launch option of `--help` is in its options table.
- **Measurement Method**: a `ctest` case that reads the README and checks that every tool name given by the server and every option of the usage text appears in it.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the README, *When* its tool tables and the server's tool list are compared, *Then* each tool of the list (all options on) is in a table with its needed option, and each option of the usage text is in the options table.
- **Dependencies**: RQ-MCP-002, RQ-MCP-023, RQ-MCP-031; ADR-MCP-004 (DEC-MCP-026)

### RQ-MCP-044: Tried on the real sampler, on objects made for the test
- **Category**: Non-Functional
- **NFR Type**: Reliability
- **EARS Type**: Ubiquitous
- **Statement**: Every requirement of this feature SHALL be verified in `ctest` against the simulated sampler; the tools SHALL then be run on the real S5000 with the owner present, on objects made for the run (programs, samples and multis created or loaded for it, the files and folders `MCPTEST*` of the owner's disk), and the README SHALL say which ones have not been.
- **Metric**: each new tool run on the real sampler, or each exception listed.
- **Measurement Method**: `ctest`; a scripted run on the real sampler, written up in the observations file.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the extended simulated conversation, *When* it runs in `ctest`, *Then* every answer matches. *Given* the owner present, *When* the real run is made, *Then* what the sampler did is written up and the objects left behind are listed.
- **Dependencies**: RQ-MCP-012, RQ-MCP-030; all requirements of this feature

---

## Open points

- **What the sampler does** on `delete_keygroup` of the last keygroup, on `set_zone_sample` of a sample already assigned to another zone, and on the rename of a file whose new name already has the extension: unknown, observed in the real run (the server refuses the last keygroup and a new name that already exists beforehand).
- **The extension of a multi's file** when saved (`.AKM` is an assumption of the simulated sampler; no multi was saved on the real one).
- **Whether the owner wants** song files, scenelists, MIDI setup, effects, eject or the others of the out-of-scope list: DEC-MCP-025 lists each with its risk.
