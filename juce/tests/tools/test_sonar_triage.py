"""Tests of juce/tools/sonar_triage.py, run by ctest when Python 3 is available.

Each test reads as a Gherkin scenario of RQ-QLT-002, RQ-QLT-006 or RQ-QLT-008. SonarCloud and the `sonar`
CLI are never reached: the script's one call to the CLI is a function the tests replace.
[TASK-QLT-001, RQ-QLT-002, RQ-QLT-006, RQ-QLT-008, ADR-QLT-001 (DEC-QLT-001, DEC-QLT-003)]
"""
import contextlib
import importlib.util
import io
import json
import pathlib
import subprocess
import tempfile
import unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "juce" / "tools" / "sonar_triage.py"

_SPEC = importlib.util.spec_from_file_location("sonar_triage", SCRIPT)
triage = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(triage)

PROJECT = "xplorer2716_XS56K"
TEST_FILE = "juce/tests/akm/ExecutorTests.cpp"
SKILL_TEST_FILE = ".github/skills/agnos-index/tests/test-build-index.sh"
PRODUCTION_FILE = "juce/akm/src/RealScheduler.cpp"
MOCK_FILE = "juce/midi/src/MockMidiBackend.cpp"


def issue(key, path, rule="cpp:S3608", severity="CRITICAL", status="OPEN", line=10):
    return {"key": key, "rule": rule, "severity": severity, "issueStatus": status,
            "component": f"{PROJECT}:{path}", "line": line, "message": f"message of {rule}"}


class FakeSonar:
    """Stands for the `sonar` CLI: serves issues page by page and records what is posted."""

    def __init__(self, issues, fail_on=None):
        self.issues = issues
        self.fail_on = fail_on
        self.gets = []
        self.posts = []

    def __call__(self, method, endpoint, data=None):
        if self.fail_on and self.fail_on in endpoint:
            raise triage.ToolError("sonar api failed: not logged in")
        if method == "get":
            self.gets.append(endpoint)
            query = dict(part.split("=", 1) for part in endpoint.split("?", 1)[1].split("&"))
            size, page = int(query["ps"]), int(query["p"])
            chunk = self.issues[(page - 1) * size:page * size]
            return {"paging": {"pageIndex": page, "pageSize": size, "total": len(self.issues)}, "issues": chunk}
        self.posts.append((endpoint, data))
        return {}


class Classification(unittest.TestCase):
    def test_a_file_under_juce_tests_is_test_code(self):
        # Given a file under juce/tests/, When classified, Then it is test code.
        self.assertTrue(triage.is_test_path(TEST_FILE))
        self.assertTrue(triage.is_test_path("juce/tests/support/src/RealSamplerSuite.cpp"))

    def test_a_file_under_a_skills_tests_folder_is_test_code(self):
        # Given a file under .github/skills/<skill>/tests/, When classified, Then it is test code.
        self.assertTrue(triage.is_test_path(SKILL_TEST_FILE))

    def test_other_files_are_production_code(self):
        # Given a library source, a skill script and the mock backend, When classified, Then each is production code.
        for path in (PRODUCTION_FILE, MOCK_FILE, ".github/skills/agnos-index/scripts/build-index.sh",
                     "juce/tools/generate_akm_items.py", "documents/_index/build_sysex_index.py"):
            self.assertFalse(triage.is_test_path(path), path)

    def test_a_name_that_only_starts_like_the_test_folder_is_production_code(self):
        # Given juce/tests_old/ and juce/testsupport/, When classified, Then they are not under juce/tests/.
        self.assertFalse(triage.is_test_path("juce/tests_old/a.cpp"))
        self.assertFalse(triage.is_test_path("juce/testsupport/a.cpp"))

    def test_the_project_prefix_of_a_component_is_removed(self):
        # Given a SonarCloud component "project:path", When its path is read, Then the project is gone.
        self.assertEqual(triage.component_path(f"{PROJECT}:{TEST_FILE}"), TEST_FILE)
        self.assertEqual(triage.component_path(TEST_FILE), TEST_FILE)

    def test_issues_are_split_into_test_and_production(self):
        # Given issues on test code and on production code, When classified, Then each lands on its side.
        issues = [issue("a", TEST_FILE), issue("b", PRODUCTION_FILE), issue("c", SKILL_TEST_FILE), issue("d", MOCK_FILE)]
        tests, production = triage.classify(issues)
        self.assertEqual([i["key"] for i in tests], ["a", "c"])
        self.assertEqual([i["key"] for i in production], ["b", "d"])


