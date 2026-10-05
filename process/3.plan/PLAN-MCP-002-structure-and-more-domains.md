# PLAN-MCP-002: MCP Server — Program Structure in Memory, the Rest of the Keygroup and the Program, Zones, Samples and Multis

## Overview

Implements `FTR-MCP-002` on top of the server of `PLAN-MCP-001`. The order is chosen so that the real sampler is reached as
early as possible: the structure tools come first because they give the server a target of its own (the sampler's memory was
empty), then the real run of the 54 parameters of FTR-MCP-001 (TASK-MCP-009, which this plan carries), then the wider
catalogue, zones, samples and multis, each verified on the simulated sampler in `ctest` and, where the sampler holds the
material, on the real one.

**Safety note.** Every tool stays on the sampler's memory; nothing saves, loads or touches the disk; Delete ALL and Clear
Sampler Memory are never sent (RQ-MCP-014). Real runs act only on objects the server created for the run and delete them
after; the owner's own programs are never selected. Every commit of the session carries "(HOL -Human on the loop)" after the
subject the `agnos-git-workflow` skill computes.

## References
- **Requirements**: RQ-MCP-013 to RQ-MCP-022 (`FTR-MCP-002`); RQ-MCP-008, RQ-MCP-012 (`FTR-MCP-001`)
- **ADRs**: ADR-MCP-002 (Proposed): DEC-MCP-010 to DEC-MCP-014; ADR-MCP-001: DEC-MCP-003, DEC-MCP-005, DEC-MCP-006,
  DEC-MCP-007 (amended), DEC-MCP-009.

The plan has 8 tasks (TASK-MCP-010 to TASK-MCP-017). 010 authors the artifacts; 011 the program structure tools (after 010);
012 the real-sampler run of the server (after 011, which creates its scratch program); 013 the catalogue's lot 3 (after
012); 014 zones (after 013); 015 samples (after 012); 016 multis (after 012); 017 the documentation (after the others
that are done). Tier L for 011 (it changes the safety rule), M for the others, S for 010 and 017.

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-MCP-010: Author FTR-MCP-002, ADR-MCP-002 and PLAN-MCP-002
- **Tier**: S
- **Status**: Done
- **Description**: Write the feature file, the architecture decision record and this plan from the owner's brief (samples, parameters outside the four groups, multis, create/rename/save of a program; the real run by the agent with the probe; commits tagged HOL) and the AKM sources.
- **Requirement refs**: RQ-MCP-013 to RQ-MCP-022
- **ADR refs**: ADR-MCP-002
- **Acceptance Criteria** (Gherkin): *Given* the owner's brief, *When* the artifacts are read, *Then* each requirement has Gherkin criteria, each decision is a `DEC-MCP-` heading, and every task below names its requirements.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**: "Save" is read as "save in memory" only (create, rename, delete); saving to disk is excluded (ADR-MCP-002 Alternatives), because §10 is the section that hung the S5000. The owner's delegation ("je te laisse faire") stands for the acceptance of the ADR, which stays "Proposed" until reviewed with the pull request.

### TASK-MCP-011: Program structure tools — create_program, rename_program, delete_program
- **Tier**: L
- **Status**: Not Started
- **Description**: Add the gateway's structure calls and the three tools, with the delete guard; move the source-search test from "no create/rename/delete" to the allow/deny list of DEC-MCP-010; give every tool its tier annotations; extend the simulated sampler's conversation.
- **Requirement refs**: RQ-MCP-013, RQ-MCP-014, RQ-MCP-015, RQ-MCP-016, RQ-MCP-017
- **ADR refs**: ADR-MCP-002 (DEC-MCP-010, DEC-MCP-011)
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler, *When* `create_program` is called with "TEST" and 3 keygroups, *Then* it is listed, current and has 3 keygroups. *Given* a 13-character name or 0 or 100 keygroups, *Then* nothing is sent and `isError` is true. *Given* a current program "TEST", *When* `rename_program` gives "TEST2", *Then* the answer names both and the list shows "TEST2". *Given* confirm "OTHER", *When* `delete_program` is called, *Then* nothing is sent and the answer names "TEST2"; *Given* confirm "TEST2", *Then* the program is gone. *Given* the sources of `juce/mcp`, *When* searched, *Then* the disk, delete-all and clear-memory primitives are not called, and the program create, rename and delete-current primitives are called only from the gateway.
- **Dependencies**: TASK-MCP-010
- **Assignee**: AI
- **Verification**: (to be filled at closure)
- **Assumptions**: None yet.

