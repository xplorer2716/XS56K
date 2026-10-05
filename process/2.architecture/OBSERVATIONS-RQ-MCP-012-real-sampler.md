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

## Zones (TASK-MCP-014), run on 2026-10-05

On a new program MCPSCRATCH (3 keygroups) created by the server, the 13 zone parameters were read for every zone of every
keygroup (12 records per Get), each set to another value (all keygroups, all zones: one Set with zone 0 while keygroup 0 is
selected), read back and set back: with the other 106 parameters, **250 steps, 0 failures**, the final values equal to the
starting ones. A write on one zone of one keygroup was then checked: zone level 25 on keygroup 2 zone 3 changed that zone alone
(the 12 values read keygroup-major, zone-minor; keygroup 2 alone gave its four zones; zone 3 alone gave the three keygroups);
the velocity to start -1234 on keygroup 3 zone 4 and the pan -50 and 50 on keygroup 1 zones 1 and 2 read back as set, then every
value was put back with a Set on all zones. The program was deleted and memory was empty again.

- **Starting values of a new program's zones**: level 0, pan 0 (code 64, the centre), output MULTI, filter 0, fine tune 0,
  semitone tune 0, keyboard tracking on, playback AS SAMPLE, velocity to start 0, high velocity 127, low velocity 0, mute off,
  solo off. The simulated sampler started a zone's pan at 0 (code 0, outside the 14 to 114 range): it now starts it at the
  centre. Its other zone values (zeros) differ from the real ones above where they are not zero (keyboard tracking, playback,
  high velocity): a difference of state, left as it is.
- **The zone number goes first in every item and zone 0 means all four**, as the spec says: a Set with zone 0 reached the four
  zones of the selected keygroups, and a Get with zone 0 answered four records per keygroup (12 with all keygroups selected),
  keygroup-major, as the simulated sampler models it.
- **The velocity to start** (a sign and a magnitude of two 7-bit bytes, up to 9999) accepted -1234 and 0.
- **No sample was in memory**, so the zone sample assignment (`&01`, `&21`) was not run; it is not a parameter of the table.

## Samples (TASK-MCP-015), run on 2026-10-05 on an empty memory

The sampler held **no sample**, so the sample tools could only be run for what they say about an empty memory; every edit of a
sample's parameters is tested on the simulated sampler only.

