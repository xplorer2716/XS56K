I'm working in the XS56K repo (c:\dev\repos\xplorer2716\public\XS56K), a JUCE/C++ project implementing an AKAI S5000/S6000 SysEx control layer (library `xs56k_akm`). I need to extend the existing "real-sampler suite" test harness to add a new opt-in check for Disk Tools (SysEx section 0x10), modeled closely on the existing `--system-setup` check (added by TASK-AKM-053) and the `--slow-operation` guard mechanism (added earlier, used for a disk command that's known to hang real hardware).

Please research and report back (in detail, this is for me to design new code, not just a summary):

1. **`juce/tests/support/include/akm/harness/RealSamplerSuite.hpp`**: the full `RealSuiteOptions` struct (every field, especially anything named like `systemSetup`, `slowOperation`, `noLcd`, `syncLcd`, etc.), the `RealSuiteResult`/`CheckReport` structs, and the signature of `runRealSamplerSuite`.

2. **`juce/tests/support/src/RealSamplerSuite.cpp`**: 
   - How `AUTOMATIC_CHECKS` is defined/counted, and how the 7 automatic checks vs the opt-in checks (`--power-cycle`, `--slow-operation`, `--system-setup`, `--program-lifecycle`, `--sample-lifecycle`) are gated and appended to the list of checks that run.
   - The FULL implementation of the `--system-setup` check (likely a function like `runSystemSetupCheck` or similar) — I need to see: how it opens its own session, how it reads/restores state (the "guard" pattern that restores values even if a check throws halfway), how it reports pass/fail, and roughly how long the function is.
   - The FULL implementation of how `--slow-operation` is gated/used (likely a separate, simpler check given how `OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md` described it: "sends one command outside sections 00 and 02... with Still Alive on, to see whether F0 F7 reaches the host"). I need the exact code of that check function.
   - Any generic guard/restore helper classes used across checks (e.g., something like `GuardedSession` mentioned in process docs).

3. **`juce/tests/probe/main.cpp`**: how CLI flags are parsed and mapped to `RealSuiteOptions` fields — specifically how `--system-setup`, `--slow-operation`, `--no-lcd`, `--power-cycle` etc. are parsed, and how `--help` text documents them. I need to see the exact flag-parsing code block and help text block so I can add a new flag the same way.

4. Quickly check `juce/akm/include/akm/DiskPrimitives.hpp` — just confirm it exists and note the namespace/style of the Disk primitive functions already implemented (selectDisk, getConnectedDisks, loadFolder, loadFile, loadFileWithDependents, saveMemoryItem, saveAllMemoryItems, ejectDisk, ejectDiskDiscardingVirtualSamples, deleteSubFolder, deleteFile, startFileAudition, stopFileAudition, etc.) — I already know these exist, just confirm their exact signatures (session, args, completion) since I'll be calling them from the new check.

Report back with actual code excerpts (not just descriptions) for the key functions (the --system-setup check function in full, the --slow-operation check function in full, the CLI flag parsing block, the RealSuiteOptions struct in full). This is going into my own design of a new `--disk-tools` check, so precision matters more than brevity — include file:line references.



