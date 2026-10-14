# FTR-APP-001: Design System of the Editor Application

## Overview

Session APP (2026-10-13). The owner starts the user interface of the editor application (`juce/app`, today a placeholder window,
`RQ-BLD-007`). The AGNOS process requires a design system before the first UI requirement is implemented (MANDATORY DESIGN SYSTEM, UI
projects): one machine-readable source of truth for every visual value, consumed by code as tokens, introduced by its own requirements and
its own ADR. This feature states what the design system SHALL be; its values (palette, theme, fonts, spacing unit) are decided in its ADR
with the owner, not here.

The starting point is `process/1.requirements/design-system-template.md` (three tiers of tokens: global, semantic, component; a shared
table of interaction states; a component catalogue; a grep-checkable "no raw literal" rule).

**Priorities.** The owner's classification of 2026-10-13: priority 1 (mandatory) is written `Must`, priority 2 (nice to have) is
written `Could`. Every requirement of this feature is priority 1.

**Mockups (added 2026-10-13, TASK-APP-006).** The owner asked for a tool that draws SVG mockups from the design system, to prototype the
user interface and discuss it before code is generated: static mockups, from a description of the screens that the application reuses
(RQ-APP-074 to RQ-APP-078).

**Depends on** nothing but the build of the application (`RQ-BLD-007`). Every other FTR-APP feature with a user interface depends on it.

**Sources.** `.github/instructions/agnos-sw-eng.instructions.md` (MANDATORY DESIGN SYSTEM), `process/1.requirements/design-system-template.md`.

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: every UI component of `juce/app`; the agents and people who change them; CI (the literal check).

---

## Functional Requirements

### RQ-APP-001: One design-system document, instantiated from the template
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The application SHALL have one design-system document, instantiated from `process/1.requirements/design-system-template.md`, that fills every section of the template (principles, tokens of the three tiers, interaction states, component catalogue, layout patterns, accessibility, framework binding, testability, governance, non-goals) before the first task that implements a UI requirement is marked Ready.
- **Rationale**: the AGNOS process forbids a UI artifact that bypasses the design system.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the design-system document, *When* it is read, *Then* no `[...]` placeholder of the template is left; *Given* a task implementing an FTR-APP UI requirement, *When* its DoR is checked, *Then* it lists the design-system RQ and ADR IDs.
- **Dependencies**: None

### RQ-APP-002: One token module, three tiers
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The application SHALL declare every visual value (colour, spacing, size, typography, stroke width, corner radius, duration) in one token module, in three tiers where a component token aliases a semantic token and a semantic token aliases a global token, and only global tokens hold a literal value; the module SHALL depend on no live UI context (window, graphics context, component tree).
- **Rationale**: a value changed in one place changes everywhere it is used; the module can be tested without a display.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the token module, *When* a unit test resolves every component token, *Then* each resolves through a semantic token to a global token; *Given* the token module, *When* it is compiled into a test with no display, *Then* it builds and the test runs.
- **Dependencies**: RQ-APP-001

### RQ-APP-003: Interaction states decided once
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The design-system document SHALL define once the visual rule of each interaction state the application uses (idle, hover, pressed, focused, disabled, selected, error, and "pending" for a value sent to the sampler and not yet read back), and every component SHALL take the look of a state from that table.
- **Rationale**: the same state looks the same everywhere; the "pending" state is specific to an editor that waits for the sampler.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the state table, *When* a component implements a state, *Then* the component catalogue entry names the state and no state rule is defined inside the component.
- **Dependencies**: RQ-APP-001, RQ-APP-002

### RQ-APP-074: Static SVG mockups drawn from the tokens and a description of the screens
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN the mockup generator runs, it SHALL write one static SVG file per screen and state of the screen descriptions, drawn only with the values of the token file (colours, sizes, spacings, radii, strokes, fonts) and with the example values the descriptions give, each mockup saying in its corner that its values are examples.
- **Rationale**: the owner's request of 2026-10-13: prototype the interface and discuss it quickly, before code is generated.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a description of the main window in its "connected" state, *When* the generator runs, *Then* it writes one SVG whose colours are all values of the token file; *Given* a token's value changed, *When* the generator runs again, *Then* the SVG uses the new value.
- **Dependencies**: RQ-APP-001, RQ-APP-002

