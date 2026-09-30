#!/usr/bin/env python3
"""Generate and check the table of the AKM item catalogue.

ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012) keeps the SysEx items the AKM layer sends as data: one record per
spec item in a human-reviewed file, juce/akm/data/items.json. This script turns that file into the
constexpr table that is checked in as juce/akm/include/akm/ItemTable.generated.hpp, and compares the
file with the spec's own item list, documents/_index/sysex_spec.items.tsv. It runs by hand and in the
test suite (ctest), never during the build.

The table is generated from the data file and NOT from the TSV: the TSV's range columns are free text
extracted from a PDF, and the spec has known errata (documents/_index/sysex_spec.kb.md). The TSV is a
checking aid: --coverage reads the ranges it can read and reports what it cannot compare.

Usage:
    python3 juce/tools/generate_akm_items.py              # write the table
    python3 juce/tools/generate_akm_items.py --check      # fail if the table is out of date
    python3 juce/tools/generate_akm_items.py --coverage   # compare the data file with the spec's items

(On Windows the interpreter is usually `python`.)

Exit status: 0 when everything holds, 1 when the table is out of date or the data file disagrees with the
spec, 2 when the data file is invalid or an input cannot be read.

[TASK-AKM-008, RQ-AKM-001, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]
"""
import argparse
import csv
import json
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
DEFAULT_DATA = ROOT / "juce" / "akm" / "data" / "items.json"
DEFAULT_OUTPUT = ROOT / "juce" / "akm" / "include" / "akm" / "ItemTable.generated.hpp"
DEFAULT_TSV = ROOT / "documents" / "_index" / "sysex_spec.items.tsv"

SCHEMA_VERSION = 1
EXIT_OK = 0
EXIT_MISMATCH = 1
EXIT_INVALID = 2

# Value formats of the spec (pp. 8-9) the schema supports: name -> (C++ enumerator, minimum, maximum).
# Qwords and the conditional layouts of later sections are added when the first item that needs one is
# catalogued (DEC-AKM-003). "string" was added by DEC-AKM-013 for the first items that carry an ASCII
# name (FTR-AKM-002): min/max are a character count, not a numeric range: STRING_MAX_LENGTH is a generous
# structural ceiling, not a spec or hardware limit — each item declares its own real bound (e.g. Program
# names: 0-20, observed on a real S5000, documents/_index/sysex_spec.kb.md "Common value codes").
BYTE_MAX = 127
WORD_MAX = 128 ** 2 - 1
DWORD_MAX = 128 ** 4 - 1
STRING_MAX_LENGTH = 255
FORMATS = {
    "byte": ("Byte", 0, BYTE_MAX),
    "word": ("Word", 0, WORD_MAX),
    "dword": ("Dword", 0, DWORD_MAX),
    "signed_byte": ("SignedByte", -BYTE_MAX, BYTE_MAX),
    "signed_word": ("SignedWord", -WORD_MAX, WORD_MAX),
    "signed_dword": ("SignedDword", -DWORD_MAX, DWORD_MAX),
    "string": ("String", 0, STRING_MAX_LENGTH),
}
KINDS = {"set": "Set", "get": "Get"}

SECTION_PATTERN = re.compile(r"^[0-7][0-9A-F]$")
IDENTIFIER_PATTERN = re.compile(r"^[A-Z][A-Za-z0-9]*$")
ARGUMENT_NAME_PATTERN = re.compile(r"^[a-z][A-Za-z0-9]*$")
REQUIREMENT_PATTERN = re.compile(r"^RQ-[A-Z]{3}-\d{3}$")

LICENSE_HEADER = """/*
XS56K - a realtime editor for the AKAI S5000/S6000 samplers
Copyright (C) 2026 https://github.com/xplorer2716

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
"""


class CatalogueError(Exception):
    """The data file is not valid; carries every problem found."""

    def __init__(self, problems):
        super().__init__("; ".join(problems))
        self.problems = problems


