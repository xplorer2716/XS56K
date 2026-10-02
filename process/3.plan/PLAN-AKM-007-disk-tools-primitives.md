# PLAN-AKM-007: Disk Tools Primitives (Phase A, new lot, §10)

## Overview

Implements `FTR-AKM-007`: one tested primitive per command row of section `10` (35 commands, 16
REPLY formats, 51 rows — the largest single-section lot so far), two new item-catalogue value-format
decisions (`qword`, a second string argument in one item), destructive-command guards for Eject,
Delete Sub-Folder and Delete File, a dedicated real-sampler slow-operation guard for the six items the
spec itself calls potentially long-running, and the section's coverage/errata closure.

## References
- **Requirements**: RQ-AKM-060 to RQ-AKM-072 (`FTR-AKM-007`)
- **ADRs**: ADR-AKM-001 — amended in place as each new value-format decision is settled (`DEC-AKM-013`
  precedent for `string`), not a new ADR document.

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-AKM-056: Author FTR-AKM-007 and PLAN-AKM-007
- **Tier**: M
- **Status**: Done
- **Description**: Write the feature file for section `10` (Disk Tools) and this plan, grouping its
  51 spec rows into requirements and tasks, flagging the real-hardware slow-operation risk already
  observed for `&01` and the two new item-catalogue value-format needs (`qword`, a second string
  argument).
- **Requirement refs**: None (precedes the requirements it produces)
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* `documents/_index/sysex_spec.items.tsv` section `10`,
  *When* `FTR-AKM-007` is compared against it, *Then* every one of the 51 rows maps to exactly one RQ.
  *Given* the owner's confirmation on trigram and scope, *When* asked, *Then* it is recorded before
  drafting starts.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: `FTR-AKM-007-disk-tools.md` and this plan written; row-mapping cross-checked by
  hand against `sysex_spec.items.tsv` while drafting (35 commands + 16 REPLYs = 51, each assigned to
  exactly one of RQ-AKM-060 to RQ-AKM-071, confirmed in the RQ-by-RQ breakdown kept in session). Owner
  confirmed trigram `AKM` (continuing the existing SysEx-layer ID space, over a literal `ALM`) and
  confirmed including the known-risk items with a reinforced opt-in guard rather than deferring them.
- **Assumptions**: None.

---

