# PLAN-AKM-001: Transport and SysEx Configuration (Phase A, lots A0+A1)

## Overview

Plan for FTR-AKM-001, the first lot of Phase A of the S5000 control plan
(`process/1.requirements/DRAFT-s5000-midi-primitives-and-workflows.md`): the AKAI SysEx transport and
section §00. It also carries the authoring of the four Phase A feature files, since they were written
in the same session. FTR-AKM-002, FTR-AKM-003 and FTR-AKM-004 (Program, Keygroup, Zone) get their own
plans, written when each lot starts, on top of what this one delivers.

## References
- **Requirements**: FTR-AKM-001 (RQ-AKM-001 to RQ-AKM-020, RQ-AKM-039 to RQ-AKM-043); FTR-AKM-002 (RQ-AKM-021 to RQ-AKM-027), FTR-AKM-003 (RQ-AKM-028 to RQ-AKM-033) and FTR-AKM-004 (RQ-AKM-034 to RQ-AKM-038) for TASK-AKM-001 only
- **ADRs**: ADR-AKM-001 (Accepted)

Implementation tasks (TASK-AKM-003 onward) are for later sessions run with `unit_tests = true`: their
acceptance criteria are written as tests, test-first (§4 of the process instructions). The plan has 12
tasks, more than one session should carry (context-management rule of at most 10 per session): a natural
split is TASK-AKM-003, 004, 005, 007 (foundations, no hardware); TASK-AKM-012 (first contact with the
sampler); TASK-AKM-006, 008 (session and §00); TASK-AKM-009, 011, 010 (opening, closing, real-sampler suite).
The plan was reworked after an independent review of ADR-AKM-001
(`process/2.architecture/REVIEW-ADR-AKM-001-opus.md`).

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-AKM-001: Author the Phase A feature files
- **Tier**: M
- **Status**: Done
- **Description**: Write FTR-AKM-001 in full (transport and §00) and FTR-AKM-002 to FTR-AKM-004 (§0A, §08, §06) at the level of the spec's item groups, from `documents/_index/sysex_spec.kb.md`, `documents/_index/sysex_spec.items.tsv` and the spec text.
- **Requirement refs**: RQ-AKM-001 to RQ-AKM-040
- **ADR refs**: None
- **Acceptance Criteria** (Gherkin): *Given* the four files, *When* the process index is regenerated, *Then* it reports no duplicate ID and lists FTR-AKM-001 to FTR-AKM-004 and RQ-AKM-001 to RQ-AKM-040. *Given* the item counts stated in FTR-AKM-002, FTR-AKM-003 and FTR-AKM-004, *When* compared with `documents/_index/sysex_spec.items.tsv`, *Then* they match. *Given* each requirement, *When* read, *Then* it has an EARS statement, a rationale, a priority, a Gherkin criterion and its dependencies.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Re-run in this session: `agnos-index` exit 0 (no duplicate), 78 entries with this plan; the item counts of sections `00`, `06`, `08`, `0A` (7; 28 + 14; 80 + 40; 95 + 46) and the sub-group counts were read from `documents/_index/sysex_spec.items.tsv` by script and match the tables in the three outline files; the Table 5 footnotes and the confirmation and checksum passages (pp. 3–6) were read in the spec text. Not verified: the value ranges of the individual items of §0A, §08 and §06 (checked only at group level; refined when each lot starts), and every frame shown in an acceptance criterion as "constructed" (to be replaced by captured frames under RQ-AKM-017).
- **Assumptions**: Artifacts are written in English, like the existing ones; FTR-AKM-001's file name keeps the French slug announced to the owner. The two additions RQ-AKM-039 and RQ-AKM-040 came out of the owner's decisions of this session (DeviceID as an application setting; known §00 state at session start). RQ-AKM-041 to RQ-AKM-043 were added later in the session, after the independent review of ADR-AKM-001, and are recorded in TASK-AKM-002.

---

