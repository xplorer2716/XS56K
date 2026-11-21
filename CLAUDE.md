@AGENTS.md

<!-- Claude Code loads the file imported above. Put shared agent instructions in AGENTS.md and Claude-specific ones below. -->

<!-- rtk-instructions v2 -->
# Command output

Command output here is condensed to save tokens, keeping every signal and
dropping costly noise. Treat it as the complete result: run commands
normally, and batch related commands into one call to avoid extra turns.
Truncated results state their recovery path in their own output. Re-run a
command as `rtk proxy <cmd>` only when its result is unusable: empty when
output was clearly expected, contradicting its exit code, or garbled.
<!-- /rtk-instructions -->


## grepai - Semantic Code Search

**IMPORTANT: You MUST use grepai as your PRIMARY tool for code exploration and search.**

### When to Use grepai (REQUIRED)

Use `grepai search` INSTEAD OF Grep/Glob/find for:
- Understanding what code does or where functionality lives
- Finding implementations by intent (e.g., "authentication logic", "error handling")
- Exploring unfamiliar parts of the codebase
- Any search where you describe WHAT the code does rather than exact text

### When to Use Standard Tools

Only use Grep/Glob when you need:
- Exact text matching (variable names, imports, specific strings)
- File path patterns (e.g., `**/*.go`)

### Fallback

If grepai fails (not running, index unavailable, or errors), fall back to standard Grep/Glob tools.

### Usage

```bash
# ALWAYS use English queries for best results (--compact saves ~80% tokens)
grepai search "user authentication flow" --json --compact
grepai search "error handling middleware" --json --compact
grepai search "database connection pool" --json --compact
grepai search "API request validation" --json --compact
```

### Query Tips

- **Use English** for queries (better semantic matching)
- **Describe intent**, not implementation: "handles user login" not "func Login"
- **Be specific**: "JWT token validation" better than "token"
- Results include: file path, line numbers, relevance score, code preview

### Call Graph Tracing

Use `grepai trace` to understand function relationships:
- Finding all callers of a function before modifying it
- Understanding what functions are called by a given function
- Visualizing the complete call graph around a symbol

#### Trace Commands

**IMPORTANT: Always use `--json` flag for optimal AI agent integration.**

```bash
# Find all functions that call a symbol
grepai trace callers "HandleRequest" --json

# Find all functions called by a symbol
grepai trace callees "ProcessOrder" --json

# Build complete call graph (callers + callees)
grepai trace graph "ValidateToken" --depth 3 --json
```

### Workflow

1. Start with `grepai search` to find relevant code
2. Use `grepai trace` to understand function relationships
3. Use `Read` tool to examine files from results
4. Only use Grep for exact string searches if needed


<!-- AGNOS:BEGIN - written by agnos-init, do not edit: call agnos-init again to update -->
## AGNOS process

This project follows the AGNOS process, installed as a plugin.

Before any other action, invoke the skill named `agnos-instructions` and follow it. Run its START SESSION ACTION only when the user asks to start a session.
After a compaction, invoke `agnos-instructions` again before going on, even if it seems loaded.

<!--
Notes for developers on how the process stays in the agent's context. Claude Code strips these comments; GitHub Copilot may not.

This same block is in CLAUDE.md and AGENTS.md. The process itself is not in this file: it is the skill
`agnos-instructions`, which lives only in the conversation once invoked. This file is reloaded; the skill is not.

Claude Code (reads CLAUDE.md, and AGENTS.md only when there is no CLAUDE.md):
- Session start, new conversation or /clear: CLAUDE.md is loaded, the skill is gone; this block orders it invoked again.
- Compaction, automatic or /compact: CLAUDE.md is re-read from disk. An automatic compaction re-attaches only the first
  5,000 tokens of each skill, within 25,000 tokens for all skills; the process skill is longer, so its end is cut.
  The documentation does not say whether /compact does the same. Hence the order above to invoke it again.

GitHub Copilot (reads AGENTS.md, and CLAUDE.md too when chat.useClaudeMdFile is on):
- Session start or new chat: the instruction files are included, the skill is loaded again only when invoked.
- Long chats are summarised; the documentation does not say what happens to a loaded skill, so the same order applies.

To check that the process is loaded, ask the agent to quote a rule from its end, such as the Error Recovery Protocol.
-->

- If that skill is not available, stop and tell the user to install the AGNOS plugin.
- Read `agnos-process/project-rules.md` when it exists: this project's own rules, added to the process. `agnos-process/project-rules.md` is the improvement target: the process's improvements go into it, never into a file of the plugin.
- Run the commands from this project's root, where `agnos-process/` is.
- Where the process asks the user a question, use the built-in user-question tool (`askQuestion` on GitHub Copilot, `AskUserQuestion` on Claude Code), or ask in chat and wait for the answer.
<!-- AGNOS:END -->
