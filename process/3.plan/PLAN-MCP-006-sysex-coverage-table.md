# PLAN-MCP-006: The Generated Table of SysEx Messages, AKM Functions and MCP Tools

## Overview

Implements `FTR-MCP-006` as `ADR-MCP-006` decides: a script in `juce/tools` that writes `documents/SYSEX-COVERAGE.md` (one table per section of the
specification, one row per message, with the AKM function and the MCP tool of each), a data file for what a scan cannot read, a check that
fails when the file is behind the code, and the pointers from `README.md`, `documents/INDEX.md` and `AGENTS.md`. The owner asked for it on
2026-10-10, and asked that the tool live in `juce/tools` with the others. No C++ changes: the plan reads the sources and changes none.

**Order of the work.** The rows first (they need nothing), then each column on its own because each has a different source and a different way to
be wrong, then the declared file and the real result, then the pointers and the `ctest` entries. Each column task **measures how many of the 384
messages its scan attributes by itself** and writes the figure in its `Verification`; if the MCP scan (TASK-MCP-071) cannot attribute most of
them, the work stops there and `DEC-MCP-037` is amended with the owner before more is built (Ask-when-blocking rule, c).

| Step | Task | What | Why here |
|---|---|---|---|
| 1 | TASK-MCP-069 | the script's skeleton: the rows of the 13 sections and Table 32, the Markdown, `--check`, determinism | nothing to scan yet; fixes the shape |
| 2 | TASK-MCP-070 | the AKM column | the simpler scan (one layer) |
| 3 | TASK-MCP-071 | the MCP column | the long chain; the go or no-go on `DEC-MCP-037` |
| 4 | TASK-MCP-072 | the declared data file: Table 32, the exclusions, what the scans leave | after both scans say what they leave |
| 5 | TASK-MCP-073 | the counts, the real `documents/SYSEX-COVERAGE.md`, the `ctest` entries, the pointers | the result and its guard |
| 6 | TASK-MCP-074 | read the table with the owner | what `none` and `catalogue only` hold is his to judge |

The plan has 6 tasks, under the limit of 10 per session.

## References
- **Requirements**: RQ-MCP-058, RQ-MCP-059, RQ-MCP-060, RQ-MCP-061, RQ-MCP-062, RQ-MCP-063, RQ-MCP-064, RQ-MCP-065 (`FTR-MCP-006`)
- **ADRs**: ADR-MCP-006 (Proposed): DEC-MCP-035 to DEC-MCP-039; ADR-AKM-001: DEC-AKM-003, DEC-AKM-012 (the item catalogue the rows come from);
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
- **Assumptions**: (1) "Public" is "defined at namespace scope with a name that a header under `juce/akm/include/akm` writes before a `(`" (`detail/` excluded): a superset of the declared functions, which only matters for a helper that shares a name with a declared function, none today. (2) The first run found two things the tests then pinned: seven items (`DiskLoadFile`, `MultiSetPartByName`, `MultiSetProgramNumber`, `ProgramCreateWithKeygroups`, `ProgramSetNumber`, `SetListRename`, `ZoneSetSample`) are fetched with `descriptor(...)` and sent with `makeRequest(item, ...)`, so a descriptor counts as sending when the body also builds a request; and the session's own class (`Impl` in `Session.cpp`) gave its seven items to every public method of `Session`, so the methods of a local class are units of their own (DEC-MCP-036 amended). (3) The three generic functions are reported on standard output and not yet in the file: which messages they apply to is what TASK-MCP-072 declares. (4) `ItemId::X` named in a source and absent from the catalogue stops the script (it cannot happen in code that compiles, and is tested with a fixture).

### TASK-MCP-071: The MCP column — the go or no-go on DEC-MCP-037
- **Tier**: L
- **Status**: Not Started
- **Description**: The script reads the tool names and the gateway methods they call in `juce/mcp/src/Tools.cpp`, the AKM functions and item IDs each
  gateway method uses in `SamplerGateway*.cpp`, the item pairs of the parameter catalogues (`StandardParameters.cpp` and the others), and the option of
  each tool from `juce/mcp/README.md`; it fills the MCP column (tool names with their option, `not offered (<decision>)` from the declared file, or
  `none`). **First it measures** how many of the 384 messages it attributes without a declaration, and stops with the figure if that is not most of them.