class Fetching(unittest.TestCase):
    def test_every_page_is_read(self):
        # Given more issues than a page holds, When fetched, Then every page is read and every issue returned.
        sonar = FakeSonar([issue(str(n), TEST_FILE) for n in range(7)])
        found = triage.fetch_issues(sonar, ["BLOCKER", "CRITICAL"], page_size=3)
        self.assertEqual(len(found), 7)
        self.assertEqual(len(sonar.gets), 3)

    def test_the_request_asks_for_the_severities_of_the_project_still_open(self):
        # Given severities, When fetched, Then the request names the project, the organization and them.
        sonar = FakeSonar([])
        triage.fetch_issues(sonar, ["BLOCKER", "CRITICAL"], page_size=500)
        request = sonar.gets[0]
        for part in (f"componentKeys={PROJECT}", f"organization={triage.ORGANIZATION}", "severities=BLOCKER,CRITICAL",
                     "resolved=false"):
            self.assertIn(part, request)

    def test_a_result_the_api_cannot_page_through_is_refused(self):
        # Given a total above the API's 10 000 limit, When fetched, Then the tool stops and says to narrow the severities.
        sonar = mock.Mock(return_value={"paging": {"total": triage.API_RESULT_LIMIT + 1}, "issues": []})
        with self.assertRaises(triage.ToolError) as context:
            triage.fetch_issues(sonar, ["MAJOR"], page_size=500)
        self.assertIn("narrow", str(context.exception))

    def test_a_failing_sonar_call_is_a_tool_error(self):
        # Given a sonar call that fails, When fetched, Then the error reaches the caller.
        with self.assertRaises(triage.ToolError):
            triage.fetch_issues(FakeSonar([], fail_on="/api/issues/search"), ["BLOCKER"], page_size=500)


class Acceptance(unittest.TestCase):
    def setUp(self):
        self.issues = [issue("t1", TEST_FILE), issue("t2", SKILL_TEST_FILE), issue("p1", PRODUCTION_FILE),
                       issue("m1", MOCK_FILE)]

    def test_without_apply_nothing_is_sent_and_the_count_is_reported(self):
        # Given test-code issues, When the acceptance runs without --apply, Then no request is posted and it says how many it would accept.
        sonar = FakeSonar(self.issues)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            sent = triage.accept_test_issues(sonar, self.issues, apply=False)
        self.assertEqual(sent, 0)
        self.assertEqual(sonar.posts, [])
        self.assertIn("would accept 2", out.getvalue())

    def test_with_apply_each_test_issue_is_accepted_with_the_comment(self):
        # Given test-code issues, When the acceptance runs with --apply, Then each gets the accept transition and a comment naming RQ-QLT-002.
        sonar = FakeSonar(self.issues)
        with contextlib.redirect_stdout(io.StringIO()):
            sent = triage.accept_test_issues(sonar, self.issues, apply=True)
        self.assertEqual(sent, 2)
        self.assertEqual([endpoint for endpoint, _ in sonar.posts], ["/api/issues/do_transition"] * 2)
        self.assertEqual([data["issue"] for _, data in sonar.posts], ["t1", "t2"])
        for _, data in sonar.posts:
            self.assertEqual(data["transition"], "accept")
            self.assertIn("RQ-QLT-002", data["comment"])

    def test_a_production_issue_is_never_touched(self):
        # Given production issues, When the acceptance runs with --apply, Then none is posted.
        sonar = FakeSonar(self.issues)
        with contextlib.redirect_stdout(io.StringIO()):
            triage.accept_test_issues(sonar, self.issues, apply=True)
        self.assertNotIn("p1", [data["issue"] for _, data in sonar.posts])
        self.assertNotIn("m1", [data["issue"] for _, data in sonar.posts])

    def test_an_issue_already_accepted_is_skipped(self):
        # Given every test issue already ACCEPTED, When the acceptance runs with --apply, Then it sends nothing.
        accepted = [issue("t1", TEST_FILE, status="ACCEPTED"), issue("t2", SKILL_TEST_FILE, status="ACCEPTED")]
        sonar = FakeSonar(accepted)
        with contextlib.redirect_stdout(io.StringIO()):
            sent = triage.accept_test_issues(sonar, accepted, apply=True)
        self.assertEqual(sent, 0)
        self.assertEqual(sonar.posts, [])

    def test_a_confirmed_or_reopened_issue_is_accepted_too(self):
        # Given test-code issues that are CONFIRMED or REOPENED, When the acceptance runs, Then they are accepted.
        pending = [issue("c", TEST_FILE, status="CONFIRMED"), issue("r", TEST_FILE, status="REOPENED")]
        sonar = FakeSonar(pending)
        with contextlib.redirect_stdout(io.StringIO()):
            sent = triage.accept_test_issues(sonar, pending, apply=True)
        self.assertEqual(sent, 2)


