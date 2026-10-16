"""Stand-in for the `gh api` calls of .github/actions/publish-deployment/publish.sh, over a JSON file of releases.

It answers only the calls the script makes, and reads the `--jq` expression only for the value it must extract (the tag, the asset name,
`length`, `.id`, `.draft`). Like GitHub, it refuses to attach an asset to a published release and a second asset of the same name, and it
allows several draft releases of one tag. The state file also carries the switches the tests use to reproduce a race.
[TASK-BLD-013, RQ-BLD-016, ADR-BLD-006 (DEC-BLD-029, DEC-BLD-030)]
"""
import json
import os
import re
import sys

STATE = os.environ["FAKE_STATE"]


def load():
    with open(STATE, encoding="utf-8") as handle:
        return json.load(handle)


def save(state):
    with open(STATE, "w", encoding="utf-8") as handle:
        json.dump(state, handle)


def release(state, release_id):
    return next(r for r in state["releases"] if r["id"] == release_id)


def parse(arguments):
    method, jq, fields, path = "GET", "", {}, None
    index = 0
    while index < len(arguments):
        argument = arguments[index]
        if argument in ("-X", "--method"):
            method = arguments[index + 1]
            index += 2
        elif argument == "--jq":
            jq = arguments[index + 1]
            index += 2
        elif argument in ("-F", "-f", "--field", "--raw-field"):
            key, _, value = arguments[index + 1].partition("=")
            fields[key] = value
            index += 2
        else:
            path = argument
            index += 1
    return method, jq, fields, path


def field_value(value):
    if value.startswith("@"):
        with open(value[1:], encoding="utf-8") as handle:
            return handle.read()
    return value


def main(arguments):
    if arguments[0] != "api":
        print("fake gh: only `gh api` is stood in for", file=sys.stderr)
        return 2
    method, jq, fields, path = parse(arguments[1:])
    state = load()

    if method == "GET" and re.fullmatch(r"repos/[^/]+/[^/]+/releases\?per_page=100", path):
        if state.pop("hide_from_first_list", False):
            save(state)
            return 0
        tag = re.search(r'tag_name == "([^"]*)"', jq).group(1)
        for found in state["releases"]:
            if found["tag_name"] == tag:
                print(found["id"], str(found["draft"]).lower())
        return 0

    if method == "POST" and re.fullmatch(r"repos/[^/]+/[^/]+/releases", path):
        created = {"id": state["next_id"], "tag_name": fields["tag_name"], "name": fields["name"],
                   "body": field_value(fields["body"]), "target": fields["target_commitish"],
                   "draft": fields["draft"] == "true", "prerelease": fields["prerelease"] == "true",
                   "published_with": None, "assets": []}
        state["next_id"] += 1
        state["releases"].append(created)
        save(state)
        print(created["id"])
        return 0

    match = re.fullmatch(r"repos/[^/]+/[^/]+/releases/(\d+)", path)
    if match:
        release_id = int(match.group(1))
        if method == "DELETE":
            state["releases"] = [r for r in state["releases"] if r["id"] != release_id]
            save(state)
            return 0
        if method == "PATCH":
            target = release(state, release_id)
            target["draft"] = fields["draft"] == "true"
            target["published_with"] = fields["make_latest"]
            save(state)
            if state.get("patch_fails_after_publishing"):
                print("HTTP 422: a concurrent workflow published this release", file=sys.stderr)
                return 1
            return 0
        if method == "GET":
            print(str(release(state, release_id)["draft"]).lower())
            return 0

    match = re.fullmatch(r"repos/[^/]+/[^/]+/releases/(\d+)/assets\?per_page=100", path)
    if match and method == "GET":
        assets = release(state, int(match.group(1)))["assets"]
        if "length" in jq:
            print(len(assets))
        else:
            name = re.search(r'\.name == "([^"]*)"', jq).group(1)
            for asset in assets:
                if asset["name"] == name:
                    print(asset["id"])
        return 0

    match = re.fullmatch(r"repos/[^/]+/[^/]+/releases/assets/(\d+)", path)
    if match and method == "DELETE":
        asset_id = int(match.group(1))
        for each in state["releases"]:
            each["assets"] = [a for a in each["assets"] if a["id"] != asset_id]
        save(state)
        return 0

    print(f"fake gh: unexpected call {method} {path}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
