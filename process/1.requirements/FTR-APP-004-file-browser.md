# FTR-APP-004: File Browser of the Sampler's Disks

## Overview

Session APP (2026-10-13). The top left area of the main window (RQ-APP-006) is a tree of the sampler's disks, folders and files, drawn in
the owner's sketch, with the basic commands on them: select, load, create a folder, rename, delete, audition a sample. Loading is the start of
the owner's working principle: a file loaded here appears in the memory view (FTR-APP-005), and selecting it there opens its editor.

What the SysEx specification allows (section 10, Tables 20 and 21) shapes this feature:

- the sampler has **one current disk and one current folder**; a folder is listed by opening it (`&13`, one level down) and left by closing
  it (`&14`, one level up). The tree can show several folders open, but listing one moves the sampler's current folder there; the current
  folder is also where a save writes (FTR-APP-006), so the browser marks it;
- **there is no copy and no move of a file** in section 10. Copy, paste and move can only be built from a load, a save into another folder
  and a delete; they are priority 2 (RQ-APP-064);
- file kinds come from the extension the sampler gives: `.AKP` program, `.WAV` sample, `.AKM` multi, `.MID` song file, `.SET` set list,
  `.ASC` scenelist, as saved by a real S5000 (OS 2.14) on 2026-10-10 (`juce/mcp/README.md`);
- a file is loaded by its name with its extension (`&2A`), with an option for samples (normal, as RAM, as virtual), or with all the files it
  depends on (`&2B`); a folder is loaded with everything in it, its sub-folders included (`&15`, user manual p. 30);
- the refresh of the list of disks (`&01`) left the owner's S5000 answering nothing until it was power-cycled (RQ-AKM-017), and the S5000
  reported 0 bytes free for a FAT32 disk that was not full (`juce/mcp/README.md`, real run of TASK-MCP-037).

**Priorities.** Priority 1 (mandatory) is written `Must`, priority 2 (nice to have) `Could` (owner, 2026-10-13). Copy, paste, move and
eject are priority 2, in FTR-APP-009.

**Depends on** FTR-APP-001 to FTR-APP-003, FTR-AKM-007 (RQ-AKM-060 to RQ-AKM-072).

**Sources.** `documents/_index/sysex_spec.items.tsv` (section 10), `documents/_index/sysex_spec.kb.md` (disk type and format codes),
`documents/akai_s5000_s6000_user_manual.1.21.pdf.md` (LOAD, pp. 29-36; DISK UTILS, pp. 226-227), `juce/akm/include/akm/DiskPrimitives.hpp`,
`juce/mcp/README.md`, `process/2.architecture/OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`.

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the person loading from and organising the sampler's disks; the memory view and the save (FTR-APP-005, FTR-APP-006).

---

## Functional Requirements

### RQ-APP-024: The disks
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the session opens, the browser SHALL show the disks connected to the sampler as the roots of its tree, by their names and types (RQ-AKM-060), with the current disk selected; WHEN the person selects a disk, the application SHALL make it the sampler's current disk (RQ-AKM-061) and show its format and free space, and SHALL show "not reported" instead of a free space of 0 bytes.
- **Rationale**: the S5000 can have several SCSI disks; it reported 0 bytes free for a disk that was not full.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler with two disks, *When* the session opens, *Then* the tree has two roots with their names and types; *When* the person selects the second, *Then* it becomes current and its format and free space are shown; *Given* a free space of 0, *Then* "not reported" is shown.
- **Dependencies**: RQ-APP-001, RQ-APP-017, RQ-AKM-060, RQ-AKM-061, RQ-AKM-062

### RQ-APP-025: Browsing the folders
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person opens a folder of the tree, the application SHALL make it the sampler's current folder (RQ-AKM-063) and show its sub-folders and files under it; the browser SHALL mark the sampler's current folder and show its path at all times.
- **Rationale**: the sampler lists only its current folder, and saves into it (FTR-APP-006).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a disk with a folder DRUMS holding KICK.WAV and a folder LOOPS, *When* the person opens DRUMS, *Then* LOOPS and KICK.WAV are shown under it, DRUMS is marked current and the path shows it.
- **Dependencies**: RQ-APP-001, RQ-APP-024, RQ-AKM-063

### RQ-APP-026: What a file is
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person selects a file, the browser SHALL show its name, its size (RQ-AKM-065) in the unit of RQ-APP-081 with the exact count of bytes in its tooltip, and its kind taken from its extension (program, sample, multi, song file, set list, scenelist, or unknown for any other extension), each kind with its own icon.
- **Rationale**: the person chooses what to load by kind and size.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* PAD.AKP of 4 096 bytes, *When* it is selected, *Then* the browser shows "PAD.AKP", "4.0 KB" (4 096 bytes in its tooltip), "program"; *Given* README.TXT, *Then* its kind is "unknown".
- **Dependencies**: RQ-APP-001, RQ-APP-025, RQ-APP-081, RQ-AKM-065
- **Amended**: 2026-10-15, the owner asked for sizes in kilobytes or megabytes as they read best (RQ-APP-081, TASK-APP-020).

### RQ-APP-027: The commands offered fit the selection
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person opens the context menu of the selected disk, folder or file, the browser SHALL offer only the commands that apply to it: for a file, load, load with its dependent files, rename, delete, and audition for a sample; for a folder, open, load the folder, create a folder in it, rename, delete; for a disk, create a folder at its root and refresh the list of disks.
- **Rationale**: a command that cannot apply is not offered, as on the sampler's own pages.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* PAD.AKP selected, *When* the context menu opens, *Then* "Audition" is not offered; *Given* KICK.WAV, *Then* it is.
- **Dependencies**: RQ-APP-001, RQ-APP-026

