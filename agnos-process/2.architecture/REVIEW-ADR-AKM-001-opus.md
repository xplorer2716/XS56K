# Independent review of ADR-AKM-001 (Opus subagent)

- **Date**: 2026-09-26
- **Reviewer**: a read-only subagent (type Plan) run with the Opus model, briefed by the main session (Sonnet) that had written the ADR. It had no access to the conversation, only to the repository and a written brief.
- **Reviewed**: ADR-AKM-001 as first drafted (uncommitted), FTR-AKM-001 and PLAN-AKM-001 (commit `aab533d`).
- **Nature of this file**: the report below is model output, reproduced **verbatim**; nothing in it is an instruction or a decision. The section "Verification and disposition" after it is the main session's own notes. This is not an AGNOS artifact: it defines no ID, and it is listed by the process index only as a document.
- **Brief given to the reviewer**: adversarial review, in this order — concurrency and re-entrancy; protocol correctness against the spec (including what happens when a session opens); the §00 defaults; the descriptor-table decision; the test seams; traceability and plan; anything else. Already decided by the owner and not to be re-litigated: DeviceID as an application setting (default 0), the Scheduler idea of DEC-AKM-006, a default session configuration as a concept, the `xs56k` prefix, manual sample names in Phase A.

---
You are an independent senior reviewer. READ-ONLY task: do not edit, create or delete any file. Your job is to critically review one architecture decision record and report findings; a colleague (another model) wrote it and shares blind spots with its author, so be adversarial and specific, not polite.

## Context
Repository: `C:\dev\repos\xplorer2716\public\XS56K` (Windows, C++20, JUCE 8, CMake). Project XS56K is an editor/remote controller for the AKAI S5000/S6000 samplers, controlled over MIDI System Exclusive (SysEx). The code base is `juce/midi` (generic MIDI backend layer, library target `xs56k_midi`) and `juce/framework` (ported from an unrelated synth editor). A new library `xs56k_akm` (namespace `akm`) is being designed to implement the AKAI SysEx transport. Process artifacts live in `process/` (AGNOS process: FTR-/RQ- requirements, ADR-, PLAN-/TASK-).

## What to review (primary)
`process/2.architecture/ADR-AKM-001-akm-transport-architecture.md` (status Proposed, decisions DEC-AKM-001 to DEC-AKM-008).

It serves the requirements in `process/1.requirements/FTR-AKM-001-transport-et-config-sysex.md` (RQ-AKM-001..020, 039, 040). Outline features FTR-AKM-002..004 (Program, Keygroup, Zone sections) build on the primitive shape it fixes. Its implementation tasks are TASK-AKM-002..010 in `process/3.plan/PLAN-AKM-001-transport-and-sysex-config.md`.

