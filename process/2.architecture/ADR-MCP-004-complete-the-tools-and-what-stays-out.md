# ADR-MCP-004: MCP Server — Completing the Tools, the Confirmation Rule, What Stays Out and the README as a Reference

## Status
Proposed — drafted in session MCP (2026-10-06) for FTR-MCP-004 (RQ-MCP-034 to RQ-MCP-044), under the owner's delegation (HOL, Human on
the loop), after the owner asked for an independent review of ADR-MCP-001 to ADR-MCP-003, for the missing tools to be implemented
"with a confirmation for what is destructive", and for a README that is a clear reference for a human. It amends ADR-MCP-002
DEC-MCP-010 and ADR-MCP-003 DEC-MCP-019 (their "never offered" lists): what is listed in DEC-MCP-024 is now offered. The other
decisions of ADR-MCP-001 to ADR-MCP-003 stand. No independent review by a second model was run on this record; the review it
answers was made by a subagent and was not re-read line by line.

## Context

The review found a pattern in the earlier ADRs: capabilities were left out for one of three reasons, none of them a request of the
owner. (i) A blanket "never offered" for anything irreversible (DEC-MCP-010, RQ-MCP-008, RQ-MCP-014, RQ-MCP-028), even where the
owner has a backup and the AKM layer has a typed confirmation. (ii) "Not a parameter": items a tool could carry (zone sample,
keygroup add and delete, a part's program) were left out of the parameter catalogue and never taken up elsewhere. (iii) "The sampler
held no sample or multi to try it on", a reason that stopped holding on 2026-10-06 when samples were loaded from the owner's disk.

Facts that shape the answer:

- The AKM layer has every primitive needed: `setZoneSample`, `getZoneSample`, `addKeygroupsToProgram`, `deleteKeygroupFromProgram`,
  `renameCurrentSample`, `deleteCurrentSample`, `createMulti`, `renameCurrentMulti`, `deleteCurrentMulti`, `setMultiPartByName`,
  `setMultiPartByIndex`, `deleteMultiPart`, `setMultiProgramNumber`, `renameFile`, `renameFolder`, `deleteFile`, `deleteSubFolder` (the
  last two take a typed confirmation), `getCurrentDiskFreeSpace`, `getFreeWaveMemoryPercent`, `getFreeWaveMemoryBytes`,
  `getFreeMpksMemoryPercent`, `startSampleAudition`, `stopSampleAudition`, `startFileAudition`, `stopFileAudition`.
- The disk items of rename, delete and audition ran on the owner's S5000 with its SCSI2SD on 2026-10-03
  (`SUMMARY-akm-sections-coverage.md`, `OBSERVATIONS-RQ-AKM-017`). The file rename takes the name without its extension: the sampler
  appends the file's own (`XS56K_RENAMED.AKP` given became `XS56K_RENAMED.AKP.AKP`).
- The existing guard for a deletion, `delete_program` (DEC-MCP-011), works: nothing is sent unless `confirm` is the exact name.
- The README of the server grew by accretion, tool by tool and flag by flag, and mixes what a person needs with notes about how the
  tools were verified; the owner finds it hard to understand which tool needs which option.

## Decision

### DEC-MCP-023: Every deletion takes a `confirm` that is the exact name of what is deleted
The tools `delete_keygroup`, `delete_sample`, `delete_multi`, `clear_part`, `delete_file` and `delete_folder` take a required `confirm`
argument. It must equal, as the sampler's screen or the listing spells it, the name of what is deleted: the current sample's, the
current multi's (also for `clear_part`: the part's program goes, the multi stays), the file's or folder's exact name, and, for
`delete_keygroup`, the name of the current program (the keygroup is named by its number in `keygroup`). Anything else, or nothing,
sends nothing to the sampler and says what `confirm` must be. A deleting tool is marked destructive in its annotations. The match is
exact on characters (not the tolerant matching the selection tools use), as `delete_program`'s is. [RQ-MCP-042]

### DEC-MCP-024: The new tools and their tiers; no new launch option
The memory tools: `set_zone_sample`, `get_zone_samples`, `add_keygroups`, `delete_keygroup`, `rename_sample`, `delete_sample`,
`create_multi`, `rename_multi`, `delete_multi`, `set_part_program`, `clear_part`, `set_multi_program_number`, `get_part_programs`
(added while implementing TASK-MCP-032: the assignments need a way to be read back and listed), `get_system_info`,
`audition_sample`. The disk tools (only with `--allow-disk`): `rename_file`, `rename_folder`, `delete_file`, `delete_folder`,
`get_disk_space`, `audition_file`. Tiers: reads for the `get_` tools, edit for the assignments, the program number and the audition,
structure for the creations, renames and deletions, disk for the rename and delete of files and folders (a folder is deleted only if it
is empty or `delete_contents` is true, and the answer says how many items go with it). A rename of a file or folder is refused when
the new name exists (a file and a folder cannot share a name). A new keygroup count never goes over 99, and the last keygroup of a
program is not deleted (a program with none is not usable). **No launch option is added**: the person configuring the server has
`--allow-disk` and `--allow-disk-refresh` to understand already, and a confirmation by name is the guard the owner asked for.
`rename_file` takes the new name without the extension, because the sampler appends it; a new name that carries the extension of the
old one is refused with that explanation. [RQ-MCP-034 to RQ-MCP-041]

