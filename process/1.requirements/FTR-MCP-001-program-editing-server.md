# FTR-MCP-001: MCP Server for Program Editing (proof of concept)

## Overview

Session MCP, 2026-10-04. The goal is a **Model Context Protocol (MCP) server** that exposes the AKM layer
(`juce/akm`, the S5000 SysEx layer) to any MCP client, so that a person can edit a program of the sampler by talking to an
assistant: "set the filter cutoff to 80", "change the filter type to 2-pole low-pass plus", "set the amplitude envelope
attack to 55". The server lives in `juce/mcp` and speaks MCP over standard input and output (the transport an MCP client
such as Claude Code launches as a local process).

This first feature is a **proof of concept**. It is deliberately limited to the editing of a program already in the
sampler's memory, in four groups: the **filter**, the **amplitude envelope**, the **filter envelope** and the two
**LFOs**. Its purpose is to show that the AKM layer can be driven this way; if the result is conclusive, other domains
(the rest of the keygroup, zones, multis, samples) are added to the same server in later features.

**Vocabulary.** The tools speak the musician's language, not the protocol's. Parameters are named in words ("filter
cutoff", "amplitude envelope attack", "LFO 1 rate"), values are given in the sampler's own front-panel units (0 to 100
for most), signed values are plain signed numbers (-100 to +100, not a sign byte and a magnitude), and choices are
named by their label on the sampler's screen ("2-POLE LP+", "TRIANGLE"), not by their code.

**In scope** (items of `documents/_index/sysex_spec.items.tsv`, in the catalogue as `ItemId`s):

| Group | Where it lives | Items (Set and Get pairs) |
|---|---|---|
| Filter | the keygroup (§08, Table 11) | mode, cutoff, resonance, keyboard track, attenuation (lot 1); the three modulation inputs' amounts (lot 2) |
| Amplitude envelope | the keygroup (§08) | attack, decay, sustain, release (lot 1); velocity to attack, on and off velocity to release, key scale (lot 2) |
| Filter envelope | the keygroup (§08) | attack, decay, sustain, release, depth (lot 1); velocity to attack, on and off velocity to release, key scale (lot 2) |
| LFO 1 and LFO 2 | the program (§0A, Table 13) | rate, delay, depth, waveform, sync (LFO 1) or re-trigger (LFO 2) (lot 1); modulation sources and amounts of rate, delay and depth, modwheel and aftertouch (LFO 1), MIDI clock sync (LFO 2) (lot 2) |

Lot 1 is the core of the demonstration (24 parameters); lot 2 completes the four groups with the items the spec lists for
them. Selecting a program (§0A `&05`/`&06`), reading the program names (`&19`), the keygroup count (`&14`) and selecting a
keygroup (§08 `&01`) are used to reach what is edited.

**Out of scope.** Creating, renaming, deleting or saving a program; the aux envelope, pitch, amplitude, pan, zones, samples,
multis, disk, system setup and every other section; loading from disk; a graphical interface; MCP features other than tools
(resources, prompts, sampling); any transport other than standard input and output; running the server against anything
but one sampler at a time.

**Depends on** FTR-AKM-001 (transport and session), FTR-AKM-002 (program primitives), FTR-AKM-003 (keygroup),
ADR-AKM-001 (DEC-AKM-001 layering, DEC-AKM-002 threading, DEC-AKM-003 and DEC-AKM-012 item catalogue).

**Sources.** `documents/_index/sysex_spec.kb.md` (state model: §08 acts on the current keygroup of the current program, keygroup
0 is "all"), `documents/akai_s5000_s6000_sysex_spec_2.10.pdf.md` pp. 20-25 (Tables 11 to 15), the MCP specification
(revisions 2026-07-28 and 2025-11-25 and earlier, see ADR-MCP-001 DEC-MCP-002).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: an MCP client (Claude Code or another) and the person using it; later features that add domains to the server; CI
  (simulated sampler only).

---

## Functional Requirements