- **Requirement refs**: RQ-MCP-060, RQ-MCP-063, RQ-MCP-065
- **ADR refs**: ADR-MCP-006 (DEC-MCP-037, DEC-MCP-038)
- **Acceptance Criteria** (Gherkin): *Given* a fixture in which the tool `rename_set_list` calls `gateway.renameSetList`, which calls
  `akm::renameSetList`, which sends `SongSetListRename`, *When* the scan runs, *Then* the message's MCP cell is `rename_set_list`; *given* a tool
  that needs `--allow-disk`, *then* its name is followed by `(--allow-disk)`; *given* a tool registered from a table, *then* it is found; *given* a
  tool the scan cannot follow, *then* the script names it and fails; *given* the real sources, *then* the 77 tools of the server are each attributed or
  named as unreadable, and the count is reported.
- **Dependencies**: TASK-MCP-070
- **Assignee**: AI
- **Verification**: Pending (the figure of messages attributed without a declaration, and the list of tools or methods left; the go or no-go).
- **Assumptions**: (1) Tier L because it is the cross-cutting scan and the point where an ADR decision may be amended. (2) If the measure says no-go,
  the owner is asked before the column is built another way (Ask-when-blocking rule c).

### TASK-MCP-072: The declared data file
- **Tier**: M
- **Status**: Not Started
- **Description**: `juce/tools/sysex_coverage_declared.json` (created by TASK-MCP-069 with the eight Table 32 sections) also holds the messages decided
  not to be offered with the ID of the decision (ejecting a disk, `DEC-MCP-028`, and whatever else the two scans show that an ADR excluded), and the links the scans could not
  attribute, each with its reason. The script refuses a declared entry naming an unknown item, function or tool, and one that a scan now covers.
- **Requirement refs**: RQ-MCP-058, RQ-MCP-060, RQ-MCP-063, RQ-MCP-065
- **ADR refs**: ADR-MCP-006 (DEC-MCP-038), ADR-MCP-005 (DEC-MCP-028)
- **Acceptance Criteria** (Gherkin): *Given* a declared entry for an item that does not exist, *When* the script runs, *Then* it fails and names the entry;
  *given* an entry for a message a scan attributes by itself, *then* it fails as stale; *given* the real file, *then* no entry is stale and every
  `not offered` cites a decision that exists in `process/2.architecture`.
- **Dependencies**: TASK-MCP-071
- **Assignee**: AI
- **Verification**: Pending (the real run with the real file; the list of entries and their reasons).
- **Assumptions**: (1) Which messages an ADR excluded is read in `ADR-MCP-004` (DEC-MCP-025) and `ADR-MCP-005` (DEC-MCP-028), not from memory;
  any message the scans leave that no ADR excludes stays `none`, the owner judging it in TASK-MCP-074.

### TASK-MCP-073: The counts, the real file, the `ctest` entries and the pointers
- **Tier**: M
- **Status**: Not Started
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
- **Verification**: Pending (`ctest` output with the two new entries; the scratch trial of the failing case; the links read in the three documents).
- **Assumptions**: (1) The two `ctest` entries run only where CMake finds Python, as the item catalogue's do; the CI configurations that have it run them.

### TASK-MCP-074: Read the table with the owner
- **Tier**: S
- **Status**: Not Started
- **Description**: Present the generated table's counts and its `none` and `catalogue only` rows to the owner; record what he decides to implement,
  to exclude (a declared `not offered`, with his word as the decision) or to leave, as a list of follow-up tasks or amendments; nothing is
  implemented here.
- **Requirement refs**: RQ-MCP-058, RQ-MCP-060
- **ADR refs**: ADR-MCP-006
- **Acceptance Criteria** (Gherkin): *Given* the generated table, *When* it is read with the owner, *Then* each `none` row is either planned, declared
  `not offered` with his decision, or left open and listed.
- **Dependencies**: TASK-MCP-073
- **Assignee**: Human and AI
- **Verification**: N/A (Tier S)
- **Assumptions**: None
