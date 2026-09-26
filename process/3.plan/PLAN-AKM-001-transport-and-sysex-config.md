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
- **Status**: Done
- **Description**: Add `juce/akm/` with the `xs56k_akm` static library (empty but for a library-info function), wire it into `juce/CMakeLists.txt`, add `juce/tests/` with Catch2 behind `BUILD_TESTS`, one smoke test, and a `ctest` step in the two headless CI workflows (`linux-headless-canary.yml` and `linux-headless-preprod.yml`; they are hand-written — `juce/tools/generate_workflows.py` generates only the deployment workflows, which this task leaves alone).
- **Requirement refs**: RQ-AKM-019, RQ-AKM-016, RQ-BLD-002, RQ-BLD-003, RQ-BLD-004
- **ADR refs**: ADR-AKM-001 (DEC-AKM-001, DEC-AKM-008)
- **Acceptance Criteria** (Gherkin): *Given* a fresh build directory, *When* `cmake -S juce -B <dir>` and a full build are run with default options, *Then* `xs56k_akm` builds warning-free and links only `xs56k_midi`. *Given* `-DBUILD_TESTS=ON`, *When* configured, built and `ctest` run, *Then* the smoke test passes. *Given* the public headers of `xs56k_akm`, *When* searched for a JUCE include, *Then* there is no match. *Given* the headless CI workflows, *When* a branch is pushed, *Then* they run `ctest` and report its result.
- **Dependencies**: TASK-AKM-002
- **Assignee**: AI
- **Verification**: Windows/MSVC 19.51, re-run this session. AC 1: fresh directory, default options: 4 libraries built, 0 warnings, Catch2 not fetched; `xs56k_akm` links only `xs56k_midi` (`cmake --graphviz`). AC 2: `-DBUILD_TESTS=ON`, Debug and Release: 0 warnings, `ctest` 2/2. AC 3: the header-scan test passes and fails on a constructed JUCE include; Grep finds 0. AC 4: `linux-headless-canary` run 36239401937 green on GCC 11.4.0, `ctest` 2/2. Not verified: macOS; `linux-headless-preprod` (triggers on `dev` only).
- **Assumptions**: One executable `xs56k_akm_tests`, its sources under `juce/tests/akm/` mirroring the library. The no-JUCE-include rule of RQ-AKM-019 is a permanent ctest (`juce/tests/CheckNoJuceIncludes.cmake`) in addition to the Catch2 smoke test, since every later task adds public headers. Only the two hand-written headless workflows run `ctest`; the generated Windows and macOS deployment workflows (whose `build-app` action already has a `run-tests` input) are untouched, so tests run on Linux only for now. Catch2's include directories are marked SYSTEM in `juce/CMakeLists.txt` (third-party code, as JUCE's, RQ-BLD-003): a defensive choice, because GCC's `-Wpedantic -Werror` would otherwise compile its headers as project code and it could not be tried here. `README.md` and `CONTRIBUTING.md` keep their "not defined yet" text, which predates this task; `AGENTS.md` now documents the build and test commands. Local iteration used `FETCHCONTENT_SOURCE_DIR_JUCE` to avoid re-cloning JUCE; the AC 1 run did not.

---

