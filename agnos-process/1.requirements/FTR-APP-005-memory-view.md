# FTR-APP-005: Memory View — the Items Loaded in the Sampler, by Type

## Overview

Session APP (2026-10-13). Under the file browser (RQ-APP-006) the owner wants an area that shows, by type, what is loaded in the sampler's
memory: samples, programs, and the rest. Selecting an item there shows its editor in the edit panel (FTR-APP-007); it is also where an item is
chosen to be saved (FTR-APP-006), as on the sampler's SAVE page, which "shows the current contents of memory" (user manual p. 216).

The sampler holds six kinds of items that the SysEx specification lists and saves (section 10, `&2C`: 1 multi, 2 program, 3 sample, 4 song
file, 5 set list, 6 scenelist). The sampler has **one current item per kind**: selecting an item makes it current on the sampler (`&05`
select by name in sections 0A, 0C, 0E, 14, 16), which is what its editor then reads and writes. Set lists have no current item
(`juce/akm/include/akm/SongPrimitives.hpp`).

**Priorities.** Priority 1 (mandatory) is written `Must`, priority 2 (nice to have) `Could` (owner, 2026-10-13). In priority 1 only
programs have an editor (FTR-APP-008); samples and multis have theirs in priority 2, and so do the rename and delete of song files, set lists
and scenelists, the creation of a multi, and "delete all" and "clear memory" (FTR-APP-009).

**Depends on** FTR-APP-001 to FTR-APP-003; FTR-AKM-002, FTR-AKM-005, FTR-AKM-006, FTR-AKM-010, FTR-AKM-011, FTR-AKM-012.

**Sources.** `documents/_index/sysex_spec.items.tsv` (sections 02, 0A, 0C, 0E, 10, 14, 16), `documents/akai_s5000_s6000_user_manual.1.21.pdf.md`
(SAVE p. 216; PROGRAM LIST p. 129; SAMPLE LIST p. 175; MULTI LIST p. 55), `juce/akm/include/akm/*.hpp`.

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the person editing the sampler; the edit panel (FTR-APP-007) and the save (FTR-APP-006).

---

## Functional Requirements

### RQ-APP-035: The items in memory, by type
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the session opens, the memory view SHALL list the items in the sampler's memory under six headings — multis, programs, samples, song files, set lists, scenelists — each heading with its count, each item by its name in the sampler's order, the current item of each kind marked.
- **Rationale**: the owner's description of the memory view; the six kinds are those the sampler saves.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler holding the programs PAD and BASS with BASS current, and the sample KICK, *When* the session opens, *Then* "Programs (2)" lists PAD and BASS with BASS marked current, "Samples (1)" lists KICK, and the four other headings show 0.
- **Dependencies**: RQ-APP-001, RQ-APP-017, RQ-AKM-023, RQ-AKM-047, RQ-AKM-083, RQ-AKM-084, RQ-AKM-091, RQ-AKM-096

### RQ-APP-036: The view follows what changes the memory
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the application has loaded, created, renamed or deleted an item, the memory view SHALL read again the lists that the action can change and show them; WHEN the person asks to refresh the memory view, it SHALL read all six lists again.
- **Rationale**: a load with dependents adds items of several kinds; the sampler's own front panel can change the memory without the application knowing.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a program loaded with its samples, *When* the load is done, *Then* the programs and the samples are read again and shown; *Given* a sample deleted on the sampler's front panel, *When* the person refreshes, *Then* it is no longer listed.
- **Dependencies**: RQ-APP-001, RQ-APP-035

### RQ-APP-037: Selecting an item opens it
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person selects an item in the memory view, the application SHALL make it the sampler's current item of its kind (select by name) and show it in the edit panel (RQ-APP-048); IF the select fails, THEN the selection SHALL go back to the sampler's current item and the error SHALL be reported (RQ-APP-010).
- **Rationale**: the editor reads and writes the sampler's current item; the owner's working principle.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* PAD and BASS with BASS current, *When* the person selects PAD, *Then* PAD is selected by name on the sampler, marked current, and the edit panel shows the program PAD.
- **Dependencies**: RQ-APP-001, RQ-APP-035, RQ-APP-048, RQ-AKM-021, RQ-AKM-045, RQ-AKM-082, RQ-AKM-087, RQ-AKM-095

### RQ-APP-038: Creating a program
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person creates a program with a name and a number of keygroups, the application SHALL create it in the sampler's memory (RQ-AKM-021), list it, select it and open it in the edit panel; IF a program of that name exists, THEN it SHALL refuse with nothing sent.
- **Rationale**: a program is the item edited in priority 1; a new one is needed to start from nothing.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* no program LEAD, *When* the person creates LEAD with 2 keygroups, *Then* LEAD is listed, current and open with 2 keygroups; *When* the person creates PAD which exists, *Then* nothing is sent.
- **Dependencies**: RQ-APP-001, RQ-APP-037, RQ-AKM-021, RQ-AKM-022

### RQ-APP-039: Renaming a program, a sample or a multi
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person renames a program, a sample or a multi, the application SHALL select it, rename it (RQ-AKM-022, RQ-AKM-045, RQ-AKM-092) and show the new name; IF another item of the same kind has that name, THEN it SHALL refuse with nothing sent.
- **Rationale**: names identify items on the sampler and on the disk; two items of a kind with one name cannot be told apart by name.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the samples KICK and SNARE, *When* the person renames KICK as KICK 2, *Then* the list shows KICK 2 and SNARE; *When* the person renames KICK as SNARE, *Then* nothing is sent.
- **Dependencies**: RQ-APP-001, RQ-APP-037, RQ-AKM-022, RQ-AKM-045, RQ-AKM-092

### RQ-APP-040: Deleting a program, a sample or a multi
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person deletes a program, a sample or a multi, the application SHALL ask for the confirmation of RQ-APP-011 naming the item, then select it, delete it (RQ-AKM-021, RQ-AKM-045, RQ-AKM-087) and list the kind again.
- **Rationale**: a deleted item not saved to a disk is lost.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the program PAD, *When* the person deletes it and confirms, *Then* PAD is no longer listed; *When* the person cancels, *Then* nothing is sent.
- **Dependencies**: RQ-APP-001, RQ-APP-011, RQ-APP-037, RQ-AKM-021, RQ-AKM-045, RQ-AKM-087

### RQ-APP-041: Auditioning a sample in memory
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person auditions a sample of the memory view, the application SHALL select it and start its audition (RQ-AKM-045), and SHALL stop it WHEN the person stops it or selects another item.
- **Rationale**: the person finds a sample by ear before assigning it to a zone.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* KICK selected, *When* the person auditions it, *Then* the audition starts; *When* the person selects SNARE, *Then* the audition of KICK stops.
- **Dependencies**: RQ-APP-001, RQ-APP-037, RQ-AKM-045

### RQ-APP-042: The free memory
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The memory view SHALL show the sampler's free and total sample (wave) memory in the unit of RQ-APP-081 and the free percentage of the memory for multis, programs, keygroups and samples (RQ-AKM-053), read again with the lists (RQ-APP-036).
- **Rationale**: the person checks the room left before loading a large sample.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler with 8 MB of wave memory of which 2 MB free, *When* the session opens, *Then* the view shows 2 MB free of 8 MB and the free percentage; *When* a sample is loaded, *Then* the figures are read again.
- **Dependencies**: RQ-APP-001, RQ-APP-035, RQ-APP-036, RQ-APP-081, RQ-AKM-053
- **Amended**: 2026-10-15, sizes in kilobytes or megabytes (RQ-APP-081, TASK-APP-020).