Ground truth to check the ADR against (read them, do not trust the ADR's description of them):
- Code seam: `juce/midi/include/common/midi/MidiPorts.hpp`, `MockMidiBackend.hpp`, `MidiMessage.hpp`, `JuceMidiBackend.hpp`; `juce/midi/src/MockMidiBackend.cpp`, `juce/midi/src/juce/JuceMidiBackend.cpp` (threading and delivery behaviour of each backend), `juce/midi/src/MidiMessage.cpp`.
- The protocol: `documents/_index/sysex_spec.kb.md` (dense summary), `documents/akai_s5000_s6000_sysex_spec_2.10.pdf.md` (full spec text; framing, checksum, confirmations and section §00 are at about lines 181-480), `documents/_index/sysex_spec.items.tsv` (one row per item).
- Older material for context only: `process/1.requirements/DRAFT-s5000-midi-primitives-and-workflows.md`, `process/1.requirements/RQ-MID-midi-layer.md`, `process/2.architecture/ADR-BLD-001-juce-cmake-build-foundation.md`.
You may use Grep/Glob/Read, or the grepai semantic search tool if available.

## Already decided by the owner — do NOT re-litigate these
- The target DeviceID is an application setting with a default of 0; the AKM library only receives it in a config when a session opens.
- The scheduler-injection idea of DEC-AKM-006 (real timer thread vs manual scheduler for tests) is confirmed as is; you may still point out a concrete defect in its details, but not argue against the idea.
- Having a default session configuration (DEC-AKM-007) is approved as a concept; the specific default values are still open to critique.
- Identifiers use the prefix `xs56k`; the old `xpl` prefix is banned everywhere.
- In Phase A, the zone's sample is given by name manually (no sample listing).

## What I want from you
Look hardest at correctness and hidden problems, in this order:
1. Concurrency and re-entrancy in DEC-AKM-004/005: is "register the command before send, release the lock before send, invoke completions outside the lock" actually sufficient and consistent with how `MockMidiBackend` (synchronous delivery on the calling thread) and `JuceMidiBackend` (JUCE-owned callback thread per device) deliver? Any deadlock, lost-wakeup, ordering, double-completion or use-after-destruction scenario (including timeout firing concurrently with a confirmation, and session destruction with a pending command or pending scheduled task)?
2. Protocol correctness against the spec: single command in flight per port, matching by DeviceID and user-ref, REPLY possibly followed by ERROR, checksum mode affecting how incoming frames are parsed, the DeviceID-0 broadcast semantics, Query/discovery collecting several DONEs, Still Alive `F0 F7`. Look for anything the ADR or the requirements get wrong or leave dangerous. Pay particular attention to the ORDER of operations when a session opens (DEC-AKM-007): what does the code know, and what can go wrong, at each step given that section §00 has no Get item and the sampler's current checksum mode is unknown when the session starts?
3. The chosen defaults in DEC-AKM-007 (checksum ON, Notification OFF, Sync LCD OFF, Auto screen update OFF, Still Alive ON): is each justified by the spec text? Any that could bite?
4. The descriptor-table decision (DEC-AKM-003) versus the alternatives, given about 200 command items in the later lots and known errata: is it the right shape? What would break first?
5. Test seams (DEC-AKM-008): can the same scenario source really run on both the simulated sampler and JuceMidiBackend? What is unrealistic about a simulated sampler that answers synchronously or from a script?
6. Traceability and completeness: requirements in FTR-AKM-001 that no decision or task covers, decisions that contradict a requirement, task ordering/tier problems in the plan, missing risks.
7. Anything else a careful reviewer would flag (naming, layering, dependency direction, build).

## Report format (under about 700 words)
- A short verdict (accept / accept with changes / rework) with one-sentence reason.
- Findings, most severe first. For each: severity (blocking / important / minor), the exact claim in the ADR or requirement (quote or ID), your evidence (file path and line, or spec page/line — verify it yourself), why it matters (concrete failure scenario), and a concrete proposed change.
- A separate short list of things you checked and found sound, so I know what NOT to worry about.
- A separate list of things you could not verify and why.
Do not pad. If you are unsure of a claim, say so rather than asserting it.

---

## Report (verbatim)

**Verdict: rework the three decisions DEC-AKM-004, 005 and 007.** DEC-AKM-001, 002 and 006 are sound. DEC-AKM-004/005 leave threading gaps that can deadlock under JUCE, and DEC-AKM-007's opening order will likely fail at the second launch on real hardware. DEC-AKM-003 and 008 need changes.

## Findings (most severe first)

**1. Blocking: the session opens while the sampler's checksum state is unknown.**
- **Claim:** DEC-AKM-007 "sends a Query…, then sets checksum mode first". The codec only knows ON or OFF (DEC-AKM-002, RQ-AKM-003).
- **Evidence:** the sampler adds a checksum to confirmations only when checksums are enabled (spec md 271-272). §00 settings cannot be read back.
- **Failure:** the ADR's own default is ON and nothing restores it on close. At the next launch, the Query (the ADR never says it carries a checksum) gets either ERROR 129 or a DONE with a checksum that the decoder misreads. Result: "no sampler at DeviceID N" against a sampler the app configured itself. The same ambiguity affects the DONE of &04 itself, and discovery when samplers on the chain are in different modes.
- **Fix:** add a third codec mode, `Unknown`:
  - Always append a checksum. The sampler ignores it when checksums are off (md 231-232).
  - Decode by each item's expected data length (OK/DONE 0 bytes, ERROR 2, Echo 4). Accept one extra byte only if it is a valid checksum.
  - Refuse commands with variable-length replies until the mode is known.
  - Decode discovery answers per responder, and count ERROR answers as "sampler present".
  - Compute the checksum when the frame is sent, not when it is submitted.

**2. Blocking: DEC-AKM-004/005 are necessary but not sufficient.**
- **Who sends the next command is unspecified.**
  - If it is the JUCE input callback: a JUCE SysEx send calls `midiOutPrepareHeader` and `midiOutLongMsg`, then loops on `Sleep(1)` with no bound (juce_Midi_windows.cpp:2874-2893). That would run inside WinMM's input callback, where Microsoft's MidiInProc documentation says not to call multimedia functions because of deadlock. It also breaks RQ-AKM-020 ("without blocking").
  - If it is the timer thread: a timeout can fire while an earlier send is still in that loop. Two `sendMessageNow` calls then run concurrently and share one member buffer, `mainPackets` (juce_MidiDevices.h:381-384, 458-468).
- **Timeout racing a confirmation:** there is no generation token and no defined cancel behaviour. A blocking cancel called under the session lock deadlocks against a timer task waiting for that lock.
- **Queue order:** the next command must be taken from the queue and registered in the same critical section that completes the previous one. Otherwise a `submit()` from inside a completion jumps the queue. Completions invoked from two threads can also arrive out of order, which breaks RQ-AKM-020.
- **Recursion:** a simulated sampler that answers synchronously nests send → complete → send as deep as the queue.
- **Destruction:** JUCE's `MidiInput::stop()` spins on a non-recursive SpinLock that is held for the whole user callback (juce_MidiDevices.cpp:70-80, 158-180). Closing a session from a completion therefore deadlocks, and so does stopping ports while holding the session lock. Two more gaps: timer tasks that capture `this`, and what happens to queued commands on close (there is no `Cancelled` result).
- **Fix:**
  - One serial executor per session performs every send and every completion or diagnostic, in order. The backend and timer threads only enqueue.
  - Timeout tasks carry a token, and cancel never blocks.
  - `close()` stops the input, cancels timers and waits outside the lock, then completes the queue as `Cancelled`.
  - Destroying a session from its own callbacks is forbidden.
  - `MidiPorts.hpp` documents that after `stop()` no callback is running. JUCE guarantees this; the mock does not (plain `bool started`, MockMidiBackend.cpp:87, 156-157).

**3. Important: the DeviceID rules contradict the spec.**
- RQ-AKM-039's acceptance test expects the list `[3]` for target 5. A sampler with ID 3 never answers a Query sent to 5 (md 214-215), so this needs a broadcast discovery, which nothing specifies.
- A sampler whose own ID is 0 answers every message (md 210-211). With target N≠0 and such a sampler on the chain, every Set runs on both samplers. The ambiguity check only covers target 0; it should cover any target.
- Count responses rather than distinct IDs: the spec is unclear whether replies carry the ID as sent or the sampler's own (md 266-267 vs 283-286).
- Fix the matching rule for target 0 now rather than after RQ-AKM-017, and also match section and item (md 269-270).

**4. Important: the §00 defaults in DEC-AKM-007.**
- **Still Alive ON:** item &07 was added in OS 2.10 (md 150-151). With "ready only when all have completed", samplers on older OS versions will likely never open. &03 may have the same problem (OS 2.00, md 134-139). Treat "not supported" on &03, &05 and &07 as a degraded mode, not a failure; the DRAFT already marks &05/&07 as non-blocking (lines 104, 106). Also cap the number of Still Alive restarts.
- **Checksum ON is the riskiest provisional choice:**
  - How the checksum covers confirmations is unverified.
  - A sampler reboot mid-session probably returns it to OFF (md 228). Every confirmation would then fail verification, and there is no recovery rule.
  - It leaves other tools broken, which RQ-AKM-018's own rationale calls harmful.
  - Default to OFF until RQ-AKM-017 has run, and add a `close()` that restores the sampler's defaults.
- **Auto screen update OFF** has no stated reason.
- **Sync LCD:** the spec says Sync ON is the default (md 1047, 1436, 1592), so FTR-AKM-001's open point claiming no default is wrong. The ADR also settles a question that the FTR deferred to FTR-AKM-002.

**5. Important: the descriptor table (DEC-AKM-003).**
- The first thing to break is the "current program / current keygroup" state. There is no way to submit a select and its operation as one unit, so a select that times out, followed by a Set, edits the wrong target.
- The table shape also cannot express:
  - conditional arguments (§0A/&0A, TSV line 212);
  - ranges on combined values such as ±9999 (TSV line 52);
  - reply counts that depend on state (RQ-AKM-031/036);
  - the alternative and blocked message layouts (kb lines 98-101).
- The planned coverage script would have to parse C++. Better: keep a human-reviewed data file, generate the `constexpr` table from it, and compare that file with the TSV.

**6. Important: test seams (DEC-AKM-008).**
- Sharing one scenario across both backends also needs a wait/clock driver that works with both the manual and the real scheduler.
- Replies scripted inside `send()` never reproduce the real case: a reply arriving on another thread before `send()` returns.
- Build the simulated sampler as a small model of the spec rather than per-test scripts: addressing, §00 state kept per port across sessions, checksum handling and ERROR 129, REPLY followed by ERROR, unsupported items.
- RQ-AKM-016 still says "built on the in-memory loopback backend"; amend it.

**7. Important: the plan.**
- TASK-AKM-006's criteria need TASK-AKM-007's simulated sampler, but 007 is not in 006's dependencies.
- The discovery window and checksum-mode switching are Session behaviour (DEC-AKM-002) but are assigned to TASK-AKM-008.
- RQ-AKM-017 (real hardware) runs last. Add a hardware capture spike right after TASK-AKM-004.
- No task covers closing a session and restoring settings.

**8. Minor.**
- An ERROR arriving after a REPLY is dropped as "unsolicited" while the caller keeps possibly bad data.
- On Linux, one ALSA thread serves all inputs (juce_Midi_linux.cpp:232, 695), not one thread per device.
- The input must be started before the first send.
- TASK-AKM-005 does not test inserting an earlier deadline (lost wakeup) or scheduling/cancelling from inside a task.
- RQ-AKM-039 says "no command is sent", but the Query has been sent.
- RQ-AKM-040's rationale contradicts RQ-AKM-009 about Notification.

## Checked and sound
- `xs56k_midi` is JUCE-free, so the dependency direction holds.
- Registering the command before sending is required by both backends: the mock's loopback, and JUCE's send, which blocks until the message is done.
- Releasing the lock around `send()` is correct.
- `F0 F7` is classified as SysEx. JUCE 8.0.15's internal round trip appears to preserve it (MidiDataConcatenator.h:72-76, UMPFactory.h:263-266), so the remaining risk is at driver level, as the ADR says.
- The item counts (7 and 203) match the TSV.
- The unprefixed `akm` namespace follows ADR-BLD-004.

## Not verifiable here
- Firmware behaviour: which DeviceID the sampler echoes, what the confirmation checksum covers, the shape of &04's own DONE, whether §00 settings survive a power cycle, and how older OS versions answer &07.
- WinMM's callback-thread layout.
- macOS CoreMIDI.

## Where the evidence is
- Spec md: `C:\dev\repos\xplorer2716\public\XS56K\documents\akai_s5000_s6000_sysex_spec_2.10.pdf.md`
- TSV and kb: `C:\dev\repos\xplorer2716\public\XS56K\documents\_index\`
- JUCE: `C:\dev\repos\xplorer2716\public\XS56K\juce\build-win-local\_deps\juce-src\modules\juce_audio_devices\` (`native\juce_Midi_windows.cpp`, `native\juce_Midi_linux.cpp`, `midi_io\juce_MidiDevices.h` and `.cpp`)
- Mock: `C:\dev\repos\xplorer2716\public\XS56K\juce\midi\src\MockMidiBackend.cpp`

### Critical Files for Implementation
- C:\dev\repos\xplorer2716\public\XS56K\process\2.architecture\ADR-AKM-001-akm-transport-architecture.md
- C:\dev\repos\xplorer2716\public\XS56K\process\1.requirements\FTR-AKM-001-transport-et-config-sysex.md
- C:\dev\repos\xplorer2716\public\XS56K\process\3.plan\PLAN-AKM-001-transport-and-sysex-config.md
- C:\dev\repos\xplorer2716\public\XS56K\juce\midi\include\common\midi\MidiPorts.hpp
- C:\dev\repos\xplorer2716\public\XS56K\juce\build-win-local\_deps\juce-src\modules\juce_audio_devices\midi_io\juce_MidiDevices.cpp

---

## Verification and disposition (main session)

Each finding was checked against the sources by the main session before being acted on. "ADR" means ADR-AKM-001, amended after this review; "FTR" and "PLAN" mean FTR-AKM-001 and PLAN-AKM-001.

| # | Finding | Main-session check | Disposition |
|---|---|---|---|
| 1 | Checksum state unknown at session open | Spec text read: a checksum sent while checksums are off "may still be transmitted; although [it] will be ignored"; confirmations carry one only when enabled (Modification History 1.30 and p. 5). Sound. | Accepted. ADR: DEC-AKM-007 (order of opening) and DEC-AKM-009 (tri-state mode). FTR: done (RQ-AKM-041). |
| 2 | Threading gaps (send from a callback, timer races, close) | JUCE 8.0.15 sources read: the Windows SysEx send waits in an unbounded polling loop (`juce_Midi_windows.cpp` 2880-2893); `MidiOutput::sendMessageNow` uses the shared `mainPackets` (`juce_MidiDevices.h` 381-389, 479); `MidiInput::stop()` takes the spin lock that the input callback holds throughout (`juce_MidiDevices.cpp` 76-80, 158-180); the mock's `started` is a plain `bool` (`MockMidiBackend.cpp` 87, 156-158). Not checked: Microsoft's guidance on calling the multimedia API from an input callback; the ADR does not rely on it. | Accepted. ADR: DEC-AKM-004 (serial executor per session, `Cancelled`, `close()`) and DEC-AKM-005 (send only from the executor, confirmations enqueued). FTR: done (RQ-AKM-042 for closing, RQ-AKM-020 reworded). |
| 3 | DeviceID rules contradict the spec | Spec text read (p. 3, lines 210-215): a sampler with a non-zero DeviceID answers only messages carrying it; DeviceID 0 answers all and is answered by all. The `[3]` example of RQ-AKM-039 cannot happen. | Accepted. ADR: DEC-AKM-007 (broadcast discovery first, ambiguity for any target when a DeviceID-0 sampler is present). FTR: done (RQ-AKM-039 rewritten). |
| 4 | §00 defaults | Modification History read: synchronisation option since OS 2.00, Still Alive since OS 2.10. Spec introductions of §0A and §0E (lines 1047 and 1592) state synchronisation ON is the default: the FTR open point that said no default is documented was wrong for `&03` (Table 5 itself states none). | Accepted. ADR: DEC-AKM-007 (tri-state settings; checksum OFF, Sync LCD OFF, Still Alive ON, Notification and Auto screen update left unchanged; ERROR `00` gives a degraded open). FTR: done (RQ-AKM-040 reworked, open points corrected, maximum total wait added to RQ-AKM-010 and RQ-AKM-011). |
| 5 | Descriptor table | Reasoning sound; the "select then operate" hazard is real given the state model. | Accepted in part. ADR: DEC-AKM-003 (reviewed data file, generated and checked-in table, schema extension deferred to FTR-AKM-002) and DEC-AKM-010 (sequences abort on failure). FTR: done (RQ-AKM-043). |
| 6 | Test seams | Sound; the mock indeed cannot script replies. | Accepted. ADR: DEC-AKM-008 (simulated sampler as a model of the spec, delivery on the sending or another thread, scenario driver). FTR: done (RQ-AKM-016 reworded). |
| 7 | Plan | Dependencies re-read: TASK-AKM-006's criteria use the simulated sampler of TASK-AKM-007. | Accepted. PLAN: done (dependencies corrected, session mechanics regrouped in TASK-AKM-006, TASK-AKM-012 for early hardware contact, TASK-AKM-011 for closing). |
| 8 | Minor points | Linux: one ALSA input thread per sequencer handle confirmed (`juce_Midi_linux.cpp` 232, 695). | Accepted. ADR: Linux note, input started before the first send, late ERROR reported as a diagnostic. FTR and PLAN: done (RQ-AKM-039 wording, RQ-AKM-040 rationale, TASK-AKM-005 criteria). |

Where the main session did not follow the reviewer to the letter:
- Finding 1 suggested always appending a checksum. It is appended only while the mode is unknown (ADR DEC-AKM-009); when the mode is known to be off it is omitted, since the spec's "ignored" claim is about fixed-length items.
- Finding 4 suggested defaulting Notification and Auto screen update to fixed values. They are left "unchanged" (not sent) until an observation gives a reason, and each §00 setting became a tri-state in the configuration.
- Finding 5 suggested one data file and a generated table. The generation is done by a script run by hand, with the generated table checked in and guarded by a drift check, so the build needs no Python.
- The proposed `close()` that restores the sampler's defaults became a "Should" requirement (RQ-AKM-042): best effort, it cannot help when the program crashes.

Not verified by the main session either: the firmware questions listed by the reviewer (they need the real sampler, RQ-AKM-017), WinMM's callback threading, macOS CoreMIDI, and the round trip of `F0 F7` through JUCE's UMP layer.