### TASK-AKM-004: Codec
- **Tier**: M
- **Status**: Done
- **Description**: Implement the pure codec of DEC-AKM-002 and DEC-AKM-009: command frame encoding, value formats, checksum in its three modes (on, off, unknown), confirmation decoding, error table and rejection of malformed or foreign messages.
- **Requirement refs**: RQ-AKM-001, RQ-AKM-002, RQ-AKM-003, RQ-AKM-004, RQ-AKM-005, RQ-AKM-006, RQ-AKM-041
- **ADR refs**: ADR-AKM-001 (DEC-AKM-002, DEC-AKM-009)
- **Acceptance Criteria** (Gherkin): the Gherkin criteria of RQ-AKM-001 to RQ-AKM-006 and of the decoding part of RQ-AKM-041, each written as a test first — in particular the spec's own frame `F0 47 5E 05 10 0C 1B 35 6D 59 F7`, the word `03 01` = 385, the signed word −37 = `01 00 25`, the rejection of a foreign, truncated or wrongly checksummed message without an exception, and, in unknown mode, a DONE decoded with and without a trailing valid checksum and rejected with a wrong extra byte.
- **Dependencies**: TASK-AKM-003
- **Assignee**: AI
- **Verification**: Windows/MSVC 19.51, re-run this session, Debug and Release: 0 warnings, `ctest` 73/73 (71 new test cases named after the ACs); red run seen first (missing headers). Covered: the spec's frames, word 385, signed word −37, rejection of foreign/truncated/badly checksummed messages without exception, unknown-mode DONE with, without and with a wrong checksum; the checksums in the test frames were added by hand. Six code mutations were each caught by 1–6 tests. `linux-headless-canary` run 36240893051 succeeded on `c085721`. Not verified: checksum coverage of confirmations and the constructed frames on the real S5000 (RQ-AKM-017).
- **Assumptions**: The codec takes bytes (`std::span`, `std::vector`), not `MidiMessage`, to stay independent of that class; it uses only its `SYSEX_START` and `SYSEX_END` constants. The checksum of a confirmation covers the bytes from the first user-ref to the last data byte, the Reply ID included: the spec (p. 4) defines the range for commands only (to check under RQ-AKM-017). In mode unknown the expected data lengths are those of DEC-AKM-009 (OK and DONE none, ERROR two, the Echo REPLY four), any other REPLY is rejected as `UnknownDataLength`, and the Echo item (§00, `&06`) is a private constant of the codec. `F0 F7` decodes as a `StillAliveMessage`, since RQ-AKM-006 says it is not malformed. A decoded ERROR with fewer than two data bytes has no error number (the session decides what that means), and a byte after Data1 and Data2 is ignored. In mode On, a message with nothing after the item is a `BadChecksum`, which is a failed verification for the run of three of RQ-AKM-041. A negative zero (sign `01`, magnitude 0) is read as 0 and never written; a sign byte other than `00` or `01` is refused. `encodeCommand` refuses any user-ref, section, item or data byte above `7F`, but not the reserved sections `44`, `45`, `4F` and `52` of spec Table 4, which no requirement asks for. The error meanings are my paraphrase of Table 3. Value formats are a `ByteWriter` (a refused value leaves the writer failed, so a run of appends is checked once) and a `ByteReader` (a failed read consumes nothing), which the generic encoder of TASK-AKM-008 will build on.

---