### TASK-AKM-002: Decide the AKM architecture
- **Tier**: L
- **Status**: Done
- **Description**: Write ADR-AKM-001: where the AKM code lives, its layers, the shape of primitives, the threading and completion model, time injection, session opening, the checksum mode and the test seams, from the existing `xs56k_midi` interfaces and the requirements of FTR-AKM-001; have it reviewed independently and rework it; add to FTR-AKM-001 the requirements that the review showed to be missing (RQ-AKM-041 to RQ-AKM-043) and correct those it showed to be wrong (RQ-AKM-007, 010, 011, 016, 020, 039, 040).
- **Requirement refs**: RQ-AKM-007, RQ-AKM-008, RQ-AKM-010, RQ-AKM-016, RQ-AKM-019, RQ-AKM-020, RQ-AKM-039, RQ-AKM-040, RQ-AKM-041, RQ-AKM-042, RQ-AKM-043
- **ADR refs**: ADR-AKM-001
- **Acceptance Criteria** (Gherkin): *Given* ADR-AKM-001, *When* read, *Then* every decision is a `DEC-AKM-NNN` heading with the requirement IDs it serves, alternatives are listed with a reason for each rejection, and a diagram is present. *Given* the independent review, *When* its findings are compared with the ADR, *Then* each is either reflected in it or refused with a reason. *Given* the ADR, *When* the owner reviews it, *Then* its status is set to Accepted or the requested changes are made.
- **Dependencies**: TASK-AKM-001
- **Assignee**: AI, with the owner's approval
- **Verification**: The owner reviewed the reworked ADR-AKM-001 and accepted it, whereupon its status was set to Accepted. Re-read in this session: the `MidiBackend`, `MidiInputPort`, `MidiOutputPort` and `MockMidiBackend` headers (synchronous delivery, no send hook), `MidiMessage::sysEx` and the classification of `F0 F7` as SysEx in `MidiMessage.cpp`, and ADR-BLD-001's anticipation of `juce/tests` and Catch2. An independent review (a read-only Opus subagent) found two blocking and several important defects; the main session checked its claims against the JUCE 8.0.15 sources (blocking Windows SysEx send, shared `mainPackets`, `MidiInput::stop()` under the callback's spin lock), the mock (`started` not atomic) and the spec (DeviceID rules, OS versions of `&03` and `&07`, synchronisation default), and reworked DEC-AKM-004, DEC-AKM-005 and DEC-AKM-007, amended DEC-AKM-003 and DEC-AKM-008, and added DEC-AKM-009 and DEC-AKM-010; findings and dispositions are in `process/2.architecture/REVIEW-ADR-AKM-001-opus.md`. Not verified: Microsoft's guidance on calling the multimedia API from an input callback (the ADR does not rely on it), macOS CoreMIDI, and every firmware behaviour (RQ-AKM-017).
- **Assumptions**: The provisional session defaults of DEC-AKM-007 (checksum off, Sync LCD off, Still Alive on, Notification and Auto screen update unchanged) are my inference from the spec's text and the review; the owner approved having a default configuration, not these values. Namespace `akm` and directory names follow the existing `common::midi` / `midiapp` pattern.

---

### TASK-AKM-003: Library skeleton and test scaffolding
- **Tier**: L
- **Status**: Not Started
- **Description**: Add `juce/akm/` with the `xs56k_akm` static library (empty but for a library-info function), wire it into `juce/CMakeLists.txt`, add `juce/tests/` with Catch2 behind `BUILD_TESTS`, one smoke test, and a `ctest` step in the headless CI workflows (through `juce/tools/generate_workflows.py` where those are generated).
- **Requirement refs**: RQ-AKM-019, RQ-AKM-016, RQ-BLD-002, RQ-BLD-003, RQ-BLD-004
- **ADR refs**: ADR-AKM-001 (DEC-AKM-001, DEC-AKM-008)
- **Acceptance Criteria** (Gherkin): *Given* a fresh build directory, *When* `cmake -S juce -B <dir>` and a full build are run with default options, *Then* `xs56k_akm` builds warning-free and links only `xs56k_midi`. *Given* `-DBUILD_TESTS=ON`, *When* configured, built and `ctest` run, *Then* the smoke test passes. *Given* the public headers of `xs56k_akm`, *When* searched for a JUCE include, *Then* there is no match. *Given* the headless CI workflows, *When* a branch is pushed, *Then* they run `ctest` and report its result.
- **Dependencies**: TASK-AKM-002
- **Assignee**: AI
- **Verification**: Not yet run.
- **Assumptions**: None yet.

---