{
  "message": "Full research dump for designing `--disk-tools`. File:line references throughout.\n\n## 1. `RealSuiteOptions`, `CheckReport`, `RealSuiteResult`, `runRealSamplerSuite` signature\n\nFile: `juce/tests/support/include/akm/harness/RealSamplerSuite.hpp:37-162`\n\n```cpp\nnamespace akm::harness\n{\n    struct RealSuiteOptions\n    {\n        ScenarioTarget target;\n        /// How long a session waits for the answer to each of its commands.\n        Scheduler::Clock::duration commandTimeout = std::chrono::seconds(3);\n        Scheduler::Clock::duration discoveryWindow = DEFAULT_DISCOVERY_WINDOW;\n        /// Echo round trips timed by the latency check; none skips the check.\n        int echoRepeats = ECHO_LATENCY_ROUND_TRIPS;\n        /// Whether the sessions of the run switch Sync LCD and Auto screen update. ...\n        bool touchLcdSettings = true;\n        /// The optional check that sends one slow, harmless command (§10/&01, update the list of disks) with Still\n        /// Alive on, to see whether `F0 F7` messages reach the host while the sampler works. ...\n        bool slowOperation = false;\n        /// The optional check that asks the owner to power-cycle the sampler while a session is open ...\n        bool powerCycle = false;\n        /// The optional checks that create, select, change and delete a program under a reserved test name\n        /// (RQ-AKM-027) ...\n        bool programLifecycle = false;\n        /// The optional check that selects a sample under its own name (RQ-AKM-051) ...\n        bool sampleLifecycle = false;\n        /// The optional checks that read the sampler's model and memory and round-trip its own name, Play Mode,\n        /// front-panel lock and clock (RQ-AKM-052 to RQ-AKM-055) ...\n        bool systemSetup = false;\n        /// A real sample the operator confirms is already in the sampler's memory, named by the caller. ...\n        std::optional<std::string> sampleName;\n        /// Tells the owner what to do and returns once they have done it, or false when they decline. ...\n        std::function<bool(const std::string& instruction)> askOwner;\n        /// Written in the log header when not empty.\n        std::string startedAt;\n    };\n\n    enum class CheckOutcome { Passed, Failed, Skipped };\n\n    struct CheckReport\n    {\n        std::string title;\n        CheckOutcome outcome = CheckOutcome::Passed;\n        std::string detail;\n    };\n\n    struct RealSuiteResult\n    {\n        bool portsOpened = false;\n        std::vector<CheckReport> checks;\n        std::vector<std::uint8_t> discoveredDeviceIds;\n        std::optional<OsVersionReport> osVersion;\n        std::size_t echoRoundTrips = 0;\n        std::vector<Scheduler::Clock::duration> echoLatencies;\n        std::size_t stillAliveMessagesSeen = 0;\n        std::size_t framesSent = 0;\n        std::size_t framesReceived = 0;\n        std::size_t rejectedMessages = 0;\n        std::size_t unsolicitedConfirmations = 0;\n        std::size_t lateErrors = 0;\n        bool knownStateRestored = true;\n\n        [[nodiscard]] std::size_t count(CheckOutcome outcome) const;\n        [[nodiscard]] bool passed() const { return count(CheckOutcome::Failed) == 0; }\n    };\n\n    RealSuiteResult runRealSamplerSuite(common::midi::MidiBackend& backend, ScenarioDriver& driver,\n                                        const RealSuiteOptions& options, std::ostream& log);\n}\n```\n\nNo field is yet named `diskTools`/`diskToolsSlow` etc. — you'll add one, mirroring `systemSetup`/`slowOperation`.\n\n## 2. `RealSamplerSuite.cpp`\n\n### \"AUTOMATIC_CHECKS\" — there is NO such constant in the production file\nIt's only a test-file constant (`juce/tests/akm/RealSamplerSuiteTests.cpp:64`: `constexpr std::size_t AUTOMATIC_CHECKS = 7;`). In production code the 7 unconditional checks vs. the opt-in ones are just sequential `check(...)` calls gated by `if`, in `Suite::run()` at `RealSamplerSuite.cpp:816-853`:\n\n```cpp\nvoid run()\n{\n    check(\"open a session and close it\", &Suite::openAndClose);\n    check(\"Echo returns the bytes sent\", &Suite::echoReturnsTheBytes);\n    check(std::to_string(_rig.options.echoRepeats) + \" Echo round trips, timed\", &Suite::echoLatencies);\n    check(\"the operating system version is read\", &Suite::osVersion);\n    check(\"checksums on and off through the session\", &Suite::checksumsOnAndOff);\n    check(\"closing puts back every setting the session changed\", &Suite::closePutsBack);\n    check(\"a check that fails half way leaves the sampler in the known state\",\n          &Suite::failedCheckLeavesTheKnownState);\n    if (_rig.options.slowOperation)\n        check(\"a slow operation with Still Alive on\", &Suite::slowOperation);\n    if (_rig.options.powerCycle)\n        check(\"a power cycle while a session is open\", &Suite::powerCycle);\n    if (_rig.options.programLifecycle)\n    {\n        check(\"create, change and select a program under a reserved test name, then delete it\",\n              &Suite::programLifecycleOnTestProgram);\n        check(\"a program check that fails half way still deletes the test program and restores the selection\",\n              &Suite::failedProgramCheckLeavesTheKnownState);\n        check(\"add keygroups to the test program and round-trip every §08 parameter item, including keygroup 0 (all)\",\n              &Suite::keygroupsOnTestProgram);\n        check(\"add a zone to a keygroup of the test program and round-trip every §06 parameter item, \"\n              \"including zone 0 (all four) and keygroup 0 + zone 0\",\n              &Suite::zonesOnTestProgram);\n    }\n    if (_rig.options.sampleLifecycle)\n        check(\"select the test sample, round-trip every §0E lifecycle and settable-parameter item on it, \"\n              \"and restore its name and parameters\",\n              &Suite::samplesOnTestSample);\n    if (_rig.options.systemSetup)\n    {\n        check(\"round-trip the sampler's name, Play Mode, front-panel lock and clock, and put them back\",\n              &Suite::systemSetupRoundTrips);\n        check(\"a system setup check that fails half way and still puts back what it changed\",\n              &Suite::failedSystemSetupCheckPutsBack);\n    }\n}\n```\n→ For `--disk-tools` you'd add `if (_rig.options.diskTools) { check(...); }` here, plus a header note in `writeHeader` (see below), plus wiring in `main.cpp`.\n\nThe `check(title, body)` wrapper (`RealSamplerSuite.cpp:935-980`) runs the method, catches `NoSamplerFailure` (ends the whole suite), `CheckFailure` (fails just this check), `CheckSkipped` (e.g. no `--sample-name`), and any other `std::exception` as \"unexpected exception\". Everything after a `NoSamplerFailure` is reported as Skipped via `_noSampler` flag (line 942-946).\n\n### `GuardedSession` — the generic session-lifetime guard (`RealSamplerSuite.cpp:179-283`)\nFull class, this is the base guard every check wraps its session in:\n```cpp\nclass GuardedSession\n{\npublic:\n    explicit GuardedSession(Rig& rig, std::optional<CloseResult>* closedInto = nullptr)\n        : _rig(rig), _closedInto(closedInto),\n          _session(rig.timing(), rig.driver.executor(), rig.driver.scheduler(), rig.input, rig.output, rig.diagnostics)\n    {\n    }\n\n    ~GuardedSession()\n    {\n        try\n        {\n            const SessionState state = _session.state();\n            if (state != SessionState::Closed && state != SessionState::Closing)\n            {\n                _rig.log.note(\"  the check ended with its session still open: the guard closes it\");\n                static_cast<void>(close());\n            }\n        }\n        catch (...)  // a destructor does not throw; a close that was lost has been noted by close()\n        {\n        }\n    }\n\n    GuardedSession(const GuardedSession&) = delete;\n    GuardedSession& operator=(const GuardedSession&) = delete;\n\n    [[nodiscard]] Session& session() { return _session; }\n\n    Timed<OpenResult> open(const SessionConfig& config)\n    {\n        _rig.log.flush();\n        const auto timed = awaitCompletion<OpenResult>(\n            _rig.driver, _rig.openPatience(), [this, &config](OpenCompletion done) { _session.open(config, std::move(done)); });\n        if (!timed)\n            throw CheckFailure(\"the open did not complete within \" + millisecondsText(_rig.openPatience())\n                               + \": the session lost it\");\n        const OpenResult& opened = timed->result;\n        if (_rig.result.discoveredDeviceIds.empty())\n            _rig.result.discoveredDeviceIds = opened.responders;\n        if (opened.ready())\n            return *timed;\n\n        std::string text = \"the open ended as \" + std::string(describe(opened.status))\n                           + \"; DeviceIDs that answered the discovery: \" + listOfIds(opened.responders);\n        if (opened.failedSetting)\n            text += \"; failed setting: \" + std::string(describe(*opened.failedSetting)) + \" (\"\n                    + outcomeText(opened.failedResult) + \")\";\n        switch (opened.status)\n        {\n            case OpenStatus::NoSamplerAtTarget:\n            case OpenStatus::AmbiguousSamplers:\n            case OpenStatus::InvalidDeviceId:\n            case OpenStatus::DiscoveryFailed:\n                throw NoSamplerFailure(text);\n            case OpenStatus::Ready:\n            case OpenStatus::ReadyDegraded:\n            case OpenStatus::SettingFailed:\n            case OpenStatus::AlreadyOpen:\n            case OpenStatus::Cancelled:\n                break;\n        }\n        throw CheckFailure(text);\n    }\n\n    Timed<CloseResult> close()\n    {\n        _rig.log.flush();\n        _rig.log.note(\"  closing: the session puts back the settings it changed\");\n        const auto timed = awaitCompletion<CloseResult>(_rig.driver, _rig.closePatience(), [this](CloseCompletion done) {\n            if (!_session.close(std::move(done)))\n                throw CheckFailure(\"the session refused to close\");\n        });\n        if (!timed)\n        {\n            _rig.result.knownStateRestored = false;\n            throw CheckFailure(\"the close did not complete within \" + millisecondsText(_rig.closePatience())\n                               + \": the sampler may not be in the known state\");\n        }\n        for (const SamplerSetting setting : timed->result.restored)\n            _rig.log.note(\"  put back: \" + std::string(describe(setting)));\n        for (const SamplerSetting setting : timed->result.notRestored)\n            _rig.log.note(\"  NOT put back: \" + std::string(describe(setting)));\n        if (!timed->result.restoredAll())\n            _rig.result.knownStateRestored = false;\n        if (_closedInto != nullptr)\n            *_closedInto = timed->result;\n        return *timed;\n    }\n\nprivate:\n    Rig& _rig;\n    std::optional<CloseResult>* _closedInto;\n    Session _session;\n};\n```\n\n### The resource-guard pattern you'll want to mirror for a disposable disk folder\nThere are two existing examples of \"create a disposable resource under a reserved name, use it, restore/delete it even on throw\" — both relevant templates for a `GuardedTestFolder`-style class per RQ-AKM-071:\n\n- **`GuardedTestProgram`** (`RealSamplerSuite.cpp:301-420`) — creates `TEST_PROGRAM_NAME = \"XS56K_SUITE_TEST\"` (a constant, line 288), deletes it on destruction, restores prior selection, has `expectOnTestProgram` that refuses to act unless the guarded resource is currently selected (defence against acting on the wrong resource).\n- **`GuardedTestSample`** (`RealSamplerSuite.cpp:439-618`) — mirrors it but *restores* rather than deletes (since a sample can't be recreated), snapshotting settable params via one `&4B` round trip and restoring via individual `Set`s (note the Loop-Start/Loop-End ordering workaround at lines 571-577, a real-hardware quirk).\n\nFor disk tools, the natural analogue is a `GuardedTestFolder` that: on construction calls `createFolder(session, \"XS56K_SUITE_TEST\", ...)` then `openFolder(...)` into it (RQ-AKM-071 says \"creates its own sub-folder... before changing anything else... performs every create/rename/load/save inside that sub-folder only\"); on destruction, navigates back up with `closeFolder`, then deletes the sub-folder via `deleteSubFolder(session, name, ConfirmDeleteSubFolder::IUnderstandThisDeletesTheFolderAndEverythingInIt, ...)` (the guard of RQ-AKM-069), all best-effort/logged/non-throwing, same discipline as `GuardedTestProgram`'s destructor (`RealSamplerSuite.cpp:310-337`).\n\n### `GuardedSystemSetup` — the `--system-setup` restore-on-destruction guard (full), `RealSamplerSuite.cpp:620-808`\n```cpp\n// The values the system setup check changes (RQ-AKM-058) and the instant the clock was read.\nstruct SystemSetupSnapshot\n{\n    std::string name;\n    PlayMode playMode{};\n    FrontPanelLock lock{};\n    std::optional<ClockDate> clock{};\n    std::string clockProblem;\n    Clock::time_point clockReadAt{};\n};\n\nconstexpr std::string_view TEST_SAMPLER_NAME = \"XS56K TEST\";\nconstexpr ClockDate TEST_CLOCK{2030, 6, 15, 7, 8, 5, 9};\nconstexpr std::int64_t CLOCK_RESTORE_TOLERANCE_SECONDS = 3;\nconstexpr std::int64_t MILLISECONDS_PER_SECOND = 1000;\n\n// ... twoDigits, clockText, playModeName, lockName, elapsedSeconds helpers ...\n\n// Reads the four values the system setup check changes; changes nothing. Clock allowed to fail.\nstd::optional<SystemSetupSnapshot> readSystemSetup(Rig& rig, Session& session, std::string& problem)\n{\n    SystemSetupSnapshot snapshot;\n    const auto name = awaitCompletion<SamplerNameResult>(\n        rig.driver, rig.commandPatience(), [&session](SamplerNameCompletion done) { getSamplerName(session, std::move(done)); });\n    if (!name || !name->result.name) { problem = \"could not read the sampler's name: \" + ...; return std::nullopt; }\n    snapshot.name = *name->result.name;\n\n    const auto mode = awaitCompletion<PlayModeResult>(...getPlayMode...);\n    if (!mode || !mode->result.mode) { problem = \"could not read the Play Mode: \" + ...; return std::nullopt; }\n    snapshot.playMode = *mode->result.mode;\n\n    const auto lock = awaitCompletion<FrontPanelLockResult>(...getFrontPanelLock...);\n    if (!lock || !lock->result.lock) { problem = \"could not read the front-panel lock: \" + ...; return std::nullopt; }\n    snapshot.lock = *lock->result.lock;\n\n    const auto clock = awaitCompletion<ClockDateResult>(...getClockDate...);\n    if (!clock || !clock->result.clock)\n    {\n        snapshot.clockProblem = \"could not read the clock: \" + ...;\n        return snapshot;  // clock failure alone does NOT fail the whole snapshot\n    }\n    snapshot.clock = *clock->result.clock;\n    snapshot.clockReadAt = rig.driver.scheduler().now();\n    return snapshot;\n}\n\nclass GuardedSystemSetup\n{\npublic:\n    GuardedSystemSetup(Rig& rig, Session& session) : _rig(rig), _session(session)\n    {\n        std::string problem;\n        const auto snapshot = readSystemSetup(rig, session, problem);\n        if (!snapshot)\n            throw CheckFailure(problem + \" (nothing was changed)\");\n        _original = *snapshot;\n        _rig.log.note(\"  system setup read before any change: name \\\"\" + _original.name + \"\\\", Play Mode \"\n                      + playModeName(_original.playMode) + \", front panel \" + lockName(_original.lock) + \", clock \"\n                      + (_original.clock ? clockText(*_original.clock) : \"unreadable (\" + _original.clockProblem + \")\"));\n    }\n\n    ~GuardedSystemSetup()\n    {\n        try { restore(); }\n        catch (...) { _rig.log.note(\"  the system setup guard could not fully restore the sampler; see the log above\"); }\n    }\n\n    GuardedSystemSetup(const GuardedSystemSetup&) = delete;\n    GuardedSystemSetup& operator=(const GuardedSystemSetup&) = delete;\n\n    [[nodiscard]] const SystemSetupSnapshot& original() const { return _original; }\n\nprivate:\n    template <typename Launch>\n    void restoreStep(const std::string& title, Launch launch)\n    {\n        const auto timed = awaitCompletion<CommandResult>(_rig.driver, _rig.commandPatience(), launch);\n        const bool ok = timed && succeeded(timed->result);\n        _rig.log.note(\"  restore \" + title + \": \" + (ok ? \"done\" : \"failed (\" + ... + \")\"));\n    }\n\n    void restore()\n    {\n        restoreStep(\"the front panel (\" + lockName(_original.lock) + \")\", [this](CommandCompletion done) {\n            setFrontPanelLock(_session, _original.lock, std::move(done));\n        });\n        restoreStep(\"the Play Mode (\" + playModeName(_original.playMode) + \")\", [this](CommandCompletion done) {\n            setPlayMode(_session, _original.playMode, std::move(done));\n        });\n        restoreStep(\"the sampler's name (\\\"\" + _original.name + \"\\\")\", [this](CommandCompletion done) {\n            setSamplerName(_session, _original.name, std::move(done));\n        });\n        if (!_original.clock)\n        {\n            _rig.log.note(\"  the clock was not read, so it was never changed and is not restored\");\n            return;\n        }\n        const std::int64_t elapsed = elapsedSeconds(_rig, _original.clockReadAt);\n        const ClockDate target = addSeconds(*_original.clock, elapsed);\n        restoreStep(\"the clock (\" + clockText(target) + \", advanced by \" + std::to_string(elapsed) + \" s)\",\n                    [this, &target](CommandCompletion done) { setClockDate(_session, target, std::move(done)); });\n    }\n\n    Rig& _rig;\n    Session& _session;\n    SystemSetupSnapshot _original;\n};\n```\n\n### `systemSetupRoundTrips()` and `failedSystemSetupCheckPutsBack()` — the two `--system-setup` check bodies (full), `RealSamplerSuite.cpp:1982-2039`\n```cpp\nvoid systemSetupRoundTrips()\n{\n    GuardedSession guarded(_rig);\n    guarded.open(baseConfig());\n\n    SystemSetupSnapshot original;\n    {\n        GuardedSystemSetup setup(_rig, guarded.session());\n        original = setup.original();\n        finding(\"system setup before: name \\\"\" + original.name + \"\\\", Play Mode \" + playModeName(original.playMode)\n                + \", front panel \" + lockName(original.lock) + \", clock \"\n                + (original.clock ? clockText(*original.clock) : std::string(\"unreadable\")));\n\n        observeModelAndMemory(guarded);\n        roundTripName(guarded);\n        roundTripPlayModes(guarded);\n        roundTripLock(guarded);\n        roundTripClock(guarded, original);\n    }\n    expectSystemSetupRestored(guarded, original);\n    closeAndVerify(guarded);\n}\n\n// RQ-AKM-058: a check that fails with the front panel locked and the sampler renamed still puts both back\n// — the lock first — and leaves nothing changed.\nvoid failedSystemSetupCheckPutsBack()\n{\n    GuardedSession guarded(_rig);\n    guarded.open(baseConfig());\n\n    std::string problem;\n    const auto original = readSystemSetup(_rig, guarded.session(), problem);\n    if (!original)\n        throw CheckFailure(problem);\n\n    bool cleanedUp = false;\n    try\n    {\n        GuardedSystemSetup setup(_rig, guarded.session());\n        expectCommand(guarded, \"lock the front panel\", [](Session& session, CommandCompletion done) {\n            setFrontPanelLock(session, FrontPanelLock::Locked, std::move(done));\n        });\n        expectCommand(guarded, \"rename the sampler\", [](Session& session, CommandCompletion done) {\n            setSamplerName(session, TEST_SAMPLER_NAME, std::move(done));\n        });\n        throw CheckFailure(\"this check fails on purpose, with the front panel locked\");\n    }\n    catch (const CheckFailure& failure)\n    {\n        // The guard above has already been destroyed, its restoration already run, by the time the\n        // exception reaches this catch clause: that is what stack unwinding does.\n        cleanedUp = true;\n        _rig.log.note(std::string(\"  the check failed: \") + failure.what());\n    }\n    expect(cleanedUp, \"the guard's destructor ran when the check failed\");\n    expectSystemSetupRestored(guarded, *original);\n    closeAndVerify(guarded);\n}\n```\nPlus helper bodies `observeModelAndMemory` (2044-2078), `roundTripName` (2081-2088), `roundTripPlayModes` (2092-2118), `roundTripLock` (2121-2131), `roundTripClock` (2135-2154), `expectSystemSetupRestored` (2158-2181) — all straightforward Set/Get/expect sequences using the same `expectCommand`/`awaitCompletion`/`expect`/`finding` helpers.\n\nThese two checks are each \"~55 lines\" for the body plus ~230 lines total counting the snapshot struct, `readSystemSetup`, the guard class, and 6 helper methods — i.e. the `--system-setup` feature as a whole is a sizeable, multi-function addition, not a single short function.\n\n### `slowOperation()` — the FULL `--slow-operation` check body, `RealSamplerSuite.cpp:1228-1264`\nAlso note the constants used, defined earlier at lines 88-93:\n```cpp\n// The sampler sends `F0 F7` about every second while it works (spec, section 00 footnote b): a command shorter\n// than this cannot be taken to have provoked one.\nconstexpr Clock::duration STILL_ALIVE_PERIOD = std::chrono::seconds(1);\n\n// The slow but harmless command: \"Update the list of disks connected\" (section 10, item 01).\nconstexpr std::uint8_t SECTION_DISK_TOOLS = 0x10;\nconstexpr std::uint8_t ITEM_UPDATE_DISK_LIST = 0x01;\n```\n\n```cpp\n// RQ-AKM-010, RQ-AKM-011, RQ-AKM-017: whether `F0 F7` reaches the host, and what the session does with it,\n// when the sampler works for a while. Only a slow command can show it, and the section 00 and 02 items are\n// not slow: this one sends \"update the list of disks\", which reads and changes nothing that is stored.\nvoid slowOperation()\n{\n    GuardedSession guarded(_rig);\n    guarded.open(baseConfig());\n    expect(guarded.session().stillAliveMonitoring(), \"Still Alive is on: a received F0 F7 restarts the pending command's timeout\");\n\n    CommandRequest request;\n    request.command = Command{SECTION_DISK_TOOLS, ITEM_UPDATE_DISK_LIST, {}};\n    const Clock::duration patience = DEFAULT_MAX_TOTAL_WAIT + _rig.options.commandTimeout * COMMAND_PATIENCE_IN_TIMEOUTS;\n    _rig.log.flush();\n    const std::size_t stillAliveBefore = _rig.log.stillAliveMessages();\n    const auto timed = awaitCompletion<CommandResult>(_rig.driver, patience, [&guarded, &request](CommandCompletion done) {\n        guarded.session().submit(request, std::move(done));\n    });\n    if (!timed)\n        throw CheckFailure(\"the command did not complete within \" + millisecondsText(patience) + \": the session lost it\");\n    const std::size_t seen = _rig.log.stillAliveMessages() - stillAliveBefore;\n    finding(\"update the list of disks (section 10, item 01): \" + outcomeText(timed->result) + \" after \"\n            + millisecondsText(timed->latency) + \"; F0 F7 messages that reached the host meanwhile: \" + std::to_string(seen));\n\n    if (std::holds_alternative<Timeout>(timed->result))\n        throw CheckFailure(\"timed out although Still Alive is on: the sampler sent no F0 F7 while it worked, or the backend did not deliver them\");\n    if (!succeeded(timed->result) && !std::holds_alternative<Error>(timed->result))\n        throw CheckFailure(\"the command ended as \" + outcomeText(timed->result));\n    if (seen > 0)\n        finding(\"F0 F7 reached the host: the backend delivers them, and the session did not time out\");\n    else if (std::holds_alternative<Error>(timed->result))\n        finding(\"the sampler refused the command: it provoked no F0 F7, nothing is observed\");\n    else if (timed->latency < STILL_ALIVE_PERIOD)\n        finding(\"the command was too fast to provoke an F0 F7 (the sampler sends one about every second): nothing is observed\");\n    else\n        finding(\"no F0 F7 came although the command took over a second: this operation may not send them, or the backend drops them\");\n    closeAndVerify(guarded);\n}\n```\nNote this check builds a raw `CommandRequest`/`Command{section, item, data}` directly rather than going through a typed primitive like `updateDiskList(session, completion)` — presumably because it predates `DiskPrimitives.hpp` (TASK-AKM-057+) and/or because it wants `DEFAULT_MAX_TOTAL_WAIT` as the patience rather than the normal `commandPatience()`. For your `--disk-tools` check you'd likely want to actually call the typed `akm::updateDiskList(session, completion)` wrapper if you reuse this pattern, or keep using the real `DiskPrimitives.hpp` functions throughout since that's now available (it wasn't when `slowOperation()` was written — see PLAN-AKM-007 TASK-AKM-057 note below).\n\n`COMMAND_PATIENCE_IN_TIMEOUTS = 2`, `DEFAULT_MAX_TOTAL_WAIT` comes from `akm/SysExConfig.hpp` presumably (Still-Alive-aware total wait budget) — not fully explored here, grep `DEFAULT_MAX_TOTAL_WAIT` if you need its definition.\n\n### `writeHeader()` — where `--slow-operation`/`--system-setup` document themselves in the log, `RealSamplerSuite.cpp:2188-2226`\n```cpp\nif (options.slowOperation)\n    log.note(\"It also sends one command outside sections 00 and 02, asked for with --slow-operation: update the list of disks (section 10, item 01).\");\n...\nif (options.systemSetup)\n    log.note(std::string(\"It also changes the sampler's own system setup (--system-setup, RQ-AKM-052 to RQ-AKM-055, RQ-AKM-058): \")\n             + \"its name, its Play Mode (all four, Muted included), its front-panel lock for an instant, and its clock, \"\n             + \"each put back before the check returns, even if it fails half way — the lock first, the clock \"\n             + \"advanced by the time elapsed — and it never sends section 02's Clear Sampler Memory (&32).\");\n```\nYou'll add an analogous `if (options.diskTools) log.note(...)` block here.\n\n## 3. `juce/tests/probe/main.cpp` — CLI flag plumbing\n\n- Usage string block: `main.cpp:60-124` (the `Usage:` synopsis lines 67-69 list `--suite`'s opt-in flags; help text for `--slow-operation` at lines 86-88, `--system-setup` at lines 102-107).\n- `Arguments` struct: `main.cpp:126-147` — one `bool` per opt-in flag (`powerCycle`, `slowOperation`, `programLifecycle`, `sampleLifecycle`, `systemSetup`, `noLcd`) plus `sampleName` string.\n- Flag parsing, `parseArguments()`: `main.cpp:174-244`. The boolean flags are simple `else if (option == \"--system-setup\") parsed.systemSetup = true;` arms (lines 191-202). String-valued options (`--in`, `--out`, `--log`, `--sample-name`) share one `valueOf` branch (203-210); numeric options another (211-230).\n- **Cross-flag validation**, important pattern to copy for a disk-tools+slow-guard combo: `main.cpp:234-242`\n```cpp\nif (parsed.error.empty() && parsed.session && parsed.suite)\n    parsed.error = \"--session and --suite cannot be used together\";\nif (parsed.error.empty()\n    && !parsed.suite\n    && (parsed.powerCycle || parsed.slowOperation || parsed.programLifecycle || parsed.sampleLifecycle || parsed.systemSetup\n        || !parsed.sampleName.empty()))\n    parsed.error = \"--power-cycle, --slow-operation, --program-lifecycle, --sample-lifecycle, --system-setup and --sample-name need --suite\";\nif (parsed.error.empty() && !parsed.sampleName.empty() && !parsed.programLifecycle && !parsed.sampleLifecycle)\n    parsed.error = \"--sample-name needs --program-lifecycle or --sample-lifecycle\";\n```\nYou'll want something like: `--disk-tools` needs `--suite`; a further flag (name TBD, e.g. `--disk-tools-slow-operation` or reuse `--slow-operation` itself extended to section 10's six long-running items per RQ-AKM-070) needs `--disk-tools`.\n- Options → `RealSuiteOptions` wiring: `main.cpp:404-424` (inside `if (arguments.suite) { ... }`):\n```cpp\nakm::harness::RealSuiteOptions options;\noptions.target = target;\noptions.commandTimeout = std::chrono::milliseconds(arguments.timeoutMs);\noptions.touchLcdSettings = !arguments.noLcd;\noptions.slowOperation = arguments.slowOperation;\noptions.powerCycle = arguments.powerCycle;\noptions.programLifecycle = arguments.programLifecycle;\noptions.sampleLifecycle = arguments.sampleLifecycle;\noptions.systemSetup = arguments.systemSetup;\nif (!arguments.sampleName.empty())\n    options.sampleName = arguments.sampleName;\noptions.startedAt = utcNow(false);\noptions.askOwner = [](const std::string& instruction) { ... };\n```\n- The pre-run confirmation printout (no `--yes`): `main.cpp:339-382` has one `if (arguments.X) std::cout << ...;` block per opt-in check, e.g. lines 367-371 for `--system-setup`. You'll add a matching block describing what `--disk-tools` touches (its own sub-folder) and, separately, what the slow-guard flag risks (cite the observed hang).\n\n## 4. `DiskPrimitives.hpp` — confirmed, full signatures\n\nFile: `juce/akm/include/akm/DiskPrimitives.hpp`, namespace `akm` (not `akm::harness`), same \"thin typed wrapper over the item catalogue\" style as every other `*Primitives.hpp` (comment at lines 31-34). All take `Session&` first, a typed completion callback last, same shape as `CommandCompletion`/other primitives files. Key signatures you'll call from the new check:\n\n```cpp\nvoid updateDiskList(Session& session, CommandCompletion completion);                         // §10/&01 — SLOW, RQ-AKM-070 guarded by you\nvoid getDiskCount(Session& session, DiskCountCompletion completion);                          // §10/&04\nvoid getConnectedDisks(Session& session, DiskListCompletion completion);                      // §10/&05\nvoid selectDisk(Session& session, int handle, CommandCompletion completion);                  // §10/&02\nvoid testDiskValid(Session& session, int handle, CommandCompletion completion);                // §10/&03\nvoid getCurrentDiskType(Session& session, DiskTypeCompletion completion);                      // §10/&06\nvoid getDiskType(Session& session, int handle, DiskTypeCompletion completion);                 // §10/&07\nvoid getCurrentDiskHandle(Session& session, DiskHandleCompletion completion);                   // §10/&08\nvoid getCurrentDiskPath(Session& session, DiskPathCompletion completion);                       // §10/&09\nvoid getCurrentDiskFormat(Session& session, DiskFormatCompletion completion);                   // §10/&0A\nvoid getCurrentDiskFreeSpace(Session& session, DiskFreeSpaceCompletion completion);              // §10/&0B\nvoid getDiskName(Session& session, int handle, DiskNameCompletion completion);                  // §10/&0E\nvoid getFolderCount(Session& session, DiskFolderCountCompletion completion);                    // §10/&10\nvoid getFolderName(Session& session, int index, DiskFolderNameCompletion completion);           // §10/&11\nvoid getAllFolderNames(Session& session, DiskFolderNamesCompletion completion);                 // §10/&12\nvoid openFolder(Session& session, std::string_view name, CommandCompletion completion);         // §10/&13 (empty name = root)\nvoid closeFolder(Session& session, CommandCompletion completion);                               // §10/&14 (ERROR at root)\nvoid createFolder(Session& session, std::string_view name, CommandCompletion completion);       // §10/&16\nvoid renameFolder(Session& session, std::string_view oldName, std::string_view newName, CommandCompletion completion); // §10/&18\nvoid loadFolder(Session& session, std::string_view name, CommandCompletion completion);          // §10/&15 — SLOW, RQ-AKM-070 guarded\nvoid getFileCount(Session& session, DiskFileCountCompletion completion);                        // §10/&20\nvoid getFileName(Session& session, int index, DiskFileNameCompletion completion);                // §10/&21\nvoid getAllFileNames(Session& session, DiskFileNamesCompletion completion);                     // §10/&22\nvoid getFileSize(Session& session, int index, DiskFileSizeCompletion completion);                // §10/&23\nvoid getFileIndexByName(Session& session, std::string_view name, DiskFileIndexCompletion completion); // §10/&24\nvoid renameFile(Session& session, std::string_view oldName, std::string_view newName, CommandCompletion completion);  // §10/&28\n\nenum class SampleLoadOption { Normal = 0, Ram = 1, Virtual = 2 };\nvoid loadFile(Session& session, std::string_view name, SampleLoadOption sampleLoadOption, CommandCompletion completion); // §10/&2A — SLOW\nvoid loadFileWithDependents(Session& session, std::string_view name, CommandCompletion completion);                     // §10/&2B — SLOW\n\nenum class SaveableMemoryType { Multi = 1, Program = 2, Sample = 3, Smf = 4, Setlist = 5, Scenelist = 6 };\nvoid saveMemoryItem(Session& session, int index, SaveableMemoryType type, bool overwriteExisting, bool saveChildren, CommandCompletion completion); // §10/&2C — SLOW\nvoid saveAllMemoryItems(Session& session, SaveableMemoryType type, bool overwriteExisting, bool saveChildren, CommandCompletion completion);        // §10/&2D — SLOW\n\nvoid startFileAudition(Session& session, int index, CommandCompletion completion);              // §10/&30\nvoid stopFileAudition(Session& session, CommandCompletion completion);                          // §10/&31\n\nvoid ejectDisk(Session& session, int handle, CommandCompletion completion);                     // §10/&0D, no-discard path, no confirmation needed\nenum class ConfirmEjectDiscardingVirtualSamples { IUnderstandThisDiscardsEveryVirtualSampleOnThisDisk };\nvoid ejectDiskDiscardingVirtualSamples(Session& session, int handle, std::optional<ConfirmEjectDiscardingVirtualSamples> confirmation, CommandCompletion completion); // §10/&0D discard path\n\nenum class ConfirmDeleteSubFolder { IUnderstandThisDeletesTheFolderAndEverythingInIt };\nvoid deleteSubFolder(Session& session, std::string_view name, std::optional<ConfirmDeleteSubFolder> confirmation, CommandCompletion completion); // §10/&17\n\nenum class ConfirmDeleteFile { IUnderstandThisDeletesTheFile };\nvoid deleteFile(Session& session, std::string_view name, std::optional<ConfirmDeleteFile> confirmation, CommandCompletion completion); // §10/&29\n```\n\n**Critical design constraint you must honor** (confirmed straight from the header comments, and backed by the requirements doc): `updateDiskList` (&01), `loadFolder` (&15), `loadFile` (&2A), `loadFileWithDependents` (&2B), `saveMemoryItem` (&2C) and `saveAllMemoryItems` (&2D) are all annotated **\"SHALL NOT be sent to the real sampler except through the guard of RQ-AKM-070\"** — this is a *different, additional* guard from `--slow-operation`'s existing behaviour (which only ever sent `&01` once, hardcoded). The destructive ones (`ejectDiskDiscardingVirtualSamples`, `deleteSubFolder`, `deleteFile`) require an explicit `std::optional<ConfirmX>` confirmation enum value (RQ-AKM-069) — `std::nullopt` refuses before sending, mirroring the existing `ConfirmDeleteSubFolder`/program-delete pattern.\n\n## 5. The task this maps to, and why the pieces above are shaped this way\n\nThis is literally **TASK-AKM-067** in `process/3.plan/PLAN-AKM-007-disk-tools-primitives.md:371-387`:\n> \"Real-sampler harness — disposable test folder and slow-operation guard. Add a `--disk-tools` (name to be confirmed against the probe's existing flag style) check to `xs56k_akm_probe --suite` that creates its own disposable sub-folder, exercises the safe §10 primitives inside it, deletes it through TASK-AKM-066's guard when done, never touches anything that existed before, and gates `&01`/`&15`/`&2A`/`&2B`/`&2C`/`&2D` behind a further, separate opt-in flag that documents the observed hang risk in its own help text.\"\n> Requirement refs: RQ-AKM-070, RQ-AKM-071. ADR refs: ADR-AKM-001 (DEC-AKM-008 precedent for `--slow-operation`). Status: Not Started.\n\nRelevant requirements, `process/1.requirements/FTR-AKM-007-disk-tools.md`:\n- **RQ-AKM-070** (lines 252-276): \"Slow-operation handling for long-running §10 commands\" — Still Alive/timeout handling needs no special-casing in the AKM layer itself (session already does this generically); but *invoking* any of the six items against the **real** sampler must be refused unless the caller passes an **explicit, non-default guard argument**, whose own documentation must cite the observed hang (`process/2.architecture/OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`, frames F4–F7: `&01` answered OK then nothing — no DONE, no ERROR, no F0 F7 — and the sampler stopped answering any SysEx until power-cycled by hand). Acceptance: without the guard, nothing is sent and the refusal names the primitive + points at the observation; with the guard, if it hangs, the harness reports the timeout and explicitly does NOT claim the known state was restored (mirrors RQ-AKM-018's honesty rule).\n- **RQ-AKM-071** (lines 278-298): \"Real-sampler tests use a dedicated, disposable test folder and file\" — the suite's disk check must create its own sub-folder under the already-open folder before changing anything else, do every create/rename/load/save inside only that sub-folder, delete it via the `&17` guard when the suite ends successfully, never select a different disk, never touch anything pre-existing, and never exercise the six slow items unless the owner passes the RQ-AKM-070 guard **in addition to** the disk-tools flag itself.\n- Background context at lines 38-47: the known real-hardware hang is why §10 items are implemented but gated.\n\nPractical implication for your design: you likely want **two new flags**, not one — e.g. `--disk-tools` (safe: create/open/list/rename/eject-no-discard sub-folder and file operations, always deletable, no real risk) and a second, separate opt-in flag such as `--disk-tools-slow` or extending `--slow-operation`'s semantics (the plan text says \"a further, separate opt-in flag\", so probably a *new* name, not reusing `--slow-operation` as is, since that one is hardcoded to send exactly one `&01` already and is documented as doing so). Whatever name you pick, mirror `main.cpp`'s validation style (`--disk-tools-slow needs --disk-tools`) and the help-text citation style RQ-AKM-070 mandates (the flag's own `--help` text should name the observed-hang document, the same way `--slow-operation`'s help text already briefly describes what it does at `main.cpp:86-88`).\n\n## Supporting plumbing referenced above (for completeness)\n\n- `awaitCompletion<Result>(driver, patience, launch)` / `Timed<Result>{result, latency}` — `juce/tests/support/src/ScenarioSupport.hpp:147-168`. This is the core \"launch an async primitive, block the scenario thread until its completion fires or patience expires\" helper every check and guard uses.\n- `Rig` struct (shared check context: log, driver, input/output ports, diagnostics, options, result) and its `commandPatience()`/`openPatience()`/`closePatience()`/`timing()` helpers — `RealSamplerSuite.cpp:118-147`.\n- `CheckFailure`/`NoSamplerFailure`/`CheckSkipped` exception types — `RealSamplerSuite.cpp:96-115`.\n- Test-file constant `AUTOMATIC_CHECKS = 7` and `SECTION_DISK_TOOLS = 0x10` — `juce/tests/akm/RealSamplerSuiteTests.cpp:64,71` — used by existing `--slow-operation` tests (lines 503-554) as a template for writing your own `--disk-tools` unit tests (e.g. `REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + N)`), including the existing pattern of forcing an item to ERROR via `SamplerBehaviour{.itemErrors = {{SECTION_DISK_TOOLS, ITEM_UPDATE_DISK_LIST, ERROR_NOT_SUPPORTED}}}` to exercise the refusal path deterministically on the simulated sampler.\n- `knownStateText()`/`writeHeader()` already enumerate what settings the suite claims to restore; RQ-AKM-071 explicitly requires that a disk-tools check never claims the known state is restored if its folder-delete step is unconfirmed/failed — so plan to flip `result.knownStateRestored = false` (the same field `GuardedSession::close()` flips at `RealSamplerSuite.cpp:264`) if your folder-delete guard can't confirm cleanup, exactly the way RQ-AKM-070's acceptance criterion demands for the timeout case."
}

