#!/usr/bin/env python3
"""Runs clang-tidy, with the checks of `juce/.clang-tidy`, on the production C++ files of XS56K that changed.

What is checked (DEC-QLT-012): with no argument, the production `.cpp`, `.hpp` and `.h` files of `juce/` (not `juce/tests/`, not the generated
files) that differ from the merge base with `origin/dev`, committed, staged, unstaged or new; a changed header through the sources that include
it. Named files are checked as given; `--all` checks every production source of the compile database.

The compile database is the one of `juce/build-tidy`, configured with Ninja and `CMAKE_EXPORT_COMPILE_COMMANDS` when it is missing (on Windows
inside Visual Studio's `vcvars64`); only the generated headers the sources include are built. clang-tidy is `--clang-tidy`, else `CLANG_TIDY`, else the one on the PATH, else Visual
Studio's own. Only the findings in the project's files are printed, each once, then a count by check.

Usage:
    python3 juce/tools/tidy.py [files...] [--all] [--tests] [--fix] [--base origin/dev] [--jobs N] [--clang-tidy PATH] [--configure]

(On Windows the interpreter is usually `python`.) See `.github/skills/static-analysis/SKILL.md`.

Exit status: 0 when clean, 1 with findings, 2 when the tool could not run (no clang-tidy, no compile database, a file clang-tidy could not read).

[TASK-QLT-024, RQ-QLT-009, RQ-QLT-010, RQ-QLT-013, ADR-QLT-002 (DEC-QLT-010, DEC-QLT-011, DEC-QLT-012)]
"""
import argparse
import collections
import concurrent.futures
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
JUCE = ROOT / "juce"
BUILD_DIR = JUCE / "build-tidy"
DATABASE = "compile_commands.json"
DEFAULT_BASE = "origin/dev"

PRODUCTION = re.compile(r"^juce/(akm|app|framework|mcp|midi)/")
TESTS = re.compile(r"^juce/tests/")  # checked only with --tests (DEC-QLT-001)
GENERATED = re.compile(r"\.generated\.hpp$")
SOURCE_SUFFIXES = (".cpp", ".hpp", ".h")
UNIT_SUFFIX = ".cpp"
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]', re.MULTILINE)
LOCATION = re.compile(r":(\d+):(\d+): (?:warning|error): ")  # after the path, which may hold a drive's colon
CHECK = re.compile(r" \[([\w.-]+)(?:,-warnings-as-errors)?\]$")
ERROR_LINE = re.compile(r": error: ")
MSVC_COMPILER = re.compile(r"(^|[\\/\s\"])cl(\.exe)?\"?\s", re.IGNORECASE)
FIX = "--fix"
MSVC_EXCEPTIONS = "/EHsc"  # the compile commands carry none, and clang-tidy then refuses `try` (ADR-QLT-002, fact 3)

