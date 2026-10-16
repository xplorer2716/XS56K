"""Tests of juce/tools/tidy.py, run by ctest when Python 3 is available.

Each test reads as a Gherkin scenario of RQ-QLT-010 or RQ-QLT-013. clang-tidy, git and the sources are replaced: the tests need none of them.
[TASK-QLT-024, RQ-QLT-010, RQ-QLT-013, ADR-QLT-002 (DEC-QLT-011, DEC-QLT-012)]
"""
import contextlib
import importlib.util
import io
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "juce" / "tools" / "tidy.py"

_SPEC = importlib.util.spec_from_file_location("tidy", SCRIPT)
tidy = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(tidy)

EDIT = "juce/app/core/src/Edit.cpp"
MODEL = "juce/app/core/src/Model.cpp"
EDIT_HEADER = "juce/app/core/include/app/Edit.hpp"
MODEL_HEADER = "juce/app/core/include/app/Model.hpp"
TEST_SOURCE = "juce/tests/app/EditTests.cpp"
GENERATED = "juce/app/core/include/app/EditorLayouts.generated.hpp"

SOURCES = {
    EDIT: '#include "app/Edit.hpp"\n',
    MODEL: '#include "app/Model.hpp"\n',
    MODEL_HEADER: '#include "app/Edit.hpp"\n',
    EDIT_HEADER: "#pragma once\n",
    TEST_SOURCE: '#include "app/Edit.hpp"\n',
}
MSVC_COMMAND = r'C:\VS\bin\cl.exe /nologo /TP -c '


def entry(rel, command="c++ -c "):
    return {"directory": str(ROOT), "file": str(ROOT / rel), "command": command + str(ROOT / rel)}


DATABASE = [entry(EDIT), entry(MODEL), entry(TEST_SOURCE)]


def finding_line(rel, line, check, message="something"):
    return f"{ROOT / rel}:{line}:5: warning: {message} [{check}]"


class FakeGit:
    def __init__(self, changed, untracked=()):
        self.changed, self.untracked = changed, untracked

    def __call__(self, *arguments):
        if arguments[0] == "merge-base":
            return "abc123\n"
        if arguments[0] == "diff":
            return "\n".join(self.changed) + "\n"
        return "\n".join(self.untracked) + "\n"


class Run:
    """A replaced clang-tidy: the output given for each source, and the sources it was run on."""

    def __init__(self, outputs=None, status=0):
        self.outputs, self.status, self.ran = outputs or {}, status, []

    def __call__(self, rel, entry_):
        self.ran.append(rel)
        output = self.outputs.get(rel, "")
        return (1 if output else self.status), output


def run_main(arguments, git=None, runner=None):
    runner = runner or Run()
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(io.StringIO()):
        status = tidy.main(arguments, git=git or FakeGit([]), runner=runner, read=lambda rel: SOURCES.get(rel, ""), files=list(SOURCES),
                           database=DATABASE)
    return status, out.getvalue(), runner


