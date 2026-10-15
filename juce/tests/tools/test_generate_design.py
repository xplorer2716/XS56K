"""Tests of juce/tools/generate_design.py, run by ctest when Python 3 is available.

Each test reads as a Gherkin scenario. [TASK-APP-007, RQ-APP-002, RQ-APP-005, ADR-APP-002 (DEC-APP-011, DEC-APP-012)]
"""
import copy
import importlib.util
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "juce" / "tools" / "generate_design.py"
TOKENS = ROOT / "juce" / "app" / "design" / "tokens.json"

MINIMAL = {
    "theme": "dark",
    "global": {"colour.grey.950": "#000000", "colour.grey.100": "#FFFFFF", "size.space.1": 4, "motion.hover": 80,
               "font.family.ui": "Inter"},
    "semantic": {"colour.surface.base": {"alias": "colour.grey.950", "use": "background"},
                 "colour.text.primary": {"alias": "colour.grey.100", "use": "text"},
                 "space.edge": {"alias": "size.space.1", "use": "edge"}},
    "component": {"row.text": "colour.text.primary", "row.gap": "size.space.1"},
    "contrast": {"minimum": 4.5, "text": ["colour.text.primary"], "on": ["colour.surface.base"], "pairs": []},
}

DOC = """# Doc

<!-- BEGIN GENERATED: global-colours -->
old
<!-- END GENERATED: global-colours -->

<!-- BEGIN GENERATED: semantic-colours -->
old
<!-- END GENERATED: semantic-colours -->

<!-- BEGIN GENERATED: contrast -->
old
<!-- END GENERATED: contrast -->
"""


