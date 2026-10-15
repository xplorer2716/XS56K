# PLAN-APP-003: Editor Application — Readable Sizes, Folding Folders and Tooltips of Cut Texts

## Overview

Session APP (2026-10-15). After trying the browser and the memory view of PLAN-APP-002, the owner asked for three changes the same day: sizes written
in kilobytes or megabytes as they read best (binary units, one decimal, the exact count of bytes in the tooltip), a double click on an open folder
that folds it, and a tooltip with the whole text wherever a text of the window is cut short, as the rows of the lists already do. The first is
an amendment of RQ-APP-026 and RQ-APP-042 through a new RQ-APP-081, the others are the new RQ-APP-079 and RQ-APP-080.

Each change is made core first where it has behaviour (the size text, the folded folders), with its tests against the simulated sampler
(RQ-APP-014, DEC-APP-010), then in the views. 3 tasks.

| Step | Task | What |
|---|---|---|
| 1 | TASK-APP-020 | sizes in bytes, KB or MB, in the browser and the memory view |
| 2 | TASK-APP-021 | a double click folds an open folder, and opens a folded one |
| 3 | TASK-APP-022 | a tooltip with the whole text on every text that is cut short |

## References
- **Requirements**: RQ-APP-079, RQ-APP-080, RQ-APP-081 (new); RQ-APP-024, RQ-APP-025, RQ-APP-026, RQ-APP-042 (amended or concerned); RQ-APP-001 to
  RQ-APP-005 (FTR-APP-001, design system); RQ-APP-014
- **ADRs**: ADR-APP-001 (DEC-APP-001, DEC-APP-004, DEC-APP-010); ADR-APP-002 (DEC-APP-011 to DEC-APP-016)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-APP-020: Sizes in bytes, kilobytes or megabytes
- **Tier**: M
- **Status**: Done
- **Description**: In `xs56k_app_core`, a function that writes a size as RQ-APP-081 says ("512 bytes", "4.0 KB", "114.6 KB", "151.2 MB"); the browser view uses it for
  the selected file and the free space of the disk, with the exact count of bytes (`bytesText`) in the tooltip, and the memory view for the free and total wave memory.
- **Requirement refs**: RQ-APP-081, RQ-APP-026, RQ-APP-042, RQ-APP-024, RQ-APP-001
- **ADR refs**: ADR-APP-001 (DEC-APP-001, DEC-APP-010); ADR-APP-002 (DEC-APP-014)
- **Acceptance Criteria** (Gherkin): *Given* 0, 512, 1 023, 1 024, 4 096, 117 398, 1 048 575, 1 048 576 and 158 548 694 bytes, *Then* "0 bytes", "512 bytes", "1023 bytes",
  "1.0 KB", "4.0 KB", "114.6 KB", "1.0 MB" (1 023.999 KB rounds to the next unit), "1.0 MB" and "151.2 MB"; *Given* a file of 117 398 bytes selected, *Then* the browser
  shows "114.6 KB" and its tooltip "117 398 bytes"; *Given* 158 281 898 bytes free of 158 548 694, *Then* the memory view shows "150.9 MB free of 151.2 MB".
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, 2026-10-15: the new case of `BrowserTests.cpp` (RQ-APP-081) failed first (link error, no `sizeText`), then, once written, failed
  on one value: 158 281 898 bytes is 150.95 MB, "150.9 MB", not the "151.0 MB" the test and this task's criterion said (an error of mine in the expected value,
  corrected in both); then passed: "0 bytes", "512 bytes", "1023 bytes", "1.0 KB", "4.0 KB", "114.6 KB", "1.0 MB" for 1 048 575 and 1 048 576, "151.2 MB",
  "not reported". `ctest -C Debug`: 100% passed, 0 failed out of 1189; build warning-clean. On the real S5000: the selected file "Half-Cutoff-sqr-bass-C.wav - sample
  - 114.6 KB" with the tooltip "Half-Cutoff-sqr-bass-C.wav - 117 398 bytes"; the memory view "150.2 MB free of 151.2 MB (99 %), MPKS 100 % free" with the tooltip
  "157 461 530 bytes free of 158 548 694 bytes (99 %), MPKS 100 % free" (read through the accessibility tree).
