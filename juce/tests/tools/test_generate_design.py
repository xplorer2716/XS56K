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
    """The editor layouts as C++ tables of the core. [RQ-APP-086, RQ-APP-049, RQ-APP-092, ADR-APP-004 (DEC-APP-024, DEC-APP-029), TASK-APP-035, TASK-APP-042]"""

    def setUp(self):
        self.gen = load_script()

    def test_given_a_layout_when_written_then_its_tabs_groups_and_parameters_are_constexpr_tables_in_order(self):
        header = self.gen.render_layouts({"sample": LAYOUT})
        self.assertIn("namespace app::layouts", header)
        self.assertIn('{"sample start position", "Start", Role::Sample, Control::Slider, {}, 0}', header)
        self.assertIn('{"sample fine tune", "Fine", Role::General, Control::Knob, {}, 0}', header)
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

    def test_given_an_order_when_written_then_it_is_a_chain_of_names_and_an_order_naming_an_absent_parameter_fails(self):
        layout = copy.deepcopy(LAYOUT)
        layout["orders"] = [["sample start position", "sample fine tune"]]
        header = self.gen.render_layouts({"sample": layout})
        chain = header[header.index("kSampleOrder0{{"):]
        self.assertLess(chain.index('"sample start position"'), chain.index('"sample fine tune"'))
        self.assertIn("kSampleOrders", header)
        layout["orders"] = [["sample start position", "sample loop end"]]
        with self.assertRaises(self.gen.TokenError) as raised:
            self.gen.render_layouts({"sample": layout})
        self.assertIn("sample loop end", str(raised.exception))

    def test_given_an_envelope_group_when_written_then_its_scope_and_stages_are_generated_in_order(self):
        # An envelope is layout data: its stages, a time and an end level each, and the sustain mark. [RQ-APP-095, DEC-APP-031, TASK-APP-044]
        layout = copy.deepcopy(LAYOUT)
        layout["tabs"][0]["groups"] = [{"title": "Amp envelope", "scope": "keygroup", "envelope": [
            {"time": "amplitude envelope attack", "level": 100},
            {"time": "amplitude envelope decay", "level": "amplitude envelope sustain"},
            {"sustain": True},
            {"time": "amplitude envelope release", "level": 0}], "parameters": [
            {"name": "amplitude envelope attack", "label": "Attack", "role": "modulation", "control": "knob"},
            {"name": "amplitude envelope decay", "label": "Decay", "role": "modulation", "control": "knob"},
            {"name": "amplitude envelope sustain", "label": "Sustain", "role": "modulation", "control": "knob"},
            {"name": "amplitude envelope release", "label": "Release", "role": "modulation", "control": "knob"}]}]
        header = self.gen.render_layouts({"program": layout})
        self.assertIn("Scope::Keygroup", header)
        self.assertIn('{"amplitude envelope attack", "", 100, false}', header)
        self.assertIn('{"amplitude envelope decay", "amplitude envelope sustain", 0, false}', header)
        self.assertIn('{"", "", 0, true}', header)
        self.assertLess(header.index('"amplitude envelope attack", ""'), header.index('"amplitude envelope release", ""'))

    def test_given_an_envelope_naming_a_parameter_of_no_group_or_an_unknown_scope_when_written_then_it_fails_and_names_it(self):
        layout = copy.deepcopy(LAYOUT)
        layout["tabs"][0]["groups"] = [{"title": "Amp envelope", "scope": "keygroup", "envelope": [
            {"time": "amplitude envelope attack", "level": 100},
            {"time": "amplitude envelope decay", "level": "amplitude envelope sustain"},
            {"sustain": True},
            {"time": "amplitude envelope release", "level": 0}], "parameters": [
            {"name": "amplitude envelope attack", "label": "Attack", "role": "modulation", "control": "knob"},
            {"name": "amplitude envelope decay", "label": "Decay", "role": "modulation", "control": "knob"},
            {"name": "amplitude envelope sustain", "label": "Sustain", "role": "modulation", "control": "knob"},
            {"name": "amplitude envelope release", "label": "Release", "role": "modulation", "control": "knob"}]}]
        layout["tabs"][0]["groups"][0]["envelope"][0]["time"] = "filter envelope attack"
        with self.assertRaises(self.gen.TokenError) as raised:
            self.gen.render_layouts({"program": layout})
        self.assertIn("filter envelope attack", str(raised.exception))
        layout = copy.deepcopy(LAYOUT)
        layout["tabs"][0]["groups"][0]["scope"] = "multi"
        with self.assertRaises(self.gen.TokenError) as raised:
            self.gen.render_layouts({"sample": layout})
        self.assertIn("multi", str(raised.exception))

    def test_given_a_group_that_starts_a_row_when_written_then_it_says_so_and_the_others_do_not(self):
        # The Envelopes tab: Amp, Filter and Aux a row each. [RQ-APP-094, DEC-APP-031, TASK-APP-044]
        layout = copy.deepcopy(LAYOUT)
        layout["tabs"][0]["groups"][1]["row"] = True
        header = self.gen.render_layouts({"sample": layout})
        self.assertIn('LayoutGroup{"Pitch", kSampleTab0Group1, Scope::None, {}, true, 0}', header)
        self.assertIn('LayoutGroup{"Playback", kSampleTab0Group0, Scope::None, {}, false, 0}', header)

    def test_given_no_layout_when_written_then_the_table_is_empty_and_still_compiles(self):
        header = self.gen.render_layouts({})
        self.assertIn("kEditorLayouts", header)


