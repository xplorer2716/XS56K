"""Tests of juce/tools/generate_sysex_coverage.py, run by ctest when Python 3 is available.

Each test reads as a Gherkin scenario. [TASK-MCP-069, RQ-MCP-058, RQ-MCP-059, RQ-MCP-060, RQ-MCP-062, RQ-MCP-063,
RQ-MCP-065, ADR-MCP-006 (DEC-MCP-035, DEC-MCP-039)]
"""
import importlib.util
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "juce" / "tools" / "generate_sysex_coverage.py"
DATA = ROOT / "juce" / "akm" / "data" / "items.json"
TSV = ROOT / "documents" / "_index" / "sysex_spec.items.tsv"
DECLARED = ROOT / "juce" / "tools" / "sysex_coverage_declared.json"

# The 13 sections of the spec's Table 4, in its order.
TABLE_4_ORDER = ["00", "02", "04", "06", "08", "0A", "0C", "0E", "10", "12", "14", "16", "20"]
TABLE_32_SECTIONS = ["2A", "2C", "2E", "32", "38", "3A", "3C", "3E"]
ROW_PREFIX = "| `&"


def load_script():
    spec = importlib.util.spec_from_file_location("generate_sysex_coverage", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def run_script(*arguments):
    # PYTHONUTF8: see test_generate_akm_items.py (a child on Windows writes its console codepage otherwise).
    env = dict(os.environ, PYTHONUTF8="1")
    return subprocess.run([sys.executable, str(SCRIPT), *arguments], capture_output=True, text=True,
                          encoding="utf-8", env=env)


class ScriptTest(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory()
        self.addCleanup(self._directory.cleanup)
        self.directory = pathlib.Path(self._directory.name)
        self.output = self.directory / "SYSEX-COVERAGE.md"

    def generate(self, *extra):
        return run_script("--output", str(self.output), *extra)

    def write_variant(self, change):
        """Writes a copy of the data file after `change(catalogue)` and returns its path."""
        catalogue = json.loads(DATA.read_text(encoding="utf-8"))
        change(catalogue)
        path = self.directory / "items.json"
        path.write_text(json.dumps(catalogue, indent=2), encoding="utf-8")
        return path

    def sections_of(self, text):
        """The section codes of the `## §XX` headings, in the order the file has them."""
        return [line[4:6] for line in text.splitlines() if line.startswith("## §")]


class RowsOfTheSections(ScriptTest):
    def test_given_the_catalogue_when_generated_then_it_has_one_table_per_section_in_the_order_of_table_4(self):
        result = self.generate()

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.sections_of(self.output.read_text(encoding="utf-8")), TABLE_4_ORDER)

    def test_given_the_catalogue_when_generated_then_every_item_is_a_row_of_its_section(self):
        items = json.loads(DATA.read_text(encoding="utf-8"))["items"]

        self.generate()
        text = self.output.read_text(encoding="utf-8")

        self.assertEqual(sum(1 for line in text.splitlines() if line.startswith(ROW_PREFIX)),
                         len(items) + len(TABLE_32_SECTIONS))
        for item in items:
            self.assertIn(f"`&{item['item']}{{{int(item['item'], 16)}}}` {item['name']}", text, item["id"])

    def test_given_a_set_and_a_get_when_generated_then_each_row_says_which_it_is(self):
        self.generate()
        text = self.output.read_text(encoding="utf-8")

        self.assertIn("`&00{0}` Query (Set)", text)
        self.assertIn("`&06{6}` Echo (Get)", text)

    def test_given_no_scan_when_generated_then_a_catalogued_message_is_catalogue_only_and_has_no_tool(self):
        self.generate()
        text = self.output.read_text(encoding="utf-8")

        self.assertIn("| `&06{6}` Echo (Get) | catalogue only | none |", text)

    def test_given_the_eight_sections_of_table_32_when_generated_then_they_are_rows_with_nothing_behind_them(self):
        self.generate()
        text = self.output.read_text(encoding="utf-8")

        table_32 = text[text.index("## Table 32"):]
        for section in TABLE_32_SECTIONS:
            row = next(line for line in table_32.splitlines() if line.startswith(f"{ROW_PREFIX}{section}{{"))
            self.assertTrue(row.endswith("| none | none |"), row)


    def test_given_item_codes_that_repeat_a_section_code_when_generated_then_table_32_heads_its_column_section_not_message(self):
        self.generate()
        text = self.output.read_text(encoding="utf-8")

        table_32 = text[text.index("## Table 32"):]
        self.assertIn("| Section | AKM function | MCP tool |", table_32)
        self.assertNotIn("| Message |", table_32)
        # `&2A` is an item of section 06 as well as a section of Table 32: the first table says so.
        self.assertIn("`&2A{42}` Get Zone Velocity->Start (Get)", text[:text.index("## Table 32")])


class TheCellsOfTheTwoColumns(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.script = load_script()

    def test_given_a_message_sent_by_two_functions_when_the_cell_is_made_then_it_lists_both(self):
        self.assertEqual(self.script.akm_cell(["getAllProgramNames", "getProgramNames"]),
                         "getAllProgramNames, getProgramNames")

    def test_given_a_catalogued_message_no_function_sends_when_the_cell_is_made_then_it_is_catalogue_only(self):
        self.assertEqual(self.script.akm_cell([]), "catalogue only")

    def test_given_a_tool_with_an_option_when_the_cell_is_made_then_the_option_follows_the_name(self):
        cell = self.script.mcp_cell([("set_parameter", None), ("load_file", "--allow-disk")], None)

        self.assertEqual(cell, "set_parameter, load_file (--allow-disk)")

    def test_given_a_message_no_tool_sends_when_the_cell_is_made_then_it_is_none(self):
        self.assertEqual(self.script.mcp_cell([], None), "none")

    def test_given_a_message_an_adr_excludes_when_the_cell_is_made_then_it_says_not_offered_and_the_decision(self):
        self.assertEqual(self.script.mcp_cell([], "DEC-MCP-028"), "not offered (DEC-MCP-028)")

    def test_given_a_name_with_a_pipe_when_the_row_is_made_then_the_pipe_is_escaped(self):
        row = self.script.message_cell({"item": "1A", "name": "a|b", "kind": "get"})

        self.assertEqual(row, "`&1A{26}` a\\|b (Get)")


class TheFileIsTheSameEveryTime(ScriptTest):
    def test_given_the_script_run_twice_when_the_files_are_compared_then_they_are_the_same_bytes(self):
        self.generate()
        first = self.output.read_bytes()
        self.generate()

        self.assertEqual(self.output.read_bytes(), first)

    def test_given_the_file_when_it_is_read_as_bytes_then_it_has_unix_line_ends_and_ends_with_one(self):
        self.generate()
        data = self.output.read_bytes()

        self.assertNotIn(b"\r", data)
        self.assertTrue(data.endswith(b"\n"))
        self.assertFalse(data.endswith(b"\n\n"))


class TheFileIsCheckedAgainstTheCode(ScriptTest):
    def test_given_the_file_just_written_when_checked_then_it_passes(self):
        self.generate()

        result = self.generate("--check")

        self.assertEqual(result.returncode, 0, result.stderr)

    def test_given_a_file_edited_by_hand_when_checked_then_it_fails_and_names_the_section(self):
        self.generate()
        text = self.output.read_text(encoding="utf-8")
        self.output.write_text(text.replace("`&06{6}` Echo (Get)", "`&06{6}` Echo (Set)"), encoding="utf-8", newline="\n")

        result = self.generate("--check")

        self.assertEqual(result.returncode, 1)
        self.assertIn("§00", result.stderr)

    def test_given_no_file_when_checked_then_it_fails_and_says_to_run_the_script(self):
        result = self.generate("--check")

        self.assertEqual(result.returncode, 1)
        self.assertIn("generate_sysex_coverage.py", result.stderr)


class TheCatalogueIsComparedWithTheSpec(ScriptTest):
    def test_given_an_item_removed_from_the_catalogue_when_run_then_it_fails_and_names_the_section_and_the_item(self):
        variant = self.write_variant(lambda catalogue: catalogue["items"].remove(
            next(item for item in catalogue["items"] if item["id"] == "SysExEcho")))

        result = self.generate("--data", str(variant))

        self.assertEqual(result.returncode, 1)
        self.assertIn("§00", result.stderr)
        self.assertIn("&06", result.stderr)
        self.assertFalse(self.output.exists())

    def test_given_an_item_the_spec_does_not_list_when_run_then_it_fails_and_names_it(self):
        def add(catalogue):
            extra = dict(next(item for item in catalogue["items"] if item["id"] == "SysExEcho"))
            extra.update(id="SysExNotInTheSpec", item="7E")
            catalogue["items"].append(extra)

        variant = self.write_variant(add)

        result = self.generate("--data", str(variant))

        self.assertEqual(result.returncode, 1)
        self.assertIn("&7E", result.stderr)

    def test_given_a_missing_input_when_run_then_it_exits_2_and_says_which(self):
        result = self.generate("--data", str(self.directory / "nothing.json"))

        self.assertEqual(result.returncode, 2)
        self.assertIn("nothing.json", result.stderr)


class TheDeclaredFile(ScriptTest):
    def test_given_the_declared_file_when_read_then_it_has_the_eight_sections_of_table_32(self):
        declared = json.loads(DECLARED.read_text(encoding="utf-8"))

        self.assertEqual([entry["section"] for entry in declared["table32"]["sections"]], TABLE_32_SECTIONS)

    def test_given_no_declared_file_when_run_then_it_exits_2_and_says_which(self):
        result = self.generate("--declared", str(self.directory / "missing-declared.json"))

        self.assertEqual(result.returncode, 2)
        self.assertIn("missing-declared.json", result.stderr)


if __name__ == "__main__":
    unittest.main()
