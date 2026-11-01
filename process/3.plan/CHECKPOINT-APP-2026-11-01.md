# Checkpoint — session APP, 2026-11-01

Branch `feature/APP`, pushed. Session variables: `platform: linux`, `unit_tests: true`. Built and tested with Clang in `juce/build-clang`
(GCC 13 stops on a `-Wdangling-reference` of `RealSamplerSuiteTests.cpp`, not of this session).

## Done
- PLAN-APP-007 (FTR-APP-006, ADR-APP-005): TASK-APP-055 (gateway `ExistingFile` refuse/skip/overwrite), TASK-APP-056 to TASK-APP-059
  (`SaveController`: item proposed, scope, MSDOS/FAT32 only, check names, report; one commit for the four), TASK-APP-060 (save dialog,
  "File > Save…" Ctrl+S, "Save…" in the memory view, mockups `main-window-save*.svg`), TASK-APP-061's checklist
  (`checklists/real-sampler-run-APP-save.md`).

## Open
- TASK-APP-061: the owner's run on the S5000 (N5, N6: what the S5000 answers to "skip if file exists" when a file is there, unknown).
- ADR-APP-005 stays Proposed until the owner reads it.
- Earlier open tasks: TASK-APP-002, 040, 041, 042, 044, 050, 052, 053, 054 (owner's runs), TASK-BLD-013 (CI).
- Next, asked by the owner: the multi editor (RQ-APP-063), the sampler's settings and MIDI setup (RQ-APP-070), song files, set lists
  and scenelists (RQ-APP-065). Each is one `Could` line of FTR-APP-009 and needs its own feature file with detailed requirements, an ADR
  and a plan: a new session (this one planned 7 tasks; the process caps a session at 10). The effects board (RQ-APP-069) is left out: the
  owner has no effects board to test it with.

## Decisions to keep in mind
- One item saved with Skip and its file there: nothing is sent (DEC-APP-038). With "check names" on and no file known there, Skip is sent.
- The whole memory: one `&2D` per kind holding items, programs, samples, multis, song files, set lists, scenelists; the first failure stops.
- "Replace all" is not offered for one file (DEC-APP-040).
