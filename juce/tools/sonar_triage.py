#!/usr/bin/env python3
"""Sorts the open SonarCloud issues of XS56K into test code and production code, and accepts the test-code ones.

The issues are read with the `sonar` CLI (`sonar api get /api/issues/search`), which holds the token in the
OS keychain: this script never sees one. The raw reply is written to a cache file outside the repository and
never printed whole; what is printed is counts by rule and by file, and a capped list of production issues.

Test code is a file under `juce/tests/` or under `.github/skills/<skill>/tests/` (DEC-QLT-001). The test-code
issues are accepted in SonarCloud, with a comment naming RQ-QLT-002, only with --apply; without it the script
says what it would do and sends nothing. An issue already accepted is skipped, so a second run sends nothing.

Usage:
    python3 juce/tools/sonar_triage.py [--severities BLOCKER,CRITICAL] [--limit 30] [--cache <file>] [--apply]

(On Windows the interpreter is usually `python`.)

Exit status: 0 when the run completed, 2 when it could not (the CLI is missing or not logged in, a call failed,
the result is more than the API can page through). No cache file is written for a failed read.

[TASK-QLT-001, RQ-QLT-002, RQ-QLT-006, RQ-QLT-008, ADR-QLT-001 (DEC-QLT-001, DEC-QLT-003)]
"""
import argparse
import collections
import json
import pathlib
import re
import subprocess
import sys
import tempfile

PROJECT_KEY = "xplorer2716_XS56K"
ORGANIZATION = "xplorer2716"
DEFAULT_SEVERITIES = "BLOCKER,CRITICAL"
PAGE_SIZE = 500                  # the API's maximum
API_RESULT_LIMIT = 10000         # SonarCloud refuses to page past 10 000 results
DEFAULT_LIST_LIMIT = 30
TOP_COUNTS = 15

# Test code, by path (DEC-QLT-001): the slash after "tests" keeps `juce/tests_old/` and `juce/testsupport/` out.
TEST_PATH = re.compile(r"^(juce/tests/|\.github/skills/[^/]+/tests/)")

# The statuses an issue is accepted from; ACCEPTED, FALSE_POSITIVE and FIXED are left alone. The CLI's
# `issueStatus` is the current field, `status` the legacy one. (RQ-QLT-008)
PENDING_STATUSES = {"OPEN", "CONFIRMED", "REOPENED"}

ACCEPT_TRANSITION = "accept"
ACCEPT_COMMENT = ("Not fixed on purpose: test code, whose style is not worth the churn of its automated changes "
                  "(owner decision 2026-10-11, RQ-QLT-002, DEC-QLT-001).")
TRANSITION_ENDPOINT = "/api/issues/do_transition"

EXIT_OK = 0
EXIT_TOOL_ERROR = 2


class ToolError(Exception):
    """The tool itself could not proceed; the message is what the CLI or the API said."""


def is_test_path(path):
    return TEST_PATH.match(path) is not None


def component_path(component):
    """The path of a SonarCloud component, which is "<project key>:<path>"."""
    return component.split(":", 1)[-1]


def classify(issues):
    """Returns (issues on test code, issues on production code), each in the order given."""
    tests, production = [], []
    for found in issues:
        (tests if is_test_path(component_path(found["component"])) else production).append(found)
    return tests, production


def sonar_call(method, endpoint, data=None):
    """One call of `sonar api`; returns the parsed JSON reply. The CLI may print a banner before it."""
    command = ["sonar", "api", method, endpoint]
    if data is not None:
        command += ["--data", json.dumps(data)]
    try:
        done = subprocess.run(command, capture_output=True, text=True, encoding="utf-8")
    except FileNotFoundError as error:
        raise ToolError("the `sonar` CLI was not found on the PATH") from error
    if done.returncode != 0:
        raise ToolError(f"sonar api {method} {endpoint} failed ({done.returncode}): {(done.stderr or done.stdout).strip()[:500]}")
    start = done.stdout.find("{")
    if start < 0:
        if method != "get":
            return {}
        raise ToolError(f"sonar api {method} {endpoint}: no JSON in the reply")
    try:
        return json.loads(done.stdout[start:])
    except ValueError as error:
        raise ToolError(f"sonar api {method} {endpoint}: the reply is not JSON ({error})") from error


