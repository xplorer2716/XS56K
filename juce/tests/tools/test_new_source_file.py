"""Tests of juce/tools/new_source_file.py, run by ctest when Python 3 is available.

Each test reads as a Gherkin scenario of RQ-QLT-001: the target of the script is a C++ file under `juce/`, whatever path it is given.
[TASK-QLT-021, RQ-QLT-001, ADR-QLT-001 (DEC-QLT-002)]
"""
import contextlib
import importlib.util
import io
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "juce" / "tools" / "new_source_file.py"

_SPEC = importlib.util.spec_from_file_location("new_source_file", SCRIPT)
tool = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(tool)

EXISTING_HEADER = "juce/app/core/include/app/Edit.hpp"


class SourcePathTests(unittest.TestCase):
    def test_given_a_cpp_path_under_juce_when_resolved_then_it_is_accepted(self):
        self.assertEqual(tool.source_path("juce/app/src/Anything.cpp"), (ROOT / "juce/app/src/Anything.cpp").resolve())

    def test_given_a_path_that_climbs_out_of_juce_when_resolved_then_it_is_refused(self):
        with self.assertRaises(ValueError):
            tool.source_path("juce/../README.cpp")

    def test_given_an_absolute_path_elsewhere_when_resolved_then_it_is_refused(self):
        with tempfile.TemporaryDirectory() as directory, self.assertRaises(ValueError):
            tool.source_path(str(pathlib.Path(directory) / "x.cpp"))

    def test_given_a_file_that_is_not_cpp_when_resolved_then_it_is_refused(self):
        with self.assertRaises(ValueError):
            tool.source_path("juce/tools/generate_design.py")


class MainTests(unittest.TestCase):
    def run_main(self, arguments):
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors), contextlib.redirect_stdout(io.StringIO()):
            return tool.main(arguments), errors.getvalue()

    def test_given_a_refused_path_when_creating_then_nothing_is_written_and_the_status_is_1(self):
        status, errors = self.run_main(["juce/../escaped.cpp"])
        self.assertEqual(status, 1)
        self.assertIn("not a", errors)
        self.assertFalse((ROOT / "escaped.cpp").exists())

    def test_given_a_refused_path_when_checking_then_the_status_is_1(self):
        status, _ = self.run_main(["--check", "README.md"])
        self.assertEqual(status, 1)

    def test_given_a_file_with_the_header_when_checking_then_the_status_is_0(self):
        status, _ = self.run_main(["--check", EXISTING_HEADER])
        self.assertEqual(status, 0)


if __name__ == "__main__":
    unittest.main()
