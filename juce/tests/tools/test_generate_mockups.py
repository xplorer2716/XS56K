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
