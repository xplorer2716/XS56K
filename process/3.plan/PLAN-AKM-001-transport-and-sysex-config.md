# PLAN-AKM-001: Transport and SysEx Configuration (Phase A, lots A0+A1)

## Overview

Plan for FTR-AKM-001, the first lot of Phase A of the S5000 control plan
(`process/1.requirements/DRAFT-s5000-midi-primitives-and-workflows.md`): the AKAI SysEx transport and
section §00. It also carries the authoring of the four Phase A feature files, since they were written
in the same session. FTR-AKM-002, FTR-AKM-003 and FTR-AKM-004 (Program, Keygroup, Zone) get their own
plans, written when each lot starts, on top of what this one delivers.

## References
- **Requirements**: FTR-AKM-001 (RQ-AKM-001 to RQ-AKM-020, RQ-AKM-039, RQ-AKM-040); FTR-AKM-002 (RQ-AKM-021 to RQ-AKM-027), FTR-AKM-003 (RQ-AKM-028 to RQ-AKM-033) and FTR-AKM-004 (RQ-AKM-034 to RQ-AKM-038) for TASK-AKM-001 only
- **ADRs**: None yet (ADR-AKM-001 to be written)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-AKM-001: Author the Phase A feature files
- **Tier**: M
- **Status**: Done
- **Description**: Write FTR-AKM-001 in full (transport and §00) and FTR-AKM-002 to FTR-AKM-004 (§0A, §08, §06) at the level of the spec's item groups, from `documents/_index/sysex_spec.kb.md`, `documents/_index/sysex_spec.items.tsv` and the spec text.
- **Requirement refs**: RQ-AKM-001 to RQ-AKM-040
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* the four files, *When* the process index is regenerated, *Then* it reports no duplicate ID and lists FTR-AKM-001 to FTR-AKM-004 and RQ-AKM-001 to RQ-AKM-040. *Given* the item counts stated in FTR-AKM-002, FTR-AKM-003 and FTR-AKM-004, *When* compared with `documents/_index/sysex_spec.items.tsv`, *Then* they match. *Given* each requirement, *When* read, *Then* it has an EARS statement, a rationale, a priority, a Gherkin criterion and its dependencies.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Re-run in this session: `agnos-index` exit 0 (no duplicate), 78 entries with this plan; the item counts of sections `00`, `06`, `08`, `0A` (7; 28 + 14; 80 + 40; 95 + 46) and the sub-group counts were read from `documents/_index/sysex_spec.items.tsv` by script and match the tables in the three outline files; the Table 5 footnotes and the confirmation and checksum passages (pp. 3–6) were read in the spec text. Not verified: the value ranges of the individual items of §0A, §08 and §06 (checked only at group level; refined when each lot starts), and every frame shown in an acceptance criterion as "constructed" (to be replaced by captured frames under RQ-AKM-017).
- **Assumptions**: Artifacts are written in English, like the existing ones; FTR-AKM-001's file name keeps the French slug announced to the owner. The two additions RQ-AKM-039 and RQ-AKM-040 came out of the owner's decisions of this session (DeviceID as an application setting; known §00 state at session start).
