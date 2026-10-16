"""Tests of .github/actions/publish-deployment/publish.sh and of the publishing steps the generator writes, run by ctest when Python 3
and bash are available.

The script runs for real, against stand-ins of `gh` and `curl` (publish_deployment_fixture/) that keep the releases in a JSON file and
behave like GitHub where it matters here: no asset on a published (immutable) release, no second asset of one name, several drafts of one
tag allowed. Never run against GitHub. Each test reads as a Gherkin scenario of RQ-BLD-016.
[TASK-BLD-013, RQ-BLD-016, ADR-BLD-006 (DEC-BLD-029, DEC-BLD-030)]
"""
import importlib.util
import json
import os
import pathlib
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
SCRIPT = ROOT / ".github" / "actions" / "publish-deployment" / "publish.sh"
FIXTURE = pathlib.Path(__file__).resolve().parent / "publish_deployment_fixture"

_SPEC = importlib.util.spec_from_file_location("generate_workflows", ROOT / "juce" / "tools" / "generate_workflows.py")
generator = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(generator)

TAG = "2026.10.16-0945-preprod"
SHA = "0123456789abcdef0123456789abcdef01234567"
PREPROD_ARCHIVES = 6
PROD_ARCHIVES = 3


def find_bash():
    # Git's own msys bash first on Windows: `bash` on the PATH there can be the WSL launcher, and Git's `bin/bash.exe` puts
    # /mingw64/bin (the real curl) ahead of the PATH it is given, which the stand-ins must come before.
    if sys.platform == "win32":
        git_bash = pathlib.Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Git" / "usr" / "bin" / "bash.exe"
        if git_bash.is_file():
            return str(git_bash)
    return shutil.which("bash")


BASH = find_bash()


