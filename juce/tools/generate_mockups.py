#!/usr/bin/env python3
"""Draws static SVG mockups of the editor application from the design system's tokens, the editor layouts and the screen descriptions.

Reads `juce/app/design/tokens.json` (through generate_design.py's resolver), the editor layouts (`juce/app/design/layouts/*.json`, and the
example layouts of `juce/app/design/examples/*.json`, which say they are examples), the screens (`juce/app/design/screens/*.json`) and the
font files (`juce/app/resources/fonts/Inter-{Regular,Medium}.woff2`), and writes one self-contained SVG per screen and state into
`documents/mockups/<screen>-<state>.svg`. Every colour, size, spacing, radius, stroke and font comes from the tokens; the screens give content
and example values only. `--check` writes nothing and fails, naming the files, when a committed mockup is behind its sources. Python 3
standard library only; never run during the build.

Usage: python3 juce/tools/generate_mockups.py [--check] [--screens DIR] [--layouts DIR ...] [--fonts DIR] [--out DIR] [--tokens PATH]

[TASK-APP-008, RQ-APP-074, RQ-APP-075, RQ-APP-076, RQ-APP-077, ADR-APP-003 (DEC-APP-017 to DEC-APP-021)]
"""
import argparse
import base64
import importlib.util
import json
import math
import pathlib
import sys
from xml.sax.saxutils import escape

ROOT = pathlib.Path(__file__).resolve().parents[2]
DESIGN = ROOT / "juce" / "app" / "design"
DEFAULT_TOKENS = DESIGN / "tokens.json"
DEFAULT_SCREENS = DESIGN / "screens"
DEFAULT_LAYOUTS = [DESIGN / "layouts", DESIGN / "examples"]
DEFAULT_FONTS = ROOT / "juce" / "app" / "resources" / "fonts"
DEFAULT_OUT = ROOT / "documents" / "mockups"
FONT_FILES = {400: "Inter-Regular.woff2", 500: "Inter-Medium.woff2"}
FONT_STACK = "Inter, sans-serif"
WEIGHT_REGULAR = 400
WEIGHT_MEDIUM = 500

# Drawing geometry that is not a visual value of the design system but a property of the shapes themselves.
KNOB_START_DEGREES = 135.0      # the knob's arc starts at the bottom left...
KNOB_SWEEP_DEGREES = 270.0      # ...and turns clockwise through the top to the bottom right
KNOB_POINTER_LENGTH = 0.55      # the pointer's length, as a part of the arc's radius
CHOICE_CELLS = 2                # a choice selector is as wide as two knob cells
SLIDER_CELLS = 3                # a slider (a position in sample points) is as wide as three knob cells
DIALOG_WIDTH_IN_KNOBS = 12      # a dialog is twelve knob diameters wide
BUTTON_WIDTH_IN_KNOBS = 3       # a dialog button is three knob diameters wide
KEYGROUP_SELECTOR_IN_KNOBS = 4  # the keygroup selector is four knob diameters wide
ELLIPSIS = "…"


class MockupError(Exception):
    """A screen, a layout or a token that cannot be drawn: the message names it."""


