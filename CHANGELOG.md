# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Initial project structure.
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
