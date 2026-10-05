# PLAN-MCP-003: MCP Server — Loading and Saving through the Sampler's Disks

## Overview

Implements `FTR-MCP-003` on top of the server of `PLAN-MCP-001` and `PLAN-MCP-002`: the disk tools (browse, load, save) behind the
guards of `ADR-MCP-003`: an opt-in launch flag, a timeout of their own for the slow commands, saves that never overwrite by
accident, and the source check as an allow list. The order is bottom-up: the options and the gateway's disk unit with the browsing
tools first (020 builds on them), then the loads, then the saves, then the simulated conversation, the real run and the
documentation.

**Safety note.** Section 10 is the section that hung the owner's S5000 (`OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`, F4 to F7).
Every `ctest` run is against the simulated sampler; the real run (TASK-MCP-023) is made only with the owner present and a disk with
a test file plugged in, one slow command at a time, and is **Blocked** until then. Nothing in this plan deletes, renames, creates a
folder, ejects or formats (`RQ-MCP-028`). Every commit carries "(HOL -Human on the loop)" after the subject the `agnos-git-workflow`
skill computes.

## References
- **Requirements**: RQ-MCP-023 to RQ-MCP-030 (`FTR-MCP-003`); RQ-MCP-014, RQ-MCP-022 (`FTR-MCP-002`)
- **ADRs**: ADR-MCP-003 (Proposed): DEC-MCP-015 to DEC-MCP-019; ADR-MCP-002: DEC-MCP-010 (amended), DEC-MCP-014; ADR-MCP-001:
  DEC-MCP-003, DEC-MCP-004, DEC-MCP-008.

The plan has 7 tasks (TASK-MCP-018 to TASK-MCP-024). 018 authors the artifacts; 019 the options and the browsing tools (after 018);
020 the loads (after 019); 021 the saves (after 019); 022 the simulated conversation and the hang test (after 020 and 021); 023 the
real run (after 022, with the owner and a disk); 024 the documentation (after 022). Tier L for 019 (a launch argument, a changed AKM
signature, a new unit and a changed safety rule), M for 020 to 023, S for 018 and 024. The owner lifted the AGNOS limit of 10 tasks
per session for this work ("tu peux faire toutes les taches", 2026-10-05): the session has done 8 tasks of PLAN-MCP-002 before it.

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-MCP-018: Author FTR-MCP-003, ADR-MCP-003 and PLAN-MCP-003
- **Tier**: S
- **Status**: Done
- **Description**: Write the feature file, the architecture decision record and this plan from the owner's request (the MCP must be able to load and save), the owner's choices (a save refuses to overwrite unless `overwrite` is explicit; the whole scope: browse, load a file, save an item, load a folder, save all; a real run at the end with a disk the owner plugs in) and the AKM disk primitives.
- **Requirement refs**: RQ-MCP-023 to RQ-MCP-030
- **ADR refs**: ADR-MCP-003
- **Acceptance Criteria** (Gherkin): *Given* the owner's request, *When* the artifacts are read, *Then* each requirement has Gherkin criteria, each decision is a `DEC-MCP-` heading, and every task below names its requirements.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**: The opt-in flag `--allow-disk` and the disk timeout `--disk-timeout-ms` are the agent's design (the owner asked for loading and saving, not for the guards), proposed in ADR-MCP-003 and reviewable with the pull request. The ADR stays "Proposed".

