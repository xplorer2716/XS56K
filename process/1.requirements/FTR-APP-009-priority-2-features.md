# FTR-APP-009: Priority-2 Features (Nice to Have)

## Overview

Session APP (2026-10-13). The owner classified the editor's needs in two priorities: priority 1, mandatory, and priority 2, nice to have.
This feature collects the priority-2 needs agreed on 2026-10-13, one requirement each, written at the level of a feature: each becomes a
feature of its own, with detailed requirements, when the owner promotes it. Each is written `Could`.

**Out of scope, not priority 2.** Recording a sample: the SysEx specification has no record command (no section and no item for it); the
sampler's RECORD pages could only be driven blind through the front-panel keys (§20). The owner left it aside on 2026-10-13.

**Depends on** FTR-APP-001 to FTR-APP-008 and the AKM primitives named in each requirement.

**Sources.** `documents/_index/sysex_spec.kb.md` (sections, Table 4), `documents/_index/sysex_spec.items.tsv`, `juce/akm/include/akm/*.hpp`,
`juce/mcp/README.md` (what the MCP server already does with the same primitives), `documents/akai_s5000_s6000_user_manual.1.21.pdf.md`.

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the person editing the sampler.

---

## Functional Requirements

### RQ-APP-062: Sample editor
- **Superseded**: 2026-10-15 by FTR-APP-010 (RQ-APP-086 to RQ-APP-089), brought to priority 1 at the owner's request; the audition stays in the memory view (RQ-APP-041, RQ-APP-083).
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a sample is selected in the memory view, the edit panel SHALL show a sample editor with its playback parameters (start, end, loop start, loop end, playback mode, original pitch, semitone and fine tune, RQ-AKM-048), its read-only information (RAM or virtual, channels, length, sample rate, RQ-AKM-049) and its audition, under the rules of FTR-APP-007.
- **Rationale**: the sampler's EDIT SAMPLE mode, as far as the SysEx specification reaches: it has no command to trim, chop, join, mix, stretch or resample a sample, and none that transfers its audio, so no waveform can be drawn.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* the sample KICK selected, *When* the sample editor is shown, *Then* its playback parameters and information are read from the sampler; *When* the person sets its loop end, *Then* it is sent and read back.
- **Dependencies**: RQ-APP-001, RQ-APP-048, RQ-AKM-045, RQ-AKM-048, RQ-AKM-049

