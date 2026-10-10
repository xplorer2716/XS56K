# PLAN-MCP-006: The Generated Table of SysEx Messages, AKM Functions and MCP Tools

## Overview

Implements `FTR-MCP-006` as `ADR-MCP-006` decides: a script in `juce/tools` that writes `documents/SYSEX-COVERAGE.md` (one table per section of the
specification, one row per message, with the AKM function and the MCP tool of each), a data file for what a scan cannot read, a check that
fails when the file is behind the code, and the pointers from `README.md`, `documents/INDEX.md` and `AGENTS.md`. The owner asked for it on
2026-10-10, and asked that the tool live in `juce/tools` with the others. The script reads the sources and changes none; the one change to C++
is an option of the simulated server, test tooling, for the check of TASK-MCP-075 (`DEC-MCP-040`).

**Order of the work.** The rows first (they need nothing), then each column on its own because each has a different source and a different way to
be wrong, then the declared file and the real result, then the pointers and the `ctest` entries, then the check against the simulated sampler. Each
column task **measures how many of the 384 messages its scan attributes by itself** and writes the figure in its `Verification`. The measure of the
MCP column (2026-10-10, before TASK-MCP-071 was built) said that a text scan of `Tools.cpp` does not hold; the owner then chose the declared
tool-to-method list with a check against the simulated sampler, and `DEC-MCP-037` was rewritten and `DEC-MCP-040` added.

| Step | Task | What | Why here |
|---|---|---|---|
| 1 | TASK-MCP-069 | the script's skeleton: the rows of the 13 sections and Table 32, the Markdown, `--check`, determinism | nothing to scan yet; fixes the shape |
| 2 | TASK-MCP-070 | the AKM column | the simpler scan (one layer) |
| 3 | TASK-MCP-071 | the MCP column: the declared list of tools and gateway methods, the scan of the gateway and of the parameters, the checks both ways | the long chain |
| 4 | TASK-MCP-072 | the rest of the declared data file: the exclusions, what the generic functions apply to, what the scans leave | after both scans say what they leave |
| 5 | TASK-MCP-073 | the counts, the real `documents/SYSEX-COVERAGE.md`, the `ctest` entries, the pointers | the result and its guard |
| 6 | TASK-MCP-075 | the check against the simulated sampler: `--trace-messages`, a third conversation, `--check-tools` | the ground truth for the links that are declared |
| 7 | TASK-MCP-076 | document the 27 messages with no tool: covered by other messages, or a gap, in a note column | the owner asked for it on reading the table |
| 8 | TASK-MCP-074 | the owner decides what to do with the gaps | his to judge |

The plan has 8 tasks (TASK-MCP-075 and TASK-MCP-076 were added on 2026-10-10), under the limit of 10 per session.

## References
- **Requirements**: RQ-MCP-058, RQ-MCP-059, RQ-MCP-060, RQ-MCP-061, RQ-MCP-062, RQ-MCP-063, RQ-MCP-064, RQ-MCP-065, RQ-MCP-066, RQ-MCP-067 (`FTR-MCP-006`)
- **ADRs**: ADR-MCP-006 (Proposed): DEC-MCP-035 to DEC-MCP-040; ADR-AKM-001: DEC-AKM-003, DEC-AKM-012 (the item catalogue the rows come from);
  ADR-MCP-005: DEC-MCP-028 (what the owner excluded)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-MCP-069: The generator's skeleton — rows, Markdown, `--check`
- **Tier**: M
- **Status**: Done
- **Description**: `juce/tools/generate_sysex_coverage.py` reads `items.json` and the spec's item list, checks each section's item count against the
  TSV's command rows, and writes `documents/SYSEX-COVERAGE.md` with one table per section (message code, name, Set or Get) and the two
  other columns still `none`; `--check` compares the committed file with what would be written. The unit tests are written first.