### TASK-MCP-019: Disk options, gateway disk unit and browsing tools
- **Tier**: L
- **Status**: Done
- **Description**: Add `--allow-disk` and `--disk-timeout-ms` to the server's arguments; give the slow AKM primitives a defaulted `CommandOptions` parameter; add the gateway's disk unit (`SamplerGatewayDisk.cpp`) with the disks, the selection, the folder contents and the navigation; add the tools `list_disks`, `select_disk`, `list_disk_contents`, `open_folder` and `close_folder`, offered only with the flag; change the source check to the allow list of DEC-MCP-019.
- **Requirement refs**: RQ-MCP-023, RQ-MCP-024, RQ-MCP-028, RQ-MCP-029
- **ADR refs**: ADR-MCP-003 (DEC-MCP-015, DEC-MCP-016, DEC-MCP-017, DEC-MCP-019)
- **Acceptance Criteria** (Gherkin): *Given* the server without `--allow-disk`, *When* tools/list is read, *Then* no disk tool is in it and calling one answers -32602; with the flag they are listed. *Given* a simulated sampler with a disk holding folders and files, *When* the disks are listed, one selected and its contents listed, *Then* the folders and files come with their sizes and the path; `open_folder` descends and `close_folder` goes up. *Given* `list_disks` without `refresh`, *Then* the refresh item is not sent; with it, it is. *Given* the sources, *Then* only the browsing, loading and saving primitives are called, only from the disk unit.
- **Dependencies**: TASK-MCP-018
- **Assignee**: AI
- **Verification**: tests written first (`juce/tests/mcp/DiskBrowseTests.cpp`, 9 cases, and 4 cases added to `ServerOptionsTests.cpp`) and run red (no option, no disk tool, no `diskTimeout`), then green. `xs56k_mcp_tests` 157 cases / 5954 assertions pass (144 before the task, 13 new). Options: `--allow-disk` (no value; `--allow-disk=yes` is refused) and `--disk-timeout-ms` (1 to 1800000, default 120000; 0, -5, "abc", 1800001 and the empty value refused, each error naming the argument); the usage text names both and warns about "switched off and on"; `gatewayConfigFrom` carries the disk timeout. Without the flag the five browsing tools are not in `tools/list` and each call answers -32602 (no `DiskGetList` accepted); with it they are listed, `list_disks` and `list_disk_contents` read-only, `select_disk`, `open_folder`, `close_folder` not read-only and not destructive. On two simulated disks (HD1 hard disk MSDOS writable, CD1 CD-ROM ISO9660 read-only): `list_disks` lists both with type, format and writability and sends no refresh; with `refresh` true it sends `&01` once; with no disk it says none is connected; `select_disk` by name or by handle marks the disk current in the next listing, an unknown name lists the disks, both or neither argument and an unknown handle are errors; `list_disk_contents` gives the folders and the files with their sizes and the path ("(root)" at the root) and says to select a disk first when none is; `open_folder` descends and lists, `close_folder` goes up, a missing folder sends nothing and lists the folders, the root cannot be left (nothing sent). A refresh the simulated sampler never answers (`SilentItem` §10/&01) takes the disk timeout (1500 ms against the command timeout of 300 ms) and the answer says "switched off and on" and "not retried". `CheckNoDestructiveCalls.cmake` is now an allow list per file (program structure primitives in `SamplerGateway.cpp`, load and save primitives and their types in `SamplerGatewayDisk.cpp`); run on `juce/mcp` it passes, and on a copy with `akm::loadFile` and `akm::deleteFile` added to `Tools.cpp` it fails naming the file. The slow AKM primitives (`updateDiskList`, `loadFolder`, `loadFile`, `loadFileWithDependents`, `saveMemoryItem`, `saveAllMemoryItems`) take a defaulted `CommandOptions`, unused by their existing callers. Full `ctest` (Debug, MSVC `/W4 /WX`): 874 of 874 pass (861 before, 13 new). `xs56k_mcp_server.exe` links with the options (built in a scratch folder), `--help` shows them. NOT run on the real sampler: nothing of this task was sent to the S5000 (no disk is attached and the refresh is the command that hung it).
- **Assumptions**: the simulated sampler's `&09` (the current path of the disk) answered the empty root path whatever folder was open: it now answers the names of the opened folders joined by '/', since the tools needed a path; the real format is unknown (only the root, empty, is given by the spec) and is observed in TASK-MCP-023. The gateway tells the root by the empty path (the spec's "a single byte = 0") and does not send `closeFolder` there. The shared helpers of the gateway (waiting for a completion, explaining an outcome) moved to an internal header `GatewayDetail.hpp`, and the disk unit reaches the session through a private `session()`. The disk tools are marked in the instructions only when the flag is given (`diskInstructions`). `list_disks` marks the current disk by asking for the current handle and treating a failure as "none". The scripted conversation is unchanged: the simulated server does not take the flag yet (TASK-MCP-022).

### TASK-MCP-020: Load a file or a folder
- **Tier**: M
- **Status**: Not Started
- **Description**: Add `load_file` (a name of the current folder, optional `with_dependents` and sample load mode) and `load_folder`, with the disk timeout, the check that the name is in the folder, the counts of programs and samples before and after, and the message of a silent sampler.
- **Requirement refs**: RQ-MCP-025, RQ-MCP-029
- **ADR refs**: ADR-MCP-003 (DEC-MCP-017)
- **Acceptance Criteria** (Gherkin): *Given* a simulated disk holding a program file and a sample file, *When* `load_file` is called on each, *Then* the program and the sample are in memory and the answer says so; with `with_dependents` the files a program depends on are loaded too; an unknown name sends nothing and lists the files; a sampler that stops answering gives an answer within the disk timeout that mentions the power cycle and no retry.
- **Dependencies**: TASK-MCP-019
- **Assignee**: AI
- **Verification**: (to be filled at closure)
- **Assumptions**: None yet.

