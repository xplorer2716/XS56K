"""Copies the AGNOS IDs of a header's function declaration onto its definition in a .cpp file.

The AGNOS performance center counts a function only where it has a body, from the IDs of its lines and of the comment block just above
it; this project writes them on the declaration, in the header. The tool pairs each definition of the .cpp files under the given paths
with the declaration of a header of `--headers` (default `juce/`) by qualified name and parameter types, or by qualified name alone when
that name has one declaration and one definition, and adds the IDs it does not cite as one `// [<ID>, ...]` line just above it.

    python juce/tools/trace_header_ids.py [--check] [--verbose] [--headers DIR] [--exclude DIR]... PATH...

Exit status: 0 done (or nothing missing with --check), 1 IDs missing with --check (nothing written), 2 tree-sitter not installed.
[TASK-QLT-028, RQ-QLT-014, RQ-QLT-015, ADR-QLT-003 (DEC-QLT-015, DEC-QLT-016, DEC-QLT-017)]
"""
import argparse
import collections
import pathlib
import re
import sys

try:
    import tree_sitter
    import tree_sitter_cpp

    PARSER_AVAILABLE = True
except ImportError:
    PARSER_AVAILABLE = False

ROOT = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_HEADERS = ROOT / "juce"
HEADER_SUFFIXES = (".hpp", ".h")
SOURCE_SUFFIX = ".cpp"
EXCLUDED_DIRS = ("JUCE", "_deps", ".git", "node_modules", "__pycache__")
EXCLUDED_DIR_PREFIX = "build"
ID_PATTERN = re.compile(r"\b(?:FTR|RQ|ADR|DEC|PLAN|TASK)-[A-Z]{3}-\d{3}\b")
SCOPE_SEPARATOR = "::"
EXIT_DONE, EXIT_MISSING, EXIT_NO_PARSER = 0, 1, 2
CRLF, LF = "\r\n", "\n"
COMMENT = "comment"
DECLARATIONS = ("declaration", "field_declaration")
DEFINITION = "function_definition"
TEMPLATE = "template_declaration"
FUNCTION_DECLARATOR = "function_declarator"
WRAPPING_DECLARATORS = ("pointer_declarator", "reference_declarator", "attributed_declarator")
NAMESPACE = "namespace_definition"
CLASSES = ("class_specifier", "struct_specifier", "union_specifier")
FRIEND = "friend_declaration"
PARAMETER_DEFAULT = "default_value"
REFERENCE_MARKS = ("&", "*")
CONST = "const"
ANONYMOUS = None


def ids_in(text):
    """The AGNOS IDs of `text`, in order, each once. [RQ-QLT-014]"""
    return list(dict.fromkeys(ID_PATTERN.findall(text)))


def node_text(node):
    """The source text of a tree-sitter node. [RQ-QLT-014]"""
    return node.text.decode("utf-8", "replace")


def function_declarator(node):
    """The function declarator under a declaration's or a definition's declarator, else None. [DEC-QLT-016]"""
    declarator = node.child_by_field_name("declarator")
    while declarator is not None and declarator.type in WRAPPING_DECLARATORS:
        nested = declarator.child_by_field_name("declarator")
        declarator = nested if nested is not None else next((c for c in declarator.named_children if c.type != "type_qualifier"), None)
    return declarator if declarator is not None and declarator.type == FUNCTION_DECLARATOR else None


def scope_of(node, skip_classes=False):
    """The names of the namespaces and classes enclosing `node`, outermost first, or None inside an anonymous namespace. [DEC-QLT-016]"""
    names = []
    parent = node.parent
    while parent is not None:
        name = parent.child_by_field_name("name")
        if parent.type == NAMESPACE:
            if name is None:
                return ANONYMOUS
            names.append(re.sub(r"\s+", "", node_text(name)))
        elif parent.type in CLASSES and name is not None and not skip_classes:
            names.append(node_text(name))
        parent = parent.parent
    return list(reversed(names))


def innermost_identifier(node):
    """The deepest name a parameter's declarator declares, else None. [DEC-QLT-016]"""
    if node is None:
        return None
    if node.type == "identifier":
        return node
    for child in reversed(node.named_children):
        found = innermost_identifier(child)
        if found is not None:
            return found
    return None


def parameter_type(parameter):
    """A parameter's type with its name, its default value, its spaces and a by-value `const` removed. [DEC-QLT-016]"""
    start = parameter.start_byte
    default = parameter.child_by_field_name(PARAMETER_DEFAULT)
    source = parameter.text
    if default is not None:
        source = source[: default.start_byte - start]
        source = source.rstrip().rstrip(b"=")
    name = innermost_identifier(parameter.child_by_field_name("declarator"))
    if name is not None and name.end_byte <= start + len(source):
        source = source[: name.start_byte - start] + source[name.end_byte - start:]
    text = source.decode("utf-8", "replace")
    if not any(mark in text for mark in REFERENCE_MARKS):
        text = re.sub(r"\bconst\b", "", text)
    return re.sub(r"\s+", "", text)