# --- reading and validating the data file ---------------------------------------------------------


def load_catalogue(path):
    try:
        return json.loads(pathlib.Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise CatalogueError([f"{path}: cannot be read as JSON ({error})"])


def validate_values(owner, label, values, problems):
    """Checks a list of argument or reply values."""
    if not isinstance(values, list):
        problems.append(f"{owner}: {label} must be a list")
        return
    seen = set()
    for index, value in enumerate(values):
        where = f"{owner}: {label}[{index}]"
        if not isinstance(value, dict):
            problems.append(f"{where} must be an object")
            continue
        name = value.get("name")
        if not isinstance(name, str) or not ARGUMENT_NAME_PATTERN.match(name):
            problems.append(f"{where}: name must be lowerCamelCase")
        elif name in seen:
            problems.append(f"{where}: name '{name}' is used twice")
        else:
            seen.add(name)
        format_name = value.get("format")
        if format_name not in FORMATS:
            problems.append(f"{where}: format '{format_name}' is not supported ({', '.join(FORMATS)})")
            continue
        _, format_min, format_max = FORMATS[format_name]
        low, high = value.get("min"), value.get("max")
        if not isinstance(low, int) or not isinstance(high, int) or isinstance(low, bool) or isinstance(high, bool):
            problems.append(f"{where}: min and max must be integers")
        elif not format_min <= low <= high <= format_max:
            problems.append(f"{where}: range {low}..{high} is not inside the {format_name} range "
                            f"{format_min}..{format_max}")


def validate(catalogue):
    """Returns the list of problems of a loaded data file; empty when it is valid."""
    problems = []
    if not isinstance(catalogue, dict) or catalogue.get("schema") != SCHEMA_VERSION:
        return [f"the data file must be an object with \"schema\": {SCHEMA_VERSION}"]

    sections = {}
    for entry in catalogue.get("sections", []):
        code = entry.get("section")
        if not isinstance(code, str) or not SECTION_PATTERN.match(code):
            problems.append(f"section {code!r}: must be two hexadecimal digits, at most 7F")
        elif code in sections:
            problems.append(f"section {code}: declared twice")
        else:
            sections[code] = entry
        if not isinstance(entry.get("complete"), bool):
            problems.append(f"section {code}: \"complete\" must be true or false")
        if not entry.get("name") or not entry.get("spec"):
            problems.append(f"section {code}: \"name\" and \"spec\" are required")

    identifiers = {}
    positions = {}
    for entry in catalogue.get("items", []):
        owner = str(entry.get("id"))
        if not IDENTIFIER_PATTERN.match(owner):
            problems.append(f"{owner}: id must be PascalCase")
        elif owner in identifiers:
            problems.append(f"{owner}: id is used twice")
        identifiers[owner] = entry

        section, item = entry.get("section"), entry.get("item")
        if section not in sections:
            problems.append(f"{owner}: section {section} is not declared in \"sections\"")
        if not isinstance(item, str) or not SECTION_PATTERN.match(item):
            problems.append(f"{owner}: item {item!r} must be two hexadecimal digits, at most 7F")
        elif (section, item) in positions:
            problems.append(f"{owner}: duplicates section {section} item {item} of {positions[(section, item)]}")
        else:
            positions[(section, item)] = owner
        if not entry.get("name"):
            problems.append(f"{owner}: name is required")

        kind = entry.get("kind")
        if kind not in KINDS:
            problems.append(f"{owner}: kind must be one of {', '.join(KINDS)}")
        validate_values(owner, "args", entry.get("args"), problems)
        if kind == "get":
            if not entry.get("reply"):
                problems.append(f"{owner}: a get needs a reply format")
            else:
                validate_values(owner, "reply", entry.get("reply"), problems)
        elif "reply" in entry:
            problems.append(f"{owner}: a set is answered by DONE and has no reply format")

        requirements = entry.get("requirements")
        if not isinstance(requirements, list) or not requirements:
            problems.append(f"{owner}: at least one requirement is needed for traceability")
        else:
            for requirement in requirements:
                if not REQUIREMENT_PATTERN.match(str(requirement)):
                    problems.append(f"{owner}: '{requirement}' is not a requirement ID")
    return problems


# --- generating the table -------------------------------------------------------------------------


def constant_name(identifier):
    """SysExQuery -> SYS_EX_QUERY."""
    return re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", identifier).upper()


def render_values(name, values):
    lines = [f"        inline constexpr std::array<ValueSpec, {len(values)}> {name}{{{{"]
    for value in values:
        enumerator = FORMATS[value["format"]][0]
        lines.append(f"            {{\"{value['name']}\", ValueFormat::{enumerator}, {value['min']}, {value['max']}}},")
    lines.append("        }};")
    return lines


def render(catalogue):
    """The text of the generated header."""
    items = catalogue["items"]
    requirements = sorted({requirement for entry in items for requirement in entry["requirements"]})
    lines = [LICENSE_HEADER.rstrip("\n")]
    lines += [
        "// GENERATED by juce/tools/generate_akm_items.py from juce/akm/data/items.json - do not edit. Change the",
        "// data file, regenerate, and review both diffs; `--check` fails when this file is out of date.",
        f"// [{', '.join(requirements)}, ADR-AKM-001 (DEC-AKM-003, DEC-AKM-012)]",
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstddef>",
        "",
        "#include \"akm/ItemDescriptor.hpp\"",
        "",
        "namespace akm",
        "{",
        "    /// One enumerator per record of the data file, in its order; the value is the record's index in",
        "    /// ITEM_TABLE.",
        "    enum class ItemId : std::size_t",
        "    {",
    ]
    lines += [f"        {entry['id']}," for entry in items]
    lines += ["    };", "", "    namespace item_data", "    {"]
    for entry in items:
        constant = constant_name(entry["id"])
        if entry["args"]:
            lines += render_values(f"{constant}_ARGS", entry["args"])
        if entry.get("reply"):
            lines += render_values(f"{constant}_REPLY", entry["reply"])
    lines += ["    }", "", "    /// Every record of the data file, indexed by ItemId.",
              f"    inline constexpr std::array<ItemDescriptor, {len(items)}> ITEM_TABLE{{{{"]
    for entry in items:
        constant = constant_name(entry["id"])
        arguments = f"item_data::{constant}_ARGS" if entry["args"] else "{}"
        reply = f"item_data::{constant}_REPLY" if entry.get("reply") else "{}"
        lines.append(f"        // section {entry['section']} item {entry['item']} [{', '.join(entry['requirements'])}]")
        # json.dumps escapes quotes and backslashes the same way a C++ string literal needs them.
        lines.append(f"        {{{json.dumps(entry['name'])}, 0x{entry['section']}, 0x{entry['item']}, "
                     f"ItemKind::{KINDS[entry['kind']]}, {arguments}, {reply}}},")
    lines += ["    }};", "}", ""]
    return "\n".join(lines)


def check_table(text, output):
    if output.exists() and output.read_text(encoding="utf-8") == text:
        print(f"{output.name} is up to date")
        return EXIT_OK
    print(f"{output} is missing or out of date - run: python3 juce/tools/generate_akm_items.py", file=sys.stderr)
    return EXIT_MISMATCH


# --- comparing the data file with the spec ---------------------------------------------------------


def read_spec_rows(path):
    """The spec's item rows: {(kind, section, item): row}, kind being 'C' (command) or 'R' (reply format)."""
    rows = {}
    with open(path, encoding="utf-8", newline="") as handle:
        reader = csv.reader(handle, delimiter="\t", quoting=csv.QUOTE_NONE)
        header = next(reader)
        for fields in reader:
            if not fields or fields[0].startswith("#"):
                continue
            row = dict(zip(header, fields + [""] * (len(header) - len(fields))))
            rows[(row["kind"], row["sec"], row["item"].lstrip("&"))] = row
    return rows


def parse_domain(text):
    """(min, max) of a domain like '0-127' or '0, 1'; None when it is free text that cannot be compared."""
    head = re.split(r"\s*;", text, maxsplit=1)[0].strip().rstrip(",").strip()
    span = re.fullmatch(r"(\d+)\s*[–-]\s*(\d+)", head)
    if span:
        return int(span.group(1)), int(span.group(2))
    if re.fullmatch(r"\d+(\s*,\s*\d+)*", head):
        listed = [int(value) for value in re.split(r"\s*,\s*", head)]
        return min(listed), max(listed)
    return None


def spec_domains(row):
    """The domain text of each data byte a row describes, or None when the row is not made of plain data
    bytes (a string, a variable-length tail): what is described is not comparable."""
    first, second = row["d1"].strip(), row["d2"].strip()
    if "…" in first or "…" in second or "char" in first:
        return None
    if first == "N/A":
        return []
    domains = [first]
    # "second" is compared against "N/A" by its own leading segment, not the whole text: a column like
    # "N/A ; {21–127}" (§0E &22/&42, Original Pitch) is a clarifying note attached to an otherwise-N/A
    # second column, not a real second data byte, the same way a bare "N/A" already is not one.
    second_head = re.split(r"\s*;", second, maxsplit=1)[0].strip()
    if second_head != "N/A":
        # "second" itself is a domain only when it carries content of its own before any embedded
        # <DataN> reference (e.g. a Set row's "0, 1 ; <Data3>(MSB) ; <Data4>(LSB)", where the leading
        # "0, 1" is the sign field). When it starts with a marker (a REPLY row's own "<Data2>(MSB) ;
        # <Data3>(LSB)", the sign already given its own column in "first"), that leading content does
        # not exist and "second" would otherwise be counted as a spurious extra domain.
        if not second.startswith("<Data"):
            domains.append(second)
        # "=" is not always there before the range (e.g. "<Data3>0-100", &23's own column, no "=").
        for marker in re.finditer(r"<Data(\d+)>=?\s*([^;]*)", second):
            domains.append(marker.group(2))
    return domains


def compare_values(owner, label, values, row):
    """Problems and notes of a record's values against the spec row that describes them."""
    domains = spec_domains(row)
    if domains is None:
        return [], [f"{owner}: {label} not compared (the spec row is variable-length)"]
    problems, notes = [], []
    if len(domains) != len(values):
        problems.append(f"{owner}: {label} has {len(values)} values, the spec row describes {len(domains)}")
        return problems, notes
    for index, (value, domain) in enumerate(zip(values, domains)):
        parsed = parse_domain(domain)
        if parsed is None:
            notes.append(f"{owner}: {label}[{index}] range not compared (free text: {domain.strip()!r})")
        elif parsed != (value["min"], value["max"]):
            problems.append(f"{owner}: {label}[{index}] range {value['min']}..{value['max']} differs from the "
                            f"spec's {parsed[0]}..{parsed[1]}")
    return problems, notes


# (section, item) pairs where the spec's own decimal column disagrees with its hex one — a documented
# transcription slip in the PDF itself (documents/_index/sysex_spec.kb.md, "Spec errata /
# inconsistencies"), not a mistake in this catalogue: noted, not flagged as a problem.
KNOWN_DEC_ERRATA = {("08", "6C")}  # &6C listed as decimal 107 (= &6B's own), should be 108 (T11/T12)


def coverage(catalogue, spec):
    """Compares the data file with the spec rows; returns (report lines, problems)."""
    report, problems = [], []
    by_position = {(entry["section"], entry["item"]): entry for entry in catalogue["items"]}
    unaccounted = []

    for section in catalogue["sections"]:
        code = section["section"]
        commands = sorted(key for key in spec if key[0] == "C" and key[1] == code)
        covered = [key for key in commands if (code, key[2]) in by_position]
        state = "complete" if section["complete"] else "partial"
        line = f"section {code}: {len(covered)} of {len(commands)} spec rows covered ({section['name']}, {state})"
        if not section["complete"]:
            line += f"; {len(commands) - len(covered)} not covered, as declared"
        report.append(line)
        if section["complete"]:
            unaccounted += [f"{code} &{key[2]} ({spec[key]['desc'][:60].strip()})" for key in commands
                            if key not in covered]

    for entry in catalogue["items"]:
        owner, section, item = entry["id"], entry["section"], entry["item"]
        command = spec.get(("C", section, item))
        if command is None:
            problems.append(f"{owner}: no row for {section} &{item} in the spec table")
            continue
        record_problems, notes = [], []
        if int(command["dec"]) != int(item, 16):
            message = f"{owner}: the spec gives item {command['dec']} decimal, not {int(item, 16)}"
            if (section, item) in KNOWN_DEC_ERRATA:
                notes.append(f"{message} (known spec erratum, not a catalogue problem)")
            else:
                record_problems.append(message)
        found, more_notes = compare_values(owner, "args", entry["args"], command)
        notes += more_notes
        record_problems += found
        if entry["kind"] == "get":
            reply = spec.get(("R", section, item))
            if reply is None:
                notes.append(f"{owner}: reply format not compared (the spec lists no separate reply row)")
            else:
                found, more = compare_values(owner, "reply", entry["reply"], reply)
                record_problems += found
                notes += more
        problems += record_problems
        report.append(f"  {owner} ({section} &{item}): {'differs from the spec' if record_problems else 'covered'}")
        report += [f"    note: {note}" for note in notes]

    report.append("unaccounted: " + ("none" if not unaccounted else "; ".join(unaccounted)))
    problems += [f"unaccounted spec row: {row}" for row in unaccounted]
    return report, problems


# --- command line -----------------------------------------------------------------------------------


def main():
    for stream in (sys.stdout, sys.stderr):
        stream.reconfigure(errors="replace")  # spec descriptions carry typographic quotes and dashes
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--data", type=pathlib.Path, default=DEFAULT_DATA, help="the item data file")
    parser.add_argument("--output", type=pathlib.Path, default=DEFAULT_OUTPUT, help="the generated table")
    parser.add_argument("--tsv", type=pathlib.Path, default=DEFAULT_TSV, help="the spec's item list")
    parser.add_argument("--check", action="store_true", help="fail if the table is out of date")
    parser.add_argument("--coverage", action="store_true", help="compare the data file with the spec's item list")
    args = parser.parse_args()

    try:
        catalogue = load_catalogue(args.data)
        problems = validate(catalogue)
        if problems:
            raise CatalogueError(problems)
        text = render(catalogue)
        spec = read_spec_rows(args.tsv) if args.coverage else None
    except CatalogueError as error:
        print(f"{args.data} is not valid:", file=sys.stderr)
        for problem in error.problems:
            print(f"  {problem}", file=sys.stderr)
        return EXIT_INVALID
    except (OSError, StopIteration) as error:
        print(f"cannot read an input: {error}", file=sys.stderr)
        return EXIT_INVALID

    status = EXIT_OK
    if args.check:
        status = max(status, check_table(text, args.output))
    if args.coverage:
        report, found = coverage(catalogue, spec)
        print("\n".join(report))
        for problem in found:
            print(f"problem: {problem}", file=sys.stderr)
        status = max(status, EXIT_MISMATCH if found else EXIT_OK)
    if not args.check and not args.coverage:
        args.output.write_text(text, encoding="utf-8", newline="\n")
        print(f"Wrote {len(catalogue['items'])} items to {args.output}")
    return status


if __name__ == "__main__":
    sys.exit(main())
