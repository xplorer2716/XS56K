"""Tests of juce/tools/generate_akm_items.py, run by ctest when Python 3 is available.

Each test reads as a Gherkin scenario. [TASK-AKM-008, RQ-AKM-001, RQ-AKM-014, ADR-AKM-001 (DEC-AKM-003,
DEC-AKM-012)]
"""
import copy
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "juce" / "tools" / "generate_akm_items.py"
DATA = ROOT / "juce" / "akm" / "data" / "items.json"

# The spec rows of section 00 (Table 5): item 02 does not exist.
SECTION_00_ITEM_COUNT = 7


def run_script(*arguments):
    # PYTHONUTF8 forces the child's own stdout/stderr to UTF-8: without it, on Windows, the child
    # writes its console codepage (e.g. cp1252), and a byte outside that mapping (the "–" ranges
    # copied from the PDF, decoded here as UTF-8) crashes subprocess.run's background reader thread
    # silently, leaving .stdout/.stderr as None instead of raising where the test could see it.
    env = dict(os.environ, PYTHONUTF8="1")
    return subprocess.run([sys.executable, str(SCRIPT), *arguments], capture_output=True, text=True,
                          encoding="utf-8", env=env)


class ScriptTest(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory()
        self.addCleanup(self._directory.cleanup)
        self.directory = pathlib.Path(self._directory.name)

    def write_variant(self, change):
        """Writes a copy of the data file after `change(catalogue)` and returns its path."""
        catalogue = json.loads(DATA.read_text(encoding="utf-8"))
        change(catalogue)
        path = self.directory / "items.json"
        path.write_text(json.dumps(catalogue, indent=2), encoding="utf-8")
        return path

    @staticmethod
    def record(catalogue, item_id):
        return next(entry for entry in catalogue["items"] if entry["id"] == item_id)


class TableIsUpToDate(ScriptTest):
    def test_given_the_checked_in_table_when_checked_then_it_matches_the_data_file(self):
        result = run_script("--check")

        self.assertEqual(result.returncode, 0, result.stderr)

    def test_given_a_data_file_that_differs_when_checked_then_it_fails_and_names_the_table(self):
        variant = self.write_variant(lambda catalogue: self.record(catalogue, "SysExNotification")["args"][0]
                                     .update(max=2))

        result = run_script("--check", "--data", str(variant))

        self.assertEqual(result.returncode, 1)
        self.assertIn("ItemTable.generated.hpp", result.stderr)

    def test_given_a_missing_table_when_checked_then_it_fails(self):
        result = run_script("--check", "--output", str(self.directory / "absent.hpp"))

        self.assertEqual(result.returncode, 1)
        self.assertIn("absent.hpp", result.stderr)

    def test_given_the_data_file_when_generated_twice_then_the_output_is_identical_and_uses_line_feeds(self):
        first = self.directory / "first.hpp"
        second = self.directory / "second.hpp"

        self.assertEqual(run_script("--output", str(first)).returncode, 0)
        self.assertEqual(run_script("--output", str(second)).returncode, 0)

        self.assertEqual(first.read_bytes(), second.read_bytes())
        self.assertNotIn(b"\r", first.read_bytes())

    def test_given_the_data_file_when_generated_then_every_record_has_an_enumerator_and_a_table_entry(self):
        output = self.directory / "table.hpp"
        self.assertEqual(run_script("--output", str(output)).returncode, 0)
        text = output.read_text(encoding="utf-8")
        catalogue = json.loads(DATA.read_text(encoding="utf-8"))

        for entry in catalogue["items"]:
            self.assertIn(entry["id"] + ",", text, entry["id"])
        self.assertIn("ITEM_TABLE", text)
        self.assertIn("GENERATED", text)


class DataFileIsValidated(ScriptTest):
    def assert_refused(self, variant, expected_text):
        result = run_script("--check", "--data", str(variant))

        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn(expected_text, result.stderr)

    def test_given_a_format_the_schema_does_not_support_when_read_then_it_is_refused_naming_the_record(self):
        # "qword" is deferred like "string" was (DEC-AKM-003); unlike "string" (added by DEC-AKM-013,
        # TASK-AKM-014), it is still unsupported, so it stays a valid example of a rejected format.
        variant = self.write_variant(lambda catalogue: self.record(catalogue, "SysExEcho")["args"][0]
                                     .update(format="qword"))

        self.assert_refused(variant, "SysExEcho")

    def test_given_a_range_wider_than_its_format_when_read_then_it_is_refused(self):
        variant = self.write_variant(lambda catalogue: self.record(catalogue, "SysExEcho")["args"][0]
                                     .update(max=128))

        self.assert_refused(variant, "SysExEcho")

    def test_given_two_records_with_the_same_section_and_item_when_read_then_it_is_refused(self):
        def duplicate(catalogue):
            clone = copy.deepcopy(self.record(catalogue, "SysExQuery"))
            clone["id"] = "SysExQueryAgain"
            catalogue["items"].append(clone)

        self.assert_refused(self.write_variant(duplicate), "SysExQueryAgain")

    def test_given_a_get_without_a_reply_format_when_read_then_it_is_refused(self):
        variant = self.write_variant(lambda catalogue: self.record(catalogue, "SystemOsVersion").pop("reply"))

        self.assert_refused(variant, "SystemOsVersion")

    def test_given_a_record_in_an_undeclared_section_when_read_then_it_is_refused(self):
        variant = self.write_variant(lambda catalogue: self.record(catalogue, "SysExQuery").update(section="04"))

        self.assert_refused(variant, "SysExQuery")


class CoverageAgainstTheSpec(ScriptTest):
    def test_given_the_data_file_and_the_spec_rows_when_coverage_runs_then_section_00_is_fully_covered(self):
        result = run_script("--coverage")

        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(f"section 00: {SECTION_00_ITEM_COUNT} of {SECTION_00_ITEM_COUNT} spec rows covered", result.stdout)
        self.assertIn("unaccounted: none", result.stdout)

    def test_given_the_two_version_items_when_coverage_runs_then_the_partial_section_reports_what_it_leaves_out(self):
        result = run_script("--coverage")

        self.assertIn("section 02", result.stdout)
        self.assertIn("partial", result.stdout)

    def test_given_a_spec_row_without_a_record_in_a_complete_section_when_coverage_runs_then_it_is_listed(self):
        def remove_echo(catalogue):
            catalogue["items"] = [entry for entry in catalogue["items"] if entry["id"] != "SysExEcho"]

        result = run_script("--coverage", "--data", str(self.write_variant(remove_echo)))

        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("unaccounted", result.stdout + result.stderr)
        self.assertIn("00 &06", result.stdout + result.stderr)

    def test_given_a_record_absent_from_the_spec_when_coverage_runs_then_it_is_reported(self):
        def add_phantom(catalogue):
            phantom = copy.deepcopy(self.record(catalogue, "SysExQuery"))
            phantom.update(id="SysExPhantom", item="02")
            catalogue["items"].append(phantom)

        result = run_script("--coverage", "--data", str(self.write_variant(add_phantom)))

        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("SysExPhantom", result.stdout + result.stderr)

    def test_given_a_range_that_differs_from_the_spec_when_coverage_runs_then_the_mismatch_is_reported(self):
        variant = self.write_variant(lambda catalogue: self.record(catalogue, "SysExStillAlive")["args"][0]
                                     .update(max=2))

        result = run_script("--coverage", "--data", str(variant))

        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("SysExStillAlive", result.stdout + result.stderr)

    def test_given_a_wrong_argument_count_when_coverage_runs_then_the_mismatch_is_reported(self):
        variant = self.write_variant(lambda catalogue: self.record(catalogue, "SysExEcho")["args"].pop())

        result = run_script("--coverage", "--data", str(variant))

        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("SysExEcho", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