class Report(unittest.TestCase):
    def setUp(self):
        self.issues = ([issue(f"t{n}", TEST_FILE) for n in range(3)] + [issue("t9", SKILL_TEST_FILE, rule="shelldre:S7688")]
                       + [issue(f"p{n}", PRODUCTION_FILE, rule="cpp:S5404", severity="BLOCKER", line=80 + n) for n in range(5)])

    def render(self, limit):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            triage.print_report(self.issues, list_limit=limit)
        return out.getvalue()

    def test_the_counts_by_side_rule_and_file_are_printed(self):
        # Given 4 test and 5 production issues, When reported, Then the counts by side, rule and file are in the output.
        text = self.render(limit=0)
        self.assertIn("test code: 4", text)
        self.assertIn("production code: 5", text)
        self.assertIn("cpp:S5404", text)
        self.assertIn(PRODUCTION_FILE, text)

    def test_the_list_of_production_issues_is_capped_and_says_how_many_are_left(self):
        # Given 5 production issues and a limit of 2, When reported, Then two are listed and the rest is counted.
        text = self.render(limit=2)
        self.assertEqual(text.count("RealScheduler.cpp:"), 2)
        self.assertIn("3 more", text)

    def test_a_limit_of_zero_lists_none(self):
        # Given a limit of 0, When reported, Then no production issue is listed.
        self.assertNotIn("RealScheduler.cpp:8", self.render(limit=0))


class SonarCall(unittest.TestCase):
    def completed(self, stdout, returncode=0, stderr=""):
        return subprocess.CompletedProcess(args=[], returncode=returncode, stdout=stdout, stderr=stderr)

    def test_the_banner_before_the_json_is_ignored(self):
        # Given the CLI printing a banner before its JSON, When called, Then the JSON is returned.
        with mock.patch.object(triage.subprocess, "run", return_value=self.completed('  banner\n{"issues": [1]}\n')) as run:
            reply = triage.sonar_call("get", "/api/issues/search?ps=1")
        self.assertEqual(reply, {"issues": [1]})
        self.assertEqual(run.call_args.args[0][:4], ["sonar", "api", "get", "/api/issues/search?ps=1"])

    def test_a_post_carries_its_body_as_json(self):
        # Given a post with data, When called, Then the body is passed with --data as JSON.
        with mock.patch.object(triage.subprocess, "run", return_value=self.completed("{}")) as run:
            triage.sonar_call("post", "/api/issues/do_transition", {"issue": "k", "transition": "accept"})
        command = run.call_args.args[0]
        self.assertEqual(command[:4], ["sonar", "api", "post", "/api/issues/do_transition"])
        self.assertEqual(json.loads(command[command.index("--data") + 1]), {"issue": "k", "transition": "accept"})

    def test_a_non_zero_exit_is_a_tool_error_with_the_cli_message(self):
        # Given a CLI that fails, When called, Then the error carries its message.
        failed = self.completed("", returncode=1, stderr="Authentication required")
        with mock.patch.object(triage.subprocess, "run", return_value=failed):
            with self.assertRaises(triage.ToolError) as context:
                triage.sonar_call("get", "/api/issues/search?ps=1")
        self.assertIn("Authentication required", str(context.exception))

    def test_a_reply_without_json_is_a_tool_error(self):
        # Given a CLI that prints no JSON, When called, Then it is a tool error.
        with mock.patch.object(triage.subprocess, "run", return_value=self.completed("nothing here")):
            with self.assertRaises(triage.ToolError):
                triage.sonar_call("get", "/api/issues/search?ps=1")

    def test_a_missing_cli_is_a_tool_error(self):
        # Given no sonar executable, When called, Then it is a tool error that says so.
        with mock.patch.object(triage.subprocess, "run", side_effect=FileNotFoundError()):
            with self.assertRaises(triage.ToolError) as context:
                triage.sonar_call("get", "/api/issues/search?ps=1")
        self.assertIn("sonar", str(context.exception))