### TASK-MCP-021: Save a memory item, save every item of a kind
- **Tier**: M
- **Status**: Not Started
- **Description**: Add `save_memory_item` (kind, name, `overwrite`, `save_children`) and `save_all_memory_items` (kind, `confirm`, `overwrite`, `save_children`), with the writability check, the listing before and after, the refusal to overwrite by default and the count tie of the bulk save.
- **Requirement refs**: RQ-MCP-026, RQ-MCP-027, RQ-MCP-029
- **ADR refs**: ADR-MCP-003 (DEC-MCP-017, DEC-MCP-018)
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding a program and a writable disk, *When* it is saved, *Then* the file is in the folder and the answer says so; an existing file without `overwrite` sends nothing and names the file; `overwrite` true sends the save; a disk that is not writable sends nothing. *Given* three programs, *When* `save_all_memory_items` is called with confirm 2, *Then* nothing is sent and the answer says 3; with confirm 3 the folder gains the files.
- **Dependencies**: TASK-MCP-019
- **Assignee**: AI
- **Verification**: (to be filled at closure)
- **Assumptions**: None yet.

### TASK-MCP-022: Simulated conversation, fidelity and the hang test
- **Tier**: M
- **Status**: Not Started
- **Description**: Give `xs56k_mcp_server_simulated` a disk (folders, files, a program and a sample to load), extend the scripted conversation to the disk tools (the executable of the test is launched with the flag), and add the test of the silent sampler with a short disk timeout.
- **Requirement refs**: RQ-MCP-029, RQ-MCP-030, RQ-MCP-023
- **ADR refs**: ADR-MCP-003 (DEC-MCP-015, DEC-MCP-017); ADR-MCP-002 (DEC-MCP-014)
- **Acceptance Criteria** (Gherkin): *Given* the extended conversation, *When* it runs against the simulated sampler in `ctest`, *Then* every answer matches the expected one. *Given* a disk timeout of 500 ms and a sampler that does not answer a load, *Then* the answer arrives in about 500 ms and mentions the power cycle.
- **Dependencies**: TASK-MCP-020, TASK-MCP-021
- **Assignee**: AI
- **Verification**: (to be filled at closure)
- **Assumptions**: None yet.

### TASK-MCP-023: Real-sampler run with the owner's disk
- **Tier**: M
- **Status**: Blocked
- **Description**: With the owner present and a disk with a test file plugged into the S5000, run the disk tools one slow command at a time (browse without the refresh, then the refresh, a load, a save without and with `overwrite`), write up what the sampler did in `OBSERVATIONS-RQ-MCP-012-real-sampler.md` and correct the simulated sampler for what it had wrong.
- **Requirement refs**: RQ-MCP-030, RQ-MCP-024, RQ-MCP-025, RQ-MCP-026
- **ADR refs**: ADR-MCP-003 (DEC-MCP-017, DEC-MCP-018); ADR-MCP-002 (DEC-MCP-014)
- **Acceptance Criteria** (Gherkin): *Given* a disk plugged into the sampler and the owner present, *When* each disk tool is run alone, *Then* its answer and what the sampler did are in the observations file, the objects created for the run are removed or listed, and the simulated sampler matches what was seen.
- **Dependencies**: TASK-MCP-022 (and the owner's disk)
- **Assignee**: Human and AI
- **Verification**: NOT DONE: blocked on the owner plugging a disk with a test file into the sampler.
- **Assumptions**: The run is made at the end of the work, as the owner chose; a hang needs a power cycle by hand.

### TASK-MCP-024: Documentation and closure
- **Tier**: S
- **Status**: Not Started
- **Description**: Update `AGENTS.md`, `CHANGELOG.md`, `juce/mcp/README.md` and the checkpoint for the disk tools, the flag, the timeout and what was and was not run on the real sampler.
- **Requirement refs**: RQ-MCP-030
- **ADR refs**: ADR-MCP-003
- **Acceptance Criteria** (Gherkin): *Given* the delivered tools, *When* the documents are read, *Then* each disk tool is listed with its tier, the flag and the timeout are explained, and the documents say the disk tools are not run on the hardware until TASK-MCP-023 is done.
- **Dependencies**: TASK-MCP-022
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**: None
