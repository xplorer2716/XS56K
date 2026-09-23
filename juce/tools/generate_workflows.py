#!/usr/bin/env python3
"""Generate the deployment workflows under .github/workflows/.

RQ-BLD-008 requires one workflow per operating system / architecture /
configuration / stream, with the file name, the workflow name and the job key
all equal (RQ-BLD-006), so a pull-request status check names all four without
cross-referencing. That is a good property and it costs fifteen near-identical
files, which is fifteen copies of one procedure waiting to drift apart.

So the files are GENERATED — adapted from xplorer2716/XplorerEditor's own
juce/tools/generate_workflows.py (AGENTS.md), same mechanism: the matrix below
is the source of truth, the .yml files are output, and each one says so in its
own header. The alternative mechanisms that would have removed the
duplication in GitHub's own terms are rejected for the same reason
XplorerEditor's RQ-BLD-023 rejects them — a reusable workflow reports its
check as `<caller> / <job>`, and a matrix reports `<workflow> / build
(windows, release)`; neither is the single self-describing string RQ-BLD-006
exists to produce.

Everything a workflow actually DOES lives in .github/actions/. What is
generated here is only a name, a trigger and a call sequence.

Reduced from XplorerEditor's own matrix: no build-provenance attestation, no
macOS launch-screenshot step — both explicitly out of scope for now
(ADR-BLD-003's out-of-scope list; nothing here is signed or has a real UI to
photograph yet).

Usage:
    python3 juce/tools/generate_workflows.py           # write
    python3 juce/tools/generate_workflows.py --check   # fail if out of date
"""
import argparse
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
WORKFLOWS = HERE.parent.parent / ".github" / "workflows"

# --- the matrix ------------------------------------------------------------
# Adding a tuple here is the whole cost of a new platform.
#
# The runner images are PINNED, and linux's pin is load-bearing rather than
# tidy: glibc is backward- but not forward-compatible, so a binary linked on a
# newer image refuses to start on an older distribution — on the user's
# machine, where no CI run is watching. `ubuntu-latest` would move that
# failure out of the pipeline (same reasoning as XplorerEditor's RQ-BLD-025).
PLATFORMS = [
    ("windows", "x64", "windows-2022"),
    ("macos", "arm64", "macos-latest"),
    ("linux", "x64", "ubuntu-22.04"),
]

STREAMS = {
    # stage -> (configurations, permission, human name)
    "prod": (("release",), "write", "production"),
    "dev": (("debug", "release"), "write", "dev"),
    "canary": (("debug", "release"), "read", "canary"),
}

GENERATED_HEADER = """# =====================================================================
# GENERATED FILE — DO NOT EDIT BY HAND.
# Source of truth : juce/tools/generate_workflows.py
# Regenerate with : python3 juce/tools/generate_workflows.py
# =====================================================================
#
# {name}
#
# One operating system, one architecture, one build configuration, one
# deployment stream — and the file name, the workflow name and the job key are
# the same string, so a pull-request check states all four without
# cross-referencing. [RQ-BLD-005, RQ-BLD-006, RQ-BLD-007, RQ-BLD-008, ADR-BLD-003]
#
# Every step delegates to a composite action under .github/actions/. That is
# what keeps one-file-per-combination affordable: the procedure exists once.
"""


def paths_filter(name: str) -> str:
    # [RQ-BLD-012] — same principle the two hand-written headless workflows
    # already apply: a change outside juce/ or this file cannot affect
    # whether the app still builds.
    return ("    paths:\n"
            "      - 'juce/**'\n"
            f"      - '.github/workflows/{name}.yml'\n"
            "      - '.github/actions/**'\n")


def triggers_for(stage: str, name: str) -> str:
    if stage == "prod":
        # Production is reached through the tag cut-deployment pushes, never
        # through a push to main: RQ-BLD-010 puts a deliberate human act
        # between a merge and a public release.
        return ("  push:\n"
                "    tags:\n"
                "      - '[0-9][0-9][0-9][0-9].[0-9][0-9].[0-9][0-9]-[0-9][0-9][0-9][0-9]'\n")
    if stage == "dev":
        # Both a push that lands on dev and a pull request that targets it
        # build dev's configuration; only the push publishes (PUBLISH's own
        # event_name guard). branches: on pull_request matches the PR's base
        # branch. [RQ-BLD-005, ADR-BLD-002 (DEC-BLD-005/DEC-BLD-006a)]
        return ("  push:\n    branches: [dev]\n" + paths_filter(name)
                + "  pull_request:\n    branches: [dev]\n" + paths_filter(name))
    # Canary triggers on push alone, restricted to anything that is not main
    # or dev: fast, unconditional feedback on a feature branch, no PR needed
    # first. [RQ-BLD-005, ADR-BLD-002]
    return "  push:\n    branches-ignore: [main, dev]\n" + paths_filter(name)


CHECKOUT = """
    steps:
      # fetch-depth 0, not the default 1: the deployment notes list the commits
      # since the previous deployment of this stream, and the previous one is
      # found among the tags. A shallow checkout yields notes claiming every
      # commit is new. (The version derivation itself needs only HEAD.)
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - id: version
        uses: ./.github/actions/resolve-version

      - id: build
        uses: ./.github/actions/build-app
        with:
          os: {os}
          config: {config}
          version-numeric: ${{{{ steps.version.outputs.numeric }}}}
          version-full: ${{{{ steps.version.outputs.full }}}}
"""