class Main(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory()
        self.addCleanup(self._directory.cleanup)
        self.cache = pathlib.Path(self._directory.name) / "issues.json"

    def run_main(self, sonar, *arguments):
        out, err = io.StringIO(), io.StringIO()
        with mock.patch.object(triage, "sonar_call", sonar), contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            status = triage.main(["--cache", str(self.cache), *arguments])
        return status, out.getvalue(), err.getvalue()

    def test_the_raw_issues_are_kept_in_the_cache_file_and_not_printed_whole(self):
        # Given issues, When the tool runs, Then the cache holds them all and the output holds counts only.
        issues = [issue(str(n), TEST_FILE) for n in range(40)]
        status, out, _ = self.run_main(FakeSonar(issues))
        self.assertEqual(status, triage.EXIT_OK)
        self.assertEqual(len(json.loads(self.cache.read_text(encoding="utf-8"))), 40)
        self.assertLess(len(out.splitlines()), 30)

    def test_a_failing_call_exits_2_with_the_message_and_writes_no_cache(self):
        # Given a failing sonar call, When the tool runs, Then it exits with 2, prints the message and writes no cache file.
        status, _, err = self.run_main(FakeSonar([], fail_on="/api/issues/search"))
        self.assertEqual(status, triage.EXIT_TOOL_ERROR)
        self.assertIn("not logged in", err)
        self.assertFalse(self.cache.exists())

    def test_apply_twice_sends_nothing_the_second_time(self):
        # Given an acceptance that ran, When it runs again on the resulting state, Then it posts nothing and exits 0.
        issues = [issue("t1", TEST_FILE), issue("t2", TEST_FILE), issue("p1", PRODUCTION_FILE)]
        first = FakeSonar(issues)
        self.run_main(first, "--apply")
        self.assertEqual(len(first.posts), 2)
        after = [issue("t1", TEST_FILE, status="ACCEPTED"), issue("t2", TEST_FILE, status="ACCEPTED"), issue("p1", PRODUCTION_FILE)]
        second = FakeSonar(after)
        status, _, _ = self.run_main(second, "--apply")
        self.assertEqual(status, triage.EXIT_OK)
        self.assertEqual(second.posts, [])

    def test_without_apply_main_posts_nothing(self):
        # Given test-code issues, When the tool runs without --apply, Then nothing is posted.
        sonar = FakeSonar([issue("t1", TEST_FILE)])
        status, out, _ = self.run_main(sonar)
        self.assertEqual(status, triage.EXIT_OK)
        self.assertEqual(sonar.posts, [])
        self.assertIn("would accept 1", out)

    def test_the_default_severities_are_blocker_and_critical(self):
        # Given no --severities, When the tool runs, Then it asks for BLOCKER and CRITICAL.
        sonar = FakeSonar([])
        self.run_main(sonar)
        self.assertIn("severities=BLOCKER,CRITICAL", sonar.gets[0])


if __name__ == "__main__":
    unittest.main()
