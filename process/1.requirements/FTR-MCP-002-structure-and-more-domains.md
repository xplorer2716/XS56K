# FTR-MCP-002: MCP Server — Program Structure in Memory, the Rest of the Keygroup and the Program, Zones, Samples and Multis

## Overview

Session MCP, 2026-10-05, run under the owner's delegation (HOL, Human on the loop: the owner reviews the commits, the
agent decides the order). FTR-MCP-001 delivered a proof of concept that edits 54 parameters of a program already in the
sampler's memory. This feature widens the server in five steps, all on the sampler's **memory** and all verified first
against the simulated sampler:

1. **Program structure in memory**: create a program (with a number of keygroups), rename the current program, delete the
   current program behind a named confirmation. This also lets a client (and the owner's real run) build its own scratch
   program instead of depending on one prepared by hand.
2. **The rest of the keygroup and of the program**: the auxiliary envelope, the keygroup's pitch and amplitude items, the
   general options of §08 and the program's output, MIDI/tune, pitch bend and keygroup modulation source items of §0A. These
   are rows of the existing catalogue (RQ-MCP-010 proved that a parameter costs a row).
3. **Zones**: the parameters of a keygroup's zones (§06), reached through a `zone` argument.
4. **Samples**: list, select and edit the parameters of the samples in memory (§0E).
5. **Multis**: list, select and edit the parameters of a multi's parts and its general information (§0C).

**Vocabulary** is unchanged: the musician's words, the sampler's own units, signed values as plain signed numbers, choices
by their screen label (FTR-MCP-001).

**Safety model.** FTR-MCP-001's "nothing destructive" rule (RQ-MCP-008) is replaced by three tiers that every tool
declares (ADR-MCP-002 DEC-MCP-010): *read* (changes nothing), *edit* (overwrites a value in memory) and *structure* (creates,
renames or deletes an object in memory). A fourth class is **never offered**: anything that saves or writes to disk
(§10), deletes everything (Delete ALL Programs, Clear Sampler Memory), or changes the sampler's own settings beyond the
session's §00 handling. Saving stays on the sampler's front panel.

**Out of scope.** Saving or loading any file; deleting all of anything; creating or deleting multis, samples or zones
(sections 0C and 0E creation items, §06 keygroup/zone creation); the disk, the system setup, the MIDI setup, song files,
scenelists, FX, the front panel; a graphical interface; any transport other than standard input and output.

**Depends on** FTR-MCP-001 (RQ-MCP-001 to RQ-MCP-012), FTR-AKM-002 (program), FTR-AKM-003 (keygroup), FTR-AKM-004
(zone), FTR-AKM-005 (sample), FTR-AKM-011 (multi), ADR-MCP-001, ADR-AKM-001.

**Sources.** `documents/_index/sysex_spec.kb.md`, `documents/_index/sysex_spec.items.tsv` (sections 06, 08, 0A, 0C, 0E), the
AKM primitives of `juce/akm/include/akm/`.

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: an MCP client (Claude Code or another) and the person using it; CI (simulated sampler only).

---

## Functional Requirements

### RQ-MCP-013: Safety tiers declared by every tool
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Every tool of the server SHALL belong to exactly one tier — read, edit or structure — which SHALL show in its MCP annotations (`readOnlyHint` true only for read; `destructiveHint` true only for a tool that can delete) and in its description, and the descriptions of the edit and structure tools SHALL state that the change acts on the sampler's memory, not on disk.
- **Rationale**: a model decides what to call from the annotations and the description; the tiers make the cost of a call readable.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the list of tools, *When* it is read, *Then* each tool's annotations match its tier, and a tool that deletes carries `destructiveHint` true.
- **Dependencies**: RQ-MCP-008 (superseded in part); ADR-MCP-002 (DEC-MCP-010)

### RQ-MCP-014: Never offered — disk, delete-all and the sampler's settings
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: The server SHALL expose no tool that saves or loads a file, that writes to or reads the disk (§10), that deletes all programs or clears the sampler's memory, or that changes a sampler setting outside the session's own §00 handling; IF a client calls such a tool, THEN the server SHALL answer an invalid-params error as for any unlisted tool.
- **Rationale**: those operations lose data or hang the sampler (observed on the S5000, `OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`); a model must not be one tool call from them.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the sources of `juce/mcp`, *When* searched for the disk primitives (`DiskPrimitives`), the Delete ALL primitives and Clear Sampler Memory, *Then* none is called. *Given* the list of tools, *Then* none is named save, load, delete all or clear.
- **Dependencies**: RQ-MCP-008; ADR-MCP-002 (DEC-MCP-010)

### RQ-MCP-015: Create a program
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `create_program` with a name and a number of keygroups, the server SHALL create that program in the sampler's memory, make it current, and answer its name and keygroup count read back from the sampler; IF the name is not valid for the sampler (empty, more than 12 characters, a character the sampler does not take) or the keygroup count is outside 1 to 99, THEN it SHALL send nothing and say what is accepted.
- **Rationale**: lets a client build a scratch program to edit, and gives the owner's real run a disposable target.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler, *When* `create_program` is called with "TEST" and 3 keygroups, *Then* `list_programs` shows it, it is current, and it has 3 keygroups. *Given* a name of 13 characters, *Then* nothing is sent and the answer is `isError` true.
- **Dependencies**: FTR-AKM-002 (RQ-AKM-020 to RQ-AKM-025); ADR-MCP-002 (DEC-MCP-010, DEC-MCP-011)

### RQ-MCP-016: Rename the current program
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `rename_program` with a new name, the server SHALL rename the current program and answer the old and the new name, the new one read back; IF no program is current or the name is not valid, THEN it SHALL send nothing (or report the sampler's refusal) and say so.
- **Rationale**: names are how programs are found; renaming is the structure edit with the least risk.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a current program "TEST", *When* `rename_program` gives "TEST2", *Then* the answer says "TEST" became "TEST2" and `list_programs` shows "TEST2".
- **Dependencies**: RQ-MCP-015; ADR-MCP-002 (DEC-MCP-010)

### RQ-MCP-017: Delete the current program only behind a named confirmation
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: The server SHALL delete a program only through `delete_program`, which acts on the current program, takes the program's name as `confirm`, and sends the deletion only when `confirm` equals the name the sampler reports for the current program at that moment (compared as the sampler spells it); IF it does not, THEN the server SHALL send nothing and answer the current program's name.
- **Rationale**: a deletion cannot be undone from the server, and a stale mental model of "the current program" is the likely error.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a current program "TEST", *When* `delete_program` is called with confirm "OTHER", *Then* nothing is sent, the program is still listed and the answer names "TEST". *Given* confirm "TEST", *Then* the program is gone and the answer says so. *Given* the sources of `juce/mcp`, *Then* the delete-all primitive is not called.
- **Dependencies**: RQ-MCP-014; ADR-MCP-002 (DEC-MCP-010, DEC-MCP-011)

### RQ-MCP-018: The rest of the keygroup and of the program
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The parameter catalogue SHALL hold, in addition to RQ-MCP-010, the Set and Get pairs of §08 for the auxiliary envelope, the keygroup pitch and amplitude items and the general options, and of §0A for output, MIDI/tune, pitch bend and the keygroup modulation sources, each named in words and in a group of its own, read and set through `get_parameters` and `set_parameter` with no new code beyond its row; every item of those groups left out SHALL be listed with its reason.
- **Rationale**: the catalogue is data; this lot completes the keygroup and the program.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* the catalogue, *When* compared with the item catalogue, *Then* every Set and Get item of those groups of §08 and §0A is a row, or is listed with its reason. *Given* a row, *When* set on the simulated sampler through `set_parameter`, *Then* it reads back the value set.
- **Dependencies**: RQ-MCP-010; ADR-MCP-002 (DEC-MCP-012)

### RQ-MCP-019: Zone parameters
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `get_parameters` or `set_parameter` on a zone parameter with a `keygroup` (a number, or "all") and a `zone` (1 to 4, or "all"), the server SHALL select that keygroup and zone, act, and read the value back; IF the parameter is a zone parameter and no zone is given, THEN it SHALL act on all zones of the keygroup(s); IF a program or keygroup parameter is given a `zone`, THEN it SHALL refuse and say so.
- **Rationale**: zones carry what makes a keygroup sound (sample, tuning, level, pan, velocity and key ranges).
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler, *When* "zone sample" is set for keygroup 1, zone 2, *Then* it reads back for that zone and not for zone 1. *Given* "filter cutoff" with `zone` 2, *Then* the answer is `isError` true.
- **Dependencies**: FTR-AKM-004; ADR-MCP-002 (DEC-MCP-012)

### RQ-MCP-020: Samples
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `list_samples`, `select_sample`, `get_sample_parameters` or `set_sample_parameter`, the server SHALL list the samples in memory, make one current by name or position, read a group or a list of its parameters, or set one and read it back, in the musician's vocabulary and with the same value rules as the program parameters; IF the sampler holds no sample, THEN it SHALL say so; no tool SHALL create, delete or rename a sample or load one.
- **Rationale**: the sample's loop points, tuning and playback mode are edited as often as the program's.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding samples, *When* one is selected and a parameter set, *Then* it reads back. *Given* an empty sampler, *Then* `list_samples` says it holds none.
- **Dependencies**: FTR-AKM-005; ADR-MCP-002 (DEC-MCP-012, DEC-MCP-013)

### RQ-MCP-021: Multis
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `list_multis`, `select_multi`, `get_multi_parameters` or `set_multi_parameter`, the server SHALL list the multis in memory, make one current by name or position, read a group or a list of its general information and of the parameters of a part (1 to 16, or "all"), or set one and read it back; IF the sampler holds no multi, THEN it SHALL say so; no tool SHALL create, delete or rename a multi, and Delete ALL Multis SHALL never be sent.
- **Rationale**: the multi's parts (program, level, pan, channel) are what a performance edits.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding a multi, *When* it is selected and "part level" set for part 2, *Then* it reads back for part 2 only.
- **Dependencies**: FTR-AKM-011; ADR-MCP-002 (DEC-MCP-012, DEC-MCP-013)

---

## Non-Functional Requirements

### RQ-MCP-022: Verified against the simulated sampler, then on the real one
- **Category**: Non-Functional
- **NFR Type**: Reliability
- **EARS Type**: Ubiquitous
- **Statement**: Every requirement of this feature SHALL be verified in `ctest` against the simulated sampler, the scripted conversation of RQ-MCP-012 SHALL be extended to cover each new tool, and each new tool SHALL be run once against the real S5000 on a program or multi created for the purpose (never on the owner's own), with what the real sampler answered recorded in the observations file and the simulated sampler corrected for any difference.
- **Metric**: new tools run on the real sampler (each, or each exception listed).
- **Measurement Method**: `ctest` for the simulated runs; a scripted conversation on the real sampler, logged on standard error.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the extended conversation, *When* it runs against the simulated sampler in `ctest`, *Then* every answer matches the expected one. *Given* the same tools on the real sampler, *When* the run ends, *Then* the objects created for it are deleted and the observations are written.
- **Dependencies**: RQ-MCP-012, all functional requirements of this feature

---

## Open points

- **Name rules of a program** (length, characters): taken from the AKM primitives' own validation; whether the S5000 takes more is observed on the real sampler.
- **Zone count and the select item**: read from the zone primitives when TASK-MCP-014 starts.
- **Sample and multi parameter lists**: the exclusions, if any, are listed by the task that adds them.
- **Real runs of zones, samples and multis** need material in the sampler (a sample in memory); the owner's sampler held none at the start of the session.
