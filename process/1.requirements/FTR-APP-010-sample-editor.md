# FTR-APP-010: Sample Editor — the "Sample" Tab of the Edit Panel

## Overview

Session APP (2026-10-15). The owner's request: the first editor of the edit panel (FTR-APP-007) is the sample's. When a sample is selected in the
memory view (it becomes the sampler's current sample, RQ-APP-037), a tab "Sample" edits its settable parameters. This brings RQ-APP-062 (priority 2)
forward to priority 1 and replaces it.

What section 0E of the SysEx specification allows (Tables 18 and 19): eight settable parameters (start, end, loop start, loop end, playback mode,
original pitch, semitone tune, fine tune; Set `&20`-`&2A`, Get `&40`-`&4B`) and four read-only ones (RAM or virtual, channels, length, rate;
`&30`-`&34`). No command trims, records or transfers the audio, so no waveform is drawn; the loop crossfade (XFADE LENGTH, XFADE CURVE) is a process of the
front panel with no SysEx item: a later "remote control" tab will drive the front panel for it (owner, 2026-10-15). On the real S5000, Set Loop End moves Loop Start
(`OBSERVATIONS-RQ-AKM-051-sample-loop-points.md`).

**Depends on** FTR-APP-001 (design system), FTR-APP-005 (memory view), FTR-APP-007 (RQ-APP-048 to RQ-APP-055), RQ-AKM-045, RQ-AKM-048, RQ-AKM-049,
RQ-MCP-020 (the gateway's sample parameters).

**Sources.** `documents/_index/sysex_spec.items.tsv` (section 0E), `documents/akai_s5000_s6000_user_manual.1.21.pdf.md` (EDIT SAMPLE, pp. 141-177),
`juce/mcp/src/StandardParameters.cpp` (`sampleParameters()`).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the person editing a sample; the program editor (FTR-APP-008), which reuses the edit panel built here.

---

## Functional Requirements

### RQ-APP-086: The "Sample" tab
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a sample is selected in the memory view, the edit panel SHALL show the sample editor with one tab, "Sample", holding three groups: Playback (start, end, playback mode), Loop (loop start, loop end), Pitch (original pitch, semitone tune, fine tune), each parameter under the sample catalogue's name and edited under the rules of RQ-APP-051 to RQ-APP-054; the four positions SHALL each be a value box to type the exact point with a slider over the sample's length under it, and the original pitch SHALL read as a note name (60 = C3).
- **Rationale**: the owner's request of 2026-10-15; the grouping is the assistant's proposal, data the owner can change (DEC-APP-006); sliders because the editor is for musicians first (owner, 2026-10-15).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the sample KICK selected, *When* the panel shows its editor, *Then* the tab "Sample" shows the eight parameters in their three groups with the values read from the sampler; *Given* an original pitch of 60, *Then* it reads "C3"; *When* the person sets the fine tune to 10, *Then* it is sent, read back and shown.
- **Dependencies**: RQ-APP-048, RQ-APP-049, RQ-APP-051 to RQ-APP-054, RQ-AKM-048, ADR-APP-004

### RQ-APP-087: What the sample is, above the tab
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The sample editor SHALL show above its tab the sample's name and its read-only information, read from the sampler with its parameters: RAM or virtual, mono or stereo, length in sample points and sample rate in Hz.
- **Rationale**: RQ-APP-050 (the context in sight); the length bounds the positions (RQ-APP-088).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* KICK, a RAM mono sample of 22 050 points at 44 100 Hz, *When* it is shown, *Then* the header reads its name and "RAM, mono, 22050 points, 44100 Hz".
- **Dependencies**: RQ-APP-050, RQ-APP-086, RQ-AKM-049

### RQ-APP-088: The positions stay within the sample
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF the person gives a start, end, loop start or loop end beyond the sample's length, THEN the application SHALL refuse it with nothing sent and show the range from 0 to the length.
- **Rationale**: the catalogue's range of a position is the sampler's whole word, far beyond a sample; RQ-APP-051 refuses out-of-range values.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* KICK of 22 050 points, *When* the person sets its end to 30 000, *Then* nothing is sent and "0 to 22050" is shown.
- **Dependencies**: RQ-APP-051, RQ-APP-087

### RQ-APP-089: Every value of the tab read back after a change
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a change of a sample parameter has been sent and read back, the application SHALL read again every parameter of the tab and show what the sampler holds.
- **Rationale**: on the real S5000 Set Loop End moves Loop Start (OBSERVATIONS-RQ-AKM-051); the sampler may adjust other values too.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler that moves the loop start when the loop end is set, *When* the person sets the loop end, *Then* the loop start shown is the one the sampler then holds.
- **Dependencies**: RQ-APP-052, RQ-APP-086