def signature_of(declarator):
    """The parameter types and the trailing qualifiers of a function declarator, as one string. [DEC-QLT-016]"""
    parameters = declarator.child_by_field_name("parameters")
    types = [parameter_type(p) for p in parameters.named_children if p.type != COMMENT] if parameters is not None else []
    qualifiers = [node_text(c) for c in declarator.children if c.type == "type_qualifier"]
    return ",".join(types) + "|" + " ".join(qualifiers)


def outer_node(node):
    """The node a comment block sits above: the template declaration wrapping `node`, else `node`. [DEC-QLT-017]"""
    return node.parent if node.parent is not None and node.parent.type == TEMPLATE else node


def comment_above(node):
    """The text of the comment block ending on the line just above `node`. [RQ-QLT-014]"""
    texts = []
    row = node.start_point[0]
    sibling = node.prev_sibling
    while sibling is not None and sibling.type == COMMENT and sibling.end_point[0] == row - 1:
        texts.append(node_text(sibling))
        row = sibling.start_point[0]
        sibling = sibling.prev_sibling
    return "\n".join(reversed(texts))


def walk(node):
    """Every node under `node`, depth first. [RQ-QLT-014]"""
    stack = [node]
    while stack:
        current = stack.pop()
        yield current
        stack.extend(reversed(current.children))


Declaration = collections.namedtuple("Declaration", "path line qualified key ids")
Definition = collections.namedtuple("Definition", "path line row indent qualified key cited")


class Parser:
    """Parses C++ files into declarations with IDs and definitions. [DEC-QLT-015]"""

    def __init__(self):
        self._parser = tree_sitter.Parser(tree_sitter.Language(tree_sitter_cpp.language()))

    def tree(self, path):
        """The syntax tree of a file. [DEC-QLT-015]"""
        return self._parser.parse(path.read_bytes())

    def declarations(self, path):
        """The function declarations of a header whose comment cites IDs. [RQ-QLT-014]"""
        found = []
        for node in walk(self.tree(path).root_node):
            if node.type not in DECLARATIONS or node.child_by_field_name(PARAMETER_DEFAULT) is not None:
                continue
            declarator = function_declarator(node)
            if declarator is None:
                continue
            ids = ids_in(comment_above(outer_node(node)))
            scope = scope_of(node, skip_classes=node.parent is not None and node.parent.type == FRIEND)
            if not ids or scope is ANONYMOUS:
                continue
            name = re.sub(r"\s+", "", node_text(declarator.child_by_field_name("declarator")))
            qualified = SCOPE_SEPARATOR.join([*scope, name])
            found.append(Declaration(path, node.start_point[0] + 1, qualified, (qualified, signature_of(declarator)), ids))
        return found

    def definitions(self, path):
        """The function definitions of a source outside any anonymous namespace. [RQ-QLT-014]"""
        lines = path.read_bytes().decode("utf-8", "replace").splitlines()
        found = []
        for node in walk(self.tree(path).root_node):
            if node.type != DEFINITION:
                continue
            declarator = function_declarator(node)
            scope = scope_of(node)
            if declarator is None or scope is ANONYMOUS:
                continue
            name = re.sub(r"\s+", "", node_text(declarator.child_by_field_name("declarator"))).removeprefix(SCOPE_SEPARATOR)
            qualified = SCOPE_SEPARATOR.join([*scope, name])
            outer = outer_node(node)
            row = outer.start_point[0]
            indent = re.match(r"\s*", lines[row]).group(0)
            cited = set(ids_in(comment_above(outer) + "\n" + node_text(outer)))
            found.append(Definition(path, row + 1, row, indent, qualified, (qualified, signature_of(declarator)), cited))
        return found


def files_under(paths, suffixes, excluded):
    """The files with one of `suffixes` under `paths`, outside the excluded and build folders. [RQ-QLT-014]"""
    result = []
    for base in paths:
        for path in sorted(base.rglob("*")):
            parts = path.relative_to(base).parts[:-1]
            if path.suffix not in suffixes or not path.is_file():
                continue
            if any(part in EXCLUDED_DIRS or part.startswith(EXCLUDED_DIR_PREFIX) for part in parts):
                continue
            if any(path.resolve().is_relative_to(e) for e in excluded):
                continue
            result.append(path)
    return result


