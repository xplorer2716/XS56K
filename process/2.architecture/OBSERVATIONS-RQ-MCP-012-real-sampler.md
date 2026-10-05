# Observations — RQ-MCP-012: the MCP server on the real S5000

Not an AGNOS artifact (no ID of its own, like the other `OBSERVATIONS-*` files). What the owner's S5000 (OS 2.14, no disk
drive, no FX board, **an empty memory: no program, no sample, no multi**) answered to the MCP server, from five scripted
conversations piped into `xs56k_mcp_server` by the assistant on 2026-10-05 under the owner's authorization (ports
`MIDIIN2 (ESI M8U eX)` / `MIDIOUT15 (ESI M8U eX)`, DeviceID 0; HOL, Human on the loop). Traceability: RQ-MCP-012, RQ-MCP-022,
RQ-MCP-015, RQ-MCP-016, RQ-MCP-017, TASK-MCP-009, TASK-MCP-012 (`FTR-MCP-001`, `FTR-MCP-002`, `PLAN-MCP-002`). The
conversations and answers are in the session's scratch folder, not committed.

## The runs

Every run ended with "settings put back: checksum mode, Still Alive, Sync LCD, Auto screen update; not put back: none",
and the sampler was left as it was found: **no program in memory** (checked by `get_status` at the end).

| Run | What it did | Result |
|---|---|---|
| A | `get_status`, `create_program` MCPSCRATCH with 3 keygroups, `get_status`, `list_programs`, `get_parameters` of the five groups for all keygroups | the 54 starting values read, one per keygroup |
| sweep | for each of the 54 parameters: `set_parameter` to another value (all keygroups), read back, then set back; a Set on keygroup 2 only; the five groups read again | 116 steps, **0 failures**, final values equal to the starting ones, keygroup 2 alone changed when set alone |
| B | rename, delete with the wrong name, create `lower case_1`, delete it, delete the program, then every tool with no program | see below |
| C | create `mmm`, `aaa`, `zzz`, `BBB`; delete and rename in several places | the sampler's order and the current program after a deletion, see below |
| D | the modulation sources of codes 12 to 14, a LFO 2 clock division, then delete the scratch program | accepted and read back, see below |

## What the sampler did that the simulated sampler did not

- **The programs are kept in alphabetical order, upper and lower case alike.** `mmm`, `aaa`, `zzz`, `BBB` created in that
  order are listed `aaa`, `BBB`, `mmm`, `zzz`; `lower case_1` sorts before `MCPSCRATCH2`; a renamed program takes its new
  place (`mmm` renamed `AAA2` is first; `zzz` renamed `b first` is second). The created program is current. The simulated
  sampler appended in creation order: it now sorts (case-insensitive) on create and rename and keeps the current program the
  same program.
- **Deleting the current program makes another current.** After deleting `BBB` (second of four) `aaa` was current; after
  deleting `aaa` (first of three) `mmm` was current; after deleting `mmm` (last of three) `b first`, the one before, was
  current; after deleting `lower case_1` (first of two) `MCPSCRATCH2` was current. The rule seen: the program before the
  deleted one, or the first when none is before it; none when memory is empty. The simulated sampler left no program current:
  it now follows the rule. (Only the cases above were run; a delete in the middle of a longer list was `BBB` in four.)
- **A name keeps its case** (`lower case_1`, `b first`), a space and an underscore are accepted. The tool limits a name to 12
  characters of plain ASCII; the sampler itself was not asked for a longer one (open).
- **No current program** (memory empty): every call that reads or edits the current program, including `rename_program` and
  `delete_program` (which read its name first), got **ERROR 4** (not found), as the simulated sampler gives; the messages say
  "Is a program selected? Use select_program first."

## What was established

- **A Set with "all keygroups" selected reaches every keygroup** (the open point of FTR-MCP-001): each of the 54 parameters
  was set with the tools' default (`all`), read back for each of the three keygroups, and each reading equalled the value set
  (the tool reports a mismatch as an error, and there was none). Setting keygroup 2 alone changed keygroup 2 alone.