### TASK-AKM-004: Codec
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement the pure codec of DEC-AKM-002 and DEC-AKM-009: command frame encoding, value formats, checksum in its three modes (on, off, unknown), confirmation decoding, error table and rejection of malformed or foreign messages.
- **Requirement refs**: RQ-AKM-001, RQ-AKM-002, RQ-AKM-003, RQ-AKM-004, RQ-AKM-005, RQ-AKM-006, RQ-AKM-041
- **ADR refs**: ADR-AKM-001 (DEC-AKM-002, DEC-AKM-009)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-001 to RQ-AKM-006 and of the decoding part of RQ-AKM-041, each written as a test first — in particular the spec's own frame `F0 47 5E 05 10 0C 1B 35 6D 59 F7`, the word `03 01` = 385, the signed word −37 = `01 00 25`, the rejection of a foreign, truncated or wrongly checksummed message without an exception, and, in unknown mode, a DONE decoded with and without a trailing valid checksum and rejected with a wrong extra byte.
- **Dependencies**: TASK-AKM-003
- **Assignee**: AI
- **Verification**: Not yet run.
- **Assumptions**: None yet.

---

### TASK-AKM-005: Scheduler and executor
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement the `Scheduler` interface of DEC-AKM-006 (real: standard-library timer thread; manual: advanced by tests) and the serial `Executor` interface of DEC-AKM-004 (real: worker thread; manual: drained by the test with `runUntilIdle()`).
- **Requirement refs**: RQ-AKM-010, RQ-AKM-011, RQ-AKM-012, RQ-AKM-016, RQ-AKM-019, RQ-AKM-020
- **ADR refs**: ADR-AKM-001 (DEC-AKM-004, DEC-AKM-006)
- **Acceptance Criteria** (Gherkin): *Given* the manual scheduler and a task scheduled 100 ms ahead, *When* time is advanced by 99 ms then 1 ms, *Then* the task runs only after the second advance. *Given* a task scheduled for 200 ms and then one for 100 ms, *When* time is advanced by 100 ms, *Then* the second runs and the first does not (a later deadline never hides an earlier one). *Given* a scheduled task, *When* it is cancelled before its time, or from inside another task, *Then* it never runs. *Given* a task that schedules another, *When* time is advanced, *Then* the new task runs at its own time. *Given* the real scheduler and a task scheduled 50 ms ahead, *When* 200 ms of wall-clock time pass, *Then* it ran exactly once, on a thread other than the caller's. *Given* the real executor and tasks posted from three threads, *When* they run, *Then* they never overlap and each thread's tasks keep their order. *Given* the real scheduler or executor destroyed with a task pending, *When* it is destroyed, *Then* it returns without running the task and without a data race.
- **Dependencies**: TASK-AKM-003
- **Assignee**: AI
- **Verification**: Not yet run.
- **Assumptions**: None yet.

---

### TASK-AKM-006: Session core
- **Tier**: L
- **Status**: Not Started
- **Description**: Implement the session state machine of DEC-AKM-002, DEC-AKM-004, DEC-AKM-005, DEC-AKM-009 and DEC-AKM-010 on the serial executor: user-ref allocation and matching (including unsolicited confirmations and a late ERROR after a REPLY), FIFO with one command in flight, sequences that stop at the first failure, completion on DONE, REPLY or ERROR, timeout with generation tokens and a maximum total wait, Still Alive, the discovery collection window, the tri-state checksum mode with its transitions, the `Cancelled` result, and an input callback that only enqueues.
- **Requirement refs**: RQ-AKM-007, RQ-AKM-008, RQ-AKM-009, RQ-AKM-010, RQ-AKM-011, RQ-AKM-012, RQ-AKM-013, RQ-AKM-020, RQ-AKM-041, RQ-AKM-043
- **ADR refs**: ADR-AKM-001 (DEC-AKM-004, DEC-AKM-005, DEC-AKM-009, DEC-AKM-010)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-007 to RQ-AKM-011, RQ-AKM-013, RQ-AKM-020, RQ-AKM-041 and RQ-AKM-043, each written as a test first — in particular: three back-to-back commands put only the first frame on the wire until its DONE arrives; a late ERROR carrying the previous user-ref is reported as unsolicited and leaves the next command pending; a confirmation delivered synchronously inside `send()`, and one delivered from another thread before `send()` returns, each complete the command exactly once; a confirmation and a timeout racing produce exactly one completion; a silent sampler times out on the manual scheduler and the next command is sent; `F0 F7` restarts the timeout only when Still Alive is on and never beyond the maximum total wait; a completion that calls `submit()` queues behind the commands already queued; no completion runs on the backend's callback thread; a sequence whose first command fails cancels the rest; two responders to a broadcast Query are both collected before the window ends.
- **Dependencies**: TASK-AKM-004, TASK-AKM-005, TASK-AKM-007, TASK-AKM-012
- **Assignee**: AI
- **Verification**: Not yet run.
- **Assumptions**: None yet.