def load_script():
    spec = importlib.util.spec_from_file_location("generate_design", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def run_script(*arguments):
    env = dict(os.environ, PYTHONUTF8="1")
    return subprocess.run([sys.executable, str(SCRIPT), *arguments], capture_output=True, text=True, env=env)


class ResolvingTokens(unittest.TestCase):
    def setUp(self):
        self.gen = load_script()

    def test_given_the_project_token_file_when_resolved_then_every_component_token_reaches_a_global_value(self):
        data = json.loads(TOKENS.read_text(encoding="utf-8"))
        resolved = self.gen.resolve_all(data)
        for name in data["component"]:
            self.assertIn(name, resolved)
        self.assertEqual(resolved["tab.underline.selected"], "#A897F2")
        self.assertEqual(resolved["knob.size.default"], 40)

    def test_given_an_alias_to_a_missing_token_when_resolved_then_it_fails_and_names_it(self):
        data = copy.deepcopy(MINIMAL)
        data["component"]["row.text"] = "colour.text.nowhere"
        with self.assertRaises(self.gen.TokenError) as raised:
            self.gen.resolve_all(data)
        self.assertIn("colour.text.nowhere", str(raised.exception))

    def test_given_a_cycle_when_resolved_then_it_fails_and_names_it(self):
        data = copy.deepcopy(MINIMAL)
        data["semantic"]["colour.text.primary"] = {"alias": "colour.text.loop", "use": "x"}
        data["semantic"]["colour.text.loop"] = {"alias": "colour.text.primary", "use": "x"}
        with self.assertRaises(self.gen.TokenError) as raised:
            self.gen.resolve_all(data)
        self.assertIn("cycle", str(raised.exception))

    def test_given_a_component_token_holding_a_value_when_resolved_then_it_fails(self):
        data = copy.deepcopy(MINIMAL)
        data["component"]["row.text"] = "#FFFFFF"
        with self.assertRaises(self.gen.TokenError) as raised:
            self.gen.resolve_all(data)
        self.assertIn("row.text", str(raised.exception))

    def test_given_a_semantic_token_aliasing_a_component_token_when_resolved_then_it_fails(self):
        data = copy.deepcopy(MINIMAL)
        data["semantic"]["colour.text.primary"] = {"alias": "row.text", "use": "x"}
        with self.assertRaises(self.gen.TokenError):
            self.gen.resolve_all(data)


class CheckingContrast(unittest.TestCase):
    def setUp(self):
        self.gen = load_script()

    def test_given_white_on_black_when_measured_then_the_ratio_is_21(self):
        self.assertAlmostEqual(self.gen.contrast_ratio("#FFFFFF", "#000000"), 21.0, places=2)

    def test_given_the_project_token_file_when_checked_then_every_text_pair_reaches_the_minimum(self):
        data = json.loads(TOKENS.read_text(encoding="utf-8"))
        rows, failures = self.gen.check_contrast(data, self.gen.resolve_all(data))
        self.assertEqual(failures, [])
        self.assertTrue(rows)

    def test_given_a_text_too_close_to_its_surface_when_checked_then_the_pair_is_named(self):
        data = copy.deepcopy(MINIMAL)
        data["global"]["colour.grey.100"] = "#111111"
        _, failures = self.gen.check_contrast(data, self.gen.resolve_all(data))
        self.assertEqual(len(failures), 1)
        self.assertIn("colour.text.primary", failures[0])
        self.assertIn("colour.surface.base", failures[0])


class WritingTheHeader(unittest.TestCase):
    def setUp(self):
        self.gen = load_script()

    def test_given_spec_names_when_transliterated_then_they_follow_the_document_rule(self):
        self.assertEqual(self.gen.cpp_name("colour.surface.panel"), "kColourSurfacePanel")
        self.assertEqual(self.gen.cpp_name("size.window.minWidth"), "kSizeWindowMinWidth")
        self.assertEqual(self.gen.cpp_name("size.text.11"), "kSizeText11")

    def test_given_the_tokens_when_the_header_is_written_then_it_holds_typed_constants_and_no_juce(self):
        header = self.gen.render_header(MINIMAL, self.gen.resolve_all(MINIMAL))
        self.assertIn("constexpr std::uint32_t kColourSurfaceBase = 0xFF000000;", header)
        self.assertIn("constexpr float kSpaceEdge = 4.0f;", header)
        self.assertIn("constexpr std::chrono::milliseconds kMotionHover{80};", header)
        self.assertIn('constexpr std::string_view kFontFamilyUi = "Inter";', header)
        self.assertIn("namespace app::tokens", header)
        includes = [line for line in header.splitlines() if line.startswith("#include")]
        self.assertFalse([line for line in includes if "juce" in line.lower()])

    def test_given_the_same_tokens_when_written_twice_then_the_bytes_are_the_same(self):
        resolved = self.gen.resolve_all(MINIMAL)
        self.assertEqual(self.gen.render_header(MINIMAL, resolved), self.gen.render_header(MINIMAL, resolved))


class WritingTheDocumentTables(unittest.TestCase):
    def setUp(self):
        self.gen = load_script()

    def test_given_the_markers_when_the_tables_are_written_then_only_their_content_changes(self):
        sections = self.gen.render_doc_sections(MINIMAL, self.gen.resolve_all(MINIMAL))
        text = self.gen.replace_sections(DOC, sections)
        self.assertTrue(text.startswith("# Doc\n"))
        self.assertIn("`colour.grey.950` | `#000000`", text)
        self.assertIn("`colour.text.primary` | `colour.grey.100` | text", text)
        self.assertNotIn("\nold\n", text)

    def test_given_a_document_without_a_marker_when_the_tables_are_written_then_it_fails_and_names_it(self):
        sections = self.gen.render_doc_sections(MINIMAL, self.gen.resolve_all(MINIMAL))
        with self.assertRaises(self.gen.TokenError) as raised:
            self.gen.replace_sections("# Doc\n", sections)
        self.assertIn("global-colours", str(raised.exception))


LAYOUT = {
    "editor": "sample",
    "tabs": [{"title": "Sample", "groups": [
        {"title": "Playback", "parameters": [
            {"name": "sample start position", "label": "Start", "role": "sample", "control": "slider"},
            {"name": "sample playback mode", "label": "Mode", "role": "sample", "control": "choice"}]},
        {"title": "Pitch", "parameters": [
            {"name": "sample fine tune", "label": "Fine", "role": "general", "control": "knob"}]}]}],
}


class WritingTheEditorLayouts(unittest.TestCase):
    """The editor layouts as C++ tables of the core. [RQ-APP-086, RQ-APP-049, ADR-APP-004 (DEC-APP-024), TASK-APP-035]"""

    def setUp(self):
        self.gen = load_script()

    def test_given_a_layout_when_written_then_its_tabs_groups_and_parameters_are_constexpr_tables_in_order(self):
        header = self.gen.render_layouts({"sample": LAYOUT})
        self.assertIn("namespace app::layouts", header)
        self.assertIn('{"sample start position", "Start", Role::Sample, Control::Slider}', header)
        self.assertIn('{"sample fine tune", "Fine", Role::General, Control::Knob}', header)
        self.assertLess(header.index('"Playback"'), header.index('"Pitch"'))
        self.assertIn('"sample"', header)
        self.assertIn("kEditorLayouts", header)
        includes = [line for line in header.splitlines() if line.startswith("#include")]
        self.assertFalse([line for line in includes if "juce" in line.lower()])

    def test_given_an_unknown_control_or_role_when_written_then_it_fails_and_names_it(self):
        for key, value in (("control", "fader"), ("role", "nowhere")):
            layout = copy.deepcopy(LAYOUT)
            layout["tabs"][0]["groups"][0]["parameters"][0][key] = value
            with self.assertRaises(self.gen.TokenError) as raised:
                self.gen.render_layouts({"sample": layout})
            self.assertIn(value, str(raised.exception))

    def test_given_no_layout_when_written_then_the_table_is_empty_and_still_compiles(self):
        header = self.gen.render_layouts({})
        self.assertIn("kEditorLayouts", header)


class CheckingTheGeneratedFiles(unittest.TestCase):
    def test_given_fresh_files_when_checked_then_it_passes_and_given_an_edit_then_it_fails_naming_the_file(self):
        with tempfile.TemporaryDirectory() as folder:
            folder = pathlib.Path(folder)
            tokens, header, doc = folder / "tokens.json", folder / "Tokens.hpp", folder / "doc.md"
            layouts, layouts_header = folder / "layouts", folder / "Layouts.hpp"
            layouts.mkdir()
            (layouts / "sample.json").write_text(json.dumps(LAYOUT), encoding="utf-8")
            tokens.write_text(json.dumps(MINIMAL), encoding="utf-8")
            doc.write_text(DOC, encoding="utf-8")
            paths = ["--tokens", str(tokens), "--header", str(header), "--doc", str(doc), "--layouts", str(layouts),
                     "--layouts-header", str(layouts_header)]
            self.assertEqual(run_script(*paths).returncode, 0)
            self.assertEqual(run_script("--check", *paths).returncode, 0)
            header.write_text(header.read_text(encoding="utf-8") + "// edited\n", encoding="utf-8")
            result = run_script("--check", *paths)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Tokens.hpp", result.stdout + result.stderr)
            self.assertEqual(run_script(*paths).returncode, 0)
            layouts_header.write_text(layouts_header.read_text(encoding="utf-8") + "// edited\n", encoding="utf-8")
            result = run_script("--check", *paths)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Layouts.hpp", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
