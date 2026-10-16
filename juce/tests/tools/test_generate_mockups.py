"""Tests of juce/tools/generate_mockups.py, run by ctest when Python 3 is available.

Each test reads as a Gherkin scenario. [TASK-APP-008, RQ-APP-074, RQ-APP-076, RQ-APP-077, RQ-APP-078, ADR-APP-003 (DEC-APP-017 to DEC-APP-021)]
"""
import copy
import importlib.util
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "juce" / "tools" / "generate_mockups.py"
TOKENS = ROOT / "juce" / "app" / "design" / "tokens.json"
FAKE_FONTS = {400: b"regular-font-bytes", 500: b"medium-font-bytes"}

LAYOUT = {
    "editor": "program",
    "example": True,
    "tabs": [{"title": "Filter", "groups": [{"title": "FILTER", "parameters": [
        {"name": "filter cutoff", "label": "CUTOFF", "role": "filter", "control": "knob"},
        {"name": "filter type", "label": "TYPE", "role": "filter", "control": "choice"}]}]}],
}

SCREEN = {
    "name": "main-window",
    "size": [1280, 800],
    "leftWidth": 300,
    "browserShare": 0.55,
    "menu": ["Sampler", "MIDI", "Help"],
    "content": {
        "browser": {"path": "HD1 / DRUMS", "rows": [{"kind": "disk", "name": "HD1", "depth": 0},
                                                     {"kind": "program", "name": "PAD.AKP", "depth": 1, "detail": "4 KB", "selected": True}]},
        "memory": {"free": "2.0 MB free of 8.0 MB", "kinds": [{"title": "PROGRAMS", "items": [{"name": "PAD", "current": True}]}]},
        "editor": {"item": "PAD", "kind": "PROGRAM", "keygroup": "KEYGROUP 1 OF 2", "layout": "program", "tab": "Filter",
                   "values": {"filter cutoff": {"text": "70", "fraction": 0.7}, "filter type": {"text": "2-POLE LP"}}},
    },
    "states": {
        "connected": {"connection": {"state": "connected", "text": "Connected: S5000, DeviceID 0"}},
        "disconnected": {"connection": {"state": "disconnected", "text": "Disconnected"}, "browser": None, "memory": None, "editor": None},
    },
}


SHAPES = {"filter types": {"name": "filter types", "shapes": {"2-POLE LP": [[0, 0.7], [0.6, 0.7], [0.92, 0], [1, 0]]}}}