### RQ-MCP-001: MCP server over standard input and output
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN an MCP client sends `server/discover`, or any request carrying `_meta` with `io.modelcontextprotocol/protocolVersion` (the stateless revision 2026-07-28), the server SHALL serve it without remembering earlier requests and answer an unsupported version with error -32022 listing the versions it supports; WHEN a client sends `initialize` (the revisions 2025-11-25 and earlier), the server SHALL answer with the client's revision when it supports it and its latest legacy revision otherwise, with the `tools` capability and its name and version, and SHALL then accept `notifications/initialized`; in both eras it SHALL serve `ping`, `tools/list` and `tools/call`, and SHALL write only protocol messages on standard output (one JSON-RPC message per line), sending its own diagnostics to standard error.
- **Rationale**: this is what lets a client launch the server and list its tools, whichever era the client speaks (a modern client probes with `server/discover` and falls back to `initialize`); a stray line on standard output corrupts the protocol.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a server on in-memory streams, *When* `server/discover` carries `2026-07-28`, *Then* the answer lists the supported versions, the `tools` capability, the server's name and version in `_meta`, `resultType` complete and `ttlMs` and `cacheScope`, and nothing else is written. *Given* a request carrying a version the server does not support, *When* it is sent, *Then* the answer is error -32022 with the supported versions. *Given* a request with no `_meta` and no earlier `initialize`, *Then* the answer is -32602. *Given* an `initialize` request carrying a supported legacy revision, *When* sent, *Then* the answer carries the same revision and the `tools` capability, and a later `tools/list` without `_meta` is served; *Given* an unknown revision, *Then* the answer carries the latest legacy one. *Given* a line that is not JSON, *When* it is read, *Then* the server answers a JSON-RPC parse error (-32700) and keeps reading. *Given* a method it does not have, *When* a request names it, *Then* the answer is a method-not-found error (-32601).
- **Dependencies**: ADR-MCP-001 (DEC-MCP-001, DEC-MCP-002)

### RQ-MCP-002: Configuration of the sampler connection by launch arguments
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the server is launched, it SHALL read the MIDI input port (`--in`), the MIDI output port (`--out`), and optionally the DeviceID (`--device-id`), the command timeout (`--timeout-ms`) and `--no-lcd`, from its arguments — so that an MCP client's server configuration is where they are set — and WHEN it is launched with `--list-ports` it SHALL print the MIDI ports and exit; IF `--in` or `--out` is missing or an argument is invalid, THEN it SHALL print the usage on standard error and exit with a non-zero status without starting the protocol.
- **Rationale**: the owner's decision: the ports are parameters of the MCP configuration; a person needs a way to find the port names.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the arguments `--in A --out B --device-id 2 --timeout-ms 3000`, *When* they are parsed, *Then* the configuration carries port A, port B, DeviceID 2 and 3000 ms. *Given* no `--out`, *When* parsed, *Then* the result is a usage error naming `--out`. *Given* `--device-id 99`, *When* parsed, *Then* the result is a usage error (a DeviceID is 0-127, as the AKM layer takes it).
- **Dependencies**: ADR-MCP-001 (DEC-MCP-008)

### RQ-MCP-003: Connection to the sampler, opened when first needed, closed on exit
- **Category**: Functional
- **EARS Type**: State-driven
- **Statement**: WHILE no session to the sampler is open, the server SHALL open one with `Session::open` the first time a tool that needs the sampler is called, and again at the next such call if the opening failed; IF the sampler does not answer, answers ambiguously or an opening setting fails, THEN the tool's result SHALL be an error that says so in plain words; WHEN the client closes standard input or sends a shutdown, the server SHALL close the session (the sampler's §00 settings are put back) before it exits.
- **Rationale**: listing the tools must work with the sampler off; the sampler may be powered on later; leaving the sampler with checksums or Still Alive changed is the failure `RQ-AKM-042` exists to prevent.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler that answers nothing, *When* a tool needing it is called, *Then* the result is an error naming the absence of an answer and the server still answers `tools/list`. *Given* the sampler then made available, *When* the same tool is called again, *Then* it succeeds. *Given* an open session, *When* standard input reaches its end, *Then* the session is closed and the section 00 settings are back to their defaults before the process ends.
- **Dependencies**: FTR-AKM-001 (RQ-AKM-039 to RQ-AKM-042); ADR-MCP-001 (DEC-MCP-003, DEC-MCP-004)