- **A Get with "all keygroups" selected answers one record per keygroup**, read by every group.
- **The starting values of a new program** (created by `create_program`, 3 keygroups): filter 2-POLE LP, cutoff 100,
  resonance 0, keyboard tracking 0, attenuation 0 dB, modulation sources VELOCITY, LFO2, AMP ENV with amounts 0;
  amplitude envelope attack 1, decay 50, sustain 100, release 15, the rest 0; filter envelope attack 0, decay 50, sustain 100,
  release 15, the rest 0; LFO 1 rate 43, delay 0, depth 0, waveform TRIANGLE, sync off, the three modulation sources KEYBOARD
  with amounts 0, modwheel 15, aftertouch 0; LFO 2 rate 0, delay 0, depth 0, waveform SINE, retrigger off, the three modulation
  sources NO SOURCE with amounts 0, clock sync off, clock division code 5 ("1 cycle per beat" in the catalogue's words).
- **Codes 12, 13 and 14 of the modulation sources** (named MODWHEEL 2, BEND 2, EXTERNAL 2): set on a filter modulation input
  and on LFO 1's rate modulation, each is **accepted and read back as the code set**. What they mean on the sampler's screen
  was **not** seen (no one was looking at it): the names stay provisional.
- **Clock division** codes 0 ("8 cycles per beat"), 5 ("1 cycle per beat") and 68 ("64 beats per cycle") were set and read
  back as set; whether the screen shows those words was not seen.
- **Create, rename and delete work in memory through the tools**, as simulated: `create_program` (the program is current,
  its keygroup count is the one asked), `rename_program` (the name read back), `delete_program` (the guard refused a name
  that was not the current program's, and a name differing only in case; the program count after the deletion is read).
- **The server's session** opened and closed cleanly in every run (Still Alive, Sync LCD, Auto screen update put back).

## Lot 3 of the catalogue (TASK-MCP-013), run on 2026-10-05

A program MCPSCRATCH (3 keygroups) created by `create_program`; every one of the **106** parameters of the catalogue (the 54 of
lots 1 and 2, the 52 of lot 3) read for all keygroups, then each set to another value on all keygroups, read back and set
back: **223 steps, 0 failures**, the final values equal to the starting ones. The program was then deleted and memory was
empty again.

- **Starting values of a new program, lot 3** (as the sampler gave them, not as the simulated sampler holds them): keygroup
  low note 21, high note 127 (every keygroup covers the whole keyboard), mute group 0, FX override OFF, FX send level 0, zone
  crossfade off, program keygroup crossfade **on**, keygroup semitone tune 0, fine tune 0, level 0 dB, pitch modulation 1
  source LFO1 with amount 100, pitch modulation 2 source AUX ENV with amount 0, keygroup amp modulation source VELOCITY
  with amount 0, aux envelope rates 0, 50, 50, 15 and levels 100, 100, 100, 0 (velocity and keyboard amounts 0), program
  loudness 85, velocity sensitivity 25, program amp modulation sources KEYBOARD and AFTERTOUCH, pan modulation sources LFO2,
  KEYBOARD and MODWHEEL (all amounts 0), program tuning 0 and 0 cents, tune template USER, tune key C, pitch bend up 2 and
  down 2, bend mode NORMAL, aftertouch pitch 0, legato off, portamento off, mode TIME, time 0. The simulated sampler starts a
  new keygroup with zeros for these (a note of 0 is outside the note range): a difference of state, not of behaviour, left as
  it is; the lot 3 test copes with it.
- **The keygroup level** (a code 0 to 10 for -30 to +30 dB in steps of 6) was set to -24 and back to 0 dB and read back as
  set: the catalogue's `offset` (code = (dB + 30) / 6) is right.
- **Low and high note** were set to 28 and 120 and back, accepted; setting a low note above the high one was not tried.
- **The tuning, pitch bend, portamento, aftertouch and modulation rows** all read back as set; what each does to the sound was
  not heard.

## Not established

- What the sampler's screen shows (modulation source and clock division labels, the effect of Auto screen update during edits).
- Which revision of MCP a real client opens with, and what a client that kills the server leaves on the sampler (a server
  killed instead of closed leaves Still Alive on, Sync LCD off and Auto screen update on, per ADR-MCP-001).
- A program name longer than 12 characters, or with characters beyond printable ASCII.
- Programs with the same name apart from case (two `mmm`/`MMM`): the simulated sampler refuses an identical name only.
- The sampler's reaction to `delete_program` while it holds a very long list.
