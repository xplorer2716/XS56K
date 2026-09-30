"""Tests of juce/tools/mutate.py, run by ctest when Python 3 is available. Each test is one of
RQ-BLD-015's own Gherkin acceptance criteria, run against the small fixture project of
juce/tests/tools/mutate_fixture/ rather than the real juce/ tree, so each test configures and builds
in a few seconds. [TASK-BLD-012, RQ-BLD-015]
"""
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "juce" / "tools" / "mutate.py"
FIXTURE = ROOT / "juce" / "tests" / "tools" / "mutate_fixture"

# The fixture project's own generator/config: the platform default, Debug where a config applies
# (multi-config generators; a single-config one ignores it). [RQ-BLD-002]
CONFIG = "Debug"

BREAK_ADD = {
    "name": "break-add",
    "file": "fixture.cpp",
    "find": "return a + b;",
    "replace": "return a - b;",
    "target": "fixture_add",
    "tests": "^fixture_add$",
}
TOUCH_MULTIPLY_UNWATCHED = {
    "name": "touch-multiply-unwatched",
    "file": "fixture.cpp",
    "find": "return a * b;",
    "replace": "return a + b;",
    "target": "fixture_unused",
    "tests": "^fixture_unused$",
}
HANG = {
    "name": "hang",
    "file": "fixture.cpp",
    "find": "bool done = true;",
    "replace": "bool done = false;",
    "target": "fixture_hang",
    "tests": "^fixture_hang$",
}
NOT_FOUND = {
    "name": "not-found",
    "file": "fixture.cpp",
    "find": "this text is not in the fixture",
    "replace": "x",
    "target": "fixture_add",
    "tests": "^fixture_add$",
}
FOUND_TWICE = {
    "name": "found-twice",
    "file": "fixture.cpp",
    "find": "return",
    "replace": "return",
    "target": "fixture_add",
    "tests": "^fixture_add$",
}


def hash_fixture_sources():
    digest = hashlib.sha256()
    for path in sorted(FIXTURE.rglob("*")):
        if path.is_file():
            digest.update(path.read_bytes())
    return digest.hexdigest()


