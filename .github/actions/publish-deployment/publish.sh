#!/usr/bin/env bash
# Attaches one archive to the draft release of this commit's tag, and publishes the release when the
# draft holds one archive per publishing workflow of the stream.
# [RQ-BLD-016, ADR-BLD-006 (DEC-BLD-029, DEC-BLD-030)]
#
# Called by action.yml, which sets: GH_TOKEN, ARCHIVE, ASSET_NAME, DISPLAY, TAG, STAGE, EXPECTED_ASSETS
# (GITHUB_REPOSITORY, GITHUB_SHA and RUNNER_TEMP come from the runner). Tested against stand-ins of `gh`
# and `curl` in juce/tests/tools/test_publish_deployment.py.
set -euo pipefail

# A canary build publishes nothing (RQ-BLD-005). Reaching here with that stage means a workflow was
# wired to the wrong action.
if [ "$STAGE" = canary ]; then
  echo "::error::canary builds publish no deployment; this action must not be called for them"
  exit 1
fi

echo "Attaching ${ASSET_NAME} to ${TAG}"

api="repos/${GITHUB_REPOSITORY}"

# "<id> <draft>" of the oldest release of this tag, drafts included, or nothing. A draft is found by
# its id only: the lookup of a draft by tag returns the newest one (DEC-BLD-030).
oldest_release() {
  gh api "${api}/releases?per_page=100" \
    --jq ".[] | select(.tag_name == \"${TAG}\") | \"\(.id) \(.draft)\"" | sort -n | sed -n 1p
}

oldest=$(oldest_release)
read -r id draft <<<"$oldest"

if [ -z "$id" ]; then
  notes="${RUNNER_TEMP}/deployment-notes.md"

  if [ -z "$STAGE" ]; then
    echo "Production deployment \`${DISPLAY}\`." > "$notes"
    # The merge into main is a NEW commit, so a production deployment
    # carries a different version from the pre-release that was
    # actually tested — accepted (ADR-BLD-003). Naming the promoted
    # build is what keeps the chain readable — derived from the
    # merge's second parent, which IS the dev tip that was merged.
    if promoted=$(git rev-parse --verify -q 'HEAD^2'); then
      promoted_version="$(TZ=UTC git log -1 --format=%cd \
          --date=format-local:'%Y.%m.%d-%H%M' "$promoted")-preprod"
      {
        echo
        echo "Promotes the pre-production deployment \`${promoted_version}\`."
      } >> "$notes"
    fi
    match='????.??.??-????'
    title="XS56K ${DISPLAY}"
    prerelease=false
  else
    {
      echo "**Pre-production deployment \`${DISPLAY}\`. This is NOT a production"
      echo "deployment** — it is built from \`dev\` for integration testing, and its"
      echo "binaries report \`-preprod\` in their reported version."
    } > "$notes"
    match='*-preprod'
    title="XS56K ${DISPLAY} (pre-production)"
    prerelease=true
  fi

  # Changes since the previous deployment OF THE SAME STREAM: a
  # production release must not list a dev release as its
  # predecessor, and vice versa.
  prev=$(git tag --list "$match" --sort=-creatordate \
         | grep -vx "$TAG" | head -1 || true)
  {
    echo
    if [ -n "$prev" ]; then
      echo "## Changes since \`${prev}\`"
      echo
      git log "${prev}..HEAD" --pretty='- %s (%h)'
    else
      echo "## Changes"
      echo
      git log -20 --pretty='- %s (%h)'
    fi
  } >> "$notes"

  # A draft, because a published release is immutable and takes no further archive. Its target is this
  # commit: the tag of a pre-production release does not exist until the release is published (that of a
  # production one was pushed by cut-deployment). Drafts of one tag do not conflict, so concurrent
  # creators make several: the oldest is kept, the others delete themselves.
  mine=$(gh api --method POST "${api}/releases" \
    -f tag_name="$TAG" -f name="$title" -F body=@"$notes" -f target_commitish="$GITHUB_SHA" \
    -F draft=true -F prerelease="$prerelease" --jq .id)
  oldest=$(oldest_release)
  read -r id draft <<<"$oldest"
  if [ "$id" != "$mine" ]; then
    gh api --method DELETE "${api}/releases/${mine}"
  fi
fi

if [ "$draft" != true ]; then
  echo "::error::the release ${TAG} is already published, so it is immutable and ${ASSET_NAME} cannot be attached to it"
  exit 1
fi

# An archive of the same name (a re-run before the release was published) is replaced.
stale=$(gh api "${api}/releases/${id}/assets?per_page=100" --jq ".[] | select(.name == \"${ASSET_NAME}\") | .id")
for asset in $stale; do
  gh api --method DELETE "${api}/releases/assets/${asset}"
done

# Uploaded under its own file name: package-deployment already named
# it for its version, os, architecture and configuration, which is
# what makes several assets of one deployment coexist without
# colliding. `curl` on the release's id, since `gh release upload` takes a tag.
curl --fail --silent --show-error --request POST \
  --header "Authorization: Bearer ${GH_TOKEN}" \
  --header "Accept: application/vnd.github+json" \
  --header "Content-Type: application/zip" \
  --data-binary "@${ARCHIVE}" \
  "https://uploads.github.com/${api}/releases/${id}/assets?name=${ASSET_NAME}"

# The upload is complete, so the workflow whose upload completes last counts every archive.
count=$(gh api "${api}/releases/${id}/assets?per_page=100" --jq length)
if [ "$count" -lt "$EXPECTED_ASSETS" ]; then
  echo "${count} of ${EXPECTED_ASSETS} archives are attached to ${TAG}: the workflow that attaches the last one publishes it"
  exit 0
fi

if [ -z "$STAGE" ]; then latest=true; else latest=false; fi
if ! gh api --method PATCH "${api}/releases/${id}" -F draft=false -f make_latest="$latest" >/dev/null; then
  # Another workflow that counted every archive published first: not an error.
  still_draft=$(gh api "${api}/releases/${id}" --jq .draft)
  if [ "$still_draft" != false ]; then
    echo "::error::the release ${TAG} could not be published"
    exit 1
  fi
fi
echo "Published ${TAG} with ${count} archives"
