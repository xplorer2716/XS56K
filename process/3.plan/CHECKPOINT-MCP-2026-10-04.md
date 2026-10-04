# Checkpoint — session MCP, 2026-10-04 (after 9 tasks)

Not an AGNOS artifact (no ID, ignored by the index): the hand-off note the CONTEXT MANAGEMENT rule asks for. Read it first
when resuming.

## Done this session (branch `feature/MCP`, from `feature/AKM`, all committed, nothing pushed)

| Task | Subject | Commit |
|---|---|---|
| TASK-MCP-001 | Author FTR-MCP-001, ADR-MCP-001 (accepted by the owner), PLAN-MCP-001 | `5b00d20` |
| TASK-MCP-002 | `juce/mcp`, nlohmann/json 3.12.0 pinned, the protocol of both MCP eras | `7ec824a` |
| TASK-MCP-003 | Parameter catalogue, lot 1 (24 parameters), tolerant name and value resolution | `4fc903c` |
| TASK-MCP-004 | Sampler gateway: blocking calls over an AKM session opened when first needed | `198d3d2` |
| TASK-MCP-005 | The six tools | `028dea2` |
| TASK-MCP-006 | The `xs56k_mcp_server` executable and its launch arguments (the MIDI ports) | `5e12abd` |
| TASK-MCP-007 | `xs56k_mcp_server_simulated` and the scripted conversation in `ctest` | `ef80617` |
| TASK-MCP-008 | Lot 2 of the catalogue: 54 parameters | `6ecbf67` |

`ctest` 803/803 on Windows/MSVC Debug at the last commit (711 before the session, 92 new MCP entries).

## Open: TASK-MCP-009, the run on the real S5000 (the owner's, on a scratch program)

The documentation of it is done (`AGENTS.md`, `CHANGELOG.md`, `juce/mcp/README.md`); the run is not. Procedure:

1. Prepare a scratch program in the sampler's memory (two or three keygroups) and keep your backup. Nothing else is touched.
2. `xs56k_mcp_server --list-ports`, then run the server by hand with the ports of the previous sessions
   (`--in "MIDIIN2 (ESI M8U eX)" --out "MIDIOUT15 (ESI M8U eX)"`) and pipe a conversation into it: `select_program` the scratch
   program, then `get_parameters` for each group. **Record the answers: they are the values to put back.**
3. Set each of the 54 parameters to another value (all keygroups, and one keygroup for a few), read each back, and note every
   answer that differs from the simulated sampler's (`juce/tests/mcp/conversations/simulated_session.expected.jsonl`).
4. Put every value back from the answers of step 2 (per keygroup where they differed). Check the program on the screen.
5. Then try it from Claude Code with the server entry of `juce/mcp/README.md`, in plain words, and see what the sampler's
   screen does (Auto screen update is on while the server is open).
6. Write `process/2.architecture/OBSERVATIONS-RQ-MCP-012-real-sampler.md`, correct the simulated sampler for what it had wrong,
   regenerate the expected conversation (the command is in `juce/tests/CMakeLists.txt`), and close TASK-MCP-009.

What to look for (the open points of `FTR-MCP-001`):
- **A Set while "all keygroups" is selected** (§08/&01 with 0 current): does it reach every keygroup? The tools' default is "all".
- **Codes 12 to 14 of the modulation sources** (named MODWHEEL 2, BEND 2, EXTERNAL 2): what the sampler's screen shows for them.
- **The ERROR the sampler gives with no current program** (the gateway handles ERROR 04 and ERROR 385 and reports others in words).
- **The clock division labels** (written from the spec's `8cy/bt`, `2bt/cy`), and the real values the program starts with.
- **Which MCP revision a real client opens with** (a `server/discover` probe, or `initialize`); the server logs on standard error.
- **A killed server** leaves Still Alive on, Sync LCD off and Auto screen update on: see how a client closes the process.

## Decisions to remember

- The server serves both eras of MCP (`ADR-MCP-001` DEC-MCP-002); only the 2026-07-28 and 2025-11-25 specification pages were
  read in full, the three older legacy revisions are served because the messages used have the same shape.
- `get_parameters` is not marked read-only: it moves the sampler's current keygroup selection.
- Units are the sampler's own (0 to 100); no Hz or seconds, the spec gives none.
- No tool creates, renames, deletes or saves; a `ctest` entry searches the sources for such calls.
- The independent Opus review offered for DEC-MCP-003 (the blocking bridge) was not run: the owner accepted the ADR without it.