---

### TASK-AKM-007: Simulated sampler and scenario harness
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement the simulated sampler of DEC-AKM-008 as a `MidiBackend` in the test tree, modelled on the spec (addressing including DeviceID 0, §00 state kept per port across sessions, checksum handling and ERROR `81`, OK / DONE / REPLY / ERROR flows including a REPLY followed by an ERROR, items unsupported by older OS versions), with knobs to answer late, never, twice, foreign, malformed or with `F0 F7`, and delivering on the sending thread or from another thread; and the `ScenarioDriver` that runs a scenario written against `MidiBackend&` with the manual or the real scheduler and executor.
- **Requirement refs**: RQ-AKM-016, RQ-AKM-019
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008)
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler in its default mode, *When* a frame is sent through its output port, *Then* the confirmations arrive on its input port with the user-refs of the frame. *Given* it scripted to stay silent, *When* a frame is sent, *Then* nothing is delivered. *Given* two simulated samplers with DeviceIDs 3 and 7 and a Query with DeviceID 0, *When* it is sent, *Then* both answer; *Given* a Query with DeviceID 5, *Then* neither answers. *Given* a sampler with DeviceID 0, *When* a command with DeviceID 5 is sent, *Then* it executes it. *Given* checksum on in the simulation and a frame without checksum, *When* it is sent, *Then* it answers ERROR `81`. *Given* the mode "deliver from another thread", *When* a frame is sent, *Then* the confirmation can arrive before `send()` returns. *Given* a scenario written against `MidiBackend&`, *When* run on the simulated sampler, *Then* the same source compiles against `JuceMidiBackend` (build only, no hardware).
- **Dependencies**: TASK-AKM-003, TASK-AKM-004, TASK-AKM-005
- **Assignee**: AI
- **Verification**: Not yet run.
- **Assumptions**: None yet.

---

### TASK-AKM-008: Item catalogue and §00 primitives
- **Tier**: M
- **Status**: Not Started
- **Description**: Create the human-reviewed data file for the items and the script that generates the checked-in descriptor table from it, checks that the table is up to date, and compares the data file with `documents/_index/sysex_spec.items.tsv` (DEC-AKM-003), with the 7 records of §00; implement the generic encoder and range validator and the §00 helpers on top of the session: discovery, checksum-mode command, the four other toggles, and Echo.
- **Requirement refs**: RQ-AKM-001, RQ-AKM-012, RQ-AKM-013, RQ-AKM-014, RQ-AKM-015
- **ADR refs**: ADR-AKM-001 (DEC-AKM-003)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-012 to RQ-AKM-015, each written as a test first on the simulated sampler — in particular discovery returning {3, 7} for two samplers, checksum mode changing the framing of the following command, the value `2` refused for the four toggles, and Echo failing with the mismatch error on `01 23 45 66`. *Given* the data file and the checked-in table, *When* the check runs, *Then* it fails if they differ. *Given* the data file and the TSV, *When* the coverage script runs, *Then* it reports the 7 rows of section `00` as covered and lists any unaccounted row. No script runs during the build.
- **Dependencies**: TASK-AKM-006, TASK-AKM-007
- **Assignee**: AI
- **Verification**: Not yet run.
- **Assumptions**: None yet.

---

### TASK-AKM-009: Session opening
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement `Session::open` (DEC-AKM-007): the configuration with its tri-state §00 settings and defaults, the input started first, the broadcast discovery, the target DeviceID verification and its failure reports (absent, ambiguous), the explicit §00 establishment starting with the checksum mode, and the degraded mode for unsupported items.
- **Requirement refs**: RQ-AKM-039, RQ-AKM-040, RQ-AKM-013, RQ-AKM-041
- **ADR refs**: ADR-AKM-001 (DEC-AKM-007, DEC-AKM-009)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-039 and RQ-AKM-040, each written as a test first — in particular "no sampler at DeviceID 5" with the list `[3]`, the ambiguity refusal for target 0 with two samplers and for target 3 with samplers 0 and 3, a sampler left with checksum on being reset with a checksummed command, a setting configured as unchanged never being sent, a sampler answering ERROR `00` to Still Alive opening the session degraded, and the session reported failed when the checksum-mode command times out.
- **Dependencies**: TASK-AKM-008
- **Assignee**: AI
- **Verification**: Not yet run.
- **Assumptions**: None yet.