### RQ-MCP-004: Parameter catalogue in the musician's vocabulary
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls the tool `list_parameters` (optionally for one group), the server SHALL answer, for every parameter of the groups in scope, its name in words, its group, a one-line description, its kind (number, signed number, choice, on/off), its range or its choices with their sampler labels, and whether it belongs to the keygroup or to the program.
- **Rationale**: the client (and the model behind it) can only use words it can see; the catalogue is the vocabulary.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the lot 1 catalogue, *When* `list_parameters` is called without argument, *Then* it lists 24 parameters, among them "filter cutoff" (number, 0 to 100), "filter type" (choice, 26 labels beginning "2-POLE LP", "4-POLE LP", "2-POLE LP+") and "LFO 1 waveform" (choice, 9 labels beginning "SINE", "TRIANGLE"). *Given* the group "amplitude envelope", *When* listed, *Then* only its parameters are returned. *Given* every parameter of the catalogue, *When* it is compared with the AKM item catalogue, *Then* its Set and Get items exist, take the arguments the table gives, and its range equals the item's range (so the catalogue cannot drift from the spec).
- **Dependencies**: ADR-MCP-001 (DEC-MCP-005); FTR-AKM-002 (RQ-AKM-024), FTR-AKM-003

### RQ-MCP-005: Reading parameters
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `get_parameters` with a group, or a list of parameter names, and optionally a keygroup (a number, or "all"), the server SHALL read each value from the current program, SHALL answer it in the catalogue's units — a choice by its label, a signed value as a signed number — and, for "all" keygroups of a keygroup parameter, SHALL answer one value per keygroup.
- **Rationale**: an assistant has to see a sound before it changes it ("make the attack a bit shorter" needs the current attack).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding a program of three keygroups whose filter cutoffs are 30, 60 and 90, *When* "filter cutoff" is read for "all", *Then* the answer is 30, 60, 90 in keygroup order. *Given* the same program, *When* it is read for keygroup 2, *Then* the answer is 60. *Given* a filter envelope depth stored as sign 1, magnitude 40, *When* it is read, *Then* the answer is -40. *Given* the filter type stored as 2, *When* read, *Then* the answer is the label "2-POLE LP+" and its code.
- **Dependencies**: RQ-MCP-004; FTR-AKM-003 (RQ-AKM-031)

### RQ-MCP-006: Writing a parameter
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `set_parameter` with a parameter name, a value (a number, or a choice label, or its code) and optionally a keygroup (a number, or "all", "all" being the default), the server SHALL resolve the name tolerantly (case, spaces and hyphens ignored, aliases accepted), refuse an unknown name, a value out of range and an unknown label without sending anything, and otherwise send the Set to the current program, read the value back with the matching Get, and answer the value the sampler reports; IF the read-back differs from the value set, THEN the answer SHALL say so and be an error.
- **Rationale**: "set the cutoff to 80" has to end with the person knowing the sampler holds 80; the AKM method is one Set and one Get verified by read-back (FTR-AKM-002 RQ-AKM-024).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler with a program, *When* "filter cutoff" is set to 80 for "all", *Then* every keygroup's cutoff reads 80 and the answer says 80. *Given* "filter type" and the label "2-pole LP+" (any case, spaces or hyphens), *When* set, *Then* the sampler holds code 2. *Given* "filter envelope depth" set to -40, *When* sent, *Then* the Set carries sign 1 and magnitude 40. *Given* "filter cutoff" set to 101, *When* requested, *Then* nothing is sent and the error names the range 0 to 100. *Given* "filter cutof", *When* requested, *Then* nothing is sent and the error proposes "filter cutoff".
- **Dependencies**: RQ-MCP-004, RQ-MCP-005; ADR-MCP-001 (DEC-MCP-006)

