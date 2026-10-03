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
