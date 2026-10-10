"""Tests of juce/tools/generate_sysex_coverage.py, run by ctest when Python 3 is available.

Each test reads as a Gherkin scenario. [TASK-MCP-069, TASK-MCP-070, TASK-MCP-071, RQ-MCP-058, RQ-MCP-059, RQ-MCP-060,
RQ-MCP-062, RQ-MCP-063, RQ-MCP-065, ADR-MCP-006 (DEC-MCP-035, DEC-MCP-036, DEC-MCP-037, DEC-MCP-038, DEC-MCP-039)]
"""
import importlib.util
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
SCRIPT = ROOT / "juce" / "tools" / "generate_sysex_coverage.py"
DATA = ROOT / "juce" / "akm" / "data" / "items.json"
TSV = ROOT / "documents" / "_index" / "sysex_spec.items.tsv"
DECLARED = ROOT / "juce" / "tools" / "sysex_coverage_declared.json"

# The 13 sections of the spec's Table 4, in its order.
TABLE_4_ORDER = ["00", "02", "04", "06", "08", "0A", "0C", "0E", "10", "12", "14", "16", "20"]
TABLE_32_SECTIONS = ["2A", "2C", "2E", "32", "38", "3A", "3C", "3E"]
ROW_PREFIX = "| `&"


def load_script():
    spec = importlib.util.spec_from_file_location("generate_sysex_coverage", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def run_script(*arguments):
    # PYTHONUTF8: see test_generate_akm_items.py (a child on Windows writes its console codepage otherwise).
    env = dict(os.environ, PYTHONUTF8="1")
    return subprocess.run([sys.executable, str(SCRIPT), *arguments], capture_output=True, text=True,
                          encoding="utf-8", env=env)


class ScriptTest(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory()
        self.addCleanup(self._directory.cleanup)
        self.directory = pathlib.Path(self._directory.name)
        self.output = self.directory / "SYSEX-COVERAGE.md"

    def generate(self, *extra):
        return run_script("--output", str(self.output), *extra)

    def write_variant(self, change):
        """Writes a copy of the data file after `change(catalogue)` and returns its path."""
        catalogue = json.loads(DATA.read_text(encoding="utf-8"))
        change(catalogue)
        path = self.directory / "items.json"
        path.write_text(json.dumps(catalogue, indent=2), encoding="utf-8")
        return path

    def akm_sources(self, source=None, header="namespace akm { void echoIt(Session& session); }"):
        """The arguments that point the script at small trees: AKM sources (empty, or with one source file), an MCP server with no tool,
        and a declared file with Table 32 only, so that nothing of the real MCP side meets the fixture's AKM side."""
        folder = self.directory / "akm"
        (folder / "src").mkdir(parents=True, exist_ok=True)
        (folder / "include").mkdir(exist_ok=True)
        if source is not None:
            (folder / "src" / "Fixture.cpp").write_text(source, encoding="utf-8")
            (folder / "include" / "Fixture.hpp").write_text(header, encoding="utf-8")
        mcp = self.directory / "mcp"
        (mcp / "src").mkdir(parents=True, exist_ok=True)
        (mcp / "include" / "mcp").mkdir(parents=True, exist_ok=True)
        (mcp / "README.md").write_text("# Server\n\n## 4. The tools\n\n## 5. Safety\n", encoding="utf-8")
        (mcp / "include" / "mcp" / "SamplerGateway.hpp").write_text("namespace mcp { class SamplerGateway { public: void close(); }; }", encoding="utf-8")
        (mcp / "src" / "Tools.cpp").write_text("namespace mcp {}", encoding="utf-8")
        (mcp / "src" / "StandardParameters.cpp").write_text("namespace mcp {}", encoding="utf-8")
        declared = json.loads(DECLARED.read_text(encoding="utf-8"))
        declared["mcp_tools"] = {}
        declared_path = self.directory / "declared-fixture.json"
        declared_path.write_text(json.dumps(declared), encoding="utf-8")
        return ["--akm-src", str(folder / "src"), "--akm-include", str(folder / "include"), "--mcp", str(mcp), "--declared", str(declared_path)]

    def sections_of(self, text):
        """The section codes of the `## §XX` headings, in the order the file has them."""
        return [line[4:6] for line in text.splitlines() if line.startswith("## §")]


class RowsOfTheSections(ScriptTest):
    def test_given_the_catalogue_when_generated_then_it_has_one_table_per_section_in_the_order_of_table_4(self):
        result = self.generate()

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.sections_of(self.output.read_text(encoding="utf-8")), TABLE_4_ORDER)

    def test_given_the_catalogue_when_generated_then_every_item_is_a_row_of_its_section(self):
        items = json.loads(DATA.read_text(encoding="utf-8"))["items"]

        self.generate()
        text = self.output.read_text(encoding="utf-8")

        self.assertEqual(sum(1 for line in text.splitlines() if line.startswith(ROW_PREFIX)),
                         len(items) + len(TABLE_32_SECTIONS))
        for item in items:
            self.assertIn(f"`&{item['item']}{{{int(item['item'], 16)}}}` {item['name']}", text, item["id"])

    def test_given_a_set_and_a_get_when_generated_then_each_row_says_which_it_is(self):
        self.generate()
        text = self.output.read_text(encoding="utf-8")

        self.assertIn("`&00{0}` Query (Set)", text)
        self.assertIn("`&06{6}` Echo (Get)", text)

    def test_given_no_function_that_sends_it_when_generated_then_a_catalogued_message_is_built_by_make_request_and_has_no_tool(self):
        self.generate(*self.akm_sources())
        text = self.output.read_text(encoding="utf-8")

        self.assertIn("| `&06{6}` Echo (Get) | makeRequest | none |", text)

    def test_given_the_eight_sections_of_table_32_when_generated_then_they_are_rows_with_nothing_behind_them(self):
        self.generate()
        text = self.output.read_text(encoding="utf-8")

        table_32 = text[text.index("## Table 32"):]
        for section in TABLE_32_SECTIONS:
            row = next(line for line in table_32.splitlines() if line.startswith(f"{ROW_PREFIX}{section}{{"))
            self.assertTrue(row.endswith("| none | none |"), row)


    def test_given_item_codes_that_repeat_a_section_code_when_generated_then_table_32_heads_its_column_section_not_message(self):
        self.generate()
        text = self.output.read_text(encoding="utf-8")

        table_32 = text[text.index("## Table 32"):]
        self.assertIn("| Section | AKM function | MCP tool |", table_32)
        self.assertNotIn("| Message |", table_32)
        # `&2A` is an item of section 06 as well as a section of Table 32: the first table says so.
        self.assertIn("`&2A{42}` Get Zone Velocity->Start (Get)", text[:text.index("## Table 32")])


class TheCellsOfTheTwoColumns(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.script = load_script()

    def test_given_a_message_sent_by_two_functions_when_the_cell_is_made_then_it_lists_both(self):
        self.assertEqual(self.script.akm_cell(["getAllProgramNames", "getProgramNames"], {"args": []}),
                         "getAllProgramNames, getProgramNames")

    def test_given_a_catalogued_message_with_numeric_arguments_no_function_sends_when_the_cell_is_made_then_it_is_make_request(self):
        item = {"args": [{"format": "byte"}, {"format": "signed_word"}]}

        self.assertEqual(self.script.akm_cell([], item), "makeRequest")

    def test_given_a_message_with_no_argument_no_function_sends_when_the_cell_is_made_then_it_is_make_request(self):
        self.assertEqual(self.script.akm_cell([], {"args": []}), "makeRequest")

    def test_given_a_message_with_one_string_argument_no_function_sends_when_the_cell_is_made_then_it_is_make_string_request(self):
        self.assertEqual(self.script.akm_cell([], {"args": [{"format": "string"}]}), "makeStringRequest")

    def test_given_a_message_with_two_string_arguments_no_function_sends_when_the_cell_is_made_then_it_is_make_two_string_request(self):
        item = {"args": [{"format": "string"}, {"format": "string"}]}

        self.assertEqual(self.script.akm_cell([], item), "makeTwoStringRequest")

    def test_given_a_function_that_sends_the_message_when_the_cell_is_made_then_the_builder_is_not_named(self):
        self.assertEqual(self.script.akm_cell(["getSamplerName"], {"args": [{"format": "string"}]}), "getSamplerName")

    def test_given_a_tool_with_an_option_when_the_cell_is_made_then_the_option_follows_the_name(self):
        cell = self.script.mcp_cell([("set_parameter", None), ("load_file", "--allow-disk")], None)

        self.assertEqual(cell, "set_parameter, load_file (--allow-disk)")

    def test_given_a_message_no_tool_sends_when_the_cell_is_made_then_it_is_none(self):
        self.assertEqual(self.script.mcp_cell([], None), "none")

    def test_given_a_message_an_adr_excludes_when_the_cell_is_made_then_it_says_not_offered_and_the_decision(self):
        self.assertEqual(self.script.mcp_cell([], "DEC-MCP-028"), "not offered (DEC-MCP-028)")

    def test_given_a_name_with_a_pipe_when_the_row_is_made_then_the_pipe_is_escaped(self):
        row = self.script.message_cell({"item": "1A", "name": "a|b", "kind": "get"})

        self.assertEqual(row, "`&1A{26}` a\\|b (Get)")


class TheFileIsTheSameEveryTime(ScriptTest):
    def test_given_the_script_run_twice_when_the_files_are_compared_then_they_are_the_same_bytes(self):
        self.generate()
        first = self.output.read_bytes()
        self.generate()

        self.assertEqual(self.output.read_bytes(), first)

    def test_given_the_file_when_it_is_read_as_bytes_then_it_has_unix_line_ends_and_ends_with_one(self):
        self.generate()
        data = self.output.read_bytes()

        self.assertNotIn(b"\r", data)
        self.assertTrue(data.endswith(b"\n"))
        self.assertFalse(data.endswith(b"\n\n"))


class TheFileIsCheckedAgainstTheCode(ScriptTest):
    def test_given_the_file_just_written_when_checked_then_it_passes(self):
        self.generate()

        result = self.generate("--check")

        self.assertEqual(result.returncode, 0, result.stderr)

    def test_given_a_file_edited_by_hand_when_checked_then_it_fails_and_names_the_section(self):
        self.generate()
        text = self.output.read_text(encoding="utf-8")
        self.output.write_text(text.replace("`&06{6}` Echo (Get)", "`&06{6}` Echo (Set)"), encoding="utf-8", newline="\n")

        result = self.generate("--check")

        self.assertEqual(result.returncode, 1)
        self.assertIn("§00", result.stderr)

    def test_given_no_file_when_checked_then_it_fails_and_says_to_run_the_script(self):
        result = self.generate("--check")

        self.assertEqual(result.returncode, 1)
        self.assertIn("generate_sysex_coverage.py", result.stderr)


class TheCatalogueIsComparedWithTheSpec(ScriptTest):
    def test_given_an_item_removed_from_the_catalogue_when_run_then_it_fails_and_names_the_section_and_the_item(self):
        variant = self.write_variant(lambda catalogue: catalogue["items"].remove(
            next(item for item in catalogue["items"] if item["id"] == "SysExEcho")))

        result = self.generate("--data", str(variant))

        self.assertEqual(result.returncode, 1)
        self.assertIn("§00", result.stderr)
        self.assertIn("&06", result.stderr)
        self.assertFalse(self.output.exists())

    def test_given_an_item_the_spec_does_not_list_when_run_then_it_fails_and_names_it(self):
        def add(catalogue):
            extra = dict(next(item for item in catalogue["items"] if item["id"] == "SysExEcho"))
            extra.update(id="SysExNotInTheSpec", item="7E")
            catalogue["items"].append(extra)

        variant = self.write_variant(add)

        result = self.generate("--data", str(variant))

        self.assertEqual(result.returncode, 1)
        self.assertIn("&7E", result.stderr)

    def test_given_a_missing_input_when_run_then_it_exits_2_and_says_which(self):
        result = self.generate("--data", str(self.directory / "nothing.json"))

        self.assertEqual(result.returncode, 2)
        self.assertIn("nothing.json", result.stderr)


class TheDeclaredFile(ScriptTest):
    def test_given_the_declared_file_when_read_then_it_has_the_eight_sections_of_table_32(self):
        declared = json.loads(DECLARED.read_text(encoding="utf-8"))

        self.assertEqual([entry["section"] for entry in declared["table32"]["sections"]], TABLE_32_SECTIONS)

    def test_given_no_declared_file_when_run_then_it_exits_2_and_says_which(self):
        result = self.generate("--declared", str(self.directory / "missing-declared.json"))

        self.assertEqual(result.returncode, 2)
        self.assertIn("missing-declared.json", result.stderr)


# A source file as the AKM library writes them: one namespace, an anonymous namespace of helpers, then the public functions.
FIXTURE_SOURCE = """
#include "akm/ItemRequest.hpp"

namespace akm
{
    namespace
    {
        constexpr ItemId TABLE[] = {ItemId::TableOne, ItemId::TableTwo};

        // ItemId::InAComment is not sent.
        void sendThroughHelper(Session& session)
        {
            session.submit(makeRequest(ItemId::HelperItem, NO_VALUES), {});
        }

        void neverCalled(Session& session)
        {
            session.submit(makeRequest(ItemId::Orphan, NO_VALUES), {});
        }

        void sendEach(Session& session)
        {
            for (const ItemId id : TABLE)
                session.submit(makeRequest(id, NO_VALUES), {});
        }
    }

    void getSamplerName(Session& session, Completion completion)
    {
        const char* text = "ItemId::InAString";
        session.submit(makeRequest(ItemId::SystemGetName, NO_VALUES),
                       [completion](const CommandResult& outcome) {
                           decodeStringReply(ItemId::SystemGetName, outcome);
                       });
    }

    void setThing(Session& session)
    {
        sendThroughHelper(session);
    }

    void setBoth(Session& session)
    {
        sendEach(session);
        setThing(session);
    }

    int clockFieldName(int field)
    {
        return descriptor(ItemId::SystemSetClock).args[field].name;
    }

    void getForAll(Session& session, ItemId getId, int count)
    {
        session.submit(makeRequest(getId, NO_VALUES), {});
    }

    ItemDescriptor describeIt(ItemId id)
    {
        return descriptor(id);
    }

    void setByDescriptor(Session& session)
    {
        const ItemDescriptor& item = descriptor(ItemId::ByDescriptor);
        session.submit(makeRequest(item, NO_VALUES), {});
    }

    void Session::open(Completion completion)
    {
        impl->begin();
        use(impl->held);
    }

    void Session::close(Completion completion)
    {
        impl->end();
    }

    void Session::flush(Completion completion)
    {
        submit(makeRequest(ItemId::FlushItem, NO_VALUES), {});
    }

    namespace
    {
        class Impl
        {
        public:
            ItemId held = ItemId::HeldItem;

            void begin()
            {
                send(ItemId::OpenItem);
            }

            void end()
            {
                send(ItemId::CloseItem);
            }

            void send(ItemId item)
            {
                session.submit(makeRequest(item, NO_VALUES), {});
            }
        };
    }
}
"""

FIXTURE_HEADER = """
namespace akm
{
    void getSamplerName(Session& session, Completion completion);
    void setThing(Session& session);
    void setBoth(Session& session);
    int clockFieldName(int field);
    void getForAll(Session& session, ItemId getId, int count);
    ItemDescriptor describeIt(ItemId id);
    void setByDescriptor(Session& session);
    class Session
    {
    public:
        void open(Completion completion);
        void close(Completion completion);
        void flush(Completion completion);
    };
}
"""

ONE_FUNCTION_HEADER = "namespace akm { void echoIt(Session& session); }"


def echo_source(item, anonymous=False):
    """A source in which one function, public or (in the anonymous namespace) a helper, sends `item`."""
    send = f"session.submit(makeRequest(ItemId::{item}, NO_VALUES), {{}});"
    if anonymous:
        return f"namespace akm\n{{\n    namespace\n    {{\n        void lost(Session& session)\n        {{\n            {send}\n        }}\n    }}\n}}\n"
    return f"namespace akm\n{{\n    void echoIt(Session& session)\n    {{\n        {send}\n    }}\n}}\n"


class TheAkmScan(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.script = load_script()

    def attribute(self, source=FIXTURE_SOURCE, header=FIXTURE_HEADER):
        return self.script.attribute_akm({"Fixture.cpp": source}, {"Fixture.hpp": header})

    def test_given_a_public_function_that_builds_a_request_when_scanned_then_it_sends_the_item(self):
        self.assertEqual(self.attribute().functions["SystemGetName"], ["getSamplerName"])

    def test_given_an_item_named_in_a_comment_or_a_string_when_scanned_then_it_is_not_sent(self):
        result = self.attribute()

        self.assertNotIn("InAComment", result.functions)
        self.assertNotIn("InAString", result.functions)

    def test_given_an_item_named_only_to_read_its_descriptor_when_scanned_then_nothing_sends_it(self):
        result = self.attribute()

        self.assertNotIn("SystemSetClock", result.functions)
        self.assertNotIn("SystemSetClock", result.unattributed)

    def test_given_a_public_function_that_calls_a_helper_when_scanned_then_it_owns_the_helpers_item(self):
        self.assertEqual(self.attribute().functions["HelperItem"], ["setThing"])

    def test_given_a_public_function_that_calls_another_public_function_when_scanned_then_it_does_not_own_what_that_one_sends(self):
        self.assertNotIn("setBoth", self.attribute().functions["HelperItem"])

    def test_given_a_table_of_items_a_helper_loops_over_when_scanned_then_the_public_function_owns_each(self):
        result = self.attribute()

        self.assertEqual(result.functions["TableOne"], ["setBoth"])
        self.assertEqual(result.functions["TableTwo"], ["setBoth"])

    def test_given_a_helper_no_public_function_reaches_when_scanned_then_its_item_is_unattributed_with_its_file_and_line(self):
        result = self.attribute()

        self.assertEqual(list(result.unattributed), ["Orphan"])
        self.assertRegex(result.unattributed["Orphan"][0], r"^Fixture\.cpp:\d+$")

    def test_given_a_public_function_made_of_two_others_when_scanned_then_it_names_them_and_sends_nothing_itself(self):
        source = """
namespace akm
{
    void holdIt(Session& session)
    {
        session.submit(makeRequest(ItemId::KeyHold, NO_VALUES), {});
    }

    void releaseIt(Session& session)
    {
        session.submit(makeRequest(ItemId::KeyRelease, NO_VALUES), {});
    }

    void pressIt(Session& session)
    {
        holdIt(session);
        releaseIt(session);
    }
}
"""
        header = "namespace akm { void holdIt(Session& s); void releaseIt(Session& s); void pressIt(Session& s); }"

        result = self.attribute(source, header)

        self.assertEqual(result.composed, {"pressIt": ["holdIt", "releaseIt"]})
        self.assertEqual(result.functions["KeyHold"], ["holdIt"])

    def test_given_a_member_function_when_scanned_then_its_name_is_qualified(self):
        self.assertEqual(self.attribute().functions["FlushItem"], ["Session::flush"])

    def test_given_a_function_that_takes_the_item_as_a_parameter_and_sends_it_when_scanned_then_it_is_generic(self):
        # `describeIt` takes an item too, but only reads its descriptor.
        self.assertEqual(self.attribute().generic, ["getForAll"])

    def test_given_a_class_of_the_source_when_scanned_then_each_of_its_methods_owns_what_it_sends(self):
        result = self.attribute()

        self.assertEqual(result.functions["OpenItem"], ["Session::open"])
        self.assertEqual(result.functions["CloseItem"], ["Session::close"])

    def test_given_an_item_in_a_member_of_a_class_when_scanned_then_the_function_that_uses_the_member_owns_it(self):
        self.assertEqual(self.attribute().functions["HeldItem"], ["Session::open"])

    def test_given_a_function_that_fetches_a_descriptor_and_builds_a_request_from_it_when_scanned_then_it_sends_the_item(self):
        self.assertEqual(self.attribute().functions["ByDescriptor"], ["setByDescriptor"])

    def test_given_braces_that_do_not_balance_when_scanned_then_the_error_names_the_file(self):
        with self.assertRaises(self.script.ScanError) as raised:
            self.attribute(source="namespace akm {\n    void f()\n    {\n")

        self.assertIn("Fixture.cpp", str(raised.exception))

    def test_given_the_real_akm_sources_when_scanned_then_no_item_is_unattributed_and_known_ones_name_their_function(self):
        sources, headers = self.script.read_akm_sources(self.script.DEFAULT_AKM_SOURCES, self.script.DEFAULT_AKM_INCLUDE)

        result = self.script.attribute_akm(sources, headers)

        self.assertEqual(result.unattributed, {})
        self.assertEqual(result.functions["SystemGetName"], ["getSamplerName"])
        self.assertEqual(result.functions["ProgramGetAllNames"], ["getAllProgramNames"])


class TheAkmColumnOfTheFile(ScriptTest):
    def write_sources(self, source):
        return self.akm_sources(source)

    def test_given_a_function_that_sends_an_item_when_generated_then_its_name_is_in_the_akm_cell_of_the_row(self):
        result = self.generate(*self.write_sources(echo_source("SysExEcho")))

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("| `&06{6}` Echo (Get) | echoIt | none |", self.output.read_text(encoding="utf-8"))

    def test_given_an_item_no_public_function_reaches_when_generated_then_it_fails_and_names_the_item_and_the_file(self):
        result = self.generate(*self.write_sources(echo_source("SysExEcho", anonymous=True)))

        self.assertEqual(result.returncode, 1)
        self.assertIn("SysExEcho", result.stderr)
        self.assertIn("Fixture.cpp", result.stderr)
        self.assertFalse(self.output.exists())

    def test_given_an_item_the_catalogue_does_not_have_when_generated_then_it_fails_and_names_it(self):
        result = self.generate(*self.write_sources(echo_source("NotInTheCatalogue")))

        self.assertEqual(result.returncode, 1)
        self.assertIn("NotInTheCatalogue", result.stderr)

    def test_given_the_real_sources_when_generated_then_a_message_the_library_sends_names_its_function(self):
        self.generate()
        text = self.output.read_text(encoding="utf-8")

        self.assertRegex(text, r"\| `&[0-9A-F]{2}\{\d+\}` [^|]+ \(Get\) \| getAllProgramNames \| [^|]+ \|")


# The pieces of the MCP side as the repository writes them: a README with its tool tables, a gateway header with its public and private
# methods, gateway sources, a Tools.cpp that calls the gateway, and the declared list.
FIXTURE_README = """
# The server

## 3. Which tools need which option

| `not_a_tool_of_section_4` | ignored |

## 4. The tools

### 4.1 Memory (no option)

| Tool | What it does |
|---|---|
| `list_things` | lists |
| `rename_thing`, `delete_thing` | rename, delete |
| `set_parameter` | sets |
| `look` | reads |

### 4.5 Disks (`--allow-disk`)

| Tool | What it does |
|---|---|
| `load_it` | loads |

### 4.6 Keys

| Tool | What it does |
|---|---|
| `press_it` | **only with `--allow-front-panel`**: presses |

## 5. Safety

| `after_the_tools` | ignored |
"""

FIXTURE_GATEWAY_HEADER = """
namespace mcp
{
    class SamplerGateway
    {
    public:
        SamplerGateway(int config);
        Outcome listThings();
        Outcome renameThing(int index);
        Outcome deleteThing(int index);
        Outcome writeParameter(int parameter);
        Outcome lookAt();
        Outcome loadIt();
        Outcome pressIt();
        void close();

    private:
        Outcome runSequence(int request);
        Outcome sendHelper();
    };
}
"""

FIXTURE_GATEWAY_SOURCE = """
namespace mcp
{
    namespace
    {
        constexpr int SECOND_ITEM = 1;
    }

    Outcome SamplerGateway::sendHelper()
    {
        return runSequence(akm::makeRequest(akm::ItemId::HelperItem, NO_VALUES));
    }

    Outcome SamplerGateway::listThings()
    {
        akm::getThingNames(session(), done);
        return sendHelper();
    }

    Outcome SamplerGateway::renameThing(int index)
    {
        akm::renameThing(session(), index, done);
        return listThings();
    }

    Outcome SamplerGateway::deleteThing(int index)
    {
        akm::deleteThing(session(), index, done);
        return Outcome{};
    }

    Outcome SamplerGateway::writeParameter(int parameter)
    {
        return runSequence(akm::makeRequest(akm::ItemId::KeygroupSelect, NO_VALUES));
    }

    Outcome SamplerGateway::lookAt()
    {
        return runSequence(akm::makeRequest(akm::ItemId::LookItem, NO_VALUES));
    }

    Outcome SamplerGateway::loadIt()
    {
        return runSequence(akm::makeRequest(akm::ItemId::LoadItem, NO_VALUES));
    }

    Outcome SamplerGateway::pressIt()
    {
        akm::holdKey(session(), key, done);
        return Outcome{};
    }
}
"""

FIXTURE_TOOLS_SOURCE = """
namespace mcp
{
    std::vector<Tool> makeThings(SamplerGateway& gateway)
    {
        tools.push_back(Tool{definition("list_things"), [&gateway](const json& arguments) { return gateway.listThings(); }});
        tools.push_back(Tool{definition("rename_thing"), [&gateway](const json& arguments) { return gateway.renameThing(1); }});
        tools.push_back(Tool{definition("delete_thing"), [&gateway](const json& arguments) { return gateway.deleteThing(1); }});
        tools.push_back(Tool{definition("set_parameter"), [&gateway](const json& arguments) { return gateway.writeParameter(1); }});
        tools.push_back(Tool{definition("look"), [&gateway](const json& arguments) { return gateway.lookAt(); }});
        tools.push_back(Tool{definition("load_it"), [&gateway](const json& arguments) { return gateway.loadIt(); }});
        auto keyTool = [&](SamplerGateway& panel) { return panel.pressIt(); };
    }
}
"""

FIXTURE_STANDARD_PARAMETERS = """
namespace mcp
{
    void addRows(std::vector<ParameterDefinition>& rows)
    {
        rows.push_back(number("level", scope, akm::ItemId::ProgramSetLevel, akm::ItemId::ProgramGetLevel));
        rows.push_back(number("pan", scope, akm::ItemId::ZoneSetPan, akm::ItemId::ZoneGetPan));
        rows.push_back(number("loop", scope, akm::ItemId::SampleSetLoop, akm::ItemId::SampleGetLoop));
    }
}
"""

FIXTURE_AKM_FUNCTION_ITEMS = {
    "getThingNames": {"ThingGetNames"},
    "renameThing": {"ThingRename"},
    "deleteThing": {"ThingDelete"},
    "holdKey": {"KeyHold"},
}

FIXTURE_ITEM_SECTIONS = {"ProgramSetLevel": ("0A", "set"), "ProgramGetLevel": ("0A", "get"), "ZoneSetPan": ("06", "set"),
                         "ZoneGetPan": ("06", "get"), "SampleSetLoop": ("0E", "set"), "SampleGetLoop": ("0E", "get")}

FIXTURE_DECLARED_TOOLS = {
    "list_things": {"gateway": ["listThings"]},
    "rename_thing": {"gateway": ["renameThing"]},
    "delete_thing": {"gateway": ["deleteThing"]},
    "set_parameter": {"gateway": ["writeParameter"], "parameters": {"sections": ["0A", "06"], "kind": "set"}},
    "look": {"gateway": ["lookAt"]},
    "load_it": {"gateway": ["loadIt"]},
    "press_it": {"gateway": ["pressIt"]},
}


class TheMcpScan(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.script = load_script()

    def model(self, declared=None, tools_source=FIXTURE_TOOLS_SOURCE, readme=FIXTURE_README):
        return self.script.attribute_mcp(
            readme_text=readme, gateway_header=FIXTURE_GATEWAY_HEADER, gateway_sources={"Gateway.cpp": FIXTURE_GATEWAY_SOURCE},
            tools_source=tools_source, parameter_source=FIXTURE_STANDARD_PARAMETERS,
            declared_tools=FIXTURE_DECLARED_TOOLS if declared is None else declared,
            akm_function_items=FIXTURE_AKM_FUNCTION_ITEMS, akm_public_functions=set(FIXTURE_AKM_FUNCTION_ITEMS),
            item_sections=FIXTURE_ITEM_SECTIONS)

    def test_given_the_readmes_tool_tables_when_read_then_the_tools_and_their_options_are_those_of_section_4_only(self):
        tools = self.script.read_readme_tools(FIXTURE_README)

        self.assertEqual(tools, {"list_things": None, "rename_thing": None, "delete_thing": None, "set_parameter": None, "look": None,
                                 "load_it": "--allow-disk", "press_it": "--allow-front-panel"})

    def test_given_a_gateway_header_when_read_then_only_the_public_methods_are_listed(self):
        public = self.script.public_methods_of(FIXTURE_GATEWAY_HEADER, "SamplerGateway")

        self.assertIn("listThings", public)
        self.assertNotIn("runSequence", public)
        self.assertNotIn("sendHelper", public)

    def test_given_a_tools_source_when_read_then_the_gateway_methods_it_calls_are_found_whatever_the_receiver_is_named(self):
        calls = self.script.gateway_calls_of(FIXTURE_TOOLS_SOURCE, {"listThings", "pressIt", "lookAt", "close"})

        self.assertEqual(calls, {"listThings", "pressIt", "lookAt"})

    def test_given_a_tool_whose_method_calls_an_akm_function_when_modelled_then_the_message_is_the_tools(self):
        self.assertEqual(self.model().shown["delete_thing"], {"ThingDelete"})

    def test_given_a_method_that_names_a_type_of_the_library_when_modelled_then_it_does_not_own_what_the_types_constructor_sends(self):
        source = """
namespace mcp
{
    Outcome SamplerGateway::lookAt()
    {
        akm::Session& held = session();
        return Outcome{};
    }
}
"""
        model = self.script.attribute_mcp(
            readme_text=FIXTURE_README, gateway_header=FIXTURE_GATEWAY_HEADER, gateway_sources={"Gateway.cpp": source},
            tools_source=FIXTURE_TOOLS_SOURCE, parameter_source=FIXTURE_STANDARD_PARAMETERS, declared_tools={"look": {"gateway": ["lookAt"]}},
            akm_function_items={"Session": {"SessionItem"}}, akm_public_functions={"Session"}, item_sections=FIXTURE_ITEM_SECTIONS)

        self.assertEqual(model.shown["look"], set())

    def test_given_a_tool_whose_method_sends_through_a_private_helper_when_modelled_then_the_helpers_message_is_the_tools(self):
        self.assertIn("HelperItem", self.model().shown["list_things"])

    def test_given_a_method_that_calls_another_public_method_when_modelled_then_that_ones_messages_are_allowed_but_not_shown(self):
        model = self.model()

        self.assertNotIn("ThingGetNames", model.shown["rename_thing"])
        self.assertIn("ThingGetNames", model.allowed["rename_thing"])
        self.assertIn("ThingRename", model.shown["rename_thing"])

    def test_given_a_parameter_tool_when_modelled_then_it_has_the_set_items_of_its_sections_only(self):
        shown = self.model().shown["set_parameter"]

        self.assertEqual({"ProgramSetLevel", "ZoneSetPan"} & shown, {"ProgramSetLevel", "ZoneSetPan"})
        self.assertNotIn("SampleSetLoop", shown)
        self.assertNotIn("ProgramGetLevel", shown)

    def test_given_an_entry_that_names_akm_functions_when_modelled_then_they_replace_what_the_shared_method_reaches(self):
        declared = dict(FIXTURE_DECLARED_TOOLS)
        declared["list_things"] = {"gateway": ["listThings"], "akm": ["getThingNames"]}

        model = self.model(declared)

        self.assertEqual(model.shown["list_things"], {"ThingGetNames"})
        self.assertIn("HelperItem", model.allowed["list_things"])

    def test_given_a_tool_that_sends_nothing_when_modelled_then_it_has_no_message(self):
        declared = dict(FIXTURE_DECLARED_TOOLS)
        declared["look"] = {"sends_nothing": "it reads the catalogue"}
        tools_source = FIXTURE_TOOLS_SOURCE.replace("gateway.lookAt()", "describe()")

        model = self.model(declared, tools_source=tools_source)

        self.assertEqual(model.shown["look"], set())
        self.assertEqual(model.problems, [])

    def test_given_a_tool_of_the_readme_with_no_entry_when_modelled_then_the_problem_names_it(self):
        declared = {name: entry for name, entry in FIXTURE_DECLARED_TOOLS.items() if name != "look"}

        self.assertTrue(any("look" in problem and "no declared entry" in problem for problem in self.model(declared).problems))

    def test_given_an_entry_for_a_tool_the_readme_does_not_have_when_modelled_then_the_problem_names_it(self):
        declared = dict(FIXTURE_DECLARED_TOOLS)
        declared["ghost_tool"] = {"gateway": ["lookAt"]}

        self.assertTrue(any("ghost_tool" in problem and "README" in problem for problem in self.model(declared).problems))

    def test_given_a_declared_method_the_gateway_does_not_have_when_modelled_then_the_problem_names_it(self):
        declared = dict(FIXTURE_DECLARED_TOOLS)
        declared["look"] = {"gateway": ["lookAtSomethingElse"]}

        self.assertTrue(any("lookAtSomethingElse" in problem for problem in self.model(declared).problems))

    def test_given_a_declared_akm_function_the_library_does_not_have_when_modelled_then_the_problem_names_it(self):
        declared = dict(FIXTURE_DECLARED_TOOLS)
        declared["list_things"] = {"gateway": ["listThings"], "akm": ["noSuchFunction"]}

        self.assertTrue(any("noSuchFunction" in problem for problem in self.model(declared).problems))

    def test_given_a_gateway_method_the_tools_call_and_no_entry_declares_when_modelled_then_the_problem_names_it(self):
        declared = dict(FIXTURE_DECLARED_TOOLS)
        declared["load_it"] = {"sends_nothing": "wrongly"}

        problems = self.model(declared).problems

        self.assertTrue(any("loadIt" in problem and "no tool declares" in problem for problem in problems), problems)

    def test_given_a_declared_method_the_tools_never_call_when_modelled_then_the_problem_names_it(self):
        declared = dict(FIXTURE_DECLARED_TOOLS)
        declared["look"] = {"gateway": ["lookAt", "pressIt"]}
        tools_source = FIXTURE_TOOLS_SOURCE.replace("panel.pressIt()", "nothing()")

        problems = self.model(declared, tools_source=tools_source).problems

        self.assertTrue(any("pressIt" in problem and "never calls" in problem for problem in problems), problems)

    def test_given_a_parameter_entry_with_a_section_the_catalogue_does_not_have_when_modelled_then_the_problem_names_it(self):
        declared = dict(FIXTURE_DECLARED_TOOLS)
        declared["set_parameter"] = {"gateway": ["writeParameter"], "parameters": {"sections": ["7F"], "kind": "set"}}

        self.assertTrue(any("7F" in problem for problem in self.model(declared).problems))


class TheMcpColumnOfTheFile(ScriptTest):
    def test_given_the_real_sources_when_generated_then_a_message_a_tool_sends_names_the_tool_and_its_option(self):
        self.generate()
        text = self.output.read_text(encoding="utf-8")

        self.assertRegex(text, r"\| getAllProgramNames \| list_programs \|")
        self.assertRegex(text, r"\| [^|]+ \| load_file \(--allow-disk\) \|")
        self.assertRegex(text, r"Key Hold \(Set\) \| holdKey \| hold_key \(--allow-front-panel\), press_key \(--allow-front-panel\) \|")
        self.assertRegex(text, r"\| makeRequest \| get_parameters \|")

    def test_given_a_tool_with_no_entry_when_generated_then_it_fails_and_names_the_tool(self):
        declared = json.loads(DECLARED.read_text(encoding="utf-8"))
        del declared["mcp_tools"]["get_status"]
        path = self.directory / "declared.json"
        path.write_text(json.dumps(declared), encoding="utf-8")

        result = self.generate("--declared", str(path))

        self.assertEqual(result.returncode, 1)
        self.assertIn("get_status", result.stderr)
        self.assertFalse(self.output.exists())

    def test_given_the_real_sources_when_generated_then_the_two_tools_that_share_code_each_have_their_own_messages(self):
        self.generate()
        text = self.output.read_text(encoding="utf-8")

        self.assertRegex(text, r"Rename File \(Set\) \| [^|]+ \| rename_file \(--allow-disk\) \|")
        self.assertRegex(text, r"Rename Folder \(Set\) \| [^|]+ \| rename_folder \(--allow-disk\) \|")


if __name__ == "__main__":
    unittest.main()
