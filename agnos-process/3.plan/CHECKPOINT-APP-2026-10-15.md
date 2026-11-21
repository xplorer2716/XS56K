# Checkpoint — session APP, 2026-10-15 (second session of the day)

Branch `feature/APP`, pushed after each task. Session variables: `platform: windows`, `unit_tests: true`.

## Done
- PLAN-APP-004: TASK-APP-026 to TASK-APP-033 (browser menu without Open and refresh, space bar audition, long folder rename, type scale +2 px,
  draggable areas, the shown disk selected again, log file `log/XS56K.log`).
- PLAN-APP-005: TASK-APP-034 (FTR-APP-010, ADR-APP-004, the plan); TASK-APP-040's mockup part (layout `juce/app/design/layouts/sample.json`,
  screen `sample-editor.json`, mockups `documents/mockups/sample-editor-*.svg`, controls `valuebox`/`slider` in the generator), approved by the
  owner ("go pour implémentation").

## Open
- TASK-APP-035 to TASK-APP-039 (the C++ of the sample editor), then the rest of TASK-APP-040 (READMEs, real run).
- TASK-APP-002 (the owner's tabs of the program editor).

## Decisions to keep in mind
- Every write re-reads the whole tab (Loop End moves Loop Start on the S5000, DEC-APP-026).
- The layout's `control` names (`valuebox`, `choice`, `knob`, `slider`) are the contract between the mockups and the C++ controls (DEC-APP-027);
  a slider sits under its value box.
- The space bar auditions the edited sample when the editor has the focus, except while typing (RQ-APP-083).
- The loop crossfade has no SysEx item: a later "remote control" tab (front panel).
- Owner preferences: short specs and chat; the assistant specifies, implements, tests; the owner can take over at any time.
