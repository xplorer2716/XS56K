#!/usr/bin/env python3
"""Writes the design system's token module and the value tables of its document from the token file.

Reads `juce/app/design/tokens.json` (global tokens hold values, semantic tokens alias global ones, component tokens alias semantic or global
ones), checks it (every alias resolves, no cycle, no semantic or component token holding a value, every listed text pair reaches the minimum
contrast), and writes:
  - `juce/app/core/include/app/DesignTokens.generated.hpp`: `constexpr` constants in namespace `app::tokens`, no JUCE include;
  - the tables between the `<!-- BEGIN GENERATED: ... -->` markers of `process/1.requirements/DESIGN-SYSTEM-APP.md`;
  - `juce/app/core/include/app/EditorLayouts.generated.hpp`: the editor layouts of `juce/app/design/layouts/*.json` as `constexpr` tables,
    with the shape sets of `juce/app/design/shapes/*.json` a choice names.
`--check` writes nothing and fails, naming the files, when one is behind its source. Never run during the build.

Usage: python3 juce/tools/generate_design.py [--check] [--tokens PATH] [--header PATH] [--doc PATH] [--layouts DIR] [--shapes DIR]
       [--layouts-header PATH]

[TASK-APP-007, TASK-APP-035, RQ-APP-002, RQ-APP-005, RQ-APP-086, ADR-APP-002 (DEC-APP-011, DEC-APP-012), ADR-APP-004 (DEC-APP-024, DEC-APP-032), TASK-APP-051]
"""
import argparse
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_TOKENS = ROOT / "juce" / "app" / "design" / "tokens.json"
DEFAULT_HEADER = ROOT / "juce" / "app" / "core" / "include" / "app" / "DesignTokens.generated.hpp"
DEFAULT_DOC = ROOT / "process" / "1.requirements" / "DESIGN-SYSTEM-APP.md"
DEFAULT_LAYOUTS = ROOT / "juce" / "app" / "design" / "layouts"
DEFAULT_SHAPES = ROOT / "juce" / "app" / "design" / "shapes"
DEFAULT_LAYOUTS_HEADER = ROOT / "juce" / "app" / "core" / "include" / "app" / "EditorLayouts.generated.hpp"
CONTROLS = {"knob": "Knob", "choice": "Choice", "switch": "Switch", "valuebox": "ValueBox", "slider": "Slider"}
ROLES = {"sample": "Sample", "modulation": "Modulation", "filter": "Filter", "amp": "Amp", "general": "General"}
SCOPES = {"program": "Program", "keygroup": "Keygroup", "zone": "Zone"}

TIERS = ("global", "semantic", "component")
COLOUR_PATTERN = re.compile(r"^#[0-9A-Fa-f]{6}$")
OPAQUE_ALPHA = 0xFF000000
SECTION_GLOBAL_COLOURS = "global-colours"
SECTION_SEMANTIC_COLOURS = "semantic-colours"
SECTION_CONTRAST = "contrast"
BEGIN_MARKER = "<!-- BEGIN GENERATED: {} -->"
END_MARKER = "<!-- END GENERATED: {} -->"


class TokenError(Exception):
    """A token file that cannot be turned into constants: the message names the token."""


def alias_of(tier, entry):
    """The name a semantic or component entry aliases."""
    if tier == "semantic" and isinstance(entry, dict):
        return entry.get("alias")
    return entry


def tier_of(data):
    """The tier of every token name, refusing a name declared in two tiers."""
    tiers = {}
    for tier in TIERS:
        for name in data.get(tier, {}):
            if name in tiers:
                raise TokenError(f"the token {name} is declared in both {tiers[name]} and {tier}")
            tiers[name] = tier
    return tiers


def resolve_all(data):
    """The global value every token resolves to, by name. Raises TokenError naming the faulty token."""
    tiers = tier_of(data)
    resolved = dict(data.get("global", {}))
    allowed_targets = {"semantic": ("global",), "component": ("semantic", "global")}

    def resolve(name, trail):
        if name in resolved:
            return resolved[name]
        if name in trail:
            raise TokenError("cycle of aliases: " + " -> ".join(trail + [name]))
        tier = tiers.get(name)
        if tier is None:
            raise TokenError(f"{trail[-1] if trail else name} aliases {name}, which is not a token")
        target = alias_of(tier, data[tier][name])
        if not isinstance(target, str) or target not in tiers:
            raise TokenError(f"the {tier} token {name} holds a value or a missing alias ({target!r}); only global tokens hold values")
        if tiers[target] not in allowed_targets[tier] and tiers[target] != tier:
            raise TokenError(f"the {tier} token {name} aliases the {tiers[target]} token {target}")
        if tier == "semantic" and tiers[target] != "global" and tiers[target] != "semantic":
            raise TokenError(f"the semantic token {name} aliases {target}, which is not a global token")
        value = resolve(target, trail + [name])
        resolved[name] = value
        return value

    for tier in ("semantic", "component"):
        for name in data.get(tier, {}):
            resolve(name, [])
    return resolved


