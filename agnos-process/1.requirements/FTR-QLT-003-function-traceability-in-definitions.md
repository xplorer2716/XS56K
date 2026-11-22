# FTR-QLT-003: Function Traceability in Definitions

## Overview

Session QLT (2026-11-22). The project's rule puts the IDs of a C++ function in its header's doc comment; the AGNOS performance center counts a function only where it has a
body, from the IDs of its lines and of the comment block just above it. Its `functions-traced` check scores 1.3/10: 1129 production definitions of `.cpp` files cite no ID, though
many have a commented declaration in a header. The owner asked to copy the IDs of each declaration onto its definition.

**In scope.** A tool that copies the IDs of a header's function declaration onto the matching definition of a `.cpp` file; its run on the production code, then on the test code.
**Out of scope.** IDs written above a class, not above a function (owner's choice: functions only); definitions with no declaration in a header (helpers of an anonymous namespace);
inline definitions of headers, already counted.

## Stakeholders

- **Owner**: the project maintainer ([xplorer2716](https://github.com/xplorer2716))
- **Consumers**: the AGNOS performance center (`--call-graph`); whoever greps a requirement ID.

---

## Functional Requirements

### RQ-QLT-014: Function ID copy from header to definition
- **Category**: Functional
- **EARS Type**: Event-driven
- **Statement**: WHEN `juce/tools/trace_header_ids.py` runs on a folder, the tool SHALL, for each function definition of a `.cpp` file that matches exactly one function declaration of a header of `juce/` by qualified name and parameter types, add the AGNOS IDs of the declaration's comment that the definition's comment block and lines do not cite, as one `// [<ID>, ...]` line just above the definition, and change nothing else in the file.
- **Rationale**: the performance center then counts the function as traced, and a grep of a requirement ID finds its implementation.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* a header declaring `void f(int)` under the comment `/// Does f. [RQ-ABC-001, DEC-ABC-002]` and a `.cpp` defining `void ns::f(int)` with no comment, *When* the tool runs, *Then* the line `// [RQ-ABC-001, DEC-ABC-002]` is just above the definition. *Given* a definition whose comment already cites `RQ-ABC-001`, *When* the tool runs, *Then* only `DEC-ABC-002` is added. *Given* a second run, *When* the tool runs again, *Then* no file changes.
- **Dependencies**: ADR-QLT-003 (DEC-QLT-015, DEC-QLT-016, DEC-QLT-017)

### RQ-QLT-015: Untraceable definition report
- **Category**: Functional
- **EARS Type**: Unwanted-behavior
- **Statement**: IF a declaration with IDs matches no definition or several, or `--check` is given and a definition lacks some IDs of its declaration, THEN the tool SHALL write nothing for it, print it with its file and line, print the counts (copied, already traced, unmatched, ambiguous), and exit 1 in `--check` mode.
- **Rationale**: an overload taken for another would put wrong IDs on a function; a silent skip would hide the gap.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* two overloads `f(int)` and `f(double)` each declared with its own IDs, *When* the tool runs, *Then* each definition gets the IDs of its own declaration. *Given* a definition whose parameters match no declaration, *When* the tool runs, *Then* it is listed as unmatched and its file is unchanged. *Given* `--check` and a definition missing an ID, *When* the tool runs, *Then* it exits 1 and writes nothing.
- **Dependencies**: RQ-QLT-014; ADR-QLT-003 (DEC-QLT-016)

## Non-Functional Requirements

### RQ-QLT-016: Tool test coverage
- **Category**: Non-Functional
- **NFR Type**: Maintainability
- **EARS Type**: Ubiquitous
- **Statement**: The tool's matching, its insertion and its `--check` mode SHALL be covered by unit tests run by `ctest`, which report themselves skipped, not passed, where tree-sitter is not installed.
- **Metric**: a ctest entry, passing where tree-sitter is installed.
- **Measurement Method**: `ctest -R qlt_trace_ids`.
- **Priority**: Must
- **Acceptance Criteria** (Gherkin): *Given* tree-sitter installed, *When* the tests run, *Then* they pass. *Given* it absent, *When* they run, *Then* ctest shows them skipped.
- **Dependencies**: RQ-QLT-014, RQ-QLT-015; ADR-QLT-003 (DEC-QLT-015)
