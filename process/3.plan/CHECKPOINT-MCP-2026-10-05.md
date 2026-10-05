# Checkpoint — session MCP, 2026-10-05 (after 8 tasks)

Not an AGNOS artifact (no ID, ignored by the index): the hand-off note the CONTEXT MANAGEMENT rule asks for. Read it first
when resuming. It follows `CHECKPOINT-MCP-2026-10-04.md`, whose TASK-MCP-009 (the real run) this session closed. Run under the
owner's delegation (HOL, Human on the loop): every commit carries "(HOL -Human on the loop)" after its AGNOS subject.

## Done this session (branch `feature/MCP`, all committed)

| Task | Subject | Commit |
|---|---|---|
| TASK-MCP-010 | FTR-MCP-002, ADR-MCP-002 (Proposed), PLAN-MCP-002 | `ff7b4e2` |
| TASK-MCP-011 | `create_program`, `rename_program`, `delete_program` (confirm by name), tiers, source check as an allow list | `31179a4` |
| TASK-MCP-012 | Run of the server on the real S5000, observations, the simulated sampler sorts programs and moves the current one (closes TASK-MCP-009) | `fb0bd5d` |
| TASK-MCP-013 | Catalogue lot 3: 106 parameters, `offset` for a stepped number | `b67d3a4` |
| TASK-MCP-014 | Zones: a `zone` argument, 13 zone rows, 119 parameters | `399f878` |
| TASK-MCP-015 | Sample tools, a 12-row sample catalogue, read-only rows, `domain` of `list_parameters` | `ba5ad3e` |
| TASK-MCP-016 | Multi tools, a 12-row part catalogue, parts numbered from 1 | `8653272` |
| TASK-MCP-017 | Documentation (`AGENTS.md`, `CHANGELOG.md`, `juce/mcp/README.md`) and this checkpoint | the commit that carries this file |

Two commits (`4593992`, `d39d863`) fixed GCC `-Werror` build failures of the pull request on `feature/MCP`; `cb24365` did the same
on `feature/AKM`. `ctest` 861 of 861 on Windows/MSVC Debug at the last commit (803 at the start of the session).

## What was verified where

- **On the real S5000** (`process/2.architecture/OBSERVATIONS-RQ-MCP-012-real-sampler.md`): the program tools; all 119 program
  parameters (keygroup, program and zone) set, read back and put back, on a program the server created and deleted; a Set with
  "all keygroups" reaches every keygroup (closes that open point of FTR-MCP-001); the programs are kept in alphabetical order and
  the previous program becomes current after a deletion; `&12` answers ERROR 3 on an empty sample memory.
- **On the simulated sampler only**: the sample and multi parameters and tools. The sampler held no sample or multi and no tool
  creates one. The AKM layer's own real runs (`OBSERVATIONS-RQ-AKM-051`, `-093`) cover the same items, but the MCP layer above them
  (catalogue rows, gateway sequences, tools) was not run on a sample or a multi.

## Open

- **A run with a sample and a multi in memory** (the owner loads or records one): `xs56k_mcp_server` with the same scripted sweep
  as the program parameters (`set_sample_parameter` and `set_multi_parameter` on every row, then put back). It would settle the
  numbering of a multi's parts against the front panel (the tools send the part minus one), the loop-end-moves-loop-start quirk
  through the tools, and the solo-clears-mute quirk. The scripts used on 2026-10-05 were in the session's scratch folder, not
  committed; a committed driver is worth adding if the run is repeated.
- **Not run on a screen**: the labels of the modulation sources 12 to 14 (MODWHEEL 2, BEND 2, EXTERNAL 2: accepted and read back
  as set) and of the clock divisions; the effect of Auto screen update during edits.
- **The pull request's CI** after the last pushes: the GCC and Clang builds had failed on `-Werror` (a missing field initializer, an
  unused function); the fixes were pushed but the runs were not read by this session.
- ADR-MCP-002 is "Proposed": the owner reviews it with the pull request.

## Decisions to remember

- Three tiers (read, edit, structure), declared in the annotations; never offered: the disk, delete-all, Clear Sampler Memory, a
  save or a load (the source-search test is an allow list) — ADR-MCP-002 DEC-MCP-010.
- `delete_program` deletes only the current program and only when `confirm` is its exact name (DEC-MCP-011).
- A parameter's scope says what to select: program, keygroup, zone (the zone goes first in the item, 0 is all four), sample, multi
  part; a stepped number has a `step` and an `offset`, a multi-byte number `magnitudeBytes` (DEC-MCP-012).
- Separate tools per domain for samples and multis; `list_parameters` takes a `domain` (DEC-MCP-013).
- The tools never worked around a sampler quirk: a Set is read back and reported as it is.
- Windows: a running `xs56k_mcp_server.exe` (the client's own) locks the executable; build it into another folder with
  `-- /p:OutDir=<dir>\` to run it by hand.
