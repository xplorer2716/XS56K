# PLAN-APP-002: Editor Application — Main Window and Menu, File Browser, Memory View

## Overview

Session APP (2026-10-14). The owner decided on 2026-10-14 that TASK-APP-002 (the tabs of the program editor) can wait: nothing in the main window,
the menu, the file browser or the memory view depends on it (the requirements that do, RQ-APP-049, RQ-APP-056 and RQ-APP-057, are in FTR-APP-007
and FTR-APP-008). This plan is the first implementation plan of the editor application (`juce/app`): the foundation that ADR-APP-001 decided
(`xs56k_sampler` split from `xs56k_mcp`, then `xs56k_app_core`), the connection that the menu and the two lists need, then the three features
the owner chose: FTR-APP-002 (main window, menu), FTR-APP-004 (file browser) and FTR-APP-005 (memory view). The condition set by PLAN-APP-001 (the
owner's code-quality branch merged first) is met: `feature/QLT` is merged (PR #16, PR #17).

Each feature is built core first, with its tests against the simulated sampler (RQ-APP-014, DEC-APP-010), then its views (the views are thin and
are checked by building and running the application, and against the mockups of `documents/mockups/`). 10 tasks, the session's limit.

**Not in this plan**: the edit panel and the program editor (FTR-APP-007, FTR-APP-008, behind TASK-APP-002), the save (FTR-APP-006), priority 2
(FTR-APP-009), and the requirements marked `Could` of the three features and of the connection (RQ-APP-008, RQ-APP-009, RQ-APP-023).

| Step | Task | What | Why here |
|---|---|---|---|
| 1 | TASK-APP-010 | split `xs56k_sampler` from `xs56k_mcp` | DEC-APP-002: the application links the gateway, not the MCP server |
| 2 | TASK-APP-011 | `xs56k_app_core`: worker, dispatcher, prompts, settings, model | DEC-APP-001, DEC-APP-003, DEC-APP-004, DEC-APP-007 |
| 3 | TASK-APP-012 | the connection and the MIDI setup, in the core | nothing can be browsed before the session is open |
| 4 | TASK-APP-013 | the file browser, read side: disks, folders, files, commands offered | first thing the person sees once connected |
| 5 | TASK-APP-014 | the file browser, commands: load, create, rename, delete, audition, refresh | the first step of the owner's working principle |
| 6 | TASK-APP-015 | the memory view, read side: six lists, follow the changes, select, free memory | where a load shows up |
| 7 | TASK-APP-016 | the memory view, commands: create a program, rename, delete, audition | completes FTR-APP-005 of priority 1 |
| 8 | TASK-APP-017 | main window, menu bar, status, MIDI setup dialog, JUCE adapters | the shell that the two views sit in |
| 9 | TASK-APP-018 | the file browser view | draws the browser's model |
| 10 | TASK-APP-019 | the memory view | draws the memory view's model |

## References
- **Requirements**: RQ-APP-006, RQ-APP-007, RQ-APP-010 to RQ-APP-014 (FTR-APP-002); RQ-APP-015 to RQ-APP-022 (FTR-APP-003, the connection and the MIDI
  setup); RQ-APP-024 to RQ-APP-034 (FTR-APP-004); RQ-APP-035 to RQ-APP-042 (FTR-APP-005); RQ-APP-001 to RQ-APP-005 (FTR-APP-001, design system)
- **ADRs**: ADR-APP-001 (Accepted, DEC-APP-001 to DEC-APP-010); ADR-APP-002 (Accepted, design system, DEC-APP-011 to DEC-APP-016); ADR-APP-003
  (Accepted, mockups, DEC-APP-017 to DEC-APP-021); ADR-MCP-001 and ADR-MCP-002 (the gateway the application reuses)

This plan implements the tasks in the format specified below.
---

## Tasks

### TASK-APP-010: Split `xs56k_sampler` from `xs56k_mcp`
- **Tier**: M
- **Status**: Done
- **Description**: Move the gateway (`SamplerGateway*.cpp`, `GatewayDetail.hpp`) and the catalogues (`ParameterCatalogue.cpp`, `StandardParameters.cpp`,
  `FxCatalogue.cpp`) into a static library `xs56k_sampler` that depends on `xs56k_akm` only; `xs56k_mcp` keeps the protocol, the tools and the options and links it.
  The files do not move and the namespace stays `mcp` (DEC-APP-002): the change is in `juce/mcp/CMakeLists.txt` only.
- **Requirement refs**: RQ-APP-014, RQ-MCP-057, RQ-BLD-014
- **ADR refs**: ADR-APP-001 (DEC-APP-002)
- **Acceptance Criteria** (Gherkin): *Given* the change, *When* the project is built and `ctest` runs, *Then* every MCP test passes without any modification,
  `xs56k_mcp` links `xs56k_sampler`, `xs56k_sampler` links neither nlohmann/json nor `xs56k_mcp`, and the source check of destructive calls
  (`CheckNoDestructiveCalls.cmake`) and `generate_sysex_coverage.py --check` still pass.
- **Dependencies**: None
- **Assignee**: AI
- **Verification**: Windows/MSVC (Visual Studio 18 2026), Debug, `BUILD_TESTS=ON`, 2026-10-14: configure passed with the new configure-time checks
  (`xs56k_sampler` links neither `nlohmann*` nor `xs56k_mcp`, nor the test support); build warning-clean, `xs56k_sampler.lib` then `xs56k_mcp.lib`,
  `xs56k_mcp_server`, `xs56k_mcp_tests` linked; `ctest -C Debug`: 100% passed, 0 failed out of 1139, among them
  `mcp_sources_call_no_destructive_primitive`, `sysex_coverage_table_is_up_to_date`, `mcp_public_headers_have_no_juce_include`; no test source changed.
- **Assumptions**: (1) Tier M, not L: a new CMake target but no new public API and no new dependency, the files being the same. (2) `README.md`'s
  architecture text names `xs56k_sampler`; the diagram image (`xs56k-global.jpeg`) is not redrawn.

### TASK-APP-011: `xs56k_app_core`: sampler worker, UI dispatcher, prompts, settings and the model's skeleton
- **Tier**: L
- **Status**: Done
- **Description**: Create `juce/app/core/` as the static library `xs56k_app_core` (no JUCE, added with the libraries whatever `BUILD_APP` says): the
  sampler worker (one thread, one job at a time, each job labelled in words), the interfaces `UiDispatcher`, `UserPrompts` (asynchronous answers) and
  `AppSettings`, the model with its listener interface and the job in progress, the reporting of a failed job (what failed and why, last values kept),
  a test helper (wait for the worker to be idle, drain the continuations, scripted prompts, in-memory settings) and the source check that keeps JUCE out
  of the core.
- **Requirement refs**: RQ-APP-010, RQ-APP-011, RQ-APP-012, RQ-APP-013, RQ-APP-014
- **ADR refs**: ADR-APP-001 (DEC-APP-001, DEC-APP-003, DEC-APP-004, DEC-APP-007, DEC-APP-009, DEC-APP-010)
- **Acceptance Criteria** (Gherkin): *Given* a job that the simulated sampler holds, *When* the controller queues it, *Then* the call returns at once, the model
  names the job ("Loading <file>") and the UI thread's work completes before the answer is released (RQ-APP-012); *Given* a job whose command is answered
  ERROR, *Then* the model carries what failed and why and keeps the last value read (RQ-APP-010); *Given* a destructive action and a prompt answered "cancel",
  *Then* no job is queued (RQ-APP-011); *Given* a load answered after the ordinary timeout and before the slow one, *Then* it succeeds, and *given* a sampler
  that never answers, *Then* the message says it may need to be switched off and on (RQ-APP-013); *Given* a JUCE include in `juce/app/core/`, *Then* the check fails.
- **Dependencies**: TASK-APP-010
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, `BUILD_TESTS=ON`, 2026-10-14, build warning-clean; `ctest -C Debug`: 100% passed, 0 failed out of 1147.
  RQ-APP-012: "Given the simulated sampler holding its answer..." passed (the call returned under 100 ms, the model named the job, the drain ran
  nothing before the answer) and "Given two jobs queued..." passed (the second job named until it ended). RQ-APP-010: "...ERROR \"parameter out of
  range\"..." passed (problem names "Creating the program LEAD" and the sampler's words; the success continuation not run). RQ-APP-011: "Given a
  destructive action..." passed (cancel: no command accepted by the sampler; confirm: deleted). RQ-APP-013: "...a load after the ordinary timeout and
  before the disk timeout..." passed (load answered after 700 ms, command timeout 300 ms, disk timeout 1500 ms) and "...never answers a load..." passed
  ("switched off and on"). `app_core_has_no_juce_include` passed; `app_core_check_fails_on_a_juce_include` passed (the check fails on the fixture,
  naming `juce_include_fixture/Fixture.cpp`). Error Recovery Protocol once: that test's first pass regex did not match because CMake wraps the
  message; fixed by matching the path alone, the check itself unchanged.
- **Assumptions**: (1) The core is added to `juce/CMakeLists.txt` with the libraries, as DEC-APP-010 says, so the headless workflows build and test it.
  (2) The slow-operation timeout is the gateway's (`waitForDisk()`), the core only chooses it for the load and refresh jobs. (3) To give one item a
  delay of its own, the simulated sampler gets a `SlowItem` knob (`SamplerBehaviour::slowItems`, test support only). (4) A problem stays in the model
  until the person dismisses it (`clearProblem`); a later success does not clear it. (5) `AppSettings` is a key-value interface; its keys are set by
  TASK-APP-012. (6) `CheckNoJuceIncludes.cmake` takes `WITH_SOURCES=ON` to scan the `.cpp` files too, and its message no longer names the AKM layer
  only. (7) Linux and macOS (GCC, Clang) are checked by the CI after the push, not locally.

### TASK-APP-012: The connection and the MIDI setup, in the core
- **Tier**: L
- **Status**: Done
- **Description**: In `xs56k_app_core`: the MIDI setup (the computer's ports through `MidiBackend`, one input, one output, DeviceID 0 to 31 and nothing else,
  stored through `AppSettings`), "Connect" (one gateway per connection, a job that opens the session and reads the sampler's model, name and OS version),
  the four connection states with the reason of a failure (missing port named, DeviceIDs that answered), "Disconnect", the change of ports while connected,
  the lost port, and quitting, which waits for the job in progress. While disconnected no job is queued.
- **Requirement refs**: RQ-APP-015, RQ-APP-016, RQ-APP-017, RQ-APP-018, RQ-APP-019, RQ-APP-020, RQ-APP-021, RQ-APP-022
- **ADR refs**: ADR-APP-001 (DEC-APP-008, DEC-APP-009)
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler at DeviceID 0 and the setup at 0, *When* "Connect" is chosen, *Then* the state is connected with
  the model, name and OS version; *Given* the setup at DeviceID 2 and the sampler at 0 only, *Then* it stays disconnected and says no sampler answered at 2 and
  one answered at 0; *Given* a stored port absent, *Then* it names it; *Given* the DeviceID 40, *Then* the setup refuses it and shows 0 to 31; *Given* "IN B",
  "OUT B", 3 validated, *Then* a new run's setup shows them; *Given* a session open and new ports validated, *Then* the session closes and the old ports are
  released before the new ones open; *Given* a session open with checksums on and a quit, *Then* the session closing is sent before the ports are released;
  *Given* a port that reports an error, *Then* the state is failed with the port's name and "Connect" is offered again.
- **Dependencies**: TASK-APP-011
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, `BUILD_TESTS=ON`, 2026-10-14, build warning-clean; `ctest -C Debug`: 100% passed, 0 failed out of 1159, the 18
  `[RQ-APP` cases among them, all 12 of `ConnectionTests.cpp` passing at their first run. RQ-APP-015: ports "IN A", "IN B" / "OUT A", "OUT B" listed with no
  port opened; DeviceID 40 and -1 refused with "0 to 31". RQ-APP-016: "IN B", "OUT B", 3 read back from the settings. RQ-APP-017/019: connecting then connected,
  "AKAI S5000", OS "2.10", DeviceID 0. RQ-APP-018: "No sampler answered at DeviceID 2" naming DeviceID 1; "IN GONE" named with "IN A" available; DeviceID 40:
  failed, no port opened. RQ-APP-019: an action refused and nothing queued while disconnected, queued once connected. RQ-APP-020: "release IN A"/"release
  OUT A" before "open IN B"/"open OUT B", connected again. RQ-APP-021: disconnect and quit: closing frames sent on OUT A, none after its release; quit told only
  after both ports were released; disconnected quit: told, no port touched. RQ-APP-022: failed, naming "IN A" and "device removed", IN A released, actions
  refused, connected again. The MCP suite unchanged and passing with the gateway's new wording.
- **Assumptions**: (1) FTR-APP-003 was not among the features the owner named, but the menu (RQ-APP-007) offers "MIDI setup", "Connect" and "Disconnect", and the
  browser and the memory view are empty without a session: its priority-1 requirements are in this plan. "Connect at start" (RQ-APP-023, `Could`) is not.
  (2) The model and the OS version come from the gateway's `readSystemInfo()`, the name from `readSamplerSettings()`: no gateway method was added for them.
  (3) A lost port (RQ-APP-022): no layer passed the input port's `onError` on (the AKM session sets its own callbacks); the gateway now wraps the input port it
  gives the session and calls a new `GatewayConfig::portError`, so the AKM layer is unchanged. (4) The gateway's opening messages said "set the DeviceID of
  the server"; they now say "set the DeviceID", shown by the editor as well as by the MCP server; no test pinned the old words. (5) On "Disconnect", a lost port
  or a change of setup the state changes at once and the gateway is destroyed on the worker after the job in progress, the busy indicator saying
  "Disconnecting from the sampler". (6) The tests' MIDI backend (`RecordingBackend`, test support) gives two pairs of named ports over the one simulated bus
  and records the opening, sending and release of each port.

### TASK-APP-013: The file browser, read side
- **Tier**: M
- **Status**: Done
- **Description**: In `xs56k_app_core`: the browser's model and controller for what is read, not changed: the disks as roots with name, type, format and free space
  ("not reported" for 0 bytes), selecting a disk, opening a folder (which makes it the sampler's current folder), the current folder and the path always known,
  the kind of a file from its extension with its size, and the commands that apply to the selected disk, folder or file. Once the session opens the browser lists
  the disks (the second half of RQ-APP-017).
- **Requirement refs**: RQ-APP-024, RQ-APP-025, RQ-APP-026, RQ-APP-027, RQ-APP-017
- **ADR refs**: ADR-APP-001 (DEC-APP-004)
- **Acceptance Criteria** (Gherkin): *Given* the simulated sampler with two disks, *When* the session opens, *Then* the model has two roots with names and types,
  and *when* the second is selected it is current with its format and free space, and a free space of 0 gives "not reported"; *Given* DRUMS holding KICK.WAV and
  LOOPS, *When* DRUMS is opened, *Then* both are under it, DRUMS is current and the path says so; *Given* PAD.AKP of 4 096 bytes, *Then* "PAD.AKP", 4 096, "program",
  and README.TXT is "unknown"; *Given* PAD.AKP selected, *Then* "audition" is not offered, and *given* KICK.WAV, it is.
- **Dependencies**: TASK-APP-012
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, `BUILD_TESTS=ON`, 2026-10-14, build warning-clean; `ctest -C Debug`: 100% passed, 0 failed out of 1167, the 8 cases of
  `BrowserTests.cpp` among them, all passing at their first run. RQ-APP-024/017: two roots HD1 "hard disk" and CD1 "CD-ROM" once connected; HD1 selected: current,
  "MSDOS", "1 000 000 bytes", root folders DRUMS and SYNTH; CD1 selected: "ISO9660", "not reported". RQ-APP-025: DRUMS opened: LOOPS and KICK.WAV under it, current
  folder DRUMS, path "DRUMS", the root kept in the tree; from DRUMS\LOOPS to SYNTH: current SYNTH; a folder that is not there: problem reported, current folder
  unchanged. RQ-APP-026: "PAD.AKP", "4 096 bytes", "program"; README.TXT and a name without extension "unknown"; the six kinds by extension, case ignored.
  RQ-APP-027: a program file offers load, load with dependents, rename, delete and no audition; a sample file offers audition; a folder and a disk their lists in
  the menu's order. Disconnecting empties the browser.
- **Assumptions**: (1) The gateway needed nothing new: the kind of a file comes from its extension in the core (`fileKindOf`), the disk's format and type from
  `DiskEntry`, the free space from `readDiskSpace`. (2) The browser's tree is the folders listed so far on the current disk, keyed by their path; selecting
  another disk starts it again. (3) Opening a folder reads the sampler's own current path first and goes up then down from it, so a folder changed on the front
  panel does not mislead it. If a step fails half way, the problem is reported and the model keeps the folder it showed, though the sampler may have moved; the next
  opening starts again from the sampler's real path. (4) The sampler's list of disks is read without its refresh (`listDisks(false)`); the refresh is TASK-APP-014's,
  behind its confirmation. (5) The browser is a `ModelListener` of the connection: it fills itself when the state becomes connected and empties itself when it
  stops being connected.

### TASK-APP-014: The file browser, commands
- **Tier**: M
- **Status**: Done
- **Description**: In `xs56k_app_core`: load a file (normal, as RAM, as virtual, or with its dependents), load a folder after a confirmation, create, rename (without
  the extension, which the sampler keeps) and delete a file or folder after a confirmation, audition a sample file without loading it and stop it on another selection,
  and refresh the list of disks after the warning that the sampler may stop answering. A refusal sends nothing; a load asks the memory view to read the lists again.
- **Requirement refs**: RQ-APP-028, RQ-APP-029, RQ-APP-030, RQ-APP-031, RQ-APP-032, RQ-APP-033, RQ-APP-034, RQ-APP-011, RQ-APP-013
- **ADR refs**: ADR-APP-001 (DEC-APP-005, DEC-APP-007)
- **Acceptance Criteria** (Gherkin): *Given* PAD.AKP using KICK.WAV, *When* it is loaded with its dependents, *Then* the program PAD and the sample KICK are in
  memory and PAD is selected, and *when* it is loaded alone, only PAD is added; *Given* the folder KIT with a sub-folder EXTRA, *When* KIT is loaded and confirmed,
  *Then* both folders' items load, and *when* cancelled, nothing is sent; *Given* LOOPS existing, *When* a folder LOOPS is created, *Then* nothing is sent and the
  reason is shown; *Given* KICK.WAV, *When* it is renamed "KICK 2", *Then* the tree shows KICK 2.WAV; *Given* OLD, *When* it is deleted and confirmed, *Then* it is gone,
  and cancelled, nothing is sent; *Given* KICK.WAV auditioned, *When* another file is selected, *Then* the stop is sent; *Given* the refresh asked and cancelled,
  *Then* nothing is sent, and confirmed, the refresh is sent with the slow timeout and the disks are listed again.
- **Dependencies**: TASK-APP-013
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, `BUILD_TESTS=ON` and `BUILD_APP=ON`, 2026-10-15, build warning-clean (the app included); `ctest -C Debug`: 100% passed, 0
  failed out of 1175, the 8 cases of `BrowserCommandTests.cpp` among them, all passing at their first run. RQ-APP-028: with dependents `&2B` sent once and no `&2A`,
  the memory change selecting the program PAD; alone `&2A` once; a file of the root loaded from DRUMS\LOOPS: the sampler back at the root, the sample KICK selected.
  RQ-APP-029: cancel: asked about KIT with "sub-folders", no command accepted, no memory change; confirm: `&15` once, one memory change. RQ-APP-030: NEW KIT under
  DRUMS, `&16` once; LOOPS refused, `&16` still once, the reason names LOOPS. RQ-APP-031: KICK.WAV renamed "KICK 2" shows KICK 2.WAV; renamed SNARE: refused, no
  command accepted. RQ-APP-032: cancel: nothing accepted, OLD still there; confirm: `&17` once, OLD gone. RQ-APP-033: `&30` once, no load; another file selected:
  `&31` once; a third selection sends no other stop. RQ-APP-034: cancel: asked with "switched off and on", no `&01`; confirm: `&01` once, the disk listed again.
- **Assumptions**: (1) The memory view's re-read after a load (RQ-APP-036) is done in TASK-APP-015; this task raises the "memory changed" event (`Model::memoryChanged`)
  with the item to select, and the acceptance criterion about the memory's items is checked there. The types the two tasks share (`app/Memory.hpp`) are added
  here. (2) One task for seven requirements: it stayed one. (3) Every command first takes the sampler to the folder of the item (`moveTo`), since the sampler acts
  on its current folder only. (4) A name already in the folder (case ignored) is refused by the core from the listing it shows, before any job: the gateway's own
  check sends listing commands first. (5) The item a load selects is the first one of the file's kind that the memory did not hold before; a load that replaced an
  item of the same name selects nothing. (6) A folder is deleted with what it holds once the person confirmed, the confirmation saying so (`deleteFolder(...,
  true)`). (7) `juce/build`'s cache held `JUCE_MODULES_ONLY=ON` from a configuration without the app, which breaks `BUILD_APP=ON`; it was reconfigured with
  `-DJUCE_MODULES_ONLY=OFF`. The root `CMakeLists.txt` forces it ON only without the app and never puts it back: not changed by this task.

### TASK-APP-015: The memory view, read side
- **Tier**: M
- **Status**: Done
- **Description**: In `xs56k_app_core`: the memory view's model and controller: the six lists (multis, programs, samples, song files, set lists, scenelists) with
  their counts, names in the sampler's order and the current item of each kind, read again after any action that changes the memory and on request, selecting an
  item (select by name, back to the current item and the error reported if it fails), and the free wave memory in bytes with the free percentages. Showing the selected
  item in the edit panel (RQ-APP-048) is FTR-APP-007's: the model records the selection and the edit panel will observe it.
- **Requirement refs**: RQ-APP-035, RQ-APP-036, RQ-APP-037, RQ-APP-042, RQ-APP-010
- **ADR refs**: ADR-APP-001 (DEC-APP-004)
- **Acceptance Criteria** (Gherkin): *Given* the programs PAD and BASS with BASS current and the sample KICK, *When* the session opens, *Then* "Programs (2)" lists
  PAD and BASS with BASS current, "Samples (1)" lists KICK and the four other headings show 0; *Given* a program loaded with its samples, *When* the load is done,
  *Then* the programs and the samples are read again; *Given* a sample deleted on the sampler's front panel, *When* the person refreshes, *Then* it is no longer listed;
  *Given* PAD and BASS with BASS current, *When* PAD is selected, *Then* PAD is selected by name on the sampler and current in the model, and *when* the select is answered
  ERROR, *Then* the current item is BASS again and the error is reported; *Given* 8 MB of wave memory with 2 MB free, *Then* "2 MB free of 8 MB" and the free percentages, read
  again after a load.
- **Dependencies**: TASK-APP-012
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, `BUILD_TESTS=ON` and `BUILD_APP=ON`, 2026-10-15, build warning-clean; `ctest -C Debug`: 100% passed, 0 failed out of 1182,
  the 7 cases of `MemoryTests.cpp` among them, all passing at their first run. RQ-APP-035: 2 programs PAD and BASS, BASS current, the samples KICK, the four other
  kinds empty; headings "Programs", "Scenelists". RQ-APP-036/028: LEAD.AKP loaded with its dependents from the browser: LEAD and SAW listed, LEAD selected and current;
  a sample removed behind the view's back gone after a refresh. RQ-APP-037/010: PAD selected: one more select by name (`0A &05`) accepted, PAD selected and current;
  a refused select of BASS: the selection back on PAD, the problem naming BASS; a set list selected in the view with no command sent. RQ-APP-042: 2 MB free of
  8 MB, 25 %, MPKS 40 %; after a load the free bytes read again (1 MB). Disconnecting empties the view. CI of the commits of TASK-APP-011, 012 and 013: the 7
  canary workflows (Linux GCC, macOS Clang, Windows MSVC, Debug and Release) green.
- **Assumptions**: (1) Set lists have no current item (FTR-APP-005), so none is marked. (2) Independent of TASK-APP-013 and TASK-APP-014 apart from the "memory changed"
  event, which the browser raises: the two tasks can be done in either order. (3) The program list comes from `listPrograms()` and its current item from `status()`,
  the gateway's program listing not marking it; the other five kinds from `listSamples()`, `listMultis()` and `listNamedItems()`, the free memory from
  `readSystemInfo()`: no gateway method added. (4) After a re-read the selection stays on its item when it is still listed; a load's item is then selected on the
  sampler (RQ-APP-028). (5) The memory tests seed programs with the raw-frame helpers of the AKM and MCP tests (`SimulatedPrograms.hpp`), which bring no MCP server.

### TASK-APP-016: The memory view, commands
- **Tier**: M
- **Status**: Done
- **Description**: In `xs56k_app_core`: create a program (name and number of keygroups; refused with nothing sent if the name exists), rename a program, a sample or a
  multi (refused if the name exists in the kind), delete one after a confirmation naming it, and audition a sample of the memory (stopped on another selection).
- **Requirement refs**: RQ-APP-038, RQ-APP-039, RQ-APP-040, RQ-APP-041, RQ-APP-011
- **ADR refs**: ADR-APP-001 (DEC-APP-005, DEC-APP-007)
- **Acceptance Criteria** (Gherkin): *Given* no program LEAD, *When* LEAD is created with 2 keygroups, *Then* it is listed, current and has 2 keygroups, and *when* PAD (which
  exists) is created, *Then* nothing is sent; *Given* the samples KICK and SNARE, *When* KICK is renamed "KICK 2", *Then* the list shows KICK 2 and SNARE, and *when* KICK is renamed
  SNARE, *Then* nothing is sent; *Given* the program PAD, *When* it is deleted and confirmed, *Then* it is no longer listed, and cancelled, nothing is sent; *Given* KICK selected,
  *When* it is auditioned, *Then* the audition starts, and *when* SNARE is selected, *Then* the audition of KICK stops.
- **Dependencies**: TASK-APP-015
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, `BUILD_TESTS=ON`, `BUILD_APP=ON`, 2026-10-15, build warning-clean; `ctest -C Debug`: 100% passed, 0 failed out of 1188,
  the 4 cases of `MemoryCommandTests.cpp` among them, all passing at their first run. RQ-APP-038: LEAD created with 2 keygroups (`0A &03` sent once), listed,
  current and selected; "pad" refused with no command sent, the reason naming PAD. RQ-APP-039: KICK renamed KICK 2 (`0E &09` once), the list KICK 2, SNARE;
  KICK 2 to SNARE refused, nothing sent. RQ-APP-040/011: PAD deleted and cancelled: one question naming PAD, nothing sent; confirmed: `0A &08` once, PAD gone,
  BASS kept. RQ-APP-041: KICK auditioned (`0E &0A`), SNARE selected: one stop (`0E &0B`), then PAD selected: no second stop.
- **Assumptions**: (1) The gateway's `createProgram`, `renameCurrent*`, `deleteCurrent*` and the audition calls are used as they are; the delete passes the name the
  person confirmed as the gateway's typed confirmation (DEC-APP-007). (2) A taken name is compared as the gateway compares it (without regard to case, spaces or
  hyphens), in the model's list, before anything is sent. (3) Song files, scenelists and set lists are renamed and deleted the same way (the gateway's named-list
  calls, a set list by its name), beyond the three kinds the requirements name, so the view needs no special case. (4) After a create or a rename the lists are read
  again and the item selected (the model's "memory changed"); after a delete they are read again. (5) The edit panel that a created program opens in (RQ-APP-038) comes
  with FTR-APP-007: here it is selected. (6) Deleting the sample being auditioned stops its audition first.

### TASK-APP-017: Main window, menu bar, status, MIDI setup dialog and the JUCE adapters
- **Tier**: L
- **Status**: Done
- **Description**: In `XS56KApp`, replacing the placeholder window: the main window with its three areas (browser at the top left, memory view under it, the edit panel's area on
  the right with its row of tabs along its bottom edge, empty until FTR-APP-007), the menu bar ("MIDI setup", "Connect" only when disconnected, "Disconnect" only when
  connected, "Quit"), the connection state and the busy indicator (the job's label, the message after a slow-operation timeout), the MIDI setup dialog, and the JUCE adapters of the
  core's interfaces (`UiDispatcher` with `MessageManager::callAsync`, `UserPrompts` with dialogs that do not block the message thread, `AppSettings` with `PropertiesFile`,
  the JUCE MIDI backend). Every visual value comes from the design tokens; the application links `xs56k_app_core` and `xs56k_sampler`, not `xs56k_mcp`.
- **Requirement refs**: RQ-APP-006, RQ-APP-007, RQ-APP-012, RQ-APP-013, RQ-APP-015, RQ-APP-016, RQ-APP-019, RQ-APP-001, RQ-APP-004, RQ-APP-005
- **ADR refs**: ADR-APP-001 (DEC-APP-001, DEC-APP-003, DEC-APP-007, DEC-APP-009, DEC-APP-010); ADR-APP-002 (DEC-APP-011 to DEC-APP-016)
- **Acceptance Criteria** (Gherkin): *Given* the application started, *When* the main window is shown, *Then* the browser is at the top left, the memory view under it and the edit
  panel's area on the right with its tab row along its bottom edge, laid out as `documents/mockups/main-window-disconnected.svg`; *When* the menu bar is opened, *Then* "MIDI setup",
  "Connect", "Disconnect" and "Quit" are offered, "Connect" only when disconnected; *Given* the application disconnected, *Then* the window says disconnected and the load and edit actions
  are disabled, and connected, it says the model and DeviceID; *Given* a job in progress, *Then* the busy indicator names it and the window keeps answering; *Given* the source check of
  raw visual literals, *Then* it passes; `BUILD_APP=ON` builds warning-clean and the application starts and quits.
- **Dependencies**: TASK-APP-012
- **Assignee**: AI
- **Verification**: (partial, 2026-10-15) Windows/MSVC, Debug, `BUILD_TESTS=ON`, `BUILD_APP=ON`: build warning-clean, `XS56K.exe` and `Inter-LICENSE.txt` next
  to it; `ctest -C Debug`: 100% passed, 0 failed out of 1184, among them `app_sources_have_no_visual_literal` (the application's sources and the core) and
  `app_visual_literal_check_fails_on_a_fixture` (the check names lines 6, 8 and 9 of the fixture). The application started and a capture of its window showed
  the three areas (Disks at the top left, Memory under it, Editor on the right with its tab row's line at the bottom), the menu bar "XS56K", "Sampler", "MIDI",
  and the status bar "Disconnected" with an empty dot, as `documents/mockups/main-window-disconnected.svg` lays them out (left column 240 px, the token's
  minimum, where the mockup's example uses 300). By the owner on the running application, 2026-10-15, with the real S5000 (MIDIN2 / MIDIOUT15, ESI M8U eX):
  Connect and Disconnect work; the MIDI setup dialog marks the ports in use and refuses the DeviceID 40 ("The DeviceID 40 is not valid: a sampler's DeviceID
  is 0 to 31."); a second instance is refused the connection, the first one holding the ports. Quitting while connected was not reported on; the owner accepted
  the task and asked to go on.
- **Assumptions**: (1) The views are not unit-tested (DEC-APP-010): what they do is in the core. This task's verification is the build, a run of the application on Windows,
  and a screenshot compared with the mockups. (2) The resizing of the areas and the restoring of the window (RQ-APP-008, RQ-APP-009, `Could`) are not done: the layout is the design
  system's default. (3) The session's dialogs of the MIDI setup follow DEC-APP-009's order: ports listed on the UI thread, nothing sent to the sampler. (4) The source check of DEC-APP-014
  did not exist: it is added here (`CheckNoVisualLiterals.cmake`, with a fixture it must fail on), scanning `juce/app/src` and `juce/app/core`. (5) Component tokens
  added (aliases of existing global sizes, no new global): `status.height`, `status.dot.size`, `status.spinner.size`, `dialog.width` (→ `size.edit.minWidth`) and
  `button.height`; the document's §4 names them. (6) The menus are "XS56K" (Quit), "Sampler" (Connect or Disconnect, only the one that applies) and "MIDI" (MIDI
  setup…); "Connect" with no stored setup opens the MIDI setup first. (7) The left column has the token's minimum width (240 px), resizing being RQ-APP-008
  (`Could`). (8) Inter is embedded from the repository's `.woff2` files; if the operating system cannot load that format, the system's sans-serif is used and the
  failure is logged (DEC-APP-015). (9) The confirmation dialog's confirm button is "Confirm", in the destructive style, whatever the action.

### TASK-APP-018: The file browser view
- **Tier**: M
- **Status**: Done
- **Description**: In `XS56KApp`: the browser's component: the tree of disks, folders and files with each kind's icon, the current folder marked and the path always shown, the
  selected file's name, size and kind, the context menu offering the commands the core says apply, and the dialogs of the loads, the refresh and the deletions through `UserPrompts`.
  It draws the model, observes its changes and calls the controller; it holds no state of the sampler.
- **Requirement refs**: RQ-APP-024, RQ-APP-025, RQ-APP-026, RQ-APP-027, RQ-APP-028, RQ-APP-029, RQ-APP-030, RQ-APP-031, RQ-APP-032, RQ-APP-033, RQ-APP-034, RQ-APP-001, RQ-APP-004, RQ-APP-005
- **ADR refs**: ADR-APP-001 (DEC-APP-001); ADR-APP-002 (DEC-APP-013, DEC-APP-014, DEC-APP-016)
- **Acceptance Criteria** (Gherkin): *Given* the application connected to a sampler, *When* the person opens a folder, *Then* its contents are listed and the current folder is marked and in the
  path; *Given* a sample file selected, *When* the context menu opens, *Then* "Audition" is offered, and for a program file it is not; *Given* a file deleted from the menu, *When* the person
  cancels the confirmation, *Then* nothing changes; the component has no raw visual literal and shows no state by colour alone (RQ-APP-005).
- **Dependencies**: TASK-APP-014, TASK-APP-017
- **Assignee**: AI
- **Verification**: Windows/MSVC, Debug, `BUILD_TESTS=ON`, `BUILD_APP=ON`, 2026-10-15, build warning-clean; `ctest -C Debug`: 100% passed, 0 failed out of 1188, among them
  `app_sources_have_no_visual_literal` over the new sources. On the real S5000 (OS 2.14, MIDIN2 / MIDIOUT15), driven through the window's accessibility tree
  (UI Automation) and captured with `PrintWindow`, the owner away by his leave: connected, the tree listed "No disk - unknown" and "S5K - FAT32"; S5K selected:
  "S5K - FAT32 (current)" with the dot and the accent bar, its root's folders under it, the path "S5K:\" and "S5K - hard disk - FAT32 - free not reported";
  MoogBass opened: the path "S5K:\MoogBass", MoogBass "(current)" with its 7 `.wav` files, waveform icon; a session opened again in MoogBass showed
  S5K > MoogBass > its files (a bug found here and fixed: the tree was empty when the root had not been listed); a file selected: "Half-Cutoff-sqr-bass-C.wav
  - sample - 117 398 bytes"; its context menu (Shift+F10) offered Load, Load as RAM, Load as virtual, Load with dependents, Rename…, Delete…, Audition and Stop the
  audition (no Open nor folder command); Audition, Stop the audition and Load sent with no problem reported. The cancel of a deletion is the core's (TASK-APP-014,
  `BrowserCommandTests.cpp`, a cancel sends nothing); no deletion, rename or refresh of the disks was sent to the owner's sampler.
- **Assumptions**: (1) Verified by a run of the application against the simulated sampler where the build allows it, and by the owner on the real sampler; the real-sampler check is the owner's
  (the refresh of the disks is never sent to the owner's sampler without asking). Done instead on the real sampler with the owner's leave of 2026-10-15 (backup of the
  disk made), with no destructive command. (2) A folder opens on a double click, on Return or from the menu; a row is selected by the mouse, the keyboard or the
  accessibility tree, and Shift+F10 opens the context menu of the selected row. (3) Each file kind has its own icon shape (page with lines, waveform, two pages, note,
  list, frame with two dots, plain page for unknown); the sample's icon also takes the sample role colour. (4) The current disk and the current folder have the dot
  and "(current)" in their tooltip and accessible name (RQ-APP-005). (5) The names of new and renamed items are asked in a JUCE `AlertWindow` that does not block
  (`TextPrompt`); a sample's load mode is chosen by three menu items. (6) A list of rows shared with the memory view (`ItemList`).

### TASK-APP-019: The memory view
- **Tier**: M
- **Status**: Not Started
- **Description**: In `XS56KApp`: the memory view's component: six headings with their counts, the items in the sampler's order with the current item of each kind marked, the selection,
  the context commands (create a program, rename, delete, audition), the free memory line and a refresh button, drawn from the core's model.
- **Requirement refs**: RQ-APP-035, RQ-APP-036, RQ-APP-037, RQ-APP-038, RQ-APP-039, RQ-APP-040, RQ-APP-041, RQ-APP-042, RQ-APP-001, RQ-APP-004, RQ-APP-005
- **ADR refs**: ADR-APP-001 (DEC-APP-001); ADR-APP-002 (DEC-APP-013, DEC-APP-014, DEC-APP-016)
- **Acceptance Criteria** (Gherkin): *Given* the application connected, *When* the main window is shown, *Then* "Programs (n)", "Samples (n)" and the four other headings are listed with their
  items and the current item of each marked, and the free memory is shown; *Given* a program loaded from the browser, *Then* it appears in the list and is selected; *Given* an item
  deleted from the context menu, *When* the person cancels, *Then* nothing changes.
- **Dependencies**: TASK-APP-016, TASK-APP-017
- **Assignee**: AI
- **Verification**: (at closure)
- **Assumptions**: (1) Selecting an item records the selection in the model; the edit panel that shows it (RQ-APP-048) comes with FTR-APP-007, so the right-hand area stays empty. (2) The
  working principle of the owner (load a file, see it in the memory view) is checked end to end here, on the simulated sampler.