SHAPES = {"filter types": {"name": "filter types", "shapes": {
    "2-POLE LP": [[0, 0.7], [0.6, 0.7], [0.92, 0], [1, 0]],
    "NOTCH 1": [[0, 0.45], [0.48, 0], [1, 0.45]]}}}


class WritingTheChoiceShapes(unittest.TestCase):
    """A choice may show the shape of its value: the shape sets as C++ tables, the choice pointing to its set.
    [RQ-APP-096, ADR-APP-004 (DEC-APP-032), TASK-APP-051]"""

    def setUp(self):
        self.gen = load_script()

    @staticmethod
    def layout(shapes="filter types", parameter=1):
        layout = copy.deepcopy(LAYOUT)
        layout["tabs"][0]["groups"][0]["parameters"][parameter]["shapes"] = shapes
        return layout

    def test_given_a_choice_naming_a_shape_set_when_written_then_each_label_has_its_polyline_in_order_and_the_choice_points_to_them(self):
        header = self.gen.render_layouts({"sample": self.layout()}, SHAPES)
        self.assertIn('{"sample playback mode", "Mode", Role::Sample, Control::Choice, kFilterTypesShapes, 0}', header)
        self.assertIn('{"sample fine tune", "Fine", Role::General, Control::Knob, {}, 0}', header)
        self.assertIn("{0.0f, 0.7f}, {0.6f, 0.7f}, {0.92f, 0.0f}, {1.0f, 0.0f}", header)
        self.assertIn('ChoiceShape{"2-POLE LP", kFilterTypesShape0}', header)
        self.assertLess(header.index('"2-POLE LP"'), header.index('"NOTCH 1"'))
        self.assertLess(header.index("kFilterTypesShapes{{"), header.index("kSampleTab0Group0{{"))

    def test_given_a_choice_naming_an_unknown_shape_set_or_shapes_on_a_knob_when_written_then_it_fails_and_names_it(self):
        unknown = {"sample": self.layout("vowels")}
        with self.assertRaises(self.gen.TokenError) as raised:
            self.gen.render_layouts(unknown, SHAPES)
        self.assertIn("vowels", str(raised.exception))
        layout = copy.deepcopy(LAYOUT)
        layout["tabs"][0]["groups"][1]["parameters"][0]["shapes"] = "filter types"
        with self.assertRaises(self.gen.TokenError) as raised:
            self.gen.render_layouts({"sample": layout}, SHAPES)
        self.assertIn("sample fine tune", str(raised.exception))

    def test_given_a_polyline_going_back_leaving_the_square_or_of_one_point_when_written_then_it_fails_and_names_its_label(self):
        for points in ([[0.5, 0.2], [0.4, 0.3]], [[0, 0], [1, 1.2]], [[0, 0]]):
            shapes = copy.deepcopy(SHAPES)
            shapes["filter types"]["shapes"]["NOTCH 1"] = points
            layouts = {"sample": self.layout()}
            with self.assertRaises(self.gen.TokenError) as raised:
                self.gen.render_layouts(layouts, shapes)
            self.assertIn("NOTCH 1", str(raised.exception))

    def test_given_a_folder_of_shape_sets_when_read_then_they_are_keyed_by_their_name(self):
        with tempfile.TemporaryDirectory() as folder:
            (pathlib.Path(folder) / "filter-types.json").write_text(json.dumps(SHAPES["filter types"]), encoding="utf-8")
            self.assertEqual(self.gen.read_shapes(folder), SHAPES)
        self.assertEqual(self.gen.read_shapes(pathlib.Path(folder) / "none"), {})


