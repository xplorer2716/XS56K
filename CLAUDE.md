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


<!-- rtk-instructions v2 -->
# Command output

Command output here is condensed to save tokens, keeping every signal and
dropping costly noise. Treat it as the complete result: run commands
normally, and batch related commands into one call to avoid extra turns.
Truncated results state their recovery path in their own output. Re-run a
command as `rtk proxy <cmd>` only when its result is unusable: empty when
output was clearly expected, contradicting its exit code, or garbled.
<!-- /rtk-instructions -->