### TASK-MCP-012: Real-sampler run of the server (closes TASK-MCP-009)
- **Tier**: M
- **Status**: Not Started
- **Description**: With the sampler connected on `MIDIIN2 (ESI M8U eX)` / `MIDIOUT15 (ESI M8U eX)`, run the server by hand (a copy of the executable), create a scratch program with `create_program`, read, set and put back the 54 parameters (all keygroups, and one keygroup for a few), try rename and delete, settle the open points of FTR-MCP-001, write `process/2.architecture/OBSERVATIONS-RQ-MCP-012-real-sampler.md`, correct the simulated sampler for what it had wrong and regenerate the expected conversation.
- **Requirement refs**: RQ-MCP-012, RQ-MCP-022, RQ-MCP-002, RQ-MCP-003
- **ADR refs**: ADR-MCP-001 (DEC-MCP-004, DEC-MCP-006, DEC-MCP-009); ADR-MCP-002 (DEC-MCP-014)
- **Acceptance Criteria** (Gherkin): *Given* the real sampler and a program created by the server, *When* each parameter is read, set to another value, read back and put back, *Then* each read-back equals the value set. *Given* the run's end, *Then* the created program is deleted and the section 00 settings are in the known state. *Given* every answer the real sampler gave that the simulated one did not, *When* the task closes, *Then* it is in the observations file and the simulator matches it.
- **Dependencies**: TASK-MCP-011
- **Assignee**: AI (the owner's sampler, with the owner's authorisation)
- **Verification**: (to be filled at closure)
- **Assumptions**: None yet.

### TASK-MCP-013: Catalogue lot 3 — the rest of the keygroup and of the program
- **Tier**: M
- **Status**: Not Started
- **Description**: Add the rows for the auxiliary envelope, the keygroup pitch and amplitude items and the general options of §08, and the output, MIDI/tune, pitch bend and keygroup modulation source items of §0A, each in a group of its own, listing every item left out with its reason.
- **Requirement refs**: RQ-MCP-018
- **ADR refs**: ADR-MCP-002 (DEC-MCP-012)
- **Acceptance Criteria** (Gherkin): *Given* the catalogue, *When* compared with the item catalogue, *Then* every Set and Get item of those groups is a row or is listed with its reason. *Given* each new row, *When* set through `set_parameter` on the simulated sampler, *Then* it reads back. *Given* the consistency test, *Then* it passes for every new row.
- **Dependencies**: TASK-MCP-012
- **Assignee**: AI
- **Verification**: (to be filled at closure)
- **Assumptions**: None yet.

### TASK-MCP-014: Zone parameters
- **Tier**: M
- **Status**: Not Started
- **Description**: Add the `Zone` scope, the `zone` argument of `get_parameters` and `set_parameter`, the zone rows and their gateway selection steps.
- **Requirement refs**: RQ-MCP-019
- **ADR refs**: ADR-MCP-002 (DEC-MCP-012)
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler, *When* a zone parameter is set for keygroup 1, zone 2, *Then* it reads back for that zone and not for zone 1. *Given* "filter cutoff" with `zone` 2, *Then* `isError` is true.
- **Dependencies**: TASK-MCP-013
- **Assignee**: AI
- **Verification**: (to be filled at closure)
- **Assumptions**: None yet.

### TASK-MCP-015: Samples — list, select, get and set parameters
- **Tier**: M
- **Status**: Not Started
- **Description**: Add the sample catalogue, the gateway's sample calls and the four sample tools (and the `domain` argument of `list_parameters`).
- **Requirement refs**: RQ-MCP-020
- **ADR refs**: ADR-MCP-002 (DEC-MCP-012, DEC-MCP-013)
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding samples, *When* one is selected and a parameter set, *Then* it reads back. *Given* an empty sampler, *Then* `list_samples` says it holds none. *Given* the sources, *Then* no sample create, delete, rename or load primitive is called.
- **Dependencies**: TASK-MCP-012
- **Assignee**: AI
- **Verification**: (to be filled at closure)
- **Assumptions**: None yet.

### TASK-MCP-016: Multis — list, select, get and set parameters
- **Tier**: M
- **Status**: Not Started
- **Description**: Add the multi catalogues (general information and part parameters), the gateway's multi calls and the four multi tools.
- **Requirement refs**: RQ-MCP-021
- **ADR refs**: ADR-MCP-002 (DEC-MCP-012, DEC-MCP-013)
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding a multi, *When* it is selected and "part level" set for part 2, *Then* it reads back for part 2 only. *Given* the sources, *Then* the multi create, delete and Delete ALL primitives are not called.
- **Dependencies**: TASK-MCP-012
- **Assignee**: AI
- **Verification**: (to be filled at closure)
- **Assumptions**: None yet.

### TASK-MCP-017: Documentation and closure
- **Tier**: S
- **Status**: Not Started
- **Description**: Update `AGENTS.md`, `CHANGELOG.md`, `juce/mcp/README.md` and the checkpoint for what was delivered and what was run on the real sampler.
- **Requirement refs**: RQ-MCP-022
- **ADR refs**: ADR-MCP-002
- **Acceptance Criteria** (Gherkin): *Given* the delivered tools, *When* the documents are read, *Then* each tool is listed with its tier and each is marked as run on the real sampler or on the simulated one only.
- **Dependencies**: TASK-MCP-011, TASK-MCP-012
- **Assignee**: AI
- **Verification**: N/A (Tier S)
- **Assumptions**: None
