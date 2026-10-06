# FTR-MCP-003: MCP Server — Loading and Saving through the Sampler's Disks

## Overview

Session MCP, 2026-10-05, at the owner's request (HOL, Human on the loop): "the MCP must be able to make the sampler load a
sample or a program, and save". FTR-MCP-002 kept the disk out of the server (RQ-MCP-014) because section 10 is the section that
hung the owner's S5000; without it, though, a person can never get a sample, a program or a multi into the sampler's memory
through the server, nor keep what they edited. This feature adds the disk, behind guards that answer the observed hang:

1. **Browse**: list the disks connected to the sampler, select one, list the contents of its current folder (sub-folders and
   files with their sizes), open a sub-folder and go back up.
2. **Load**: load a file (a program, a sample or a multi, its extension deciding), with or without the files it depends on; load a
   folder.
3. **Save**: save one memory item (a program, a sample or a multi) to the current folder, and save every item of a kind. A save
   never replaces an existing file unless the caller says so explicitly.

**What the server cannot do.** It does not move files between the computer and the sampler: SysEx section 10 acts on the
sampler's **own disks** (hard disk, floppy, CD-ROM, removable). "Load this sample" means "load this file from the disk plugged
into the sampler".

**Guards (ADR-MCP-003).** The disk tools exist only when the server is launched with `--allow-disk` (the owner's explicit choice,
written in the client's server configuration); the slow commands have a long, configurable timeout and a message that says what
a silent sampler means; the disk refresh is never sent unless asked, and asked for by a launch option of its own (`--allow-disk-refresh`, RQ-MCP-031); saves refuse to overwrite by default; and eject and format stay never offered (create-folder was added by RQ-MCP-032 and the rename and delete of files and folders by
RQ-MCP-039 of FTR-MCP-004, at the owner's request of 2026-10-06).

**Vocabulary** is unchanged: names as the sampler's screen shows them, sizes in bytes, no invented file format.

**Out of scope.** Ejecting a disk (deleting and renaming a file or a folder were taken up by FTR-MCP-004), formatting, saving or loading song
files and set lists, scenelists and MIDI files (SMF), auditioning a file from disk, moving files between the computer and the
sampler, and everything FTR-MCP-002 already excludes (Delete ALL programs and multis, Clear Sampler Memory).

**Depends on** FTR-MCP-001, FTR-MCP-002, FTR-AKM-007 (the disk primitives: RQ-AKM-060 to RQ-AKM-071), ADR-MCP-001, ADR-MCP-002,
ADR-AKM-001. The observation of the hang: `process/2.architecture/OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md` (F4 to F7).

**Sources.** `documents/_index/sysex_spec.kb.md` (§10: refresh the list, pick a handle, select; the disk selection by SysEx is not
the front panel's), `documents/_index/sysex_spec.items.tsv` (section 10), the AKM disk primitives in
`juce/akm/include/akm/DiskPrimitives.hpp`.

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: an MCP client (Claude Code or another) and the person using it; CI (simulated sampler only).

---

## Functional Requirements

### RQ-MCP-023: The disk tools are opt-in
- **Category**: Functional
- **EARS Type**: State-driven
- **Statement**: WHILE the server was launched without `--allow-disk`, it SHALL offer none of the disk tools (they are absent from `tools/list`, and a call to one is an invalid-params error); WHILE it was launched with it, it SHALL offer them with their tier stated in their annotations and descriptions.
- **Rationale**: section 10 can leave the sampler answering nothing until it is switched off and on (the observed hang); the default configuration must not be one tool call from it.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the server without `--allow-disk`, *When* tools/list is read, *Then* no disk tool is in it and calling `load_file` answers -32602. *Given* `--allow-disk`, *Then* the disk tools are listed.
- **Dependencies**: RQ-MCP-002, RQ-MCP-013; ADR-MCP-003 (DEC-MCP-015)

### RQ-MCP-024: Browsing the disks
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `list_disks`, `select_disk`, `list_disk_contents`, `open_folder` or `close_folder`, the server SHALL list the connected disks (name, type, format, whether writable), make one current, list the current folder's sub-folders and files (name, size) with the current path, descend into a sub-folder or go back up, and answer in plain words; `list_disks` SHALL send the sampler's refresh of its disk list only when asked for it (`refresh` true), since that command is the one that hung; IF no disk is connected or none is selected, THEN it SHALL say so and say what to do.
- **Rationale**: a person cannot load a file whose name they cannot see.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler with a disk holding folders and files, *When* the disks are listed, one is selected and its contents listed, *Then* the folders and files come with their sizes and the path. *Given* `open_folder` of a sub-folder, *Then* the path and the listing follow it, and `close_folder` returns to the parent. *Given* `list_disks` without `refresh`, *Then* the refresh item is not sent.
- **Dependencies**: FTR-AKM-007 (RQ-AKM-060 to RQ-AKM-065); ADR-MCP-003 (DEC-MCP-016)

### RQ-MCP-025: Loading a file or a folder
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `load_file` with a name of the current folder (and optionally `with_dependents` and, for a sample, how to load it: normal, RAM or virtual), or `load_folder` with a name of a sub-folder, the server SHALL send the matching load command with the long disk timeout, wait for its end, and answer what is now in memory (the programs and samples counted before and after); IF the file or folder is not in the current folder, THEN it SHALL send nothing and list what is there; IF the sampler does not answer within the timeout, THEN it SHALL say that the sampler may need to be switched off and on, and SHALL NOT retry.
- **Rationale**: this is the way a sample, a program or a multi gets into memory through the server.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated disk holding a program file and a sample file, *When* `load_file` is called on each, *Then* the program and the sample are in memory and the answer says so. *Given* `with_dependents`, *Then* the files a program depends on are loaded too. *Given* an unknown name, *Then* nothing is sent and the answer lists the files. *Given* a simulated sampler that stops answering, *Then* the answer comes within the disk timeout and mentions the power cycle.
- **Dependencies**: FTR-AKM-007 (RQ-AKM-066, RQ-AKM-070); ADR-MCP-003 (DEC-MCP-017)

### RQ-MCP-026: Saving one memory item, never overwriting by accident
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: WHEN a client calls `save_memory_item` with the kind (program, sample or multi), the name of the item, and optionally `overwrite` and `save_children`, the server SHALL find the item's position in memory, send the save with the long disk timeout and answer whether a file with that name is now in the current folder; IF `overwrite` is not true and the current folder already holds a file of the item's name, THEN it SHALL send nothing and say so; IF the item is not in memory, THEN it SHALL send nothing and list what is.
- **Rationale**: a save that silently replaces a file loses data on a disk the person cannot see from the server.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding a program and a writable disk, *When* the program is saved, *Then* the file is in the folder and the answer says so. *Given* the file exists and `overwrite` absent, *Then* nothing is sent and the answer names the file. *Given* `overwrite` true, *Then* the save is sent. *Given* a disk that is not writable, *Then* nothing is sent and the answer says so.
- **Dependencies**: FTR-AKM-007 (RQ-AKM-067, RQ-AKM-070); ADR-MCP-003 (DEC-MCP-017, DEC-MCP-018)

### RQ-MCP-027: Saving every item of a kind
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `save_all_memory_items` with a kind, `confirm` equal to the number of items of that kind in memory, and optionally `overwrite` and `save_children`, the server SHALL send the bulk save with the long disk timeout and answer how many files the current folder gained; IF `confirm` differs from the count, THEN it SHALL send nothing and give the count.
- **Rationale**: a bulk write is the most expensive mistake of the section; naming the count ties the call to what the client was last told.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* three programs in memory, *When* `save_all_memory_items` is called with confirm 2, *Then* nothing is sent and the answer says 3. *Given* confirm 3, *Then* the save is sent and the folder gains the files.
- **Dependencies**: RQ-MCP-026; ADR-MCP-003 (DEC-MCP-018)

### RQ-MCP-028: Never offered, still
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: The server SHALL expose no tool that ejects or formats a disk (the tools that rename or delete a file or a folder are those of RQ-MCP-039, which ask for a confirmation), and none that deletes all programs, samples or multis or clears the sampler's memory; the disk primitives it calls SHALL be those of browsing, loading, saving and creating a folder, called only from the gateway's disk unit.
- **Rationale**: those are the irreversible operations of the section; saving and loading are the ones a person needs.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the sources of `juce/mcp`, *When* searched for the delete, rename, eject and format primitives of the disk, *Then* there is no call, and the load, save, select, create-folder and folder-navigation primitives are called only from the gateway's disk unit.
- **Dependencies**: RQ-MCP-014; ADR-MCP-003 (DEC-MCP-019)

---

## Non-Functional Requirements

### RQ-MCP-029: Slow commands have their own timeout
- **Category**: Non-Functional
- **NFR Type**: Reliability
- **EARS Type**: Ubiquitous
- **Statement**: The server SHALL give the slow section 10 commands (the refresh of the disk list, the loads and the saves) a timeout of their own, `--disk-timeout-ms` (default 120000 ms), separate from the ordinary command timeout, and SHALL answer a silent sampler with a message that says how long it waited and that the sampler may have to be switched off and on.
- **Metric**: the answer to a silent simulated sampler arrives within the disk timeout plus the gateway's margin.
- **Measurement Method**: a `ctest` case with a simulated sampler that stops answering and a short disk timeout.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a disk timeout of 500 ms and a sampler that does not answer a load, *When* `load_file` is called, *Then* the answer arrives in about 500 ms and mentions the power cycle.
- **Dependencies**: RQ-MCP-009, FTR-AKM-007 (RQ-AKM-070); ADR-MCP-003 (DEC-MCP-017)

### RQ-MCP-030: Verified on the simulator, then on a real disk with the owner
- **Category**: Non-Functional
- **NFR Type**: Reliability
- **EARS Type**: Ubiquitous
- **Statement**: Every requirement of this feature SHALL be verified in `ctest` against the simulated sampler, and the disk tools SHALL be run on the real S5000 once, only with the owner present and a disk with a test file plugged in, each slow command alone, with what the sampler did recorded in the observations file; until then the documentation SHALL say they are not run on the hardware.
- **Metric**: each disk tool run on the real sampler, or each exception listed.
- **Measurement Method**: `ctest`; a scripted conversation on the real sampler with `--allow-disk`.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the extended scripted conversation, *When* it runs against the simulated sampler in `ctest`, *Then* every answer matches. *Given* a disk plugged into the sampler, *When* the owner is present, *Then* the real run is made one slow command at a time and written up.
- **Dependencies**: RQ-MCP-012, RQ-MCP-022, all functional requirements of this feature

### RQ-MCP-033: A save is checked against a listing the sampler has refreshed
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF, after a save, a file of a saved item is missing from the folder's listing, THEN the server SHALL close and open the current folder again, read the listing once more, and answer from that second listing; it SHALL NOT say that the folder gained nothing when the second listing holds the files.
- **Rationale**: observed on the owner's S5000 on 2026-10-06: the sampler keeps a folder's file list until the folder is opened, and a bulk save of 13 samples into a folder that already held one file was listed as 1 file, 5 s and 17 s later too; the folder held 13.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a sampler that keeps a stale file list and a root that holds a file, *When* `save_all_memory_items` saves three programs, *Then* the answer says the folder gained three files. *Given* the folder SYNTH, *When* a program is saved there, *Then* the folder is closed and opened again and is still the current folder.
- **Dependencies**: RQ-MCP-026, RQ-MCP-027; ADR-MCP-003 (DEC-MCP-022)

### RQ-MCP-032: Creating a folder on a writable disk
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the client calls `create_folder` with a name, the server SHALL create an empty sub-folder of that name in the current folder of the current disk (section 10, item &16), SHALL NOT open it, and SHALL answer after reading the folder's listing back; IF no disk is selected, the disk is read-only, the name is empty or a path, or a folder or a file of the folder already bears the name (compared without case, spaces or hyphens), THEN the server SHALL send nothing and say why.
- **Rationale**: the owner asked for it on 2026-10-06 (a place to save test files in, without cluttering the root); it deletes and replaces nothing, the AKM primitive exists and ran on the real S5000 on 2026-10-03.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a writable disk with a folder DRUMS, *When* `create_folder` is called with MCPTEST, *Then* one section 10 item &16 is sent, the answer says the folder is created and empty, and `list_disk_contents` lists MCPTEST while the current folder is unchanged. *Given* the name DRUMS, a path `A/B`, an empty name, a read-only disk or no disk, *When* it is called, *Then* nothing is sent and the answer says why.
- **Dependencies**: RQ-MCP-023, RQ-MCP-024; ADR-MCP-003 (DEC-MCP-021)

### RQ-MCP-031: The refresh of the disk list is a launch option of its own
- **Category**: Non-Functional
- **NFR Type**: Safety
- **EARS Type**: Unwanted behaviour
- **Statement**: IF the server is launched with `--allow-disk` but without `--allow-disk-refresh`, THEN the server SHALL NOT send the sampler's refresh of its disk list (section 10, item 01): `list_disks` SHALL not offer a `refresh` argument and SHALL refuse `refresh: true` with an answer that names `--allow-disk-refresh`, with nothing sent to the sampler; `--allow-disk-refresh` without `--allow-disk` SHALL be a usage error.
- **Metric**: number of `&01` frames accepted by the simulated sampler when the option is absent: 0, whatever the arguments of `list_disks`.
- **Measurement Method**: `ctest` cases on the tool list, on `list_disks` with and without the option, and on the argument parser.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a server launched with `--allow-disk` only, *When* `list_disks` is called with `refresh` true, *Then* the answer is an error that names `--allow-disk-refresh` and the sampler received no section 10 item 01. *Given* a server launched with both options, *When* `list_disks` is called with `refresh` true, *Then* the refresh is sent once.
- **Dependencies**: RQ-MCP-023, RQ-MCP-024, RQ-MCP-029; ADR-MCP-003 (DEC-MCP-020)

---

## Open points

- **The file name a save gives** (the item's name plus its extension): not assumed; the verification after a save looks for the item's name in the listing, and the real run settles the rule (the S5000 appended a second extension on a rename).
- **Whether the refresh (`&01`) is needed** with a disk that is already connected: the spec says the count and the list may be stale until it has run once in a session; the first observed hang was on a sampler with no disk at all. **Answered on 2026-10-05 for the owner's S5000**: with its SCSI2SD disk (S5K, FAT32) already connected, the refresh was not needed (the list, the selection, the browsing and the loads all worked without it) and, sent, it hung the sampler until a power cycle (RQ-MCP-031, DEC-MCP-020).
- **Which of `&15`, `&2A`, `&2B`, `&2C`, `&2D` can hang with a disk plugged in**: unknown; the real run goes one at a time.
- **How a program's samples are named when loaded with their dependents**, and what happens to a name already in memory.