def relative_luminance(colour):
    channels = [int(colour[index:index + 2], 16) / 255 for index in (1, 3, 5)]
    linear = [c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4 for c in channels]
    return 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2]


def contrast_ratio(first, second):
    """The WCAG 2.x contrast ratio of two `#RRGGBB` colours."""
    lighter, darker = sorted((relative_luminance(first), relative_luminance(second)), reverse=True)
    return (lighter + 0.05) / (darker + 0.05)


def check_contrast(data, resolved):
    """Rows (text, surface, ratio) of the listed pairs, and one failure line per pair under the minimum."""
    contrast = data.get("contrast", {})
    minimum = contrast.get("minimum", 0)
    pairs = [(text, surface) for text in contrast.get("text", []) for surface in contrast.get("on", [])]
    pairs += [tuple(pair) for pair in contrast.get("pairs", [])]
    rows, failures = [], []
    for text, surface in pairs:
        for name in (text, surface):
            if name not in resolved:
                raise TokenError(f"the contrast list names {name}, which is not a token")
        ratio = contrast_ratio(resolved[text], resolved[surface])
        rows.append((text, surface, ratio))
        if ratio < minimum:
            failures.append(f"{text} on {surface}: {ratio:.2f} : 1, under the minimum {minimum} : 1")
    return rows, failures


def cpp_name(name):
    """`colour.surface.panel` -> `kColourSurfacePanel`: dots and hyphens dropped, each word capitalised."""
    words = re.split(r"[.\-]", name)
    return "k" + "".join(word[:1].upper() + word[1:] for word in words)


def cpp_constant(name, value):
    identifier = cpp_name(name)
    if isinstance(value, str) and COLOUR_PATTERN.match(value):
        return f"constexpr std::uint32_t {identifier} = 0x{OPAQUE_ALPHA | int(value[1:], 16):08X};"
    if isinstance(value, str):
        return f'constexpr std::string_view {identifier} = "{value}";'
    if name.startswith("motion."):
        return f"constexpr std::chrono::milliseconds {identifier}{{{int(value)}}};"
    if isinstance(value, int) and name.startswith("font.weight."):
        return f"constexpr int {identifier} = {value};"
    return f"constexpr float {identifier} = {float(value)!r}f;"


def target_name(data, name):
    """The token a constant is resolved from, for its comment."""
    for tier in ("semantic", "component"):
        if name in data.get(tier, {}):
            return alias_of(tier, data[tier][name])
    return None


def render_header(data, resolved):
    lines = [
        "// GENERATED by juce/tools/generate_design.py from juce/app/design/tokens.json: do not edit by hand.",
        "// The design system's token module: global, semantic and component tokens as constants, no JUCE.",
        "// [RQ-APP-002, ADR-APP-002 (DEC-APP-011, DEC-APP-012), process/1.requirements/DESIGN-SYSTEM-APP.md]",
        "#pragma once",
        "",
        "#include <chrono>",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace app::tokens",
        "{",
    ]
    for tier in TIERS:
        lines.append(f"    // {tier} tokens")
        for name in data.get(tier, {}):
            comment = f"  // {name}" if tier == "global" else f"  // {name} -> {target_name(data, name)}"
            lines.append("    " + cpp_constant(name, resolved[name]) + comment)
        lines.append("")
    lines.append("}")
    return "\n".join(lines) + "\n"