class SelectionTests(unittest.TestCase):
    def test_given_one_changed_source_when_run_with_no_argument_then_only_it_is_checked(self):
        status, _, runner = run_main([], git=FakeGit([EDIT, "README.md"]))
        self.assertEqual(status, tidy.EXIT_CLEAN)
        self.assertEqual(runner.ran, [EDIT])

    def test_given_a_changed_header_when_run_then_the_sources_that_include_it_are_checked_through_other_headers(self):
        _, _, runner = run_main([], git=FakeGit([EDIT_HEADER]))
        self.assertEqual(sorted(runner.ran), [EDIT, MODEL])

    def test_given_an_untracked_source_when_run_then_it_is_checked(self):
        _, _, runner = run_main([], git=FakeGit([], untracked=[MODEL]))
        self.assertEqual(runner.ran, [MODEL])

    def test_given_only_test_code_and_generated_files_changed_when_run_then_nothing_is_checked(self):
        status, out, runner = run_main([], git=FakeGit([TEST_SOURCE, GENERATED]))
        self.assertEqual(status, tidy.EXIT_CLEAN)
        self.assertEqual(runner.ran, [])
        self.assertIn("no production C++ source", out)

    def test_given_all_when_run_then_every_production_source_is_checked(self):
        _, _, runner = run_main(["--all"])
        self.assertEqual(sorted(runner.ran), [EDIT, MODEL])

    def test_given_tests_when_run_on_all_then_the_test_sources_are_checked_too(self):
        _, _, runner = run_main(["--all", "--tests"])
        self.assertEqual(sorted(runner.ran), [EDIT, MODEL, TEST_SOURCE])

    def test_given_tests_when_a_test_file_has_a_finding_then_it_is_reported(self):
        status, out, _ = run_main([TEST_SOURCE, "--tests"], runner=Run({TEST_SOURCE: finding_line(TEST_SOURCE, 3, "modernize-use-ranges")}))
        self.assertEqual(status, tidy.EXIT_FINDINGS)
        self.assertIn("EditTests.cpp:3:5", out)

    def test_given_a_named_file_when_run_then_it_is_checked_whatever_changed(self):
        _, _, runner = run_main([MODEL], git=FakeGit([EDIT]))
        self.assertEqual(runner.ran, [MODEL])


class FindingTests(unittest.TestCase):
    def test_given_findings_in_project_and_juce_files_when_run_then_only_the_project_ones_are_printed_and_the_status_is_1(self):
        juce_line = f"{ROOT / 'juce/build-tidy/_deps/juce-src/modules/juce_core/x.h'}:3:1: warning: copied [performance-unnecessary-value-param]"
        output = "\n".join([finding_line(EDIT, 12, "modernize-use-ranges"), juce_line])
        status, out, _ = run_main([EDIT], runner=Run({EDIT: output}))
        self.assertEqual(status, tidy.EXIT_FINDINGS)
        self.assertIn("juce/app/core/src/Edit.cpp:12:5: something [modernize-use-ranges]", out)
        self.assertNotIn("juce_core", out)

    def test_given_a_header_finding_reported_by_two_sources_when_run_then_it_is_printed_once(self):
        line = finding_line(EDIT_HEADER, 7, "bugprone-empty-catch")
        status, out, _ = run_main(["--all"], runner=Run({EDIT: line, MODEL: line}))
        self.assertEqual(status, tidy.EXIT_FINDINGS)
        self.assertEqual(out.count("Edit.hpp:7:5"), 1)
        self.assertIn("1  bugprone-empty-catch", out)

    def test_given_a_source_clang_tidy_cannot_read_when_run_then_the_status_is_2(self):
        error = f"{ROOT / 'juce/build-tidy/_deps/x.h'}:1:1: error: unknown type name 'foo' [clang-diagnostic-error]"
        status, out, _ = run_main([EDIT], runner=Run({EDIT: error}))
        self.assertEqual(status, tidy.EXIT_TOOL_ERROR)
        self.assertIn("could not analyse", out)

    def test_given_no_compile_database_when_run_then_it_says_how_and_the_status_is_2(self):
        errors = io.StringIO()
        with tempfile.TemporaryDirectory() as empty, contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(errors):
            status = tidy.main([EDIT], git=FakeGit([]), runner=Run(), read=lambda rel: "", files=[], build_dir=pathlib.Path(empty))
        self.assertEqual(status, tidy.EXIT_TOOL_ERROR)
        self.assertIn("--configure", errors.getvalue())


class CommandTests(unittest.TestCase):
    def test_given_an_msvc_command_when_run_then_exceptions_are_enabled_for_clang_tidy(self):
        self.assertEqual(tidy.extra_arguments(entry(EDIT, MSVC_COMMAND)), ["--extra-arg=/EHsc"])

    def test_given_a_gcc_or_clang_command_when_run_then_no_argument_is_added(self):
        self.assertEqual(tidy.extra_arguments(entry(EDIT, "/usr/bin/c++ -std=c++20 -c ")), [])
        self.assertEqual(tidy.extra_arguments(entry(EDIT, "/usr/bin/clang++ -c ")), [])


if __name__ == "__main__":
    unittest.main()
