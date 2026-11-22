"""Tests of juce/tools/trace_header_ids.py, run by ctest when Python 3 is available.

Each test reads as a Gherkin scenario of RQ-QLT-014 or RQ-QLT-015. Run as a script, the file exits 77 (skipped for ctest) when
tree-sitter is not installed (RQ-QLT-016).
[TASK-QLT-028, RQ-QLT-014, RQ-QLT-015, RQ-QLT-016, ADR-QLT-003 (DEC-QLT-015, DEC-QLT-016, DEC-QLT-017)]
"""
import contextlib
import importlib.util
import io
import pathlib
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "juce" / "tools" / "trace_header_ids.py"
SKIP_EXIT_CODE = 77

_SPEC = importlib.util.spec_from_file_location("trace_header_ids", SCRIPT)
trace = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(trace)

HEADER = "include/ns/F.hpp"
SOURCE = "src/F.cpp"


class Project:
    """A temporary folder holding a header and a source, run through the tool. [RQ-QLT-014]"""

    def __init__(self, header, source, newline="\n"):
        self._dir = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self._dir.name)
        self.write(HEADER, header, newline)
        self.write(SOURCE, source, newline)

    def write(self, rel, text, newline="\n"):
        path = self.root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(text.replace("\n", newline).encode("utf-8"))

    def read(self, rel=SOURCE):
        return (self.root / rel).read_bytes().decode("utf-8")

    def run(self, *options):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = trace.main([*options, "--headers", str(self.root), str(self.root / "src")])
        return code, out.getvalue()

    def close(self):
        self._dir.cleanup()


@unittest.skipUnless(trace.PARSER_AVAILABLE, "tree-sitter is not installed")
class CopyFromHeader(unittest.TestCase):
    """RQ-QLT-014: the IDs of a declaration are copied onto its definition."""

    def project(self, header, source, newline="\n"):
        p = Project(header, source, newline)
        self.addCleanup(p.close)
        return p

    def test_given_an_uncommented_definition_when_the_tool_runs_then_the_ids_are_above_it(self):
        # Given a header declaring f(int) with IDs and a definition with no comment
        p = self.project("namespace ns\n{\n    /// Does f. [RQ-ABC-001, DEC-ABC-002]\n    void f(int value);\n}\n",
                         "#include \"ns/F.hpp\"\n\nvoid ns::f(int v)\n{\n}\n")
        # When the tool runs
        code, _ = p.run()
        # Then the line with both IDs is just above the definition
        self.assertEqual(0, code)
        self.assertEqual("#include \"ns/F.hpp\"\n\n// [RQ-ABC-001, DEC-ABC-002]\nvoid ns::f(int v)\n{\n}\n", p.read())

    def test_given_a_definition_citing_one_id_when_the_tool_runs_then_only_the_other_is_added_below_its_comment(self):
        # Given a definition whose comment already cites RQ-ABC-001
        p = self.project("namespace ns\n{\n    /// Does f. [RQ-ABC-001, DEC-ABC-002]\n    void f(int value);\n}\n",
                         "namespace ns\n{\n    // Does f. [RQ-ABC-001]\n    void f(int value)\n    {\n    }\n}\n")
        # When the tool runs
        p.run()
        # Then only DEC-ABC-002 is added, below the comment, at the definition's indentation
        self.assertEqual("namespace ns\n{\n    // Does f. [RQ-ABC-001]\n    // [DEC-ABC-002]\n    void f(int value)\n    {\n    }\n}\n",
                         p.read())

    def test_given_a_traced_file_when_the_tool_runs_again_then_nothing_changes(self):
        # Given a file the tool already ran on
        p = self.project("namespace ns\n{\n    /// [RQ-ABC-001]\n    void f(int value);\n}\n", "void ns::f(int v)\n{\n}\n")
        p.run()
        first = p.read()
        # When it runs again
        code, out = p.run()
        # Then the file is unchanged and the definition is counted as traced
        self.assertEqual(0, code)
        self.assertEqual(first, p.read())
        self.assertIn("copied 0, traced 1", out)

    def test_given_a_method_of_a_class_when_the_tool_runs_then_its_qualified_definition_gets_the_ids(self):
        # Given a const method, a constructor and a destructor of a class
        p = self.project("namespace ns\n{\n    class C\n    {\n    public:\n        /// [RQ-ABC-003]\n        C();\n"
                         "        /// [RQ-ABC-004]\n        ~C();\n        /// [RQ-ABC-005]\n        int m() const;\n    };\n}\n",
                         "namespace ns\n{\n    C::C() = default;\n\n    C::~C()\n    {\n    }\n\n    int C::m() const\n    {\n"
                         "        return 0;\n    }\n}\n")
        # When the tool runs
        p.run()
        # Then each definition gets the IDs of its own declaration
        text = p.read()
        self.assertIn("    // [RQ-ABC-004]\n    C::~C()", text)
        self.assertIn("    // [RQ-ABC-005]\n    int C::m() const", text)

    def test_given_crlf_line_endings_when_the_tool_runs_then_they_are_kept(self):
        # Given a source written with CRLF line endings
        p = self.project("namespace ns\n{\n    /// [RQ-ABC-001]\n    void f(int value);\n}\n", "void ns::f(int v)\n{\n}\n", "\r\n")
        # When the tool runs
        p.run()
        # Then the added line ends with CRLF too, and no line ends with LF alone
        text = p.read()
        self.assertIn("// [RQ-ABC-001]\r\nvoid ns::f(int v)\r\n", text)
        self.assertNotIn("\n", text.replace("\r\n", ""))

    def test_given_a_type_qualified_differently_when_the_name_is_unique_then_the_ids_are_copied(self):
        # Given a header naming Session and a source naming ns::Session, with one declaration and one definition of g
        p = self.project("namespace ns\n{\n    class Session;\n    /// [RQ-ABC-006]\n    void g(Session& session);\n}\n",
                         "void ns::g(ns::Session& session)\n{\n}\n")
        # When the tool runs
        p.run()
        # Then the definition gets the IDs
        self.assertIn("// [RQ-ABC-006]\nvoid ns::g", p.read())

    def test_given_a_helper_of_an_anonymous_namespace_when_the_tool_runs_then_it_is_left_alone(self):
        # Given a helper of an anonymous namespace with the same name as a declared function
        p = self.project("namespace ns\n{\n    /// [RQ-ABC-001]\n    void f(int value);\n}\n",
                         "namespace ns\n{\n    namespace\n    {\n        void f(int v)\n        {\n        }\n    }\n}\n")
        before = p.read()
        # When the tool runs
        p.run()
        # Then the file is unchanged
        self.assertEqual(before, p.read())