def render_doc_sections(data, resolved):
    colours = [(name, value) for name, value in data.get("global", {}).items() if isinstance(value, str) and COLOUR_PATTERN.match(value)]
    global_table = ["| Token | Value |", "|---|---|"] + [f"| `{name}` | `{value}` |" for name, value in colours]
    semantic_table = ["| Token | Aliases | Use |", "|---|---|---|"]
    for name, entry in data.get("semantic", {}).items():
        if name.startswith("colour."):
            semantic_table.append(f"| `{name}` | `{alias_of('semantic', entry)}` | {entry.get('use', '') if isinstance(entry, dict) else ''} |")
    rows, _ = check_contrast(data, resolved)
    contrast_table = ["| Text | On | Ratio |", "|---|---|---|"] + [f"| `{text}` | `{surface}` | {ratio:.2f} |" for text, surface, ratio in rows]
    return {
        SECTION_GLOBAL_COLOURS: "\n".join(global_table),
        SECTION_SEMANTIC_COLOURS: "\n".join(semantic_table),
        SECTION_CONTRAST: "\n".join(contrast_table),
    }


def replace_sections(text, sections):
    for key, content in sections.items():
        begin, end = BEGIN_MARKER.format(key), END_MARKER.format(key)
        start, stop = text.find(begin), text.find(end)
        if start < 0 or stop < start:
            raise TokenError(f"the document has no '{begin}' ... '{end}' markers for the section {key}")
        text = text[:start + len(begin)] + "\n" + content + "\n" + text[stop:]
    return text


