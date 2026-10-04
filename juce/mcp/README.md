# juce/mcp — MCP server for the AKAI S5000/S6000

An [MCP](https://modelcontextprotocol.io) server that lets an MCP client (Claude Code or another) edit a program of the
sampler by talking to it: "set the filter cutoff to 80", "change the filter type to 2-POLE LP+", "set the amplitude
envelope attack to 55". It is a proof of concept limited to the filter, the amplitude envelope, the filter envelope and the
two LFOs of a program already in the sampler's memory. Requirements: `process/1.requirements/FTR-MCP-001-program-editing-server.md`;
decisions: `process/2.architecture/ADR-MCP-001-mcp-server-architecture.md`.

## Run it

```
xs56k_mcp_server --list-ports
xs56k_mcp_server --in "<port the sampler sends on>" --out "<port it receives on>"
```

Optional: `--device-id <0-31>` (default 0), `--timeout-ms <ms>` (default 2000), `--no-lcd` (leave the sampler's Sync LCD and
Auto screen update settings alone). The server speaks MCP on standard input and output and logs on standard error, so it is
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

| Tool | What it does |
|---|---|
| `get_status` | whether the sampler answers, how many programs, the current one and its keygroups |
| `list_programs` | the programs in memory, with their positions |
| `select_program` | makes a program current, by name or by position |
| `list_parameters` | the 54 parameters, their names, what they accept and what they do (optionally one group) |
| `get_parameters` | reads a group or a list of parameters, for one keygroup or all |
| `set_parameter` | sets one parameter (a number, a choice such as `2-POLE LP+`, or on/off), then reads it back |

Everything acts on the sampler's **memory**, not on disk, and nothing is created, renamed, deleted or saved. The program is
lost if the sampler is switched off without saving it from the front panel.

When a client closes the server's standard input, the server closes its session and puts the sampler's section 00 settings
back (checksums, Still Alive, Notification, Sync LCD, Auto screen update). A server that is killed instead leaves Still Alive
on, Sync LCD off and Auto screen update on until the sampler is switched off.

## Layout

`src/McpServer.cpp` (JSON-RPC lines, both eras of MCP), `src/ParameterCatalogue.cpp` and `src/StandardParameters.cpp` (the
parameters as a table of rows, checked against the AKM item catalogue by the tests), `src/SamplerGateway.cpp` (blocking calls
over an AKM session), `src/Tools.cpp` (the six tools), `src/ServerOptions.cpp` (the launch arguments) and `server/main.cpp`
(the executable, the only file that knows the JUCE MIDI backend). To add a parameter, add a row to `StandardParameters.cpp`.
Tests are in `juce/tests/mcp/`.
