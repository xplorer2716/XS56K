# Independent review of DEC-AKM-019 — Opus subagent (read-only), saved verbatim

Requested for TASK-AKM-072 (session AKM, 2026-10-03), as for ADR-AKM-001 before it. The report below is the
subagent's own text; the "Verification and disposition" section after it is the author's.

---

## Review of DEC-AKM-019 (ADR-AKM-001 lines 514-539)

I read the decision text and checked it against the sources: `juce/akm/src/Session.cpp`, `Session.hpp`, `CommandOptions.hpp`, `SessionConfig.hpp`, `FrontPanel.hpp`/`.cpp`, `SysExConfig.cpp`, `Checksum.hpp` and RQ-AKM-075.

### Blocking

None.

### Should-fix

**S1. Releasing keys before the checksum restore depends on the session's checksum-mode tracker being right, and the close itself can make it wrong (question b).**
- `cancelEverything` (Session.cpp:846-855) drops the command in flight without going through `complete()`. So the line that sets the mode to Unknown when a checksum-mode command does not succeed (Session.cpp:521-524) never runs for a cancelled command.
- Example: the session is in mode Off, the application's `setChecksums(true)` (SysExConfig.cpp:61-62) is in flight when `close()` runs, and a key is held.
  - The sampler may already have switched to On, but the session still thinks Off.
  - The closing Release is then framed without a checksum (Session.cpp:298) and decoded in mode Off (Session.cpp:325).
  - If the sampler drops that frame, the Release times out. That timeout ends the restoring, so the checksum mode is never put back and the key stays down.
- Today's order is safe here because the checksum restore always goes out with a checksum and is decoded in Unknown, whatever the tracker says (Session.cpp:297-298, 325). DEC-019 puts commands in front of it, so its claim "costs no framing change" (ADR:528-529) no longer holds in this case.
- Fix, either one:
  - (preferred, and right regardless of this decision) in `cancelEverything`, set the mode to Unknown when the cancelled command had `checksumModeAfterDone`. A Release sent in Unknown carries a checksum the sampler ignores if it does not expect one (Checksum.hpp:30), and a DONE decodes in either mode.
  - or order the close as: checksum restore, then keys, then the other settings.
- Confirmation matching is otherwise safe. A late DONE from the cancelled Hold or Release has a different user-ref, and for Hold a different item too (Session.cpp:456-468), so it can only be reported as unsolicited.

**S2. Held keys are not tied to the device they were sent to (question d).**
- The set records keycodes only, but the target can change while keys are remembered:
  - `bindTarget` can be called at any time (Session.cpp:924-928).
  - A session never opened runs application commands (Session.cpp:238-246 only blocks Opening and OpenFailed; Session.hpp:87-88).
  - `beginOpen` resets the target (Session.cpp:639) and may bind a different one (Session.cpp:697).
- So a key held on device X before an `open()` or a rebind is "released" on device Y at close, and stays down on X. If the open failed, the closing Release is instead refused as NoTargetBound (Session.cpp:283-286), which at least reports it.
- `changed` has the same flaw, but there it only means writing a default to Y. Here it leaves a key stuck on X and sends an unwanted Release to Y.
- Fix: remember `(deviceId, keycode)` pairs. At close, list keys held on a device other than the current target in `keysNotReleased` instead of sending them.
- Not a problem after close: `closing` is never reset (only Session.cpp:184 and 973), so `beginOpen` refuses (Session.cpp:629-630) and a closed session cannot be reused.

**S3. Forget a key whose Hold the sampler says it does not support (questions a and e).**
- For settings, the session forgets on NOT_SUPPORTED (Session.cpp:527-529). DEC-019 forgets only on a successful Release.
- On a sampler or OS without §20, every Hold would then trigger a closing Release that also errors. The key lands in `keysNotReleased` and `restoredAll()` is false on every close.
- It also conflicts with RQ-AKM-075's acceptance criterion "no held key → no §20 frame is sent".
- Fix: erase the key in `complete()` when the Hold answers NOT_SUPPORTED, alongside line 528.

**S4. The decision claims more for the destructor and the manual executor than the code does.**
- RQ-AKM-075 says the layer "SHALL leave no key held by a session that is gone".
- The destructor runs the closing only when `runsOnItsOwnThread()` (Session.cpp:895). It waits at most 2×commandTimeout+1s (Session.cpp:71-72, 901), then sets `alive` to false and the remaining closing tasks do nothing (Session.cpp:903, 210-212).
- With Still Alive on (it is on by default, SessionConfig.hpp:52), each restoring command can be stretched up to `maxTotalWait` (Session.cpp:409-416). With keys first, up to 43 Releases now run before Still Alive is restored.
- ADR:537-538 should state these limits, so the requirement is not read as a guarantee.

