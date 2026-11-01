# ADR-APP-005: Saving Memory Items to a Disk

## Status
Proposed

## Context
FTR-APP-006 (RQ-APP-043 to RQ-APP-047, all Must) asks the application to save, as the sampler's SAVE page does, the selected item alone or with
its children, all the items of its kind, or the whole memory, into the browser's current folder, asking before a file is replaced. The gateway
(`xs56k_sampler`, ADR-APP-001 DEC-APP-002) already saves: `saveMemoryItem` (`&2C`) and `saveAllMemoryItems` (`&2D`), each checked on a
refreshed listing (ADR-MCP-003 DEC-MCP-018, DEC-MCP-022). But without `overwrite` it refuses a save as soon as one file bears an item's name and
sends nothing (`SamplerGatewayDisk.cpp`, `saveAllMemoryItems`), while RQ-APP-045 needs "skip the existing files", and the specification's flag
"skip if the file exists" does exactly that on the sampler. Owner's answers (session APP, 2026-11-01): the gateway gains a skip mode; the save is
started from the File menu and from the memory view's context menu.

## Decision

### DEC-APP-038: The gateway saves with one of three answers to an existing file: refuse, skip, overwrite
`saveMemoryItem` and `saveAllMemoryItems` take an `ExistingFile` (`Refuse`, `Skip`, `Overwrite`) instead of the boolean `overwrite`. `Refuse` is
the present behaviour (nothing sent when a file bears an item's name); `Skip` keeps the files on the disk: for one item whose file is there,
nothing is sent; otherwise the save goes with the specification's "skip if file exists" flag (Data4/Data2 = 0); `Overwrite` replaces them.
What a real S5000 answers to that flag when a file exists is not known (the simulated sampler answers an error): the owner's run decides
(TASK-APP-061). The MCP tools pass `Refuse` or `Overwrite` as their `overwrite` argument says, so
the server's behaviour and its tests do not change; the outcome says which files were already there before the save, so a skip is reported.
[RQ-APP-045, RQ-APP-046, RQ-MCP-026, RQ-MCP-027]

### DEC-APP-039: A save controller in the core, one save dialog in the views
A `SaveController` in `xs56k_app_core` (no JUCE, ADR-APP-001 DEC-APP-001) prepares and runs a save: the item proposed (the memory view's
selection, else the current item of its kind), the scope (alone, with children, all of the kind with or without children, whole memory), the
destination (the browser's current disk and folder), "check names" (on by default, remembered in `AppSettings`). It refuses with nothing sent a
disk whose format is not MSDOS or FAT32 (RQ-APP-047). It asks its questions through `UserPrompts` (DEC-APP-007), runs the gateway calls as one
job of the sampler worker with the slow-operation timeout (DEC-APP-003, RQ-APP-013), then has the browser show the folder again and reports the
files saved, skipped and missing (RQ-APP-046). The JUCE view is one dialog opened by "File > Save…" and by "Save…" in the memory view's context
menu, every visual value from the design tokens (ADR-APP-002), drawn first as a mockup (ADR-APP-003). [RQ-APP-043 to RQ-APP-047]

### DEC-APP-040: Check names asks per file when the files are known, once otherwise; the whole memory is the six kinds in turn
For one item saved alone, the file its name gives is looked up in the current folder's listing: when it exists, the person answers replace, skip,
replace all or cancel. When the files written cannot be known before sending (children, all of a kind, whole memory) and at least one item's file
exists, the question is asked once: replace, skip or cancel. Cancel sends nothing. The whole memory is one `saveAllMemoryItems` per kind that
holds items (program, sample, multi, song file, set list, scenelist), in that order, in one job; a failure stops the job and the report says
which kinds were saved. [RQ-APP-044, RQ-APP-045]

## Consequences
- The MCP server keeps its two answers; only the gateway's signature changes, with its tests and the MCP tool that calls it.
- The application can skip existing files with the sampler doing the skipping, children included, in one command per kind.
- A skip is reported from the listing before the save, not from the sampler, which does not say which files it skipped.

## Alternatives Considered
- **The application saves the items one by one, skipping those already on the disk** (rejected by the owner): no control over the children,
  many more commands.
- **Saving from the browser's folder menu**: the item is chosen in memory, as on the sampler's SAVE page; the browser only gives the destination.

## Diagram
```mermaid
sequenceDiagram
    participant V as Save dialog (JUCE)
    participant S as SaveController (core)
    participant P as UserPrompts
    participant W as Sampler worker
    participant G as SamplerGateway
    V->>S: save(item, scope, checkNames)
    S->>S: disk format MSDOS or FAT32? else refuse
    alt check names on and a file exists
        S->>P: replace / skip / (replace all) / cancel
        P-->>S: answer
    end
    S->>W: one job
    W->>G: saveMemoryItem / saveAllMemoryItems(kind, ExistingFile, children)
    G-->>W: SaveOutcome (files before, after)
    W-->>S: outcome on the UI thread
    S->>V: saved, skipped, missing; browser shows the folder
```
