# XS56K MCP server — control an AKAI S5000/S6000 from an AI assistant

This program lets an AI assistant (Claude Code, or any other [MCP](https://modelcontextprotocol.io) client) work on your sampler
through MIDI, in plain musical words: *"set the filter cutoff to 80"*, *"load the sample S1 and play it on zone 1 of the first
keygroup"*, *"build a multi with BASS on part 1 and LEAD on part 2"*, *"save the program"*.

**What you need**
- an AKAI S5000 or S6000 connected to the computer by MIDI (SysEx must reach it: use the ports the sampler is on);
- an MCP client that can start a program and talk to it on its standard input and output;
- the program `xs56k_mcp_server` (built with the rest of the project, see [Build](#build)).

**What it does not do:** it does not record, edit audio, format a disk, eject a disk or touch the files of your computer. It works on
what is in the **sampler's memory**, and — only if you ask for it — on the **sampler's own disks**.

## Contents
1. [Start it](#1-start-it)
2. [The launch options](#2-the-launch-options)
3. [Which tools need which option](#3-which-tools-need-which-option)
4. [The tools](#4-the-tools)
5. [Safety: what asks for a confirmation, what is never done](#5-safety)
6. [What has been tried on a real sampler](#6-what-has-been-tried-on-a-real-sampler)
7. [When something goes wrong](#7-when-something-goes-wrong)
8. [For developers](#8-for-developers)

---

## 1. Start it

**Find the MIDI ports of the sampler**

```
xs56k_mcp_server --list-ports
```

It prints the inputs (what the sampler sends to the computer) and the outputs (what the computer sends to the sampler).

**Tell your MCP client how to start the server.** The ports are part of the server's configuration: they are written in the
arguments of the server's entry in the client's own settings. With Claude Code:

```
claude mcp add xs56k -- <path to>/xs56k_mcp_server --in "MIDIIN2 (ESI M8U eX)" --out "MIDIOUT15 (ESI M8U eX)"
```

(or the same `command` and `args` in the client's settings file; that file is yours, do not commit it). To let the assistant load and
save files on the sampler's disks, add `--allow-disk` to those arguments (see the next section).

**You do not start the server yourself**: the client starts it, and stops it by closing its standard input. On start the server
opens a session with the sampler at the first call that needs it (so the list of tools works with the sampler switched off). It
switches the sampler's *Still Alive* on and, depending on `--screen`, its *Sync LCD* and *Auto screen update* (see the next
section). When the client closes the server it puts what it changed back to the sampler's **standard values** (Still Alive off, Sync
LCD on, Auto screen update off), **not to what you had set**. A server that is killed instead leaves them changed until the sampler is
switched off and on.

**To try the tools without a sampler**, run `xs56k_mcp_server_simulated` instead (built with the tests): the same server over a
simulated sampler holding three programs, three samples and two multis, and two disks when started with `--allow-disk`.

### Build

```
cmake -S juce -B juce/build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build juce/build --target xs56k_mcp_server
```

The program is `juce/build/mcp/xs56k_mcp_server` (in a `Debug` folder with Visual Studio). On Windows, a running server locks its
file: build into another folder with `cmake --build juce/build --target xs56k_mcp_server -- "/p:OutDir=<folder>\"` to run a fresh copy.

---

## 2. The launch options

Each row says what happens **if you leave the option out** and **if you give it**.

| Option | If you leave it out | If you give it |
|---|---|---|
| `--in "<port>"` | the server refuses to start: **required** | the MIDI input port, i.e. what the sampler sends to the computer |
| `--out "<port>"` | the server refuses to start: **required** | the MIDI output port, i.e. what the computer sends to the sampler |
| `--device-id <0-31>` | the sampler's DeviceID is taken to be `0` | the DeviceID you set on the sampler |
| `--timeout-ms <ms>` | an ordinary command waits `2000` ms for the sampler's answer | that many ms (1 to 60000) |
| `--screen <mode>` | the same as `--screen independent` | `independent`, `follow` or `as-is`: how the sampler's screen behaves while the server runs (see below) |
| `--allow-disk` | no disk tool exists: the server can neither read nor write a disk | the **16 disk tools** exist: browse the sampler's disks, load, save, rename, delete, create folders, free space, audition files |
| `--allow-disk-refresh` | `list_disks` cannot ask the sampler to refresh its list of disks (**recommended**: it hung the owner's S5000 until it was switched off and on) | `list_disks` has a `refresh` argument that does it. Only with `--allow-disk` |
| `--disk-timeout-ms <ms>` | a slow disk command (a load, a save) waits `120000` ms; after that the sampler may have to be switched off and on, and nothing is retried | that many ms (1 to 1800000) |
| `--list-ports` | the server starts normally | prints the MIDI ports and exits |
| `--help` | the server starts normally | prints the options and exits |

**What `--screen` chooses.** Two settings of the sampler decide how its front panel reacts to what the assistant does: *Sync LCD*
(whether the sampler's screen and the selection made over MIDI follow each other) and *Auto screen update* (whether the sampler
redraws its screen when it processes an edit received over MIDI).

| `--screen` | Sync LCD | Auto screen update | In plain words |
|---|---|---|---|
| `independent` (the default) | off | on | the assistant works on **its own selection** and does not move your screen; the screen is redrawn after each edit, so you see a change when the screen shows the program (or sample, or multi) the assistant is editing |
| `follow` | on | on | the screen **follows the assistant's selection**, and what you select on the sampler becomes the assistant's selection: you see everything it does, but your own navigation on the front panel changes what it edits |
| `as-is` | not touched | not touched | the server sends nothing about these two settings: they stay as you left them (the old `--no-lcd`) |

When the server stops it puts the settings it changed back to the sampler's **standard values** (Sync LCD on, Auto screen update off),
not to what you had set; with `as-is` it changed nothing, so it puts nothing back. On the S5000 the pages do not follow the edits
with Auto screen update off (seen on 2026-10-04). What the screen shows in each of the three cases has not yet been observed on a
real sampler; it is part of the planned real run.

`--allow-disk-refresh` without `--allow-disk` is an error. With none of the two, the server never touches a disk.

---

## 3. Which tools need which option

| Group of tools | Tools | Needs |
|---|---|---|
| Status and information | 3 | nothing |
| Programs, keygroups and zones | 11 | nothing |
| Samples | 7 | nothing |
| Multis | 11 | nothing |
| **Disks and files** | **16** | **`--allow-disk`** |
| The refresh of the disk list | an argument of `list_disks` | `--allow-disk` **and** `--allow-disk-refresh` |

That is **32 tools without any option and 48 with `--allow-disk`**. A disk tool that is not enabled is absent from the server's list of
tools, and calling it is an error.

**Tiers.** Every tool tells the client what it does to the sampler, in its MCP annotations:

| Tier | Meaning | Examples |
|---|---|---|
| **read** | changes nothing | `get_status`, `list_samples`, `list_disk_contents` |
| **change** | changes a value, a selection, a name, or adds something | `set_parameter`, `select_program`, `create_multi`, `load_file` |
| **destructive** | deletes or replaces something: always asks for a `confirm` (or `overwrite`) | `delete_program`, `delete_file`, `save_memory_item` |

---

## 4. The tools

Values are in the sampler's own units (0 to 100 for most parameters), signed values are plain signed numbers, and choices are
named as the sampler's screen names them (`"2-POLE LP+"`, `"10B"`). Every value that is set is read back from the sampler and
reported as the sampler holds it. Zones are numbered 1 to 4, keygroups and multi parts from 1, and positions in lists from 0.

In the tables, **\*** marks a required argument. **Confirm** says which exact text a destructive tool needs.

### 4.1 Status and information (no option)

| Tool | What it does | Arguments |
|---|---|---|
| `get_status` | whether the sampler answers, how many programs it holds, which one is current and how many keygroups it has | none |
| `get_system_info` | the model (S5000 or S6000), the operating system version, the free wave memory (percent and bytes: check it before loading a large sample) and the free program, keygroup, sample and multi memory | none |
| `list_parameters` | the names of the parameters, what each accepts, and what it does | `domain` (`program`, `sample` or `multi`), `group` (to narrow the list) |

### 4.2 Programs, keygroups and zones (no option)

A program has 1 to 99 keygroups; a keygroup has 4 zones, each of which plays a sample.

| Tool | What it does | Arguments | Confirm |
|---|---|---|---|
| `list_programs` | the programs in memory, with their positions (the sampler keeps them in alphabetical order) | none | |
| `select_program` | makes a program current | `name` or `index` | |
| `create_program` | creates a program and makes it current | `name`\*, `keygroups`\* (1 to 99) | |
| `rename_program` | renames the current program | `name`\* | |
| `delete_program` | **deletes the current program** | `confirm`\* | its exact name |
| `get_parameters` | reads parameters of the current program (the filter, the envelopes, the LFOs, the keygroup's options, the zones' settings...) | `group` or `parameters`\*, `keygroup`, `zone` | |
| `set_parameter` | sets one parameter, for one keygroup (or all) and one zone (or all) | `parameter`\*, `value`\*, `keygroup`, `zone` | |
| `add_keygroups` | adds empty keygroups to the current program (up to 99 in all) | `count`\* | |
| `delete_keygroup` | **deletes one keygroup** of the current program, with its zones; never the last one | `keygroup`\*, `confirm`\* | the program's exact name |
| `set_zone_sample` | makes a zone of a keygroup play a sample that is in memory | `sample`\*, `zone`\* (1 to 4), `keygroup`\* | |
| `get_zone_samples` | the sample each zone plays, for one keygroup or all | `keygroup` | |

`get_parameters` moves the sampler's keygroup selection. `list_parameters` gives the 119 program parameters.

### 4.3 Samples (no option)

A sample can only come from a disk (`load_file`) or be recorded on the sampler: no tool creates one. All the sample tools act on the
**current** sample.

| Tool | What it does | Arguments | Confirm |
|---|---|---|---|
| `list_samples` | the samples in memory, with their positions | none | |
| `select_sample` | makes a sample current | `name` or `index` | |
| `get_sample_parameters` | the current sample's parameters: start, end, loop start and end, playback mode, original pitch, tunings; and, read-only, its type, channels, length and rate | `group` or `parameters` | |
| `set_sample_parameter` | sets one parameter of the current sample | `parameter`\*, `value`\* | |
| `rename_sample` | renames the current sample (1 to 20 characters; a name another sample bears is refused) | `name`\* | |
| `delete_sample` | **deletes the current sample from memory** (it is lost unless it is on a disk) | `confirm`\* | its exact name |
| `audition_sample` | plays the current sample, or stops it (a started audition plays until stopped) | `action`\* (`start` or `stop`) | |

### 4.4 Multis (no option)

A multi has 32, 64 or 128 parts; each part plays one program. The multi tools act on the **current** multi.

| Tool | What it does | Arguments | Confirm |
|---|---|---|---|
| `list_multis` | the multis in memory, with their positions and the current one's number of parts | none | |
| `select_multi` | makes a multi current | `name` or `index` | |
| `create_multi` | creates an empty multi and makes it current | `name`\* (1 to 20 characters) | |
| `rename_multi` | renames the current multi | `name`\* | |
| `delete_multi` | **deletes the current multi from memory** | `confirm`\* | its exact name |
| `get_multi_parameters` | the settings of the parts (level, pan, channel, mute, solo...) | `group` or `parameters`, `part` | |
| `set_multi_parameter` | sets one setting of one part (or all) | `parameter`\*, `value`\*, `part`\* | |
| `set_part_program` | makes a part play a program that is in memory | `part`\*, then `program` (its name) **or** `position` (its place in `list_programs`) | |
| `get_part_programs` | which parts play a program, and which | none | |
| `clear_part` | **removes the program of a part** (the multi stays) | `part`\*, `confirm`\* | the multi's exact name |
| `set_multi_program_number` | sets the program number (1 to 128) by which a MIDI program change selects the multi, or switches it off with `null` | `number`\* | |

### 4.5 Disks and files (`--allow-disk`)

These tools act on the **sampler's own disks** (hard disk, floppy, CD-ROM, removable), not on the files of your computer. They work
on a *current disk* and a *current folder* that the sampler itself keeps: `select_disk` chooses the disk (at its root), `open_folder`
and `close_folder` move down and up.

**Browsing and information**

| Tool | What it does | Arguments |
|---|---|---|
| `list_disks` | the connected disks (name, type, format, writable or not) and the current one. The sampler's own refresh of that list is sent only with `--allow-disk-refresh` and `refresh: true` | `refresh` (only with `--allow-disk-refresh`) |
| `select_disk` | makes a disk current | `name` or `handle` |
| `list_disk_contents` | the current folder's sub-folders and files, with sizes in bytes | none |
| `open_folder` | goes into a sub-folder and lists it | `name`\* |
| `close_folder` | goes back up to the parent folder | none |
| `get_disk_space` | the free bytes of the current disk | none |
| `audition_file` | plays a file of the current folder without loading it (a started audition plays until stopped), or stops | `action`\* (`start` or `stop`), `name` (for `start`) |

**Loading** (adds to the sampler's memory; the answer lists what was added)

| Tool | What it does | Arguments |
|---|---|---|
| `load_file` | loads a program, a sample or a multi file of the current folder | `name`\*, `with_dependents` (also load the samples a program uses), `sample_mode` (`normal`, `ram` or `virtual`) |
| `load_folder` | loads a sub-folder and everything in it | `name`\* |

A large sample takes time: a 40 MB sample took about a minute.

**Saving** (needs a writable disk; **never replaces a file unless `overwrite` is true**)

| Tool | What it does | Arguments | Confirm |
|---|---|---|---|
| `save_memory_item` | saves one program, sample or multi to the current folder, then checks the folder | `kind`\* (`program`, `sample`, `multi`), `name`\*, `overwrite`, `save_children` | |
| `save_all_memory_items` | saves every item of a kind | `kind`\*, `confirm`\*, `overwrite`, `save_children` | **the number** of items of that kind in memory |

A program is saved as `<name>.AKP`, a sample as `<name>.WAV`; the extension of a multi file has not been seen on a real sampler.

**Organising the disk** (needs a writable disk)

| Tool | What it does | Arguments | Confirm |
|---|---|---|---|
| `create_folder` | creates an empty sub-folder of the current folder (it does not open it) | `name`\* | |
| `rename_file` | renames a file; give the new name **without** the extension, which the sampler keeps | `name`\*, `new_name`\* | |
| `rename_folder` | renames a sub-folder | `name`\*, `new_name`\* | |
| `delete_file` | **deletes a file. Irreversible** | `name`\*, `confirm`\* | the file's exact name, as listed |
| `delete_folder` | **deletes a sub-folder. Irreversible.** A folder that holds files or folders is refused, with the count, unless `delete_contents` is true; then everything in it goes | `name`\*, `confirm`\*, `delete_contents` | the folder's exact name |

---

## 5. Safety

**Every deletion asks for a confirmation.** A tool that deletes something takes a `confirm` argument that must be the **exact name**
of what is deleted (for `delete_keygroup`, the program's name; for `save_all_memory_items`, the number of items). If it is
missing or wrong, **nothing is sent to the sampler** and the answer says what to give. The destructive tools say so in their
annotations, so a client can ask you before it calls them.

**Never done, whatever the options:**
- deleting *all* programs, samples or multis, and clearing the sampler's memory;
- formatting or ejecting a disk;
- the sampler's front-panel keys, its name, clock, play mode, panel lock and MIDI setup;
- song files, set lists, scenelists and the effects board.

(Some of these exist in the sampler's protocol and may be added later; none is offered today. They are listed with their reasons in
`process/2.architecture/ADR-MCP-004-complete-the-tools-and-what-stays-out.md`.)

**The changes are in the sampler's memory** and are lost when it is switched off, unless they are saved to a disk (`save_memory_item`,
`save_all_memory_items`) or from the front panel.

**The disk is opt-in.** Without `--allow-disk` the server cannot read, write, rename or delete anything on a disk. A slow disk
command has once left a real S5000 answering nothing until it was switched off and on; for that reason the disk commands have a long
timeout of their own, are never retried, and the refresh of the disk list is a separate option that should stay off.

**It is checked by a test.** A test (`mcp_sources_call_no_destructive_primitive`) searches the server's source for the commands that
delete, rename, create, save, load, eject, format or clear, and allows each one in a single file only.

---

## 6. What has been tried on a real sampler

On an AKAI S5000 (OS 2.14) with a SCSI2SD disk, 2026-10-05 and 2026-10-06, with the owner present, on objects made for the test; the
full record is `process/2.architecture/OBSERVATIONS-RQ-MCP-012-real-sampler.md`.

| Tried and worked | Not tried, or with a known limit |
|---|---|
| **Programs**: create, rename, delete; all 119 parameters set, read back and put back | the sampler's own answer to deleting the last keygroup of a program (the server refuses it first) |
| **Keygroups and zones**: `add_keygroups`, `delete_keygroup` (the keygroups after a deleted one move down by one), `set_zone_sample`, `get_zone_samples` (the same sample may play in two zones) | |
| **Samples**: the 8 editable parameters; `rename_sample`, `delete_sample` (no sample is selected afterwards); `audition_sample` (heard, and the stop cuts the sound) | |
| **Multis**: `create_multi` (32 parts), `rename_multi`, `delete_multi`, `set_part_program` by name and by position, `get_part_programs`, `clear_part`, `set_multi_program_number`, and **all 12 part parameters**; the part numbers are those of the front panel | |
| **Disk**: browsing, `create_folder`, `rename_file` (a `.WAV` and a `.AKP`: the sampler adds the extension), `rename_folder`, `delete_file`, `delete_folder` with and without `delete_contents`, `audition_file` (heard, and the stop cuts the sound) | **`get_disk_space` says 0 bytes free** for this FAT32 disk: the S5000 does not seem to report it, so do not rely on it |
| **Load and save**: `load_file` (a program, a 40 MB sample, a multi), the control of `with_dependents` (without it no sample is added, with it the program's samples are), `load_folder` (13 samples); `save_memory_item` (a program, a sample, a multi), `save_children` (the program and the samples it uses), `save_all_memory_items` (13 samples), the refusal to overwrite and the save with `overwrite` | a save of a very large sample |
| **Information**: `get_system_info` | |
| **Screen**: `--screen independent` (the owner's screen did not move), `--screen follow` (it followed the assistant and showed each edit), `--screen as-is` (nothing moved) | `independent` was not tried with the screen on the very program the assistant edits; `follow` does not change the page: with the screen on the file system page it stayed there when a multi was selected |

A saved program is `<name>.AKP`, a sample `<name>.WAV`, a multi `<name>.AKM`.

**Do not use the refresh of the disk list (`--allow-disk-refresh`):** on this sampler it never answered, and the sampler had to be
switched off and on. It is not needed for a disk that was plugged in before the server started.

---

## 7. When something goes wrong

| What you see | What it means | What to do |
|---|---|---|
| "No sampler answered at DeviceID 0" | nothing answered on the MIDI ports | check the sampler is on, the cable, the two ports (`--list-ports`), and that no other program holds the ports (another copy of the server, a MIDI editor) |
| "The sampler did not answer while ... (no reply within 2000 ms)" | one command got no answer | try again; if it keeps happening, switch the sampler off and on |
| the same after a **load, save or other disk command** ("... may have to be switched off and on") | the sampler stopped answering during a slow disk command | switch the sampler **off and on**, then let the client call the tool again (the server opens a new session by itself). Nothing was retried |
| "Is a disk selected? Use select_disk first." | no disk is current | `list_disks`, then `select_disk` |
| "Is a program / sample / multi selected?" | the tool acts on the current one and there is none | `select_program`, `select_sample` or `select_multi` |
| a destructive tool answers "nothing was deleted" | the `confirm` was not the exact name | give the name exactly as listed |
| a disk tool is "unknown" (error -32602) | the server was started without `--allow-disk` | add `--allow-disk` to the server's arguments in the client's settings |

---

## 8. For developers

- **Requirements and decisions:** `process/1.requirements/FTR-MCP-001` to `FTR-MCP-004`, `process/2.architecture/ADR-MCP-001` to
  `ADR-MCP-004`, the plans `process/3.plan/PLAN-MCP-001` to `PLAN-MCP-004`.
- **Layout:** `src/McpServer.cpp` (JSON-RPC lines, both eras of MCP: the stateless revision `2026-07-28` and `initialize` for
  `2025-11-25` and earlier), `src/ParameterCatalogue.cpp` and `src/StandardParameters.cpp` (the parameters as a table of rows,
  checked against the AKM item catalogue by the tests), `src/SamplerGateway.cpp` and `src/SamplerGatewayDisk.cpp` (blocking calls
  over an AKM session: memory and disk), `src/Tools.cpp` (the tools), `src/ServerOptions.cpp` (the launch arguments), `server/main.cpp`
  (the executable, the only file that knows the JUCE MIDI backend). To add a parameter, add a row to `StandardParameters.cpp`.
- **Tests:** `xs56k_mcp_tests` and the `mcp_*` entries of `ctest`, all against the simulated sampler (`juce/tests/mcp/`). Two scripted
  conversations are piped into `xs56k_mcp_server_simulated` (one with `--allow-disk --allow-disk-refresh`) and compared with
  `*.expected.jsonl`; `mcp_readme_names_every_tool_and_option` checks that this README names every tool the server lists and every
  option of its usage text. To regenerate an expected conversation after a deliberate change, run the simulated server on the
  `.jsonl` input and read the difference.
- **Process:** every change goes through the AGNOS process of `.github/instructions/agnos-sw-eng.instructions.md`; the tests are
  written first and run red before the code.
