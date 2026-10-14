#!/usr/bin/env python3
"""Writes the design system's token module and the value tables of its document from the token file.

Reads `juce/app/design/tokens.json` (global tokens hold values, semantic tokens alias global ones, component tokens alias semantic or global
ones), checks it (every alias resolves, no cycle, no semantic or component token holding a value, every listed text pair reaches the minimum
contrast), and writes:
  - `juce/app/core/include/app/DesignTokens.generated.hpp`: `constexpr` constants in namespace `app::tokens`, no JUCE include;
  - the tables between the `<!-- BEGIN GENERATED: ... -->` markers of `process/1.requirements/DESIGN-SYSTEM-APP.md`.
`--check` writes nothing and fails, naming the files, when either is behind the token file. Never run during the build.

Usage: python3 juce/tools/generate_design.py [--check] [--tokens PATH] [--header PATH] [--doc PATH]

[TASK-APP-007, RQ-APP-002, RQ-APP-005, ADR-APP-002 (DEC-APP-011, DEC-APP-012)]
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
    arguments = parser.parse_args(argv)
    try:
        header, doc = generate(arguments.tokens, arguments.doc)
    except (TokenError, OSError, ValueError) as error:
        print(f"generate_design: {error}", file=sys.stderr)
        return 1
    outputs = [(pathlib.Path(arguments.header), header), (pathlib.Path(arguments.doc), doc)]
    if arguments.check:
        behind = [str(path) for path, content in outputs if not path.exists() or path.read_text(encoding="utf-8") != content]
        if behind:
            print("generate_design: behind the token file, run python3 juce/tools/generate_design.py: " + ", ".join(behind))
            return 1
        print("generate_design: the token module and the document's tables are up to date")
        return 0
    for path, content in outputs:
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(content)
    print(f"generate_design: wrote {outputs[0][0]} and the tables of {outputs[1][0]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