def load_design_module():
    path = pathlib.Path(__file__).resolve().parent / "generate_design.py"
    spec = importlib.util.spec_from_file_location("generate_design", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


DESIGN_MODULE = load_design_module()


def fmt(number):
    """A number as SVG writes it: at most two decimals, no trailing zeros, so that the output is the same bytes every time."""
    text = f"{number:.2f}".rstrip("0").rstrip(".")
    return "0" if text == "-0" else text


class Theme:
    """The resolved tokens, read by name; an unknown name is a MockupError that names it."""

    def __init__(self, data):
        try:
            self.values = DESIGN_MODULE.resolve_all(data)
        except DESIGN_MODULE.TokenError as error:
            raise MockupError(str(error)) from error

    def __call__(self, name):
        if name not in self.values:
            raise MockupError(f"the token {name} is not in the token file")
        return self.values[name]


class Canvas:
    """SVG elements in drawing order."""

    def __init__(self, theme):
        self.t = theme
        self.items = []

    def rect(self, x, y, w, h, fill, radius=0.0, stroke=None, stroke_width=None, opacity=None):
        attributes = f'x="{fmt(x)}" y="{fmt(y)}" width="{fmt(w)}" height="{fmt(h)}" fill="{fill}"'
        if radius:
            attributes += f' rx="{fmt(radius)}"'
        if stroke:
            attributes += f' stroke="{stroke}" stroke-width="{fmt(stroke_width)}"'
        if opacity is not None:
            attributes += f' fill-opacity="{fmt(opacity)}"'
        self.items.append(f"<rect {attributes}/>")

    def circle(self, cx, cy, r, fill):
        self.items.append(f'<circle cx="{fmt(cx)}" cy="{fmt(cy)}" r="{fmt(r)}" fill="{fill}"/>')

    def line(self, x1, y1, x2, y2, stroke, width):
        self.items.append(f'<line x1="{fmt(x1)}" y1="{fmt(y1)}" x2="{fmt(x2)}" y2="{fmt(y2)}" stroke="{stroke}" '
                          f'stroke-width="{fmt(width)}" stroke-linecap="round"/>')

    def arc(self, cx, cy, r, start, sweep, stroke, width):
        if sweep <= 0:
            return
        end = start + sweep
        x1, y1 = cx + r * math.cos(math.radians(start)), cy + r * math.sin(math.radians(start))
        x2, y2 = cx + r * math.cos(math.radians(end)), cy + r * math.sin(math.radians(end))
        large = 1 if sweep > 180 else 0
        self.items.append(f'<path d="M {fmt(x1)} {fmt(y1)} A {fmt(r)} {fmt(r)} 0 {large} 1 {fmt(x2)} {fmt(y2)}" fill="none" '
                          f'stroke="{stroke}" stroke-width="{fmt(width)}" stroke-linecap="round"/>')

    def text(self, x, y, content, size, fill, weight=WEIGHT_REGULAR, anchor="start", tracking=None, upper=False):
        shown = content.upper() if upper else content
        attributes = f'x="{fmt(x)}" y="{fmt(y)}" font-size="{fmt(size)}" font-weight="{weight}" fill="{fill}"'
        if anchor != "start":
            attributes += f' text-anchor="{anchor}"'
        if tracking:
            attributes += f' letter-spacing="{fmt(tracking)}"'
        self.items.append(f"<text {attributes}>{escape(shown)}</text>")


def role_colour(t, role):
    return t(f"colour.role.{role}")


def draw_label(c, x, y, content, colour=None, anchor="start"):
    t = c.t
    c.text(x, y, content, t("font.label.size"), colour or t("colour.text.secondary"), WEIGHT_MEDIUM, anchor, t("font.label.tracking"), True)


def draw_panel(c, x, y, w, h):
    c.rect(x, y, w, h, c.t("colour.surface.panel"), c.t("radius.panel"))


def draw_not_connected(c, x, y, w, h, message):
    t = c.t
    c.text(x + w / 2, y + h / 2, message, t("font.value.size"), t("colour.text.secondary"), anchor="middle")


def draw_row(c, x, y, w, row):
    t = c.t
    height = t("row.height")
    inside = t("space.inside")
    if row.get("selected"):
        c.rect(x, y, w, height, t("row.surface.hover"))
        c.rect(x, y, t("stroke.arc"), height, t("row.bar.selected"))
    indent = x + inside + row.get("depth", 0) * t("size.space.4")
    icon = t("size.icon.small")
    icon_y = y + (height - icon) / 2
    kind = row.get("kind", "file")
    if kind in ("disk", "folder"):
        c.rect(indent, icon_y, icon, icon, "none", t("radius.control"), t("colour.text.secondary"), t("stroke.hairline"))
        if kind == "folder":
            c.line(indent, icon_y + icon / 3, indent + icon, icon_y + icon / 3, t("colour.text.secondary"), t("stroke.hairline"))
    else:
        c.rect(indent + icon / 4, icon_y, icon / 2, icon, "none", t("radius.control"), t("colour.text.secondary"), t("stroke.hairline"))
    baseline = y + height / 2 + t("font.value.size") / 3
    c.text(indent + icon + t("size.space.2"), baseline, row["name"], t("font.value.size"), t("colour.text.primary"))
    if row.get("detail"):
        c.text(x + w - inside, baseline, row["detail"], t("font.caption.size"), t("colour.text.secondary"), anchor="end")
    if row.get("current"):
        c.circle(x + w - inside - t("size.space.1"), y + height / 2, t("radius.control"), t("row.dot.current"))


def draw_browser(c, x, y, w, h, browser):
    t = c.t
    inside = t("space.inside")
    draw_panel(c, x, y, w, h)
    draw_label(c, x + inside, y + inside + t("font.label.size"), "Disks")
    if browser is None:
        draw_not_connected(c, x, y, w, h, "Not connected")
        return
    path_y = y + inside + t("font.label.size") + t("size.space.2") + t("font.caption.size")
    c.text(x + inside, path_y, browser.get("path", ""), t("font.caption.size"), t("colour.text.secondary"))
    row_y = path_y + t("size.space.2")
    for row in browser.get("rows", []):
        if row_y + t("row.height") > y + h - inside:
            break
        draw_row(c, x, row_y, w, row)
        row_y += t("row.height")


def draw_memory(c, x, y, w, h, memory):
    t = c.t
    inside = t("space.inside")
    draw_panel(c, x, y, w, h)
    draw_label(c, x + inside, y + inside + t("font.label.size"), "Memory")
    if memory is None:
        draw_not_connected(c, x, y, w, h, "Not connected")
        return
    c.text(x + w - inside, y + inside + t("font.label.size"), memory.get("free", ""), t("font.caption.size"), t("colour.text.secondary"),
           anchor="end")
    row_y = y + inside + t("font.label.size") + t("size.space.2")
    for kind in memory.get("kinds", []):
        items = kind.get("items", [])
        if row_y + t("row.height") > y + h - inside:
            break
        draw_label(c, x + inside, row_y + t("row.height") / 2 + t("font.label.size") / 3, f"{kind['title']} ({len(items)})")
        row_y += t("row.height")
        for item in items:
            if row_y + t("row.height") > y + h - inside:
                break
            draw_row(c, x, row_y, w, dict(item, kind=item.get("kind", "item"), depth=1))
            row_y += t("row.height")


def cell_width(t):
    return t("knob.size.default") + t("size.space.8")


def control_cells(parameter):
    # A choice and a value box alone are two knob cells wide, a slider three. [RQ-APP-086, ADR-APP-004 (DEC-APP-027)]
    control = parameter.get("control")
    if control == "slider":
        return SLIDER_CELLS
    return CHOICE_CELLS if control in ("choice", "valuebox") else 1


def group_size(t, group):
    inside = t("space.inside")
    cells = sum(control_cells(parameter) for parameter in group["parameters"])
    width = cells * cell_width(t) + 2 * inside
    height = (inside + t("font.label.size") + t("space.between") + t("knob.size.default") + t("space.between")
              + t("font.label.size") + t("size.space.1") + t("valuebox.height") + inside)
    return width, height


def draw_value_box(c, x, y, w, text, pending):
    t = c.t
    c.rect(x, y, w, t("valuebox.height"), t("valuebox.surface"), t("radius.control"))
    colour = t("colour.status.pending") if pending else t("valuebox.text")
    shown = text + ELLIPSIS if pending else text
    c.text(x + w / 2, y + t("valuebox.height") / 2 + t("font.value.size") / 3, shown, t("font.value.size"), colour, anchor="middle")


def draw_knob(c, cx, cy, fraction, colour, pending):
    t = c.t
    stroke = t("stroke.arc")
    radius = t("knob.size.default") / 2 - stroke
    c.circle(cx, cy, radius - stroke, t("colour.surface.recessed"))
    c.arc(cx, cy, radius, KNOB_START_DEGREES, KNOB_SWEEP_DEGREES, t("knob.arc.track"), stroke)
    value_colour = t("colour.status.pending") if pending else colour
    c.arc(cx, cy, radius, KNOB_START_DEGREES, KNOB_SWEEP_DEGREES * fraction, value_colour, stroke)
    angle = math.radians(KNOB_START_DEGREES + KNOB_SWEEP_DEGREES * fraction)
    pointer = radius * KNOB_POINTER_LENGTH
    c.line(cx, cy, cx + pointer * math.cos(angle), cy + pointer * math.sin(angle), t("knob.pointer"), t("stroke.focus"))


def draw_slider(c, x, cy, w, fraction, colour, pending):
    """A horizontal track, filled up to the value in the parameter's colour, with its thumb. [RQ-APP-086, ADR-APP-004 (DEC-APP-027)]"""
    t = c.t
    stroke = t("stroke.arc")
    c.line(x, cy, x + w, cy, t("slider.track"), stroke)
    value_colour = t("colour.status.pending") if pending else colour
    end = x + w * fraction
    if fraction > 0:
        c.line(x, cy, end, cy, value_colour, stroke)
    c.circle(end, cy, t("slider.thumb.size") / 2, t("slider.thumb"))


def draw_choice(c, x, y, w, text, pending):
    t = c.t
    h = t("size.control.medium")
    inside = t("space.inside")
    c.rect(x, y, w, h, t("choice.surface"), t("radius.control"))
    baseline = y + h / 2 + t("font.value.size") / 3
    c.text(x + inside, baseline, "<", t("font.value.size"), t("choice.arrow"))
    c.text(x + w - inside, baseline, ">", t("font.value.size"), t("choice.arrow"), anchor="end")
    shown = text + ELLIPSIS if pending else text
    colour = t("colour.status.pending") if pending else t("choice.text")
    c.text(x + w / 2, baseline, shown, t("font.value.size"), colour, anchor="middle")


def draw_group(c, x, y, group, values, pending):
    t = c.t
    inside = t("space.inside")
    width, height = group_size(t, group)
    draw_panel(c, x, y, width, height)
    title_y = y + inside + t("font.label.size")
    roles = {parameter.get("role", "general") for parameter in group["parameters"]}
    marker = role_colour(t, sorted(roles)[0]) if len(roles) == 1 else t("colour.text.secondary")
    c.rect(x + inside, y + inside, t("stroke.arc"), t("font.label.size"), marker)
    draw_label(c, x + inside + t("stroke.arc") + t("size.space.2"), title_y, group["title"])
    knob_top = title_y + t("space.between")
    cell_x = x + inside
    for parameter in group["parameters"]:
        cells = control_cells(parameter)
        w = cells * cell_width(t)
        colour = role_colour(t, parameter.get("role", "general"))
        value = values.get(parameter["name"], {})
        is_pending = parameter["name"] in pending
        centre = cell_x + w / 2
        if parameter.get("control") == "choice":
            box_y = knob_top + (t("knob.size.default") - t("size.control.medium")) / 2
            draw_choice(c, cell_x + t("size.space.1"), box_y, w - t("size.space.2"), value.get("text", ""), is_pending)
        elif parameter.get("control") == "slider":
            margin = t("slider.thumb.size")
            draw_slider(c, cell_x + margin, knob_top + t("knob.size.default") / 2, w - 2 * margin, float(value.get("fraction", 0.0)),
                        colour, is_pending)
        elif parameter.get("control") == "valuebox":
            box_y = knob_top + (t("knob.size.default") - t("valuebox.height")) / 2
            draw_value_box(c, cell_x + t("size.space.1"), box_y, w - t("size.space.2"), value.get("text", ""), is_pending)
        else:
            draw_knob(c, centre, knob_top + t("knob.size.default") / 2, float(value.get("fraction", 0.0)), colour, is_pending)
        label_y = knob_top + t("knob.size.default") + t("space.between") + t("font.label.size")
        draw_label(c, centre, label_y, parameter["label"], anchor="middle")
        if parameter.get("control") not in ("choice", "valuebox"):
            draw_value_box(c, cell_x + t("size.space.1"), label_y + t("size.space.1"), w - t("size.space.2"), value.get("text", ""), is_pending)
        cell_x += w
    return width, height


def draw_editor(c, x, y, w, h, editor, layouts):
    t = c.t
    inside = t("space.inside")
    header_h = t("size.control.large") + 2 * inside
    tab_h = t("tab.height")
    draw_panel(c, x, y, w, header_h)
    if editor is None:
        draw_panel(c, x, y + header_h + t("space.edge"), w, h - header_h - t("space.edge"))
        draw_not_connected(c, x, y + header_h, w, h - header_h, "Not connected: choose Sampler, then Connect")
        return None
    layout = layouts.get(editor["layout"])
    if layout is None:
        raise MockupError(f"the screen names the layout {editor['layout']}, which is not in the layout folders")
    title_y = y + header_h / 2 + t("font.title.size") / 3
    c.text(x + inside, title_y, editor["item"], t("font.title.size"), t("header.title"), WEIGHT_MEDIUM)
    item_width = len(editor["item"]) * t("font.title.size")
    draw_label(c, x + inside + item_width, title_y, editor["kind"], t("header.kind"))
    if "keygroup" in editor:
        selector_w = KEYGROUP_SELECTOR_IN_KNOBS * t("knob.size.default")
        draw_choice(c, x + w - inside - selector_w, y + (header_h - t("size.control.medium")) / 2, selector_w, editor["keygroup"], False)
    elif "info" in editor:
        # An editor with no part to choose shows what the item is instead (the sample's information). [RQ-APP-087]
        c.text(x + w - inside, title_y, editor["info"], t("font.caption.size"), t("colour.text.secondary"), anchor="end")

    tabs = layout["tabs"]
    tab = next((candidate for candidate in tabs if candidate["title"] == editor.get("tab")), tabs[0])
    group_x, group_y = x, y + header_h + t("space.group")
    row_height = 0.0
    for group in tab["groups"]:
        width, height = group_size(t, group)
        if group_x > x and group_x + width > x + w:
            group_x, group_y = x, group_y + row_height + t("space.group")
            row_height = 0.0
        draw_group(c, group_x, group_y, group, editor.get("values", {}), set(editor.get("pending", [])))
        group_x += width + t("space.group")
        row_height = max(row_height, height)

    bar_y = y + h - tab_h
    draw_panel(c, x, bar_y, w, tab_h)
    tab_x = x + inside
    for candidate in tabs:
        selected = candidate is tab
        colour = t("tab.text.selected") if selected else t("tab.text")
        label_w = len(candidate["title"]) * t("font.label.size") + 2 * inside
        draw_label(c, tab_x + label_w / 2, bar_y + tab_h / 2 + t("font.label.size") / 3, candidate["title"], colour, "middle")
        if selected:
            c.rect(tab_x, bar_y, label_w, t("stroke.focus"), t("tab.underline.selected"))
        tab_x += label_w
    return layout


def draw_menu(c, width, items):
    t = c.t
    height = t("row.height")
    c.rect(0, 0, width, height, t("colour.surface.panel"))
    x = t("space.edge")
    for item in items:
        c.text(x, height / 2 + t("font.value.size") / 3, item, t("font.value.size"), t("colour.text.primary"))
        x += len(item) * t("font.value.size") * 0.6 + t("size.space.6")


def draw_status(c, width, height, connection, caption):
    t = c.t
    bar_h = t("tab.height")
    y = height - bar_h
    inside = t("space.inside")
    c.rect(0, y, width, bar_h, t("colour.surface.panel"))
    state = connection.get("state", "disconnected")
    dot = {"connected": t("status.dot.connected"), "failed": t("status.dot.failed")}.get(state, t("colour.text.disabled"))
    c.circle(t("space.edge") + inside / 2, y + bar_h / 2, t("radius.control") + t("stroke.hairline"), dot)
    baseline = y + bar_h / 2 + t("font.value.size") / 3
    c.text(t("space.edge") + inside + t("size.space.2"), baseline, connection.get("text", ""), t("font.value.size"), t("colour.text.primary"))
    if connection.get("busy"):
        busy_x = width / 2
        c.arc(busy_x, y + bar_h / 2, t("size.icon.small") / 2, 0, KNOB_SWEEP_DEGREES, t("status.spinner"), t("stroke.focus"))
        c.text(busy_x + t("size.icon.default"), baseline, connection["busy"], t("font.value.size"), t("colour.text.secondary"))
    c.text(width - t("space.edge"), baseline, caption, t("font.caption.size"), t("colour.text.secondary"), anchor="end")


def draw_dialog(c, width, height, dialog):
    t = c.t
    inside = t("space.inside")
    c.rect(0, 0, width, height, t("dialog.scrim"), opacity=t("opacity.scrim"))
    w = DIALOG_WIDTH_IN_KNOBS * t("knob.size.default")
    lines = dialog.get("text", [])
    button_h = t("size.control.large")
    h = (2 * t("size.space.4") + t("font.heading.size") + t("size.space.4") + len(lines) * (t("font.value.size") + t("size.space.2"))
         + t("size.space.4") + button_h)
    x, y = (width - w) / 2, (height - h) / 2
    c.rect(x, y, w, h, t("dialog.surface"), t("radius.panel"), t("dialog.border"), t("stroke.hairline"))
    text_y = y + t("size.space.4") + t("font.heading.size")
    c.text(x + t("size.space.4"), text_y, dialog["title"], t("font.heading.size"), t("colour.text.primary"), WEIGHT_MEDIUM)
    text_y += t("size.space.4")
    for line in lines:
        text_y += t("font.value.size") + t("size.space.2")
        c.text(x + t("size.space.4"), text_y, line, t("font.value.size"), t("colour.text.secondary"))
    button_w = BUTTON_WIDTH_IN_KNOBS * t("knob.size.default")
    button_x = x + w - t("size.space.4") - button_w
    button_y = y + h - t("size.space.4") - button_h
    for button in reversed(dialog.get("buttons", [])):
        destructive = button.get("destructive", False)
        fill = t("button.destructive.surface") if destructive else t("button.surface")
        colour = t("button.destructive.text") if destructive else t("button.text")
        c.rect(button_x, button_y, button_w, button_h, fill, t("radius.control"))
        c.text(button_x + button_w / 2, button_y + button_h / 2 + t("font.value.size") / 3, button["text"], t("font.value.size"), colour,
               WEIGHT_MEDIUM, "middle")
        button_x -= button_w + inside


def font_faces(fonts):
    faces = []
    for weight in sorted(fonts):
        encoded = base64.b64encode(fonts[weight]).decode("ascii")
        faces.append(f"@font-face{{font-family:Inter;font-weight:{weight};src:url(data:font/woff2;base64,{encoded}) format('woff2');}}")
    return "".join(faces)


def merged_state(screen, state):
    if state not in screen.get("states", {}):
        raise MockupError(f"the screen {screen.get('name')} has no state {state}")
    content = dict(screen.get("content", {}))
    content.update(screen["states"][state])
    return content


def render_state(screen, state, data, layouts, fonts):
    """The SVG of one state of one screen. Raises MockupError naming what cannot be drawn."""
    t = Theme(data)
    c = Canvas(t)
    width, height = screen["size"]
    content = merged_state(screen, state)
    edge = t("space.edge")
    c.rect(0, 0, width, height, t("colour.surface.base"))
    draw_menu(c, width, screen.get("menu", []))
    top = t("row.height") + edge
    bottom = height - t("tab.height") - edge
    left_w = screen["leftWidth"]
    browser_h = (bottom - top - edge) * screen.get("browserShare", 0.5)
    draw_browser(c, edge, top, left_w, browser_h, content.get("browser"))
    draw_memory(c, edge, top + browser_h + edge, left_w, bottom - top - browser_h - edge, content.get("memory"))
    edit_x = edge + left_w + edge
    layout = draw_editor(c, edit_x, top, width - edit_x - edge, bottom - top, content.get("editor"), layouts)
    caption = f"MOCKUP {screen['name']} / {state}: example values"
    if layout is not None and layout.get("example"):
        caption += "; example grouping, not decided (TASK-APP-002)"
    draw_status(c, width, height, content.get("connection", {}), caption)
    if content.get("dialog"):
        draw_dialog(c, width, height, content["dialog"])
    header = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" '
              f'font-family="{FONT_STACK}">')
    comment = "<!-- GENERATED by juce/tools/generate_mockups.py: do not edit by hand. [RQ-APP-074, ADR-APP-003] -->"
    return "\n".join([header, comment, f"<style>{font_faces(fonts)}</style>", *c.items, "</svg>"]) + "\n"