### RQ-APP-063: Multi editor
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a multi is selected in the memory view, the edit panel SHALL show a multi editor with its parts (the program each plays and the part's parameters, RQ-AKM-089, RQ-AKM-090, RQ-AKM-092), the parts' mute and solo, the multi's program number, the adding and clearing of parts; the memory view SHALL let the person create a multi (RQ-AKM-087).
- **Rationale**: the sampler's MULTI mode.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* the multi LIVE selected, *When* the multi editor is shown, *Then* its parts and the program of each are read from the sampler; *When* the person assigns PAD to part 2, *Then* it is sent and read back.
- **Dependencies**: RQ-APP-001, RQ-APP-048, RQ-AKM-087, RQ-AKM-089, RQ-AKM-090, RQ-AKM-092

### RQ-APP-064: Copy, paste and move files
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person copies a file of the browser and pastes it into another folder, the application SHALL load the file (RQ-AKM-066), open the destination folder and save the loaded item there (RQ-AKM-067), and, for a move, then delete the source file after the confirmation of RQ-APP-011; it SHALL say beforehand that the copy goes through the sampler's memory and refuse a file whose name is already held in memory.
- **Rationale**: the SysEx specification has no copy and no move of a file (section 10), so they can only be built from a load, a save and a delete, for the six kinds of items the sampler saves.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* KICK.WAV in DRUMS and no sample KICK in memory, *When* the person copies it into LOOPS, *Then* the file is loaded, saved into LOOPS, and DRUMS still holds KICK.WAV; *Given* a sample KICK already in memory, *Then* the copy is refused with nothing sent.
- **Dependencies**: RQ-APP-001, RQ-APP-011, RQ-APP-028, RQ-APP-043, RQ-AKM-065, RQ-AKM-066, RQ-AKM-067

### RQ-APP-065: Song files, set lists and scenelists
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person selects, renames or deletes a song file or a scenelist of the memory view, or edits the set lists, the application SHALL send the matching commands (RQ-AKM-082, RQ-AKM-084, RQ-AKM-095), deletions after the confirmation of RQ-APP-011.
- **Rationale**: the six kinds are listed in priority 1 (RQ-APP-035), but only programs, samples and multis can be changed there.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* the song file SONG 1, *When* the person renames it SONG A, *Then* it is renamed and listed as SONG A.
- **Dependencies**: RQ-APP-001, RQ-APP-035, RQ-AKM-082, RQ-AKM-084, RQ-AKM-095

### RQ-APP-066: Delete all of a kind, clear the memory
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person asks to delete all the programs, all the samples or all the multis, or to clear the sampler's memory, the application SHALL ask for a confirmation that states how many items will be lost, then send the command (RQ-AKM-025, RQ-AKM-046, RQ-AKM-056, RQ-AKM-088).
- **Rationale**: the sampler offers them (CLEAR MEMORY, user manual p. 36); they lose everything not saved.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* 12 samples in memory, *When* the person asks to delete all samples, *Then* the confirmation says 12 samples will be lost, and nothing is sent unless the person confirms.
- **Dependencies**: RQ-APP-001, RQ-APP-011, RQ-AKM-025, RQ-AKM-046, RQ-AKM-056, RQ-AKM-088

### RQ-APP-067: Renaming an item as it is saved
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person gives a new name to the item being saved, the application SHALL save it under that name, the way the sampler's SAVE page allows (user manual p. 217).
- **Rationale**: saving an edited copy keeps the original on the disk; the SysEx save (`&2C`) takes no name, so how to do it is a design decision to take before this requirement is detailed.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* the program PAD, *When* the person saves it as PAD 2, *Then* the folder holds PAD 2.AKP.
- **Dependencies**: RQ-APP-001, RQ-APP-043

### RQ-APP-068: Ejecting a disk
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person ejects a removable disk, the application SHALL ask whether to fail if the disk is in use or to discard the virtual samples that play from it, then eject it (RQ-AKM-069) and remove it from the browser.
- **Rationale**: the spec's `&0D` has these two modes.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* a removable disk, *When* the person ejects it with "fail if in use", *Then* the eject is sent with that mode.
- **Dependencies**: RQ-APP-001, RQ-APP-011, RQ-APP-024, RQ-AKM-069

### RQ-APP-069: Effects board
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person opens the effects, the application SHALL show the board's layout, the modules' types and states, the channels' mutes and the modules' parameters, and send their changes (RQ-AKM-099 to RQ-AKM-101).
- **Rationale**: the sampler's FX mode (user manual pp. 186-215); the board is optional on the S5000.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler with an effects board, *When* the person opens the effects, *Then* the layout and modules are read from it; *Given* no board, *Then* the application says there is none.
- **Dependencies**: RQ-APP-001, RQ-AKM-099, RQ-AKM-100, RQ-AKM-101

### RQ-APP-070: The sampler's own settings and MIDI setup
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person opens the sampler's settings, the application SHALL show and send its name, clock and date, play mode and front-panel lock (RQ-AKM-052, RQ-AKM-054, RQ-AKM-055) and its MIDI setup switches and filters (RQ-AKM-078, RQ-AKM-079).
- **Rationale**: the sampler's UTILITIES pages (user manual pp. 219-224); not to be confused with the application's own MIDI setup (RQ-APP-015).
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* the sampler named S5000, *When* the person renames it STUDIO, *Then* it is sent and read back.
- **Dependencies**: RQ-APP-001, RQ-AKM-052, RQ-AKM-054, RQ-AKM-055, RQ-AKM-078, RQ-AKM-079

### RQ-APP-071: Front-panel keys
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person presses a key of an on-screen copy of the sampler's front panel, the application SHALL send its hold and release (RQ-AKM-073, RQ-AKM-074), and SHALL release every held key when the session closes (RQ-AKM-075).
- **Rationale**: reaches the sampler's functions that no SysEx item gives; the sampler's screen cannot be read back, so the person must watch it.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* the on-screen panel, *When* the person clicks EDIT PROGRAM, *Then* its hold then its release are sent.
- **Dependencies**: RQ-APP-001, RQ-AKM-073, RQ-AKM-074, RQ-AKM-075

### RQ-APP-072: How the sampler's screen follows the application
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person chooses in the application's settings whether the sampler's screen follows the items the application selects or stays independent, the application SHALL open its sessions with the matching section 00 setting (RQ-AKM-040), as the MCP server's `--screen` option does (DEC-MCP-027).
- **Rationale**: with its screen synchronised, the sampler shows what the application edits; the specification recommends leaving it off except when needed.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* "follow" chosen, *When* the session opens, *Then* the screen-synchronisation setting is switched on.
- **Dependencies**: RQ-APP-001, RQ-APP-016, RQ-AKM-040

### RQ-APP-073: Curves drawn and edited with the mouse
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person edits a program, the application SHALL draw the curves of the envelopes, of the LFOs and of the filter from the values read from the sampler, and WHEN the person drags a point of a curve, it SHALL send the matching parameters' values under the rules of FTR-APP-007 (sent, read back, one write pending per parameter).
- **Rationale**: the owner's choice of 2026-10-13, on the reference of the plugin Vital: curves editable with the mouse, priority 2.
- **Priority**: Could
- **Acceptance Criteria** (Gherkin): *Given* an amplitude envelope with attack 10, *When* the person drags the attack point to the right, *Then* the attack is set to a greater value and read back, and the curve is redrawn from the value read.
- **Dependencies**: RQ-APP-001, RQ-APP-052, RQ-APP-053, RQ-APP-054, RQ-AKM-030
