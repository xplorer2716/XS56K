#!/usr/bin/env python3
"""Creates a C++ source file that starts with the project's licence header, copied from `juce/LICENSE_HEADER.txt`.

The header is never written by hand nor regenerated: this copies the template, then appends the rest of the file (from `--body FILE`, or
nothing). It refuses to overwrite a file that exists. LF line endings, UTF-8. See `.github/skills/new-source-file/SKILL.md`.

Usage: python3 juce/tools/new_source_file.py <path> [--body FILE]
       python3 juce/tools/new_source_file.py --check <path> ...   (exit 1 naming the files that do not start with the header)
"""
import argparse
import pathlib
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
TEMPLATE = ROOT / "juce" / "LICENSE_HEADER.txt"
SOURCE_ROOT = ROOT / "juce"
SOURCE_SUFFIXES = (".cpp", ".hpp", ".h")


def source_path(path):
    """`path` resolved, when it is a C++ file under `juce/`; otherwise ValueError. A path given by an assistant is never trusted further.
    [TASK-QLT-021, RQ-QLT-001]"""
    resolved = (ROOT / path).resolve()
    if not resolved.is_relative_to(SOURCE_ROOT.resolve()) or resolved.suffix not in SOURCE_SUFFIXES:
        raise ValueError(f"{path}: not a {'/'.join(SOURCE_SUFFIXES)} file under juce/")
    return resolved


def body_path(path):
    """`path` resolved, when it is a file of the repository or of the system's temporary directory (where a session writes its scratch
    files); otherwise ValueError. [TASK-QLT-021, RQ-QLT-001]"""
    resolved = pathlib.Path(path).resolve()
    if not any(resolved.is_relative_to(root.resolve()) for root in (ROOT, pathlib.Path(tempfile.gettempdir()))):
        raise ValueError(f"{path}: the body is neither in the repository nor in the temporary directory")
    return resolved


def header():
    return TEMPLATE.read_text(encoding="utf-8")


def create(path, body=""):
    path = source_path(path)
    if path.exists():
        raise FileExistsError(f"{path} exists: nothing written")
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as stream:
        stream.write(header() + body)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="+")
    parser.add_argument("--body", help="a file whose text follows the header")
    parser.add_argument("--check", action="store_true", help="check that each file starts with the header")
    arguments = parser.parse_args(argv)
    if arguments.check:
        try:
            missing = [p for p in arguments.paths if not source_path(p).read_text(encoding="utf-8").startswith(header())]
        except ValueError as error:
            print(f"new_source_file: {error}", file=sys.stderr)
            return 1
        if missing:
            print("new_source_file: no licence header: " + ", ".join(missing))
            return 1
        print("new_source_file: every file starts with the licence header")
        return 0
    if len(arguments.paths) != 1:
        print("new_source_file: give one path to create", file=sys.stderr)
        return 1
    try:
        body = body_path(arguments.body).read_text(encoding="utf-8") if arguments.body else ""
        create(arguments.paths[0], body)
    except (OSError, ValueError) as error:  # FileExistsError is an OSError
        print(f"new_source_file: {error}", file=sys.stderr)
        return 1
    print(f"new_source_file: created {arguments.paths[0]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