### RQ-MCP-007: Reaching the program and the keygroup to edit
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a client calls `get_status`, the server SHALL answer whether the sampler is connected, the number of programs in memory, the current program's name and its number of keygroups; WHEN it calls `list_programs`, the names of the programs in memory with their positions; WHEN it calls `select_program` with a name or a position, the server SHALL make that program current and answer its name and keygroup count, SHALL answer "no such program" for a name or position the sampler does not have, and SHALL address every later edit to it; a keygroup argument beyond the program's keygroups SHALL be refused with the count.
- **Rationale**: every §08 and §0A parameter acts on the current program (and §08 on the current keygroup); the person has to say which sound is meant.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler holding programs `PAD`, `BASS` and `LEAD`, *When* `list_programs` is called, *Then* the three names are listed in memory order. *Given* `select_program` with the name `BASS`, *When* `get_status` is called, *Then* the current program is `BASS`. *Given* a name that does not exist, *When* selected, *Then* the result is an error saying so. *Given* a program of 2 keygroups, *When* keygroup 3 is asked for, *Then* nothing is sent and the error says the program has 2.
- **Dependencies**: FTR-AKM-002 (RQ-AKM-021, RQ-AKM-023); FTR-AKM-003 (RQ-AKM-028)

### RQ-MCP-008: Editing only, nothing destructive
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: The server SHALL expose no tool that creates, renames, deletes or saves a program, a keygroup or any other object, and no tool that sends a command of a section other than §0A selection and information, §08 selection and the parameter items of the catalogue; IF a client calls a tool the server does not list, THEN it SHALL answer an invalid-params error; the descriptions of its tools SHALL state that a change acts on the sampler's memory, not on disk, and the tools that only read SHALL carry the read-only annotation.
- **Rationale**: an MCP client is a model, not a person at the front panel; the proof of concept must not be one tool call away from losing data. (The owner has a backup, but this is not a reason to ship the risk.)
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the list of tools, *When* it is read, *Then* it holds only `get_status`, `list_programs`, `select_program`, `list_parameters`, `get_parameters` and `set_parameter`, and the three that only read carry `readOnlyHint` true. *Given* the sources of `juce/mcp`, *When* searched for the delete, create, rename and save primitives of the AKM layer, *Then* there is no call.
- **Dependencies**: FTR-AKM-002 (RQ-AKM-025); ADR-MCP-001 (DEC-MCP-007)

### RQ-MCP-009: Errors are reported, never hang or crash
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF the sampler answers an ERROR, a refusal or nothing within the command timeout, or an argument does not fit the tool's schema, THEN the server SHALL answer the tool call with `isError` true and a message in plain words (what was asked, what happened, what the person can do), SHALL answer a malformed request with a JSON-RPC error, and SHALL stay able to answer the next request; one request SHALL be handled at a time, in order.
- **Rationale**: a model reads the message to decide what to do next; a hang blocks the client, a crash loses the session's restoring of the sampler's settings.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a simulated sampler that does not answer, *When* `set_parameter` is called, *Then* the answer arrives within the command timeout, carries `isError` true and says the sampler did not answer. *Given* an ERROR 04 (not found) from the sampler, *When* a program is selected, *Then* the message says the program was not found. *Given* a `tools/call` without the required `parameter` argument, *When* sent, *Then* the answer is an error naming it, and the next call succeeds.
- **Dependencies**: FTR-AKM-001 (RQ-AKM-010, RQ-AKM-012); ADR-MCP-001 (DEC-MCP-003)

