# XS56K

> Editor for the AKAI S5000 and S6000 samplers

Enables bidirectional control of these samplers through a modern interface, bringing these
legendary samplers back to life.

**Status:** experimental — no usable editor UI yet. The MIDI/SysEx control layer for Program,
Keygroup, Keygroup Zone and Sample (`§0A`/`§08`/`§06`/`§0E`) is implemented and proven against both
a simulated sampler and a real AKAI S5000 (see
[`process/3.plan/SUMMARY-akm-sections-coverage.md`](process/3.plan/SUMMARY-akm-sections-coverage.md)
for exactly what's covered); the application itself (`juce/app`) is still a placeholder window, not
a real editor.

## Getting started

### Prerequisites

- A C++20 compiler and CMake ≥ 3.22.
- On Linux: `libasound2-dev` (ALSA headers); the GUI target additionally needs `libx11-dev
  libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libxext-dev libfreetype6-dev
  libfontconfig1-dev libgl1-mesa-dev`.
- [JUCE](https://juce.com/) 8.0.15 — fetched automatically by CMake (`FetchContent`), nothing to
  install separately.

### Installation

No packaged build is published yet. Clone and build from source (commands below are for
bash/Linux/macOS; on Windows, use a generator of your choice, e.g. Visual Studio, and build in the
IDE or with `cmake --build juce/build --config Debug`):

```bash
git clone https://github.com/xplorer2716/XS56K.git
cd XS56K
cmake -S juce -B juce/build -DCMAKE_BUILD_TYPE=Debug -DBUILD_APP=ON
cmake --build juce/build -j"$(nproc)"
```

### Usage

The build above produces an `XS56K` executable that opens a single placeholder window — there is no
real editor UI yet. `juce/app` is intentionally a bare `juce::DocumentWindow`, there only to
exercise the build/version/deploy plumbing.

## Development

```bash
# Build (libraries only)
cmake -S juce -B juce/build -DCMAKE_BUILD_TYPE=Debug
cmake --build juce/build -j"$(nproc)"

# Run the tests
cmake -S juce -B juce/build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build juce/build -j"$(nproc)"
ctest --test-dir juce/build --output-on-failure

# Lint / format
# No separate step: the build itself is warning-clean at -Wall -Wextra -Wpedantic -Werror
# (/W4 /WX on MSVC) for project code, enforced by the xs56k::warnings CMake target.
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for the full development workflow, and
[AGENTS.md](AGENTS.md) for the complete command reference and project conventions.

## Contributing

Contributions are welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) and our
[Code of Conduct](CODE_OF_CONDUCT.md) before opening an issue or a pull request.

**⚠️ For contributors using AI coding agents** (Claude Code, GitHub Copilot, OpenAI Codex, or similar):
adherence to the [AGNOS process](process/) is **mandatory and non-negotiable**. All work must follow
AGNOS planning, work tracking, and commit conventions as documented in [AGENTS.md](AGENTS.md). This
ensures traceability, consistency, and maintainability of all contributions.

**Pull requests from AI agents that do not comply with these requirements will be rejected without
justification.**

## Support

See [SUPPORT.md](SUPPORT.md) for how to get help.

## Security

Please do **not** report security vulnerabilities in public issues.
See [SECURITY.md](SECURITY.md).

## License

Distributed under the AGPL-3.0-or-later license. See [LICENSE](LICENSE) for details.
