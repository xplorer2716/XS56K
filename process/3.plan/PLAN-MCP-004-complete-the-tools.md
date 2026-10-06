# PLAN-MCP-004: MCP Server — Completing the Tools and a Reference README

## Overview

Implements `FTR-MCP-004` on top of the server of `PLAN-MCP-001` to `PLAN-MCP-003`: the tools that the review of ADR-MCP-001 to
ADR-MCP-003 (2026-10-06) found missing (zone sample assignment, keygroups, samples, multis, files and folders, information,
audition), each deletion behind a `confirm` that is the exact name of what is deleted (`ADR-MCP-004`), and a README that a person can
use as a reference. The order follows the dependencies and the risk: the read-only and assignment tools first, the deletions after,
the disk operations behind `--allow-disk`, then the README and the real run.

**Safety note.** The new tools delete memory items, files and folders. Every `ctest` run is against the simulated sampler; the real
run (TASK-MCP-037) is made with the owner present, on objects made for it (programs, samples and multis created or loaded for the
run, the `MCPTEST*` folders and test files of the owner's disk) and never on the owner's own files. The refresh of the disk list is
never sent (`ADR-MCP-003` DEC-MCP-020). Every commit carries "(HOL -Human on the loop)" after the subject the `agnos-git-workflow`
skill computes.

## References
- **Requirements**: RQ-MCP-034 to RQ-MCP-044 (`FTR-MCP-004`); RQ-MCP-013 (`FTR-MCP-001`); RQ-MCP-020, RQ-MCP-021 (`FTR-MCP-002`);
  RQ-MCP-023, RQ-MCP-024, RQ-MCP-032 (`FTR-MCP-003`)
- **ADRs**: ADR-MCP-004 (Proposed): DEC-MCP-023 to DEC-MCP-026; ADR-MCP-003: DEC-MCP-019 (amended); ADR-MCP-002: DEC-MCP-010
  (amended), DEC-MCP-011

The plan has 10 tasks (TASK-MCP-028 to TASK-MCP-037). 028 authors the artifacts; 029 the zone sample tools; 030 the keygroups; 031
the samples; 032 the multis; 033 the file and folder operations; 034 the information tools; 035 the audition; 036 the README, the
simulated conversation and the documents; 037 the real run (after 036, with the owner). Tier M for 029 to 035, L for 036 (a rewrite
of the reference, a test that reads it and the conversation), S for 028, M for 037. The tasks 029 to 035 are independent of one
another and are done in this order only for readability; each ends with a green full `ctest` and a commit. The owner lifted the
AGNOS limit of tasks per session ("tu peux faire toutes les taches", 2026-10-05).

What the review found and this plan does not schedule (ADR-MCP-004 DEC-MCP-025: the owner decides): song files, set lists,
scenelists; the sampler's name, clock, play mode, lock and MIDI setup; the effects board; ejecting a disk; deleting all and clearing
the memory; the front-panel keys.

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-MCP-028: Author FTR-MCP-004, ADR-MCP-004 and PLAN-MCP-004
- **Tier**: S
- **Status**: Done
- **Description**: Write the feature file, the architecture decision record and this plan from the owner's requests of 2026-10-06 (review the ADRs for what was excluded without being asked; implement the missing tools; a confirmation for what is destructive; the README must be a clear reference for a human; structure the approach in a plan).
- **Requirement refs**: RQ-MCP-034 to RQ-MCP-044
- **ADR refs**: ADR-MCP-004
- **Acceptance Criteria** (Gherkin): *Given* the owner's requests, *When* the artifacts are read, *Then* each requirement has Gherkin criteria, each decision is a `DEC-MCP-` heading, and every task below names its requirements.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**: The confirmation rule (the exact name), the folder-contents rule of `delete_folder` and the choice of no new launch option are the agent's design for what the owner asked ("ok pour avoir une confirmation pour ce qui est destructif"); the tool names are the agent's. ADR-MCP-004 is Proposed and reviewable with the pull request.

### TASK-MCP-029: Zone samples: `set_zone_sample` and `get_zone_samples`
- **Tier**: M
- **Status**: Done
- **Description**: Add the gateway's zone sample calls (`setZoneSample`, `getZoneSamples`, selecting the keygroup like the parameter tools do) and the two tools; the simulated sampler's behaviour for §06/&01 and &21 is checked and completed.
- **Requirement refs**: RQ-MCP-034, RQ-MCP-013
- **ADR refs**: ADR-MCP-004 (DEC-MCP-024)
- **Acceptance Criteria** (Gherkin): *Given* a program with two keygroups and a sample KICK, *When* `set_zone_sample` is called with KICK, zone 1 and keygroup 2, *Then* the assignment is sent for keygroup 2 and read back; *When* `get_zone_samples` is called for keygroup 2, *Then* zone 1 is KICK and the others have no sample; an unknown sample, a zone outside 1 to 4 and an unknown keygroup send nothing and list what exists.
- **Dependencies**: TASK-MCP-028
- **Assignee**: AI
- **Verification**: `juce/tests/mcp/ZoneSampleToolsTests.cpp` (7 cases, over the shared `ToolRig.hpp`), re-run in this session: `xs56k_mcp_tests "[zonesample]"` → "All tests passed (119 assertions in 7 test cases)". They cover: both tools listed (set not read-only and not destructive, get read-only); KICK assigned to zone 1 of keygroup 2 of BASS sends one `&01`, is read back and listed for keygroup 2 only (zones 2 to 4 "no sample", keygroups 1 and 3 absent), and the listing of all keygroups shows keygroup 1 zone 1 as "no sample"; `kick` in lower case is assigned as "KICK"; an unknown sample, zones 0, 5 and -1, keygroup 4 of 3, a missing or badly typed argument and an extra argument send nothing and say what exists; no program in memory is an error. The two scripted conversations' tool lists gained exactly `get_zone_samples` and `set_zone_sample` (their expected output regenerated and the difference read: only answer id 2). Full `ctest` (Debug, MSVC `/W4 /WX`): 916 of 916 pass (909 before, 7 new). **The tests were written before the code but were not run red before it** (written and built together): a process gap, noted here and not repeated. NOT run on the real sampler (TASK-MCP-037).
- **Assumptions**: `set_zone_sample` needs `keygroup` (no "all keygroups"); a zone has no way to be cleared (the sampler's set takes a name, and "no sample" is only a reading); the sample is found without regard to case, spaces or hyphens and sent under the name the sampler lists; `get_zone_samples` without a keygroup reads all of them, one command per zone (five per keygroup).

### TASK-MCP-030: Keygroups: `add_keygroups` and `delete_keygroup`
- **Tier**: M
- **Status**: Done
- **Description**: Add the gateway calls and the two tools; `delete_keygroup` takes `confirm` equal to the current program's exact name and refuses the last keygroup; the source check allows the two primitives in the memory unit.
- **Requirement refs**: RQ-MCP-035, RQ-MCP-042
- **ADR refs**: ADR-MCP-004 (DEC-MCP-023, DEC-MCP-024)
- **Acceptance Criteria** (Gherkin): *Given* a program PAD with 2 keygroups, *When* `add_keygroups` is called with 2, *Then* it has 4; *When* `delete_keygroup` is called for keygroup 3 with `confirm` "PAD", *Then* it has 3; with a wrong or no `confirm`, nothing is sent; the last keygroup is refused.
- **Dependencies**: TASK-MCP-028
- **Assignee**: AI
- **Verification**: tests first, **run red before the code** (`KeygroupToolsTests.cpp`, 7 cases: "test cases: 7 | 0 passed | 7 failed"), then green: `xs56k_mcp_tests "[keygroup]"` → "All tests passed (112 assertions in 7 test cases)" (re-run in this session). They cover: `add_keygroups` not destructive and `delete_keygroup` destructive in the annotations; BASS (3) plus 2 gives 5, read back by `get_status`; 97 more (over 99), 0, -2, "two", no count and an extra argument send nothing; keygroup 3 of BASS with `confirm` BASS leaves 2, read by `get_status`; `confirm` "bass", "PAD", "BASS ", "" and none or no keygroup send nothing and name "BASS"; keygroups 4, 0 and -1 are refused with "3 keygroups"; the only keygroup of PAD is refused as the last one. The two conversations' tool lists gained exactly `add_keygroups` and `delete_keygroup` (regenerated, difference read). The source check allows `akm::deleteKeygroupFromProgram` in `SamplerGateway.cpp` only. Full `ctest` (Debug, MSVC `/W4 /WX`): 923 of 923 pass (916 before, 7 new). One discrepancy caught by running the tests: two of my own assertions counted the keygroup commands the rig sends to build BASS and LEAD (2) as if they were the tool's; the assertions now compare with the count before the call (a correction of the expectation, not of a behaviour). NOT run on the real sampler (TASK-MCP-037).
- **Assumptions**: `delete_keygroup` is confirmed by the program's name and names the keygroup by number; the keygroup count is checked before the confirmation, and the last keygroup is refused before it too (nothing to confirm); what the sampler does with the numbers after a deleted keygroup is not known (the answer tells the client to read the keygroups again); the cap of 99 keygroups is the sampler's own (the AKM layer's count limit for `addKeygroupsToProgram` is 1 to 98 per call, the tool allows 1 to 99 and the gateway refuses a total above 99).

### TASK-MCP-031: Samples: `rename_sample` and `delete_sample`
- **Tier**: M
- **Status**: Done
- **Description**: Add the gateway calls and the two tools on the current sample; `delete_sample` takes `confirm` equal to the current sample's exact name; a new name that another sample bears is refused.
- **Requirement refs**: RQ-MCP-036, RQ-MCP-042
- **ADR refs**: ADR-MCP-004 (DEC-MCP-023, DEC-MCP-024)
- **Acceptance Criteria** (Gherkin): *Given* samples KICK and SNARE with KICK current, *When* `rename_sample` is called with "KICK2", *Then* the list has KICK2 and SNARE; *When* `delete_sample` is called with `confirm` "KICK2", *Then* only SNARE remains; with a wrong `confirm`, nothing is sent.
- **Dependencies**: TASK-MCP-028
- **Assignee**: AI
- **Verification**: tests first, **run red before the code** (`SampleStructureToolsTests.cpp`, 7 cases: "test cases: 7 | 0 passed | 7 failed"), then green: `xs56k_mcp_tests "[samplestructure]"` → "All tests passed (136 assertions in 7 test cases)". They cover: `rename_sample` not destructive and `delete_sample` destructive; KICK renamed to KICK2 sends one rename, is listed and KICK is gone; "SNARE", "snare" and "Sn-are" are refused as already held; a missing, empty, 21-character, non-string, non-ASCII name and an extra argument send nothing; no current sample asks for `select_sample` for both tools; `delete_sample` with `confirm` KICK leaves SNARE and PAD ("2 samples"); `confirm` "kick", "SNARE", "KICK ", "", none or a number send nothing and name "KICK". The conversations' tool lists gained exactly `delete_sample` and `rename_sample` (regenerated, difference read). One existing test encoded the old rule ("no tool deletes or renames a sample", `SampleToolsTests.cpp`): it now forbids only `delete_all_samples`, `load_sample` and `create_sample`, its other checks unchanged (a change of expectation caused by ADR-MCP-004, not made to force a pass). Full `ctest` (Debug, MSVC `/W4 /WX`): 930 of 930 pass (923 before, 7 new). NOT run on the real sampler (TASK-MCP-037).
- **Assumptions**: a sample's name is 1 to 20 printable ASCII characters (the AKM item's 20; the owner's disk has samples of 14 characters); a new name is refused when another sample bears it without regard to case, spaces or hyphens (stricter than the sampler); renaming a sample to a name that differs from its own only by case is allowed; the sampler's own handling of a duplicate is not known.

### TASK-MCP-032: Multis: create, rename, delete, part program, clear part, program number
- **Tier**: M
- **Status**: Done
- **Description**: Add the gateway calls and the seven tools (`create_multi`, `rename_multi`, `delete_multi`, `set_part_program`, `clear_part`, `set_multi_program_number`, and the read `get_part_programs` added to read the assignments back); parts are numbered from 1 and sent minus one; the deleting ones take `confirm` equal to the current multi's exact name.
- **Requirement refs**: RQ-MCP-037, RQ-MCP-038, RQ-MCP-042
- **ADR refs**: ADR-MCP-004 (DEC-MCP-023, DEC-MCP-024)
- **Acceptance Criteria** (Gherkin): *Given* two multis, *When* `create_multi` is called with "STAGE", *Then* it is listed and current; *When* `set_part_program` is called with part 2 and a program, *Then* the part index 1 is assigned; *When* `clear_part` and `delete_multi` are called with a wrong `confirm`, *Then* nothing is sent.
- **Dependencies**: TASK-MCP-028
- **Assignee**: AI
- **Verification**: tests first, **run red before the code** (`MultiStructureToolsTests.cpp`, 14 cases: "test cases: 14 | 0 passed | 14 failed"), then green: `xs56k_mcp_tests "[multistructure]"` → "All tests passed (328 assertions in 14 test cases)". They cover: the tiers (the two deleting tools destructive, `get_part_programs` a read); STAGE created once, listed as current, "3 multis"; duplicates "LIVE", "live", "Stu-dio" and bad names refused for create and rename; LIVE renamed to LIVE2; `delete_multi` with `confirm` LIVE leaves STUDIO; wrong or missing `confirm` for `delete_multi` and `clear_part` send nothing and name "LIVE"; with no current multi all six acting tools answer "select_multi" and send nothing; part 2 given the program "lead" is sent by name with the part index 1 and read back, by position 2 it is sent by index and read back as "PAD"; an unknown program (listing the programs), position 3 or -1, part 0 or 1000, no or both of `program` and `position`, a missing or badly typed part send nothing; `clear_part` removes the program of part 2 (index 1) and a part that plays nothing is refused; the program number 5 then null are set and read back, 0, 129, "five", -3 refused; `get_part_programs` says no part plays a program. **The red phase and a segmentation fault caught a real defect**: with no current multi, the first call of `rename_multi` (and of the other tools acting on the current multi) dereferenced a session that was not open yet (the helper `currentMultiEntry` did not connect); it now connects, as `listMultis` does. The conversations' tool lists gained exactly the seven tools (regenerated, difference read). One existing test encoded the old rule ("no tool creates, deletes, renames or assigns a multi", `MultiToolsTests.cpp`): it now forbids only `delete_all_multis`, its other checks unchanged (a change of expectation caused by ADR-MCP-004). `ctest` excluding only the unrelated `bld_mutate_tool_script_tests` (the build-tool test, 42 s, run in full at the end of the batch): 943 of 943 pass. NOT run on the real sampler (TASK-MCP-037).
- **Assumptions**: a multi's name is 1 to 20 printable ASCII characters (the AKM item's 20); a new name is refused when another multi bears it without regard to case, spaces or hyphens; `clear_part` is confirmed by the multi's name (the part's program goes, the multi stays); the part number is checked against the current multi's part count (32, 64 or 128); the program number is 1 to 128 or null; whether the S5000 keeps a multi's parts when the multi is renamed is not known.

### TASK-MCP-033: Files and folders: rename and delete
- **Tier**: M
- **Status**: Done
- **Description**: Add the gateway calls and the four tools (`rename_file`, `rename_folder`, `delete_file`, `delete_folder`), only with `--allow-disk`; a rename refuses an existing new name and an extension the sampler would double; `delete_folder` refuses a non-empty folder unless `delete_contents` is true; the source check allows the primitives and their confirmation types in the disk unit.
- **Requirement refs**: RQ-MCP-039, RQ-MCP-042
- **ADR refs**: ADR-MCP-004 (DEC-MCP-023, DEC-MCP-024); ADR-MCP-003 (DEC-MCP-019)
- **Acceptance Criteria** (Gherkin): *Given* a writable disk with TEST.AKP and a folder OLD holding two files, *When* `delete_file` is called with the name and a wrong `confirm`, *Then* nothing is sent; with the right one the file is gone; *When* `delete_folder` is called for OLD with its name as `confirm`, *Then* it is refused and says 2 items; with `delete_contents` true it is deleted; the renames refuse an existing name and a read-only disk.
- **Dependencies**: TASK-MCP-028
- **Assignee**: AI
- **Verification**: tests first, **run red before the code** (`DiskFileOpsTests.cpp`, 11 cases: "test cases: 11 | 0 passed | 11 failed"), then green: `xs56k_mcp_tests "[fileops]"` → "All tests passed (288 assertions in 11 test cases)". They cover: the four tools listed with the flag (renames not destructive, deletions destructive) and absent without it (-32602); TEST.AKP renamed with "NEW" becomes NEW.AKP, one `&28` sent, the others unchanged; a new name that carries the extension, one a file bears ("other" against OTHER.AKP) and a name that is not there send nothing and say why; missing, blank, path-like or badly typed arguments, a read-only disk (CD1) and no disk send nothing for all four tools; a folder renamed, a name a folder or a file bears refused; `delete_file` with `confirm` TEST.AKP removes only that file; `confirm` "test.akp", "OTHER.AKP", "TEST.AKP ", "TEST" and "" send nothing and give the exact name; an empty folder is deleted without `delete_contents`; the folder OLD (a file and a folder) is refused with "2 items" and `delete_contents`, nothing sent and the current folder still the root, then deleted with `delete_contents` true ("and the 2 items it held"); a wrong `confirm` for a folder sends nothing; with the sampler's stale file list a deletion and a rename are read back from a refreshed listing. Two corrections of the **simulated** sampler came out of it: in the stale-list mode it now also serves the file index by name and answers sizes from its copy whatever the folder now holds (the first attempt read a size of an index that no longer existed). The conversations' tool lists gained exactly the four tools (regenerated, difference read). One existing test encoded the old rule (`DiskSaveTests.cpp` forbade `delete_file`, `delete_folder` and `rename_file`): it keeps `eject` and `format` forbidden. The source check allows the rename and delete primitives and `ConfirmDeleteFile` and `ConfirmDeleteSubFolder` in `SamplerGatewayDisk.cpp` only. `ctest` excluding only the unrelated `bld_mutate_tool_script_tests` (run in full at the end of the batch): 954 of 954 pass. NOT run on the real sampler (TASK-MCP-037, on the `MCPTEST*` folders and test files only).
- **Assumptions**: `rename_file` takes the new name without the extension (the sampler appends it: seen on the S5000 for a program file, `.AKP`; **not observed for a `.WAV` file**, which the real run must check); the expected new name is the new name plus the old extension, and a listing that does not show it after a reopen is an error; a file and a folder cannot share a name; to count what a folder holds before its deletion the gateway opens it and closes it again (two extra commands); the delete tools match `name` tolerantly but `confirm` must equal the listed name exactly; the sampler's own behaviour on deleting a folder with its contents is not observed (the AKM suite deleted an empty one).

### TASK-MCP-034: Information: `get_system_info` and `get_disk_space`
- **Tier**: M
- **Status**: Not Started
- **Description**: Add the gateway reads (model, OS version, free wave memory in percent and bytes, free program-and-keygroup memory, free space of the current disk) and the two read tools (`get_disk_space` only with `--allow-disk`).
- **Requirement refs**: RQ-MCP-040
- **ADR refs**: ADR-MCP-004 (DEC-MCP-024)
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler, *When* `get_system_info` is called, *Then* the answer states the model, the OS version and both free-memory figures; *When* `get_disk_space` is called with a disk selected, *Then* it gives the free bytes, and with none it says to select one.
- **Dependencies**: TASK-MCP-028
- **Assignee**: AI
- **Verification**: (to be filled at closure)
- **Assumptions**: None yet.

### TASK-MCP-035: Audition: `audition_sample` and `audition_file`
- **Tier**: M
- **Status**: Not Started
- **Description**: Add the gateway calls and the two tools with an `action` of start or stop (`audition_file` only with `--allow-disk`, and takes a file name for start).
- **Requirement refs**: RQ-MCP-041
- **ADR refs**: ADR-MCP-004 (DEC-MCP-024)
- **Acceptance Criteria** (Gherkin): *Given* a current sample, *When* `audition_sample` is called with start then stop, *Then* the sampler is sent the audition start then the stop; `audition_file` with start needs a name the folder lists.
- **Dependencies**: TASK-MCP-028
- **Assignee**: AI
- **Verification**: (to be filled at closure)
- **Assumptions**: None yet.

### TASK-MCP-036: The README as a reference, the simulated conversation and the documents
- **Tier**: L
- **Status**: Not Started
- **Description**: Rewrite `juce/mcp/README.md` as the human reference of ADR-MCP-004 DEC-MCP-026 (what it is and needs, start, options table, which tool needs which option, the tools by domain with arguments and confirmations, the safety rules, what has been tried on a real sampler, troubleshooting, developers' notes last); add a `ctest` case that checks that every tool the server lists and every option of the usage text is in it; extend the scripted conversations with the new tools; update `AGENTS.md`, `CHANGELOG.md` and the checkpoint.
- **Requirement refs**: RQ-MCP-043, RQ-MCP-044
- **ADR refs**: ADR-MCP-004 (DEC-MCP-026)
- **Acceptance Criteria** (Gherkin): *Given* the README, *When* its tool tables and the server's tool list are compared, *Then* each tool of the list (all options on) is in a table with its needed option, and each option of the usage text is in the options table; the scripted conversations match.
- **Dependencies**: TASK-MCP-029 to TASK-MCP-035
- **Assignee**: AI
- **Verification**: (to be filled at closure)
- **Assumptions**: None yet.

### TASK-MCP-037: Real-sampler run of the new tools with the owner
- **Tier**: M
- **Status**: Blocked
- **Description**: With the owner present, run each new tool on the real S5000 on objects made for the run (a program created for it, samples loaded from the disk, a multi created for it, the `MCPTEST*` folders and a test file), one at a time, write up what the sampler did in `OBSERVATIONS-RQ-MCP-012-real-sampler.md`, list what is left on the disk, and correct the simulated sampler for what it did differently.
- **Requirement refs**: RQ-MCP-044
- **ADR refs**: ADR-MCP-004
- **Acceptance Criteria** (Gherkin): *Given* the owner present, *When* each new tool is run alone, *Then* its answer and what the sampler did are in the observations file, the objects created are removed or listed, and the simulated sampler matches what was seen.
- **Dependencies**: TASK-MCP-036 (and the owner)
- **Assignee**: Human and AI
- **Verification**: NOT DONE: blocked until the tools exist and the owner is present.
- **Assumptions**: Never run on the owner's own files; no tool of this plan is run on the refresh of the disk list; a deletion is run only on the test file and folders made for the run.
