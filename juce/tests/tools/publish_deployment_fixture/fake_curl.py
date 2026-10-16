"""Stand-in for the `curl` upload of .github/actions/publish-deployment/publish.sh, over the state file of fake_gh.py.

Like GitHub it answers 422 (curl exit code 22 under `-f`) when the release is published or already holds an asset of that name.
[TASK-BLD-013, RQ-BLD-016, ADR-BLD-006 (DEC-BLD-029)]
"""
import json
import os
import re
import sys

STATE = os.environ["FAKE_STATE"]


def main(arguments):
    url = arguments[-1]
    data = next(a for a in arguments if a.startswith("@"))[1:]
    match = re.search(r"/releases/(\d+)/assets\?name=([^&]+)$", url)
    if not match or not os.path.isfile(data):
        print(f"fake curl: unexpected call {arguments}", file=sys.stderr)
        return 2
    release_id, name = int(match.group(1)), match.group(2)
    with open(STATE, encoding="utf-8") as handle:
        state = json.load(handle)
    target = next((r for r in state["releases"] if r["id"] == release_id), None)
    if target is None or not target["draft"]:
        print("HTTP 422: Cannot upload assets to an immutable release.", file=sys.stderr)
        return 22
    if any(a["name"] == name for a in target["assets"]):
        print("HTTP 422: already_exists", file=sys.stderr)
        return 22
    target["assets"].append({"id": state["next_asset"], "name": name})
    state["next_asset"] += 1
    with open(STATE, "w", encoding="utf-8") as handle:
        json.dump(state, handle)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
