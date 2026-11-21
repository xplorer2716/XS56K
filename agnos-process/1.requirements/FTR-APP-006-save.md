# FTR-APP-006: Saving Memory Items to a Disk, as the Sampler's SAVE Page Does

## Overview

Session APP (2026-10-13). The owner: "on peut sélectionner un artefact en mémoire et le sauvegarder, on va essayer de rester le plus proche
de ce qui se fait sur le sampler". The sampler's SAVE page (user manual pp. 216-218) works this way:

- the item to save is the one being worked on, and another item of memory can be chosen instead;
- it is saved into the currently selected folder, which the person can change (open and close folders, create a new folder, choose another
  disk);
- the person saves the item alone or with its children (a multi with its programs and their samples), all the items of its kind with or
  without their children, or the entire contents of memory;
- with CHECK NAMES on, a file of the same name in the folder makes the sampler ask YES (replace), SKIP (keep the file on disk), YES TO ALL
  or CANCEL SAVE; with it off, files of the same name are replaced without a question;
- the sampler saves only to an MS-DOS formatted disk (p. 218).

What the SysEx specification gives (section 10): `&2C` saves one memory item and `&2D` all the items of a kind, each with one flag "skip if the
file exists / overwrite" and one flag "save children" — **no question per file and no rename before saving**. The application therefore
checks the folder's listing itself to ask per file; the rename before saving is priority 2 (FTR-APP-009). Disk format codes: 1 MSDOS, 2
FAT32 (`documents/_index/sysex_spec.kb.md`); the owner's FAT32 disk was saved to on a real S5000 (`juce/mcp/README.md`, TASK-MCP-037).

**Priorities.** Priority 1 (mandatory) is written `Must`, priority 2 (nice to have) `Could` (owner, 2026-10-13).

**Depends on** FTR-APP-004 (the current folder is the destination), FTR-APP-005 (the item is chosen there), FTR-AKM-007 (RQ-AKM-067).

**Sources.** `documents/akai_s5000_s6000_user_manual.1.21.pdf.md` (SAVE, pp. 216-218), `documents/_index/sysex_spec.items.tsv` (section 10
`&2C`, `&2D`), `juce/akm/include/akm/DiskPrimitives.hpp` (`saveMemoryItem`, `saveAllMemoryItems`), `process/2.architecture/ADR-MCP-003`
(DEC-MCP-018, DEC-MCP-022: the save checked against a refreshed listing).

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the person keeping their work on the sampler's disks.

---

## Functional Requirements

### RQ-APP-043: Saving the selected item into the current folder
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person asks to save, the application SHALL propose the item selected in the memory view (the current item of its kind when none is selected), the destination being the sampler's current folder shown in the browser (RQ-APP-025), and SHALL let the person choose to save it alone or with its children; once the person validates, it SHALL save it (RQ-AKM-067) with the slow-operation timeout (RQ-APP-013).
- **Rationale**: the sampler's SAVE page works this way (p. 216).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the program PAD selected and the folder KIT current, *When* the person saves it with its children, *Then* PAD is saved into KIT with the "save children" flag; *Given* nothing selected and BASS the current program, *When* the person asks to save, *Then* BASS is proposed.
- **Dependencies**: RQ-APP-001, RQ-APP-013, RQ-APP-025, RQ-APP-037, RQ-AKM-067

### RQ-APP-044: Saving all the items of a kind, or the whole memory
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person chooses to save all the items of the selected item's kind, with or without their children, the application SHALL save them into the current folder (`&2D`, RQ-AKM-067); WHEN the person chooses to save the entire memory, it SHALL save all the items of each of the six kinds into the current folder.
- **Rationale**: the sampler offers both (p. 217); the specification has no single "save everything", so the whole memory is the six kinds in turn.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* three samples in memory, *When* the person saves all samples, *Then* one "save all" of kind sample is sent; *When* the person saves the entire memory, *Then* one "save all" is sent for each of the six kinds.
- **Dependencies**: RQ-APP-001, RQ-APP-043, RQ-AKM-067

### RQ-APP-045: Check names before replacing a file
- **Category**: Functional
- **EARS Type**: Complex
- **Statement**: WHILE "check names" is on (the default), WHEN a file the save would write already exists in the current folder, the application SHALL ask, before sending, whether to replace it, skip it, replace all, or cancel the save, and SHALL send each item with the matching "overwrite" or "skip" flag; IF the application cannot tell before sending which files a save writes (children, a save of all items), THEN it SHALL ask once for the whole save: replace existing files, skip existing files, or cancel. WHILE "check names" is off, the application SHALL replace existing files without asking.
- **Rationale**: the sampler's CHECK NAMES (p. 217); the specification has one flag per save, so the question is asked from the folder's listing.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* PAD.AKP in the folder and "check names" on, *When* the person saves the program PAD alone and answers "skip", *Then* the save is sent with "skip if the file exists"; *When* the person cancels, *Then* nothing is sent; *Given* "check names" off, *Then* the save is sent with "overwrite" and no question is asked.
- **Dependencies**: RQ-APP-001, RQ-APP-011, RQ-APP-043, RQ-APP-044, RQ-AKM-065

### RQ-APP-046: The save is checked on the disk
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN a save is done, the application SHALL list the current folder again, re-opening it when a file expected is missing (DEC-MCP-022), show the saved files in the browser and tell the person which files were saved, which were skipped and which are missing.
- **Rationale**: a DONE says the command ended, not that the expected file is on the disk.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the program PAD saved alone into KIT, *When* the save is done, *Then* the browser lists PAD.AKP in KIT and the application says PAD.AKP was saved.
- **Dependencies**: RQ-APP-001, RQ-APP-025, RQ-APP-043

### RQ-APP-047: Only to a disk the sampler can write
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF the current disk's format is not MSDOS or FAT32 (RQ-AKM-062), THEN the application SHALL refuse the save with nothing sent and say that the sampler saves only to an MS-DOS formatted disk.
- **Rationale**: the user manual, p. 218: no save to Akai or other formats.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the current disk formatted S3000, *When* the person asks to save, *Then* nothing is sent and the reason is shown; *Given* a FAT32 disk, *Then* the save proceeds.
- **Dependencies**: RQ-APP-001, RQ-APP-024, RQ-APP-043, RQ-AKM-062