def build_outputs(screens, layouts, data, fonts):
    """Every mockup, by file name."""
    outputs = {}
    for screen in screens:
        for state in screen.get("states", {}):
            outputs[f"{screen['name']}-{state}.svg"] = render_state(screen, state, data, layouts, fonts)
    return dict(sorted(outputs.items()))


def read_json_folder(folder):
    folder = pathlib.Path(folder)
    if not folder.is_dir():
        return {}
    return {path.stem: json.loads(path.read_text(encoding="utf-8")) for path in sorted(folder.glob("*.json"))}


def read_fonts(folder):
    fonts = {}
    for weight, name in FONT_FILES.items():
        path = pathlib.Path(folder) / name
        if not path.is_file():
            raise MockupError(f"the font file {path} is missing")
        fonts[weight] = path.read_bytes()
    return fonts


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="write nothing; fail when a committed mockup is behind its sources")
    parser.add_argument("--tokens", default=str(DEFAULT_TOKENS))
    parser.add_argument("--screens", default=str(DEFAULT_SCREENS))
    parser.add_argument("--layouts", action="append", help="a folder of layouts; may be given several times")
    parser.add_argument("--fonts", default=str(DEFAULT_FONTS))
    parser.add_argument("--out", default=str(DEFAULT_OUT))
    arguments = parser.parse_args(argv)
    try:
        data = json.loads(pathlib.Path(arguments.tokens).read_text(encoding="utf-8"))
        layouts = {}
        for folder in arguments.layouts or DEFAULT_LAYOUTS:
            layouts.update(read_json_folder(folder))
        screens = list(read_json_folder(arguments.screens).values())
        outputs = build_outputs(screens, layouts, data, read_fonts(arguments.fonts))
    except (MockupError, OSError, ValueError, KeyError) as error:
        print(f"generate_mockups: {error}", file=sys.stderr)
        return 1
    out = pathlib.Path(arguments.out)
    if arguments.check:
        behind = [name for name, svg in outputs.items() if not (out / name).is_file() or (out / name).read_text(encoding="utf-8") != svg]
        if behind:
            print("generate_mockups: behind their sources, run python3 juce/tools/generate_mockups.py: " + ", ".join(behind))
            return 1
        print(f"generate_mockups: the {len(outputs)} mockups are up to date")
        return 0
    out.mkdir(parents=True, exist_ok=True)
    for name, svg in outputs.items():
        with open(out / name, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(svg)
    print(f"generate_mockups: wrote {len(outputs)} mockups into {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