### DEC-MCP-025: What stays not offered, and the owner decides
Not offered by this record, each left to the owner with its reason (none is excluded for good): song files, set lists, scenelists
(AKM has them; run on the real sampler against an empty memory only; delete is the risky item); the sampler's name, clock, play mode,
front-panel lock and MIDI setup (§00 settings and §04, which has no Get, so a previous value cannot be read and put back); the
effects board (§12, run on the simulator only: the owner has no EB20); ejecting a disk (not run on the real sampler, by design);
deleting all programs, all samples or all multis and Clearing the Sampler's Memory (AKM has typed confirmations; a total loss: if
offered, a count confirmation and a launch option of their own); the front-panel keys (a key press can answer "ENT" to any delete or
save screen and bypasses every guard); saving or loading song files, scenelists and MIDI files. Format and moving files between the
computer and the sampler do not exist in the SysEx specification. The source check keeps these forbidden until a decision changes it.
[RQ-MCP-042]

### DEC-MCP-026: The README is the human reference, organised by what a person needs to ask
`juce/mcp/README.md` is rewritten in this order: what the server is and what it needs (sampler, MIDI ports, an MCP client); how to
start it; a table of every launch option with its default and what it turns on; a table that says, for each group of tools, which
option it needs (none, `--allow-disk`, `--allow-disk-refresh`); the tools by domain (status, programs, keygroups and zones,
samples, multis, disks and files, information), each in a table with its arguments, whether it changes or deletes anything and
which `confirm` it asks for; the safety rules in plain words (what is never offered, what asks for a confirmation, what is
destructive); what has been tried on a real sampler and what has not; troubleshooting (the sampler does not answer, the ports, the
hang and the power cycle); and, last, the layout and tests for developers. Notes on how the tools were verified live in the process
documents, not in the README. A `ctest` case checks that every tool the server lists and every option of its usage text is in the
README. [RQ-MCP-043]

### DEC-MCP-027: `--screen independent | follow | as-is` replaces `--no-lcd`
Decided on 2026-10-06, after the owner found `--no-lcd` unclear. The server acts on two settings of section 00: Sync LCD (`&03`: on,
the selection made over MIDI and what the front panel displays follow each other, so that a selection on one changes the other;
off, they are independent) and Auto screen update (`&05`: on, the sampler redraws its screen when it processes a message; observed
on the S5000 on 2026-10-04: with it off the pages do not follow the edits). `--screen independent` (the default: what the server
always did when `--no-lcd` was left out) sets Sync LCD off and Auto screen update on; `--screen follow` sets both on; `--screen
as-is` sets neither and sends nothing. The session's close puts each setting it changed back to its **documented default** (Sync
LCD on, Auto screen update off: `samplerDefault`, RQ-AKM-042), **not to the value the sampler held before**, since section 00 has no
Get for them: an earlier text of this repository's documents said "as they were", which was wrong and is corrected. `--no-lcd` is
removed (its only user is the owner, whose configuration does not use it); it is refused with a message that names `--screen as-is`.
Which value is the better default (`independent`, or `follow` so that the person sees what the assistant edits) depends on what the
S5000's screen shows in each case, which has not been observed: the owner's check of the screen is part of TASK-MCP-037, and the
default changes only if it shows that it should. This supersedes the `--no-lcd` of ADR-MCP-001 (DEC-MCP-004 and DEC-MCP-008).
[RQ-MCP-045, RQ-MCP-002, RQ-MCP-003]

## Consequences

- **Easier.** A client can load a sample, assign it to a zone, build a multi, free memory, fix names and tidy the owner's disk; the
  loop "load, make a program, make it play, save" works end to end.
- **Harder.** Twenty-one more tools: the server offers 17 tools without `--allow-disk` and 27 with it before this record, 32 and 48
  after it; a client picks among more of them, so every description says in one line when to use it and what it needs.
- **Constrained.** A deletion of a file or a folder is irreversible and the sampler gives no undo: the guard is the confirmation by
  name, the folder-contents rule and the owner's own judgement; the real run uses only the `MCPTEST*` folders and test files.
- **Risk.** What the sampler does on the edge cases (the last keygroup, a sample assigned twice, a rename to an existing name) is
  unknown; the server refuses what it can beforehand and reports what the sampler answered.
- **Risk.** The source check allows more destructive primitives in two files (the gateway units for memory and disk): a call
  elsewhere fails it, and the primitives of DEC-MCP-025 stay forbidden everywhere.

## Alternatives Considered

- **One launch option per risk class** (for example `--allow-disk-delete`). Safer by default, but the owner asked for a
  confirmation instead, and every option makes the README harder to follow. Rejected for now; it can be added with no change to the
  tools.
- **A single generic `delete` tool with a kind.** Fewer tools, but the annotations, the `confirm` rule and the descriptions differ
  per kind, and a model picks a specific tool more reliably than it fills a kind argument. Rejected.
- **Putting zone sample assignment in the parameter catalogue.** It is a text value, not a number or a choice; a tool of its own
  says so. Rejected.

### Domain dictionary

| Term | Meaning |
|---|---|
| `confirm` | the argument a deleting tool requires: the exact name of what is deleted |
| destructive tool | a tool that deletes something; its annotation says so |
| zone | one of the four sample slots of a keygroup, numbered 1 to 4 |
| part | one of the slots of a multi, numbered from 1 in the tools (the sampler counts from 0) |
| disk tier | the tools that read or write the sampler's disk; they exist only with `--allow-disk` |
