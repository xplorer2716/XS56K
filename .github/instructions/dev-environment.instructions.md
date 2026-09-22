---
applyTo: "**"
---

# Dev environment / local AI tooling — resume checklist

This file tracks what a fresh session/container needs to re-check before relying on
the local AI tooling (Ollama, grepai) set up for this repository. Add new items here
as the setup grows — this is not limited to Ollama/grepai.

Environment note: outbound network access in this container is restricted by an
egress policy. `registry.ollama.ai` and `huggingface.co` are **blocked**. `github.com`
(releases, API for attached repos) works. Keep this in mind before assuming a
`curl`/`ollama pull` will succeed — prefer GitHub as the transfer channel for any
binary asset needed here.

## 1. Ollama

- Installed as a standalone binary at `/usr/local/bin/ollama` (downloaded from the
  `ollama/ollama` GitHub release asset `ollama-linux-amd64.tgz`, CUDA/ROCm libs
  stripped out — not installed via `ollama.com/install.sh`, which is blocked here).
- Check it's running: `pgrep -af "ollama serve"` or `curl -s http://127.0.0.1:11434/`
  (expects `Ollama is running`).
- If not running, start it detached (it does **not** survive a container/session
  restart, nor does it persist as a real background job across tool-call boundaries
  in some environments — check again after any long pause):

  ```bash
  nohup ollama serve > /tmp/ollama_serve.log 2>&1 & disown
  sleep 3
  curl -s http://127.0.0.1:11434/
  ```

## 2. `nomic-embed-text` model

- `ollama pull nomic-embed-text` **does not work** here (`registry.ollama.ai` is
  blocked by the egress policy). The model was instead built locally from a GGUF file.
- Check it's present: `ollama list` should show `nomic-embed-text:latest`.
- If missing, the GGUF weights are attached as release assets on this repo's GitHub
  release tagged `#init` (https://github.com/xplorer2716/XS56K/releases/tag/%23init):
  `nomic-embed-text-v1.5.f16.gguf` (274 MB, used) and `nomic-embed-text-v1.5.Q8_0.gguf`
  (146 MB, smaller/quantized alternative).
- To reinstall: download the asset (via the GitHub API asset id — the `%23` in the
  tag confuses some proxies on the plain `releases/download/...` URL, so prefer
  `https://api.github.com/repos/xplorer2716/XS56K/releases/assets/<id>` with
  `-H "Accept: application/octet-stream"`), then:

  ```bash
  cat > Modelfile <<'EOF'
  FROM ./nomic-embed-text-v1.5.f16.gguf
  EOF
  ollama create nomic-embed-text -f Modelfile
  ```

- Sanity check: `curl -s http://127.0.0.1:11434/api/embeddings -d '{"model":"nomic-embed-text","prompt":"test"}'`
  should return a JSON `embedding` array.

## 3. grepai

- Config already initialized at `.grepai/config.yaml` (provider: ollama, model:
  nomic-embed-text). `.grepai/` is **git-ignored on purpose** (binary index, runtime
  lock files, and a config pointing at `http://localhost:11434` which is only valid
  inside this container) — it is not meant to be committed, and must be regenerated
  in any new container.
- Check status: `grepai status` (files indexed, chunk count, watcher state).
- `grepai status`'s "Watcher: not running" can be a false negative if the watcher was
  started manually (see below) instead of via `grepai watch --background` — check for
  a live process before assuming it's actually down: `pgrep -af "grepai watch"`.
- To (re)start the watcher:

  ```bash
  cd /home/user/XS56K
  grepai watch --background
  ```

  If this times out ("background process failed to become ready... after 30s") —
  this can happen on the *first* run in a fresh container, when the initial full-repo
  embedding pass takes longer than the readiness check — fall back to a manually
  detached run instead:

  ```bash
  nohup grepai watch --no-ui > /tmp/grepai_watch.log 2>&1 & disown
  ```

  Then poll `grepai status` until `Files indexed` stops growing.

## 4. Order of operations

Ollama must be running (step 1) **and** the `nomic-embed-text` model must exist
(step 2) before `grepai watch`/`grepai search` will work — grepai calls Ollama's
`/api/embeddings` endpoint for every indexed chunk and every search query.