def cpp_text(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def render_envelope(editor, group):
    """An envelope group's stages as C++ initialisers: a time and an end level (a parameter, or a fixed level 0 to 100), or the sustain
    mark. Each name must be one of the group's parameters. [RQ-APP-095, ADR-APP-004 (DEC-APP-031)]"""
    names = {parameter["name"] for parameter in group.get("parameters", [])}
    stages = []
    for stage in group.get("envelope", []):
        if stage.get("sustain"):
            stages.append('{"", "", 0, true}')
            continue
        level = stage.get("level", 0)
        for name in [stage.get("time")] + ([level] if isinstance(level, str) else []):
            if name not in names:
                raise TokenError(f"the layout {editor} draws {name} in the envelope of {group['title']}, which does not show it")
        if isinstance(level, str):
            stages.append(f"{{{cpp_text(stage['time'])}, {cpp_text(level)}, 0, false}}")
        else:
            stages.append(f"{{{cpp_text(stage['time'])}, \"\", {int(level)}, false}}")
    return stages


CLOSE_TABLE = "    }};"  # the end of a constexpr table of the generated header


def shape_table(name):
    """The C++ name of a shape set's tables: "filter types" gives kFilterTypes."""
    return "k" + "".join(word[:1].upper() + word[1:] for word in name.split())


def check_polyline(set_name, label, points):
    """A shape is at least two points in the unit square, x never decreasing. [RQ-APP-096, DEC-APP-032]"""
    if len(points) < 2:
        raise TokenError(f"the shape {label} of {set_name} has fewer than two points")
    for x, y in points:
        if not (0 <= x <= 1 and 0 <= y <= 1):
            raise TokenError(f"the shape {label} of {set_name} leaves the unit square at {x}, {y}")
    if any(later[0] < earlier[0] for earlier, later in zip(points, points[1:])):
        raise TokenError(f"the shape {label} of {set_name} goes back along x")


def render_shapes(shapes, lines):
    """Every shape set as tables: a polyline per label, then the set, label by label in the file's order."""
    for set_name, shape_set in sorted(shapes.items()):
        table = shape_table(set_name)
        entries = []
        for index, (label, points) in enumerate(shape_set.get("shapes", {}).items()):
            check_polyline(set_name, label, points)
            pairs = ", ".join(f"{{{float(x)!r}f, {float(y)!r}f}}" for x, y in points)
            lines.append(f"    inline constexpr std::array<ShapePoint, {len(points)}> {table}Shape{index}{{{{{pairs}}}}};")
            entries.append(f"ChoiceShape{{{cpp_text(label)}, {table}Shape{index}}}")
        lines.append(f"    inline constexpr std::array<ChoiceShape, {len(entries)}> {table}Shapes{{{{")
        lines += [f"        {entry}," for entry in entries]
        lines.append(CLOSE_TABLE)


def render_parameter(editor, parameter, shapes):
    """One parameter's initialiser. Its control and role must be known ones; a choice may name a shape set."""
    control, role = parameter.get("control"), parameter.get("role")
    if control not in CONTROLS:
        raise TokenError(f"the layout {editor} gives {parameter.get('name')} the unknown control {control}")
    if role not in ROLES:
        raise TokenError(f"the layout {editor} gives {parameter.get('name')} the unknown role {role}")
    fields = [cpp_text(parameter["name"]), cpp_text(parameter["label"]), f"Role::{ROLES[role]}", f"Control::{CONTROLS[control]}"]
    shape_set = parameter.get("shapes")
    if shape_set is None:
        fields.append("{}")  # every field given: no missing-initialiser warning [RQ-BLD-003]
    else:
        if control != "choice":
            raise TokenError(f"the layout {editor} gives shapes to {parameter['name']}, which is not a choice")
        if shape_set not in shapes:
            raise TokenError(f"the layout {editor} gives {parameter['name']} the unknown shape set {shape_set}")
        fields.append(f"{shape_table(shape_set)}Shapes")
    return "{" + ", ".join(fields) + "}"


def render_parameters(editor, group, name, shapes):
    """The table of one group's parameters, as lines."""
    rows = [render_parameter(editor, parameter, shapes) for parameter in group.get("parameters", [])]
    return ([f"    inline constexpr std::array<LayoutParameter, {len(rows)}> {name}{{{{"] + [f"        {row}," for row in rows] + [CLOSE_TABLE])


def render_scope(editor, group):
    scope = group.get("scope")
    if scope is not None and scope not in SCOPES:
        raise TokenError(f"the layout {editor} gives the group {group['title']} the unknown scope {scope}")
    return f"Scope::{SCOPES[scope]}" if scope else "Scope::None"


def render_group(editor, group, name, lines, shapes):
    """One group: its tables are appended to `lines`, its initialiser is returned."""
    lines += render_parameters(editor, group, name, shapes)
    scope_cpp = render_scope(editor, group)
    envelope = render_envelope(editor, group)
    envelope_cpp = "{}"
    if envelope:
        envelope_cpp = f"{name}Envelope"
        lines.append(f"    inline constexpr std::array<EnvelopeStage, {len(envelope)}> {envelope_cpp}{{{{")
        lines += [f"        {stage}," for stage in envelope]
        lines.append(CLOSE_TABLE)
    row = "true" if group.get("row") else "false"
    return f"LayoutGroup{{{cpp_text(group['title'])}, {name}, {scope_cpp}, {envelope_cpp}, {row}}}"


def render_tab(editor, prefix, tab_index, tab, lines, shapes):
    """One tab: its groups' tables, then its own, are appended to `lines`; its initialiser is returned."""
    group_names = [render_group(editor, group, f"{prefix}Tab{tab_index}Group{group_index}", lines, shapes)
                   for group_index, group in enumerate(tab.get("groups", []))]
    name = f"{prefix}Tab{tab_index}"
    lines.append(f"    inline constexpr std::array<LayoutGroup, {len(group_names)}> {name}Groups{{{{")
    lines += [f"        {group}," for group in group_names]
    lines.append(CLOSE_TABLE)
    return f"LayoutTab{{{cpp_text(tab['title'])}, {name}Groups}}"


def render_orders(editor, prefix, layout, lines):
    """The chains of parameters whose values must not decrease, each name one the layout shows. [RQ-APP-092, DEC-APP-029]"""
    shown = {parameter["name"] for tab in layout.get("tabs", []) for group in tab.get("groups", [])
             for parameter in group.get("parameters", [])}
    order_names = []
    for order_index, order in enumerate(layout.get("orders", [])):
        for name in order:
            if name not in shown:
                raise TokenError(f"the layout {editor} orders {name}, which it does not show")
        name = f"{prefix}Order{order_index}"
        lines.append(f"    inline constexpr std::array<std::string_view, {len(order)}> {name}{{{{")
        lines += [f"        {cpp_text(item)}," for item in order]
        lines.append(CLOSE_TABLE)
        order_names.append(f"std::span<const std::string_view>{{{name}}}")
    lines.append(f"    inline constexpr std::array<std::span<const std::string_view>, {len(order_names)}> {prefix}Orders{{{{")
    lines += [f"        {order}," for order in order_names]
    lines.append(CLOSE_TABLE)


def render_editor(editor, layout, lines, shapes):
    """One editor: its tabs and orders are appended to `lines`; its initialiser is returned."""
    prefix = "k" + editor[:1].upper() + editor[1:]
    tab_names = [render_tab(editor, prefix, tab_index, tab, lines, shapes) for tab_index, tab in enumerate(layout.get("tabs", []))]
    lines.append(f"    inline constexpr std::array<LayoutTab, {len(tab_names)}> {prefix}Tabs{{{{")
    lines += [f"        {tab}," for tab in tab_names]
    lines.append(CLOSE_TABLE)
    render_orders(editor, prefix, layout, lines)
    return f"EditorLayout{{{cpp_text(editor)}, {prefix}Tabs, {prefix}Orders}}"


def render_layouts(layouts, shapes=None):
    """The editor layouts, by editor name, as constexpr tables in namespace `app::layouts` (types in `app/EditorLayout.hpp`), after the
    shape sets, by name, that their choices may name."""
    shapes = shapes or {}
    lines = [
        "// GENERATED by juce/tools/generate_design.py from the JSON files of juce/app/design/layouts/ and shapes/: do not edit by hand.",
        "// The editor layouts: each editor's tabs, groups and parameters, by catalogue name; the shapes a choice draws. No JUCE.",
        "// [RQ-APP-049, RQ-APP-086, RQ-APP-092, RQ-APP-096, ADR-APP-001 (DEC-APP-006), ADR-APP-004 (DEC-APP-024, DEC-APP-029, DEC-APP-032)]",
        "#pragma once",
        "",
        "#include <array>",
        "",
        '#include "app/EditorLayout.hpp"',
        "",
        "namespace app::layouts",
        "{",
    ]
    render_shapes(shapes, lines)
    editors = [render_editor(editor, layout, lines, shapes) for editor, layout in sorted(layouts.items())]
    lines.append(f"    inline constexpr std::array<EditorLayout, {len(editors)}> kEditorLayouts{{{{")
    lines += [f"        {editor}," for editor in editors]
    lines.append(CLOSE_TABLE)
    lines.append("}")
    return "\n".join(lines) + "\n"


def read_layouts(folder):
    folder = pathlib.Path(folder)
    if not folder.is_dir():
        return {}
    layouts = {}
    for path in sorted(folder.glob("*.json")):
        layout = json.loads(path.read_text(encoding="utf-8"))
        layouts[layout.get("editor", path.stem)] = layout
    return layouts


def read_shapes(folder):
    """The shape sets of a folder, by their name. [RQ-APP-096, DEC-APP-032]"""
    folder = pathlib.Path(folder)
    if not folder.is_dir():
        return {}
    sets = [json.loads(path.read_text(encoding="utf-8")) for path in sorted(folder.glob("*.json"))]
    return {shape_set["name"]: shape_set for shape_set in sets}


def generate(tokens_path, doc_path):
    data = json.loads(pathlib.Path(tokens_path).read_text(encoding="utf-8"))
    resolved = resolve_all(data)
    _, failures = check_contrast(data, resolved)
    if failures:
        raise TokenError("contrast too low: " + "; ".join(failures))
    header = render_header(data, resolved)
    doc = replace_sections(pathlib.Path(doc_path).read_text(encoding="utf-8"), render_doc_sections(data, resolved))
    return header, doc


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="write nothing; fail when a generated file is behind the token file")
    parser.add_argument("--tokens", default=str(DEFAULT_TOKENS))
    parser.add_argument("--header", default=str(DEFAULT_HEADER))
    parser.add_argument("--doc", default=str(DEFAULT_DOC))
    parser.add_argument("--layouts", default=str(DEFAULT_LAYOUTS))
    parser.add_argument("--shapes", default=str(DEFAULT_SHAPES))
    parser.add_argument("--layouts-header", default=str(DEFAULT_LAYOUTS_HEADER))
    arguments = parser.parse_args(argv)
    try:
        header, doc = generate(arguments.tokens, arguments.doc)
        layouts_header = render_layouts(read_layouts(arguments.layouts), read_shapes(arguments.shapes))
    except (TokenError, OSError, ValueError) as error:
        print(f"generate_design: {error}", file=sys.stderr)
        return 1
    outputs = [(pathlib.Path(arguments.header), header), (pathlib.Path(arguments.doc), doc),
               (pathlib.Path(arguments.layouts_header), layouts_header)]
    if arguments.check:
        behind = [str(path) for path, content in outputs if not path.exists() or path.read_text(encoding="utf-8") != content]
        if behind:
            print("generate_design: behind their sources, run python3 juce/tools/generate_design.py: " + ", ".join(behind))
            return 1
        print("generate_design: the token module, the document's tables and the editor layouts are up to date")
        return 0
    for path, content in outputs:
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(content)
    print(f"generate_design: wrote {outputs[0][0]}, the tables of {outputs[1][0]} and {outputs[2][0]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