---

### TASK-AKM-010: Real-sampler suite and observations
- **Tier**: L
- **Status**: Not Started
- **Description**: Add the opt-in real-sampler suite (configured by DeviceID, port names and optional sample name, ending in a known state), run it against the S5000, complete the observations of RQ-AKM-017 that TASK-AKM-012 did not cover, and set the provisional timeout, maximum total wait, discovery window and session defaults from them.
- **Requirement refs**: RQ-AKM-010, RQ-AKM-012, RQ-AKM-017, RQ-AKM-018, RQ-AKM-039, RQ-AKM-040, RQ-AKM-042
- **ADR refs**: ADR-AKM-001 (DEC-AKM-006, DEC-AKM-007, DEC-AKM-008)
- **Acceptance Criteria** (Gherkin): *Given* the S5000 connected, *When* the suite runs, *Then* the Echo test passes and the observations listed in RQ-AKM-017 are recorded, including 50 Echo latencies, the DeviceID carried by confirmations, and whether the JUCE backend delivers `F0 F7`. *Given* the recorded latencies, *When* the defaults are set, *Then* the timeout and window constants carry the measured values and their margin. *Given* a test that fails midway, *When* the suite ends, *Then* the checksum mode is back to the recorded default (RQ-AKM-018). ADR-AKM-001 is amended if an observation contradicts a decision (for example the session defaults).
- **Dependencies**: TASK-AKM-009, TASK-AKM-011, TASK-AKM-012
- **Assignee**: AI for the suite; the owner for the run against the hardware
- **Verification**: Not yet run.
- **Assumptions**: None yet.

---

### TASK-AKM-011: Session closing
- **Tier**: M
- **Status**: Not Started
- **Description**: Implement `Session::close` (DEC-AKM-004, RQ-AKM-042): the in-flight and queued commands completed as `Cancelled`, the best-effort restore of the documented §00 defaults, the input stopped outside any session lock, the executor joined, and the refusal of a close requested from one of the session's own completions.
- **Requirement refs**: RQ-AKM-042, RQ-AKM-013, RQ-AKM-040
- **ADR refs**: ADR-AKM-001 (DEC-AKM-004)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-042, each written as a test first — in particular the in-flight and queued commands completing as cancelled before the checksum-off command is sent, closing returning after the restoring commands time out on a silent sampler, and a close from a completion being refused. *Given* a session destroyed without an explicit close, *When* it is destroyed, *Then* it performs the same shutdown without a data race.
- **Dependencies**: TASK-AKM-009
- **Assignee**: AI
- **Verification**: Not yet run.
- **Assumptions**: None yet.

---

### TASK-AKM-012: First contact with the sampler
- **Tier**: M
- **Status**: Not Started
- **Description**: Add a small probe program in the test tree (built with `BUILD_TESTS`, not shipped) that opens the S5000 through `JuceMidiBackend`, sends an Echo and a broadcast Query with checksum off and on, and logs every frame in both directions with timestamps; the owner runs it and the log is kept. It fixes early the facts the session design rests on: which DeviceID confirmations carry, the shape of DONE and ERROR, whether confirmations carry a checksum while it is on, whether `F0 F7` is delivered, the response latency, and the sampler's OS version.
- **Requirement refs**: RQ-AKM-017, RQ-AKM-004, RQ-AKM-007, RQ-AKM-011
- **ADR refs**: ADR-AKM-001 (DEC-AKM-007, DEC-AKM-009)
- **Acceptance Criteria** (Gherkin): *Given* the S5000 connected and its ports visible, *When* the probe runs, *Then* it writes a log with one line per frame (direction, time, bytes). *Given* the log, *When* read, *Then* it shows at least one Echo REPLY, one Query DONE, and the answers with checksum on and off. *Given* the observations, *When* they contradict a decision or an assumption of ADR-AKM-001 or of a requirement, *Then* those are amended before TASK-AKM-006 starts.
- **Dependencies**: TASK-AKM-003, TASK-AKM-004
- **Assignee**: AI for the probe; the owner for the run against the hardware
- **Verification**: Not yet run.
- **Assumptions**: Only the codec (TASK-AKM-004) and the existing JUCE backend are needed, so it can run before the session exists; the probe sends its frames by hand, one at a time, without the session's flow control.
