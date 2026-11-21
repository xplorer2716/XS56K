# PLAN-APP-006: Editor Application — Roadmap of the Program Editor

## Overview

Session APP (2026-10-15). The reference path to a program editor that edits a whole program (FTR-APP-008), one tab at a time, each tab usable on
the real S5000 when it is done. Two orders, independent of each other:

- **The order of implementation** (the tasks below): each tab brings at most one new piece of the editor, so that each step is small and tested.
  The program's own parameters first (nothing to choose), then the keygroup selector, then the four zones, then the most complex control.
- **The order of the tabs on screen** (for the musician): the order in which a program is built. Since a layout places its tabs as it likes
  (DEC-APP-024), a tab built fourth can be shown first.

| Implementation | Tab | Scope | New piece |
|---|---|---|---|
| 1 | Program | program | the program editor itself: the program chosen by name, the program-scope parameters of `ParameterCatalogue::standard()` (DEC-APP-030) |
| 2 | Envelopes | keygroup | the keygroup selector (RQ-APP-058); each group says its scope (RQ-APP-056); the envelope graph; Amp, Filter and Aux envelopes a row each |
| 3 | Filter | keygroup | none: a layout |
| 4 | Keygroup | keygroup and zones | the parameters of the four zones; a sample chosen among those in memory (RQ-APP-060); orders note and velocity (RQ-APP-092) |
| 5 | (in Keygroup) | program | adding and deleting keygroups (RQ-APP-061) |
| 6 | LFO (named Mod sources until 2026-10-16) | program | none: LFO 1 and LFO 2 |
| 7 | Modulation | keygroup | the routings of the modulation sources, source to destination |
| 8 | (selector) | keygroup | "all keygroups": a state "the values differ" in the controls (RQ-APP-059, Could) |
| 9 | (fixes) | — | the owner's run of 2026-10-17: steps (TASK-APP-052), the arrows of a choice (TASK-APP-053), the plus of the enhanced filters (TASK-APP-054), done before 8 |