### RQ-MCP-010: Lot 2, the rest of the four groups
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The parameter catalogue SHALL, after lot 2, hold every Set and Get pair of the AKM item catalogue that belongs to the filter, the amplitude envelope, the filter envelope and the two LFOs, each named in words, and every one of them SHALL be read and set through the same tools without any new code beyond its row.
- **Rationale**: the catalogue is data (the same design choice as the item catalogue, DEC-AKM-003); lot 2 proves that adding parameters costs rows, not code.
- **Priority**: Should
- **Acceptance Criteria** (Gherkin): *Given* the lot 2 catalogue, *When* compared with the item catalogue, *Then* no filter, amplitude envelope, filter envelope or LFO item of §08 and §0A is unaccounted for, or each one left out is listed with its reason. *Given* "LFO 1 rate modulation source", *When* set to a source label of Table 15, *Then* the sampler holds its code.
- **Dependencies**: RQ-MCP-004 to RQ-MCP-006

---

## Non-Functional Requirements

### RQ-MCP-011: Layering and maintainability
- **Category**: Non-Functional
- **NFR Type**: Maintainability
- **EARS Type**: Ubiquitous
- **Statement**: The server's library SHALL depend on `xs56k_akm` and on the JSON library only, SHALL expose no JUCE type in its public headers, and SHALL keep the protocol, the parameter catalogue, the sampler gateway and the tools in separate units each testable without the others' dependencies; the build SHALL stay warning-clean at the project's warning level.
- **Metric**: number of JUCE includes in `juce/mcp/include` (0); build warnings (0).
- **Measurement Method**: a compile check like `juce/tests/compile_checks` (no JUCE header reachable from the library's public headers); the build itself at `/W4 /WX` and `-Wall -Wextra -Wpedantic -Werror`.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the public headers of `juce/mcp`, *When* compiled in a translation unit that has no JUCE include path, *Then* they compile.
- **Dependencies**: ADR-MCP-001 (DEC-MCP-001); ADR-AKM-001 (DEC-AKM-001)

### RQ-MCP-012: Verified without hardware, then on the real sampler
- **Category**: Non-Functional
- **NFR Type**: Reliability
- **EARS Type**: Ubiquitous
- **Statement**: Every requirement of this feature SHALL be verified in `ctest` against the simulated sampler (no hardware), and the owner SHALL run the server once against the real S5000 on a scratch program, reading and setting every lot 1 parameter, with each value put back; what the real sampler answers that the simulated one does not SHALL be recorded in an observations file and the simulated sampler corrected, as for every AKM section.
- **Metric**: lot 1 parameters set and read back on the real sampler (24 of 24 or each exception listed).
- **Measurement Method**: `ctest` for the simulated runs; a scripted conversation (or an MCP client session) on the real sampler, logged on standard error.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a scripted JSON-RPC conversation (initialize, tools/list, select a program, set and read each lot 1 parameter), *When* it runs against the simulated sampler in `ctest`, *Then* every answer matches the expected one. *Given* the same conversation on the real sampler with a scratch program, *When* it ends, *Then* the program holds the values it held before, and the sampler's settings are in the known state.
- **Dependencies**: all functional requirements; FTR-AKM-001 (RQ-AKM-017, RQ-AKM-027)

---

## Open points

- **Which client era.** The server serves both eras; the revision a real client opens with (modern probe or legacy `initialize`) is observed by the real run (`TASK-MCP-009`). Of the legacy revisions only 2025-11-25 was read in full.
- **Setting with "all" keygroups** (§08 `&01` with 0 current): the spec says a Set then reaches every keygroup (Table 11, p. 20); not yet observed on the real sampler. The default of the tools is "all", which is the natural reading of "set the filter cutoff"; the real run (`TASK-MCP-009`) confirms or changes it.
- **The current keygroup is left as the last edit set it** (selecting a keygroup is a side effect of an edit); whether to put the sampler's keygroup selection back is decided with the real run.
- **Scratch program for the real run**: prepared by the owner by hand (recommended), or a creation tool added later; this feature has none (`RQ-MCP-008`).
- **Modulation source names** (Table 15, lot 2): to be read from the spec when `TASK-MCP-008` starts.