@unittest.skipUnless(BASH, "bash is not available")
class PublishScriptTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        base = pathlib.Path(self.directory.name)
        self.state_file = base / "state.json"
        self.repository = base / "repository"
        self.bin = base / "bin"
        self.temp = base / "runner-temp"
        for folder in (self.repository, self.bin, self.temp):
            folder.mkdir()
        self.write_state({"next_id": 1, "next_asset": 1, "releases": []})
        for command, fake in (("gh", "fake_gh.py"), ("curl", "fake_curl.py")):
            wrapper = self.bin / command
            wrapper.write_text(f'#!/usr/bin/env bash\nexec "{pathlib.Path(sys.executable).as_posix()}" "{(FIXTURE / fake).as_posix()}" "$@"\n',
                               encoding="utf-8", newline="\n")
            wrapper.chmod(wrapper.stat().st_mode | stat.S_IEXEC)
        git = ["git", "-C", str(self.repository), "-c", "user.name=test", "-c", "user.email=test@example.com"]
        subprocess.run(git + ["init", "-q"], check=True)
        subprocess.run(git + ["commit", "-q", "--allow-empty", "-m", "first"], check=True)
        # A real `gh` or `curl` ahead of the stand-ins would send a call to GitHub: stop before any script runs.
        resolved = subprocess.run([BASH, "--noprofile", "--norc", "-c", "command -v gh curl"], env=self.environment(),
                                  capture_output=True, text=True, check=True).stdout.split()
        self.assertEqual(len(resolved), 2)
        for command in resolved:
            self.assertIn("bin", command)
            self.assertNotIn("/mingw64/", command)
            self.assertNotIn("/usr/", command)

    def environment(self):
        return dict(os.environ, FAKE_STATE=str(self.state_file), PATH=str(self.bin) + os.pathsep + os.environ["PATH"])

    def write_state(self, state):
        self.state_file.write_text(json.dumps(state), encoding="utf-8")

    def state(self):
        return json.loads(self.state_file.read_text(encoding="utf-8"))

    def run_script(self, asset_name, stage="preprod", expected=PREPROD_ARCHIVES, tag=TAG):
        archive = self.temp / asset_name
        archive.write_bytes(b"archive of " + asset_name.encode())
        environment = dict(self.environment(), GH_TOKEN="token", ARCHIVE=str(archive), ASSET_NAME=asset_name,
                           DISPLAY=tag.split("-preprod")[0], TAG=tag, STAGE=stage, EXPECTED_ASSETS=str(expected),
                           GITHUB_REPOSITORY="owner/repository", GITHUB_SHA=SHA, RUNNER_TEMP=str(self.temp))
        # Invoked like the runner does for `shell: bash`: no profile, no rc file.
        return subprocess.run([BASH, "--noprofile", "--norc", str(SCRIPT)], cwd=self.repository, env=environment,
                              capture_output=True, text=True, check=False)

    def releases_of(self, tag=TAG):
        return [r for r in self.state()["releases"] if r["tag_name"] == tag]

    def test_given_six_workflows_when_they_attach_one_after_the_other_then_the_release_is_published_by_the_sixth(self):
        for number in range(1, PREPROD_ARCHIVES):
            result = self.run_script(f"XS56K-{number}.zip")
            self.assertEqual(result.returncode, 0, result.stderr)
            (release,) = self.releases_of()
            self.assertTrue(release["draft"], f"published after {number} archives")
            self.assertEqual(len(release["assets"]), number)
        result = self.run_script(f"XS56K-{PREPROD_ARCHIVES}.zip")
        self.assertEqual(result.returncode, 0, result.stderr)
        (release,) = self.releases_of()
        self.assertFalse(release["draft"])
        self.assertEqual(len(release["assets"]), PREPROD_ARCHIVES)

    def test_given_the_first_workflow_when_it_creates_the_release_then_it_is_a_draft_prerelease_on_the_commit(self):
        self.assertEqual(self.run_script("XS56K-1.zip").returncode, 0)
        (release,) = self.releases_of()
        self.assertEqual((release["target"], release["prerelease"], release["name"]),
                         (SHA, True, "XS56K 2026.10.16-0945 (pre-production)"))
        self.assertIn("NOT a production", release["body"])

    def test_given_a_preprod_release_when_published_then_it_is_not_marked_latest(self):
        for number in range(1, PREPROD_ARCHIVES + 1):
            self.run_script(f"XS56K-{number}.zip")
        self.assertEqual(self.releases_of()[0]["published_with"], "false")

    def test_given_a_prod_stage_when_the_third_archive_is_attached_then_it_is_published_as_the_latest_release(self):
        for number in range(1, PROD_ARCHIVES + 1):
            result = self.run_script(f"XS56K-{number}.zip", stage="", expected=PROD_ARCHIVES, tag="2026.10.16-0945")
            self.assertEqual(result.returncode, 0, result.stderr)
        (release,) = self.releases_of("2026.10.16-0945")
        self.assertEqual((release["draft"], release["prerelease"], release["published_with"], release["name"]),
                         (False, False, "true", "XS56K 2026.10.16-0945"))

    def test_given_a_competing_draft_created_between_the_lookup_and_the_creation_when_continuing_then_the_older_draft_keeps_the_archive(self):
        competitor = {"id": 5, "tag_name": TAG, "name": "competitor", "body": "", "target": SHA, "draft": True, "prerelease": True,
                      "published_with": None, "assets": []}
        self.write_state({"next_id": 6, "next_asset": 1, "releases": [competitor], "hide_from_first_list": True})
        result = self.run_script("XS56K-1.zip")
        self.assertEqual(result.returncode, 0, result.stderr)
        (release,) = self.releases_of()
        self.assertEqual(release["id"], 5)
        self.assertEqual([a["name"] for a in release["assets"]], ["XS56K-1.zip"])

    def test_given_a_published_release_when_a_workflow_attaches_then_it_fails_saying_the_release_is_published(self):
        for number in range(1, PREPROD_ARCHIVES + 1):
            self.run_script(f"XS56K-{number}.zip")
        result = self.run_script("XS56K-late.zip")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("published", result.stderr + result.stdout)
        self.assertEqual(len(self.releases_of()[0]["assets"]), PREPROD_ARCHIVES)

    def test_given_an_archive_already_on_the_draft_when_the_workflow_runs_again_then_the_draft_holds_one_archive_of_that_name(self):
        self.run_script("XS56K-1.zip")
        result = self.run_script("XS56K-1.zip")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual([a["name"] for a in self.releases_of()[0]["assets"]], ["XS56K-1.zip"])

    def test_given_a_publish_that_fails_because_another_workflow_published_when_the_release_is_published_then_the_script_succeeds(self):
        for number in range(1, PREPROD_ARCHIVES):
            self.run_script(f"XS56K-{number}.zip")
        state = self.state()
        state["patch_fails_after_publishing"] = True
        self.write_state(state)
        result = self.run_script(f"XS56K-{PREPROD_ARCHIVES}.zip")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(self.releases_of()[0]["draft"])

    def test_given_the_canary_stage_when_the_script_runs_then_it_fails_and_creates_nothing(self):
        result = self.run_script("XS56K-1.zip", stage="canary")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.state()["releases"], [])


class GeneratedPublishStepTests(unittest.TestCase):
    def publishing_workflows(self, stage):
        return {name: body for name, body in generator.generate().items() if name.endswith(f"-{stage}.yml")}

    def test_given_the_generator_when_the_preprod_workflows_are_read_then_each_expects_one_archive_per_preprod_workflow(self):
        workflows = self.publishing_workflows("preprod")
        self.assertEqual(len(workflows), PREPROD_ARCHIVES)
        for name, body in workflows.items():
            self.assertIn(f"expected-assets: {len(workflows)}\n", body, name)

    def test_given_the_generator_when_the_prod_workflows_are_read_then_each_expects_one_archive_per_prod_workflow(self):
        workflows = self.publishing_workflows("prod")
        self.assertEqual(len(workflows), PROD_ARCHIVES)
        for name, body in workflows.items():
            self.assertIn(f"expected-assets: {len(workflows)}\n", body, name)

    def test_given_the_generator_when_the_canary_workflows_are_read_then_none_publishes(self):
        for name, body in self.publishing_workflows("canary").items():
            self.assertNotIn("publish-deployment", body, name)
            self.assertNotIn("expected-assets", body, name)


if __name__ == "__main__":
    unittest.main()