- **The names of all samples (`&12`) answer ERROR 3 ("unknown error: the command could not be completed") when no sample is in
  memory**, not an empty REPLY and not ERROR 4 (the programs' `&19` answers ERROR 4). The count (`&10`) answers 0. The simulated
  sampler answered an empty REPLY: it now answers ERROR 3, two AKM tests that stated the empty reply were changed to say what
  the real sampler did, and the gateway asks the count first and the names only when there is a sample.
- **`select_sample` of a name or of a position with no sample** reads as "not found" (ERROR 4), as for programs.
- **Reading or setting a parameter of the current sample with none selected** answers ERROR 4 ("requested ... item could not be
  found"); the tools say "Is a sample selected? Use select_sample first."
- **Not run**: any read or write of a sample's parameters (positions of four 7-bit bytes, original pitch, tunes, playback mode,
  type, channels, length, rate), the `domain` argument beyond its text, and the loop-end-moves-loop-start quirk of
  `OBSERVATIONS-RQ-AKM-051-sample-loop-points.md` through the tools. They need a sample in memory (the owner's memory held none).

## Multis (TASK-MCP-016), run on 2026-10-05 on an empty memory

The sampler held **no multi**, and no MCP tool creates one, so the multi tools could only be run for what they say about an empty
memory; every read and write of a part's parameters is tested on the simulated sampler only. What stands on the real S5000 is the
AKM layer's own run of the same items (`OBSERVATIONS-RQ-AKM-093-multi.md`: every §0C item obeyed on part **3 sent as 3**, setting a
part's solo clears its mute, `&44` answers 31 for 32 parts).

- **With no multi**: `list_multis` says "The sampler holds no multi." (the count `&40` is asked first; `&51` also reads as an empty
  list per the AKM run), `select_multi` of a name or a position reads as "not found" (ERROR 4), and reading or setting a part
  parameter with no current multi answers ERROR 4 while the tools count the multi's parts, with "Is a multi selected? Use
  select_multi first."
- **Assumption not observed: the part numbering.** The tools number parts from 1 and send the part minus one (spec Table 16: the
  part number is 0 to 127); whether the S5000's front panel calls the wire's part 0 "part 1" is not known (the AKM run only
  used wire part 3). To observe with a multi in memory.
- **Not run**: every part parameter (MIDI channel 1A to 16B as codes 0 to 31, output, pan over the codes 14 to 114, fine tune
  over 0 to 100, transpose over 0 to 72, notes), a Set on all parts (one command per part), and the quirk that a part's solo
  clears its mute through the tools.

## The disk tools (TASK-MCP-023), run on 2026-10-05 with the owner present — partly

The owner's S5000 holds a SCSI2SD as its hard disk. Memory empty at the start (no program, sample or multi). Server built from the
branch and launched with `--allow-disk`; one slow command per call, the owner watching. Calls sent through a scripted client
(one session per run).

| Tool | What the sampler did |
|---|---|
| `list_disks` (no refresh) | two disks at once, in 0.0 s: handle 128 "No disk" (floppy, format 8, writable) and handle 129 "S5K" (hard disk, FAT32, writable). The refresh was not needed to see the SCSI2SD |
| `list_disk_contents` with no disk selected | an error: "selected disk is invalid (error 257)" (the simulated sampler answered error 4) |
| `select_disk` (by name `S5K`) | accepted in 0.8 s; the current folder is the root |
| `list_disk_contents` at the root | 0.2 s: 8 folders (`System Volume Information`, `DrumsLoops`, `Soul - Funky - Acid jazz`, `Rap`, `DiscoLoops`, `MoogBass`, `AKWF`, `Rhodes Attacks By Ueberschall`) and 4 files (`New Program  1.AKP` 516 bytes, `X01.AKP` 516 bytes, `S1.WAV` 40264748 bytes, a 35555372-byte `.wav`). File names keep their case and their extension; a name can hold spaces and parentheses |
| `load_file` `X01.AKP` | 0.3 s. The memory went from no program to one, `X01` (one keygroup), made current; the program is named without its extension |
| `load_file` `S1.WAV`, `sample_mode` ram | **60.4 s** for 40 MB (well inside the 120 s disk timeout), then one sample `S1` (the name without `.WAV`), current: stereo, 44100 Hz, 10066176 points, type RAM |
| **`list_disks` with `refresh: true`** | **no reply within 120 s, and the next command got none within 2 s: the sampler answered nothing until the owner switched it off and on.** The server could not put its session settings back (checksum, Still Alive, Sync LCD, Auto screen update) and said so. The owner says this is linked to the SCSI2SD and had said it before; the refresh was sent by this session against that knowledge (the plan listed it), which is why the refresh now needs its own launch option (DEC-MCP-020, TASK-MCP-025) |

After the power cycle the memory was empty again (the program and the RAM sample are gone, as expected) and the disk list, the
selection and the browsing worked as before.

**The sample tools on a real sample** (the first time: S1 loaded from the disk). The defaults of a loaded stereo sample: start 0,
end = length (10066176), loop start 1, loop end 10066161 (length minus 15), playback mode NO LOOPING, original pitch 60, semitone
tune 0, fine tune 0. All eight editable parameters were set to another value, read back as set, and set back (start, end, loop
start, loop end, playback mode, original pitch, semitone tune, fine tune); a Set of a read-only row (`sample length`) is refused
with nothing sent. One quirk, as the catalogue already says: setting the loop end to 10000000 left the loop start at **16**, not 1,
and putting the loop end back and the loop start to 1 restored both. Nothing was saved to the disk, S1.WAV is unchanged.

**Established:** browsing the SCSI2SD, selecting it, loading a program and a very large sample, reading and setting every
parameter of a real sample. **Not run:** `load_folder`, `load_file` with dependents, `save_memory_item`, `save_all_memory_items`
(no overwrite, then overwrite), the multi tools (no multi on the disk), and the refresh, which must not be run again on this
sampler. Things the simulated sampler does differently and that are still to correct after the saves: the error code with no disk
selected (257), the file names kept as on the disk, and the time of a load of a large file.

## Not established

- What the sampler's screen shows (modulation source and clock division labels, the effect of Auto screen update during edits).
- Which revision of MCP a real client opens with, and what a client that kills the server leaves on the sampler (a server
  killed instead of closed leaves Still Alive on, Sync LCD off and Auto screen update on, per ADR-MCP-001).
- A program name longer than 12 characters, or with characters beyond printable ASCII.
- Programs with the same name apart from case (two `mmm`/`MMM`): the simulated sampler refuses an identical name only.
- The sampler's reaction to `delete_program` while it holds a very long list.
