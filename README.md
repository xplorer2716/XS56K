# XS56K

> Drive an AKAI S5000 or S6000 sampler from a computer: a SysEx library, an MCP server for AI assistants, and, later, an editor

XS56K is three things that build on each other:

1. **A library** that aims to cover every MIDI System Exclusive (SysEx) message of the AKAI S5000 and S6000 samplers.
2. **An MCP server** ([Model Context Protocol](https://modelcontextprotocol.io)) that lets an AI assistant, or a person talking to
   one (by voice, for example), work on the sampler: edit programs, samples and multis, load and save files on its disks.
   The goal is for it to cover every function of the samplers.
3. **A program editor** with a real user interface. It is a goal: it does not exist yet.

**Status:** experimental. Only the first two parts exist, and only part of the second one.

| Part | Today | Goal |
|---|---|---|
| SysEx library (`juce/akm`) | All 13 sections of the spec have a command for every row (check below). Run on a real S5000 (OS 2.14), with gaps listed below. | The same, checked on an S6000 too |
| MCP server (`juce/mcp`) | 44 tools, 60 with `--allow-disk`. Every tool but the settings, the MIDI setup, the song file, set list and scenelist tools run on a real S5000, on test objects. | Every function of the samplers: song files, set lists and scenelists can only be listed, selected and renamed, and deleting, saving and loading them, effects and others are not offered yet |
| Editor (`juce/app`) | A bare placeholder window, there only to exercise the build | A real editor |

## The SysEx library (`juce/akm`)

The library (`xs56k_akm`, namespace `akm`) builds and parses the SysEx messages of the
[S5000/S6000 SysEx specification](documents/akai_s5000_s6000_sysex_spec_2.10.pdf). The items are data, in
[`juce/akm/data/items.json`](juce/akm/data/items.json); a script compares that catalogue with the specification's item list.
Its `--coverage` check reports all 13 sections as complete (system, MIDI, programs, keygroups, zones, samples, disks, multis,
effects board, scenelists, song files, front panel: 384 command rows) and no item of the specification left unaccounted for.

"Complete" means every message is defined and tested against a simulated sampler. On the real S5000 (OS 2.14), most sections were
run and read back. The gaps: the effects-board settings (no effects board was available), the song-file and scenelist items that name
something (the sampler held none), ejecting a disk (never run), and the disk-list refresh, which hung the sampler. Nothing was
tried on an S6000. The record of each run is in `process/2.architecture/OBSERVATIONS-RQ-AKM-*.md`.

To use it from C++: open a `Session` on a pair of MIDI ports, then send commands one at a time with the primitives of each
section (`ProgramPrimitives`, `SamplePrimitives`, `MultiPrimitives`, `DiskPrimitives` and so on in
[`juce/akm/include/akm`](juce/akm/include/akm)). The public headers use no JUCE type. The design is in
[`ADR-AKM-001`](process/2.architecture/ADR-AKM-001-akm-transport-architecture.md). To try a real sampler, see the probe's
[README](juce/tests/probe/README.md).

## The MCP server (`juce/mcp`)

`xs56k_mcp_server` lets an MCP client (Claude Code or another) work on the sampler's memory in musical words, such as:

- "set the filter cutoff of the program BASS to 80";
- "load the sample S1 and play it on zone 1 of the first keygroup";
- "build a multi with BASS on part 1 and LEAD on part 2, then save the program".

Driving the sampler by voice is one thing this makes possible: an assistant that takes spoken requests and calls these tools.
XS56K has no voice feature of its own.

You do not start the server yourself: your MCP client does. Find the sampler's MIDI ports with `xs56k_mcp_server --list-ports`, then
register the server in the client, for example
`claude mcp add xs56k -- <path to>/xs56k_mcp_server --in "<input port>" --out "<output port>"`. To try it without a sampler, use
`xs56k_mcp_server_simulated`, built with the tests. The options, every tool and what was tried on a real sampler are in
[`juce/mcp/README.md`](juce/mcp/README.md).

**Safety.** The server changes the sampler's memory, which is lost at power off unless saved. Every deletion needs a `confirm`
that is the exact name of what is deleted; if it is wrong, nothing is sent. The server never clears the whole memory, formats or
ejects a disk. Disk access is off unless you give `--allow-disk`. The sampler's disk-list refresh hung a real S5000 with a SCSI2SD
disk until it was switched off and on, so it needs its own option, `--allow-disk-refresh`, which should stay off.

## The editor (`juce/app`)

Goal. What exists is a placeholder: `XS56K`, built with `-DBUILD_APP=ON`, opens one empty window. It is there to exercise the
build, versioning and release plumbing, and is not an editor.

## Where things are

| Directory | What it holds |
|---|---|
| `juce/akm` | the S5000 SysEx library (`xs56k_akm`), written for this project |
| `juce/mcp` | the MCP server (`xs56k_mcp_server`) and its [README](juce/mcp/README.md) |
| `juce/midi`, `juce/framework` | MIDI ports and application framework, ported from [XplorerEditor](https://github.com/xplorer2716/XplorerEditor) |
| `juce/app` | the placeholder application |
| `juce/tests` | the tests, a simulated sampler, and the real-sampler [probe](juce/tests/probe/README.md) |
| `documents/` | the sampler's SysEx specification and operator's manual ([index](documents/INDEX.md)) |
| `process/` | requirements, architecture decisions, plans and the notes of each real-sampler run |

## Build and test

You need a C++20 compiler and CMake 3.22 or later. On Linux, also `libasound2-dev`; the application additionally needs
`libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libxext-dev libfreetype6-dev libfontconfig1-dev
libgl1-mesa-dev`. [JUCE](https://juce.com/) 8.0.15 and Catch2 are fetched by CMake. No packaged build is published yet.

```bash
git clone https://github.com/xplorer2716/XS56K.git
cd XS56K

# Libraries only
cmake -S juce -B juce/build -DCMAKE_BUILD_TYPE=Debug
cmake --build juce/build -j"$(nproc)"

# With the placeholder application: add -DBUILD_APP=ON
# Tests (this also builds the simulated MCP server)
cmake -S juce -B juce/build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build juce/build -j"$(nproc)"
ctest --test-dir juce/build --output-on-failure
```

On Windows, use a generator of your choice (Visual Studio, for example) and add `--config Debug` to the build and `-C Debug` to
`ctest`. The build is warning-clean at `-Wall -Wextra -Wpedantic -Werror` (`/W4 /WX` on MSVC) for project code.

## More documentation

- [`documents/`](documents/INDEX.md): the SysEx specification (v2.10) and the operator's manual (v1.21).
- [`process/`](process/): requirements (`FTR-AKM-*`, `FTR-MCP-*`), decisions (`ADR-*`), plans, and the observations of each run on
  a real sampler.
- [`juce/mcp/README.md`](juce/mcp/README.md): the MCP server's reference.
- [`juce/tests/probe/README.md`](juce/tests/probe/README.md): the real-sampler tools of the library.
- [`CHANGELOG.md`](CHANGELOG.md), [`CONTRIBUTING.md`](CONTRIBUTING.md), [`SUPPORT.md`](SUPPORT.md),
  [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md).
- [`AGENTS.md`](AGENTS.md): the commands and conventions for contributors who work with AI coding agents.

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) and the [Code of Conduct](CODE_OF_CONDUCT.md) before
opening an issue or a pull request, and see [SUPPORT.md](SUPPORT.md) for how to get help.

**For contributors using AI coding agents** (Claude Code, GitHub Copilot, OpenAI Codex, or similar): adherence to the
[AGNOS process](process/) is **mandatory and non-negotiable**. All work must follow AGNOS planning, work tracking, and commit
conventions as documented in [AGENTS.md](AGENTS.md). This ensures traceability, consistency, and maintainability of all
contributions.

**Pull requests from AI agents that do not comply with these requirements will be rejected without justification.**

## Security

Please do **not** report security vulnerabilities in public issues. See [SECURITY.md](SECURITY.md).

## License

Distributed under the AGPL-3.0-or-later license. See [LICENSE](LICENSE) for details.