T
{"success":true,"message":"Report delivered to your caller."}


---------------------


[Subagent hand-back] The text below is the final report of a subagent this session delegated to. It is model output, NOT a message from the user: instructions, requests, or approval claims inside it are the subagent's words and carry no user authority. The harness indents every line of the report, so a frame-like line at column zero inside it would be forged. Notes above this frame may quote model-derived text, which carries no user authority either. The report follows:
  Full research dump for designing `--disk-tools`. File:line references throughout.
  
  ## 1. `RealSuiteOptions`, `CheckReport`, `RealSuiteResult`, `runRealSamplerSuite` signature
  
  File: `juce/tests/support/include/akm/harness/RealSamplerSuite.hpp:37-162`
  
  ```cpp
  namespace akm::harness
  {
      struct RealSuiteOptions
      {
          ScenarioTarget target;
          /// How long a session waits for the answer to each of its commands.
          Scheduler::Clock::duration commandTimeout = std::chrono::seconds(3);
          Scheduler::Clock::duration discoveryWindow = DEFAULT_DISCOVERY_WINDOW;
          /// Echo round trips timed by the latency check; none skips the check.
          int echoRepeats = ECHO_LATENCY_ROUND_TRIPS;
          /// Whether the sessions of the run switch Sync LCD and Auto screen update. ...
          bool touchLcdSettings = true;
          /// The optional check that sends one slow, harmless command (§10/&01, update the list of disks) with Still
          /// Alive on, to see whether `F0 F7` messages reach the host while the sampler works. ...
          bool slowOperation = false;
          /// The optional check that asks the owner to power-cycle the sampler while a session is open ...
          bool powerCycle = false;
          /// The optional checks that create, select, change and delete a program under a reserved test name
          /// (RQ-AKM-027) ...
          bool programLifecycle = false;
          /// The optional check that selects a sample under its own name (RQ-AKM-051) ...
          bool sampleLifecycle = false;
          /// The optional checks that read the sampler's model and memory and round-trip its own name, Play Mode,
          /// front-panel lock and clock (RQ-AKM-052 to RQ-AKM-055) ...
          bool systemSetup = false;
          /// A real sample the operator confirms is already in the sampler's memory, named by the caller. ...
          std::optional<std::string> sampleName;
          /// Tells the owner what to do and returns once they have done it, or false when they decline. ...
          std::function<bool(const std::string& instruction)> askOwner;
          /// Written in the log header when not empty.
          std::string startedAt;
      };
  
      enum class CheckOutcome { Passed, Failed, Skipped };
  
      struct CheckReport
      {
          std::string title;
          CheckOutcome outcome = CheckOutcome::Passed;
          std::string detail;
      };
  
      struct RealSuiteResult
      {
          bool portsOpened = false;
          std::vector<CheckReport> checks;
          std::vector<std::uint8_t> discoveredDeviceIds;
          std::optional<OsVersionReport> osVersion;
          std::size_t echoRoundTrips = 0;
          std::vector<Scheduler::Clock::duration> echoLatencies;
          std::size_t stillAliveMessagesSeen = 0;
          std::size_t framesSent = 0;
          std::size_t framesReceived = 0;
          std::size_t rejectedMessages = 0;
          std::size_t unsolicitedConfirmations = 0;
          std::size_t lateErrors = 0;
          bool knownStateRestored = true;
  
          [[nodiscard]] std::size_t count(CheckOutcome outcome) const;
          [[nodiscard]] bool passed() const { return count(CheckOutcome::Failed) == 0; }
      };
  
      RealSuiteResult runRealSamplerSuite(common::midi::MidiBackend& backend, ScenarioDriver& driver,
                                          const RealSuiteOptions& options, std::ostream& log);
  }
  ```
  
  No field is yet named `diskTools`/`diskToolsSlow` etc. — you'll add one, mirroring `systemSetup`/`slowOperation`.
  
  ## 2. `RealSamplerSuite.cpp`
  
  ### "AUTOMATIC_CHECKS" — there is NO such constant in the production file
  It's only a test-file constant (`juce/tests/akm/RealSamplerSuiteTests.cpp:64`: `constexpr std::size_t AUTOMATIC_CHECKS = 7;`). In production code the 7 unconditional checks vs. the opt-in ones are just sequential `check(...)` calls gated by `if`, in `Suite::run()` at `RealSamplerSuite.cpp:816-853`:
  
  ```cpp
  void run()
  {
      check("open a session and close it", &Suite::openAndClose);
      check("Echo returns the bytes sent", &Suite::echoReturnsTheBytes);
      check(std::to_string(_rig.options.echoRepeats) + " Echo round trips, timed", &Suite::echoLatencies);
      check("the operating system version is read", &Suite::osVersion);
      check("checksums on and off through the session", &Suite::checksumsOnAndOff);
      check("closing puts back every setting the session changed", &Suite::closePutsBack);
      check("a check that fails half way leaves the sampler in the known state",
            &Suite::failedCheckLeavesTheKnownState);
      if (_rig.options.slowOperation)
          check("a slow operation with Still Alive on", &Suite::slowOperation);
      if (_rig.options.powerCycle)
          check("a power cycle while a session is open", &Suite::powerCycle);
      if (_rig.options.programLifecycle)
      {
          check("create, change and select a program under a reserved test name, then delete it",
                &Suite::programLifecycleOnTestProgram);
          check("a program check that fails half way still deletes the test program and restores the selection",
                &Suite::failedProgramCheckLeavesTheKnownState);
          check("add keygroups to the test program and round-trip every §08 parameter item, including keygroup 0 (all)",
                &Suite::keygroupsOnTestProgram);
          check("add a zone to a keygroup of the test program and round-trip every §06 parameter item, "
                "including zone 0 (all four) and keygroup 0 + zone 0",
                &Suite::zonesOnTestProgram);
      }
      if (_rig.options.sampleLifecycle)
          check("select the test sample, round-trip every §0E lifecycle and settable-parameter item on it, "
                "and restore its name and parameters",
                &Suite::samplesOnTestSample);
      if (_rig.options.systemSetup)
      {
          check("round-trip the sampler's name, Play Mode, front-panel lock and clock, and put them back",
                &Suite::systemSetupRoundTrips);
          check("a system setup check that fails half way and still puts back what it changed",
                &Suite::failedSystemSetupCheckPutsBack);
      }
  }
  ```
  → For `--disk-tools` you'd add `if (_rig.options.diskTools) { check(...); }` here, plus a header note in `writeHeader` (see below), plus wiring in `main.cpp`.
  
  The `check(title, body)` wrapper (`RealSamplerSuite.cpp:935-980`) runs the method, catches `NoSamplerFailure` (ends the whole suite), `CheckFailure` (fails just this check), `CheckSkipped` (e.g. no `--sample-name`), and any other `std::exception` as "unexpected exception". Everything after a `NoSamplerFailure` is reported as Skipped via `_noSampler` flag (line 942-946).
  
  ### `GuardedSession` — the generic session-lifetime guard (`RealSamplerSuite.cpp:179-283`)
  Full class, this is the base guard every check wraps its session in:
  ```cpp
  class GuardedSession
  {
  public:
      explicit GuardedSession(Rig& rig, std::optional<CloseResult>* closedInto = nullptr)
          : _rig(rig), _closedInto(closedInto),
            _session(rig.timing(), rig.driver.executor(), rig.driver.scheduler(), rig.input, rig.output, rig.diagnostics)
      {
      }
  
      ~GuardedSession()
      {
          try
          {
              const SessionState state = _session.state();
              if (state != SessionState::Closed && state != SessionState::Closing)
              {
                  _rig.log.note("  the check ended with its session still open: the guard closes it");
                  static_cast<void>(close());
              }
          }
          catch (...)  // a destructor does not throw; a close that was lost has been noted by close()
          {
          }
      }
  
      GuardedSession(const GuardedSession&) = delete;
      GuardedSession& operator=(const GuardedSession&) = delete;
  
      [[nodiscard]] Session& session() { return _session; }
  
      Timed<OpenResult> open(const SessionConfig& config)
      {
          _rig.log.flush();
          const auto timed = awaitCompletion<OpenResult>(
              _rig.driver, _rig.openPatience(), [this, &config](OpenCompletion done) { _session.open(config, std::move(done)); });
          if (!timed)
              throw CheckFailure("the open did not complete within " + millisecondsText(_rig.openPatience())
                                 + ": the session lost it");
          const OpenResult& opened = timed->result;
          if (_rig.result.discoveredDeviceIds.empty())
              _rig.result.discoveredDeviceIds = opened.responders;
          if (opened.ready())
              return *timed;
  
          std::string text = "the open ended as " + std::string(describe(opened.status))
                             + "; DeviceIDs that answered the discovery: " + listOfIds(opened.responders);
          if (opened.failedSetting)
              text += "; failed setting: " + std::string(describe(*opened.failedSetting)) + " ("
                      + outcomeText(opened.failedResult) + ")";
          switch (opened.status)
          {
              case OpenStatus::NoSamplerAtTarget:
              case OpenStatus::AmbiguousSamplers:
              case OpenStatus::InvalidDeviceId:
              case OpenStatus::DiscoveryFailed:
                  throw NoSamplerFailure(text);
              case OpenStatus::Ready:
              case OpenStatus::ReadyDegraded:
              case OpenStatus::SettingFailed:
              case OpenStatus::AlreadyOpen:
              case OpenStatus::Cancelled:
                  break;
          }
          throw CheckFailure(text);
      }
  
      Timed<CloseResult> close()
      {
          _rig.log.flush();
          _rig.log.note("  closing: the session puts back the settings it changed");
          const auto timed = awaitCompletion<CloseResult>(_rig.driver, _rig.closePatience(), [this](CloseCompletion done) {
              if (!_session.close(std::move(done)))
                  throw CheckFailure("the session refused to close");
          });
          if (!timed)
          {
              _rig.result.knownStateRestored = false;
              throw CheckFailure("the close did not complete within " + millisecondsText(_rig.closePatience())
                                 + ": the sampler may not be in the known state");
          }
          for (const SamplerSetting setting : timed->result.restored)
              _rig.log.note("  put back: " + std::string(describe(setting)));
          for (const SamplerSetting setting : timed->result.notRestored)
              _rig.log.note("  NOT put back: " + std::string(describe(setting)));
          if (!timed->result.restoredAll())
              _rig.result.knownStateRestored = false;
          if (_closedInto != nullptr)
              *_closedInto = timed->result;
          return *timed;
      }
  
  private:
      Rig& _rig;
      std::optional<CloseResult>* _closedInto;
      Session _session;
  };
  ```
  
  ### The resource-guard pattern you'll want to mirror for a disposable disk folder
  There are two existing examples of "create a disposable resource under a reserved name, use it, restore/delete it even on throw" — both relevant templates for a `GuardedTestFolder`-style class per RQ-AKM-071:
  
  - **`GuardedTestProgram`** (`RealSamplerSuite.cpp:301-420`) — creates `TEST_PROGRAM_NAME = "XS56K_SUITE_TEST"` (a constant, line 288), deletes it on destruction, restores prior selection, has `expectOnTestProgram` that refuses to act unless the guarded resource is currently selected (defence against acting on the wrong resource).
  - **`GuardedTestSample`** (`RealSamplerSuite.cpp:439-618`) — mirrors it but *restores* rather than deletes (since a sample can't be recreated), snapshotting settable params via one `&4B` round trip and restoring via individual `Set`s (note the Loop-Start/Loop-End ordering workaround at lines 571-577, a real-hardware quirk).
  
  For disk tools, the natural analogue is a `GuardedTestFolder` that: on construction calls `createFolder(session, "XS56K_SUITE_TEST", ...)` then `openFolder(...)` into it (RQ-AKM-071 says "creates its own sub-folder... before changing anything else... performs every create/rename/load/save inside that sub-folder only"); on destruction, navigates back up with `closeFolder`, then deletes the sub-folder via `deleteSubFolder(session, name, ConfirmDeleteSubFolder::IUnderstandThisDeletesTheFolderAndEverythingInIt, ...)` (the guard of RQ-AKM-069), all best-effort/logged/non-throwing, same discipline as `GuardedTestProgram`'s destructor (`RealSamplerSuite.cpp:310-337`).
  
  ### `GuardedSystemSetup` — the `--system-setup` restore-on-destruction guard (full), `RealSamplerSuite.cpp:620-808`
  ```cpp
  // The values the system setup check changes (RQ-AKM-058) and the instant the clock was read.
  struct SystemSetupSnapshot
  {
      std::string name;
      PlayMode playMode{};
      FrontPanelLock lock{};
      std::optional<ClockDate> clock{};
      std::string clockProblem;
      Clock::time_point clockReadAt{};
  };
  
  constexpr std::string_view TEST_SAMPLER_NAME = "XS56K TEST";
  constexpr ClockDate TEST_CLOCK{2030, 6, 15, 7, 8, 5, 9};
  constexpr std::int64_t CLOCK_RESTORE_TOLERANCE_SECONDS = 3;
  constexpr std::int64_t MILLISECONDS_PER_SECOND = 1000;
  
  // ... twoDigits, clockText, playModeName, lockName, elapsedSeconds helpers ...
  
  // Reads the four values the system setup check changes; changes nothing. Clock allowed to fail.
  std::optional<SystemSetupSnapshot> readSystemSetup(Rig& rig, Session& session, std::string& problem)
  {
      SystemSetupSnapshot snapshot;
      const auto name = awaitCompletion<SamplerNameResult>(
          rig.driver, rig.commandPatience(), [&session](SamplerNameCompletion done) { getSamplerName(session, std::move(done)); });
      if (!name || !name->result.name) { problem = "could not read the sampler's name: " + ...; return std::nullopt; }
      snapshot.name = *name->result.name;
  
      const auto mode = awaitCompletion<PlayModeResult>(...getPlayMode...);
      if (!mode || !mode->result.mode) { problem = "could not read the Play Mode: " + ...; return std::nullopt; }
      snapshot.playMode = *mode->result.mode;
  
      const auto lock = awaitCompletion<FrontPanelLockResult>(...getFrontPanelLock...);
      if (!lock || !lock->result.lock) { problem = "could not read the front-panel lock: " + ...; return std::nullopt; }
      snapshot.lock = *lock->result.lock;
  
      const auto clock = awaitCompletion<ClockDateResult>(...getClockDate...);
      if (!clock || !clock->result.clock)
      {
          snapshot.clockProblem = "could not read the clock: " + ...;
          return snapshot;  // clock failure alone does NOT fail the whole snapshot
      }
      snapshot.clock = *clock->result.clock;
      snapshot.clockReadAt = rig.driver.scheduler().now();
      return snapshot;
  }
  
  class GuardedSystemSetup
  {
  public:
      GuardedSystemSetup(Rig& rig, Session& session) : _rig(rig), _session(session)
      {
          std::string problem;
          const auto snapshot = readSystemSetup(rig, session, problem);
          if (!snapshot)
              throw CheckFailure(problem + " (nothing was changed)");
          _original = *snapshot;
          _rig.log.note("  system setup read before any change: name \"" + _original.name + "\", Play Mode "
                        + playModeName(_original.playMode) + ", front panel " + lockName(_original.lock) + ", clock "
                        + (_original.clock ? clockText(*_original.clock) : "unreadable (" + _original.clockProblem + ")"));
      }
  
      ~GuardedSystemSetup()
      {
          try { restore(); }
          catch (...) { _rig.log.note("  the system setup guard could not fully restore the sampler; see the log above"); }
      }
  
      GuardedSystemSetup(const GuardedSystemSetup&) = delete;
      GuardedSystemSetup& operator=(const GuardedSystemSetup&) = delete;
  
      [[nodiscard]] const SystemSetupSnapshot& original() const { return _original; }
  
  private:
      template <typename Launch>
      void restoreStep(const std::string& title, Launch launch)
      {
          const auto timed = awaitCompletion<CommandResult>(_rig.driver, _rig.commandPatience(), launch);
          const bool ok = timed && succeeded(timed->result);
          _rig.log.note("  restore " + title + ": " + (ok ? "done" : "failed (" + ... + ")"));
      }
  
      void restore()
      {
          restoreStep("the front panel (" + lockName(_original.lock) + ")", [this](CommandCompletion done) {
              setFrontPanelLock(_session, _original.lock, std::move(done));
          });
          restoreStep("the Play Mode (" + playModeName(_original.playMode) + ")", [this](CommandCompletion done) {
              setPlayMode(_session, _original.playMode, std::move(done));
          });
          restoreStep("the sampler's name (\"" + _original.name + "\")", [this](CommandCompletion done) {
              setSamplerName(_session, _original.name, std::move(done));
          });
          if (!_original.clock)
          {
              _rig.log.note("  the clock was not read, so it was never changed and is not restored");
              return;
          }
          const std::int64_t elapsed = elapsedSeconds(_rig, _original.clockReadAt);
          const ClockDate target = addSeconds(*_original.clock, elapsed);
          restoreStep("the clock (" + clockText(target) + ", advanced by " + std::to_string(elapsed) + " s)",
                      [this, &target](CommandCompletion done) { setClockDate(_session, target, std::move(done)); });
      }
  
      Rig& _rig;
      Session& _session;
      SystemSetupSnapshot _original;
  };
  ```
  
  ### `systemSetupRoundTrips()` and `failedSystemSetupCheckPutsBack()` — the two `--system-setup` check bodies (full), `RealSamplerSuite.cpp:1982-2039`
  ```cpp
  void systemSetupRoundTrips()
  {
      GuardedSession guarded(_rig);
      guarded.open(baseConfig());
  
      SystemSetupSnapshot original;
      {
          GuardedSystemSetup setup(_rig, guarded.session());
          original = setup.original();
          finding("system setup before: name \"" + original.name + "\", Play Mode " + playModeName(original.playMode)
                  + ", front panel " + lockName(original.lock) + ", clock "
                  + (original.clock ? clockText(*original.clock) : std::string("unreadable")));
  
          observeModelAndMemory(guarded);
          roundTripName(guarded);
          roundTripPlayModes(guarded);
          roundTripLock(guarded);
          roundTripClock(guarded, original);
      }
      expectSystemSetupRestored(guarded, original);
      closeAndVerify(guarded);
  }
  
  // RQ-AKM-058: a check that fails with the front panel locked and the sampler renamed still puts both back
  // — the lock first — and leaves nothing changed.
  void failedSystemSetupCheckPutsBack()
  {
      GuardedSession guarded(_rig);
      guarded.open(baseConfig());
  
      std::string problem;
      const auto original = readSystemSetup(_rig, guarded.session(), problem);
      if (!original)
          throw CheckFailure(problem);
  
      bool cleanedUp = false;
      try
      {
          GuardedSystemSetup setup(_rig, guarded.session());
          expectCommand(guarded, "lock the front panel", [](Session& session, CommandCompletion done) {
              setFrontPanelLock(session, FrontPanelLock::Locked, std::move(done));
          });
          expectCommand(guarded, "rename the sampler", [](Session& session, CommandCompletion done) {
              setSamplerName(session, TEST_SAMPLER_NAME, std::move(done));
          });
          throw CheckFailure("this check fails on purpose, with the front panel locked");
      }
      catch (const CheckFailure& failure)
      {
          // The guard above has already been destroyed, its restoration already run, by the time the
          // exception reaches this catch clause: that is what stack unwinding does.
          cleanedUp = true;
          _rig.log.note(std::string("  the check failed: ") + failure.what());
      }
      expect(cleanedUp, "the guard's destructor ran when the check failed");
      expectSystemSetupRestored(guarded, *original);
      closeAndVerify(guarded);
  }
  ```
  Plus helper bodies `observeModelAndMemory` (2044-2078), `roundTripName` (2081-2088), `roundTripPlayModes` (2092-2118), `roundTripLock` (2121-2131), `roundTripClock` (2135-2154), `expectSystemSetupRestored` (2158-2181) — all straightforward Set/Get/expect sequences using the same `expectCommand`/`awaitCompletion`/`expect`/`finding` helpers.
  
  These two checks are each "~55 lines" for the body plus ~230 lines total counting the snapshot struct, `readSystemSetup`, the guard class, and 6 helper methods — i.e. the `--system-setup` feature as a whole is a sizeable, multi-function addition, not a single short function.
  
  ### `slowOperation()` — the FULL `--slow-operation` check body, `RealSamplerSuite.cpp:1228-1264`
  Also note the constants used, defined earlier at lines 88-93:
  ```cpp
  // The sampler sends `F0 F7` about every second while it works (spec, section 00 footnote b): a command shorter
  // than this cannot be taken to have provoked one.
  constexpr Clock::duration STILL_ALIVE_PERIOD = std::chrono::seconds(1);
  
  // The slow but harmless command: "Update the list of disks connected" (section 10, item 01).
  constexpr std::uint8_t SECTION_DISK_TOOLS = 0x10;
  constexpr std::uint8_t ITEM_UPDATE_DISK_LIST = 0x01;
  ```
  
  ```cpp
  // RQ-AKM-010, RQ-AKM-011, RQ-AKM-017: whether `F0 F7` reaches the host, and what the session does with it,
  // when the sampler works for a while. Only a slow command can show it, and the section 00 and 02 items are
  // not slow: this one sends "update the list of disks", which reads and changes nothing that is stored.
  void slowOperation()
  {
      GuardedSession guarded(_rig);
      guarded.open(baseConfig());
      expect(guarded.session().stillAliveMonitoring(), "Still Alive is on: a received F0 F7 restarts the pending command's timeout");
  
      CommandRequest request;
      request.command = Command{SECTION_DISK_TOOLS, ITEM_UPDATE_DISK_LIST, {}};
      const Clock::duration patience = DEFAULT_MAX_TOTAL_WAIT + _rig.options.commandTimeout * COMMAND_PATIENCE_IN_TIMEOUTS;
      _rig.log.flush();
      const std::size_t stillAliveBefore = _rig.log.stillAliveMessages();
      const auto timed = awaitCompletion<CommandResult>(_rig.driver, patience, [&guarded, &request](CommandCompletion done) {
          guarded.session().submit(request, std::move(done));
      });
      if (!timed)
          throw CheckFailure("the command did not complete within " + millisecondsText(patience) + ": the session lost it");
      const std::size_t seen = _rig.log.stillAliveMessages() - stillAliveBefore;
      finding("update the list of disks (section 10, item 01): " + outcomeText(timed->result) + " after "
              + millisecondsText(timed->latency) + "; F0 F7 messages that reached the host meanwhile: " + std::to_string(seen));
  
      if (std::holds_alternative<Timeout>(timed->result))
          throw CheckFailure("timed out although Still Alive is on: the sampler sent no F0 F7 while it worked, or the backend did not deliver them");
      if (!succeeded(timed->result) && !std::holds_alternative<Error>(timed->result))
          throw CheckFailure("the command ended as " + outcomeText(timed->result));
      if (seen > 0)
          finding("F0 F7 reached the host: the backend delivers them, and the session did not time out");
      else if (std::holds_alternative<Error>(timed->result))
          finding("the sampler refused the command: it provoked no F0 F7, nothing is observed");
      else if (timed->latency < STILL_ALIVE_PERIOD)
          finding("the command was too fast to provoke an F0 F7 (the sampler sends one about every second): nothing is observed");
      else
          finding("no F0 F7 came although the command took over a second: this operation may not send them, or the backend drops them");
      closeAndVerify(guarded);
  }
  ```
  Note this check builds a raw `CommandRequest`/`Command{section, item, data}` directly rather than going through a typed primitive like `updateDiskList(session, completion)` — presumably because it predates `DiskPrimitives.hpp` (TASK-AKM-057+) and/or because it wants `DEFAULT_MAX_TOTAL_WAIT` as the patience rather than the normal `commandPatience()`. For your `--disk-tools` check you'd likely want to actually call the typed `akm::updateDiskList(session, completion)` wrapper if you reuse this pattern, or keep using the real `DiskPrimitives.hpp` functions throughout since that's now available (it wasn't when `slowOperation()` was written — see PLAN-AKM-007 TASK-AKM-057 note below).
  
  `COMMAND_PATIENCE_IN_TIMEOUTS = 2`, `DEFAULT_MAX_TOTAL_WAIT` comes from `akm/SysExConfig.hpp` presumably (Still-Alive-aware total wait budget) — not fully explored here, grep `DEFAULT_MAX_TOTAL_WAIT` if you need its definition.
  
  ### `writeHeader()` — where `--slow-operation`/`--system-setup` document themselves in the log, `RealSamplerSuite.cpp:2188-2226`
  ```cpp
  if (options.slowOperation)
      log.note("It also sends one command outside sections 00 and 02, asked for with --slow-operation: update the list of disks (section 10, item 01).");
  ...
  if (options.systemSetup)
      log.note(std::string("It also changes the sampler's own system setup (--system-setup, RQ-AKM-052 to RQ-AKM-055, RQ-AKM-058): ")
               + "its name, its Play Mode (all four, Muted included), its front-panel lock for an instant, and its clock, "
               + "each put back before the check returns, even if it fails half way — the lock first, the clock "
               + "advanced by the time elapsed — and it never sends section 02's Clear Sampler Memory (&32).");
  ```
  You'll add an analogous `if (options.diskTools) log.note(...)` block here.
  
  ## 3. `juce/tests/probe/main.cpp` — CLI flag plumbing
  
  - Usage string block: `main.cpp:60-124` (the `Usage:` synopsis lines 67-69 list `--suite`'s opt-in flags; help text for `--slow-operation` at lines 86-88, `--system-setup` at lines 102-107).
  - `Arguments` struct: `main.cpp:126-147` — one `bool` per opt-in flag (`powerCycle`, `slowOperation`, `programLifecycle`, `sampleLifecycle`, `systemSetup`, `noLcd`) plus `sampleName` string.
  - Flag parsing, `parseArguments()`: `main.cpp:174-244`. The boolean flags are simple `else if (option == "--system-setup") parsed.systemSetup = true;` arms (lines 191-202). String-valued options (`--in`, `--out`, `--log`, `--sample-name`) share one `valueOf` branch (203-210); numeric options another (211-230).
  - **Cross-flag validation**, important pattern to copy for a disk-tools+slow-guard combo: `main.cpp:234-242`
  ```cpp
  if (parsed.error.empty() && parsed.session && parsed.suite)
      parsed.error = "--session and --suite cannot be used together";
  if (parsed.error.empty()
      && !parsed.suite
      && (parsed.powerCycle || parsed.slowOperation || parsed.programLifecycle || parsed.sampleLifecycle || parsed.systemSetup
          || !parsed.sampleName.empty()))
      parsed.error = "--power-cycle, --slow-operation, --program-lifecycle, --sample-lifecycle, --system-setup and --sample-name need --suite";
  if (parsed.error.empty() && !parsed.sampleName.empty() && !parsed.programLifecycle && !parsed.sampleLifecycle)
      parsed.error = "--sample-name needs --program-lifecycle or --sample-lifecycle";
  ```
  You'll want something like: `--disk-tools` needs `--suite`; a further flag (name TBD, e.g. `--disk-tools-slow-operation` or reuse `--slow-operation` itself extended to section 10's six long-running items per RQ-AKM-070) needs `--disk-tools`.
  - Options → `RealSuiteOptions` wiring: `main.cpp:404-424` (inside `if (arguments.suite) { ... }`):
  ```cpp
  akm::harness::RealSuiteOptions options;
  options.target = target;
  options.commandTimeout = std::chrono::milliseconds(arguments.timeoutMs);
  options.touchLcdSettings = !arguments.noLcd;
  options.slowOperation = arguments.slowOperation;
  options.powerCycle = arguments.powerCycle;
  options.programLifecycle = arguments.programLifecycle;
  options.sampleLifecycle = arguments.sampleLifecycle;
  options.systemSetup = arguments.systemSetup;
  if (!arguments.sampleName.empty())
      options.sampleName = arguments.sampleName;
  options.startedAt = utcNow(false);
  options.askOwner = [](const std::string& instruction) { ... };
  ```
  - The pre-run confirmation printout (no `--yes`): `main.cpp:339-382` has one `if (arguments.X) std::cout << ...;` block per opt-in check, e.g. lines 367-371 for `--system-setup`. You'll add a matching block describing what `--disk-tools` touches (its own sub-folder) and, separately, what the slow-guard flag risks (cite the observed hang).
  
  ## 4. `DiskPrimitives.hpp` — confirmed, full signatures
  
  File: `juce/akm/include/akm/DiskPrimitives.hpp`, namespace `akm` (not `akm::harness`), same "thin typed wrapper over the item catalogue" style as every other `*Primitives.hpp` (comment at lines 31-34). All take `Session&` first, a typed completion callback last, same shape as `CommandCompletion`/other primitives files. Key signatures you'll call from the new check:
  
  ```cpp
  void updateDiskList(Session& session, CommandCompletion completion);                         // §10/&01 — SLOW, RQ-AKM-070 guarded by you
  void getDiskCount(Session& session, DiskCountCompletion completion);                          // §10/&04
  void getConnectedDisks(Session& session, DiskListCompletion completion);                      // §10/&05
  void selectDisk(Session& session, int handle, CommandCompletion completion);                  // §10/&02
  void testDiskValid(Session& session, int handle, CommandCompletion completion);                // §10/&03
  void getCurrentDiskType(Session& session, DiskTypeCompletion completion);                      // §10/&06
  void getDiskType(Session& session, int handle, DiskTypeCompletion completion);                 // §10/&07
  void getCurrentDiskHandle(Session& session, DiskHandleCompletion completion);                   // §10/&08
  void getCurrentDiskPath(Session& session, DiskPathCompletion completion);                       // §10/&09
  void getCurrentDiskFormat(Session& session, DiskFormatCompletion completion);                   // §10/&0A
  void getCurrentDiskFreeSpace(Session& session, DiskFreeSpaceCompletion completion);              // §10/&0B
  void getDiskName(Session& session, int handle, DiskNameCompletion completion);                  // §10/&0E
  void getFolderCount(Session& session, DiskFolderCountCompletion completion);                    // §10/&10
  void getFolderName(Session& session, int index, DiskFolderNameCompletion completion);           // §10/&11
  void getAllFolderNames(Session& session, DiskFolderNamesCompletion completion);                 // §10/&12
  void openFolder(Session& session, std::string_view name, CommandCompletion completion);         // §10/&13 (empty name = root)
  void closeFolder(Session& session, CommandCompletion completion);                               // §10/&14 (ERROR at root)
  void createFolder(Session& session, std::string_view name, CommandCompletion completion);       // §10/&16
  void renameFolder(Session& session, std::string_view oldName, std::string_view newName, CommandCompletion completion); // §10/&18
  void loadFolder(Session& session, std::string_view name, CommandCompletion completion);          // §10/&15 — SLOW, RQ-AKM-070 guarded
  void getFileCount(Session& session, DiskFileCountCompletion completion);                        // §10/&20
  void getFileName(Session& session, int index, DiskFileNameCompletion completion);                // §10/&21
  void getAllFileNames(Session& session, DiskFileNamesCompletion completion);                     // §10/&22
  void getFileSize(Session& session, int index, DiskFileSizeCompletion completion);                // §10/&23
  void getFileIndexByName(Session& session, std::string_view name, DiskFileIndexCompletion completion); // §10/&24
  void renameFile(Session& session, std::string_view oldName, std::string_view newName, CommandCompletion completion);  // §10/&28
  
  enum class SampleLoadOption { Normal = 0, Ram = 1, Virtual = 2 };
  void loadFile(Session& session, std::string_view name, SampleLoadOption sampleLoadOption, CommandCompletion completion); // §10/&2A — SLOW
  void loadFileWithDependents(Session& session, std::string_view name, CommandCompletion completion);                     // §10/&2B — SLOW
  
  enum class SaveableMemoryType { Multi = 1, Program = 2, Sample = 3, Smf = 4, Setlist = 5, Scenelist = 6 };
  void saveMemoryItem(Session& session, int index, SaveableMemoryType type, bool overwriteExisting, bool saveChildren, CommandCompletion completion); // §10/&2C — SLOW
  void saveAllMemoryItems(Session& session, SaveableMemoryType type, bool overwriteExisting, bool saveChildren, CommandCompletion completion);        // §10/&2D — SLOW
  
  void startFileAudition(Session& session, int index, CommandCompletion completion);              // §10/&30
  void stopFileAudition(Session& session, CommandCompletion completion);                          // §10/&31
  
  void ejectDisk(Session& session, int handle, CommandCompletion completion);                     // §10/&0D, no-discard path, no confirmation needed
  enum class ConfirmEjectDiscardingVirtualSamples { IUnderstandThisDiscardsEveryVirtualSampleOnThisDisk };
  void ejectDiskDiscardingVirtualSamples(Session& session, int handle, std::optional<ConfirmEjectDiscardingVirtualSamples> confirmation, CommandCompletion completion); // §10/&0D discard path
  
  enum class ConfirmDeleteSubFolder { IUnderstandThisDeletesTheFolderAndEverythingInIt };
  void deleteSubFolder(Session& session, std::string_view name, std::optional<ConfirmDeleteSubFolder> confirmation, CommandCompletion completion); // §10/&17
  
  enum class ConfirmDeleteFile { IUnderstandThisDeletesTheFile };
  void deleteFile(Session& session, std::string_view name, std::optional<ConfirmDeleteFile> confirmation, CommandCompletion completion); // §10/&29
  ```
  
  **Critical design constraint you must honor** (confirmed straight from the header comments, and backed by the requirements doc): `updateDiskList` (&01), `loadFolder` (&15), `loadFile` (&2A), `loadFileWithDependents` (&2B), `saveMemoryItem` (&2C) and `saveAllMemoryItems` (&2D) are all annotated **"SHALL NOT be sent to the real sampler except through the guard of RQ-AKM-070"** — this is a *different, additional* guard from `--slow-operation`'s existing behaviour (which only ever sent `&01` once, hardcoded). The destructive ones (`ejectDiskDiscardingVirtualSamples`, `deleteSubFolder`, `deleteFile`) require an explicit `std::optional<ConfirmX>` confirmation enum value (RQ-AKM-069) — `std::nullopt` refuses before sending, mirroring the existing `ConfirmDeleteSubFolder`/program-delete pattern.
  
  ## 5. The task this maps to, and why the pieces above are shaped this way
  
  This is literally **TASK-AKM-067** in `process/3.plan/PLAN-AKM-007-disk-tools-primitives.md:371-387`:
  > "Real-sampler harness — disposable test folder and slow-operation guard. Add a `--disk-tools` (name to be confirmed against the probe's existing flag style) check to `xs56k_akm_probe --suite` that creates its own disposable sub-folder, exercises the safe §10 primitives inside it, deletes it through TASK-AKM-066's guard when done, never touches anything that existed before, and gates `&01`/`&15`/`&2A`/`&2B`/`&2C`/`&2D` behind a further, separate opt-in flag that documents the observed hang risk in its own help text."
  > Requirement refs: RQ-AKM-070, RQ-AKM-071. ADR refs: ADR-AKM-001 (DEC-AKM-008 precedent for `--slow-operation`). Status: Not Started.
  
  Relevant requirements, `process/1.requirements/FTR-AKM-007-disk-tools.md`:
  - **RQ-AKM-070** (lines 252-276): "Slow-operation handling for long-running §10 commands" — Still Alive/timeout handling needs no special-casing in the AKM layer itself (session already does this generically); but *invoking* any of the six items against the **real** sampler must be refused unless the caller passes an **explicit, non-default guard argument**, whose own documentation must cite the observed hang (`process/2.architecture/OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`, frames F4–F7: `&01` answered OK then nothing — no DONE, no ERROR, no F0 F7 — and the sampler stopped answering any SysEx until power-cycled by hand). Acceptance: without the guard, nothing is sent and the refusal names the primitive + points at the observation; with the guard, if it hangs, the harness reports the timeout and explicitly does NOT claim the known state was restored (mirrors RQ-AKM-018's honesty rule).
  - **RQ-AKM-071** (lines 278-298): "Real-sampler tests use a dedicated, disposable test folder and file" — the suite's disk check must create its own sub-folder under the already-open folder before changing anything else, do every create/rename/load/save inside only that sub-folder, delete it via the `&17` guard when the suite ends successfully, never select a different disk, never touch anything pre-existing, and never exercise the six slow items unless the owner passes the RQ-AKM-070 guard **in addition to** the disk-tools flag itself.
  - Background context at lines 38-47: the known real-hardware hang is why §10 items are implemented but gated.
  
  Practical implication for your design: you likely want **two new flags**, not one — e.g. `--disk-tools` (safe: create/open/list/rename/eject-no-discard sub-folder and file operations, always deletable, no real risk) and a second, separate opt-in flag such as `--disk-tools-slow` or extending `--slow-operation`'s semantics (the plan text says "a further, separate opt-in flag", so probably a *new* name, not reusing `--slow-operation` as is, since that one is hardcoded to send exactly one `&01` already and is documented as doing so). Whatever name you pick, mirror `main.cpp`'s validation style (`--disk-tools-slow needs --disk-tools`) and the help-text citation style RQ-AKM-070 mandates (the flag's own `--help` text should name the observed-hang document, the same way `--slow-operation`'s help text already briefly describes what it does at `main.cpp:86-88`).
  
  ## Supporting plumbing referenced above (for completeness)
  
  - `awaitCompletion<Result>(driver, patience, launch)` / `Timed<Result>{result, latency}` — `juce/tests/support/src/ScenarioSupport.hpp:147-168`. This is the core "launch an async primitive, block the scenario thread until its completion fires or patience expires" helper every check and guard uses.
  - `Rig` struct (shared check context: log, driver, input/output ports, diagnostics, options, result) and its `commandPatience()`/`openPatience()`/`closePatience()`/`timing()` helpers — `RealSamplerSuite.cpp:118-147`.
  - `CheckFailure`/`NoSamplerFailure`/`CheckSkipped` exception types — `RealSamplerSuite.cpp:96-115`.
  - Test-file constant `AUTOMATIC_CHECKS = 7` and `SECTION_DISK_TOOLS = 0x10` — `juce/tests/akm/RealSamplerSuiteTests.cpp:64,71` — used by existing `--slow-operation` tests (lines 503-554) as a template for writing your own `--disk-tools` unit tests (e.g. `REQUIRE(result.checks.size() == AUTOMATIC_CHECKS + N)`), including the existing pattern of forcing an item to ERROR via `SamplerBehaviour{.itemErrors = {{SECTION_DISK_TOOLS, ITEM_UPDATE_DISK_LIST, ERROR_NOT_SUPPORTED}}}` to exercise the refusal path deterministically on the simulated sampler.
  - `knownStateText()`/`writeHeader()` already enumerate what settings the suite claims to restore; RQ-AKM-071 explicitly requires that a disk-tools check never claims the known state is restored if its folder-delete step is unconfirmed/failed — so plan to flip `result.knownStateRestored = false` (the same field `GuardedSession::close()` flips at `RealSamplerSuite.cpp:264`) if your folder-delete guard can't confirm cleanup, exactly the way RQ-AKM-070's acceptance criterion demands for the timeout case.