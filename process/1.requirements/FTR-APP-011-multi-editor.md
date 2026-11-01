# FTR-APP-011: Multi Editor — the "Parts" Tab of the Edit Panel

## Overview

Session APP (2026-11-01). The owner: after the save, the multi ("continue sur multi"). This brings RQ-APP-063 (priority 2) forward and replaces
it. When a multi is selected in the memory view (it becomes the sampler's current multi, RQ-APP-037), the edit panel shows a multi editor, as the
program editor does for a program (FTR-APP-007, FTR-APP-008).

What section 0C of the SysEx specification allows (Tables 16 and 17): a multi has 32, 64 or 128 parts, set when it is created (`&01`, a stored
setting for the multis created afterwards); each part plays a program of memory or none (`&32`-`&34` part by index, by name, delete part;
`&45`, `&46` read), and has twelve parameters (MIDI channel, mute, solo, level, output, pan, effects channel, FX send level, fine tune,
transpose, low note, high note; Set `&10`-`&1B`, Get `&20`-`&2B`); the multi has a program number, off or 1 to 128 (Set `&31`, Get `&41`). The parts
cannot be added or removed one by one: "adding and clearing parts" (RQ-APP-063) is giving a part a program and taking it away. The rename and
the deletion of a multi are those of the memory view (RQ-APP-039, RQ-APP-040).

**Priorities.** Priority 1 is written `Must`, priority 2 `Could` (owner, 2026-10-13). The multi editor stays priority 2: `Could`.

**Depends on** FTR-APP-005 (memory view), FTR-APP-007 (RQ-APP-048 to RQ-APP-055), FTR-APP-008 (the keygroup selector, RQ-APP-058, RQ-APP-059),
RQ-AKM-087 to RQ-AKM-092, RQ-MCP-021, RQ-MCP-037, RQ-MCP-038 (the gateway's multis).

**Sources.** `documents/_index/sysex_spec.items.tsv` (section 0C), `documents/akai_s5000_s6000_user_manual.1.21.pdf.md` (MULTI mode),
`juce/mcp/src/StandardParameters.cpp` (`multiParameters()`), `juce/mcp/include/mcp/SamplerGateway.hpp` (the multi calls).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the person building a multi on the S5000.

---

## Functional Requirements

### RQ-APP-100: The "Parts" tab
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a multi is selected in the memory view, the edit panel SHALL show the multi editor with one tab, "Parts", holding the
  multi's program number in a group of multi scope, and the part shown in three groups of part scope: Program (the program it plays), Mix (mute,
  solo, level, output, pan, effects channel, FX send level) and Setup (MIDI channel, fine tune, transpose, low note, high note), each parameter
  under the multi catalogue's name, read from the sampler and edited under the rules of RQ-APP-051 to RQ-APP-054; the low note SHALL stay at or
  below the high note (RQ-APP-092).
- **Rationale**: the sampler's MULTI mode; the grouping is the assistant's proposal, data the owner can change (DEC-APP-006).
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* the multi LIVE selected, *When* the panel shows its editor, *Then* the tab "Parts" shows part 1's
  program and twelve parameters and the multi's program number, read from the sampler; *When* the person sets part 1's level to 70, *Then* it
  is sent, read back and shown; *Given* a high note of 60, *When* the person sets the low note to 72, *Then* nothing is sent and the range is shown.
- **Dependencies**: RQ-APP-048, RQ-APP-049, RQ-APP-051 to RQ-APP-054, RQ-APP-092, RQ-AKM-089, RQ-AKM-090

### RQ-APP-101: The part shown, or all parts
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a multi is shown, the editor SHALL offer above its tab a part selector holding each of the multi's parts, numbered from 1
  with the program it plays, and "All parts"; WHEN the person chooses a part, the tab SHALL show that part's values; WHEN all parts are chosen,
  a change SHALL go to every part and a value the parts do not share SHALL read "VARIES", as for the keygroups (RQ-APP-058, RQ-APP-059).
- **Rationale**: a multi has 32 to 128 parts: one at a time, as the program editor shows one keygroup; the program in the selector says which
  parts are used.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* LIVE of 32 parts with PAD on part 2, *When* it is shown, *Then* the selector holds parts 1 to 32,
  part 2 reading PAD, and "All parts"; *When* the person chooses part 2, *Then* part 2's values are read; *When* all parts are chosen and the
  person mutes, *Then* every part is muted.
- **Dependencies**: RQ-APP-058, RQ-APP-059, RQ-APP-100

### RQ-APP-102: The program of a part
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person chooses a program of memory for the part shown, the application SHALL make the part play it and read it back;
  WHEN the person chooses no program, it SHALL take the program away from the part, with no question (the program stays in memory); IF all
  parts are chosen, THEN it SHALL refuse a change of program with nothing sent and say to choose one part.
- **Rationale**: RQ-APP-063's "adding and clearing parts": the parts of a multi are fixed in number; a part is used when it plays a program.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* LIVE's part 2 playing nothing, *When* the person chooses PAD, *Then* it is sent and part 2 reads PAD
  in the tab and the selector; *When* the person chooses no program, *Then* part 2 plays nothing and PAD is still in memory; *Given* all parts
  chosen, *Then* a change of program is refused with nothing sent.
- **Dependencies**: RQ-APP-100, RQ-APP-101, RQ-AKM-092

### RQ-APP-103: The multi's program number
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a multi is shown, the editor SHALL read its program number (OFF or 1 to 128, as on the front panel); WHEN the person sets
  it, the application SHALL send it and read it back.
- **Rationale**: the program change that recalls the multi (MULTI mode).
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* LIVE with no program number, *When* it is shown, *Then* it reads OFF; *When* the person sets 5,
  *Then* it is sent and reads 5.
- **Dependencies**: RQ-APP-100, RQ-AKM-091

### RQ-APP-104: Creating a multi from the memory view
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person asks the memory view for a new multi and gives its name, the application SHALL create it on the sampler, with the
  number of parts the sampler gives new multis, and SHALL list it, current and selected; IF another multi bears the name (without regard to case,
  spaces or hyphens), THEN it SHALL refuse it with nothing sent.
- **Rationale**: RQ-APP-063 (RQ-AKM-087); as the program's creation (RQ-APP-038).
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* the multi M1 in memory, *When* the person creates LIVE, *Then* LIVE is created, listed and
  selected, and its editor shown; *When* the person creates m-1, *Then* nothing is sent and the reason is shown.
- **Dependencies**: RQ-APP-035, RQ-APP-037, RQ-APP-038, RQ-AKM-087

---

## Non-Functional Requirements

The rules of FTR-APP-001 and FTR-APP-007 apply: every visual value from the design tokens (RQ-APP-002), the layout as data (RQ-APP-049), the
mockup drawn first (RQ-APP-074).

---

## Traceability

| Requirement | Replaces | AKM | MCP |
|---|---|---|---|
| RQ-APP-100 | RQ-APP-063 | RQ-AKM-089, RQ-AKM-090 | RQ-MCP-021 |
| RQ-APP-101 | RQ-APP-063 | RQ-AKM-090 | RQ-MCP-021 |
| RQ-APP-102 | RQ-APP-063 | RQ-AKM-092 | RQ-MCP-038 |
| RQ-APP-103 | RQ-APP-063 | RQ-AKM-091 | RQ-MCP-038 |
| RQ-APP-104 | RQ-APP-063 | RQ-AKM-087 | RQ-MCP-037 |