- **Requirement refs**: RQ-MCP-058, RQ-MCP-062, RQ-MCP-063, RQ-MCP-065
- **ADR refs**: ADR-MCP-006 (DEC-MCP-035, DEC-MCP-039)
- **Acceptance Criteria** (Gherkin): *Given* the catalogue of 384 items, *When* the script runs, *Then* the file has 13 section tables and 384
  message rows, in the order of Table 4; *given* the script run twice, *then* the two files are the same bytes; *given* the file edited, *then*
  `--check` exits non-zero and names the section; *given* a catalogue item removed, *then* the script fails on the count against the TSV.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Test first: `juce/tests/tools/test_generate_sysex_coverage.py` written before the script, run with the script moved aside: 15 results, 7 failures and 8 errors (the 6 cell tests count as one error, their class not loading). After the script: `python -m unittest discover -s juce/tests/tools -p "test_generate_sysex_coverage.py"` 22 tests OK (21 first, plus the one added when the real output showed that `&2A` is both an item of section 06 and a section of Table 32, so Table 32's column is headed "Section"). Real run to a scratch file: "Wrote 384 messages", the second run with `--check` "is up to date", 392 rows starting with "| `&" (384 + the 8 sections of Table 32) under 14 headings (the 13 sections in Table 4's order, then Table 32); the file has LF ends. The catalogue's item codes equal the spec's command rows section by section (the script stops otherwise; tried by removing and adding an item in the tests). The file was written to the scratchpad, not to `documents/`: it is committed by TASK-MCP-073.
- **Assumptions**: (1) The item code is written with the `&`, in upper-case hex, then the decimal in braces as the spec does (`&0A{10}`), followed by the catalogue's name and `(Set)` or `(Get)`. (2) The declared data file `juce/tools/sysex_coverage_declared.json` is created here with the eight sections of Table 32 only, because the Table 32 rows need them; TASK-MCP-072 adds the exclusions and the links the scans cannot read. (3) With no scan yet, every catalogued message reads `catalogue only` and `none`: the cell functions already give the three states of each column and are tested with given lists. (4) The script is not yet a `ctest` entry and the file is not linked: TASK-MCP-073.

### TASK-MCP-070: The AKM column
- **Tier**: M
- **Status**: Done
- **Description**: The script reads `juce/akm/src/*.cpp` and `juce/akm/include/akm/*.hpp`, attributes each `ItemId::<Id>` to the public function that
  sends it (following the helpers of the translation unit one step), and fills the AKM column: the function names, `catalogue only`, or `none`.
  It lists what it cannot read and exits non-zero. It records how many of the 384 messages it attributes.
- **Requirement refs**: RQ-MCP-059, RQ-MCP-063, RQ-MCP-065
- **ADR refs**: ADR-MCP-006 (DEC-MCP-036)
- **Acceptance Criteria** (Gherkin): *Given* a fixture source in which `getAllProgramNames` sends item `ProgramGetAllNames` through a helper, *When*
  the scan runs, *Then* the message's cell is `getAllProgramNames`; *given* a catalogued item no function names, *then* `catalogue only`;
  *given* a public function whose body the scan cannot read, *then* the script names it and fails; *given* the real sources, *then* every one of
  the 158 item names found by the text search of 2026-10-10 in `juce/akm/src` has a function.
