# Code review of `juce/akm` and `juce/mcp` (C++), 2026-10-11

- **Requested by**: the owner, 2026-10-11: a detailed code review of the repository, aimed at `juce/akm` and `juce/mcp`, by a senior C++/JUCE engineer, with instructions for the priority remediations; the Python code and the test code are left out.
- **Reviewer**: Claude Sonnet 5.5, the main session. No second model reviewed this report, and the reviewer is the assistant that wrote much of the code (see `git log`): a blind spot of the author may be shared by the reviewer. Everything below is a proposal; nothing in it is a decision (decisions are the owner's and go in ADRs).
- **Revision reviewed**: branch `feature/MCP`, commit `c4bcfa0`. The code in scope is that of `09b5e6e`; `c4bcfa0` changed documents only.
- **In scope** (C++ only): `juce/akm/include`, `juce/akm/src` (32 `.cpp`, 38 headers, 9,193 lines, not counting the generated `ItemTable.generated.hpp` of 2,870 lines), `juce/mcp/include`, `juce/mcp/src`, `juce/mcp/server` (12 `.cpp`, 8 headers, 11,571 lines, of which about 1,200 are the parameter and effects tables).
- **Read for context only**: `juce/midi` (the port contract, the JUCE backend, the mock backend), JUCE 8.0.15 `juce_MidiDevices.cpp` as fetched in `juce/build/_deps`, and `juce/tests/CheckNoDestructiveCalls.cmake`, which is a safety mechanism of the server.
- **Not reviewed**: Python, `juce/tests/**`, `juce/framework`, `juce/app`, `juce/akm/data/items.json`, `ItemTable.generated.hpp` (generated), and the numbers in the tables of `StandardParameters.cpp` (its builder functions, lines 18-290, were read; its rows, lines 291-941, were not read line by line) and of `FxCatalogue.cpp` (read, but its ranges are not checked against Tables 24 and 25 of the specification).
- **Method**: every hand-written source file in scope was read, except the rows of `StandardParameters.cpp` and a few small headers (Appendix B). Three suspicions were then **run** on the simulated sampler (`xs56k_mcp_server_simulated --trace-messages`, Appendix A), and the behaviour of JUCE's `MidiInput::stop()` and nlohmann's integer conversion was read in their sources as fetched by CMake. No static analyser or sanitizer was run (none is installed on this machine), the code was not rebuilt for the review (the simulated server is the existing Debug build), and nothing was sent to the owner's sampler.
- **How to read a finding**: *Severity* is High (acts on the wrong object, or undefined behaviour or a crash on a path that can occur), Medium (a gap in robustness or safety, a rare failure, a debt that grows with each feature) or Low (hygiene). *Confidence* is **Reproduced** (run), **Read** (follows from the code as read) or **Plausible** (needs a test to be sure). *Effort* is S (under half a day), M (half a day to two days), L (more), for one engineer who knows the code; these are rough. *Priority* is P0 (before any new feature), P1 (next), P2 (planned refactoring), P3 (housekeeping).

## 1. Summary

**Verdict: the architecture is sound and the safety thinking is better than usual for a project of this age; three small, local defects should be fixed before the next feature, a short list of robustness gaps comes next, and a refactoring programme is needed before the tool count grows again.** No finding shows data destroyed by a request that a client would normally make: the destructive tools are gated by `confirm`, and the launch options that open the risky tools are off by default. The three P0 defects can make a tool act on the wrong object when a client sends a huge number (CR-01, which the schemas allow only because most of them declare no maximum) or an ambiguous name (CR-02), or can crash it when the sampler answers inconsistently (CR-03); two of the three were reproduced.

| ID | Priority | Severity | Confidence | Effort | Finding |
|---|---|---|---|---|---|
| CR-01 | P0 | High | Reproduced | S | Numeric tool arguments are narrowed to `int` before they are range-checked: `delete_keygroup` with 4294967297 deletes keygroup 1 |
| CR-02 | P0 | Medium | Reproduced | M | Item names are matched by a label normaliser and the first match wins: "AB" selects "A B", and "Strings Pole" is refused after "Strings Poles" |
| CR-03 | P0 | High | Read | S | An index the sampler returns is used to subscript a vector without a bound check (five sites) |
| CR-04 | P1 | Medium | Read | M | The session's lifetime rests on a raw `this` and on a stop semantic that the port contract does not state and the mock does not have |
| CR-05 | P1 | Medium | Read | S | An exception that leaves a task, or a completion called twice, ends the process |
| CR-06 | P1 | Medium | Read | S | The diagnostic log writes to `std::cerr` on the session thread and can block it |
| CR-07 | P1 | Medium | Read | M | No safety net when the process dies with a key held: no limit on a held key, no handling of signals |
| CR-08 | P1 | Medium | Read | M-L | The launch options are enforced only where tools are registered, and the extra layer is a text search |
| CR-09 | P1 | Medium | Read | M | No sanitizer, no static analysis, no fuzzing, a short list of warnings |
| CR-10 | P2 | Medium | Read | M | The AKM primitives repeat the same reply-decoding code 55 times |
| CR-11 | P2 | Medium | Read | L | The gateway: 82 public methods, 121 hand-written waits, an implicit "connected" precondition, no error category |
| CR-12 | P2 | Medium | Read | L | `Tools.cpp`: 3,735 lines, registering functions of up to 490 lines, three copies of the same argument code |
| CR-13 | P2 | Medium | Read | M | One command per file, per list item and per keygroup: every disk call pays for file sizes it does not show |
| CR-14 | P2 | Low | Read | M | The protocol loop is single-threaded, cannot cancel, and reads a line of any length |
| CR-15 | P3 | Low | Read | S | Comments and README sentences that the code has outgrown |
| CR-16 | P3 | Low | Read | S | A list of small defects, each with its line |
| CR-17 | P3 | Low | Read | S | Test doubles in the production library, unnamed threads, ambiguous overloads, unpinned JUCE |
| CR-18 | P3 | Low | Read | S | Strings read from the sampler are echoed to the model as they are |

## 2. What is done well (keep it that way)

- **Layering.** `xs56k_midi` ← `xs56k_akm` ← `xs56k_mcp` ← `server/main.cpp`. A search finds no `juce::` in `juce/akm` or `juce/mcp`; only `server/main.cpp` reaches JUCE, through `JuceMidiBackend`.
- **Time and threads are injected.** `Executor` and `Scheduler` are interfaces with a thread implementation (`ThreadExecutor`, `RealScheduler`) and a manual one (`ManualExecutor`, `ManualScheduler`), so the timeouts, the discovery window and the Still Alive restart can be tested in no time.
- **The session state machine** (`Session.cpp`): one command in flight; a generation number so that a stale timer cannot complete the next command (`Session.cpp:576-581`); a late ERROR after a REPLY is attributed to the right command (`COMPLETED_COMMANDS_REMEMBERED`); a sequence stops at its first failure; the close restores the keys first, then the §00 settings with the checksum mode first, and always finishes; `close()` refuses to run from the session thread, which prevents the deadlock (`Session.cpp:1042-1048`).
- **The codec is total.** `decodeMessage` returns `Rejected` with a reason for every malformed input, and every index it uses is derived from a checked size (`Confirmation.cpp:127-177`); `ByteReader` reads are all-or-nothing; `ByteWriter` fails and stays failed. The 7-bit rule is enforced three times (`ByteWriter::appendString`, `makeRequest`, `encodeCommand`): **a name given by a client cannot carry `F7` or any status byte into a frame, so SysEx injection through a tool argument is not possible.**
- **The item catalogue is data**, generated and checked, and the ranges of the arguments are enforced in one function (`ItemRequest.cpp:92-118`).
- **Destructive AKM primitives need a typed token**: a one-enumerator `enum class` in a `std::optional` with no default (`ConfirmDeleteFile::IUnderstandThisDeletesTheFile`, `ConfirmClearSamplerMemory`, and the others), refused as `NotConfirmed` otherwise.
- **The MCP layer checks its arguments by hand**: `additionalProperties: false` in every schema and `unknownArguments` in every handler; names are resolved against the sampler's own listing before anything is sent; every mutation is read back and compared (`SamplerGateway.cpp:1129-1140`, `1341-1343`, `1881-1885`); the clock text is parsed exactly (`Tools.cpp:2439-2482`, every `std::stoi` is on a validated digit string).
- **Safe defaults at launch**: disk tools, the disk refresh and the front-panel keys are off unless asked for. `tests/CheckNoDestructiveCalls.cmake` is a fitness function that says which unit may call which primitive.
- **Standard output carries nothing but the protocol** (`main.cpp:19-24`), and the Windows streams are put in binary mode (`main.cpp:57-63`).
- **Hygiene.** No raw `new`, `delete`, `malloc`, `reinterpret_cast`, `const_cast` or `detach()` in scope (searched); `[[nodiscard]]` 69 times in the AKM headers; `constexpr` tables; the threads are the last data member (`ThreadExecutor.hpp:55-56`), so they start on a fully built object; warnings are errors.
- **What the real sampler taught is written down** in the code comments and in `OBSERVATIONS-*.md`, with the facts the code depends on.

## 3. Findings

### P0: fix before any new feature

#### CR-01: Numeric tool arguments are narrowed to `int` before they are range-checked

- **Severity**: High (a destructive tool acts on the wrong object). **Likelihood**: low (a client must send a number of 2^31 or more). **Confidence**: Reproduced. **Effort**: S.
- **Where**: `Tools.cpp:117-128` (`wholeNumber`), and the narrowings `static_cast<int>(*x)` at `Tools.cpp:152` (the `keygroup` of `get_parameters` and `set_parameter`), `1924` (`set_zone_sample`, keygroup), `1952` (`get_zone_samples`), `2006-2012` (`delete_keygroup`), `2227` and `2229` (`set_part_program`, position and part), `2256` (`clear_part`). Each of them checks a lower bound only (`delete_keygroup` checks only that the value is a whole number), then converts to `int`; the gateway checks the upper bound on the converted number.
- **What happens**: `wholeNumber` returns an `int64_t`. `static_cast<int>(4294967297)` is 1 (modulo 2^32). The gateway sees keygroup 1, which exists, and sends the command. Appendix A1: on a program of three keygroups, `delete_keygroup` with `keygroup: 4294967297` and the right `confirm` answers `Deleted keygroup 4294967297 ... it now has 2 keygroups`, and the trace shows item `0C` (Delete Keygroup) sent. `99999999999999` becomes 276447231 and is refused, so the defect depends on the value.
- **A second hole in the same function** (Reproduced, Appendix A3): `wholeNumber` takes `value.get<std::int64_t>()` for any JSON integer, including an unsigned one above `INT64_MAX`, which nlohmann converts with a `static_cast` that wraps (`nlohmann/detail/conversions/from_json.hpp:76-80`). `set_parameter` of `program fine tune` with 18446744073709551606 answers `program fine tune = -10 (program), as read back from the sampler's memory`: the number became -10, a valid value for every parameter whose range is negative.
- **Remediation**:
  1. In `wholeNumber`, test `value.is_number_unsigned()` first and return nothing when it exceeds `INT64_MAX`:
     ```cpp
     if (value.is_number_unsigned())
     {
         const auto big = value.get<std::uint64_t>();
         if (big > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
             return std::nullopt;
         return static_cast<std::int64_t>(big);
     }
     ```
  2. Add the one function that may turn a JSON number into an `int`, and use it for every integer argument:
     ```cpp
     /// `value` as an int when it is a whole number inside [min, max] (both inside int's range), else nothing.
     std::optional<int> wholeNumberIn(const json& value, std::int64_t min, std::int64_t max);
     ```
  3. Give every integer argument a named upper bound (`MAX_KEYGROUP = 99`, `MAX_PART = 128`, `MAX_PROGRAM_POSITION = 16383`), use `wholeNumberIn` at the seven sites above, add the missing check at `2006-2012`, and put the same `maximum` in each JSON schema (today the schemas of `delete_keygroup`, `set_zone_sample`, `get_zone_samples`, `set_part_program` and `clear_part` have a `minimum` only).
  4. Make the pattern impossible to write again: a check (CMake text search as for `CheckNoDestructiveCalls.cmake`, or clang-tidy `bugprone-narrowing-conversions`) that fails when `static_cast<int>(*` appears in `Tools.cpp`.
- **Verify**: a test generated from `tools/list`: for every tool and every property of type integer, call with `4294967297`, `9223372036854775808`, `-4294967295` and `1e300`, and require `isError: true` and an empty `--trace-messages` entry for that call. Done when Appendix A1 answers `The argument 'keygroup' must be a whole number from 1 to 99` and sends nothing.

#### CR-02: Item names are matched by a label normaliser, and the first match wins

- **Severity**: Medium (a non-destructive tool acts on the wrong item; a legitimate name is refused). **Confidence**: Reproduced. **Effort**: M.
- **Where**: `normalizeText` (`ParameterCatalogue.cpp:191-231`) was written for parameter names and choice labels: it lower-cases, drops spaces, `_` and `-`, and also rewrites the words `plus` and `minus` to `+` and `-` and `poles` to `pole`. It is now the identity of **item names** at 37 sites of `SamplerGateway*.cpp` (10 in `SamplerGateway.cpp`, 22 in `SamplerGatewayDisk.cpp`, 4 in `SamplerGatewayLists.cpp`, 1 in `SamplerGatewayKeys.cpp`) and 6 of `Tools.cpp`. Most lookups take the first item that matches: `fileNamed` and `folderNamed` (`SamplerGatewayDisk.cpp:556-576`), `assignZoneSample` (`SamplerGateway.cpp:941-943`), `assignPartProgram` (`1659-1661`), `selectDiskByName` (`SamplerGatewayDisk.cpp:147-149`), `openFolder` (`268-270`), `loadFile` and `loadFolder` (`353-355`, `397-399`), `saveMemoryItem` (`885-886`).
- **What happens** (Appendix A2): with the programs `A B` and `AB` in memory, `set_part_program` with `program: "AB"` assigns **`A B`** (the list is sorted, `A B` comes first) and the answer says so; `part 1` and `part 2` end up on the same program. After `create_multi "Strings Poles"`, `create_multi "Strings Pole"` is refused as "already holds a multi named Strings Poles". For the destructive file tools the exact `confirm` limits the harm, but by reading `SamplerGatewayDisk.cpp:714-730` it also makes `delete_file` with `name` and `confirm` both `AB.wav` refuse when `A B.wav` is listed first (not run: the disk tools need `--allow-disk`).
- **What is decided and what is not**: `PLAN-MCP-004` (assumptions of the sample and multi tasks) decided on purpose that a new name is refused when another item bears it "without regard to case, spaces or hyphens (stricter than the sampler)", and `FTR-MCP-001` decided a tolerant lookup of **parameter** names. Neither decided that the words `plus`, `minus` and `poles` change a name, or that an ambiguous match picks the first item. The set lists already do the right thing: `positionsNamed` (`SamplerGatewayLists.cpp:114-127`) prefers the exact name, then the similar ones, and `renameSetList` and `deleteSetList` refuse several matches (`342-344`, `403-405`).
- **Remediation**:
  1. Add `mcp/NameMatching.hpp` with one function that every lookup uses, built from `positionsNamed`:
     ```cpp
     enum class MatchKind { Exact, Unique, Ambiguous, None };
     struct NameMatch { MatchKind kind = MatchKind::None; std::vector<std::size_t> positions; };
     /// Exact (byte for byte) wins; else a single item that equals `wanted` ignoring case, spaces, hyphens and underscores; else Ambiguous with every candidate.
     NameMatch matchName(const std::vector<std::string>& names, std::string_view wanted);
     ```
  2. Split the normaliser: keep `normalizeText` for parameter names and labels; add `normalizeName` for item names (case, spaces, hyphens, underscores, and nothing else).
  3. Replace the 37 and 6 uses. On `Ambiguous`, answer with the candidates and send nothing: `Several programs match "AB": "A B" (position 0), "AB" (position 1). Give the exact name.`
  4. Leave the "refused when another item bears it, ignoring case, spaces or hyphens" rule as the owner decided it, now through `normalizeName`; whether to keep it stricter than the sampler stays his decision (see section 4).
- **Verify**: Appendix A2 as two regression tests (exact name wins; `Strings Pole` is accepted after `Strings Poles`), and one test per lookup family (`set_part_program`, `set_zone_sample`, `select_disk`, `open_folder`, `load_file`, `save_memory_item`) with two names that differ only by a space: the answer lists both, nothing is sent.

#### CR-03: An index the sampler returns is used to subscript a vector without a bound check

- **Severity**: High (undefined behaviour or a crash on a path that can occur). **Likelihood**: low, but the cause is an ordinary race. **Confidence**: Read (not reproduced: the simulated sampler cannot answer inconsistently). **Effort**: S.
- **Where**: `listSamples` reads the names (`getAllSampleNames`) and then, in a second command, the current index (`getCurrentSampleIndex`), and stores the index as `listing.current` without comparing it with the number of names (`SamplerGateway.cpp:1195-1213`). Four callers then write `samples[static_cast<std::size_t>(*current)]`: `startSampleAudition` (`562`), `renameCurrentSample` (`1231`, `1253`), `deleteCurrentSample` (`1263`). `listNamedItems` does the same for song files and scenelists (`SamplerGatewayLists.cpp:241-247`) and `renameCurrentNamedItem` subscripts with it (`328`).
- **Why it can happen**: the two reads are two round trips of about 12 ms each (`OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`), and the owner works at the sampler's front panel while the server runs: a sample deleted or loaded between the two reads makes the index point past the list. A sampler that answers a stale index does the same. In Debug on MSVC the subscript asserts; in Release it reads past the end.
- **Remediation**:
  1. In `listSamples` and `listNamedItems`, accept `current` only when it is below the number of entries; otherwise answer `The sampler reports sample 5 as current but lists 4 samples: its memory changed while it was read. Ask again.` (or read the pair once more).
  2. Give `SampleListing` and `NamedListing` a `const Entry* currentEntry() const` that returns null when `current` is empty or out of range, and replace the five direct subscripts, so that the unsafe form cannot be written again.
- **Verify**: a fault switch in the simulated sampler (test code) that answers a current index above its list; `audition_sample`, `rename_sample`, `delete_sample` and `rename_song_file` must answer `isError: true`, not crash, and delete nothing.

### P1: robustness and safety nets

#### CR-04: The session's lifetime rests on a raw `this` and on a stop semantic that nothing states

- **Severity**: Medium. **Confidence**: Read. **Effort**: M.
- **Where**: `Session.cpp:217-232`. `scheduleSessionTask` hands the scheduler a lambda that captures `this` and a shared flag `alive`; on the scheduler's thread it tests the flag and then calls `postToSession`, which reads `this->executor`. `~Session` clears the flag and destroys the object (`964-980`). Between the test and the use there is no protection: if the destructor runs on another thread in that window, `postToSession` uses a destroyed object. The window is a few instructions wide and needs a timer to fire during the destruction, so it is rare; the consequence is undefined behaviour. The input callback has the same shape (`955-958`: it captures the raw `impl` and calls `impl->postToSession`).
- **What makes the input side safe today, and what does not**: JUCE 8.0.15's `MidiInput::stop()` takes a spin lock that `consume()` also holds for the whole of its callbacks (`juce_MidiDevices.cpp:76-80` and `158-182`), so `input.stop()` in the destructor returns only when no callback is running. That is a property of this JUCE version, not of the port contract: `MidiPorts.hpp:51-53` says only "Start/stop message delivery without closing the device", and the **mock backend's `stop()` is `started = false`** (`MockMidiBackend.cpp:155-157`, with a delivery that deliberately calls out without a lock, `225-245`). A test on the mock therefore cannot detect this class of bug, and another backend would not have the guarantee.
- **Remediation**:
  1. Keep `this` out of what runs on the scheduler's thread:
     ```cpp
     TimerHandle scheduleSessionTask(Scheduler::Clock::duration delay, Task work)
     {
         return scheduler.scheduleAfter(delay, [flag = alive, target = &executor, work = std::move(work)]() mutable {
             if (!flag->load())
                 return;
             target->post([flag, work = std::move(work)] {
                 if (flag->load())
                     work();
             });
         });
     }
     ```
     This needs the executor to outlive the scheduler's pending tasks, which `Connection` in `SamplerGateway.cpp:164-179` already guarantees (the executor is declared first); write that rule in `Session.hpp`.
  2. In `~Session`, after the close wait, clear `alive`, then, when the executor runs on its own thread, post a barrier task and wait for it with a bound. The barrier runs after every task posted before it, so no session task is running when the destructor goes on, and every later task sees the flag cleared.
  3. State the contract in `MidiPorts.hpp`: `stop()` returns only when no callback of this port is running and none will start; it must not be called from a callback. Make `MockMidiBackend`'s `stop()` honour it (a counter of running deliveries and a wait), so that the existing session tests exercise it.
- **Verify**: a test that destroys a `Session` while a thread keeps delivering frames through the mock and a short timer keeps firing, run under ThreadSanitizer and AddressSanitizer (CR-09).

#### CR-05: An exception that leaves a task, or a completion called twice, ends the process

- **Severity**: Medium. **Confidence**: Read. **Effort**: S.
- **Where**: `ThreadExecutor::run` (`ThreadExecutor.cpp:58-75`) and `RealScheduler::run` (`RealScheduler.cpp:59-85`) call the task with no `try`; `Task.hpp:24-26` says "It must not throw" and nothing enforces it. No `catch` exists anywhere in `juce/akm`. The tasks run user code: the completions of `submit`, `open` and `close`, and the observer of a collecting command. The gateway's completion is `promise->set_value(result)` (`GatewayDetail.hpp:48-57`), which throws `std::future_error` if it is called a second time, and copies `result` (which can throw `std::bad_alloc`).
- **Consequence**: the process ends at once; the session is not closed, so the §00 settings the server changed stay changed and a held key stays held (CR-07), and the client sees a dead server.
- **Remediation**:
  1. In both loops, `try { task(); } catch (...) { ... }`, and report through a handler given at construction (`std::function<void(std::exception_ptr)>`; the default asserts in Debug and ignores in Release). Swallowing is deliberate here: the alternative is to lose the sampler's state.
  2. In `Session::Impl`, call every piece of user code (`unit->completion`, `opening->completion`, `closingContext->completion`, `options.onConfirmation`) through one guarded helper that catches and reports a new `DiagnosticKind::CompletionThrew`.
  3. Make `detail::await` idempotent:
     ```cpp
     struct Slot { std::promise<Result> promise; std::once_flag once; };
     auto slot = std::make_shared<Slot>();
     start([slot](const Result& r) { std::call_once(slot->once, [&] { slot->promise.set_value(r); }); });
     ```
- **Verify**: a completion that throws and one that is called twice must not end the test process, and the session must answer the next command.

#### CR-06: The diagnostic log can block the session thread

- **Severity**: Medium. **Confidence**: Read. **Effort**: S.
- **Where**: `DiagnosticSink.hpp:60` says `report` "must neither throw nor block", and `GatewayConfig::diagnosticLog` repeats it (`SamplerGateway.hpp:68-72`); `main.cpp:113` passes `std::cerr << line`. A write to a pipe blocks when the pipe is full, and an MCP client that does not drain the server's standard error fills it. The write happens on the session thread, which then stops answering: every later command times out, and the cause is invisible.
- **Remediation**: a small bounded log (`push` never blocks, a writer thread owns the `std::cerr` write, the oldest line is dropped when the queue is full and the drops are counted and reported once as `N diagnostic lines dropped`), used by `main.cpp` as the `diagnosticLog`.
- **Verify**: a log function that blocks for ever, a session that completes ten commands anyway.

#### CR-07: No safety net when the process dies with a key held

- **Severity**: Medium (only with `--allow-front-panel`, but the effect is on a real instrument). **Confidence**: Read, and the repeating key is observed. **Effort**: M.
- **Where**: a held key repeats on the real S5000 until it is released: "the cursor ran on to the right end of the name" after `hold_key cursor_right` for 3.8 s (`OBSERVATIONS-RQ-MCP-012-real-sampler.md:563` and `574`). The only releases are `release_key` and the orderly close (`Session.cpp:796-819`, `SamplerGatewayKeys.cpp:153-168`, `Tools.cpp:3634-3640`). The repeat is observed for a cursor key; a key such as `ent_play` held on a delete screen could repeat a confirmation (not tried). `main.cpp:86-148` installs no signal or console handler, and the close runs only when standard input reaches its end (`main.cpp:134-141`); a crash, `SIGKILL`, a task that throws (CR-05) or a console that is closed leaves the key down. `hold_key` has no duration limit.
- **Remediation**:
  1. A dead-man for held keys, in the session: `SessionTiming::keyHoldLimit` (default 10 s). When a command with `holdsKey` is sent, schedule `onKeyHoldExpired(deviceId, keycode)`; if the key is still in `keysHeld`, it submits an internal Release and reports a diagnostic. `hold_key` says in its answer that the key is released by itself after N seconds, and takes an optional `max_seconds` up to a cap.
  2. Orderly shutdown on signals: in `main.cpp`, `SetConsoleCtrlHandler` (Ctrl-C, Ctrl-Break, close, log-off, shutdown) on Windows and `sigaction` for `SIGINT`, `SIGTERM` and `SIGHUP` elsewhere. The handler only sets an atomic flag and wakes a shutdown thread; that thread closes the session with a deadline and then calls `std::_Exit`. `Session::close` may be called from any thread but the session's own, but `SamplerGateway` keeps `_connection` in a `unique_ptr` that the main thread resets, so the gateway needs a mutex-guarded `std::shared_ptr<Connection>` and a `requestClose()`.
  3. Say in the README that `SIGKILL` and a crash cannot be handled and that the dead-man limits the damage.
- **Verify**: the simulated sampler records the Release when the held key's limit expires, and when the shutdown path runs with a key held.

#### CR-08: The launch options are enforced only where tools are registered

- **Severity**: Medium (defence in depth: no current path is known to bypass it). **Confidence**: Read. **Effort**: M-L.
- **Where**: `--allow-disk`, `--allow-disk-refresh` and `--allow-front-panel` decide which tools are registered (`Tools.cpp:3723-3732`) and whether `list_disks` accepts `refresh` (`Tools.cpp:1427-1430`). `GatewayConfig` carries none of them (`ServerOptions.cpp:187-197`), so `SamplerGateway::listDisks(bool refresh)` sends the disk refresh whenever it is asked (`SamplerGatewayDisk.cpp:76-91`), which is the command that hung a real S5000. The second layer is a text search for names (`tests/CheckNoDestructiveCalls.cmake:41-42`: `(ItemId::|akm::)[A-Za-z]*(verbs)`); it cannot see a command built by hand (`akm::Command{section, item, ...}` given to `Session::submit`), a name reached through `using namespace`, or an item chosen at run time from the parameter tables.
- **A check-then-act window**: `deleteCurrentProgram` reads the current name, compares it with `confirm`, then sends the delete as a separate command (`SamplerGateway.cpp:486-512`); the same shape is used for the current sample, multi, song file and scenelist. The front panel, or a MIDI program change, can change the selection between the commands.
- **Remediation**:
  1. A command filter at the lowest layer, so that no new gateway code can go round it: `SessionConfig::commandFilter` (`std::function<bool(const Command&)>`), tested in `Session::Impl::enqueue` for application units, refusing with a new `RefusalReason::NotAllowed`. The gateway builds it from `GatewayConfig`, to which `allowDisk`, `allowDiskRefresh` and `allowFrontPanel` are added:
     ```cpp
     bool allowed(const akm::Command& c, const GatewayConfig& g)
     {
         if (c.section == 0x10 && c.item == 0x0D) return false;                          // Eject Disk: never
         if (c.section == 0x10 && c.item == 0x01) return g.allowDisk && g.allowDiskRefresh;  // refresh of the disk list
         if (c.section == 0x10) return g.allowDisk;
         if (c.section == 0x20) return g.allowFrontPanel;
         return true;
     }
     ```
  2. Optional second stage: the items that destroy (`SystemClearMemory`, the delete-all items, the file and folder deletions) are sent only after the gateway has *armed* that item for the next command; the filter consumes the arming. A stray call then fails even if the gateway code is wrong.
  3. Shorten the window of the "delete the current X" family: send `select X by name` and `delete the current X` in one `submitSequence`, after the `confirm` comparison, so that nothing of this session comes between them. Say plainly that the front panel still can; the window goes from three round trips to one.
  4. Keep the CMake search as the outer layer.
- **Verify**: a table test that submits a raw `Command` for every `ItemId` through a session built with each combination of the three options and asserts which are refused.

#### CR-09: No sanitizer, no static analysis, no fuzzing, a short list of warnings

- **Severity**: Medium (it is what would have caught CR-03 and CR-04 without a reviewer). **Confidence**: Read (`.github/workflows` and `juce/CMakeLists.txt:106-118`: no sanitizer, no `.clang-tidy`, no `.clang-format`). **Effort**: M.
- **Remediation**:
  1. Warnings: add `-Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion -Wcast-align -Wformat=2` for GCC and Clang, and `/w14242 /w14254 /w14263 /w14265 /w14287 /w14296 /w14311 /w14545 /w14546 /w14547 /w14549 /w14555 /w14619 /w14640 /w14826 /w14905 /w14906 /w14928` for MSVC, first on `xs56k_akm` and `xs56k_mcp` only, then as the default. Expect noise from the `int` conversions of CR-01 and from `Session.cpp`.
  2. A `.clang-tidy` (`bugprone-*`, `performance-*`, `concurrency-*`, `modernize-use-nodiscard`, `cppcoreguidelines-narrowing-conversions`) run in one CI job on `akm` and `mcp` through `CMAKE_CXX_CLANG_TIDY`; a `.clang-format` taken from the current style.
  3. Two sanitizer variants in the workflow generator `juce/tools/generate_workflows.py` (not in the generated YAML): Clang or GCC Debug with `-fsanitize=address,undefined` running the whole `ctest`, and a second with `-fsanitize=thread` running the `akm` session tests and the `mcp` tests.
  4. libFuzzer targets, built only with `-DBUILD_FUZZERS=ON`: `decodeMessage(frame, mode)` in the three checksum modes, `ByteReader`, and `McpServer::handleLine`; seed them with the frames and the conversations the tests already hold.
  5. Pin JUCE by commit, not by tag (`juce/CMakeLists.txt:67-71`); nlohmann/json is already pinned by hash.
- **Done when**: the sanitizer jobs are green on `main`, and CR-03 and CR-04 have a failing test that the job catches before the fix.

### P2: planned refactoring

#### CR-10: The AKM primitives repeat the same reply-decoding code

- **Severity**: Medium (maintainability). **Confidence**: Read, counted. **Effort**: M.
- **Evidence**: 55 occurrences of `std::get_if<Reply>`, most of them in a lambda of the shape `session.submit(makeRequest(...), [completion](const CommandResult& outcome) { Result result{std::nullopt, outcome}; if (const auto* rep = std::get_if<Reply>(&outcome)) { decode...; assign } if (completion) completion(result); })` (17 in `DiskPrimitives.cpp`, 10 in `MultiPrimitives.cpp`, 8 in `ProgramPrimitives.cpp`, 5 in `SamplePrimitives.cpp`, 15 in eight other files); 22 copies of `msb = index / 128`, `lsb = index % 128`; `DATA_BYTE_BASE` defined in seven files; `NO_VALUES` in eleven; `answersEmptyMemory` twice (`MultiPrimitives.cpp:51`, `ProgramPrimitives.cpp:50`); the helper set `decodeWordReply`, `decodeNameReply`, `nameReplyOptions` twice, identical (`SongPrimitives.cpp:39-63`, `SceneListPrimitives.cpp:38-62`). The better shape already exists in three files: `getSingleValue` (`SystemSetup.cpp:50-63`), the helpers of `SongPrimitives.cpp` and of `MultiFxPrimitives.cpp:42-98`.
- **Remediation**:
  1. `Protocol.hpp`: `inline constexpr std::int64_t DATA_BYTE_BASE = 128;` and `struct TwoBytes { std::int64_t msb, lsb; }; constexpr TwoBytes splitTwoBytes(int value);` with its inverse over a decoded reply.
  2. An internal header `src/ReplyHelpers.hpp`: `answersEmptyMemory`, `NO_VALUES`, and
     ```cpp
     template <typename Result, typename Decode>
     void submitGet(Session& session, CommandRequest request, std::function<void(const Result&)> completion, Decode decode);
     // builds Result{}, stores the outcome, calls decode(result, replyBytes) only for a Reply, then calls the completion
     ```
  3. Migrate one file per commit, largest first (`DiskPrimitives.cpp`, `MultiPrimitives.cpp`, `ProgramPrimitives.cpp`, `SamplePrimitives.cpp`), each commit leaving `ctest` unchanged.
  4. Optional, an API break the owner must accept: the roughly forty `XxxResult` structs (`count`, `name`, `handle`, ...) could become one `template <class T> struct Outcome { std::optional<T> value; CommandResult outcome; }`; the field names are public today.
- **Done when**: no `std::get_if<Reply>` outside the helper and no per-file `DATA_BYTE_BASE`.

#### CR-11: The gateway: size, waits, preconditions, error model

- **Severity**: Medium (maintainability, and one latent crash). **Confidence**: Read. **Effort**: L.
- **Evidence**:
  1. `SamplerGateway` has 82 public methods in a 985-line header and five translation units (3,756 lines); the file `SamplerGateway.cpp` is 1,900 lines.
  2. 121 hand-written `await<...>(waitFor(1), [&](std::function<void(const X&)> done) { akm::fn(session, std::move(done)); })` followed by the same two checks. The sentence `The sampler session did not complete the command in time.` is written out 74 times, and is also defined as a constant in four files (`SamplerGateway.cpp:654`, `SamplerGatewayFx.cpp:37`, `SamplerGatewayKeys.cpp:41`, `SamplerGatewayLists.cpp:42`) that is used 36 times.
  3. **A precondition no type states.** Helpers such as `currentProgramName` and `keygroupCount` use `_connection->session` and rely on a caller having connected. The public `readFxModule` (`SamplerGatewayFx.cpp:81`) and `readFxParameter` (`133`) never call `connect()`: called first in a session, they dereference a null `_connection`. The current tools cannot do that (`readFxLayout` runs before them, `Tools.cpp:3423-3434`), so it is latent.
  4. `Outcome<T>` carries a sentence and no category. A caller cannot tell a timeout from a refusal from a sampler error, so it cannot retry, count or log by kind; `explain` takes a `bool needsCurrentProgram` and a noun as a string (`GatewayDetail.hpp:61-86`).
- **Remediation**:
  1. One helper for the wait: `template <class R, class Start> Outcome<R> SamplerGateway::ask(int commands, Start&& start)` that returns `failure(SESSION_TIMED_OUT)` itself, plus `explainedValue(result, "counting the programs")` for the "value missing, say why" step. Convert the 121 sites file by file, `SamplerGatewayDisk.cpp` and `SamplerGateway.cpp` first.
  2. `Connection& connection()` that connects or reports, and make the helpers take it (`currentProgramName(Connection&)`), so a missing connection cannot compile into a dereference; until then, call `connect()` at the top of `readFxModule` and `readFxParameter`.
  3. `enum class FailureKind { NotConnected, TimedOut, Refused, SamplerError, Unreadable, Inconsistent, Unconfirmed }` in `Outcome`; keep the sentence.
  4. Later, split the facade by domain (`ProgramGateway`, `SampleGateway`, `MultiGateway`, `DiskGateway`, `ListGateway`, `FxGateway`, `PanelGateway`) over one `SessionHost`.
- **Done when**: the literal `did not complete the command in time` appears once.

#### CR-12: `Tools.cpp` is one 3,735-line unit with registering functions of up to 490 lines

- **Severity**: Medium (maintainability, build time). **Confidence**: Read. **Effort**: L.
- **Evidence**: 77 tools registered by 66 `tools.push_back(Tool{` statements, some inside a loop or a helper lambda (the named lists, the delete-all tools, `rename_file` and `rename_folder`, the three key tools); `makeDiskTools` is lines 1395-1887 (492 lines), `makeMemoryExtraTools` 1891-2325 (435), `makeFxTools` 3292-3598 (306). The parameter-selection code ("a group or a list of names, resolved, or a problem") is written three times (`get_parameters` `601-667`, `get_sample_parameters` `887-939`, `get_multi_parameters` `1111-1165`); the pair "name or index, exactly one" five times (`select_program`, `select_sample`, `select_multi`, `select_disk`, `select_*` of the lists). Every handler repeats `contains`, `at`, `is_string`, `get<std::string>`. The calendar arithmetic (`isLeapYear`, `daysInMonth`, `dayOfWeekOf`, `Tools.cpp:2373-2392`) lives in the presentation unit.
- **Remediation**:
  1. An argument reader, so that a handler states what it takes and gets either values or one problem sentence:
     ```cpp
     Arguments args(arguments, {"name", "index"});            // refuses an unknown key
     const auto name  = args.text("name", 1, 20);              // optional<string>
     const auto index = args.integer("index", 0, 16383);       // optional<int>, CR-01's wholeNumberIn inside
     if (const auto problem = args.exactlyOne(name, index)) return failure(*problem);
     ```
  2. One file per family (`ProgramTools.cpp`, `SampleTools.cpp`, `MultiTools.cpp`, `DiskTools.cpp`, `ListTools.cpp`, `FxTools.cpp`, `PanelTools.cpp`, `SettingsTools.cpp`) behind the same `make*Tools` declarations, with `makeAllTools` unchanged.
  3. Table-driven registration for the select tools and the three parameter families, as the named lists already are (`Tools.cpp:2917-3117`).
  4. Move the date logic to `<chrono>` (`std::chrono::year_month_day{...}.ok()`, `std::chrono::weekday`).
- **Do it after CR-01**, since the argument reader is where CR-01's helper lives.

#### CR-13: Commands per file, per list item and per keygroup

- **Severity**: Medium (latency grows with the contents of the sampler). **Confidence**: Read. **Effort**: M.
- **Evidence**: a command round trip is about 12 ms (median of 50 Echo, `OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`).
  1. `listDiskContents` sends `getFileSize` once per file (`SamplerGatewayDisk.cpp:230-241`), and it is called by `loadFile`, `loadFolder`, `startFileAudition`, `readDiskSpace`, `writableFolder` (so by `rename_file`, `rename_folder`, `delete_file`, `delete_folder`, `create_folder`, `save_*`) and `listAfterChange`, which need names and not sizes. A folder of 100 files costs about 1.2 s per call, two or three times per tool.
  2. `deleteFolder` lists the inside of the folder, with its sizes, only to count it (`SamplerGatewayDisk.cpp:769-776`); `getFileCount` and `getFolderCount` exist.
  3. `memoryNames()` reads the six lists, three of them one command per item (`listNamedItems`, `SamplerGatewayLists.cpp:226-249`), twice per load and once per save, although a save needs one kind.
  4. `readZoneSamples` sends a keygroup selection and four reads per keygroup (`SamplerGateway.cpp:899-917`): about 500 commands for a program of 99 keygroups.
- **Remediation**: `listDiskNames()` (names only) for the callers that look a name up, sizes only for `list_disk_contents`; count with `getFileCount` and `getFolderCount`; `namesOfKind(kind)` for a save; send the per-item reads of a list as one `runSequence` (one wait instead of N). Measure before and after on the simulated sampler by counting trace entries per tool call, and put the counts in a test, so that a regression shows.

#### CR-14: The protocol loop is single-threaded, cannot cancel, and reads a line of any length

- **Severity**: Low. **Confidence**: Read. **Effort**: M.
- **Where**: `McpServer::serve` reads with `std::getline` and handles each request to its end (`McpServer.cpp:119-134`). A tool that waits for the disk timeout (up to 1,800,000 ms, `ServerOptions.cpp:51`) leaves `ping` and `notifications/cancelled` unread; a client that gives up and kills the server triggers CR-07. A client line of any size is read into memory.
- **Remediation**: cap the line (for example 4 MiB) and answer `-32600` for a longer one; read standard input on its own thread, serve `ping` while a call runs, and let a `notifications/cancelled` stop the gateway's wait (the late completion is already safe: the promise is shared). Return `structuredContent` next to the text later, if the clients in use read it.

### P3: housekeeping

#### CR-15: Comments and README sentences that the code has outgrown

- `SamplerGateway.hpp:36-42` says the gateway sends "only the creation, the renaming and the deletion of the current program ... never anything that saves, loads, touches the disk or deletes everything (RQ-MCP-014)". It now saves, loads, uses the disk and deletes everything.
- `Tools.hpp:36-40`, `44-46`, `49-52` describe the first tools as the only ones ("none of them creates, renames, deletes or saves anything"): true of each named function, but the file reads as a statement about the server; `makeAllTools` (`Tools.hpp:113-114`) lists four of its eight groups.
- Root `README.md`, paragraph "Safety": "The server never clears the whole memory, formats or ejects a disk" is false since `clear_sampler_memory` exists; "Every deletion needs a `confirm` that is the exact name" is false for the delete-all tools, which ask for a count.
- **Remediation**: rewrite the four passages from the tool list; a `ctest` could search the README for the word `never` in a sentence that names a tool the server offers.

#### CR-16: Small defects, each with its line

1. `SamplerGateway.cpp:296`: `steps[std::min(failed, steps.size() - 1)]` underflows when `steps` is empty (every caller passes at least one).
2. `Tools.cpp:219-224` and `1013-1018`: `values.front()` on a possibly empty vector (not empty today).
3. `ParameterCatalogue.cpp:464` and `332`: a loop and a modulo that assume `step >= 1` (data, tested).
4. `Protocol.hpp:71-74`: `maxUnsignedValue(width)` shifts by `7 * width`; undefined from width 10 (8 is the largest used).
5. `RealScheduler.cpp:53`, `ManualScheduler.cpp:35`: `now + delay` overflows for a delay near `duration::max()` (only finite constants are passed).
6. `Session.cpp:285`: `const CommandRequest request = unit->requests[unit->next];` copies the request, its data and its `std::function`; a `const&` is enough.
7. `Tools.cpp:2928`, `2957`, `3066`, `3145-3147`: `(...).c_str()` of a temporary given to a `const char*` parameter that `definition` copies at once; safe today, fragile; take a `std::string`.
8. `FrontPanel.cpp:85-95`: `pressKey` sends the Hold and the Release as two `submit` calls; `submitSequence` would keep them adjacent and stop after a failed Hold.
9. `GatewayDetail.hpp:61-86`: `explain(outcome, doing, config, bool needsCurrentProgram, const std::string& currentObject)`: a flag and a noun as a string; an `enum class Subject` would do.
10. `Session.hpp:150-151`: `close(CloseCompletion)` and `close(Task)` make `close({})` and `close(nullptr)` ambiguous.
11. `ParameterCatalogue.cpp:296-316`: `resolveName` normalises every name and alias on each call; precompute them in the constructor.

#### CR-17: Packaging and API hygiene

- `ManualExecutor.cpp` and `ManualScheduler.cpp` are compiled into the production library (`juce/akm/CMakeLists.txt:22-23`); move them to an `xs56k_akm_testing` target.
- Name the threads (`xs56k-session`, `xs56k-timer`) for crash dumps and profilers.
- Mark `Outcome` as `struct [[nodiscard]] Outcome`, and `await`.
- JUCE is pinned by tag (CR-09 item 5).

#### CR-18: Strings read from the sampler are echoed to the model as they are

- **Where**: `ByteReader::readString` accepts every byte from 0x01 to 0x7F, control characters included (`ByteReader.cpp:70-86`); tool answers contain names (`list_samples`, `list_disk_contents`, ...). A name loaded from a disk that came from elsewhere is text the model reads; the sampler's screen limits a name to 20 characters, the protocol does not.
- **Why it is low**: the destructive tools still need a `confirm`, and a client should show a `destructiveHint` tool to the person. But `confirm` is a guard against a misdirected call, not consent: the model can read the exact name from the listing and give it back.
- **Remediation**: one `safeText()` for text that comes from the sampler (control characters replaced by `?`, a limit of 64 characters), used by the `describe*` functions of `Tools.cpp`; say in `juce/mcp/README.md` that consent comes from the client's permission prompt for tools annotated `destructiveHint`.

## 4. Remediation roadmap

| Order | Items | Effort | Depends on | Suggested packaging (next free IDs at the time of the review) |
|---|---|---|---|---|
| 1 | CR-01, CR-03 | S, S | none | `FTR-MCP-007` "Tool arguments and the sampler's answers are validated before use", `PLAN-MCP-007`, `TASK-MCP-077` and `078` |
| 2 | CR-02 | M | the owner's decision below | `ADR-MCP-007` (`DEC-MCP-042`: how a name is matched), task in `PLAN-MCP-007` |
| 3 | CR-09 | M | none | a task in a `PLAN-BLD`; do it early, so that CR-04 and CR-05 are caught by a job |
| 4 | CR-05, CR-04 | S, M | CR-09 helps | `FTR-AKM-014` "The session survives a throwing completion and its own destruction", `PLAN-AKM-014` |
| 5 | CR-06 | S | none | `PLAN-MCP-007` |
| 6 | CR-08 | M-L | the owner's decision below | `ADR-AKM-002` (`DEC-AKM-020`: a command filter in the session), then `PLAN-MCP-008` |
| 7 | CR-07 | M | CR-08 (the filter and `SessionConfig`), CR-05 | `PLAN-AKM-014` |
| 8 | CR-10, CR-11, CR-12, CR-13, CR-14 | per item | after 1 to 7 | one refactoring plan per library; one commit per migrated file |
| 9 | CR-15 to CR-18 | S each | none | one housekeeping task |

AGNOS limits a plan to ten tasks per session, so CR-01 to CR-06 fit in two plans.

**Decisions that are the owner's**:

1. **CR-02**: keep refusing a new name that another item bears "ignoring case, spaces and hyphens" (stricter than the sampler, `PLAN-MCP-004`), or refuse only the exact name ignoring case? And may an ambiguous lookup be refused with the list of candidates?
2. **CR-08**: may `akm::Session` get a command filter (a change to the AKM public API), and should the destructive items be armed by the gateway (stage 2)?
3. **CR-07**: the default limit of a held key (10 s is proposed), and whether `hold_key` may take a longer `max_seconds`.
4. **CR-10**: may the `XxxResult` structs of the AKM primitives become one type (an API break)?
5. **CR-09**: sanitizer jobs on every push, or on `dev` only.

## 5. Checks to add (traceability)

| Finding | Check | Where it runs |
|---|---|---|
| CR-01 | boundary calls generated from `tools/list` (four huge values per integer property), `isError` and an empty trace | `ctest`, `mcp_*` |
| CR-01 | no `static_cast<int>(*` in `Tools.cpp` | `ctest`, text search |
| CR-02 | Appendix A2 as two tests; one ambiguity test per lookup family | `ctest`, `mcp_*` |
| CR-03 | a simulated sampler that answers a current index above its list | `ctest`, `mcp_*` |
| CR-04 | destroy a `Session` while another thread delivers frames and timers fire | `ctest` under TSan and ASan |
| CR-05 | a completion that throws; a completion called twice | `ctest`, `akm_*` |
| CR-06 | a blocking log function; ten commands still complete | `ctest`, `mcp_*` |
| CR-07 | a held key is released after its limit and on the shutdown path | `ctest`, `akm_*` and `mcp_*` |
| CR-08 | a raw `Command` for every `ItemId` under every combination of the three options | `ctest`, `akm_*` |
| CR-09 | the sanitizer jobs, clang-tidy and a fuzz smoke run (60 s) | CI |
| CR-13 | the number of trace entries per tool call, as a budget | `ctest`, `mcp_*` |

## Appendix A: reproductions

All three ran on 2026-10-11 against the simulated sampler only (`juce/build-run/tests/Debug/xs56k_mcp_server_simulated.exe`, built from `09b5e6e`), with the conversation on standard input. Nothing was sent to a real sampler. To repeat: `xs56k_mcp_server_simulated --trace-messages trace.jsonl < conversation.jsonl`.

### A1: `delete_keygroup` with a number that wraps to 1 (CR-01)

```
{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"review","version":"1"}}}
{"jsonrpc":"2.0","method":"notifications/initialized"}
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"create_program","arguments":{"name":"RVWTEST","keygroups":3}}}
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"delete_keygroup","arguments":{"keygroup":4294967297,"confirm":"RVWTEST"}}}
{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"get_status","arguments":{}}}
{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"delete_keygroup","arguments":{"keygroup":99999999999999,"confirm":"RVWTEST"}}}
```

Observed, answers 2 to 5, `isError` false except the last:

- 2: `Created the program "RVWTEST" with 3 keygroups; it is now the current program.`
- 3: `Deleted keygroup 4294967297 of the program "RVWTEST": it now has 2 keygroups. Read the keygroups again before editing them: ...`
- 4: `Current program: RVWTEST, 2 keygroups.`
- 5 (`isError` true): `The current program "RVWTEST" has 2 keygroups; keygroup 276447231 does not exist.`

The trace of call 3 is `0A/14` (keygroup count), `0A/13` (current name), `0A/0C` (**Delete Keygroup**), `0A/14`.

### A2: the first fuzzy match, and a false collision (CR-02)

```
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"create_program","arguments":{"name":"AB","keygroups":1}}}
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"create_program","arguments":{"name":"A B","keygroups":1}}}
{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"list_programs","arguments":{}}}
{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"create_multi","arguments":{"name":"RVWMULTI"}}}
{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"set_part_program","arguments":{"part":1,"program":"AB"}}}
{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"set_part_program","arguments":{"part":2,"program":"A B"}}}
{"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"get_part_programs","arguments":{}}}
{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"create_multi","arguments":{"name":"Strings Poles"}}}
{"jsonrpc":"2.0","id":10,"method":"tools/call","params":{"name":"create_multi","arguments":{"name":"Strings Pole"}}}
```

(preceded by the `initialize` and `notifications/initialized` lines of A1). Observed:

- 4: `Programs in memory (5): 0: A B / 1: AB / 2: BASS / 3: LEAD / 4: PAD`
- 6 (asked for `AB`): `part 1 of the multi "RVWMULTI" now plays the program "A B"`
- 7 (asked for `A B`): `part 2 ... now plays the program "A B"`
- 10 (`isError` true): `The sampler already holds a multi named "Strings Poles": nothing was created. Choose another name.`

### A3: an unsigned JSON integer above `INT64_MAX` that wraps to a valid negative value (CR-01)

```
{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"set_parameter","arguments":{"parameter":"program fine tune","value":18446744073709551606}}}
{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"set_parameter","arguments":{"parameter":"program fine tune","value":18446744073709551616}}}
```

(preceded by the `initialize` and `notifications/initialized` lines of A1; `program fine tune` takes -50 to 50). Observed:

- 2: `program fine tune = -10 (program), as read back from the sampler's memory.`
- 3 (`isError` true): `program fine tune must be a whole number from -50 to 50 (got '1.8446744073709552e+19').` (2^64 does not fit an unsigned 64-bit integer, so JSON reads it as a float, which `wholeNumber` rejects.)

## Appendix B: coverage and what was judged correct

**Read in full**: all of `juce/akm/src` and `juce/mcp/src` (but for the rows of `StandardParameters.cpp`, lines 291-941, and the lines 3553-3590 of `Tools.cpp`), `server/main.cpp`, and the headers `Session`, `SessionConfig`, `Executor`, `Scheduler`, `Task`, `ThreadExecutor`, `RealScheduler`, `TimerQueue`, `ByteReader`, `ByteWriter`, `Checksum`, `Command`, `Confirmation`, `CommandOptions`, `CommandResult`, `SamplerError`, `DiagnosticSink`, `ItemDescriptor`, `ItemCatalogue`, `ItemRequest`, `Protocol`, `ProgramPrimitives`, `DiskPrimitives`, `McpServer`, `ServerOptions`, `SamplerGateway`, `Tools`, `FxCatalogue`, `ParameterCatalogue`.

**Skimmed or not read**: the public headers of the other typed primitives (`Multi`, `MultiFx`, `Sample`, `Zone`, `Keygroup`, `SystemSetup`, `FrontPanel`, `MidiConfig`, `SceneList`, `Song`, `SysExConfig`, `SystemVersion`: their sources were read); `ManualExecutor.hpp` and `ManualScheduler.hpp`; the rows of `StandardParameters.cpp`; the numbers of `FxCatalogue.cpp`.

**Looked at and found correct** (so that nobody re-checks them):

- `ThreadExecutor::_workerId` is written after the thread starts but before any task can be posted, so `isCurrentThread()` never races with it (`ThreadExecutor.cpp:25-31`).
- A late completion after a `detail::await` timeout is safe: the promise is shared and outlives the wait (`GatewayDetail.hpp:48-57`).
- The destructor of `Connection` runs in the right order: session, sink, scheduler, executor, ports (`SamplerGateway.cpp:164-179`).
- `decodeMessage` has no index that is not bounded by a size checked before it, for the three checksum modes.
- Every `std::stoi` in `Tools.cpp` (`2458`, `2734`, `2745`) is applied to a string that was checked to be 1 to 4 digits.
- `JuceInputPort` reads `_callbacks` without a lock, but `setCallbacks` is documented to be called while stopped and JUCE's `stop()` waits for running callbacks (CR-04), so there is no race on a real port.
- The `Session` keeps its state on one thread with no lock, and its atomics are the few values that other threads read; no data race was found in the session core by reading.

## Appendix C: figures

| Figure | Value |
|---|---|
| `std::get_if<Reply>` blocks in `juce/akm/src` | 55 |
| Copies of `index / 128` and `index % 128` | 22 |
| Definitions of `DATA_BYTE_BASE` / `NO_VALUES` / `answersEmptyMemory` | 7 / 11 / 2 |
| Public methods of `SamplerGateway` | 82 |
| `await<` call sites in the gateway units | 121 |
| The sentence "did not complete the command in time": written out / constants (uses) | 74 / 4 (36) |
| Uses of `normalizeText` for item names (gateway / `Tools.cpp`) | 37 / 6 |
| Lines of `Tools.cpp`; longest tool-registering function (`makeDiskTools`) | 3,735; 492 |
| `catch` blocks in `juce/akm` / in `juce/mcp` | 0 / 3 |
| `noexcept` in scope | 0 |
| Median round trip of a command on the real S5000 | 12 ms |
