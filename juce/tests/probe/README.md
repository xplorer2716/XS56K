# xs56k_akm_probe — the real-sampler tools

The probe (`main.cpp`) is built by the test command of `AGENTS.md` as `xs56k_akm_probe`, in `<build dir>/tests/probe/` (with a
`<config>` folder on Visual Studio). It needs the sampler, is run by the owner, and is never run by CI against hardware.
`xs56k_akm_probe --help` lists every option. The text below is the full description of each mode, what it sends and what was
observed on the owner's S5000. Requirements: `RQ-AKM-017` and the RQ ids cited inline.

Modes: `--list` (the MIDI ports), `--in/--out` (first contact), `--session` (a real `Session`), `--suite` (seven checks) and the opt-in
checks of the suite (`--program-lifecycle`, `--sample-lifecycle`, `--multi-lifecycle`, `--system-setup`, `--disk-tools` and its
variants, `--front-panel`, `--midi-config`, `--song-files`, `--scenelists`, `--multi-fx`).

Rules that hold for every run:
- It never sends section 02's Clear Sampler Memory (`&32`), which no real-sampler test may call.
- A slow section 10 command (`--slow-operation`, `--disk-tools-slow`) once left the S5000 answering nothing until it was switched off
  and on: run one only with the owner present and ready for a power cycle
  (`process/2.architecture/OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`, F4 to F7).
- The settings a check changes are put back, even when the check fails half way.
- Exit status 0 when every check passed or was skipped, 2 when no sampler answered at the DeviceID, 3 otherwise.
- The same suite runs against the simulated sampler in `ctest` (tag `[suite]`).

- **First-contact probe** (needs the sampler; run by the owner): built by the test command above as
  `xs56k_akm_probe` (`juce/<build dir>/tests/probe/`, with a `<config>` folder on Visual Studio). `xs56k_akm_probe --list`
  shows the MIDI ports; `xs56k_akm_probe --in "<port the sampler sends on>" --out "<port it receives on>"`
  sends fifteen SysEx frames one at a time and writes `akm-probe-<UTC date>.log`. It switches the
  sampler's checksum and Still Alive settings on and off and ends with both off. [RQ-AKM-017, RQ-AKM-044, TASK-AKM-012]
- **Session smoke test** (needs the sampler; run by the owner): `xs56k_akm_probe --session --in "<port the sampler sends on>"
  --out "<port it receives on>"` (same program, same ports) drives a real `Session` through discovery, the checksum mode,
  50 timed Echo round trips, the OS version and the other section 00 settings, writes `akm-session-<UTC date>.log` and
  ends with checksums off, Still Alive off, Notification on, Sync LCD on and Auto screen update off; `--no-lcd` leaves
  Sync LCD and Auto screen update alone. Exit status 0 when every step went as it had to, 2 when no sampler answered
  at the DeviceID, 3 otherwise. [RQ-AKM-017, TASK-AKM-013]
