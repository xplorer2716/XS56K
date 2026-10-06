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
- **Requirements**: RQ-MCP-034 to RQ-MCP-045 (`FTR-MCP-004`); RQ-MCP-013 (`FTR-MCP-001`); RQ-MCP-020, RQ-MCP-021 (`FTR-MCP-002`);
  RQ-MCP-023, RQ-MCP-024, RQ-MCP-032 (`FTR-MCP-003`)
- **ADRs**: ADR-MCP-004 (Proposed): DEC-MCP-023 to DEC-MCP-027; ADR-MCP-003: DEC-MCP-019 (amended); ADR-MCP-002: DEC-MCP-010
  (amended), DEC-MCP-011

The plan has 10 tasks (TASK-MCP-028 to TASK-MCP-037). 028 authors the artifacts; 029 the zone sample tools; 030 the keygroups; 031
the samples; 032 the multis; 033 the file and folder operations; 034 the information tools; 035 the audition; 036 the README, the
simulated conversation and the documents; 037 the real run (after 036, with the owner). Tier M for 029 to 035, L for 036 (a rewrite
of the reference, a test that reads it and the conversation), S for 028, M for 037. The tasks 029 to 035 are independent of one
another and are done in this order only for readability; each ends with a green `ctest` run (the complete suite once per batch, the
targeted tests and `ctest -E bld_mutate_tool_script_tests` per task) and a commit. TASK-MCP-038 (Tier S) was added after 036, when the
owner found the option table of the README unclear. The owner lifted the AGNOS limit of tasks per session ("tu peux faire toutes les
taches", 2026-10-05).

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
- **Status**: Done
- **Description**: Add the gateway reads (model, OS version, free wave memory in percent and bytes, free program-and-keygroup memory, free space of the current disk) and the two read tools (`get_disk_space` only with `--allow-disk`).
- **Requirement refs**: RQ-MCP-040
- **ADR refs**: ADR-MCP-004 (DEC-MCP-024)
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler, *When* `get_system_info` is called, *Then* the answer states the model, the OS version and both free-memory figures; *When* `get_disk_space` is called with a disk selected, *Then* it gives the free bytes, and with none it says to select one.
- **Dependencies**: TASK-MCP-028
- **Assignee**: AI
- **Verification**: tests first, **run red before the code** (`SystemInfoToolsTests.cpp`, 5 cases: "test cases: 5 | 0 passed | 5 failed"), then green: `xs56k_mcp_tests "[sysinfo]"` → "All tests passed (49 assertions in 5 test cases)". They cover: `get_system_info` read-only, `get_disk_space` read-only and absent without the flag (-32602); a sampler with 16 MiB of 64 MiB of wave memory free and 80 percent of the other memory gives "Model: AKAI S5000", "Operating system: ", "Free wave memory: ... (16777216 bytes of 67108864)" and "Free program, keygroup, sample and multi memory: 80%"; an S6000 is named, and a model code that is neither says "Model: not recognised"; an extra argument is refused for both; `get_disk_space` on HD1 (1000000 bytes free) says so with a size in MB, and with no disk selected says to use `select_disk`. The conversations' tool lists gained exactly `get_system_info` (without the flag) and `get_system_info` and `get_disk_space` (with it) (regenerated, difference read). `ctest` excluding only the unrelated `bld_mutate_tool_script_tests` (run in full at the end of the batch): 959 of 959 pass. NOT run on the real sampler (TASK-MCP-037).
- **Assumptions**: the operating system is given as "major.minor" (the sub-version is always 0 per the spec's table, so it is not shown); a model code that is neither 0 nor 1 is reported as not recognised rather than guessed; the memory figures are what §02/&30, &31, &33 and &34 answer; a megabyte is 1024 x 1024 bytes, as the samplers count memory; the free space is read from the disk the sampler has as current (the selection by SysEx).

### TASK-MCP-035: Audition: `audition_sample` and `audition_file`
- **Tier**: M
- **Status**: Done
- **Description**: Add the gateway calls and the two tools with an `action` of start or stop (`audition_file` only with `--allow-disk`, and takes a file name for start).
- **Requirement refs**: RQ-MCP-041
- **ADR refs**: ADR-MCP-004 (DEC-MCP-024)
- **Acceptance Criteria** (Gherkin): *Given* a current sample, *When* `audition_sample` is called with start then stop, *Then* the sampler is sent the audition start then the stop; `audition_file` with start needs a name the folder lists.
- **Dependencies**: TASK-MCP-028
- **Assignee**: AI
- **Verification**: tests first, **run red before the code** (`AuditionToolsTests.cpp`, 5 cases: "test cases: 5 | 0 passed | 5 failed"), then green: `xs56k_mcp_tests "[audition]"` → "All tests passed (104 assertions in 5 test cases)". They cover: `audition_sample` an edit (not read-only, not destructive), `audition_file` absent without the flag (-32602) and listed with it; with KICK current the start is sent once and the answer says it plays "until it is stopped", then the stop is sent once; no current sample, an action that is not "start" or "stop" (a word, empty, a number, null, none) and an extra argument send nothing; `audition_file` start of "kick.wav" sends the start with the file's position in the listing (1) and the stop is sent once; a file that is not there (listing the files), no name, a non-string name, a bad action and no disk send nothing. The conversations' tool lists gained exactly `audition_sample` (without the flag) and `audition_sample` and `audition_file` (with it) (regenerated, difference read). `ctest` excluding only the unrelated `bld_mutate_tool_script_tests` (run in full at the end of the batch): 964 of 964 pass. NOT run on the real sampler (TASK-MCP-037; the disk audition ran on it on 2026-10-03 through the AKM probe).
- **Assumptions**: an audition plays until stopped, and the tool says so in its answer and description; stopping needs no current sample or file (the stop is sent whenever asked); the file is found by its whole name without regard to case, spaces or hyphens and started by its position in the listing; a file that is not a sample is left to the sampler to answer.

### TASK-MCP-036: The README as a reference, the simulated conversation and the documents
- **Tier**: L
- **Status**: Done
- **Description**: Rewrite `juce/mcp/README.md` as the human reference of ADR-MCP-004 DEC-MCP-026 (what it is and needs, start, options table, which tool needs which option, the tools by domain with arguments and confirmations, the safety rules, what has been tried on a real sampler, troubleshooting, developers' notes last); add a `ctest` case that checks that every tool the server lists and every option of the usage text is in it; extend the scripted conversations with the new tools; update `AGENTS.md`, `CHANGELOG.md` and the checkpoint.
- **Requirement refs**: RQ-MCP-043, RQ-MCP-044
- **ADR refs**: ADR-MCP-004 (DEC-MCP-026)
- **Acceptance Criteria** (Gherkin): *Given* the README, *When* its tool tables and the server's tool list are compared, *Then* each tool of the list (all options on) is in a table with its needed option, and each option of the usage text is in the options table; the scripted conversations match.
- **Dependencies**: TASK-MCP-029 to TASK-MCP-035
- **Assignee**: AI
- **Verification**: the check written first and **run red before the README was touched**: `ctest -R mcp_readme_names_every_tool_and_option` → failed, "The README leaves out tools: set_zone_sample, get_zone_samples, ... delete_folder (21) options: --in, --out, --list-ports, --help"; then green after the rewrite: "Test #961: mcp_readme_names_every_tool_and_option ... Passed". The check (`CheckReadmeCoversTools.cmake`) reads the tool names from the simulated server launched with `--allow-disk --allow-disk-refresh` (a `tools/list` request in `conversations/list_tools.jsonl`, 48 tools) and the options from the shipped server's `--help`, and requires each between backticks in the README. The README is organised as DEC-MCP-026 says: what it is and needs, how to start, the table of every option, which tools need which option (32 without an option, 48 with `--allow-disk`), the tools by domain with arguments and the exact `confirm`, the safety rules, what was tried on a real sampler (a two-column table), what to do when something goes wrong, and the developers' notes last. The two scripted conversations gained 18 and 9 requests (the new memory tools; the new disk tools): 114 and 45 answers, each new answer read one by one (successes and refusals of a wrong `confirm`), expected output regenerated. `CHANGELOG.md`, `AGENTS.md` (a pointer), ADR-MCP-003, FTR-MCP-003 and ADR-MCP-004 (the tool counts: 32 and 48) were updated. Full `ctest` (Debug, MSVC `/W4 /WX`), the complete suite including the build-tool test: 966 of 966 pass (916 at TASK-MCP-029's start of the batch plus the new cases, as counted per task).
- **Assumptions**: the README is in English like the other documents of the repository; the table "what has been tried on a real sampler" lists the tools of ADR-MCP-004 as not tried and must be updated by TASK-MCP-037; `get_part_programs` was added during TASK-MCP-032 (ADR-MCP-004 DEC-MCP-024 and FTR-MCP-004 RQ-MCP-038 say so), hence 15 new memory tools and 6 new disk tools (21), not the 20 first counted.

### TASK-MCP-037: Real-sampler run of the new tools with the owner
- **Tier**: M
- **Status**: In Progress
- **Description**: With the owner present, run each new tool on the real S5000 on objects made for the run (a program created for it, samples loaded from the disk, a multi created for it, the `MCPTEST*` folders and a test file), one at a time, write up what the sampler did in `OBSERVATIONS-RQ-MCP-012-real-sampler.md`, list what is left on the disk, and correct the simulated sampler for what it did differently. It includes the owner's check of the sampler's screen with each value of `--screen` (RQ-MCP-045): what is shown when the person selects another program than the assistant's, with Sync LCD off and with it on.
- **Requirement refs**: RQ-MCP-044
- **ADR refs**: ADR-MCP-004
- **Acceptance Criteria** (Gherkin): *Given* the owner present, *When* each new tool is run alone, *Then* its answer and what the sampler did are in the observations file, the objects created are removed or listed, and the simulated sampler matches what was seen.
- **Dependencies**: TASK-MCP-036 (and the owner)
- **Assignee**: Human and AI
- **Verification**: PARTLY DONE on 2026-10-06, the owner present: every new tool was run on the real S5000 on objects made for the run, one at a time, and written up in `OBSERVATIONS-RQ-MCP-012-real-sampler.md` ("The tools of ADR-MCP-004"): `get_system_info`, `add_keygroups`, `delete_keygroup` (the numbers after a deleted keygroup move down), `set_zone_sample`, `get_zone_samples`, `rename_sample`, `delete_sample`, `audition_sample`, `create_multi`, `rename_multi`, `delete_multi`, `set_part_program`, `get_part_programs`, `clear_part`, `set_multi_program_number`, the 12 part parameters of a real multi, `get_disk_space` (the sampler answers 0 bytes free for this FAT32 disk), `audition_file`, `rename_file` of a `.WAV` (the extension is appended), `rename_folder`, `delete_file`, `delete_folder` with and without `delete_contents`, `save_memory_item` with `save_children` and of a multi (`.AKM` confirmed), and the control of `load_file` without and with `with_dependents`. Everything created was deleted again and the disk is back to its original content. Still to do: the owner's check of the screen with each `--screen` mode, and the correction of the simulated sampler (TASK-MCP-041).
- **Assumptions**: Never run on the owner's own files; no tool of this plan is run on the refresh of the disk list; a deletion is run only on the test file and folders made for the run.

### TASK-MCP-038: The option table of the README and the `--help` text say what each option does when given and when left out
- **Tier**: S
- **Status**: Done
- **Description**: The row of `--no-lcd` read "off", a double negative that said nothing about what the server does by default; the table of options now has two columns (what happens if the option is left out, what happens if it is given), a paragraph explains the two sampler settings behind `--no-lcd` (Sync LCD and Auto screen update), and the `--help` text says what the server does without the option.
- **Requirement refs**: RQ-MCP-043
- **ADR refs**: ADR-MCP-004 (DEC-MCP-026)
- **Acceptance Criteria** (Gherkin): *Given* the README, *When* the row of `--no-lcd` is read, *Then* it says what the server does to Sync LCD and Auto screen update when the option is left out and when it is given.
- **Dependencies**: TASK-MCP-036
- **Assignee**: AI (at the owner's remark of 2026-10-06)
- **Verification**: N/A (Tier S). Checked anyway: `xs56k_mcp_tests "[options]"` → "All tests passed (115 assertions in 18 test cases)" and `ctest -R "mcp_readme|mcp_server_executable"` → 4 of 4 pass, `mcp_readme_names_every_tool_and_option` included.
- **Assumptions**: the meaning of the two settings is taken from the SysEx specification (§00 items &03 and &05, footnote a of &03) and from the code (`SamplerGateway.cpp` sets `autoScreenUpdate` On and leaves `syncLcd` at its default Off unless `--no-lcd`); the S5000's pages not following the edits with Auto screen update off was observed on 2026-10-04 (`OBSERVATIONS-RQ-AKM-080-midi-config.md`).

### TASK-MCP-039: `--screen independent | follow | as-is` replaces `--no-lcd`
- **Tier**: L
- **Status**: Done
- **Description**: Replace the launch argument `--no-lcd` by `--screen` with three values: `independent` (the default, what the server did without `--no-lcd`), `follow` (Sync LCD and Auto screen update both on) and `as-is` (neither touched); the gateway's configuration carries the mode; `--no-lcd` is refused with a message that names `--screen as-is`; the README, the `--help` text and the documents that said "put back as they were" say that the close puts the settings back to the sampler's documented defaults.
- **Requirement refs**: RQ-MCP-045, RQ-MCP-002, RQ-MCP-003, RQ-MCP-043
- **ADR refs**: ADR-MCP-004 (DEC-MCP-027); ADR-MCP-001 (DEC-MCP-004, DEC-MCP-008)
- **Acceptance Criteria** (Gherkin): *Given* no `--screen`, *When* a session is opened, *Then* the sampler is sent Sync LCD off and Auto screen update on; with `follow` both on; with `as-is` neither; *When* the session is closed, *Then* the settings it changed are back to their documented defaults; *Given* an unknown value or `--no-lcd`, *When* parsed, *Then* the result is a usage error that names the three values or `--screen as-is`.
- **Dependencies**: TASK-MCP-036
- **Assignee**: AI (at the owner's request of 2026-10-06)
- **Verification**: tests first, **red before the code** (the build of `xs56k_mcp_tests` failed: "'screen': is not a member of 'mcp::GatewayConfig'", "'ScreenMode': the symbol to the left of a '::' must be a type"), then green. `xs56k_mcp_tests "[options]"` → "All tests passed (168 assertions in 21 test cases)" and `"[gateway]"` → "All tests passed (470 assertions in 33 test cases)". They cover: no `--screen` gives `independent`; the three values, in the spaced and the `--screen=value` forms, reach the gateway's configuration; `sideways`, empty, `Follow`, `asis`, `off`, `1` and a missing value are usage errors naming `--screen`, `independent`, `follow` and `as-is`; `--no-lcd` is refused and the message names `--screen as-is`; the usage text names `--screen`, not `--no-lcd`, gives the three values, the default and what the close puts back; with `independent` the sampler is sent Sync LCD 0 and Auto screen update 1 (once each), with `follow` both 1, with `as-is` nothing, and after the close of `independent` and `follow` Sync LCD is on and Auto screen update off, after the close of `as-is` still nothing was sent. `ctest` excluding only the unrelated `bld_mutate_tool_script_tests` (run in full at the end of the batch): 970 of 970 pass. The README (the option table with two columns and a table of the three modes), the CHANGELOG, FTR-MCP-001 (RQ-MCP-002), ADR-MCP-001 (DEC-MCP-004, DEC-MCP-008) and PLAN-MCP-001 were made consistent: `--no-lcd` no longer appears in a current requirement, decision or document of the MCP server (the AKM probe has its own `--no-lcd`, unrelated). **A discrepancy was caught while reading the code for this task**: the documents and my own explanations said the close "puts the settings back as they were"; the session puts each setting it changed back to its documented default (`samplerDefault`: Sync LCD on, Auto screen update off), since section 00 has no Get. The existing test "the section 00 settings are back to their defaults" already said so. NOT run on a real sampler: what the S5000's screen shows in each mode is part of TASK-MCP-037.
- **Assumptions**: `independent` stays the default (it is what the server did without `--no-lcd`); `follow` sets Sync LCD on and Auto screen update on, as the owner's description of "follow" implies, and its effect on the S5000's screen is deduced from the specification (§00 `&03` footnote a), not observed; `--no-lcd` is removed outright, not kept as an alias, since the only configuration known (the owner's `.mcp.json`) does not use it.

### TASK-MCP-040: `get_disk_space` says when the sampler reports 0 bytes free
- **Tier**: M
- **Status**: Done
- **Description**: On the owner's S5000 the free space of the SCSI2SD disk (FAT32) is reported as 0 bytes while files can be written to it (`OBSERVATIONS-RQ-MCP-012-real-sampler.md`): the tool must not present that as a fact. When the sampler answers 0, the answer says that the sampler reports 0 bytes, that it probably does not report the free space of this kind of disk, and that the figure must not be relied on.
- **Requirement refs**: RQ-MCP-040
- **ADR refs**: ADR-MCP-004 (DEC-MCP-024)
- **Acceptance Criteria** (Gherkin): *Given* a disk for which the sampler reports 0 bytes free, *When* `get_disk_space` is called, *Then* the answer says the sampler reports 0 bytes, that the figure is probably not reported for this kind of disk, and gives no size in MB; *Given* a disk with free space, *Then* the answer is unchanged.
- **Dependencies**: TASK-MCP-034
- **Assignee**: AI
- **Verification**: test first, **run red** (`SystemInfoToolsTests.cpp`: "test cases: 6 | 5 passed | 1 failed", the new case failing), then green: `xs56k_mcp_tests "[sysinfo]"` → "All tests passed (60 assertions in 6 test cases)". The new case gives a disk for which the sampler reports 0 bytes and checks the answer ("reports 0 bytes free", the disk's name, "probably does not report", no size in MB); the existing case (1000000 bytes free, "about 1.0 MB") is unchanged. `ctest` excluding only the unrelated `bld_mutate_tool_script_tests`: 971 of 971 pass. The README table and the observations already say so; checked on the real sampler only by the observation that gave rise to it.
- **Assumptions**: a reported 0 is treated as "not reported" for any disk (a real full disk would be reported as 0 too, but the tool then tells the person not to rely on it, which is the safe wording).

### TASK-MCP-041: The simulated sampler matches what the real S5000 did
- **Tier**: M
- **Status**: Not Started
- **Description**: Correct the simulated sampler, each with its test, for what the real runs showed and it did differently: the error with no disk selected (257, "selected disk is invalid", against 4), the current path below the root written with a backslash (`AKWF\AKWF_oboe` against `AKWF/AKWF_oboe`), the size of a saved program (516 bytes, observed for a program of one keygroup), of a saved sample (44 bytes of header plus 2 bytes per sample point and channel: 1376 bytes for a 666-point mono sample) and of a saved multi (2354 bytes for a multi of 32 parts), and the memory after the deletion of a sample (none is selected); check against the observations the other behaviours the tools rely on (the keygroups after a deleted one move down, a sample keeps its place when renamed, a new multi has 32 parts) and add a test for each that has none.
- **Requirement refs**: RQ-MCP-044, RQ-MCP-030
- **ADR refs**: ADR-MCP-003 (DEC-MCP-022); ADR-MCP-004
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler, *When* each of the behaviours above is exercised, *Then* it answers as the real sampler did (the observations file is the source), and no earlier test is changed except where its expectation was the old, wrong behaviour.
- **Dependencies**: TASK-MCP-037
- **Assignee**: AI
- **Verification**: (to be filled at closure)
- **Assumptions**: None yet.