class MutateScriptTest(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory()
        self.addCleanup(self._directory.cleanup)
        self.work_dir = pathlib.Path(self._directory.name)
        self.before_hash = hash_fixture_sources()

    def tearDown(self):
        self.assertEqual(hash_fixture_sources(), self.before_hash,
                         "the fixture's own source tree must never be touched by a test run")

    def run_mutate(self, alterations, workers=1, timeout_seconds=15, extra_args=()):
        alterations_path = self.work_dir / "alterations.json"
        alterations_path.write_text(json.dumps(alterations), encoding="utf-8")
        json_out = self.work_dir / "results.json"
        command = [sys.executable, str(SCRIPT), "--project-root", str(FIXTURE), "--alterations",
                  str(alterations_path), "--workers", str(workers), "--timeout", str(timeout_seconds),
                  "--config", CONFIG, "--json-out", str(json_out), *extra_args]
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8")
        results = json.loads(json_out.read_text(encoding="utf-8")) if json_out.exists() else None
        return result, results

    def outcome_of(self, results, name):
        return next(entry for entry in results if entry["name"] == name)


class CaughtOrMissed(MutateScriptTest):
    def test_given_a_list_of_alterations_when_the_check_runs_then_the_tree_is_unchanged_and_each_alteration_is_caught_or_missed(self):
        process, results = self.run_mutate([BREAK_ADD, TOUCH_MULTIPLY_UNWATCHED])

        self.assertEqual(process.returncode, 1, process.stderr)  # one missed alteration
        self.assertEqual(self.outcome_of(results, "break-add")["outcome"], "caught")
        self.assertEqual(self.outcome_of(results, "touch-multiply-unwatched")["outcome"], "missed")


class Parallelism(MutateScriptTest):
    def test_given_two_alterations_and_two_workers_when_the_check_runs_then_they_run_at_the_same_time(self):
        two_hangs = [dict(HANG, name="hang-1"), dict(HANG, name="hang-2")]

        started_serial = time.monotonic()
        self.run_mutate(two_hangs, workers=1, timeout_seconds=3)
        serial_duration = time.monotonic() - started_serial

        started_parallel = time.monotonic()
        self.run_mutate(two_hangs, workers=2, timeout_seconds=3)
        parallel_duration = time.monotonic() - started_parallel

        # Each hang alone costs at least the 3 s timeout; run one after another that is at least 6 s,
        # run together at once it is close to 3 s. A generous margin keeps this from being flaky.
        self.assertLess(parallel_duration, serial_duration * 0.75,
                        f"serial {serial_duration:.1f}s, parallel {parallel_duration:.1f}s")


class TimeoutCaught(MutateScriptTest):
    def test_given_an_alteration_under_which_a_test_hangs_when_the_check_runs_then_it_is_reported_caught_by_timeout(self):
        process, results = self.run_mutate([HANG], timeout_seconds=3)

        self.assertEqual(process.returncode, 0, process.stderr)  # caught, not missed
        outcome = self.outcome_of(results, "hang")
        self.assertEqual(outcome["outcome"], "caught")
        self.assertIn("failed", outcome["detail"])


class SkippedWhenNotUniquelyFound(MutateScriptTest):
    def test_given_an_alteration_whose_text_is_found_no_time_when_the_check_runs_then_it_is_skipped(self):
        process, results = self.run_mutate([NOT_FOUND])

        self.assertEqual(process.returncode, 0, process.stderr)
        outcome = self.outcome_of(results, "not-found")
        self.assertEqual(outcome["outcome"], "skipped")
        self.assertIn("0 times", outcome["detail"])

    def test_given_an_alteration_whose_text_is_found_more_than_once_when_the_check_runs_then_it_is_skipped(self):
        process, results = self.run_mutate([FOUND_TWICE])

        self.assertEqual(process.returncode, 0, process.stderr)
        outcome = self.outcome_of(results, "found-twice")
        self.assertEqual(outcome["outcome"], "skipped")
        self.assertIn("2 times", outcome["detail"])  # "return" appears in both add() and multiply()


class OnlyWhatIsNeeded(MutateScriptTest):
    def test_given_an_alteration_of_one_source_file_when_the_check_runs_then_only_its_target_is_built_and_only_its_tests_run(self):
        process, results = self.run_mutate([BREAK_ADD], extra_args=["--keep-scratch"])
        self.assertEqual(process.returncode, 0, process.stderr)  # caught, not missed

        scratch_root = self._latest_scratch_root()
        try:
            build_dir = scratch_root / "break-add" / "_mutate_build"
            # A multi-config generator (Visual Studio) writes project scaffolding for every target at
            # configure time regardless of what is later built, so only the linked executables
            # themselves (not CMakeFiles/ intermediates or .vcxproj/.filters/... project files) say
            # what was actually built.
            built_names = {
                path.stem for path in build_dir.rglob("*")
                if path.is_file() and "CMakeFiles" not in path.parts and path.suffix in ("", ".exe")
            }
            self.assertIn("fixture_add", built_names, "the target the alteration names must be built")
            self.assertNotIn("fixture_hang", built_names,
                             "a target the alteration does not name must not be built")

            outcome = self.outcome_of(results, "break-add")
            self.assertNotIn("fixture_unused", outcome["log"])
            self.assertNotIn("fixture_hang", outcome["log"])
        finally:
            if scratch_root is not None:
                shutil.rmtree(scratch_root, ignore_errors=True)

    @staticmethod
    def _latest_scratch_root():
        candidates = sorted(pathlib.Path(tempfile.gettempdir()).glob("mutate-*"), key=lambda p: p.stat().st_mtime)
        return candidates[-1] if candidates else None


if __name__ == "__main__":
    unittest.main()