- **Real-sampler suite** (needs the sampler; run by the owner, opt-in, never run by CI against hardware):
  `xs56k_akm_probe --suite --in "<port the sampler sends on>" --out "<port it receives on>"` (same program, same ports)
  runs seven checks, each on a session opened with `Session::open` and closed with `Session::close` — open and close,
  Echo, 50 timed Echo round trips, the OS version, checksums on and off, every setting put back, and a check that fails
  half way and must leave the sampler in the known state — writes `akm-suite-<UTC date>.log` and ends with the
  observations of RQ-AKM-017. It changes only section 00 settings, never a program, multi or sample, and ends with checksums
  off, Still Alive off, Notification on, Sync LCD on and Auto screen update off; `--no-lcd` leaves Sync LCD and Auto screen
  update alone. Two extra checks are asked for: `--power-cycle` asks you to switch the sampler off and on while a
  session is open, and `--slow-operation` sends one command outside sections 00 and 02 ("update the list of disks",
  section 10 item 01) with Still Alive on, to see whether `F0 F7` reaches the host. Observed once on an S5000
  (OS 2.14, no disk drive attached): `--slow-operation` got no reply at all, `F0 F7` included, and left the sampler
  answering no SysEx — a fresh discovery included — until it was power-cycled by hand; run `--power-cycle` on its
  own, not together with `--slow-operation`, if the point is to test persistence across a graceful restart
  (`process/2.architecture/OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`). More opt-in checks — `--program-lifecycle`,
  `--sample-lifecycle` with `--sample-name`, listed by `--help` — each put back what they change, and
  `--system-setup` adds two checks on the sampler's own settings: it reads the model and the memory, then round-trips
  the sampler's name, its four Play Modes (the Muted mode the spec's data column leaves out included, whether the
  sampler accepts it being what is observed), its front-panel lock (locked for an instant) and its clock, and puts each
  back — the lock first, the clock advanced by the time elapsed, to about three seconds — even when a check fails half
  way. It never sends section 02's Clear Sampler Memory (`&32`), which no real-sampler test may call.
  `--disk-tools` (needs `--suite`) adds one check on the disk (section 10, RQ-AKM-071): it lists the connected disks
  (`&04`, `&05`, read only), tests the writable ones (`&03`, read only) and asks the owner to choose one of the valid
  ones, which it selects (`&02`, RQ-AKM-061 — every disk operation acts on that SysEx selection, which the front panel
  does not set; nothing in section 10 clears it, so it stays selected). It creates the disposable sub-folder
  `XS56K_SUITE_TEST` under that disk's current folder, works inside it, and deletes it again through the confirmed
  `&17` guard. No usable disk, or no way to ask, skips the check before anything is selected or created. It also reads
  the selected disk's type and name by handle (`&07`, `&0E`). `--disk-tools-files` (needs `--disk-tools`) adds one
  check on the file items (RQ-AKM-068, RQ-AKM-069): it saves the test program into the sub-folder (the owner confirms
  the file on the sampler; no owner to ask, no save), then reads (`&21`, `&23`, `&24`), renames (`&28`), deletes
  (`&29`). It does not audition: `&30`/`&31` audition a sample from disk, and this check saves a program. The rename
  takes the name without its extension, since the sampler appends the file's own extension (seen on the S5000:
  `XS56K_RENAMED.AKP` given became `XS56K_RENAMED.AKP.AKP`); after it, the check lists the sub-folder (`&22`) and
  expects exactly the renamed file.
  `--disk-tools-audition` (needs `--disk-tools`) adds one check on the audition of a sample from disk (RQ-AKM-068,
  `&30`, `&31`): the owner confirms that a `.WAV` file is at the root of the selected disk; the first one found is
  started and, after 3 seconds, stopped. A stop the sampler refuses is recorded, not failed (a shorter sample may have
  ended). It plays a sound and saves nothing.
  It touches nothing that existed before. `--disk-tools-slow OP` (needs `--disk-tools`) sends one of the six long-running section 10 items inside that
  sub-folder with Still Alive on — `update-list`, `load-folder`, `load-file`, `load-file-with-dependents`,
  `save-memory-item`, `save-all-memory-items` — one per run (RQ-AKM-070). Each is documented as potentially hanging the
  sampler (`process/2.architecture/OBSERVATIONS-RQ-AKM-017-real-sampler-suite.md`, frames F4–F7): a hang needs a power
  cycle by hand, so run one only when ready for it. `load-file` and `load-file-with-dependents` send one save (`&2C`)
  first, since a file can only be made inside the sub-folder by saving; the owner then confirms on the sampler that the
  file is there (declining skips the check, and a save is never sent without a way to ask).
  `--front-panel` adds the owner-driven check of the front panel (section 20, RQ-AKM-076): the probe prints the mapping
  of PC keys to sampler keys (`juce/tests/support/include/akm/harness/FrontPanelRemote.hpp` and its `.cpp`, the one source
  of the mapping and of what is printed), you confirm that the sampler shows a screen of your choice, then every key you
  press is sent as the sampler key it stands for — F1–F8, digits, `-` `+`, cursors, Enter (ENT/PLAY), Space (ENT/PLAY held
  until the next Space), Escape (EXIT), the mode keys as letters (`m x s p r u v l w k j`), the data wheel on the up/down
  and page keys, Tab for a text mode that sends printable keys as ASCII — and nothing else is sent; `q` ends it. The keys
  act on whatever the sampler shows: SAVE, ENT/PLAY or the wheel can change or delete data on some screens, so choose the
  screen with care. Every key still held is released at the end, and by the session's close if the check fails (DEC-AKM-019).
  Windows console only: elsewhere, or when stdin is not a console, the check is skipped. Run on the real sampler by the
  owner in two runs (2026-10-04, `process/2.architecture/OBSERVATIONS-RQ-AKM-076-front-panel.md`): every Hold, Release and
  data wheel step was accepted and the owner reports the shortcuts worked; not yet pressed: Escape, `-`/`+`, the other
  digits, EDIT SAMPLE, EDIT PROGRAM, RECORD, UTILITIES, and any character in the text mode, so whether the S5000 takes
  Backspace/Enter as ASCII 8/13, or counts Holds of one key, is still open.
  `--midi-config` adds two owner-guided checks on the sampler's MIDI setup (section 04, RQ-AKM-080). Section 04 has no Get,
  so the check cannot read what the sampler holds: before anything is sent it asks you what UTILITIES > MIDI SETUP shows
  (PROGRAM CHANGE, MULTI SELECT, MULTI SLCT CH, EXT APM CONTROL, AFTERTOUCH) and, on MIDI FILTER, one filter you pick (event
  type, channel, on or off). It then changes each of those to another value, one at a time, asks you to confirm on the
  sampler's screen that it shows the new value, and puts each back to the value you declared before touching the next
  (a setting may depend on another). A "no" is noted and the other settings are still tried; the check then asks you to
  confirm the original screens are back and fails at the end naming every setting you did not see. Declining a question
  skips it. Each value is also put back when a check fails half way, which the second check provokes on purpose with
  MULTI SELECT. It changes your stored MIDI setup for a moment: a filter that ignores NoteOn silences that channel while
  it lasts. The values you declare are not verified, and a wrong declaration is put back as given (the log records it).
  The check switches Auto screen update on for its session (§00/&05; put back off at the close, left alone with `--no-lcd`)
  and, after a "no", asks you to leave the page, open it again and look once more: a screen that is not redrawn by itself
  looks like an item the sampler ignored. Observed (`process/2.architecture/OBSERVATIONS-RQ-AKM-080-midi-config.md`, third
  run, 2026-10-04): the S5000 obeys all seven items and every change was seen at once on its screen. With Auto screen update
  off (the first two runs) only `&01` and `&07` showed, so §00/&05 must be on for the sampler's pages to follow SysEx. Not
  observed: the channel code of `&06`/`&07` for port B (1B to 16B, "Port A & B", FTR-AKM-009 open points), the other filter
  event types and the effect on real MIDI input.
  `--multi-lifecycle` adds two checks on the multis (section 0C, RQ-AKM-093): it creates one program and one multi under
  the reserved names `XS56K_SUITE_TEST` and `XS56K_MULTI_TEST` (and stops without touching anything if a multi already
  bears the second), round-trips every item of the section on them (the twelve part parameters, the Gets of general
  information, the program number, the part assignment by name and by index, the renaming, the selection), then deletes both
  and selects again the multi that was current, even when a check fails half way. It never sends Delete ALL Multis (`&07`)
  and never `&01` (the number of parts of new multis, which no item reads back). Observed on an S5000 (OS 2.14) holding no
  multi, three runs (`process/2.architecture/OBSERVATIONS-RQ-AKM-093-multi.md`); with the owner's own multis in memory the
  selection of an existing one is the only part not yet run on hardware.
  `--song-files` adds two checks on the sampler's MIDI song files and set lists (section 16, RQ-AKM-085): it reads the number of
  each and every name (16 at most), selects each song file by index and by name, renames the first song file and the
  first set list and reads the new names back, then puts every name and the selection back, even when a check fails
  half way. Section 16 cannot create a song file or a set list, so it works on what the sampler holds, never deletes,
  and is skipped (after logging what an empty memory answers) when there is none. Observed once on an S5000 (OS 2.14)
  that held none: both counts 0, ERROR 4 for every item naming something
  (`process/2.architecture/OBSERVATIONS-RQ-AKM-085-song-files.md`); run it again once a MIDI song file is loaded.
  `--scenelists` adds the same two checks on the sampler's scenelists (section 14, RQ-AKM-097): it reads the number of
  scenelists and every name (16 at most), selects each by index and by name, renames the first and reads the new name
  back, then puts the name and the selection back, even when a check fails half way. Section 14 cannot create a scenelist,
  so it works on what the sampler holds, never deletes, and is skipped when there is none. Observed once on an S5000
  (OS 2.14) that held none: the section is supported, the count is 0, ERROR 4 for every item naming something
  (`process/2.architecture/OBSERVATIONS-RQ-AKM-097-scenelist.md`); run it again once a scenelist is loaded.
  `--multi-fx` adds two checks on the multis' effects (section 12, RQ-AKM-102): it creates a test multi under the reserved
  name `XS56K_MULTI_TEST` (and stops without touching anything if a multi already bears it), reads whether an FX board is
  installed (`&01`) and, with none, logs the answers of the other Gets and sends no Set (the check is then skipped); with
  an EB20 it changes the mute of channel 0, the enabled state of its module 3, its first parameter of module 2 and the
  type of module 2, putting each back, then deletes the test multi and selects again the multi that was current, even when
  a check fails half way. Observed once on an S5000 (OS 2.14) with no board
  (`process/2.architecture/OBSERVATIONS-RQ-AKM-102-multi-fx.md`): `&01`, `&10` and `&11` answer 0, and the Gets that name a
  channel and a module answer ERROR 2. The owner has no EB20, so the round trip with a board is tested on the simulated
  sampler only.
  Exit status 0 when every check
  passed or was skipped and the known state is confirmed, 2 when no sampler answered at the DeviceID, 3 otherwise. The same
  suite runs against the simulated sampler in `ctest` (tag `[suite]`). [RQ-AKM-017, RQ-AKM-018, TASK-AKM-010]