ZONE_GRID = {"title": "Zones", "scope": "zone", "zones": 4, "parameters": [
    {"name": "zone sample", "label": "Sample", "role": "sample", "control": "sample"},
    {"name": "zone level", "label": "Level", "role": "amp", "control": "valuebox"},
    {"name": "zone playback", "label": "Playback", "role": "sample", "control": "choice"}]}


class WritingTheZoneGrid(unittest.TestCase):
    """The Keygroup tab's four zones: a group drawn as a grid, a row per parameter and a column per zone; the zone's sample chosen among
    those in memory. [RQ-APP-057, RQ-APP-060, ADR-APP-004 (DEC-APP-033), TASK-APP-046]"""

    def setUp(self):
        self.gen = load_script()

    @staticmethod
    def layout(group):
        layout = copy.deepcopy(LAYOUT)
        layout["tabs"][0]["groups"] = [copy.deepcopy(group)]
        return layout

    def test_given_a_zone_group_of_four_columns_when_written_then_it_says_its_zones_and_the_sample_control(self):
        header = self.gen.render_layouts({"program": self.layout(ZONE_GRID)})
        self.assertIn('{"zone sample", "Sample", Role::Sample, Control::Sample, {}, 0}', header)
        self.assertIn('LayoutGroup{"Zones", kProgramTab0Group0, Scope::Zone, {}, false, 4}', header)

    def test_given_a_group_that_is_no_grid_when_written_then_it_has_no_zone_columns(self):
        header = self.gen.render_layouts({"sample": LAYOUT})
        self.assertIn('LayoutGroup{"Pitch", kSampleTab0Group1, Scope::None, {}, false, 0}', header)

    def test_given_columns_outside_a_zone_scope_a_knob_in_a_grid_or_a_sample_outside_one_when_written_then_it_fails_and_names_it(self):
        keygroup = copy.deepcopy(ZONE_GRID)
        keygroup["scope"] = "keygroup"
        knob = copy.deepcopy(ZONE_GRID)
        knob["parameters"][1]["control"] = "knob"
        loose = {"title": "Loose", "scope": "zone", "parameters": [copy.deepcopy(ZONE_GRID["parameters"][0])]}
        five = copy.deepcopy(ZONE_GRID)
        five["zones"] = 5
        for group, named in ((keygroup, "Zones"), (knob, "zone level"), (loose, "zone sample"), (five, "Zones")):
            layouts = {"program": self.layout(group)}
            with self.assertRaises(self.gen.TokenError) as raised:
                self.gen.render_layouts(layouts)
            self.assertIn(named, str(raised.exception))


class WritingAWiderControl(unittest.TestCase):
    """A choice whose labels are long takes the cells the layout gives it. [RQ-APP-057, ADR-APP-004 (DEC-APP-027), TASK-APP-048]"""

    def setUp(self):
        self.gen = load_script()

    def test_given_a_choice_of_three_cells_when_written_then_it_carries_them(self):
        layout = copy.deepcopy(LAYOUT)
        layout["tabs"][0]["groups"][0]["parameters"][1]["cells"] = 3
        header = self.gen.render_layouts({"sample": layout})
        self.assertIn('{"sample playback mode", "Mode", Role::Sample, Control::Choice, {}, 3}', header)

    def test_given_cells_on_a_knob_or_out_of_one_to_three_when_written_then_it_fails_and_names_it(self):
        for group, parameter, cells in ((1, 0, 2), (0, 1, 4), (0, 1, 0)):
            layout = copy.deepcopy(LAYOUT)
            named = layout["tabs"][0]["groups"][group]["parameters"][parameter]
            named["cells"] = cells
            layouts = {"sample": layout}
            with self.assertRaises(self.gen.TokenError) as raised:
                self.gen.render_layouts(layouts)
            self.assertIn(named["name"], str(raised.exception))


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
