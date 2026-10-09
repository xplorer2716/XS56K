# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- This section may be incomplete. for full history, please check git history.
- Initial project structure.
- MCP server (`juce/mcp`, `xs56k_mcp_server`): an MCP client such as Claude Code can work on the sampler by talking to it, in the
  musician's vocabulary and the sampler's own units, each Set read back from the sampler when the sampler can read it back. **56 tools
  without any option**: programs (list, select, create, rename, delete, delete all), 119 program parameters, keygroups (add, delete), the
  sample of each zone, samples (list, select, 12 parameters, rename, delete, delete all, audition), multis (list, select, create, rename,
  delete, delete all, 12 part parameters, the program of each part, the program number), the sampler's model and free memory, its
  settings (name, clock, play mode, front-panel lock; the other three are still given when the sampler refuses its play mode, as a
  real S5000 once did), its MIDI setup and filters (the sampler cannot read them back, and the tools say
  so), song files, set lists and scenelists (list, select, rename, delete), the effects board (channel mute, module type and state,
  module parameters) and clearing the sampler's memory. **16 more with `--allow-disk`**: browse the sampler's own disks, create, rename
  and delete folders and files, load a file or a folder, save one item or every item of a kind, song files, set lists and scenelists
  included (never replacing a file unless `overwrite` is true), free space, audition a file; `--disk-timeout-ms` sets how long the slow
  disk commands wait, and the sampler's disk-list refresh needs `--allow-disk-refresh` as well (it hung a real S5000). **5 more with
  `--allow-front-panel`**: press, hold and release a front-panel key, turn the data wheel, send ASCII keys (61 tools with this option, 77
  with both). **Every deletion asks for a `confirm` that is the exact name of what is deleted; deleting every item of a kind and clearing
  the memory ask for the number of items that would be lost**, and nothing is sent otherwise. The MIDI ports are launch arguments
  (`--in`, `--out`; `--list-ports` shows them), and `--screen independent|follow|as-is` chooses how the sampler's screen behaves while
  the server runs (`independent` by default; it replaces `--no-lcd`, which is now refused). It never formats or ejects a disk. It speaks
  both eras of MCP over standard input and output; `juce/mcp/README.md` is its reference. Run on a real S5000 (2026-10-05 to 07, on test
  objects): the tools of programs, keygroups, zones, samples, multis and disks, the program and multi parameters, browsing, loading and
  saving on a real disk; the sampler reports 0 bytes free for its FAT32 disk, and the three `--screen` modes behaved as designed against
  the sampler's screen. The settings, MIDI setup, song file, set list, scenelist, delete-all, clear-memory, effects board and front-panel
  key tools have run on the simulated sampler only (`xs56k_mcp_server_simulated` lets you try the server with no sampler).
- AKM: front panel control primitives (SysEx section 20): hold, release and press a front-panel key (the 43 keys of
  the spec's Table 31), move the data wheel, send ASCII keyboard data. A session now releases the keys it held when it
  closes. `xs56k_akm_probe --suite --front-panel` lets you drive the sampler's front panel from the PC keyboard
  (Windows console).
- AKM: MIDI configuration primitives (SysEx section 04): program change enable, multi select and its channel, external
  APM controller, aftertouch type, and allow or ignore a MIDI filter by event type and channel. The sampler cannot read
  these back, so `xs56k_akm_probe --suite --midi-config` asks you what its MIDI SETUP and MIDI FILTER pages show, changes
  each setting, has you confirm it on the sampler's screen, and puts back the values you declared.
- AKM: MIDI song file primitives (SysEx section 16): select a song file by name or by index, rename or delete the
  current one, count the song files and read their names, and count, name, rename and delete the set lists by index.
  `xs56k_akm_probe --suite --song-files` reads what the sampler holds, renames the first song file and the first set
  list and puts every name and the selection back; it never deletes anything.
- AKM: multi primitives (SysEx section 0C): create, select, rename and delete a multi, set the number of parts of new
  multis, the twelve part parameters (MIDI channel, mute, solo, level, output, pan, effects channel and send, fine tune,
  transpose, low and high note), the program number, a part's program by index or by name, and every Get of general
  information; "Delete ALL Multis" needs an explicit confirmation. `xs56k_akm_probe --suite --multi-lifecycle` round-trips all
  of it on a test multi and a test program it creates and deletes again.
- AKM: scenelist primitives (SysEx section 14): select a scenelist by name or by index, rename or delete the current one,
  count the scenelists and read their names, the current one's index and name. `xs56k_akm_probe --suite --scenelists`
  reads what the sampler holds, renames the first scenelist and puts the name and the selection back; it never deletes
  anything.
- AKM: Multi FX primitives (SysEx section 12): whether an FX board is installed, the number of channels and of the modules
  of a channel, the mute of a channel, the type and the enabled state of a module (the Table 24 module types are
  named) and the value of a module's parameter as a signed number. `xs56k_akm_probe --suite --multi-fx` creates a test
  multi, reads the board and, with an EB20 installed, changes a few values on the test multi and puts them back; with no
  board it only reads, and deletes the test multi again. Every Set is untested on a real sampler for want of a board.

<!--
Sections to use under each version (only those that apply):
### Added       for new features.
### Changed     for changes in existing functionality.
### Deprecated  for soon-to-be removed features.
### Removed     for now removed features.
### Fixed       for any bug fixes.
### Security    in case of vulnerabilities.
-->

[Unreleased]: https://github.com/xplorer2716/XS56K/commits/main