### TASK-AKM-057: Disk discovery
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement `&01` (Update List of Disks), `&04` (Get Number of Disks) and `&05` (Get
  List of All Connected Disks, decoding each entry's handle/type/format/SCSI ID/writable flag/name).
- **Requirement refs**: RQ-AKM-060
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-060, on the simulated sampler.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: to be filled at closure.
- **Assumptions**: None.

### TASK-AKM-058: Disk selection and status
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement `&02` (Select Disk), `&03` (Test Disk Valid), `&06`/`&07` (current/
  specified disk type), `&08` (index of current disk) and `&09` (current path).
- **Requirement refs**: RQ-AKM-061
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-061, on the simulated sampler.
- **Dependencies**: TASK-AKM-057
- **Assignee**: AI
- **Verification**: to be filled at closure.
- **Assumptions**: None.

### TASK-AKM-059: Disk format, free space and name
- **Tier**: L
- **Status**: Not Started
- **Description**: Implement `&0A` (Get Format), `&0B` (Get Free Space) and `&0E` (Get Disk Name).
  `&0B` is the catalogue's first `qword`-formatted item: extend `generate_akm_items.py`'s schema to
  accept `format: "qword"` (mirroring `DEC-AKM-013`'s addition of `string`), record the new decision
  as `DEC-AKM-017` under `ADR-AKM-001`, and decode it to `std::uint64_t` end to end.
- **Requirement refs**: RQ-AKM-062
- **ADR refs**: ADR-AKM-001 (new DEC-AKM-017)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-062, on the simulated sampler;
  `generate_akm_items.py --check` up to date after the schema change.
- **Dependencies**: TASK-AKM-057
- **Assignee**: AI
- **Verification**: to be filled at closure.
- **Assumptions**: None.

### TASK-AKM-060: Folder navigation, listing and management
- **Tier**: L
- **Status**: Not Started
- **Description**: Implement `&10`-`&14` (sub-folder count/name/all-names, open, close) and `&16`/
  `&18` (create, rename). `&18` is the catalogue's first item carrying two consecutive
  null-terminated strings: extend the item-catalogue schema to express a second string argument,
  recording the new decision under `ADR-AKM-001`.
- **Requirement refs**: RQ-AKM-063
- **ADR refs**: ADR-AKM-001 (new DEC)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-063, on the simulated sampler;
  `generate_akm_items.py --check` up to date after the schema change.
- **Dependencies**: TASK-AKM-058
- **Assignee**: AI
- **Verification**: to be filled at closure.
- **Assumptions**: None.

### TASK-AKM-061: Load Folder
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement `&15` (Load Folder), completing on DONE or ERROR; no real-sampler call
  without the guard of TASK-AKM-067.
- **Requirement refs**: RQ-AKM-064
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-064, on the simulated sampler.
- **Dependencies**: TASK-AKM-060
- **Assignee**: AI
- **Verification**: to be filled at closure.
- **Assumptions**: None.

### TASK-AKM-062: File listing, info and rename
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement `&20`-`&24` (file count/name/all-names/size/index-by-name) and `&28`
  (rename), reusing the two-string item shape added by TASK-AKM-060.
- **Requirement refs**: RQ-AKM-065
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-065, on the simulated sampler.
- **Dependencies**: TASK-AKM-060
- **Assignee**: AI
- **Verification**: to be filled at closure.
- **Assumptions**: None.

### TASK-AKM-063: Load File, with and without dependent children
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement `&2A` (Load File) and `&2B` (Load File including dependents); no
  real-sampler call without the guard of TASK-AKM-067.
- **Requirement refs**: RQ-AKM-066
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-066, on the simulated sampler.
- **Dependencies**: TASK-AKM-062
- **Assignee**: AI
- **Verification**: to be filled at closure.
- **Assumptions**: None.

### TASK-AKM-064: Save Memory Item(s) to disk
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement `&2C` (Save Memory Item) and `&2D` (Save All Memory Items), with no
  default for the overwrite flag; no real-sampler call without the guard of TASK-AKM-067.
- **Requirement refs**: RQ-AKM-067
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-067, on the simulated sampler.
- **Dependencies**: TASK-AKM-062
- **Assignee**: AI
- **Verification**: to be filled at closure.
- **Assumptions**: None.

### TASK-AKM-065: Sample audition from disk
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement `&30` (Start Audition) and `&31` (Stop Audition).
- **Requirement refs**: RQ-AKM-068
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-068, on the simulated sampler.
- **Dependencies**: TASK-AKM-057
- **Assignee**: AI
- **Verification**: to be filled at closure.
- **Assumptions**: None.

### TASK-AKM-066: Destructive command guards for Eject, Delete Sub-Folder and Delete File
- **Tier**: M
- **Status**: Not Started
- **Description**: Guard `&0D` (Eject Disk, discard option), `&17` (Delete Sub-Folder) and `&29`
  (Delete File) behind an explicit confirmation argument, mirroring `RQ-AKM-025`/`RQ-AKM-046`/
  `RQ-AKM-056`.
- **Requirement refs**: RQ-AKM-069
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-069, on the simulated sampler.
- **Dependencies**: TASK-AKM-060, TASK-AKM-062
- **Assignee**: AI
- **Verification**: to be filled at closure.
- **Assumptions**: None.

### TASK-AKM-067: Real-sampler harness — disposable test folder and slow-operation guard
- **Tier**: L
- **Status**: Not Started
- **Description**: Add a `--disk-tools` (name to be confirmed against the probe's existing flag
  style) check to `xs56k_akm_probe --suite` that creates its own disposable sub-folder, exercises the
  safe §10 primitives inside it, deletes it through TASK-AKM-066's guard when done, never touches
  anything that existed before, and gates `&01`/`&15`/`&2A`/`&2B`/`&2C`/`&2D` behind a further,
  separate opt-in flag that documents the observed hang risk in its own help text.
- **Requirement refs**: RQ-AKM-070, RQ-AKM-071
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008 precedent for `--slow-operation`)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-070 and RQ-AKM-071, on the
  simulated sampler in `ctest`, and on the real sampler run by the owner (safe path only, unless the
  owner explicitly also passes the slow-operation guard).
- **Dependencies**: TASK-AKM-057 to TASK-AKM-066
- **Assignee**: AI, with the owner running the real-sampler suite
- **Verification**: to be filled at closure.
- **Assumptions**: None.

### TASK-AKM-068: Coverage of section §10 and errata resolution
- **Tier**: M
- **Status**: Not Started
- **Description**: Run `generate_akm_items.py --coverage` for section `10`, list any exclusion with
  its reason, record the `&0D`/`&0E` decimal-column erratum in `KNOWN_DEC_ERRATA` and in
  `sysex_spec.kb.md`, and update `SUMMARY-akm-sections-coverage.md` and `AGENTS.md` if the suite's
  options changed.
- **Requirement refs**: RQ-AKM-072
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-072.
- **Dependencies**: TASK-AKM-057 to TASK-AKM-067
- **Assignee**: AI
- **Verification**: to be filled at closure.
- **Assumptions**: None.
