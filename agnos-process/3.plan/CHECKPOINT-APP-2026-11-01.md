# Checkpoint — session APP, 2026-11-01

Branch `feature/APP`, pushed. Session variables: `platform: linux`, `unit_tests: true`. Built and tested with Clang in `juce/build-clang`
(GCC 13 stops on a `-Wdangling-reference` of `RealSamplerSuiteTests.cpp`, not of this session).

## Done
- PLAN-APP-007 (FTR-APP-006, ADR-APP-005): TASK-APP-055 (gateway `ExistingFile` refuse/skip/overwrite), TASK-APP-056 to TASK-APP-059
  (`SaveController`: item proposed, scope, MSDOS/FAT32 only, check names, report; one commit for the four), TASK-APP-060 (save dialog,
  "File > Save…" Ctrl+S, "Save…" in the memory view, mockups `main-window-save*.svg`), TASK-APP-061's checklist
  (`checklists/real-sampler-run-APP-save.md`).
- PLAN-APP-008 (FTR-APP-011, ADR-APP-006), at the owner's request past the 10-task cap ("Continue sur multi"): TASK-APP-062 to
  TASK-APP-066 (layout `multi.json`, gateway `readMultiProgramNumber`, multi binding of the `EditController`, part program, program
  number, "Create a multi…", mockups `multi-editor-*.svg`; one commit), TASK-APP-067's checklist (`checklists/real-sampler-run-APP-multi.md`).

## Open
- TASK-APP-061: the owner's run on the S5000 (N5, N6: what the S5000 answers to "skip if file exists" when a file is there, unknown).
- TASK-APP-067: the owner's run of the multi editor (T7: how long "All parts" takes to read on the real S5000, unknown).
- ADR-APP-005 and ADR-APP-006 stay Proposed until the owner reads them.
- Earlier open tasks: TASK-APP-002, 040, 041, 042, 044, 050, 052, 053, 054 (owner's runs), TASK-BLD-013 (CI).
- Next, asked by the owner: the sampler's settings and MIDI setup (RQ-APP-070), song files, set lists and scenelists (RQ-APP-065). Each
  is one `Could` line of FTR-APP-009 and needs its own feature file with detailed requirements, an ADR and a plan: a new session (this
  one did 13 tasks; the owner stopped it after the multi). The effects board (RQ-APP-069) is left out: the
  owner has no effects board to test it with.

## Decisions to keep in mind
- One item saved with Skip and its file there: nothing is sent (DEC-APP-038). With "check names" on and no file known there, Skip is sent.
- The whole memory: one `&2D` per kind holding items, programs, samples, multis, song files, set lists, scenelists; the first failure stops.
- "Replace all" is not offered for one file (DEC-APP-040).
