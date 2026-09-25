#!/usr/bin/env bash
#
# Derives the product version from the commit being built — the ONLY place a
# version is computed. [RQ-BLD-009, ADR-BLD-003 (DEC-BLD-011)]
#
# Adapted from xplorer2716/XplorerEditor's own
# .github/actions/resolve-version/resolve-version.sh (AGENTS.md) — same
# derivation, same reasoning (see ADR-BLD-003), same stage names ("preprod",
# not "dev" — owner decision, aligned with XplorerEditor's own naming after
# an initial deviation was caught and reverted, see ADR-BLD-002) and no
# "XPL_" prefix on the env var (owner decision, TASK-BLD-002 follow-up; see
# ADR-BLD-001's amended DEC-BLD-003).
#
# Three forms, because no single string can serve every consumer:
#
#   numeric  2026.8.19.1740   CMake, JUCE target, Windows FILEVERSION, CFBundleVersion
#   display  2026.08.19-1740  tag, release title, deployment archive names
#   full     …-1740-preprod   About box, ProductVersion string
#
# PURE FUNCTION OF THE COMMIT. No counter, no github.run_number, no repository
# variable, no wall-clock "now" — multiple workflows can build one commit
# concurrently and must compute the identical string without communicating,
# and a re-run of a failed workflow must produce the version it produced the
# first time.
set -euo pipefail

ref="${REF:-${GITHUB_REF:-}}"

# main or a tag = production; dev = the pre-production stream; anything else
# (a feature branch, or a pull_request's refs/pull/N/merge) = canary.
# A tag ref is production, not "whatever refs/heads/main is": cut-deployment
# (TASK-BLD-007) triggers production by pushing a tag, so its ref is
# refs/tags/…, never refs/heads/main.
case "$ref" in
    refs/heads/main | refs/tags/*) stage="" ;;
    refs/heads/dev)                stage="-preprod" ;;
    *)                             stage="-canary" ;;
esac

# Formatted by git, not by date(1): a GNU-only `date -d @epoch` would disagree
# with the BSD one on a macOS runner. TZ=UTC makes every runner agree on which
# day and which minute a commit belongs to.
stamp="$(TZ=UTC git log -1 --format=%cd --date=format-local:'%Y %m %d %H%M' HEAD)"
read -r year month day hhmm <<<"$stamp"

# ISO 8601 UTC commit timestamp, taken from the same commit as everything
# else rather than from the clock, so rebuilding a commit is reproducible.
timestamp="$(TZ=UTC git log -1 --format=%cd --date=format-local:'%Y-%m-%dT%H:%M:%SZ' HEAD)"

# 10# forces base ten: 08 is not a valid octal literal, and a version derived
# in August would otherwise fail arithmetic expansion outright.
numeric="${year}.$((10#$month)).$((10#$day)).$((10#$hhmm))"
display="${year}.${month}.${day}-${hhmm}"
full="${display}${stage}"

{
    echo "numeric=${numeric}"
    echo "display=${display}"
    echo "full=${full}"
    echo "stage=${stage#-}"
    echo "timestamp=${timestamp}"
} >> "${GITHUB_OUTPUT:-/dev/stdout}"

echo "Resolved version: ${full} (numeric ${numeric})" >&2