### RQ-APP-028: Loading a file
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person loads a file, the application SHALL load it by its name with its extension (RQ-AKM-066), as normal, as RAM or as virtual when the person chose so for a sample, or with all the files it depends on when the person chose "load with dependents"; once the load is done, the memory view SHALL show the new items (RQ-APP-036) and select the loaded one.
- **Rationale**: loading is the first step of the owner's working principle.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* PAD.AKP that uses KICK.WAV, *When* the person loads it with its dependents, *Then* the memory view lists the program PAD and the sample KICK and PAD is selected; *When* the person loads it alone, *Then* only PAD is added.
- **Dependencies**: RQ-APP-001, RQ-APP-013, RQ-APP-027, RQ-AKM-066

### RQ-APP-029: Loading a folder
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person loads a folder, the application SHALL ask for a confirmation that says the folder's files and its sub-folders' files will all be loaded, then load it (RQ-AKM-064) and update the memory view.
- **Rationale**: a folder load can fill the sampler's memory with more than the person sees in the tree.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a folder KIT with a sub-folder EXTRA, *When* the person loads KIT and confirms, *Then* the items of KIT and EXTRA appear in the memory view; *When* the person cancels, *Then* nothing is sent.
- **Dependencies**: RQ-APP-001, RQ-APP-013, RQ-APP-027, RQ-AKM-064

### RQ-APP-030: Creating a folder
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person creates a folder with a name in a folder or at a disk's root, the application SHALL create it there (RQ-AKM-063) and show it in the tree; IF a folder of that name exists there, THEN it SHALL refuse with nothing sent.
- **Rationale**: the person organises the disk before saving to it.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* DRUMS open, *When* the person creates NEW KIT, *Then* NEW KIT appears under DRUMS; *When* the person creates LOOPS which exists, *Then* nothing is sent and the reason is shown.
- **Dependencies**: RQ-APP-001, RQ-APP-025, RQ-AKM-063

### RQ-APP-031: Renaming a file or a folder
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person renames a file or a folder, the application SHALL rename it on the disk (RQ-AKM-063, RQ-AKM-065), the person typing a file's new name without its extension, which the sampler keeps, and SHALL show the new name in the tree; IF another file or folder of the folder has that name, THEN it SHALL refuse with nothing sent.
- **Rationale**: the sampler keeps the extension of a renamed file (real S5000, `juce/mcp/README.md`).
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* KICK.WAV, *When* the person renames it KICK 2, *Then* the tree shows KICK 2.WAV.
- **Dependencies**: RQ-APP-001, RQ-APP-025, RQ-AKM-063, RQ-AKM-065

### RQ-APP-032: Deleting a file or a folder
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person deletes a file or a folder, the application SHALL ask for the confirmation of RQ-APP-011, saying for a folder that what it holds is lost, then delete it (RQ-AKM-063, RQ-AKM-065, RQ-AKM-069) and remove it from the tree.
- **Rationale**: a deletion on the disk cannot be undone.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the folder OLD, *When* the person deletes it and confirms, *Then* OLD is no longer in the tree; *When* the person cancels, *Then* nothing is sent.
- **Dependencies**: RQ-APP-001, RQ-APP-011, RQ-AKM-069

### RQ-APP-033: Auditioning a sample on the disk
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person auditions a sample file, the application SHALL start its audition from the disk without loading it (RQ-AKM-068), and SHALL stop it WHEN the person stops it or selects another file.
- **Rationale**: the person listens before loading.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* KICK.WAV selected, *When* the person auditions it, *Then* the start of the audition is sent with the file's index; *When* the person selects another file, *Then* the stop is sent.
- **Dependencies**: RQ-APP-001, RQ-APP-027, RQ-AKM-068

### RQ-APP-034: Refreshing the list of disks
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person asks to refresh the list of disks, the application SHALL ask for a confirmation that says the sampler may stop answering until it is switched off and on (RQ-AKM-017), then send the refresh with the slow-operation timeout (RQ-APP-013) and list the disks again.
- **Rationale**: a disk inserted after the session opened is not listed otherwise; the risk is real on the owner's S5000.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the browser, *When* the person asks for the refresh and cancels, *Then* nothing is sent; *When* the person confirms, *Then* the refresh is sent and the disks are listed again.
- **Dependencies**: RQ-APP-001, RQ-APP-011, RQ-APP-013, RQ-AKM-060, RQ-AKM-070

### RQ-APP-079: Folding an open folder
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the person double-clicks a folder of the tree that shows its contents, the browser SHALL fold it (its sub-folders and files no longer shown, nothing sent to the sampler); WHEN the person double-clicks a folded folder, the application SHALL open it as RQ-APP-025 does and show its contents again.
- **Rationale**: the owner's request of 2026-10-15: a tree whose opened folders cannot be closed grows until it cannot be read.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* DRUMS opened with KICK.WAV under it, *When* the person double-clicks DRUMS, *Then* KICK.WAV is no longer shown and nothing is sent; *When* the person double-clicks DRUMS again, *Then* DRUMS is opened on the sampler and KICK.WAV is shown again.
- **Dependencies**: RQ-APP-025