VSWHERE = pathlib.Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
VS_CLANG_TIDY = pathlib.Path("VC") / "Tools" / "Llvm" / "x64" / "bin" / "clang-tidy.exe"
VS_VCVARS = pathlib.Path("VC") / "Auxiliary" / "Build" / "vcvars64.bat"
VS_NINJA = pathlib.Path("Common7") / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "Ninja"
GENERATED_TARGETS = ["XS56KAppFonts"]  # its BinaryData.h is included by the sources and written by a build step, not by the configuration
CONFIGURE_ARGUMENTS = ["-S", "juce", "-B", "juce/build-tidy", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug", "-DBUILD_APP=ON", "-DBUILD_TESTS=ON",
                       "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]

EXIT_CLEAN = 0
EXIT_FINDINGS = 1
EXIT_TOOL_ERROR = 2


class ToolError(Exception):
    """The tool could not run; the message says why and how to fix it."""


Finding = collections.namedtuple("Finding", "path line column check message")


def relative(path):
    """`path` relative to the repository, with `/`, or None when it is outside."""
    resolved = pathlib.Path(path).resolve()
    if not resolved.is_relative_to(ROOT):
        return None
    return resolved.relative_to(ROOT).as_posix()


def is_production(rel, tests=False):
    """A production C++ file of `juce/`, or with `tests` a test file too (DEC-QLT-001); never a generated one."""
    if rel is None or not rel.endswith(SOURCE_SUFFIXES) or GENERATED.search(rel) is not None:
        return False
    return PRODUCTION.match(rel) is not None or (tests and TESTS.match(rel) is not None)


def run_git(*arguments):
    done = subprocess.run(["git", *arguments], cwd=ROOT, capture_output=True, text=True, encoding="utf-8")
    if done.returncode != 0:
        raise ToolError(f"git {' '.join(arguments)} failed: {(done.stderr or done.stdout).strip()[:300]}")
    return done.stdout


def changed_files(git, base):
    """The files that differ from the merge base with `base`: committed, staged, unstaged, and untracked ones."""
    merge_base = git("merge-base", base, "HEAD").strip()
    names = git("diff", "--name-only", merge_base).splitlines() + git("ls-files", "--others", "--exclude-standard").splitlines()
    return sorted({name.strip() for name in names if name.strip()})


def read_database(build_dir):
    path = pathlib.Path(build_dir) / DATABASE
    if not path.is_file():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def database_units(entries, tests=False):
    """The production sources of the compile database, by repository path, with their entry."""
    units = {}
    for entry in entries:
        rel = relative(pathlib.Path(entry["directory"]) / entry["file"])
        if is_production(rel, tests) and rel.endswith(UNIT_SUFFIX):
            units[rel] = entry
    return units


def includes_of(text):
    """The file names (last component) a file includes."""
    return {pathlib.PurePosixPath(name).name for name in INCLUDE.findall(text)}


def units_including(header, units, files, read):
    """The sources of `units` that include `header`, directly or through other files of `files` (both repository paths)."""
    included_by = collections.defaultdict(set)
    for rel in set(files) | set(units):
        for name in includes_of(read(rel)):
            included_by[name].add(rel)
    found, seen, waiting = set(), {header}, [header]
    while waiting:
        current = waiting.pop()
        for includer in included_by.get(pathlib.PurePosixPath(current).name, ()):
            if includer in seen:
                continue
            seen.add(includer)
            if includer in units:
                found.add(includer)
            else:
                waiting.append(includer)
    return found


def production_files(tests=False):
    """Every production C++ file of `juce/` (and test file, with `tests`), by repository path."""
    return [rel for rel in (relative(path) for path in JUCE.rglob("*")) if is_production(rel, tests)]


def read_source(rel):
    try:
        return (ROOT / rel).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def select_units(names, units, files, read, tests=False):
    """The sources to check for the files `names`: a source as itself, a header through its includers."""
    selected = set()
    for rel in names:
        if not is_production(rel, tests):
            continue
        if rel.endswith(UNIT_SUFFIX):
            if rel in units:
                selected.add(rel)
        else:
            selected |= units_including(rel, units, files, read)
    return sorted(selected)


def parse_findings(output, tests=False):
    """The findings of a clang-tidy output that are in the project's production files."""
    findings = set()
    for line in output.splitlines():
        line = line.strip()
        location, check_name = LOCATION.search(line), CHECK.search(line)
        if location is None or check_name is None or location.start() == 0:
            continue
        rel = relative(line[:location.start()])
        if is_production(rel, tests):
            message = line[location.end():check_name.start()]
            findings.add(Finding(rel, int(location[1]), int(location[2]), check_name[1], message))
    return findings


def unreadable(output, returncode, findings):
    """The first error of a file clang-tidy could not analyse (an error outside the project, no finding), or None."""
    if returncode == 0 or findings:
        return None
    for line in output.splitlines():
        if ERROR_LINE.search(line):
            return line.strip()[:300]
    return None


def extra_arguments(entry):
    command = entry.get("command") or " ".join(entry.get("arguments", []))
    return [f"--extra-arg={MSVC_EXCEPTIONS}"] if MSVC_COMPILER.search(command + " ") else []


def run_clang_tidy(executable, build_dir, rel, entry, fix=False):
    """One clang-tidy run on a source; returns (exit status, output)."""
    command = [str(executable), "-p", str(build_dir), "--quiet", *([FIX] if fix else []), *extra_arguments(entry), str(ROOT / rel)]
    done = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
    return done.returncode, done.stdout + done.stderr


def check(units_to_check, units, run, jobs, tests=False):
    """Runs every source; returns the findings and the sources that could not be analysed, with their first error."""
    findings, failures = set(), {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        results = {rel: pool.submit(run, rel, units[rel]) for rel in units_to_check}
        for rel, future in results.items():
            returncode, output = future.result()
            found = parse_findings(output, tests)
            findings |= found
            failure = unreadable(output, returncode, found)
            if failure:
                failures[rel] = failure
    return findings, failures


def report(findings, failures, checked):
    for finding in sorted(findings):
        print(f"{finding.path}:{finding.line}:{finding.column}: {finding.message} [{finding.check}]")
    for rel, failure in sorted(failures.items()):
        print(f"{rel}: clang-tidy could not analyse it: {failure}")
    counts = collections.Counter(finding.check for finding in findings)
    if counts:
        print("\nby check:")
        for name, count in counts.most_common():
            print(f"  {count:4d}  {name}")
    print(f"\ntidy: {checked} source(s) checked, {len(findings)} finding(s), {len(failures)} not analysed")


def visual_studio():
    """The installation path of the newest Visual Studio with the C++ tools, or None."""
    if not VSWHERE.is_file():
        return None
    done = subprocess.run([str(VSWHERE), "-latest", "-prerelease", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                           "-property", "installationPath"], capture_output=True, text=True)
    path = done.stdout.strip().splitlines()
    return pathlib.Path(path[0]) if path else None


def find_clang_tidy(given):
    for candidate in (given, os.environ.get("CLANG_TIDY"), shutil.which("clang-tidy")):
        if candidate and pathlib.Path(candidate).is_file():
            return pathlib.Path(candidate)
    studio = visual_studio()
    if studio and (studio / VS_CLANG_TIDY).is_file():
        return studio / VS_CLANG_TIDY
    raise ToolError("clang-tidy not found: give --clang-tidy PATH or set CLANG_TIDY (Windows: Visual Studio's 'C++ Clang tools'; Linux: apt install clang-tidy; "
                    "macOS: brew install llvm)")


def configure():
    """Configures `juce/build-tidy` to write the compile database, and builds only the generated headers the sources include."""
    print("tidy: configuring juce/build-tidy (compile database and generated headers, a minute or two)", file=sys.stderr)
    build = ["cmake", "--build", "juce/build-tidy", "--target", *GENERATED_TARGETS]
    if sys.platform == "win32":
        studio = visual_studio()
        if studio is None or not (studio / VS_VCVARS).is_file():
            raise ToolError("Visual Studio with the C++ tools not found: configure juce/build-tidy by hand with "
                            "'cmake " + " ".join(CONFIGURE_ARGUMENTS) + "' in a developer prompt")
        script = (f'@echo off\r\ncall "{studio / VS_VCVARS}" >nul\r\nset "PATH={studio / VS_NINJA};%PATH%"\r\n'
                  f'cmake {" ".join(CONFIGURE_ARGUMENTS)} && {" ".join(build)}\r\n')
        with tempfile.TemporaryDirectory() as directory:
            batch = pathlib.Path(directory) / "configure-tidy.bat"
            batch.write_text(script, encoding="utf-8")
            done = subprocess.run(["cmd", "/c", str(batch)], cwd=ROOT, capture_output=True, text=True, errors="replace")
    else:
        if shutil.which("ninja") is None:
            raise ToolError("ninja not found: install it (apt install ninja-build, brew install ninja)")
        done = subprocess.run(["cmake", *CONFIGURE_ARGUMENTS], cwd=ROOT, capture_output=True, text=True, errors="replace")
        if done.returncode == 0:
            done = subprocess.run(build, cwd=ROOT, capture_output=True, text=True, errors="replace")
    if done.returncode != 0:
        raise ToolError(f"configuring juce/build-tidy failed: {(done.stdout + done.stderr).strip()[-500:]}")


def parse_arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("files", nargs="*", help="files to check (repository paths); none: what changed")
    parser.add_argument("--all", action="store_true", help="check every production source")
    parser.add_argument("--base", default=DEFAULT_BASE, help=f"the branch the changes are taken from (default {DEFAULT_BASE})")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--clang-tidy", dest="clang_tidy")
    parser.add_argument("--configure", action="store_true", help="configure juce/build-tidy again first")
    parser.add_argument("--tests", action="store_true", help="check the test code too (for the automatic fixes, on the owner's request)")
    parser.add_argument("--fix", action="store_true", help="apply the fixes clang-tidy offers (one source at a time); review the diff")
    return parser.parse_args(argv)


def real_runner(arguments):
    """clang-tidy run on one source, the compile database configured first when it is missing or asked for."""
    executable = find_clang_tidy(arguments.clang_tidy)
    if arguments.configure or read_database(BUILD_DIR) is None:
        configure()
    return lambda rel, entry: run_clang_tidy(executable, BUILD_DIR, rel, entry, arguments.fix)


def selection(arguments, units, git, read, files):
    """The sources to check: all of them, the files named, or what changed against the base branch."""
    if arguments.all:
        return sorted(units)
    names = [relative(ROOT / name) for name in arguments.files] if arguments.files else changed_files(git, arguments.base)
    return select_units([name for name in names if name], units, files if files is not None else production_files(arguments.tests), read, arguments.tests)


def main(argv=None, git=run_git, runner=None, read=read_source, files=None, database=None, build_dir=BUILD_DIR):
    arguments = parse_arguments(argv)
    try:
        if runner is None:
            runner = real_runner(arguments)
        entries = database if database is not None else read_database(build_dir)
        if entries is None:
            raise ToolError(f"no {DATABASE} in juce/build-tidy: run with --configure")
        units = database_units(entries, arguments.tests)
        selected = selection(arguments, units, git, read, files)
        if not selected:
            print("tidy: no production C++ source to check")
            return EXIT_CLEAN
        findings, failures = check(selected, units, runner, 1 if arguments.fix else max(1, arguments.jobs), arguments.tests)  # fixes in a shared header must not race
    except ToolError as error:
        print(f"tidy: {error}", file=sys.stderr)
        return EXIT_TOOL_ERROR
    report(findings, failures, len(selected))
    if failures:
        return EXIT_TOOL_ERROR
    return EXIT_FINDINGS if findings else EXIT_CLEAN


if __name__ == "__main__":
    sys.exit(main())