@unittest.skipUnless(trace.PARSER_AVAILABLE, "tree-sitter is not installed")
class Untraceable(unittest.TestCase):
    """RQ-QLT-015: what cannot be paired is reported and left alone; --check writes nothing."""

    def project(self, header, source):
        p = Project(header, source)
        self.addCleanup(p.close)
        return p

    def test_given_two_overloads_when_the_tool_runs_then_each_definition_gets_its_own_ids(self):
        # Given f(int) and f(double), each declared with its own IDs
        p = self.project("namespace ns\n{\n    /// [RQ-ABC-001]\n    void f(int value);\n    /// [RQ-ABC-002]\n    void f(double value);\n}\n",
                         "void ns::f(double v)\n{\n}\n\nvoid ns::f(const int v)\n{\n}\n")
        # When the tool runs
        p.run()
        # Then each definition carries the IDs of its own declaration
        self.assertEqual("// [RQ-ABC-002]\nvoid ns::f(double v)\n{\n}\n\n// [RQ-ABC-001]\nvoid ns::f(const int v)\n{\n}\n", p.read())

    def test_given_parameters_matching_no_declaration_when_the_tool_runs_then_it_is_unmatched_and_unchanged(self):
        # Given two declared overloads and a definition of a third
        p = self.project("namespace ns\n{\n    /// [RQ-ABC-001]\n    void f(int value);\n    /// [RQ-ABC-002]\n    void f(double value);\n}\n",
                         "void ns::f(char v)\n{\n}\n")
        before = p.read()
        # When the tool runs
        code, out = p.run("--verbose")
        # Then the file is unchanged and the declarations are reported unmatched
        self.assertEqual(0, code)
        self.assertEqual(before, p.read())
        self.assertIn("unmatched 2", out)
        self.assertIn("F.hpp:4", out)

    def test_given_two_definitions_of_one_declaration_when_the_tool_runs_then_both_are_ambiguous_and_unchanged(self):
        # Given one declaration and two sources defining it
        p = self.project("namespace ns\n{\n    /// [RQ-ABC-001]\n    void f(int value);\n}\n", "void ns::f(int v)\n{\n}\n")
        p.write("src/G.cpp", "void ns::f(int v)\n{\n}\n")
        # When the tool runs
        code, out = p.run()
        # Then neither is changed and the case is reported
        self.assertEqual(0, code)
        self.assertNotIn("RQ-ABC-001", p.read())
        self.assertNotIn("RQ-ABC-001", p.read("src/G.cpp"))
        self.assertIn("ambiguous 1", out)

    def test_given_check_and_a_missing_id_when_the_tool_runs_then_it_exits_1_and_writes_nothing(self):
        # Given --check and a definition missing an ID of its declaration
        p = self.project("namespace ns\n{\n    /// [RQ-ABC-001]\n    void f(int value);\n}\n", "void ns::f(int v)\n{\n}\n")
        before = p.read()
        # When the tool runs
        code, out = p.run("--check")
        # Then it exits 1, lists the definition, and the file is unchanged
        self.assertEqual(1, code)
        self.assertIn("F.cpp:1", out)
        self.assertEqual(before, p.read())


if __name__ == "__main__":
    if not trace.PARSER_AVAILABLE:
        print("tree-sitter is not installed: skipped (pip install -r the performance center's requirements-callgraph.txt)")
        sys.exit(SKIP_EXIT_CODE)
    unittest.main()