def fetch_issues(call, severities, page_size=PAGE_SIZE):
    """Every open issue of the given severities, read page by page."""
    base = (f"/api/issues/search?organization={ORGANIZATION}&componentKeys={PROJECT_KEY}"
            f"&resolved=false&severities={','.join(severities)}&ps={page_size}")
    found, page = [], 1
    while True:
        reply = call("get", f"{base}&p={page}")
        total = reply["paging"]["total"]
        if total > API_RESULT_LIMIT:
            raise ToolError(f"{total} issues: more than the {API_RESULT_LIMIT} the API pages through; narrow the severities")
        found.extend(reply["issues"])
        if len(found) >= total or not reply["issues"]:
            return found
        page += 1


def is_pending(found):
    return (found.get("issueStatus") or found.get("status")) in PENDING_STATUSES


def accept_test_issues(call, issues, apply):
    """Accepts the pending test-code issues when `apply`; returns how many were sent."""
    tests, _ = classify(issues)
    pending = [found for found in tests if is_pending(found)]
    if not apply:
        print(f"dry run: would accept {len(pending)} test-code issues ({len(tests) - len(pending)} already handled); "
              "nothing sent, use --apply")
        return 0
    sent = 0
    for found in pending:
        try:
            call("post", TRANSITION_ENDPOINT, {"issue": found["key"], "transition": ACCEPT_TRANSITION, "comment": ACCEPT_COMMENT})
        except ToolError as error:
            raise ToolError(f"{error} (after {sent} accepted)") from error
        sent += 1
    print(f"accepted {sent} test-code issues ({len(tests) - len(pending)} were already handled)")
    return sent


def print_counts(title, counts):
    print(f"\n{title}:")
    for name, count in counts.most_common(TOP_COUNTS):
        print(f"  {count:5d}  {name}")


def print_report(issues, list_limit):
    """Counts by side, rule and file, and the first `list_limit` production issues; never the whole data."""
    tests, production = classify(issues)
    print(f"issues: {len(issues)}  test code: {len(tests)}  production code: {len(production)}")
    print_counts("test code by rule", collections.Counter(found["rule"] for found in tests))
    print_counts("production code by rule", collections.Counter(found["rule"] for found in production))
    print_counts("production code by file", collections.Counter(component_path(found["component"]) for found in production))
    if list_limit > 0 and production:
        print(f"\nproduction issues (first {min(list_limit, len(production))} of {len(production)}):")
        for found in production[:list_limit]:
            print(f"  {found['severity']:8} {found['rule']:18} {component_path(found['component'])}:{found.get('line', '-')}")
        if len(production) > list_limit:
            print(f"  ... {len(production) - list_limit} more (see the cache file)")


def default_cache():
    return pathlib.Path(tempfile.gettempdir()) / f"sonar_triage_{PROJECT_KEY}.json"


def parse_arguments(argv):
    parser = argparse.ArgumentParser(description="Sorts the open SonarCloud issues and accepts the test-code ones.")
    parser.add_argument("--severities", default=DEFAULT_SEVERITIES, help=f"comma-separated (default {DEFAULT_SEVERITIES})")
    parser.add_argument("--limit", type=int, default=DEFAULT_LIST_LIMIT, help="production issues to list (0: none)")
    parser.add_argument("--cache", type=pathlib.Path, default=default_cache(), help="file that receives the raw issues")
    parser.add_argument("--apply", action="store_true", help="accept the test-code issues in SonarCloud (default: dry run)")
    return parser.parse_args(argv)


def main(argv=None):
    arguments = parse_arguments(sys.argv[1:] if argv is None else argv)
    severities = [name.strip() for name in arguments.severities.split(",") if name.strip()]
    try:
        issues = fetch_issues(sonar_call, severities)
        arguments.cache.write_text(json.dumps(issues), encoding="utf-8", newline="\n")
        print(f"raw issues: {arguments.cache}")
        print_report(issues, arguments.limit)
        print()
        accept_test_issues(sonar_call, issues, arguments.apply)
    except ToolError as error:
        print(f"sonar_triage: {error}", file=sys.stderr)
        return EXIT_TOOL_ERROR
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