### RQ-APP-075: The description of the screens is data the application reuses
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: The descriptions of the screens SHALL be data files from which both the mockup generator and the application take what they share — the editors' tabs, their groups, the parameters of each group by their catalogue names, their roles and their kind of control — so that a change of a group is made once, in one file.
- **Rationale**: the owner's answer of 2026-10-13: the description serves again for the code; the editor layouts of DEC-APP-006 are that data.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a parameter moved from one tab to another in the layout file, *When* the mockups and the application's layout table are generated again, *Then* both show the parameter in its new tab.
- **Dependencies**: RQ-APP-074, RQ-APP-049, RQ-APP-056, RQ-APP-057

### RQ-APP-076: A mockup opens anywhere, alone
- **Category**: Functional
- **EARS Type**: Ubiquitous
- **Statement**: Each mockup SHALL be one self-contained SVG file, with the design system's font embedded in it, that a web browser shows identically without network access and without the font installed.
- **Rationale**: the mockups are exchanged between the owner and the assistant and viewed in a browser.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a mockup, *When* it is opened in a browser on a computer without Inter and without network, *Then* its text is drawn in Inter.
- **Dependencies**: RQ-APP-074

### RQ-APP-077: The committed mockups are never behind their sources
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a committed mockup differs from what the generator writes from the current token file and screen descriptions, THEN the generator's `--check`, run as a `ctest` entry, SHALL fail and name the mockup.
- **Rationale**: the template's §7.4 requires a mechanism that keeps mockups and tokens in step; without it, mockups drift silently.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a token changed and the mockups not regenerated, *When* `ctest` runs, *Then* the check fails and names the mockups to regenerate.
- **Dependencies**: RQ-APP-074

---

## Non-Functional Requirements

### RQ-APP-004: No raw visual literal outside the token module
- **Category**: Non-Functional
- **NFR Type**: Maintainability
- **EARS Type**: Ubiquitous
- **Statement**: The application's UI code SHALL contain no raw colour literal and no raw numeric literal of a tokenised category outside the token module.
- **Metric**: 0 matches of the check over `juce/app` outside the token module.
- **Measurement Method**: a check run as a `ctest` entry, failing when a raw literal is found.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a UI source file with `juce::Colour (0x1e242c)` outside the token module, *When* the check runs, *Then* it fails and names the file and line; *Given* no such literal, *Then* it passes.
- **Dependencies**: RQ-APP-002

### RQ-APP-005: Never colour alone
- **Category**: Non-Functional
- **NFR Type**: Accessibility
- **EARS Type**: Ubiquitous
- **Statement**: The application SHALL convey no state or meaning by colour alone: every state of the state table and every status (connected, error, pending, destructive action) SHALL have a second cue (text, icon, shape or position).
- **Metric**: every row of the state table and every status colour has a second cue named in the design-system document.
- **Measurement Method**: review of the design-system document's state table and status colours.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* the state table, *When* it is reviewed, *Then* each state and status names its second cue.
- **Dependencies**: RQ-APP-003

### RQ-APP-078: The mockup generator is tested
- **Category**: Non-Functional
- **NFR Type**: Maintainability
- **EARS Type**: Ubiquitous
- **Statement**: The mockup generator SHALL have unit tests, written before it, of its reading of the token file and of the descriptions, of the SVG it writes and of its `--check`, run as `ctest` entries when CMake finds Python 3.
- **Metric**: every function of the generator is exercised by at least one test.
- **Measurement Method**: the tests in `juce/tests/tools/`, run by `ctest`.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a description naming a token that does not exist, *When* the generator runs, *Then* it fails and names the token; *Given* the tests, *When* `ctest` runs, *Then* they pass.
- **Dependencies**: RQ-APP-074, RQ-APP-077