**The tabs on screen** (the assistant's proposal; the owner decides, TASK-APP-002):

| Screen | Tab | Holds |
|---|---|---|
| 1 | Keygroup | the key span, the four zones: sample, level, pan, tune, velocity span, playback, mute and solo |
| 2 | Envelopes | Amp (with the keygroup level), Filter and Aux envelopes, a row each |
| 3 | Filter | mode, cutoff, resonance, keyboard track, attenuation |
| 4 | LFO | LFO 1, LFO 2 |
| 5 | Modulation | the routings of the sources |
| 6 | Program | output, tuning, pitch bend, portamento: set once |

The three envelopes (Amp, Filter, Aux) are graphs moved by the mouse, over their knobs (owner, 2026-10-16; DEC-APP-031). A zone's sample could open the sample editor.

## References
- **Requirements**: RQ-APP-056 to RQ-APP-061 (FTR-APP-008); RQ-APP-048 to RQ-APP-055, RQ-APP-091, RQ-APP-092, RQ-APP-097, RQ-APP-098 (FTR-APP-007); RQ-APP-093 to RQ-APP-096, RQ-APP-099 (FTR-APP-008); RQ-AKM-022 to
  RQ-AKM-035
- **ADRs**: ADR-APP-004 (DEC-APP-024 to DEC-APP-037); ADR-APP-001

This plan implements the tasks in the format specified below.
---

## Tasks

Each task starts with its requirements and, where a new piece needs one, a decision in ADR-APP-004 or a new ADR; its layout is the assistant's
proposal until the owner decides (TASK-APP-002). Every task: core tests on the simulated sampler first, then a run on the real S5000.

### TASK-APP-043: The Program tab
- **Tier**: M
- **Status**: Done
- **Description**: The program editor opens on a program selected in the memory view, chosen on the sampler by name; its first tab holds the
  program-scope parameters: Output, Tuning, Pitch bend, Portamento; the header gives the keygroup count. The S5000's SysEx has no MIDI
  setting of a program (§0A), so none is shown.
- **Requirement refs**: RQ-APP-093 (new), RQ-APP-048 (amended), RQ-APP-056, RQ-APP-057
- **ADR refs**: ADR-APP-004 (DEC-APP-030)
- **Acceptance Criteria** (Gherkin): *Given* the program PAD selected, *Then* the Program tab shows its values read from the sampler; *When* the
  person changes one, *Then* it is sent and read back.
- **Dependencies**: TASK-APP-042
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-15: `layouts/program.json`, `screens/program-editor.json`, the mockups
  `program-editor-connected.svg` and `-writing.svg` generated and looked at (a long template name overflows its choice in the mockup). Two new
  cases of `EditTests.cpp` (PAD's fourteen values, "2 keygroups", legato "ON"; loudness 70 and legato typed "off" sent and read back) failed
  first to compile, then passed; the "no editor" case moved to a multi; 71 app cases passed; `ctest -C Debug`: 1215 passed; the editor built
  warning-clean. Real run by the owner (2026-10-15): the label Legato renamed "Mono legato", as the sampler's screen says. Open: a change of
  the tune template or of the key changes nothing on the sampler's screen, and "Read again" gives back the value the editor set, not the
  screen's (the MCP real run of PLAN-MCP-002 read back every value set). Diagnosed with the owner (2026-10-16): the S5000 applies the
  template and the key sent by SysEx (the per-key graphic of MIDI/TUNE shows the new tuning, and it is heard), but its fields TEMPLATE and KEY
  keep showing USER and C, even after leaving the page; the Get gives back the value sent. A firmware behaviour, nothing changed in the editor.
  A template changed on the front panel is read back by "Read again"; the other twelve parameters work (owner, 2026-10-16).
- **Assumptions**: (1) The edit controller generalises from the sample catalogue to a catalogue chosen by the layout.

### TASK-APP-044: The Envelopes tab and the keygroup selector
- **Tier**: M
- **Status**: In Progress
- **Description**: The keygroup selector (1 to the keygroup count); the Envelopes tab: the Amp, Filter and Aux envelopes a row each, each drawn
  as a graph that the mouse moves (owner, 2026-10-16), with its other parameters beside it; each group shows its scope.
- **Requirement refs**: RQ-APP-094, RQ-APP-095 (new), RQ-APP-056, RQ-APP-058
- **ADR refs**: ADR-APP-004 (DEC-APP-030, DEC-APP-031)
- **Acceptance Criteria** (Gherkin): RQ-APP-058's, RQ-APP-094's and RQ-APP-095's; the mockup validated by the owner first.
- **Dependencies**: TASK-APP-043
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-16. Mockup first: the Amp tab, then at the owner's requests the three envelopes in one tab
  (a row each, `row` in the layout) and a colour per envelope (new role `amp`, blue); `screens/envelopes-editor.json` and its two mockups looked
  at; generator tests written first and failing (the envelope stages and the scope generated, a name outside its group or an unknown scope
  refused, a row started; the graph drawn from the values with its breakpoints and the scope, a row drawn under the first): 21 design and 19
  mockup tests pass. Core: five cases (attacks 10 and 40 on two keygroups, the second chosen and set to 50 alone; keygroups 0 and 3 refused;
  the Program tab shown and shown again with nothing read; every program group's scope against the catalogue) failed first to compile, then
  passed; 75 app cases; `ctest -C Debug`: 1219 passed; the editor built warning-clean. Real S5000 (OS 2.14) through `uia.ps1`: "New Program 1"
  opened on Envelopes, "Keygroup 1 of 1", the three rows in their colours, Amp 1 / 50 / 100 / 15 read, the pane scrolling (captured). The drag
  of a breakpoint, a program of several keygroups and the tab clicks: the owner's to try. Owner, 2026-10-16: the envelopes, their graphs and
  the tabs work; the velocity, depth and key parameters are still to be found and checked on the sampler's screens.
- **Assumptions**: —

### TASK-APP-045: The Filter tab
- **Tier**: M (re-tiered from S: a new screen description and its two mockups)
- **Status**: Done
- **Description**: Mode, cutoff, resonance, keyboard track, attenuation (its envelope is in the Envelopes tab): a layout, no code expected.
- **Requirement refs**: RQ-APP-057, RQ-APP-058
- **ADR refs**: ADR-APP-004 (DEC-APP-024)
- **Acceptance Criteria** (Gherkin): *Given* keygroup 2 chosen, *Then* the Filter tab shows keygroup 2's filter.
- **Dependencies**: TASK-APP-044
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-16. Mockup first: `screens/filter-editor.json`, `filter-editor-connected.svg` and `-writing.svg`
  looked at; the labels Resonance, Key track and Attenuation overflowed their knobs, shortened to Reso, Track, Atten; validated by the owner.
  Core case (cutoffs 50 and 70 on two keygroups, keygroup 2 chosen, the Filter tab shows 70, five values, the group KEYGROUP) and the Program
  tab moved to index 2: failed against the header generated before the tab (4 cases), then passed once regenerated; 76 app cases. Build
  warning-clean; `ctest -C Debug` from git-bash: 1223 passed (from PowerShell, `bld_publish_deployment_script_tests` fails: no `bash` nor
  `sed` on its PATH, unrelated to this task). `tidy.py`: no production C++ source changed. Real S5000: the owner's to try.
- **Assumptions**: (1) The Filter tab sits between Envelopes and Program, as in the proposed screen order.

### TASK-APP-051: The filter type drawn as its shape
- **Tier**: M
- **Status**: Done
- **Description**: The filter type's control draws the response of the chosen type (level against frequency), with its previous and next
  arrows, in the tokens' colours. The 26 shapes are design data (normalised polylines): 19 from the user manual (EDIT PROGRAM, FILTER MODE,
  pp. 114-118); LO<>HI, LO<>BAND, BAND<>HI, BI-NOTCH, BI-PEAK, BI-PHASE and VOWELISER have no drawing there, the owner gives them, and until
  then they show their name only.
- **Requirement refs**: RQ-APP-096 (new: the filter type is seen by its shape), RQ-APP-057
- **ADR refs**: ADR-APP-004 (DEC-APP-032, new: the shapes as data, the drawn choice)
- **Acceptance Criteria** (Gherkin): *Given* the type 2-POLE LP, *Then* its shape is drawn; *When* the person moves to 4-POLE LP, *Then* it is
  sent, read back and its shape drawn; *Given* a type with no shape yet, *Then* its name is shown. The mockup validated by the owner first.
- **Dependencies**: TASK-APP-045
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-16. RQ-APP-096 and DEC-APP-032 written first. `shapes/filter-types.json`: the 19 drawings read
  from the PDF's vector paths (PyMuPDF), then idealised (x not decreasing, levels rounded). Tests first and failing: generator 4 (the set and
  its polylines in order, the choice pointing to them; an unknown set or shapes on a knob refused; a polyline going back, leaving the square
  or of one point refused, naming its label; the folder read by name), mockups 3 (the 2-POLE LP shape in the filter colour above the choice;
  VOWELISER its name alone; an unknown set refused), core 1 (19 shapes, each a catalogue label; 2-POLE LP ends at 1, 0; VOWELISER none) failed
  to compile; then 25 design, 21 mockup tests and the layout cases pass. Every parameter row now carries its shapes (`{}` when none) so that no
  compiler warns of a missing initialiser: two expectations of an existing generator test follow. Round trip (4-POLE LP set, read back,
  shown, its shape found) passed at once, the write being generic. Mockup validated by the owner; the choice under a shape is a value box high,
  in the mockup and the cell. Build warning-clean; 78 app cases; `ctest -C Debug`: 1225 passed; `tidy.py`: 14 sources, 0 findings; the IDE's
  SonarQube: nothing on the changed files. Not verified: the cell drawn by JUCE (the owner's run), GCC and Apple Clang (CI).
- **Assumptions**: (1) Owner, 2026-10-16: "pour le type de filtre j'aimerais avoir un contrôle graphique qui montre la forme du filtre". (2) The
  seven shapes the manual does not draw are left for a later pass with the owner's drawings (owner, 2026-10-16). (3) The '+' types keep the
  shape of their base type, as the manual draws them.

### TASK-APP-046: The Keygroup tab and its four zones
- **Tier**: M
- **Status**: Done
- **Description**: The key span and, for each of the four zones, sample, level, pan, tune, velocity span, playback, mute and solo; the sample chosen
  among the samples in memory; low note before high note, low velocity before high velocity (orders).
- **Requirement refs**: RQ-APP-057, RQ-APP-060, RQ-APP-092
- **ADR refs**: ADR-APP-004 (DEC-APP-033, new)
- **Acceptance Criteria** (Gherkin): RQ-APP-060's.
- **Dependencies**: TASK-APP-044
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-16. The owner chose the zones in columns, as the S5000's ZONE page. Generator tests first and
  failing: design 3 (a zone grid's columns and the `sample` control generated; a group that is none has 0; columns outside a zone scope, a knob
  in a grid, a sample outside one, five columns refused, naming them), mockups 2 (each zone a column under its number, each parameter a row
  labelled once; a zone with no sample reads NONE); then 28 design and 23 mockup tests pass. Mockup: the crossfade group overlapped its scope
  and the grid ran under the tabs, so the crossfade moved beside the grid as "Xfade"; validated by the owner. Core first and failing to
  compile: five cases (zone 1 KICK, zones 2 to 4 none, zone 2's level -3 in its column; SNARE assigned to zone 2 by name and read back; zone 3's
  level set alone; zone 1's low velocity 70 refused above its high 63 while zone 2's is accepted; no sample refused with nothing sent); the
  first fourth case failed on a fixture that left zone 2's high velocity at 0, given 127 as its title says; the Keygroup tab now opens first,
  so the tab indices of the earlier cases moved and the Envelopes cases show their tab; the layout case skips the `sample` control, which no
  catalogue row backs. 83 app cases pass. Build warning-clean; `ctest -C Debug`: 1230 passed; `tidy.py`: 14 sources, 0 findings (`rebuild`
  over the cognitive complexity threshold at first: its cells moved into `addCells`); the IDE's SonarQube: nothing. Not verified: the grid
  drawn by JUCE and the real S5000 (the owner's run), GCC and Apple Clang (CI).
- **Assumptions**: (1) The zones are a column each (owner, 2026-10-16), replacing the leading-argument idea. (2) In the grid the numbers are value
  boxes and the rest choices, so that fourteen rows fit. (3) The Keygroup tab is first, as in the proposed screen order (TASK-APP-002 decides).
  (4) The gateway cannot remove a zone's sample: NONE is shown and refused (RQ-APP-060's "or no sample" stays open).

### TASK-APP-047: Adding and deleting keygroups
- **Tier**: M (re-tiered from S at the session's start: a controller action and its tests)
- **Status**: Done
- **Description**: In the Keygroup tab, add keygroups and delete the current one after confirmation; the last one cannot be deleted.
- **Requirement refs**: RQ-APP-061, RQ-APP-011
- **ADR refs**: ADR-APP-001 (DEC-APP-007); ADR-APP-004 (DEC-APP-034, new)
- **Acceptance Criteria** (Gherkin): RQ-APP-061's.
- **Dependencies**: TASK-APP-046
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-16. Mockups first: tests of the header's buttons (left of the selector, a disabled one greyed)
  and of a dialog's field failed, then 25 mockup tests pass; `keygroup-editor-connected`, `-add-keygroups` and `-delete-keygroup` looked at
  (the delete button named as the real dialog names it, "Confirm"); validated by the owner. Core first and failing to compile: four cases (2
  added: the selector goes from 1 to 4, PAD has 4 on the sampler, 1 to 97 offered; a cancelled count sends nothing; keygroup 3 deleted after a
  question naming it and PAD: 1 to 3, keygroup 3 shown; a cancelled deletion sends nothing, the last keygroup asks nothing and sends nothing);
  then 87 app cases pass. Build warning-clean (a parameter hiding a member of `juce::Component` renamed); `ctest -C Debug`: 1234 passed;
  `tidy.py`: 15 sources, 0 findings; the IDE's SonarQube: nothing. Not verified: the dialogs drawn by JUCE and the real S5000 (the owner's
  run), GCC and Apple Clang (CI).
- **Assumptions**: (1) The buttons are in the header, so on every tab (owner: "fais comme tu proposes"). (2) The confirmation dialog keeps its
  button "Confirm". (3) A count that is not 1 to the most keeps the dialog open, the field in the pending colour.

### TASK-APP-048: The LFO tab
- **Tier**: M (re-tiered from S: a screen description, its mockups, and a layout field)
- **Status**: Done
- **Description**: LFO 1 and LFO 2 (program); the Aux envelope is in the Envelopes tab.
- **Requirement refs**: RQ-APP-056, RQ-APP-057
- **ADR refs**: ADR-APP-004 (DEC-APP-024, DEC-APP-027)
- **Acceptance Criteria** (Gherkin): *Given* the tab shown, *Then* each group reads PROGRAM.
- **Dependencies**: TASK-APP-044
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-16. Mockup `lfo-editor`: LFO 1 (rate, delay, depth, wave, sync, modwheel, aftertouch) and LFO 2
  (rate, delay, depth, wave, retrigger, clock sync, division), a row each; the labels Modwheel and Aftertch overlapped, shortened to Wheel and
  Touch; the clock division's labels overflowed a choice, so a layout parameter may say `"cells"` (owner's choice): tests first and failing
  (design 2: three cells generated, cells on a knob or out of 1 to 3 refused; mockups 1: the group a knob cell wider; C++ 1: the division three
  cells wide), then 30 design and 26 mockup tests pass. Core case (LFO 1's rate 43 read, 14 values, each group PROGRAM) failed to compile
  with the layout case, then passed; the Program tab moved to index 4. The tab first named "Mod sources" is "LFO" (owner, 2026-10-16: the
  envelopes and external controllers are modulation sources too; the Modulation tab routes them all). 89 app cases; build warning-clean;
  `ctest -C Debug`: 1236 passed; `tidy.py`: 15 sources, 0 findings. Mockup validated by the owner. Not verified: the real S5000, GCC and
  Apple Clang (CI).
- **Assumptions**: (1) The LFO's modulation inputs (the source and amount of its rate, delay and depth) go to the Modulation tab. (2) The core
  case and the `cells` field were written before the owner validated the mockup; nothing changed after it.

### TASK-APP-049: The Modulation tab
- **Tier**: M
- **Status**: Done
- **Description**: The routings of the keygroup modulation sources: each source, its destination and its amount.
- **Requirement refs**: RQ-APP-057, RQ-APP-056
- **ADR refs**: ADR-APP-004 (DEC-APP-035, new: the control of a routing)
- **Acceptance Criteria** (Gherkin): *Given* the Aux envelope routed to the filter cutoff, *Then* the tab shows the routing; *When* the person changes
  its amount, *Then* it is sent and read back.
- **Dependencies**: TASK-APP-048
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-16. The 17 inputs of the catalogue inventoried (pitch 2, filter 3, keygroup amp 1, program amp 2,
  pan 3, LFO 1 and LFO 2 rate, delay and depth), a source a program value and an amount a keygroup's or the program's; the owner chose a group
  per destination. Generator tests first and failing: design 2 (columns with their title and scope, the group pointing to them; columns that do
  not divide the parameters, a knob in them, an unknown scope, zones too refused), mockups 1 (each input a row named once, the cells under
  their column, its scope above it); then 32 design and 27 mockup tests pass. Mockup `modulation-editor`: the heads "SOURCE · PROGRAM" touched,
  then a two-line head pushed LFO 2 under the tabs, so the scope went to the group's title line above its column; validated by the owner.
  C++ first and failing to compile: the layout case checks each cell's catalogue scope against its column; the core case (AUX ENV routed to
  filter input 1 at 50 on keygroup 1: 34 values, the routing shown; 70 set, read back on keygroup 1); then 90 app cases pass. The view's zone
  grid now lays out any grid. Build warning-clean; `ctest -C Debug`: 1237 passed; `tidy.py`: 15 sources, 0 findings; the IDE's SonarQube:
  nothing. Not verified: the grid drawn by JUCE and the real S5000 (the owner's run), GCC and Apple Clang (CI).
- **Assumptions**: (1) The keygroup's amplitude input and the program's two are two groups, their amounts living in different places. (2) The
  Modulation tab sits between LFO and Program; the Program tab moved to index 5.

### TASK-APP-052: The attenuation in steps of 6 dB
- **Tier**: M
- **Status**: In Progress
- **Description**: The controller refuses a value between two of the catalogue's steps before anything is sent; the knob and the slider move
  by the step. Found by the owner's run of 2026-10-17 (F1).
- **Requirement refs**: RQ-APP-097
- **ADR refs**: ADR-APP-004 (DEC-APP-036)
- **Acceptance Criteria** (Gherkin): RQ-APP-097's.
- **Dependencies**: TASK-APP-045
- **Assignee**: AI
- **Verification**: Linux/Clang 18, Debug, 2026-10-21 (cloud session, no sampler; GCC 13 stops on a `-Wdangling-reference` of `RealSamplerSuiteTests.cpp`, not this task's). Core case (12 sent and read back; 1 refused with nothing sent, the problem naming "0, 6, 12, 18, 24 or 30 dB" and "nothing was sent") failed first (the sampler got 22 commands, 1 read back as 0), then passed; knob and slider step from the catalogue's `step` (`ParameterCell`, a view, DEC-APP-010). `ctest`: 1244 passed; `tidy.py --base HEAD`: 19 sources, 0 findings. Not verified: the knob on the real S5000 (owner's run, F1).
- **Assumptions**: (1) The keygroup level (code 0-10, steps of 6 dB in the catalogue) gets the same rule.

### TASK-APP-053: The arrows of every choice step it
- **Tier**: M
- **Status**: In Progress
- **Description**: `steppedValue` in the core; a `ChoiceBox` for every choice of the application (the edit panel's and the MIDI setup's),
  whose arrows choose the previous or next item and whose middle opens the list. Found by the owner's run of 2026-10-17 (F4).
- **Requirement refs**: RQ-APP-098
- **ADR refs**: ADR-APP-004 (DEC-APP-036, DEC-APP-027)
- **Acceptance Criteria** (Gherkin): RQ-APP-098's (core test of `steppedValue` and of the controller; the clicks on the real editor).
- **Dependencies**: TASK-APP-052
- **Assignee**: AI
- **Verification**: Linux/Clang 18, Debug, 2026-10-21. Core cases first (`steppedValue`: a choice's previous and next code, the attenuation by 6, nothing past the ends; the filter type stepped right then left twice, the second left sending nothing) failed to compile, then passed. `ChoiceBox` (new, `juce/app/src`) for the edit panel's choices, the keygroup selector and the MIDI setup dialog's two lists; the edit panel steps through `steppedValue`, the others through the list's items; the arrow's width shared with the look-and-feel (`choiceArrowWidth`). The editor built warning-clean; `ctest`: 1244 passed; `tidy.py`: 0 findings. Not verified: the clicks themselves (a view, never run here: no display, no sampler).
- **Assumptions**: (1) Past the first or the last value an arrow does nothing (no wrap-around). (2) A zone's sample stepped to NONE is refused as before (K8).

### TASK-APP-054: A plus on the enhanced filters
- **Tier**: M
- **Status**: In Progress
- **Description**: `marks` in the shape sets, checked and generated (`ChoiceShape::mark`); the cell and the mockups draw them; 2-POLE LP+,
  2-POLE BP+ and 1-POLE HP+ carry a "+" above their cutoff. Found by the owner's run of 2026-10-17 (F5).
- **Requirement refs**: RQ-APP-099
- **ADR refs**: ADR-APP-004 (DEC-APP-037, DEC-APP-032)
- **Acceptance Criteria** (Gherkin): RQ-APP-099's; a generator test refuses a mark without a shape or outside the square.
- **Dependencies**: TASK-APP-051
- **Assignee**: AI
- **Verification**: Linux/Clang 18, Debug, 2026-10-21. Generator tests first (a mark generated with its shape and none for the others; a mark without a shape, outside the square or without text refused; the mockup draws the `+` in the role colour over the cutoff, none for 2-POLE LP) failed (3 of 62), then passed (62 OK; the old assertion on the `ChoiceShape` initialiser updated to its two new fields). Core case (the three enhanced types carry `+` above their top, 2-POLE LP and VOWELISER none) failed, then passed. `generate_design.py --check` and `generate_mockups.py --check` up to date (no mockup shows an enhanced type). Not verified: the drawing in the editor and its likeness to the sampler's screen (owner's run, F5).
- **Assumptions**: (1) The `+` stands over the corner of the low and high passes and over the middle of the band pass, at 0.9 of the height: the sampler's screen was not seen.

### TASK-APP-050: All keygroups at once
- **Tier**: M
- **Status**: In Progress
- **Description**: "All keygroups" in the selector: a value shared by all keygroups is shown, otherwise "the values differ"; a change goes to all.
- **Requirement refs**: RQ-APP-059
- **ADR refs**: ADR-APP-004
- **Acceptance Criteria** (Gherkin): RQ-APP-059's.
- **Dependencies**: TASK-APP-045
- **Assignee**: AI
- **Verification**: Linux/Clang 18, Debug, 2026-10-21. Three core cases first (PAD grown to 3 keygroups, cutoffs 50, 50, 70: all keygroups shows the cutoff VARIES and the attenuation not, 60 set reads 60 on the three; deleting with all chosen sends nothing and asks for one keygroup, keygroup 3 then shows 70; KICK in zone 1 of keygroup 1 only: zone 1's sample varies, zone 2's not) failed (keygroup stayed 1, nothing differed), then passed. `ALL_KEYGROUPS` is -1, the existing case "keygroup 0 changes nothing" kept unchanged. The selector gets "All keygroups" after the last keygroup (2 keygroups or more), Delete greyed while it is chosen; a choice that varies reads VARIES. `ctest`: 1244 passed; `tidy.py`: 0 findings. Not verified: the real S5000 (the gateway's "all" reads each keygroup, `KeygroupSelection::all()`).
- **Assumptions**: (1) An order (low and high velocity or note) is checked against the first keygroup's values only. (2) A zone's sample chosen for all keygroups is assigned keygroup by keygroup.
