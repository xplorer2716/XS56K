# FTR-MCP-006: A Generated Table of the SysEx Specification's Messages, the AKM Function and the MCP Tool of Each

## Overview

The goal of the project (root `README.md`) is that the AKM library covers every message of the S5000/S6000 SysEx specification and that the MCP
server offers every function of the samplers that the specification gives, except what the owner has excluded. Until now that has been
checked section by section (`process/3.plan/SUMMARY-akm-sections-coverage.md`, `generate_akm_items.py --coverage`, a count of the
catalogue against the spec's rows), never message by message across the three layers. On 2026-10-10 the owner asked for a table to answer it:

- one table **per section of the specification**, one row per message;
- a column with the **name of the AKM function** that implements the message;
- a column with the **MCP tool** that offers it;
- the whole **generated into a Markdown file**, referenced by the root `README.md`;
- the tool that generates it **kept in the repository**, so that it can be run again; the owner asked that it live in `juce/tools` with the
  other tools.

Facts the requirements rest on (read in the repository on 2026-10-10):

- The item catalogue `juce/akm/data/items.json` holds **384 items**, one per command of the specification's Control tables, in the 13 sections
  `00 02 04 06 08 0A 0C 0E 10 12 14 16 20`. `documents/_index/sysex_spec.items.tsv` has 384 command rows (`C`) and 176 REPLY rows (`R`, the
  reply format of a Get): a REPLY is not a message of its own, so the rows of the table are the 384.
- The specification's Table 32 also gives **eight sections that are in neither the catalogue, nor the TSV, nor the code**: the forms of four
  sections addressed by index (`2A` program, `2C` multi, `2E` sample, `32` multi effects) and the four blocked multi-request sections (`38`
  keygroup zone, `3A` keygroup, `3C` program, `3E` multi). The table must show them, with nothing behind them.
- A message reaches the sampler through a **named function** of the AKM library (`juce/akm/include/akm`: `getAllProgramNames`,
  `renameSetList`, ...), or through the **catalogue alone**: any code may build a request for an item with `akm::makeRequest(ItemId::X)` and send
  it with `Session::submit`, which is how the MCP server sets and reads the keygroup, zone and program parameters (`StandardParameters.cpp`).
  A quick text search on 2026-10-10 found 158 of the 384 item names in the AKM sources, 226 in the MCP sources, and 4 in neither
  (`ProgramGetUserTuneTemplate`, `ProgramSetUserTuneTemplate`, `SampleGetAllBasicParams`, `SampleGetAllSettableParams`).
- The MCP server has 77 tools (56 with no option, 72 with `--allow-disk`, 61 with `--allow-front-panel`); `juce/mcp/README.md` names each one
  and the option it needs, and a test keeps that list equal to the server's.
- `generate_akm_items.py` (`juce/tools`) is the model for a tool of this kind: a Python script with no dependency, a `--check` mode run by `ctest`,
  and unit tests in `juce/tests/tools`.

---

## Functional Requirements

### RQ-MCP-058: One table per section, one row per message
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The generated file SHALL hold, for each of the 13 sections of the SysEx specification in the order of its Table 4 and then one
  group for the eight sections of its Table 32, a table with one row per message of the section, whose columns are: the message (its item code
  as the spec writes it, its name, and whether it is a Set or a Get), the AKM function, and the MCP tool.
- **Rationale**: the owner asked for the specification's messages by section, each with the function and the tool that implement it.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the catalogue of 384 items, *When* the file is generated, *Then* it has 13 section tables with 384
  message rows in all, each row giving the section's own item code; *and* a group for the eight sections of Table 32, each as one row.
- **Dependencies**: None

### RQ-MCP-059: The AKM column names the function, or the generic builder
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The AKM column of a message SHALL name every public function of the AKM library (as declared under `juce/akm/include/akm`) whose
  implementation sends that message; where no function does but the message is in the catalogue, it SHALL name the generic builder that makes it
  (`makeRequest`, or `makeStringRequest` or `makeTwoStringRequest` for an item whose arguments are one or two names); where it is not in the
  catalogue (the sections of Table 32) it SHALL say `none`.
- **Rationale**: the AKM library builds a message either through a named function or only through the catalogue and its generic builder; the reader
  must be able to tell the two apart, and a gap from either. *Amended 2026-10-10 (the owner):* the cell said `catalogue only`, which names nothing;
  it names the builder now.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a message sent only by `getAllProgramNames`, *When* the file is generated, *Then* its AKM cell is
  `getAllProgramNames`; *given* a catalogued message with numeric arguments that no AKM function sends, *then* its cell is `makeRequest`; *given* one
  that takes one name, *then* `makeStringRequest`; *given* a section of Table 32, *then* its cell is `none`.
- **Dependencies**: RQ-MCP-058

### RQ-MCP-060: The MCP column names the tools, the option they need, and what is left out
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The MCP column of a message SHALL name every tool of the MCP server that sends it, followed by the launch option that enables the
  tool when it needs one (`--allow-disk`, `--allow-front-panel`); a message the owner or an ADR decided not to offer SHALL say `not offered`
  followed by the ID of the decision; a message that the server's own session sends at its opening and its closing, and that no tool carries,
  SHALL say `server session`; any other message with no tool SHALL say `none`.
- **Rationale**: a message with no tool is a decision (ejecting a disk), the server's own business (the session's configuration of the sampler) or
  a gap, and the table must show which. *Amended 2026-10-10 (TASK-MCP-072):* `server session` was added after the first run showed the six
  messages of section 00 that the server sends itself reading `none`, like a gap.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a message sent by `set_parameter`, *Then* its MCP cell is `set_parameter`; *given* one sent only by
  `load_file`, *then* it is `load_file (--allow-disk)`; *given* one that `ADR-MCP-005` excludes, *then* it says `not offered (DEC-MCP-028)`;
  *given* one that nothing sends and nothing excludes, *then* it says `none`; *given* one that the session sends and no tool does, *then* it
  says `server session`.
