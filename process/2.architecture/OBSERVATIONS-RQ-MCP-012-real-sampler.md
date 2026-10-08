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

The saves and the loads with dependents (second part of the run, after the power cycle, same disk, the owner present):

| Tool | What the sampler did |
|---|---|
| `save_memory_item` program `MCPSAVETEST` (created for the run, 1 keygroup), no `overwrite` | 1.9 s. The file `MCPSAVETEST.AKP` appeared at the root, **516 bytes** (the same size as the owner's `X01.AKP`; the simulated sampler gives 4096), found by the listing the tool makes afterwards. The name is the program's name plus `.AKP`, in the program's case |
| the same save again, no `overwrite` | refused by the tool before anything was sent (0.3 s): the folder already holds `MCPSAVETEST.AKP`, "Pass overwrite true to replace it" |
| the same save with `overwrite: true` | 0.7 s, the file replaced, still 516 bytes, one file of that name in the folder, the other files untouched |
| `load_file` `X01.AKP` with `with_dependents` | 0.4 s, accepted: program X01 added to the one in memory (programs 2, was 1) and no sample added. **This showed nothing about the dependents**: whether X01 refers to any sample is not known (516 bytes). Programs in memory are listed in alphabetical order whatever the order they were loaded (MCPSAVETEST, X01) |
| `load_file` `MCPSAVETEST.AKP` with `with_dependents`, memory empty (2026-10-06) | **1.0 s: programs 1 (was 0, added MCPSAVETEST) and samples 1 (was 0, added Al_Jarreau-Flame)**, the sample being the one the owner had associated to the program on the sampler and saved with it (the owner cleared the memory before the run; `Al_Jarreau-Flame.WAV`, 368084 bytes, is at the root of the disk). The dependents are loaded, the sample named as its file without the extension. Not run: the same file without `with_dependents` (the control that shows the flag is what loads the sample) |

Browsing and loading a folder (2026-10-06, the owner present; after a first browse of seven folders at once, piped into `head` and
killed half way, the sampler stopped answering until the owner switched it off and on, cause not established):

| Tool | What the sampler did |
|---|---|
| `open_folder` `AKWF` | 0.6 s: 65 sub-folders (`AKWF_0001` ... `AKWF_violin`, in the disk's order) and no file |
| `open_folder` of a sub-folder of `AKWF` | 0.8 to 1.2 s; `AKWF_theremin` 26 files, `AKWF_oboe` 13, `AKWF_clarinett` 25. **The current path below the root is written with a backslash: `AKWF\AKWF_theremin`** (the simulated sampler joined the names with `/`) |
| `close_folder` | 0.5 s, back in `AKWF` with its listing |
| `load_folder` `AKWF_oboe` (from `AKWF`) | **1.4 s: samples 14 (was 1), the 13 files added as `AKWF_oboe_0001` to `AKWF_oboe_0013`** (the names of the files without `.WAV`); the memory lists them in alphabetical order with `Al_Jarreau-Flame` after them, and the first of the folder is the current sample. Programs unchanged (1 was 1) |

`create_folder` and the saves of samples (2026-10-06, the owner present; memory emptied by the owner's restart, then `AKWF_oboe` loaded again: 13 samples):

| Tool | What the sampler did |
|---|---|
| `create_folder` `MCPTEST` at the root | 2.1 s, accepted; the root then lists 9 folders; the new folder is empty. Two more, `MCPTEST2` and `MCPTEST3`, were created the same way |
| `save_memory_item` sample `AKWF_oboe_0001` in `MCPTEST` | 0.4 s: **a sample is saved as a `.WAV` file**, `AKWF_oboe_0001.WAV`, 1376 bytes (the simulated sampler had written `.AKS`, an extension that is in no spec and that the owner never mentioned: corrected) |
| `save_all_memory_items` samples, `confirm` 12 (13 in memory) | refused by the tool, nothing sent |
| the same with `confirm` 13, no `overwrite`, `AKWF_oboe_0001.WAV` already in the folder | refused by the tool, nothing sent, names the existing file |
| the same with `overwrite` | 2.0 s, accepted. **The listing read right after showed 1 file; the folder holds 13** (checked by listing it again in a new session). The tool therefore said "the folder gained no new file", which was wrong |
| `save_all_memory_items` in an empty folder (`MCPTEST2`) | 2 s, the listing right after showed all 13 files |
| the same scenario as the first (`MCPTEST3`: one file saved, then the bulk save with `overwrite`) | the listing stayed at 1 file right after, **and 5 s and 17 s later**; it showed 13 only after `close_folder` and `open_folder` of the same folder. **The sampler caches a folder's file list, and a bulk save into a folder that already holds a file does not refresh it** (an empty folder was refreshed). To fix in the tool: after a save whose files are missing from the listing, close and reopen the folder and list again |

Left on the owner's disk by the run, to delete by hand (no tool deletes): `MCPSAVETEST.AKP` at the root, and the folders `MCPTEST`,
`MCPTEST2` and `MCPTEST3` with their 13 `.WAV` files each. `Al_Jarreau-Flame.WAV` at the root is the owner's.

The listing of a folder is **not alphabetical** (the order of the FAT directory: `New Program  1.AKP`, `MCPSAVETEST.AKP`, `X01.AKP`,
`S1.WAV`, the `.wav` of 35 MB): the tools give it as the sampler does. The two test programs were then deleted from memory with
`delete_program`. **`MCPSAVETEST.AKP` is still on the owner's disk (root of S5K): the owner deletes it by hand**, no tool deletes a file.

After the power cycle the memory was empty again (the program and the RAM sample are gone, as expected) and the disk list, the
selection and the browsing worked as before.

**The sample tools on a real sample** (the first time: S1 loaded from the disk). The defaults of a loaded stereo sample: start 0,
end = length (10066176), loop start 1, loop end 10066161 (length minus 15), playback mode NO LOOPING, original pitch 60, semitone
tune 0, fine tune 0. All eight editable parameters were set to another value, read back as set, and set back (start, end, loop
start, loop end, playback mode, original pitch, semitone tune, fine tune); a Set of a read-only row (`sample length`) is refused
with nothing sent. One quirk, as the catalogue already says: setting the loop end to 10000000 left the loop start at **16**, not 1,
and putting the loop end back and the loop start to 1 restored both. Nothing was saved to the disk, S1.WAV is unchanged.

**Established:** browsing the SCSI2SD, selecting it, loading a program (with and without dependents) and a very large sample,
loading a folder (13 samples in 1.4 s), saving a program (refused without `overwrite` when the file exists, replaced with it), the load with dependents of a program that refers to a sample (the sample is loaded too) and reading and setting every parameter of a
real sample. The load without `with_dependents` as a control, `save_all_memory_items`, the save of a sample and of a multi and `save_children` were run
later the same day (tables above and below); the multi tools were run in TASK-MCP-037. **Not run:** the refresh, which must not be run again on this sampler.
Things the simulated sampler did differently:
the error code with no disk selected (257 against 4), the path format below the root (`AKWF\AKWF_oboe` against `AKWF/AKWF_oboe`), the size of a saved program (516 bytes against
4096): **corrected in TASK-MCP-041** (2026-10-06), as are the sizes of a saved sample and multi. The order of a folder listing (the disk's, not alphabetical) needed
no correction: the simulated sampler lists files in the order they were added and replaces an overwritten file in place, and does not sort. The time of a load of a
large file (60 s on the real sampler) is different and **accepted** by the owner (2026-10-07): it changes no logic.

## The tools of ADR-MCP-004 (TASK-MCP-037), run on 2026-10-06 with the owner present

The server built from the branch, launched with `--allow-disk` (screen mode `independent`, the default), one tool at a time through a
scripted client, the output written to files. The refresh of the disk list was never sent; the sampler never stopped answering. The
objects were made for the run: a program `MCPTEST1`/`MCPTEST2`, a multi `MCPMULTI*`, the folder `MCPTEST` of the owner's disk (the
owner had deleted the earlier `MCPTEST*` folders and `MCPSAVETEST.AKP` by hand between the runs). Everything created was deleted
again by the tools: the disk root is back to its 8 folders and 5 files, the memory holds the 12 `AKWF_oboe` samples loaded earlier
and nothing else.

| Tool | What the sampler did |
|---|---|
| `get_system_info` | `AKAI S5000`, operating system 2.14, free wave memory 99 % (158324678 bytes of 158548694), free program, keygroup, sample and multi memory 99 % |
| `add_keygroups` | 1 keygroup added to a 1-keygroup program: 2, in 0.0 s |
| `set_zone_sample`, `get_zone_samples` | `AKWF_oboe_0001` on zone 1 and `AKWF_oboe_0002` on zone 2 of keygroup 2, each read back by the sampler (0.2 s); the other zones "no sample"; **the same sample can play in two zones** (`AKWF_oboe_0002` on zones 2 and 3) |
| `delete_keygroup` | a wrong `confirm` is refused with nothing sent; keygroup 1 of 2 deleted with the program's name: **the former keygroup 2 became keygroup 1 with its zones** (the numbers after a deleted keygroup move down by one); the last keygroup is refused by the tool before anything is sent (what the sampler itself does was not tried) |
| `rename_sample`, `delete_sample` | `AKWF_oboe_0013` renamed `MCPREN`: it stays at position 12 (the memory order is alphabetical); a name another sample bears is refused by the tool; a wrong `confirm` refused; deleted: 12 samples left and **no sample is selected afterwards** |
| `audition_sample` | start and stop accepted (the stop followed the start at once; whether anything was heard was not checked at first; **heard, and the stop cuts the sound: see "Audition heard" below**) |
| `create_multi`, `rename_multi`, `delete_multi` | a multi is created with **32 parts** and is current; renamed; a wrong `confirm` refused; deleted |
| `set_part_program`, `get_part_programs`, `clear_part`, `set_multi_program_number` | by name and by position, read back (the same program on two parts is allowed); the program number 5 and then `null` read back; a wrong `confirm` for `clear_part` refused, then the part cleared |
| `get_multi_parameters`, `set_multi_parameter` | **all 12 part parameters of part 1 of a real multi** set to another value, read back as set, and put back (mute, solo, level, output, pan, effects channel, FX send, MIDI channel, fine tune, transpose, low note, high note); the defaults of a new multi's part: level 100, output OP1/2, pan 0, effects OFF, MIDI channel 1A, low note 21, high note 127 |
| `get_disk_space` | **"0 bytes free" for the disk `S5K`** (FAT32 on a SCSI2SD), which cannot be true since files were written to it: the S5000 does not seem to report the free space of this disk (TASK-MCP-040 makes the tool say so) |
| `audition_file` | start and stop of a `.WAV` file accepted |
| `rename_file` of a `.WAV` | `MCPREN` given: the file became `MCPREN.WAV`, **the sampler appends the extension as it does for a `.AKP`** (0.6 s); renamed back |
| `rename_folder` | `MCPTEST` to `MCPTESTB` and back, 0.8 s each, the folder kept its place in the listing |
| `delete_file` | a wrong `confirm` refused with nothing sent; the file deleted (0.3 s) |
| `delete_folder` | refused while the folder held a file ("1 item (1 file, 0 folders)", 1.0 s: the gateway opens it to count); deleted with `delete_contents` (1.8 s for 1 file, 2.2 s for 4 files) |
| `save_memory_item` with `save_children` | the program `MCPTEST1.AKP` (516 bytes) and **the two samples it uses**, `AKWF_oboe_0001.WAV` and `AKWF_oboe_0002.WAV` (1376 bytes each), were written |
| `save_memory_item` of a multi | `MCPMULTI3.AKM`, 2354 bytes: **the extension of a multi's file is `.AKM`**, as the simulated sampler assumed |
| `load_file` control | the program `MCPTEST1.AKP` loaded **without** `with_dependents`: samples 10, was 10; **with** it: samples 12, was 10, `AKWF_oboe_0001` and `AKWF_oboe_0002` added; the multi `MCPMULTI3.AKM` loaded: multis 1, was 0 |

Not run: the sampler's own answer to deleting the last keygroup of a program (the tool refuses first), and the numbering of the
parts against the front panel (the tools send the part minus one).

### The three `--screen` modes against the screen (2026-10-06, the owner at the sampler)

Two test programs, `MCPSCR1` and `MCPSCR2`, one keygroup each. Before each run the owner selected `MCPSCR1` on the front panel and
opened the filter page showing the cutoff (100). One server per mode, the same calls: `select_program` of `MCPSCR2`, `filter cutoff`
set to 60, 8 s pause, 40, 8 s pause, 100. Every call answered and read back correctly in all three runs; the owner watched the screen
and said what they saw. Both test programs were deleted afterwards (the sampler holds no program again).

| `--screen` | What the owner saw | As designed? |
|---|---|---|
| `independent` (default) | the screen stayed on `MCPSCR1`, its cutoff stayed at 100, nothing moved | yes: the assistant's selection does not move the owner's screen |
| `follow` | the screen went to `MCPSCR2` and the owner saw the whole sequence 60, 40, 100 | yes: the screen follows the assistant |
| `as-is` | the screen stayed on `MCPSCR1`, nothing changed | consistent with the sampler being left as the previous close put it; the run does not say which of the two settings was responsible, because the state of the sampler at that moment was not read (§00 has no Get) |

### Audition heard (2026-10-06, the owner listening)

The sample `AKWF_oboe` files are far too short to judge, so the test used files of the `DiscoLoops` folder; every call was accepted.

| What was run | What the owner heard |
|---|---|
| `audition_file` of `004_drumloop7.wav` (385322 bytes, about 4.4 s), start, 5 s, stop | the sample played, to its end (it was shorter than the wait: this did not show that the stop works) |
| `audition_file` of `Taste Of Honey - Boogie Ooggie - Loop.wav` (1366822 bytes, about 15 s), start, 3 s, stop | the sound started after the command, then **stopped about 3 s later**: the stop works. The start answered after 3.4 s (the sampler reads the file first) |
| `load_file` of that file (5.0 s, 13 samples then), `select_sample`, `audition_sample` start, 3 s, stop | the sound started at once (the start answered in 0.1 s) and **stopped about 3 s later** |

The loaded sample was then deleted with `delete_sample` (12 samples again). The free wave memory was 158341622 bytes before the load, 156156442 with the sample, 157555190 after the deletion: 786432 bytes were not given back (the reason is unknown; not looked into).

### Numbering of the multi parts, and `follow` from a page that is not the multi's (2026-10-06, the owner at the front panel)

A program `MCPPART` and a multi `MCPMULTI5` (32 parts) were made for the run. `set_part_program` put `MCPPART` on part 1 (read back as part 1),
then, in a server run with `--screen follow` that selected the multi and then set part 5, on part 5. With the sampler's screen on the
file-system page, **the screen did not move to the multi** when the multi was selected: `follow` makes the screen follow the selection
within the kind of page that is shown (the program page followed the program in the `--screen` run above), it does not change page
(a reading of the two runs, not tested apart). With the multi's part page open, the owner saw `MCPPART` on **part 1 and part 5**: **the part
number the tools send is the number the front panel shows** (the tools send the part minus one on the wire). Both objects were deleted
afterwards (no program, no multi in the sampler).

Afterwards the two programs were deleted through a server run with `--screen as-is`; the sampler answered that it held no program, but
the owner's screen still showed `MCPSCR1` (a stale display, not the memory: the screen is not redrawn after a change made over MIDI
when the server leaves the settings alone).

What this does not show: that the two settings are put back to the standard values when a server closes (the owner did not look at the
settings pages), and what `independent` shows when the owner's screen is on the program the assistant edits (the case the README
describes: "you see a change when the screen shows the program the assistant is editing").

## Not established

- What the sampler's screen shows (modulation source and clock division labels, the effect of Auto screen update during edits).
- Which revision of MCP a real client opens with, and what a client that kills the server leaves on the sampler (a server
  killed instead of closed leaves Still Alive on, Sync LCD off and Auto screen update on, per ADR-MCP-001).
- A program name longer than 12 characters, or with characters beyond printable ASCII.
- Programs with the same name apart from case (two `mmm`/`MMM`): the simulated sampler refuses an identical name only.
- The sampler's reaction to `delete_program` while it holds a very long list.

## The sizes of saved files and the time of a deletion (TASK-MCP-042, TASK-MCP-043), run on 2026-10-07

The owner's S5000 (OS 2.14, memory empty) and its SCSI2SD disk, the owner present; the real `xs56k_mcp_server` with `--allow-disk`,
driven one tool call at a time by a script that times each answer (the refresh of the disk list never sent, no call piped into
anything that can end early). It was run to read real figures after the code and the tests of TASK-MCP-042 and TASK-MCP-043 had
been written with figures whose source was not in this file. Test objects only: the folders `MCPSIZE` and `MCPDEL`, the programs
`MCPSZ1`, `MCPSZ2`, `MCPSZ3`, `MCPSZ4`, `MCPSZ10`, and two samples loaded from the disk; all deleted at the end by the tools
(`delete_folder` with `delete_contents`, `delete_program`, `delete_sample`, each with the exact name), the sampler left with no
program and no sample and the root of the disk as it was found.

| What | What the sampler did |
|---|---|
| `save_memory_item` of a program, **1, 2, 3, 4 and 10 keygroups** | `.AKP` files of **516, 868, 1220, 1572 and 3684 bytes**: 164 bytes plus 352 per keygroup, for the five counts |
| `save_memory_item` of a **mono sample of 616 points** (`AKWF_oboe_0001`, loaded from a 1344-byte file) | `AKWF_oboe_0001.WAV` of **1376 bytes** = 144 + 2 x 616 |
| `save_memory_item` of a **stereo sample of 91985 points** (`Al_Jarreau-Flame`, loaded from a 368084-byte file) | `Al_Jarreau-Flame.WAV` of **368084 bytes** = 144 + 2 x 2 x 91985: the same size as the file it was loaded from |
| the order of the files of a folder | the order they were written, not alphabetical: `MCPSZ1.AKP`, `MCPSZ3.AKP`, `MCPSZ10.AKP` (saved in that order), and in the second folder `MCPSZ1`, `MCPSZ2`, `MCPSZ3`, `MCPSZ4`, `MCPSZ10`, the two samples; an overwrite was not tried here |
| `delete_folder` of `MCPDEL` (7 files: 5 programs, 2 samples) with `delete_contents` | **2.67 s** for the whole tool call ("the 7 items it held"). The call opens the folder first to count (an `open_folder` takes about 0.4 s), so the deletion itself took about 2.2 s, above the 2 s of an ordinary command |
| `delete_folder` of `MCPSIZE` (5 files) with `delete_contents` | 2.37 s for the whole call (earlier runs: 1.8 s for 1 file, 2.2 s for 4 files) |

What this changes: the figures of TASK-MCP-043 are now measured, not only written in a comment (the multi stays at the one size seen,
2354 bytes for 32 parts). The 144-byte header of a saved sample holds for a mono and a stereo sample of very different lengths. The
claim in a test comment that the source file of a loaded sample is "100 bytes shorter than its saved copy" is **not** what was seen:
the oboe file is 32 bytes shorter than its saved copy (1344 and 1376) and the stereo file is the same size as its copy (368084). Left
as it was, after the run: the free wave memory read 99 % afterwards (158131378 of 158548694 bytes) with no sample in memory, bytes not
given back after the deletions, as already noted for 786432 bytes in TASK-MCP-037.