def group(items, field):
    """`items` grouped by one of their fields. [DEC-QLT-016]"""
    grouped = collections.defaultdict(list)
    for item in items:
        grouped[getattr(item, field)].append(item)
    return grouped


def merged_ids(declarations):
    """The IDs of declarations of one function if they all cite the same, else None (ambiguous). [RQ-QLT-015]"""
    sets = {tuple(d.ids) for d in declarations}
    return list(sets.pop()) if len(sets) == 1 else None


def pair(declarations, definitions):
    """([(definition, ids)], unmatched declarations, ambiguous count): by key, then by qualified name for what is left. [DEC-QLT-016]"""
    pairs, ambiguous, used = [], 0, set()
    by_key, definitions_by_key = group(declarations, "key"), group(definitions, "key")
    for key, decls in by_key.items():
        defs = definitions_by_key.get(key, [])
        if not defs:
            continue
        ids = merged_ids(decls)
        used.update(id(d) for d in decls)
        if ids is None or len(defs) > 1:
            ambiguous += 1
            continue
        pairs.append((defs[0], ids))
    paired = {id(d) for d, _ in pairs} | {id(d) for k in by_key for d in definitions_by_key.get(k, [])}
    left_decls = [d for d in declarations if id(d) not in used]
    left_defs = group([d for d in definitions if id(d) not in paired], "qualified")
    unmatched = []
    for qualified, decls in group(left_decls, "qualified").items():
        defs = left_defs.get(qualified, [])
        if len({d.key for d in decls}) == 1 and len(defs) == 1 and merged_ids(decls) is not None:
            pairs.append((defs[0], merged_ids(decls)))
        elif defs and len({d.key for d in decls}) == 1:
            ambiguous += 1
        else:
            unmatched.extend(decls)
    return pairs, unmatched, ambiguous


def insert_lines(path, additions):
    """Adds each `(row, indent, ids)` line above its row, keeping the file's line endings. [DEC-QLT-017]"""
    text = path.read_bytes().decode("utf-8")
    newline = CRLF if CRLF in text else LF
    lines = text.split(newline)
    for row, indent, ids in sorted(additions, reverse=True):
        lines.insert(row, f"{indent}// [{', '.join(ids)}]")
    path.write_bytes(newline.join(lines).encode("utf-8"))


def main(argv=None):
    """Runs the tool; returns its exit status. [RQ-QLT-014, RQ-QLT-015]"""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="+", type=pathlib.Path, help="folders or files whose .cpp definitions are traced")
    parser.add_argument("--headers", type=pathlib.Path, default=DEFAULT_HEADERS, help="folder whose headers are read (default juce/)")
    parser.add_argument("--exclude", type=pathlib.Path, action="append", default=[], help="folder left out of PATH (repeatable)")
    parser.add_argument("--check", action="store_true", help="write nothing; exit 1 when a definition lacks IDs")
    parser.add_argument("--verbose", action="store_true", help="also list the declarations matching no definition")
    options = parser.parse_args(argv)
    if not PARSER_AVAILABLE:
        print("trace_header_ids: tree-sitter is not installed (pip install tree-sitter tree-sitter-cpp)", file=sys.stderr)
        return EXIT_NO_PARSER
    excluded = [e.resolve() for e in options.exclude]
    cpp = Parser()
    declarations = [d for h in files_under([options.headers], HEADER_SUFFIXES, excluded=[]) for d in cpp.declarations(h)]
    sources = [p for p in options.paths if p.is_file()] + files_under([p for p in options.paths if p.is_dir()], (SOURCE_SUFFIX,), excluded)
    definitions = [d for s in sources for d in cpp.definitions(s)]
    pairs, unmatched, ambiguous = pair(declarations, definitions)
    additions, traced = collections.defaultdict(list), 0
    for definition, ids in pairs:
        missing = [i for i in ids if i not in definition.cited]
        if not missing:
            traced += 1
            continue
        additions[definition.path].append((definition.row, definition.indent, missing))
        if options.check:
            print(f"{definition.path}:{definition.line}: {definition.qualified} lacks {', '.join(missing)}")
    if options.verbose:
        for d in unmatched:
            print(f"{d.path}:{d.line}: {d.qualified} matches no definition")
    copied = sum(len(a) for a in additions.values())
    if options.check:
        print(f"trace_header_ids: missing {copied}, traced {traced}, unmatched {len(unmatched)}, ambiguous {ambiguous}")
        return EXIT_MISSING if copied else EXIT_DONE
    for path, lines in additions.items():
        insert_lines(path, lines)
    print(f"trace_header_ids: copied {copied}, traced {traced}, unmatched {len(unmatched)}, ambiguous {ambiguous}")
    return EXIT_DONE


if __name__ == "__main__":
    sys.exit(main())