- **Dependencies**: TASK-MCP-069
- **Assignee**: AI
- **Verification**: Test first: 15 tests of the scan added to `juce/tests/tools/test_generate_sysex_coverage.py`, run before the scanner existed: 37 tests, 4 failures and 11 errors. After the scanner, and after the first run on the real sources showed what it missed (see Assumptions), `python -m unittest discover -s juce/tests/tools -p "test_generate_sysex_coverage.py"`: 40 tests OK. Real run (`python juce/tools/generate_sysex_coverage.py --output <scratch>`, then `--check` "is up to date"): **158 of the 384 messages are sent by a named AKM function, which is the 158 the text search of 2026-10-10 found in `juce/akm/src`**; 226 are `catalogue only`; no item is unattributed and none is missing from the catalogue; the functions that send the item their caller names are `getForAllKeygroups`, `getForAllZones` and `getForAllZonesAllKeygroups`. Eight messages have more than one function (`ejectDisk` and `ejectDiskDiscardingVirtualSamples`; `Session::open`, `Session::close` and the setters for the §00 settings).
- **Assumptions**: (1) "Public" is "defined at namespace scope with a name that a header under `juce/akm/include/akm` writes before a `(`" (`detail/` excluded): a superset of the declared functions, which only matters for a helper that shares a name with a declared function, none today. (2) The first run found two things the tests then pinned: seven items (`DiskLoadFile`, `MultiSetPartByName`, `MultiSetProgramNumber`, `ProgramCreateWithKeygroups`, `ProgramSetNumber`, `SetListRename`, `ZoneSetSample`) are fetched with `descriptor(...)` and sent with `makeRequest(item, ...)`, so a descriptor counts as sending when the body also builds a request; and the session's own class (`Impl` in `Session.cpp`) gave its seven items to every public method of `Session`, so the methods of a local class are units of their own (DEC-MCP-036 amended). (3) The three generic functions are reported on standard output and not yet in the file: which messages they apply to is what TASK-MCP-072 declares. (4) `ItemId::X` named in a source and absent from the catalogue stops the script (it cannot happen in code that compiles, and is tested with a fixture). (5) *Amended 2026-10-10, at the owner's request:* the cell of a message that no named function sends names the generic builder that makes it (`makeRequest`, or `makeStringRequest` or `makeTwoStringRequest` by the formats of the item's arguments) instead of saying `catalogue only`, which named nothing; the 226 messages are the same, all 226 take numbers only so all say `makeRequest`; 44 tests pass (the 4 new ones are on the three builders and on a message a function sends), and `--check` on the real run "is up to date".

### TASK-MCP-071: The MCP column — the declared tool list, the scan of the gateway and of the parameters
- **Tier**: L
- **Status**: Done
- **Description**: The script reads the tools and their launch options from `juce/mcp/README.md`; the declared list of `sysex_coverage_declared.json`
  (for each of the 77 tools, the gateway methods it calls, or for a family of kinds the AKM functions, or `none` with a reason); for each gateway
  method, in `SamplerGateway*.cpp`, the AKM functions and items it reaches through its helpers, and the ones it reaches through other public
  methods apart; and the parameter pairs of `StandardParameters.cpp` by scope. It fills the MCP column (tool names with their option, or `none`),
  and stops, naming the difference, when a tool and its entry, or a method and `Tools.cpp`, do not match. The 77 entries are written here from a
  reading of `Tools.cpp`, `SamplerGateway.hpp` and `SamplerGatewayLists.cpp`.
- **Requirement refs**: RQ-MCP-060, RQ-MCP-063, RQ-MCP-065
- **ADR refs**: ADR-MCP-006 (DEC-MCP-037, DEC-MCP-038)
- **Acceptance Criteria** (Gherkin): *Given* a fixture in which the declared entry of `rename_set_list` is the gateway method `renameSetList`, which
  calls `akm::renameSetList`, which sends `SetListRename`, *When* the scan runs, *Then* the message's MCP cell is `rename_set_list`; *given* a tool
  that needs `--allow-disk`, *then* its name is followed by `(--allow-disk)`; *given* a tool of the README with no declared entry, or an entry with
  no tool, *then* the script fails and names it; *given* a declared method that does not exist, or a gateway method that `Tools.cpp` calls and no
  entry declares, or a declared method that `Tools.cpp` never calls, *then* it fails and names it; *given* the real sources and the real entries,
  *then* the 77 tools are each attributed or declared as sending nothing.
- **Dependencies**: TASK-MCP-070
- **Assignee**: AI
- **Verification**: Test first: 19 tests of the MCP scan (the README's tools and options, the gateway's public methods, the calls of `Tools.cpp`, the shown and allowed messages of a tool, the parameters by section, the entries that name AKM functions, the six failures of the checks both ways) and 3 tests of the real file added to `test_generate_sysex_coverage.py`, run before the code existed: 63 tests, 2 failures and 17 errors. After the code, a first real run found the one entry the draft had missed (`readFxModule`, called by a helper shared by `get_fx_parameter` and `set_fx_parameter`), and the real table showed that `press_key` sent nothing (its AKM function `pressKey` is `holdKey` and `releaseKey`), so three more tests were written first (composed AKM functions, a type of the library is not a call, `Key Hold` lists `press_key`): 65 tests, `python -m unittest discover -s juce/tests/tools -p "test_generate_sysex_coverage.py"` OK. Real run to a scratch file: "Wrote 384 messages", **346 of the 384 messages are sent by one of the 77 tools**, `--check` "is up to date". The 77 declared entries are in `sysex_coverage_declared.json` (`mcp_tools`): 6 parameter tools by section, 14 tools whose shared gateway method is narrowed by the AKM functions named (the song files, set lists and scenelists, the three delete-all tools), `list_parameters` sends nothing, the others one to four gateway methods. The rows of the two tools that share code have their own message each (`rename_file` has `Rename File`, `rename_folder` `Rename Folder`). **Not proved by this task:** that tool A and not tool B calls a method, for the shared code; TASK-MCP-075 checks it against the simulated sampler.
- **Assumptions**: (1) The entries were drafted from a reading of `definition(...)` and its lambda in `Tools.cpp` (58 tools), the 17 that no such reading reached were written by hand from `Tools.cpp`, `SamplerGateway.hpp` and `SamplerGatewayLists.cpp`, and the script then found the one missing method by its both-way check. (2) The messages of the parameter tools are all the items `StandardParameters.cpp` names, by section (`0A 08 06` for the program, keygroup and zone, `0E` for the sample, `0C` for the multi parts) and by kind (set or get). (3) An AKM function that is made of two others (`pressKey`) gives its tool the messages of both (the AKM column still names `holdKey` and `releaseKey` alone, as the implementers); a gateway method that calls another public gateway method does not (those messages are only allowed, for TASK-MCP-075). (4) The 38 messages with no tool are listed by the real run (sections `00` sent by the session, memory sizes, the program number, sample and disk reads, `Eject Disk`, the user tune template, and others): TASK-MCP-072 gives each its status where one exists (the session's messages as sent by the server itself, `Eject Disk` as not offered, `DEC-MCP-028`), and TASK-MCP-074 shows the rest to the owner.

### TASK-MCP-072: The declared data file
- **Tier**: M
- **Status**: Done
- **Description**: `juce/tools/sysex_coverage_declared.json` (created by TASK-MCP-069 with the eight Table 32 sections, and given the tool list by
  TASK-MCP-071) also holds the messages decided not to be offered with the ID of the decision (ejecting a disk, `DEC-MCP-028`, and whatever else the
  scans show that an ADR excluded), the messages the server sends itself and not through a tool (the seven of section 00, which the session sends at its opening and closing, and which `--screen` chooses), which messages `getForAllKeygroups`, `getForAllZones` and `getForAllZonesAllKeygroups` apply to, and the links the scans
  could not attribute, each with its reason. The script refuses a declared entry naming an unknown item, function or tool, and one that a scan now
  covers.
- **Requirement refs**: RQ-MCP-058, RQ-MCP-060, RQ-MCP-063, RQ-MCP-065
- **ADR refs**: ADR-MCP-006 (DEC-MCP-038), ADR-MCP-005 (DEC-MCP-028)
- **Acceptance Criteria** (Gherkin): *Given* a declared entry for an item that does not exist, *When* the script runs, *Then* it fails and names the entry;
  *given* an entry for a message a scan attributes by itself, *then* it fails as stale; *given* the real file, *then* no entry is stale and every
  `not offered` cites a decision that exists in `process/2.architecture`.
- **Dependencies**: TASK-MCP-071
- **Assignee**: AI
- **Verification**: Test first: 17 tests of the declared facts (the four kinds of entries, each with its stale and unknown cases, and five on the real file) added to `test_generate_sysex_coverage.py`, run before the code existed: 82 tests, 4 failures and 13 errors. After the code, and after the first run showed that the fixtures of the AKM tests met the real declarations (they now carry a declared file with Table 32 only): `python -m unittest discover -s juce/tests/tools -p "test_generate_sysex_coverage.py"` 82 tests OK. Real run to a scratch file, then `--check` "is up to date": 346 of the 384 messages have a tool, 1 is `not offered (DEC-MCP-028)` (`Eject Disk`, the only message an ADR excludes: read in DEC-MCP-025 and DEC-MCP-028), 6 are `server session` (the six of section 00 that a `Session::` function sends; `Echo` is not one of them and stays `none`), the refresh of the disk list reads `list_disks (--allow-disk and --allow-disk-refresh)`, the Get items of section 08 that the parameters name read `getForAllKeygroups` and those of section 06 `getForAllZones, getForAllZonesAllKeygroups`; **31 messages remain `none`**: they are the list for TASK-MCP-074.
- **Assumptions**: (1) Which messages an ADR excluded was read in `ADR-MCP-004` (DEC-MCP-025) and `ADR-MCP-005` (DEC-MCP-028): only `Eject Disk` is a message; formatting and file transfer are outside the specification, and the refresh of the disk list is offered. (2) The `server session` cell is a fourth state the plan did not foresee, added to RQ-MCP-060 and DEC-MCP-038 after the first run (a gap and the server's own business must not look alike). (3) A generic function applies to the Get items of its sections that the standard parameters name, so an item of those sections no parameter names keeps the generic builder.

### TASK-MCP-073: The counts, the real file, the `ctest` entries and the pointers
- **Tier**: M
- **Status**: Done
- **Description**: The script writes the table of counts that opens the file (RQ-MCP-061); the real `documents/SYSEX-COVERAGE.md` is generated and
  committed; `juce/tests/CMakeLists.txt` gets `sysex_coverage_table_is_up_to_date` and `sysex_coverage_script_tests` beside the item catalogue's;
  the root `README.md` links the file, `documents/INDEX.md` lists it, `AGENTS.md` gets the command, `CHANGELOG.md` records it; the process index is rebuilt.
- **Requirement refs**: RQ-MCP-061, RQ-MCP-062, RQ-MCP-063, RQ-MCP-064, RQ-MCP-065
- **ADR refs**: ADR-MCP-006 (DEC-MCP-039)
- **Acceptance Criteria** (Gherkin): *Given* the generated file, *Then* the counts of its first table equal the counts of its rows; *given* a new tool
  added to the server's README and `Tools.cpp` without a line in the table, *When* `ctest` runs, *Then* `sysex_coverage_table_is_up_to_date` fails
  and names the tool (tried once on a scratch copy, not committed); *given* `ctest`, *then* the whole suite passes on Windows.
- **Dependencies**: TASK-MCP-072
- **Assignee**: AI
- **Verification**: Test first: 4 tests of the counts added to `test_generate_sysex_coverage.py` (the counts open the file before section 00, each section's counts equal its rows read back, the total is the catalogue's 384 and the sum of the sections, the four states of the MCP column and the two of the AKM column add up to the messages), run before the code: 86 tests, 4 errors. After the code: 86 tests OK. `python juce/tools/generate_sysex_coverage.py` wrote the real `documents/SYSEX-COVERAGE.md` (384 messages; 210 with a named AKM function, 174 built by a generic builder; 346 with a tool, 1 not offered, 6 server session, 31 none) and `--check` says "is up to date". `cmake -S juce -B juce/build-run` registered the two entries and `ctest -R sysex_coverage` passed both (`sysex_coverage_table_is_up_to_date`, `sysex_coverage_script_tests`). The failing case tried on a scratch copy of `juce/mcp` (not committed): a row `ghost_tool` added to its README made `--check` exit 1 with "the tool ghost_tool is in the README and has no declared entry"; renaming `hold_key` there also gave "SamplerGateway::holdPanelKey is called in Tools.cpp and no tool declares it". The links: `README.md` (the library section, the server section, "More documentation"), `documents/INDEX.md`, `AGENTS.md` (one bullet with the command) and `CHANGELOG.md`, read back. Full `ctest` on Windows (MSVC, Debug): 1102 of 1102 pass.
- **Assumptions**: (1) The two entries are registered beside the item catalogue's, inside the same `if(Python3_FOUND)`; the message printed when Python is missing now names the coverage table too. (2) The counts add `Server session` as a column, which RQ-MCP-061 did not list, since RQ-MCP-060 gained that state in TASK-MCP-072. (3) The root `README.md` still says, in its safety paragraph of the server, that "the server never clears the whole memory, formats or ejects a disk": that has been untrue since TASK-MCP-051 and is not changed here (outside this task); it is left for the owner to see.

### TASK-MCP-075: The check against the simulated sampler
- **Tier**: L
- **Status**: Done
- **Description**: `xs56k_mcp_server_simulated` gets an option `--trace-messages <file>` that writes, for each `tools/call`, the tool and the section and
  item codes of the commands the simulated sampler accepted (the session is opened by one untraced call first); a third conversation with no
  expected output calls the tools the two scripted conversations do not (`delete_all_programs` and `delete_all_samples` at the time of writing);
  `generate_sysex_coverage.py --check-tools <trace>` compares the trace with the table and fails on a message that a tool sent and its row, with what
  its declared methods send through other methods, does not give; it lists the tools no call reached and the messages of a row no call sent. A
  `ctest` entry runs the simulated server on the three conversations and then the check. The tests come first.
- **Requirement refs**: RQ-MCP-066, RQ-MCP-063, RQ-MCP-065
- **ADR refs**: ADR-MCP-006 (DEC-MCP-040)
- **Acceptance Criteria** (Gherkin): *Given* the simulated server run with `--trace-messages` on a conversation that calls `rename_file`, *When* the
  trace is read, *Then* it has a line for that call giving the file-rename item and no other tool's; *given* a declared entry that gives `rename_file`
  the method of `rename_folder`, *then* `--check-tools` fails and names `rename_file` and the message; *given* the real declarations, *then* the
  check passes and lists the tools no conversation calls; *given* `ctest`, *then* the whole suite passes.
- **Dependencies**: TASK-MCP-071, TASK-MCP-073
- **Assignee**: AI
- **Verification**: Test first: 11 tests added to `test_generate_sysex_coverage.py` (the trace read; a message a tool sent against its row, through another method, unknown to the catalogue, for an unknown tool; the tools no call reached and the messages of a row no call sent, listed without failing; the conversations run through a stand-in server, and the error naming a conversation when no trace is written; the check on the real server, skipped without the server), run before the code: 97 tests, 10 errors, 1 skipped. After the Python code and the C++ option (`--trace-messages` of `xs56k_mcp_server_simulated`, built with `/W4 /WX`, no warning) and `tool_messages.jsonl` (`delete_all_programs`, `delete_all_samples`): **0 of the 77 tools are left uncalled by the three conversations**. The first real run found 50 messages sent by tools and not in their rows, two causes: a Set is read back (the parameter tools send the Get of the same sections: now allowed) and four memory reads of `get_system_info` that the table did not give it because the gateway passes `akm::getFreeWaveMemoryPercent` as a value, not as a call (the scan now takes any `akm::name`, and a destructor keeps its tilde so that `~Session` no longer looks like a function `Session`); the second run: "every message the tools sent is in the table", and the list of rows' unexercised messages showed three tools whose row carried the messages of the three kinds of list (`rename_set_list`, `delete_set_list`, `clear_sampler_memory`), now narrowed by their AKM function in the declared file. The failing case tried on a scratch declaration (not committed): `rename_file` and `rename_folder` given each other's gateway method passed the both-ways check and failed this one with "rename_file sent Rename File (&28 of section 10), which neither its row of the table nor the methods it calls give" and the same for `rename_folder`, exit 1. Final: 99 tests OK, and full `ctest` on Windows (MSVC, Debug) 1103 of 1103 with the new entry `sysex_coverage_tools_match_the_simulated_sampler`; `documents/SYSEX-COVERAGE.md` regenerated and `--check` up to date (350 messages with a tool).
- **Assumptions**: (1) The session is opened by an untraced `get_status` before the first traced call, so the messages of the session (section 00) are not given to a tool; the close after the last line is not traced either. (2) 25 tools are called and have messages in their row that no call sent (the parameters the conversations do not set, the select by index where the conversation selects by name, the opening and closing of a folder, and the like): this is listed by the check on each run and is not a failure, a call taking one path of a tool. (3) What the check proves is one way: a tool does not send a message its row omits; that a message of a row is sent by that tool is proved only for the calls the conversations make. (4) The shipped server is unchanged; the option is in the simulated server, test tooling that is never shipped.

### TASK-MCP-076: Document the messages with no tool: covered by other messages, or a gap
- **Tier**: M
- **Status**: Not Started
- **Description**: The declared file gets a list of notes on messages that no tool sends. A note says `covered` (the tools that get the same
  information or function, and the messages they send instead) or `gap` (what exists of it in AKM or in another tool, and what is missing). The
  tables get a fourth column, `Note`; a `covered` message reads `covered by <tools>` in the MCP column and has its own count; the script refuses a
  note on a message a tool sends, and a `covered` note that names a message the tool does not send. The 27 messages are documented from the spec's
  own descriptions and the messages the tools send (read in the table), with the owner's corrections of 2026-10-10: `Get Current Program Index`,
  `Get the Multi Numbers of All Multis` and `Get the Number of Parts in All the Multis` are gaps, not equivalents. The tests come first.
- **Requirement refs**: RQ-MCP-067, RQ-MCP-058, RQ-MCP-060, RQ-MCP-061, RQ-MCP-063, RQ-MCP-065
- **ADR refs**: ADR-MCP-006 (DEC-MCP-038)
- **Acceptance Criteria** (Gherkin): *Given* `Get the number of disks connected` with a note that `list_disks` sends `Get list of all connected disks` instead,
  *When* the file is generated, *Then* its MCP cell says `covered by list_disks (--allow-disk)` and its note gives the reason; *given* a note naming a
  message the tool does not send, or a note on a message a tool sends, *then* the script fails and names the note; *given* the counts, *then* they
  equal the rows, with the new column; *given* `Set Program Number`, `Get Current Program Index`, `Get the Multi Numbers of All Multis`, *then* each
  stays `none` and its note says what exists and what is missing.
- **Dependencies**: TASK-MCP-075
- **Assignee**: AI
- **Verification**: Pending
- **Assumptions**: (1) The wording of each note is taken from the spec's own description of the message and from the messages the covering tools send
  in the generated table, not from memory. (2) Table 32's table keeps its three columns: its rows are sections, not messages.

### TASK-MCP-074: Read the table with the owner
- **Tier**: S
- **Status**: In Progress
- **Description**: Present the generated table's counts and its `none` and generic-builder rows to the owner; record what he decides to implement,
  to exclude (a declared `not offered`, with his word as the decision) or to leave, as a list of follow-up tasks or amendments; nothing is
  implemented here.
- **Requirement refs**: RQ-MCP-058, RQ-MCP-060
- **ADR refs**: ADR-MCP-006
- **Acceptance Criteria** (Gherkin): *Given* the generated table, *When* it is read with the owner, *Then* each `none` row is either planned, declared
  `not offered` with his decision, or left open and listed.
- **Dependencies**: TASK-MCP-073, TASK-MCP-075, TASK-MCP-076
- **Assignee**: Human and AI
- **Verification**: Presented to the owner on 2026-10-10, waiting for his decisions. The table has 384 messages: 350 with an MCP tool, 1 `not offered` (Eject Disk, DEC-MCP-028), 6 `server session`, **27 with none**, and the 8 sections of Table 32 with no code. The 27, read in the file and grouped: **(a) six that are functions the server lacks**: the program number of a program (`&0A` set, `&11` get, `&18` get all, section 0A: the server has `set_multi_program_number` and nothing for programs), the user tune template (`&33` set, `&3B` get, section 0A), the number of parts of new multis (`&01`, section 0C); **(b) fourteen that are other forms of what a tool already does**: `&02` Create Program (the server uses `&03`, with keygroups), the current keygroup (`&02`, section 08), the current program index (`&12`), the name of a sample by index (`&11`, section 0E), the eight disk-information reads of section 10 (`&03 &04 &06 &07 &0A &11 &21 &24`, which `list_disks` and `list_disk_contents` cover through other messages), the current scenelist name and song file name (`&14`, sections 14 and 16); **(c) six batch reads** that only save messages: the parameters of a part in one message (`&47`), the mute and solo of all parts (`&48`), the multi numbers (`&50`) and part counts (`&52`) of all multis, the sample parameters in one message (`&34`, `&4B` of section 0E); **(d) `Echo`** (`&06`, section 00), a test of the link. The owner's decisions are to be recorded here.
- **Assumptions**: None