TAG_GUARD = """
      # The tag is minted by cut-deployment from this same commit, so the two
      # agree by construction. Checking says so out loud, and catches a tag
      # pushed by hand — the one way they could diverge.
      - name: The tag must be this commit's own version
        shell: bash
        run: |
          if [ "${{{{ github.ref_name }}}}" != "${{{{ steps.version.outputs.display }}}}" ]; then
            echo "::error::tag ${{{{ github.ref_name }}}} is not this commit's version ${{{{ steps.version.outputs.display }}}} — pushed by hand instead of by cut-deployment?"
            exit 1
          fi
"""

PUBLISH = """
      # Runs on push only. For prod the only trigger IS a tag push, so this
      # never excludes anything there; for dev it is what keeps a pull
      # request that targets dev from publishing a release before the merge
      # actually lands. [RQ-BLD-005, ADR-BLD-002]
      - id: package
        if: github.event_name == 'push'
        uses: ./.github/actions/package-deployment
        with:
          artefact-dir: ${{{{ steps.build.outputs.artefact-dir }}}}
          version: ${{{{ steps.version.outputs.display }}}}
          os: {os}
          arch: {arch}
          config: {config}

      - uses: ./.github/actions/publish-deployment
        if: github.event_name == 'push'
        with:
          archive: ${{{{ steps.package.outputs.archive }}}}
          archive-name: ${{{{ steps.package.outputs.archive-name }}}}
          version-display: ${{{{ steps.version.outputs.display }}}}
          version-full: ${{{{ steps.version.outputs.full }}}}
          stage: ${{{{ steps.version.outputs.stage }}}}
          token: ${{{{ secrets.GITHUB_TOKEN }}}}
"""

CANARY_UPLOAD = """
      # Canary builds publish NO deployment (RQ-BLD-005). The binary stays
      # downloadable from this run's own artifacts only.
      - uses: actions/upload-artifact@v4
        with:
          name: XS56K-${{{{ steps.version.outputs.full }}}}-{os}-{arch}-{config}
          path: ${{{{ steps.build.outputs.artefact-dir }}}}
          if-no-files-found: error
"""

DEV_PR_UPLOAD = """
      # A pull request targeting dev verifies the merge result before it lands:
      # build and test, but PUBLISH's own guard keeps it from publishing, so an
      # unmerged PR never produces a release. Its version stage is "-canary"
      # (resolve-version.sh's pull_request rule) — this exact commit was
      # never merged into dev, and the label says so.
      - if: github.event_name == 'pull_request'
        uses: actions/upload-artifact@v4
        with:
          name: XS56K-${{{{ steps.version.outputs.full }}}}-{os}-{arch}-{config}
          path: ${{{{ steps.build.outputs.artefact-dir }}}}
          if-no-files-found: error
"""


def workflow(os_name: str, arch: str, runner: str, config: str, stage: str) -> tuple[str, str]:
    name = f"{os_name}-{arch}-{config}-{stage}"
    _, permission, human = STREAMS[stage]
    tail = {"prod": TAG_GUARD + PUBLISH, "dev": PUBLISH + DEV_PR_UPLOAD, "canary": CANARY_UPLOAD}[stage]
    permissions = f"  contents: {permission}\n"
    body = (
        GENERATED_HEADER.format(name=name)
        + f"name: {name}\n\non:\n{triggers_for(stage, name)}\npermissions:\n"
          f"{permissions}\njobs:\n  {name}:\n"
          f"    name: {os_name} {arch} {config.capitalize()} ({human})\n"
          f"    runs-on: {runner}\n"
        + CHECKOUT.format(os=os_name, config=config)
        + tail.format(os=os_name, arch=arch, config=config)
    )
    return name, body


def generate() -> dict[str, str]:
    out = {}
    for os_name, arch, runner in PLATFORMS:
        for stage, (configs, _, _) in STREAMS.items():
            for config in configs:
                name, body = workflow(os_name, arch, runner, config, stage)
                out[f"{name}.yml"] = body
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true",
                        help="fail if any generated workflow is missing or out of date")
    args = parser.parse_args()

    stale = []
    for filename, body in generate().items():
        target = WORKFLOWS / filename
        if args.check:
            # encoding="utf-8" is not optional here: the header carries an em
            # dash, and the platform default text encoding is not UTF-8 on
            # every OS — XplorerEditor's own RQ-BLD-033 finding, applying
            # unchanged to this script since it was adapted from theirs.
            if not target.exists() or target.read_text(encoding="utf-8") != body:
                stale.append(filename)
        else:
            # newline="\n" stops write_text from translating "\n" to the
            # platform's own line separator — without it, a Windows run
            # rewrites every file to CRLF, whereas the committed files (and a
            # Linux/macOS run) are LF.
            target.write_text(body, encoding="utf-8", newline="\n")

    if args.check:
        if stale:
            print("Out of date — run python3 juce/tools/generate_workflows.py:", file=sys.stderr)
            for name in stale:
                print(f"  {name}", file=sys.stderr)
            return 1
        print(f"{len(generate())} generated workflows are up to date")
        return 0

    print(f"Wrote {len(generate())} workflows to {WORKFLOWS}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