- **Assumptions**: (1) A value that rounds to 1024.0 in its unit is written in the next unit ("1.0 MB", not "1024.0 KB"). (2) The decimal mark is the point, as in the
  rest of the English interface. (3) The free space of a disk keeps "not reported" for 0 (RQ-APP-024). (4) Gigabytes added beyond the owner's "kilo ou mega" for the
  free space of a large disk, RQ-APP-081 amended to say so. (5) The free space of a disk is written by `sizeText` too, so the existing case of RQ-APP-024 now expects
  "976.6 KB" for 1 000 000 bytes (a behaviour the amended requirement changed, not a test made to pass). (6) The path, the details and the free memory became labels
  (`juce::Label`) so that they carry a tooltip.

### TASK-APP-021: Folding an open folder
- **Tier**: M
- **Status**: Not Started
- **Description**: In `xs56k_app_core`, the browser's state records the folded folders, and `FileBrowser` folds a folder (nothing sent) or opens a folded one (as
  `openFolder`, which unfolds it); the browser view hides what a folded folder holds and calls one or the other on a double click of a folder, by its state.
- **Requirement refs**: RQ-APP-079, RQ-APP-025
- **ADR refs**: ADR-APP-001 (DEC-APP-001, DEC-APP-004, DEC-APP-010)
- **Acceptance Criteria** (Gherkin): *Given* DRUMS opened, *When* it is folded, *Then* the model says it is folded and nothing is sent; *When* it is opened again, *Then*
  it is opened on the sampler, listed and no longer folded; *Given* DRUMS folded, *When* a sub-folder of it is opened from elsewhere (a load in it), *Then* DRUMS is
  unfolded; the application shows DRUMS without its files once folded.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: (at closure)
- **Assumptions**: (1) Folding is a state of the view kept in the model (DEC-APP-004), lost on disconnection like the rest of the browser's state. (2) Opening a folder
  unfolds it and every folder on its path, so the current folder is always shown. (3) A double click on a disk keeps its present meaning (open its root).

### TASK-APP-022: Tooltips of the texts cut short
- **Tier**: M
- **Status**: Not Started
- **Description**: In `XS56KApp`, a text component that gives its whole text as its tooltip when, and only when, it does not fit its bounds, used for the texts drawn
  by the views (the browser's path and details, the memory view's free memory, the status bar's connection, job and problem, the areas' titles and hints, the
  dialogs' labels); the rows keep theirs. The delay before a tooltip appears becomes a design-system token (`motion.tooltip`).
- **Requirement refs**: RQ-APP-080, RQ-APP-001, RQ-APP-002, RQ-APP-004, RQ-APP-005
- **ADR refs**: ADR-APP-002 (DEC-APP-011, DEC-APP-013, DEC-APP-014); ADR-APP-001 (DEC-APP-001)
- **Acceptance Criteria** (Gherkin): *Given* the window at its minimum size and the path "S5K:\\Soul - Funky - Acid jazz", *When* the pointer rests on the path, *Then*
  its whole text is shown in a tooltip; *Given* "Disks", which fits, *Then* no tooltip; *Given* the token check and the visual-literal check, *Then* they pass.
- **Dependencies**: TASK-APP-020
- **Assignee**: AI
- **Verification**: (at closure)
- **Assumptions**: (1) The views are not unit-tested (DEC-APP-010): checked by a run of the application, with the tooltip shown through the accessibility tree or a capture.
  (2) `motion.tooltip` is a new global motion token of 700 ms, JUCE's default, the mockups having no tooltip. (3) Texts already given whole (the rows' names) keep their
  tooltip as it is.