def load_script():
    spec = importlib.util.spec_from_file_location("generate_mockups", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def run_script(*arguments):
    env = dict(os.environ, PYTHONUTF8="1")
    return subprocess.run([sys.executable, str(SCRIPT), *arguments], capture_output=True, text=True, env=env)


def tokens():
    return json.loads(TOKENS.read_text(encoding="utf-8"))


class RenderingAScreen(unittest.TestCase):
    def setUp(self):
        self.gen = load_script()

    def render(self, screen=SCREEN, state="connected", data=None, layout=LAYOUT):
        return self.gen.render_state(screen, state, data or tokens(), {"program": layout}, FAKE_FONTS)

    def test_given_a_screen_when_rendered_then_it_is_an_svg_of_the_screen_size(self):
        svg = self.render()
        self.assertTrue(svg.startswith("<svg"))
        self.assertIn('width="1280"', svg)
        self.assertIn('height="800"', svg)

    def test_given_a_screen_when_rendered_then_every_colour_is_a_value_of_the_token_file(self):
        data = tokens()
        allowed = {value.upper() for value in data["global"].values() if isinstance(value, str) and value.startswith("#")}
        used = {colour.upper() for colour in re.findall(r"#[0-9A-Fa-f]{6}\b", self.render())}
        self.assertTrue(used)
        self.assertLessEqual(used, allowed)

    def test_given_a_token_value_changed_when_rendered_then_the_new_value_is_drawn(self):
        data = tokens()
        data["global"]["colour.grey.950"] = "#010203"
        self.assertIn("#010203", self.render(data=data))

    def test_given_a_parameter_of_an_unknown_role_when_rendered_then_it_fails_and_names_the_token(self):
        layout = copy.deepcopy(LAYOUT)
        layout["tabs"][0]["groups"][0]["parameters"][0]["role"] = "nowhere"
        with self.assertRaises(self.gen.MockupError) as raised:
            self.render(layout=layout)
        self.assertIn("colour.role.nowhere", str(raised.exception))

    def test_given_a_screen_when_rendered_then_the_fonts_are_embedded_and_nothing_is_fetched(self):
        svg = self.render()
        self.assertIn("@font-face", svg)
        self.assertIn("data:font/woff2;base64,", svg)
        self.assertEqual(re.findall(r'(?:href|src)="https?:', svg), [])

    def test_given_a_screen_when_rendered_then_its_caption_says_the_values_are_examples(self):
        svg = self.render()
        self.assertIn("example values", svg)
        self.assertIn("main-window", svg)

    def test_given_an_example_layout_when_rendered_then_the_caption_says_the_grouping_is_not_decided(self):
        self.assertIn("example grouping", self.render())

    def test_given_the_same_screen_when_rendered_twice_then_the_bytes_are_the_same(self):
        self.assertEqual(self.render(), self.render())

    def test_given_the_disconnected_state_when_rendered_then_it_says_so_and_shows_no_parameter(self):
        svg = self.render(state="disconnected")
        self.assertIn("Disconnected", svg)
        self.assertNotIn("CUTOFF", svg)

    def test_given_a_dialog_when_rendered_then_it_names_its_object_and_buttons(self):
        screen = copy.deepcopy(SCREEN)
        screen["states"]["confirm"] = {"connection": {"state": "connected", "text": "Connected"},
                                       "dialog": {"title": "Delete the program PAD?", "text": ["PAD is lost if it was not saved."],
                                                  "buttons": [{"text": "Cancel"}, {"text": "Delete", "destructive": True}]}}
        svg = self.render(screen=screen, state="confirm")
        self.assertIn("Delete the program PAD?", svg)
        self.assertIn(">Delete<", svg)

    def test_given_value_box_parameters_when_rendered_then_no_knob_is_drawn_and_their_values_are(self):
        # A position in sample points is typed, not turned: a value box alone. [RQ-APP-086, ADR-APP-004 (DEC-APP-027), TASK-APP-040]
        layout = copy.deepcopy(LAYOUT)
        layout["tabs"][0]["groups"][0]["parameters"] = [
            {"name": "sample start position", "label": "START", "role": "sample", "control": "valuebox"}]
        screen = copy.deepcopy(SCREEN)
        screen["content"]["editor"]["values"] = {"sample start position": {"text": "1024"}}
        svg = self.render(screen=screen, layout=layout)
        self.assertEqual(svg.count("<path"), 0)
        self.assertIn(">1024<", svg)
        self.assertIn(">START<", svg)

    def test_given_a_slider_parameter_when_rendered_then_a_track_its_thumb_and_its_value_box_are_drawn_and_no_knob(self):
        # A position a musician drags, and can type: a slider with its value box. [RQ-APP-086, ADR-APP-004 (DEC-APP-027), TASK-APP-040]
        data = tokens()
        layout = copy.deepcopy(LAYOUT)
        layout["tabs"][0]["groups"][0]["parameters"] = [
            {"name": "sample start position", "label": "START", "role": "sample", "control": "slider"}]
        screen = copy.deepcopy(SCREEN)
        screen["content"]["editor"]["values"] = {"sample start position": {"text": "1024", "fraction": 0.25}}
        svg = self.render(screen=screen, layout=layout, data=data)
        self.assertEqual(svg.count("<path"), 0)
        self.assertIn(">1024<", svg)
        self.assertIn(">START<", svg)
        thumb = self.gen.Theme(data)("slider.thumb")
        self.assertIn(f'fill="{thumb}"', svg)

    def test_given_an_envelope_group_when_rendered_then_its_shape_is_drawn_from_the_values_with_its_scope_and_breakpoints(self):
        # Attack 10, decay 50, sustain 80, release 30: a rise, a fall to 80 %, a plateau, a fall to 0. [RQ-APP-095, DEC-APP-031, TASK-APP-044]
        data = tokens()
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
        screen = copy.deepcopy(SCREEN)
        screen["content"]["editor"]["tab"] = "Filter"
        screen["content"]["editor"]["values"] = {
            "amplitude envelope attack": {"text": "10", "fraction": 0.1}, "amplitude envelope decay": {"text": "50", "fraction": 0.5},
            "amplitude envelope sustain": {"text": "80", "fraction": 0.8}, "amplitude envelope release": {"text": "30", "fraction": 0.3}}
        svg = self.render(screen=screen, layout=layout, data=data)
        theme = self.gen.Theme(data)
        lines = re.findall(r'<polyline points="([^"]+)"[^>]*stroke="([^"]+)"', svg)
        self.assertEqual(len(lines), 1)
        points, stroke = lines[0]
        self.assertEqual(stroke, theme("colour.role.modulation"))
        xy = [tuple(float(v) for v in pair.split(",")) for pair in points.split()]
        self.assertEqual(len(xy), 5)  # the start, the attack's peak, the decay's end, the plateau's end, the release's end
        (x0, y0), (x1, y1), (x2, y2), (x3, y3), (x4, y4) = xy
        self.assertLess(y1, y2)                         # the peak is above the sustain
        self.assertAlmostEqual(y2, y3)                  # the plateau
        self.assertAlmostEqual(y0, y4)                  # back to 0
        self.assertAlmostEqual((x2 - x1) / (x1 - x0), 5.0, places=3)  # the decay is five times the attack
        self.assertIn(">KEYGROUP<", svg)
        handles = re.findall(r'<circle [^>]*fill="' + re.escape(theme("envelope.handle")) + '"', svg)
        self.assertEqual(len(handles), 3)  # a breakpoint per stage that has a time

    def render_type(self, text, shapes=None, shape_set="filter types", data=None):
        layout = copy.deepcopy(LAYOUT)
        layout["tabs"][0]["groups"][0]["parameters"][1]["shapes"] = shape_set
        screen = copy.deepcopy(SCREEN)
        screen["content"]["editor"]["values"]["filter type"] = {"text": text}
        shapes = SHAPES if shapes is None else shapes
        return self.gen.render_state(screen, "connected", data or tokens(), {"program": layout}, FAKE_FONTS, shapes)

    def test_given_a_choice_with_shapes_when_rendered_then_the_shape_of_its_value_is_drawn_in_its_role_colour_above_the_choice(self):
        # 2-POLE LP: from the pass level, flat, then down to 0 at the right. [RQ-APP-096, DEC-APP-032, TASK-APP-051]
        data = tokens()
        svg = self.render_type("2-POLE LP", data=data)
        theme = self.gen.Theme(data)
        lines = re.findall(r'<polyline points="([^"]+)"[^>]*stroke="([^"]+)"', svg)
        self.assertEqual(len(lines), 1)
        points, stroke = lines[0]
        self.assertEqual(stroke, theme("colour.role.filter"))
        xy = [tuple(float(v) for v in pair.split(",")) for pair in points.split()]
        self.assertEqual(len(xy), 4)
        (x0, y0), (x1, y1), (x2, y2), (x3, y3) = xy
        self.assertAlmostEqual(y0, y1)   # flat over the pass band
        self.assertGreater(y2, y1)       # then down (y grows downwards)
        self.assertAlmostEqual(y2, y3)   # to 0
        self.assertLess(x0, x3)
        choice_y = float(re.search(r'<text x="[\d.]+" y="([\d.]+)"[^>]*>2-POLE LP<', svg).group(1))
        self.assertLess(max(y0, y3), choice_y)
        self.assertIn(">TYPE<", svg)

    def test_given_a_value_with_no_shape_when_rendered_then_only_its_name_is_shown(self):
        svg = self.render_type("VOWELISER")
        self.assertEqual(svg.count("<polyline"), 0)
        self.assertIn(">VOWELISER<", svg)

    def test_given_a_choice_naming_an_unknown_shape_set_when_rendered_then_it_fails_and_names_it(self):
        with self.assertRaises(self.gen.MockupError) as raised:
            self.render_type("2-POLE LP", shape_set="vowels")
        self.assertIn("vowels", str(raised.exception))

    def test_given_a_group_that_starts_a_row_when_rendered_then_it_is_drawn_under_the_first_at_the_left(self):
        # Two narrow groups would share a row; the second starts its own. [RQ-APP-094, DEC-APP-031, TASK-APP-044]
        layout = copy.deepcopy(LAYOUT)
        second = {"title": "SECOND", "row": True, "parameters": [{"name": "filter resonance", "label": "RES", "role": "filter", "control": "knob"}]}
        layout["tabs"][0]["groups"].append(second)
        svg = self.render(layout=layout)
        first_x, first_y = [float(v) for v in re.search(r'<text x="([\d.]+)" y="([\d.]+)"[^>]*>FILTER<', svg).groups()]
        second_x, second_y = [float(v) for v in re.search(r'<text x="([\d.]+)" y="([\d.]+)"[^>]*>SECOND<', svg).groups()]
        self.assertAlmostEqual(first_x, second_x)
        self.assertGreater(second_y, first_y)

    def test_given_an_editor_with_no_keygroup_and_an_information_line_when_rendered_then_the_line_is_shown(self):
        # The sample editor's header: its information instead of a keygroup selector. [RQ-APP-087, TASK-APP-040]
        screen = copy.deepcopy(SCREEN)
        del screen["content"]["editor"]["keygroup"]
        screen["content"]["editor"]["info"] = "RAM, mono, 22050 points, 44100 Hz"
        svg = self.render(screen=screen)
        self.assertIn("RAM, mono, 22050 points, 44100 Hz", svg)
        self.assertNotIn("KEYGROUP", svg)

    def test_given_text_with_markup_characters_when_rendered_then_it_is_escaped(self):
        screen = copy.deepcopy(SCREEN)
        screen["content"]["editor"]["item"] = "A&B <1>"
        self.assertIn("A&amp;B &lt;1&gt;", self.render(screen=screen))


class WritingAndCheckingTheMockups(unittest.TestCase):
    def setUp(self):
        self.gen = load_script()

    def test_given_a_screen_with_two_states_when_generated_then_one_file_per_state(self):
        outputs = self.gen.build_outputs([SCREEN], {"program": LAYOUT}, tokens(), FAKE_FONTS)
        self.assertEqual(sorted(outputs), ["main-window-connected.svg", "main-window-disconnected.svg"])

    def test_given_written_mockups_when_checked_then_it_passes_and_given_an_edit_then_it_fails_naming_the_file(self):
        with tempfile.TemporaryDirectory() as folder:
            folder = pathlib.Path(folder)
            screens, layouts, fonts, out = folder / "screens", folder / "layouts", folder / "fonts", folder / "out"
            for path in (screens, layouts, fonts):
                path.mkdir()
            (screens / "main-window.json").write_text(json.dumps(SCREEN), encoding="utf-8")
            (layouts / "program.json").write_text(json.dumps(LAYOUT), encoding="utf-8")
            (fonts / "Inter-Regular.woff2").write_bytes(FAKE_FONTS[400])
            (fonts / "Inter-Medium.woff2").write_bytes(FAKE_FONTS[500])
            paths = ["--screens", str(screens), "--layouts", str(layouts), "--fonts", str(fonts), "--out", str(out), "--tokens", str(TOKENS)]
            self.assertEqual(run_script(*paths).returncode, 0)
            self.assertEqual(run_script("--check", *paths).returncode, 0)
            edited = out / "main-window-connected.svg"
            edited.write_text(edited.read_text(encoding="utf-8") + "<!-- edited -->", encoding="utf-8")
            result = run_script("--check", *paths)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("main-window-connected.svg", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