### Nit
- N1. ADR:523-524 "a Hold … whose ERROR followed a REPLY" and ADR:524 "(DONE or REPLY)": §20 items have no REPLY. This wording was carried over from the settings text.
- N2. The remember must sit exactly where `changed.insert` is (Session.cpp:312-313): after the refusal, no-target and encode checks (277-308), before `send`. The forget must sit in `complete()` only, not in `recordResult`; otherwise a Release cancelled by `cancelEverything` would wrongly forget the key. Worth stating in the task.
- N3. `holdsKey`/`releasesKey` in `CommandOptions` and the new `CloseResult` fields must use a plain `std::uint8_t`, not `FrontPanelKey`. Otherwise headers include each other in a loop: FrontPanel.hpp:25 includes Session.hpp, and CommandOptions.hpp:27 includes SessionConfig.hpp.
- N4. Same key held twice: a set assumes the sampler does not count Holds (one Release clears two Holds). The spec is silent; add it to the real-sampler risks. Releasing in ascending keycode order is arbitrary. Releasing in reverse order of holding would mirror a human chord, if key combinations matter on the S5000.
- N5. Only Holds sent through `holdKey` are remembered. A raw `submit`/`submitSequence` of `FrontPanelKeyHold` without the option is not, as with `changesSetting` (CommandOptions.hpp:78-80). Say so in the decision, or detect §20/&01 from the command bytes in `startCommand`.

### Answers to the questions
- **(a)** "Remember on send, forget on a successful Release" is correct in every interleaving asked about:
  - A Hold in flight when `close()` runs was remembered at send, and is released.
  - A Hold queued but never sent, or refused because it was enqueued after `close()` set `closing` (Session.cpp:230, 973), is not remembered and nothing is sent.
  - `pressKey` with its Release queued or in flight at close: the Release is cancelled, the key stays remembered and the closing releases it.
  - A Release that times out keeps the key, and the closing tries once more.
  - Two keys are each remembered and each released.
  - The exceptions are S3 (a Hold answered with an ERROR) and N4 (the same key held twice).
- **(b)** Framing is fine for a correct tracker (key items are Delimited, and in Unknown a checksum is sent), but see S1.
- **(c)** Application commands submitted late are refused (Session.cpp:230-233). Each closing step goes on a turn of its own, so there is no recursion. The first timeout still ends everything. `close()` always finishes as long as the session thread runs; the destructor limits are in S4.
- **(d)** See S2.
- **(e)** Keycodes alone are adequate, with two points:
  - DONE means only "queued", so `keysReleased` means "Release queued". Document it.
  - Consider carrying the `CommandResult` for each key in `keysNotReleased`. The case that matters is "Timeout or not tried, the key may be physically down" versus "ERROR, the key was probably never down". The owner needs to tell these apart because a stuck key needs someone at the panel.

### Verdict
The approach is sound and fits DEC-AKM-004. Before implementing, fix S1 (the cleanest fix is marking the mode Unknown in `cancelEverything`) and S2 (remember the device with each key). S3 and S4 are small corrections to the decision text.

### Critical files
- `juce/akm/src/Session.cpp`
- `juce/akm/include/akm/CommandOptions.hpp`
- `juce/akm/include/akm/SessionConfig.hpp`
- `juce/akm/src/FrontPanel.cpp`
- `process/2.architecture/ADR-AKM-001-akm-transport-architecture.md`

---

## Verification and disposition

Each claim was checked against the sources in the session that received the report (2026-10-03), before the code was written.

| Finding | Verified against | Disposition |
|---|---|---|
| S1 — a cancelled checksum-mode command leaves the mode tracker stale | `Session.cpp`: `cancelEverything` completes the in-flight command through `recordResult(…, Cancelled{})`, never through `complete()`, which is where `setChecksumMode(Unknown)` runs for a command that did not succeed. Confirmed. | **Adopted.** `cancelEverything` sets the mode to Unknown when the cancelled command had `checksumModeAfterDone`; a test of the reviewer's scenario is added. Keys stay first in the closing. |
| S2 — held keys not tied to a device | `Session.cpp`: `target` is an atomic int, `bindTarget` posts a store at any time, `beginOpen` resets and rebinds it. Confirmed. | **Adopted.** The session remembers `(deviceId, keycode)`; a key held on a device other than the current target at close is listed in `keysNotReleased` and not sent. |
| S3 — forget a Hold answered NOT_SUPPORTED | `complete()` already does it for `changesSetting`. Confirmed. | **Adopted.** Erased in `complete()` for a Hold answered ERROR 0. Test added. |
| S4 — destructor limits | `Session.cpp`: the destructor closes only when `runsOnItsOwnThread()` and waits a bounded time. Confirmed. | **Adopted** in the decision text: RQ-AKM-075's "no key held by a session that is gone" is a best effort bounded as DEC-AKM-004 bounds the settings. |
| N1 — "REPLY" wording | §20 has no REPLY row. Confirmed. | **Adopted**, wording fixed. |
| N2 — where to remember and forget | Same place as `changed.insert` (after the refusal, target and encode checks), forget in `complete()` only. | **Followed** and stated in the decision. |
| N3 — plain `std::uint8_t` in `CommandOptions`/`CloseResult` | `FrontPanel.hpp` includes `Session.hpp`; a `FrontPanelKey` in `CommandOptions.hpp` would loop. | **Followed.** |
| N4 — a key held twice, release order | The spec is silent on whether a sampler counts Holds. | **Noted, not changed.** One Release per remembered key, ascending keycode order; recorded as a risk to check on the real sampler. |
| N5 — only `holdKey` remembers | Same as `changesSetting`. | **Stated** in the decision; raw submits are not tracked. |
| (e) — `keysReleased` means "Release queued"; carry a `CommandResult` per key | DONE of a §20 item only means queued (Table 30, note a). | **Documented** that `keysReleased` means "Release queued". Carrying a `CommandResult` per key is **not adopted**: the session's diagnostics already report each failed closing command, and nothing consumes it yet. |
