# PLAN-AKM-013: Multi FX Primitives (Phase A, new lot, §12)

## Overview

Implements `FTR-AKM-013`: one tested primitive per command row of section `12` (11 commands, 7 REPLY formats — 18
rows), and a real-sampler check that creates a test multi, reads what the sampler answers about its FX board and,
when a board is installed, round-trips the items and puts every value back.

Section `12` acts on the current multi (§0C). Its shapes need nothing new in the codec: channel, module, parameter
index and flags are `Byte` values, a REPLY is one data byte (three for `&51`), and a parameter value is a sign byte
and a two-byte magnitude, catalogued as three `Byte` values as every signed value of the earlier sections is (the
coverage check compares the spec's rows byte by byte); the primitive composes and decomposes the signed `int`. The
module type takes the codes of Table 24, named by an enumeration as the number of parts of new multis is.

**Safety note.** The owner's sampler has no FX board, so the Sets can only be tested on the simulated sampler; the
real-sampler check sends none when `&01` says "none". With a board it changes only the FX of a test multi the check
creates and deletes, and puts each value back (`RQ-AKM-102`).

## References
- **Requirements**: RQ-AKM-099 to RQ-AKM-103 (`FTR-AKM-013`)
- **ADRs**: ADR-AKM-001 (Accepted): DEC-AKM-003, DEC-AKM-012, DEC-AKM-013. No new decision is expected.

The plan has 6 tasks (TASK-AKM-100 to TASK-AKM-105): 100 authors the artifacts; 101 delivers the discovery Gets and
the model of the section in the simulated sampler; 102 the channel and module items (after 101); 103 the parameter
values (after 101); 104 is the real-sampler check (after 101 to 103); 105 closes the coverage (after 101 to 104).

This plan implements the tasks in the format specified below.

---

## Tasks

### TASK-AKM-100: Author FTR-AKM-013 and PLAN-AKM-013
- **Tier**: S
- **Status**: Done
- **Description**: Write the feature file and this plan for section `12`, from the spec's Figure 2 and Tables 22 to 25.
- **Requirement refs**: RQ-AKM-099, RQ-AKM-100, RQ-AKM-101, RQ-AKM-102, RQ-AKM-103
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* section `12` of the spec, *When* the feature file is read, *Then* each of its 18 rows is the subject of a requirement.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: N/A (Tier S). Both files written this session; `agnos-index` re-run after them.
- **Assumptions**: The owner's instruction to continue with §14 then §12 (session AKM, 2026-10-04) stands in for the DoR approval of each task of the plan. The owner said the sampler holds no EB20 card, so no Set of the section can be tested on hardware.

---

### TASK-AKM-101: FX board and layout discovery, and the section in the simulated sampler
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue and implement `&01`, `&10`, `&11` as primitives in `MultiFxPrimitives`, and model section
  `12` in the simulated sampler: the card, the layout of channels and modules (seeded by `setFxBoard`, with `eb20Layout`
  for the tests that want an EB20 as Figure 2) and the answers of a sampler with no board.
- **Requirement refs**: RQ-AKM-099
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-099.
- **Dependencies**: TASK-AKM-100
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean build (`/W4 /WX`), `ctest --test-dir juce/build -C Debug` 693/693
  passed after a full re-run in this session (687 before, 6 new in `MultiFxDiscoveryTests.cpp`, written before the
  code, `ctest -R RQ-AKM-099` 6/6): with an EB20 the card reads `Eb20` (frame section `12`, item `01`, no data), the
  channel count 4 (item `10`) and the module counts 6, 6, 2, 2 (item `11`, the channel as its one data byte); with no
  board the card is none and the channel count a REPLY of 0, not a failure; a channel the board lacks fails ERROR 04
  with no count; channels 128 and -1 are refused `ArgumentOutOfRange` with nothing sent; a card code the layer cannot
  name (5) is a REPLY with no card. `generate_akm_items.py --check`: up to date (376 items); `--coverage`: `section 12:
  3 of 11 spec rows covered (Multi FX, partial)`, `unaccounted: none`. `ItemCatalogueTests.cpp`'s count formula
  extended by the 3 records. Not verified: the real sampler (TASK-AKM-104).
- **Assumptions**: The spec is silent on what a sampler with no board answers to `&10` and `&11`: the simulated sampler
  answers a count of 0 and ERROR `04` (a modelling choice, `SimulatedSampler.cpp` `executeMultiFx`), the real answers
  being for TASK-AKM-104 to observe. The three discovery items need no current multi in the model (they describe the
  hardware); the items of TASK-AKM-102/103, which act on a multi's effects, will. One board for the whole sampler in the
  model, not one per multi. A card code other than 0 or 1 is not given as a card (the spec names two). Section `12` was
  added to `items.json` with `complete: false` until TASK-AKM-105. The Python test that used §12 as its example of an
  undeclared section now uses `2A`: the expectation (an undeclared section is refused) is unchanged, only the example
  section was no longer undeclared, as when section 04 was declared.

---

### TASK-AKM-102: Channel mute, module type and module state
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue and implement `&20`, `&21`, `&30`, `&31`, `&40`, `&41` and the `FxModuleType` enumeration
  of Table 24, and model them in the simulated sampler.
- **Requirement refs**: RQ-AKM-100
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-100.
- **Dependencies**: TASK-AKM-101
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean build (`/W4 /WX`), `ctest --test-dir juce/build -C Debug` 700/700
  passed after a full re-run in this session (693 before, 6 in `MultiFxConfigurationTests.cpp` and 1 in
  `test_generate_akm_items.py`, written before the code; `ctest -R RQ-AKM-100` 7/7 for the C++ ones): muting channel 1
  sends section `12`, item `20`, data `01 01` and reads MUTE (then `01 00`, ON), channel 0 untouched; setting module
  2 of channel 0 to Flange sends `30` with `00 02 03` and reads Flange (`31` with `00 02`), channel 1 untouched;
  a code Table 24 does not name (`11`) is read back unchanged; disabling module 3 of channel 0 sends `40` with
  `00 03 00` and reads disabled (`41`), enabling it `00 03 01`; a channel or module the board lacks fails ERROR 04
  with nothing changed and no value; with no multi current the Sets and Gets fail ERROR 04; a channel or module of 128 or
  -1, or a type code of 128, is refused `ArgumentOutOfRange` with nothing sent. `generate_akm_items.py --check`: up to
  date (382 items); `--coverage`: `section 12: 9 of 11 spec rows covered`, `unaccounted: none`. `ItemCatalogueTests.cpp`'s
  count formula extended by the 6 records. Not verified: the real sampler (TASK-AKM-104).
- **Assumptions**: The type of a module, and a flag, are not checked against Table 24 or the board's rules: the protocol
  is "as flexible and extensible as possible", the catalogue takes the spec's own range (0-127), and the layer names the
  17 known codes with an enumeration that a cast can step outside of (RQ-AKM-100's last criterion was reworded
  accordingly: it asked for a refusal above 16, which the spec's range does not support). "Only modules 2 and 3 of
  channels 0 and 1 may be changed" is not enforced in the model, the sampler's answer being unknown. As for the
  discovery items, the model answers ERROR 04 for what it does not have (channel, module, current multi). `--coverage`
  found §12 &30's spec row malformed by the PDF's text flow (its description is merged into the second data column, so the
  row seemed to describe 5 values, not 3): `spec_domains` now cuts a merged description that follows the last domain,
  and leaves a mid-column one (§10 &2C) as it was; `test_generate_akm_items.py` gained a test of the new rule.

---

### TASK-AKM-103: FX parameter values
- **Tier**: M
- **Status**: Done
- **Description**: Catalogue and implement `&50`, `&51` (a signed value of a sign byte and a two-byte magnitude) and
  model them in the simulated sampler.
- **Requirement refs**: RQ-AKM-101
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-101.
- **Dependencies**: TASK-AKM-101
- **Assignee**: AI
- **Verification**: Windows/MSVC Debug: clean build (`/W4 /WX`), `ctest --test-dir juce/build -C Debug` 706/706
  passed after a full re-run in this session (700 before, 6 new in `MultiFxParametersTests.cpp`, written before the
  code, `ctest -R RQ-AKM-101` 6/6): setting parameter 1 of module 2 of channel 0 to -25 sends section `12`, item `50`
  and `00 02 01 01 00 19`, and `&51` (`00 02 01`) reads -25; 4000 sends `00 1F 20` as sign and magnitude and reads 4000;
  16383 and -16383 send `7F 7F` with sign 0 and 1 and round-trip, 0 is a positive zero; a parameter set changes
  neither another parameter of the module nor the same one of another module or channel (the unset ones read 0); a
  magnitude of 16384 or -16384, and a channel, module or parameter of 128, are refused `ArgumentOutOfRange` with
  nothing sent; a channel or module the board lacks, and no current multi, fail ERROR 04 with no value.
  `generate_akm_items.py --check`: up to date (384 items); `--coverage`: `section 12: 11 of 11 spec rows covered`,
  `unaccounted: none`. `ItemCatalogueTests.cpp`'s count formula extended by the 2 records. Not verified: the real
  sampler (TASK-AKM-104).
- **Assumptions**: The value is catalogued as three `Byte` values (sign, magnitude MSB, magnitude LSB), as the spec's
  rows list them and as every signed value of the earlier sections is, and not as one `signed_word`: the coverage check
  compares rows byte by byte (it reads six values for `&50`, one `signed_word` would be four). FTR-AKM-013 and this plan
  were reworded accordingly. A parameter never set reads 0 in the model; the sampler's answer for a parameter that does
  not exist on the module's type is unknown (Table 25's ranges are not enforced), and out of reach of a sampler with no
  board. The first Python text-mode rewrite of two files in this lot (TASK-AKM-102) changed their line endings; they were
  put back to LF before the commit, and later scripted edits write with `newline=''`.

---

### TASK-AKM-104: Real-sampler check of the Multi FX
- **Tier**: L
- **Status**: Not Started
- **Description**: Add `--multi-fx` to `xs56k_akm_probe --suite`: a check that creates a test multi, reads the board
  and, with none, logs what the other Gets answer and is skipped; with one, round-trips the mute of a channel, the
  state of a module, the type of a changeable module and a parameter, putting each back; on every exit path the test
  multi is deleted and the current multi selected again. Run it on the real sampler (no board).
- **Requirement refs**: RQ-AKM-102
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008 precedent for the suite's opt-in flags)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-102, on the simulated sampler in `ctest` and on the
  real sampler (the empty-board answers only).
- **Dependencies**: TASK-AKM-101, TASK-AKM-102, TASK-AKM-103
- **Assignee**: AI, running the real-sampler check under the owner's standing authorization
- **Verification**: (to fill at closure)
- **Assumptions**: (to fill at closure)

---

### TASK-AKM-105: Coverage of section §12
- **Tier**: M
- **Status**: Not Started
- **Description**: Run `generate_akm_items.py --coverage` for section `12`, list any exclusion with its reason,
  flip the section's `complete` flag in `items.json`, and update `SUMMARY-akm-sections-coverage.md`, `AGENTS.md`
  (the new probe option) and `CHANGELOG.md`.
- **Requirement refs**: RQ-AKM-103
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-103.
- **Dependencies**: TASK-AKM-101 to TASK-AKM-104
- **Assignee**: AI
- **Verification**: (to fill at closure)
- **Assumptions**: (to fill at closure)