### TASK-AKM-005: Scheduler and executor
- **Tier**: M
- **Status**: Done
- **Description**: Implement the `Scheduler` interface of DEC-AKM-006 (real: standard-library timer thread; manual: advanced by tests) and the serial `Executor` interface of DEC-AKM-004 (real: worker thread; manual: drained by the test with `runUntilIdle()`).
- **Requirement refs**: RQ-AKM-010, RQ-AKM-011, RQ-AKM-012, RQ-AKM-016, RQ-AKM-019, RQ-AKM-020
- **ADR refs**: ADR-AKM-001 (DEC-AKM-004, DEC-AKM-006)
- **Acceptance Criteria** (Gherkin): *Given* the manual scheduler and a task scheduled 100 ms ahead, *When* time is advanced by 99 ms then 1 ms, *Then* the task runs only after the second advance. *Given* a task scheduled for 200 ms and then one for 100 ms, *When* time is advanced by 100 ms, *Then* the second runs and the first does not (a later deadline never hides an earlier one). *Given* a scheduled task, *When* it is cancelled before its time, or from inside another task, *Then* it never runs. *Given* a task that schedules another, *When* time is advanced, *Then* the new task runs at its own time. *Given* the real scheduler and a task scheduled 50 ms ahead, *When* 200 ms of wall-clock time pass, *Then* it ran exactly once, on a thread other than the caller's. *Given* the real executor and tasks posted from three threads, *When* they run, *Then* they never overlap and each thread's tasks keep their order. *Given* the real scheduler or executor destroyed with a task pending, *When* it is destroyed, *Then* it returns without running the task and without a data race.
- **Dependencies**: TASK-AKM-003
- **Assignee**: AI
- **Verification**: Windows/MSVC 19.51, re-run this session, Debug and Release: 0 warnings, `ctest` 103/103 (30 new). Covered: manual scheduler ordering, cancellation and `now()` at each deadline; real scheduler: a 50 ms task ran once on another thread, deadline order, cancellation, destruction with a task an hour ahead; real executor: 3 producers × 500 tasks with no overlap and per-producer order, discard on destruction; manual executor drain order and thread. Seven mutations were each caught by 1–2 tests; the 11 real-thread tests passed 30 repeats. `linux-headless-canary` run 36241914566 succeeded on `8388af1` (GCC 11.4.0). Not verified: data races under a sanitizer (none on MSVC, no CI job); macOS.
- **Assumptions**: The manual scheduler and the manual executor live in `xs56k_akm` beside the real ones, as `MockMidiBackend` lives in `xs56k_midi`, rather than in the test tree; the ADR says only that the manual ones are advanced or drained by tests. `Scheduler::Clock` is `std::chrono::steady_clock`, and the manual clock starts at its epoch. `TimerHandle::cancel()` never blocks and does not stop a task that has already started (the generation token of DEC-AKM-004 covers that); a cancelled task stays in the queue until it reaches the front, and `pendingCount()` counts the live ones. Tasks must not throw (a throw on the timer or worker thread ends the process). `Executor::isCurrentThread()` is added to the interface because DEC-AKM-004 asserts against a `close()` from a session's own completion. Destroying a `ThreadExecutor` discards the tasks not yet started, as the acceptance criterion says, so a session's `close()` posts its final task and waits for it before the executor goes. The "executor destroyed with a task pending" test needs a 300 ms wait for the destroying thread to be scheduled before the running task is let go: it is the one timing-dependent assertion, and if that thread were stalled longer the failure would be spurious, not a hidden defect. The tests for this task were written before the code, but I did not run the build to see them fail first (the seven mutations are my evidence that they can fail). `xs56k_akm` now links `Threads::Threads`.

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
- **Status**: Done
- **Description**: Implement the simulated sampler of DEC-AKM-008 as a `MidiBackend` in the test tree, modelled on the spec (addressing including DeviceID 0, §00 state kept per port across sessions, checksum handling and ERROR `81`, OK / DONE / REPLY / ERROR flows including a REPLY followed by an ERROR, items unsupported by older OS versions), with knobs to answer late, never, twice, foreign, malformed or with `F0 F7`, and delivering on the sending thread or from another thread; and the `ScenarioDriver` that runs a scenario written against `MidiBackend&` with the manual or the real scheduler and executor.
- **Requirement refs**: RQ-AKM-016, RQ-AKM-019
- **ADR refs**: ADR-AKM-001 (DEC-AKM-008)
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler in its default mode, *When* a frame is sent through its output port, *Then* the confirmations arrive on its input port with the user-refs of the frame. *Given* it scripted to stay silent, *When* a frame is sent, *Then* nothing is delivered. *Given* two simulated samplers with DeviceIDs 3 and 7 and a Query with DeviceID 0, *When* it is sent, *Then* both answer; *Given* a Query with DeviceID 5, *Then* neither answers. *Given* a sampler with DeviceID 0, *When* a command with DeviceID 5 is sent, *Then* it executes it. *Given* checksum on in the simulation and a frame without checksum, *When* it is sent, *Then* it answers ERROR `81`. *Given* the mode "deliver from another thread", *When* a frame is sent, *Then* the confirmation can arrive before `send()` returns. *Given* a scenario written against `MidiBackend&`, *When* run on the simulated sampler, *Then* the same source compiles against `JuceMidiBackend` (build only, no hardware).
- **Dependencies**: TASK-AKM-003, TASK-AKM-004, TASK-AKM-005
- **Assignee**: AI
- **Verification**: Windows/MSVC 19.51, re-run this session, Debug and Release: 0 warnings, `ctest` 150/150 (47 new); tests written first, but the red run was not executed separately. Every AC above is covered (user-refs echoed; silent; DeviceIDs 3 and 7 with a Query to 0, 5 and 3; DeviceID 0 accepting a command for 5; ERROR 81 for a frame without checksum; delivery from another thread before and after `send()` returns), plus the late/twice/foreign/malformed/`F0 F7`/Still Alive/REPLY-then-ERROR knobs, OS-version items, settings kept across sessions and the Echo scenario on the manual and real drivers. The `xs56k_akm_scenarios_on_juce` target builds that Echo scenario against `JuceMidiBackend` in both configurations. Nine code mutations were each caught by 1–3 tests; the 38 threaded and scenario tests passed 30 repeats. `linux-headless-canary` run 36254482080 on `9d3f6ff`: `ctest` 150/150 on GCC 11.4.0, after two failed pushes (headers hidden by the `Testing/` rule of `.gitignore`; GCC's `-Wmissing-field-initializers`), both fixed; a fresh clone of the fixed tree also built and passed on MSVC. Not verified: macOS; the model against the real S5000 (RQ-AKM-017).
- **Assumptions**: The simulation, the drivers and the Echo scenario are in a test-support library `xs56k_akm_test_support` (`juce/tests/support`, namespace `akm::harness`; not `testing`, because the `Testing/` rule of `.gitignore` also matches that directory name on a case-insensitive file system, which left the headers out of my first commit); the JUCE check is `juce/tests/compile_checks`, compiled and never linked. One `SimulatedSampler` is one port, several sit on one bus; `powerCycle()` resets its settings (whether §00 survives a power cycle on the real sampler is unknown). Where the spec is silent the model chooses, and each choice is documented and, when it matters to the session, a knob: OK follows acceptance and precedes execution, so it uses the previous settings and is absent when the checksum fails (ERROR 81 alone); confirmations carry the sampler's own DeviceID unless told otherwise; the DONE of the checksum command follows the previous mode unless told otherwise; a silent sampler still executes; bytes after an item's data are ignored while checksums are off; notification defaults to on and auto screen update to off. Only §00 is modelled, any other section answers ERROR 0 until FTR-AKM-002 to 004. The manual driver steps time by 1 ms and does not wait for other threads, so a simulation delivering from its own thread needs the real driver. `OnOtherThreadBeforeSendReturns` forbids sending from an input callback. A `§` in a Catch2 test name broke `ctest` on Windows; test names are ASCII.

---

### TASK-AKM-008: Item catalogue and §00 primitives
- **Tier**: M
- **Status**: Not Started
- **Description**: Create the human-reviewed data file for the items and the script that generates the checked-in descriptor table from it, checks that the table is up to date, and compares the data file with `documents/_index/sysex_spec.items.tsv` (DEC-AKM-003), with the 7 records of §00; implement the generic encoder and range validator and the §00 helpers on top of the session: discovery, checksum-mode command, the four other toggles, and Echo.
- **Requirement refs**: RQ-AKM-001, RQ-AKM-012, RQ-AKM-013, RQ-AKM-014, RQ-AKM-015, RQ-AKM-044
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
- **Status**: In Progress
- **Description**: Add the first-contact probe: its logic (`runFirstContactProbe`, in the test-support library, so that CI runs it on the simulated sampler) and a small console program `xs56k_akm_probe` (test tree, built with `BUILD_TESTS`, not shipped) that opens the S5000 through `JuceMidiBackend` and, one frame at a time through the codec and without the session's flow control, sends the checksums-off frame of the spec, a broadcast Query and a Query to another DeviceID, Get OS version and Get sub-version (§02/&00 and &01, RQ-AKM-044), an Echo with checksums off and with an unexpected checksum, then with checksums on a Query, an Echo and a Query without checksum, switches checksums off again and toggles Still Alive on and off. It logs every frame in both directions with times, a decoded reading in each checksum mode, the latency of each step and an observations block; the owner runs it and the log is kept. It fixes early the facts the session design rests on: which DeviceID confirmations carry, the shape of DONE and ERROR, whether confirmations carry a checksum while it is on, whether `F0 F7` is delivered when one occurs, the response latency and the sampler's OS version.
- **Requirement refs**: RQ-AKM-017, RQ-AKM-004, RQ-AKM-007, RQ-AKM-011, RQ-AKM-018, RQ-AKM-044
- **ADR refs**: ADR-AKM-001 (DEC-AKM-007, DEC-AKM-009)
- **Acceptance Criteria** (Gherkin): *Given* the S5000 connected and its ports visible, *When* the probe runs, *Then* it writes a log with one line per frame (direction, time, bytes). *Given* the log, *When* read, *Then* it shows at least one Echo REPLY, one Query DONE, and the answers with checksum on and off. *Given* the observations, *When* they contradict a decision or an assumption of ADR-AKM-001 or of a requirement, *Then* those are amended before TASK-AKM-006 starts. *Given* the probe run on the simulated sampler in CI, *When* it ends, *Then* it has sent the expected fifteen frames, each with its own user-ref, and the sampler is left with checksums off and Still Alive off, whatever it started with.
- **Dependencies**: TASK-AKM-003, TASK-AKM-004
- **Assignee**: AI for the probe; the owner for the run against the hardware
- **Verification**: Windows/MSVC 19.51, re-run this session, Debug and Release: 0 warnings, `ctest` 168/168 (18 new: 15 for the probe, 3 for the §02 items of the simulated sampler); tests written first, red run not executed separately. On the simulated sampler the probe sends the 15 expected frames in order, each with its own user-ref, logs one line per frame (direction, time, bytes), and leaves checksums and Still Alive off even from the opposite start; it copes with a silent sampler, a delay, another DeviceID, echoed DeviceIDs, `F0 F7` and missing ports. Nine mutations were each caught by 1–4 tests; the 32 probe/simulation/scenario tests passed 20 repeats. `xs56k_akm_probe` builds and links against JUCE; `--help`, `--list` and an unknown port behave; over the virtual loopback port `LoopBe Internal MIDI` it sent the 15 frames and got the 15 back through the real Windows MIDI stack (a loopback returns the commands: it proves SysEx transport, not sampler behaviour). CI on `547eed0`: `linux-headless-canary` and the six generated canaries succeeded, 168/168 on GCC, MSVC and Apple Clang, with the probe executable built on each. Not verified: the run against the S5000 (the owner's; the log is to be kept and read).
- **Assumptions**: The logic is `runFirstContactProbe` in the test-support library (so CI runs it on the simulated sampler) and the program `juce/tests/probe/main.cpp` only wires JuceMidiBackend, the command line and the log file. Two additions were decided with the owner: the OS version (§02/&00 and &01, RQ-AKM-044) and the closing reset of checksums and Still Alive (RQ-AKM-018). The probe begins with the spec's checksums-off frame to DeviceID 0 (every sampler), which works whether checksums are on or off, so that the state is known. Still Alive is toggled to see whether the OS supports it (DONE or ERROR 0), but an `F0 F7` shows in the log only if the sampler sends one during the run: provoking one needs a slow command, left to TASK-AKM-010. A step that gets no answer does not stop the probe, and the closing frames are always sent. §00 persistence across a power cycle and the initial state of Notification, Auto screen update and Still Alive are not probed: the first shows in the log as the presence of OK, the rest belongs to TASK-AKM-010. The task stays In Progress until the owner has run the probe on the S5000 and its log has been read against ADR-AKM-001.