- **Dependencies**: RQ-MCP-058

### RQ-MCP-061: A count per section opens the file
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The generated file SHALL begin with a table giving, for each section and for the whole, the number of messages, how many have a
  named AKM function, how many are built by a generic builder, how many have an MCP tool, how many are `not offered`, and how many are `none` in the MCP
  column.
- **Rationale**: the question behind the table is whether anything is missing; the counts answer it before the rows do.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* the generated file, *Then* the counts of the first table equal the counts of the rows below it,
  section by section.
- **Dependencies**: RQ-MCP-058, RQ-MCP-059, RQ-MCP-060

### RQ-MCP-062: A script in `juce/tools` writes the file, and can be run again
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the script `juce/tools/generate_sysex_coverage.py` is run with no argument, the script SHALL write the Markdown file from
  the files of the repository alone (the item catalogue, the spec's item list, the AKM and MCP sources and README, and one data file of declared
  facts), with the same bytes every time for the same inputs, and SHALL need nothing but Python 3's standard library.
- **Rationale**: the owner asked that the tool be kept so that the table can be produced again; a result that depends on the day or on an
  installed package could not be compared.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a clean checkout, *When* the script is run twice, *Then* the two files are identical and the
  second run changes no file of the working tree.
- **Dependencies**: RQ-MCP-058

### RQ-MCP-063: A table that is behind the code fails the tests
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF the committed file differs from what the script would write, THEN `generate_sysex_coverage.py --check` SHALL exit with a
  non-zero code and name the first section whose rows differ; IF a catalogue item, a public AKM function or an MCP tool exists that the script
  can neither attribute to a message nor find in the declared data file, THEN the script SHALL exit with a non-zero code and list each one. The
  `--check` run SHALL be a `ctest` entry.
- **Rationale**: a table that nobody has to maintain by hand stays right only if a change that makes it wrong breaks the build of the tests.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a new tool added to the server without a line in the table, *When* `ctest` runs, *Then* the entry
  fails and names the tool; *given* the file edited by hand, *then* `--check` fails and names the section.
- **Dependencies**: RQ-MCP-062

### RQ-MCP-064: The README and the document index point to the file; AGENTS.md has the command
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The root `README.md` SHALL link to the generated file where it speaks of the library's and the server's coverage of the
  specification, `documents/INDEX.md` SHALL list it, and `AGENTS.md` SHALL give the command that regenerates it.
- **Rationale**: the owner asked that the root `README.md` reference the file; the other two are where the project already lists its documents
  and its commands.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the repository, *When* `README.md` is read, *Then* a link to the file is there and the file exists.
- **Dependencies**: RQ-MCP-062

### RQ-MCP-066: The links between tools and messages are checked against the simulated sampler
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a tool, called by the scripted conversations against the simulated sampler, sends a message that the table gives neither as
  that tool's own nor as one that its declared gateway methods send through the other methods they call, THEN the check SHALL exit with a non-zero
  code and name the tool and the message; the check SHALL list the tools the conversations never call, and the messages of a tool's row that no
  call of it sent. The check SHALL be a `ctest` entry.
- **Rationale**: which tool calls which gateway method is the one link of the table that no reading of the code proves (`ADR-MCP-006`, DEC-MCP-037:
  15 of the 77 tools are registered through shared code); the simulated sampler records what each tool really sent. Added 2026-10-10 on the owner's
  choice of the declared list with this check.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a declared entry that gives `rename_file` the gateway method of `rename_folder`, *When* the check runs on
  a conversation that renames a file, *Then* it fails and names `rename_file` and the file-rename message; *given* the real declarations, *then* it
  passes and lists the tools no conversation calls.
- **Dependencies**: RQ-MCP-060, RQ-MCP-063

## Non-Functional Requirements

### RQ-MCP-065: The script is tested
- **Category**: Non-Functional
- **NFR Type**: Maintainability
- **EARS Type**: Ubiquitous
- **Statement**: The script SHALL have unit tests in `juce/tests/tools`, named in Gherkin, run by `ctest`, over small fixture inputs: the rows of
  a section, the attribution of a message to a function and to a tool, the three cell states of each column, the counts, the determinism of the
  output and the failures of RQ-MCP-063 and RQ-MCP-066.
- **Metric**: every cell state and every failure of RQ-MCP-058 to RQ-MCP-063 has at least one test; the suite passes.
- **Measurement Method**: `ctest -R sysex_coverage` on the three platforms of the CI; the test names are read against the Gherkin criteria above.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the test file, *When* `ctest` runs, *Then* the cases of the unit tests pass and each one cites its
  RQ ID.
- **Dependencies**: RQ-MCP-062, RQ-MCP-063, RQ-MCP-066
