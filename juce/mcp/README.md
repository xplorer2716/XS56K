# juce/mcp — MCP server for the AKAI S5000/S6000

An [MCP](https://modelcontextprotocol.io) server that lets an MCP client (Claude Code or another) edit the sampler's memory by
talking to it: "set the filter cutoff to 80", "change the filter type to 2-POLE LP+", "create a program with 4 keygroups",
"set the loop end of the sample to 1500", "put part 3 of the multi on channel 10B". Requirements:
`process/1.requirements/FTR-MCP-001-program-editing-server.md` and `FTR-MCP-002-structure-and-more-domains.md`; decisions:
`process/2.architecture/ADR-MCP-001-mcp-server-architecture.md` and `ADR-MCP-002-safety-tiers-and-domain-targets.md`.

## Run it

```
xs56k_mcp_server --list-ports
xs56k_mcp_server --in "<port the sampler sends on>" --out "<port it receives on>"
```

Optional: `--device-id <0-31>` (default 0), `--timeout-ms <ms>` (default 2000), `--no-lcd` (leave the sampler's Sync LCD and
Auto screen update settings alone), `--allow-disk` (offer the disk tools), `--allow-disk-refresh` (with `--allow-disk`: let `list_disks` refresh the sampler's disk list; see
below) and `--disk-timeout-ms <ms>` (default 120000). The server speaks MCP on standard input and output and logs on standard error, so it is
started by the client, not by hand.

**The MIDI ports are the server's configuration.** Put them in the arguments of the server entry of your client's MCP
configuration. With Claude Code, for example:

```
claude mcp add xs56k -- <path to>/xs56k_mcp_server --in "MIDIIN2 (ESI M8U eX)" --out "MIDIOUT15 (ESI M8U eX)"
```

(or the equivalent `command` and `args` in the client's own settings file; that file is yours, do not commit it).

To try the tools with no sampler, run `xs56k_mcp_server_simulated` instead (built with the tests): the same server over a
simulated sampler holding three programs (PAD, BASS and LEAD, BASS current).

## What it does

Seventeen tools (twenty-six with `--allow-disk`, see below). Each declares a tier in its MCP annotations: **read** changes nothing, **edit** overwrites a value or a
selection in memory, **structure** creates, renames or deletes a program in memory.

| Tool | Tier | What it does |
|---|---|---|
| `get_status` | read | whether the sampler answers, how many programs, the current one and its keygroups |
| `list_programs` | read | the programs in memory, with their positions (the sampler keeps them in alphabetical order) |
| `list_parameters` | read | the parameters, their names, what they accept and do (optionally one group, and a `domain`: program, sample or multi) |
| `select_program` | edit | makes a program current, by name or by position |
| `get_parameters` | edit | reads a group or a list of parameters of the current program, for one keygroup or all, and for one zone or all (it moves the sampler's keygroup selection) |
| `set_parameter` | edit | sets one parameter (a number, a choice such as `2-POLE LP+`, or on/off) for a keygroup and a zone, then reads it back |
| `create_program` | structure | creates a program with a name (up to 12 characters) and 1 to 99 keygroups, and makes it current |
| `rename_program` | structure | renames the current program |
| `delete_program` | structure | deletes the **current** program, only when `confirm` is its exact name |
| `list_samples`, `select_sample` | read, edit | the samples in memory with their positions; makes one current |
| `get_sample_parameters`, `set_sample_parameter` | read, edit | the current sample's 12 parameters (four of them read-only: type, channels, length, rate) |
| `list_multis`, `select_multi` | read, edit | the multis in memory with their positions and the current one's part count; makes one current |
| `get_multi_parameters`, `set_multi_parameter` | read, edit | the 12 parameters of a part of the current multi (parts numbered from 1; a Set needs its `part`, a number or `all`) |

Everything above acts on the sampler's **memory**: nothing deletes all programs or multis or clears the memory, and no tool
creates, deletes or renames a sample or a multi. A program is lost if the sampler is switched off without saving it, from the
front panel or with the disk tools below.

### The disk tools (only with `--allow-disk`)

A person cannot get a sample, a program or a multi into the sampler, or keep one, without the disk. These nine tools exist only when
the server is launched with `--allow-disk` (write it in the client's server configuration, next to the ports), because **a slow
section 10 command has once left a real S5000 answering nothing until it was switched off and on**. They act on the sampler's own
disks (hard disk, floppy, CD-ROM, removable), not on the computer's files.

| Tool | Tier | What it does |
|---|---|---|
| `list_disks` | read | the connected disks (handle, name, type, format, writable), the current one marked; the sampler's refresh of its list (the risky command) only with the launch option `--allow-disk-refresh` and `refresh` true |
| `select_disk` | edit | makes a disk current, by name or handle |
| `list_disk_contents` | read | the current folder's sub-folders and files with their sizes, and the path |
| `open_folder`, `close_folder` | edit | descends into a sub-folder, goes back up |
| `load_file` | structure | loads a file of the current folder (a program, a sample, a multi), with `with_dependents` the files it depends on, a `sample_mode` (normal, ram, virtual); answers the memory before and after |
| `load_folder` | structure | loads a sub-folder and everything in it |
| `save_memory_item` | disk, destructive | saves a program, sample or multi by name to the current folder of a writable disk; refuses if a file of that name is there unless `overwrite` is true; checks the folder afterwards |
| `save_all_memory_items` | disk, destructive | saves every item of a kind, only when `confirm` is how many there are in memory |

**The refresh of the disk list is off unless `--allow-disk-refresh` is given**: on the owner's S5000, whose disk is a SCSI2SD, it left the
sampler answering nothing until it was switched off and on, and it is not needed for a disk that was plugged in before the server
started (the list, the selection, the browsing and the loads worked without it). `--disk-timeout-ms` (default 120000) is how long a
load or a save waits. After it the answer says the sampler may have to
be switched off and on, and nothing is retried. No tool deletes or renames a file or a folder, creates a folder, ejects or formats a
disk. **Run on the real sampler (2026-10-05, owner present, SCSI2SD disk)**: browsing, `select_disk`, `load_file` of a program (0.3 s)
and of a 40 MB sample (60 s) work, loading with dependents too, and a program was saved (refused when its file exists, replaced with `overwrite`); the refresh hung the
sampler; `load_folder`, `save_all_memory_items`, saving a sample or a multi and the multi tools are tested against the simulated
sampler only so far (`PLAN-MCP-003` TASK-MCP-023).

**What was run on a real S5000** (2026-10-05, `process/2.architecture/OBSERVATIONS-RQ-MCP-012-real-sampler.md`): the program tools
and all 119 program parameters, on a program the server created and deleted. The sample tools were then run on a real sample (S1, loaded from the
disk): the eight editable parameters were set, read back and put back. The multi tools were run against an empty memory only; their
parameters are verified on the simulated sampler until a multi is in memory.

Open points: the numbering of a multi's parts against the front panel (the tools send the part minus one), what codes 12 to 14 of
the modulation sources show on the screen, and whether a program name of more than 12 characters is kept.

On Windows a running `xs56k_mcp_server.exe` (the client's own) locks the file: build into another folder with
`cmake --build <dir> --target xs56k_mcp_server -- "/p:OutDir=<folder>\"` to run a fresh copy by hand.

When a client closes the server's standard input, the server closes its session and puts the sampler's section 00 settings
back (checksums, Still Alive, Notification, Sync LCD, Auto screen update). A server that is killed instead leaves Still Alive
on, Sync LCD off and Auto screen update on until the sampler is switched off.

## Safety rules

- No tool deletes or renames a file or a folder, creates a folder, ejects or formats a disk, deletes all programs or multis, clears
  the sampler's memory, or creates, deletes or renames a sample or a multi. The `ctest` entry `mcp_sources_call_no_destructive_primitive`
  searches the sources and allows each primitive in one file only: the gateway creates, renames and deletes a program, its disk unit
  (`SamplerGatewayDisk.cpp`) loads and saves.
- The disk tools exist only with `--allow-disk`, and the refresh of the disk list only with `--allow-disk-refresh` as well (it hung the
  owner's S5000); they never replace a file unless `overwrite` is true. Browsing, loading and the save of a program have been run on the real
  sampler, the bulk save and the save of a sample or a multi not yet (see above).
- The ports are the server's configuration, in the client's own MCP settings file, which is never committed.
- Requirements `RQ-MCP-001` to `RQ-MCP-030`, tasks `TASK-MCP-002` to `TASK-MCP-024`.

## Tests

`xs56k_mcp_tests` and the `mcp_*` entries of `ctest`, all against the simulated sampler. `xs56k_mcp_server_simulated` (built with the
tests, never shipped) is the same server over a simulated sampler holding three programs, three samples and two multis; a scripted
conversation piped into it is a `ctest` entry (`--allow-disk` adds two disks and a second conversation).

## Layout

`src/McpServer.cpp` (JSON-RPC lines, both eras of MCP), `src/ParameterCatalogue.cpp` and `src/StandardParameters.cpp` (the
parameters as a table of rows, checked against the AKM item catalogue by the tests), `src/SamplerGateway.cpp` (blocking calls
over an AKM session), `src/Tools.cpp` (the tools), `src/ServerOptions.cpp` (the launch arguments) and `server/main.cpp`
(the executable, the only file that knows the JUCE MIDI backend). To add a parameter, add a row to `StandardParameters.cpp`.
Tests are in `juce/tests/mcp/`.
